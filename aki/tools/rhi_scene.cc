// The scene of aki/ultragui_scenes/src/scene.aki (shared by the OpenGL and
// D3D11 tests), drawn through the C++ RHI. Screenshots of this program and
// of the aki test are compared by tools/compare_shots.sh. Build against a
// reference build of the library (-DULTRAGUI_BACKEND_OPENGL=ON or
// -DULTRAGUI_BACKEND_D3D11=ON, VULKAN off):
//
//   nix develop -c bash -c 'cmake -S . -B /tmp/ugui-ref-gl -G Ninja \
//     -DULTRAGUI_BACKEND_OPENGL=ON -DULTRAGUI_BACKEND_VULKAN=OFF \
//     -DULTRAGUI_BUILD_SHADERS=ON -DULTRAGUI_AUDIO=OFF -DULTRAGUI_LOTTIE=OFF \
//     -DULTRAGUI_VIDEO=OFF -DULTRAGUI_LUA=OFF -DULTRAGUI_BUILD_TESTS=OFF &&
//     ninja -C /tmp/ugui-ref-gl ultragui &&
//     clang++ -std=c++20 -O1 -I. -DSCENE_OPENGL aki/tools/rhi_scene.cc \
//       /tmp/ugui-ref-gl/libultragui.a $(pkg-config --libs --cflags glfw3 freetype2 harfbuzz) \
//       -ldl -lpthread -o /tmp/rhi_scene_gl'
//
// UGUI_SCENE_SECS keeps the window open and redrawing that long.
#include <stdio.h>
#include <stdlib.h>

#include <ugui/platform/platform.h>
#include <ugui/render/texture_backend.h>

#include "scene_common.h"

using namespace ugui;
using namespace scene;

struct Textures {
  RHITextureHandle checker = kInvalidTexture;
  RHITextureHandle smooth = kInvalidTexture;
  RHITextureHandle target = kInvalidTexture;
};

static Textures MakeTextures(RHI& rhi) {
  Textures t;
  t.checker = rhi.CreateTexture(8, 8, RHIFormat::kRgba8Unorm, nullptr, RHIFilter::kNearest);
  u8 px[256] = {};
  for (size_t y = 0; y < 8; ++y)
    for (size_t x = 0; x < 8; ++x) {
      bool on = (x + y) % 2 == 0;
      size_t o = (y * 8 + x) * 4;
      px[o] = on ? 240 : 40;
      px[o + 1] = on ? 200 : 60;
      px[o + 2] = on ? 60 : 160;
      px[o + 3] = 255;
    }
  rhi.UpdateTexture(t.checker, px);
  u8 ramp[64] = {};
  for (size_t y = 0; y < 4; ++y)
    for (size_t x = 0; x < 4; ++x) {
      size_t o = (y * 4 + x) * 4;
      ramp[o] = static_cast<u8>(x * 85);
      ramp[o + 1] = static_cast<u8>(y * 85);
      ramp[o + 2] = static_cast<u8>(255 - x * 60);
      ramp[o + 3] = 255;
    }
  t.smooth = rhi.CreateTexture(4, 4, RHIFormat::kBgra8Unorm, ramp, RHIFilter::kLinear);
  t.target = rhi.CreateRenderTarget(128, 96);
  return t;
}

static void SubmitBatches(RHI& rhi, const Geometry& g, bool is_text, RHITextureHandle atlas) {
  for (const Batch& b : g.batches) {
    rhi.SetScissor(b.scissor);
    if (is_text)
      rhi.DrawTextTriangles(g.vertices.data(), static_cast<u32>(g.vertices.size()), g.indices.data() + b.index_offset,
                            b.index_count, atlas);
    else
      rhi.DrawTriangles(g.vertices.data(), static_cast<u32>(g.vertices.size()), g.indices.data() + b.index_offset,
                        b.index_count, b.texture);
  }
}

static bool DrawScene(RHI& rhi, TextEngine& te, FontHandle font, const Textures& tex) {
  te.BeginFrame();

  Rect off_full = {0, 0, 128, 96};
  Geometry off_quads, off_text;
  off_quads.Quad({8, 8, 112, 80}, rgba(0xff8030, 1), rgba(0xffd060, 1), Vertex2D::PackRadii(14.0f), 0, 3, rgba(0xffffff, 1));
  off_quads.Close(off_full, kInvalidTexture);
  off_text.Text(te, font, 24, 30, "RT", 28, rgba(0x102040, 1));
  off_text.Close(off_full, kInvalidTexture);

  Vec2 ds = rhi.display_size();
  Rect full = {0, 0, ds.x, ds.y};
  Geometry quads, text;

  BuildMain(quads, text, te, font, tex.checker, tex.smooth, tex.target, full);

  te.FlushAtlas();
  RHITextureHandle atlas = RhiHandleFromTextureId(te.atlas_texture());

  rhi.AcquireFrame();
  if (rhi.BeginOffscreen(tex.target, Color{0.2f, 0.0f, 0.3f, 1.0f})) {
    SubmitBatches(rhi, off_quads, false, atlas);
    SubmitBatches(rhi, off_text, true, atlas);
    rhi.EndOffscreen(tex.target);
  }

  if (!rhi.BeginFrame(Color{0.07f, 0.08f, 0.12f, 1.0f})) return false;
  SubmitBatches(rhi, quads, false, atlas);
  SubmitBatches(rhi, text, true, atlas);
  rhi.ResetScissor();
  rhi.EndFrame();
  return true;
}

int main() {
  Platform platform;
  Platform::WindowConfig wc;
  wc.width = 640;
  wc.height = 400;
  wc.title = "rhi_scene";
  wc.resizable = false;
#ifdef SCENE_OPENGL
  wc.opengl = true;
#endif
  if (!platform.Init(wc)) {
    fprintf(stderr, "rhi_scene: no window\n");
    return 0;
  }
  RHI rhi;
  RHIConfig config;
  config.platform = &platform;
  if (!rhi.Init(config)) {
    fprintf(stderr, "rhi_scene: RHI init failed\n");
    return 1;
  }
  TextEngine te;
  te.Init(&rhi);
  FontHandle font = te.LoadFont(kFontPath);
  Textures tex = MakeTextures(rhi);

  const char* env = getenv("UGUI_SCENE_SECS");
  f64 hold = env ? atof(env) : 0.0;
  f64 start = platform.time();
  int frames = 0;
  while (true) {
    platform.PollEvents();
    DrawScene(rhi, te, font, tex);
    ++frames;
    if (frames >= 3 && platform.time() - start >= hold) break;
    if (platform.ShouldClose()) break;
  }
  rhi.DestroyTexture(tex.checker);
  rhi.DestroyTexture(tex.smooth);
  rhi.DestroyRenderTarget(tex.target);
  te.Shutdown();
  rhi.Shutdown();
  platform.Shutdown();
  return 0;
}
