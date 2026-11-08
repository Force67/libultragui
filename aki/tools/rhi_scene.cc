// The scene of aki/ultragui_opengl/tests/support/scene.aki (and the copy in
// ultragui_d3d11), drawn through the C++ RHI. Screenshots of this program and
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <ugui/platform/platform.h>
#include <ugui/render/texture_backend.h>
#include <ugui/render/vertex.h>
#include <ugui/rhi/rhi.h>
#include <ugui/text/text_engine.h>

using namespace ugui;

static const char* kFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

struct Batch {
  Rect scissor;
  RHITextureHandle texture;
  u32 index_offset;
  u32 index_count;
};

static u32 rgba(u32 c, f32 a) {
  f32 r = static_cast<f32>((c >> 16) & 0xFF) / 255.0f;
  f32 g = static_cast<f32>((c >> 8) & 0xFF) / 255.0f;
  f32 b = static_cast<f32>(c & 0xFF) / 255.0f;
  return Vertex2D::PackColor(r, g, b, a);
}

static Vertex2D vertex(f32 x, f32 y, f32 u, f32 v, u32 color, u32 color2, u32 radii, f32 softness, f32 hw, f32 hh,
                       f32 bw, u32 bc) {
  return {{x, y}, {u, v}, color, color2, radii, softness, {hw, hh}, bw, bc};
}

struct Geometry {
  std::vector<Vertex2D> vertices;
  std::vector<u32> indices;
  std::vector<Batch> batches;
  u32 batch_start = 0;

  void Close(Rect scissor, RHITextureHandle texture) {
    u32 total = static_cast<u32>(indices.size());
    if (total <= batch_start) return;
    batches.push_back({scissor, texture, batch_start, total - batch_start});
    batch_start = total;
  }

  void Quad(Rect rect, u32 color, u32 color2, u32 radii, f32 softness, f32 border_width, u32 border_color) {
    f32 hw = rect.w * 0.5f;
    f32 hh = rect.h * 0.5f;
    u32 base = static_cast<u32>(vertices.size());
    vertices.push_back(vertex(rect.x, rect.y, 0, 0, color, color2, radii, softness, hw, hh, border_width, border_color));
    vertices.push_back(vertex(rect.x + rect.w, rect.y, 1, 0, color, color2, radii, softness, hw, hh, border_width, border_color));
    vertices.push_back(vertex(rect.x + rect.w, rect.y + rect.h, 1, 1, color, color2, radii, softness, hw, hh, border_width, border_color));
    vertices.push_back(vertex(rect.x, rect.y + rect.h, 0, 1, color, color2, radii, softness, hw, hh, border_width, border_color));
    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
  }

  void Shadow(Rect rect, u32 color, f32 blur, f32 spread, f32 ox, f32 oy, f32 tl, f32 tr, f32 br, f32 bl) {
    Rect r = {rect.x + ox - spread - blur, rect.y + oy - spread - blur, rect.w + (spread + blur) * 2.0f,
              rect.h + (spread + blur) * 2.0f};
    Quad(r, color, color, Vertex2D::PackRadii(tl + spread, tr + spread, br + spread, bl + spread), blur, 0.0f, 0);
  }

  void Text(TextEngine& te, FontHandle font, f32 x, f32 y, const char* s, f32 size, u32 color) {
    TextRun run = te.Shape(font, s, static_cast<u32>(strlen(s)), size);
    f32 cursor_x = x;
    f32 baseline_y = y + run.ascent;
    for (u32 i = 0; i < run.glyph_count; ++i) {
      const auto& g = run.glyphs[i];
      if (g.bmp_w <= 0 || g.bmp_h <= 0) {
        cursor_x += g.x_advance;
        continue;
      }
      f32 gx = roundf((cursor_x + g.bearing_x + g.x_offset) * 1.0f) * 1.0f;
      f32 gy = roundf((baseline_y - g.bearing_y + g.y_offset) * 1.0f) * 1.0f;
      f32 gw = roundf(g.bmp_w * 1.0f) * 1.0f;
      f32 gh = roundf(g.bmp_h * 1.0f) * 1.0f;
      u32 base = static_cast<u32>(vertices.size());
      vertices.push_back(vertex(gx, gy, g.u0, g.v0, color, color, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0));
      vertices.push_back(vertex(gx + gw, gy, g.u1, g.v0, color, color, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0));
      vertices.push_back(vertex(gx + gw, gy + gh, g.u1, g.v1, color, color, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0));
      vertices.push_back(vertex(gx, gy + gh, g.u0, g.v1, color, color, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0));
      indices.push_back(base);
      indices.push_back(base + 1);
      indices.push_back(base + 2);
      indices.push_back(base);
      indices.push_back(base + 2);
      indices.push_back(base + 3);
      cursor_x += g.x_advance;
    }
  }
};

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

  quads.Quad({20, 20, 300, 170}, rgba(0x2a3a5a, 1), rgba(0x2a3a5a, 1), Vertex2D::PackRadii(16, 16, 16, 16), 0, 2, rgba(0x8fb0ff, 1));
  quads.Quad({340, 20, 280, 60}, rgba(0xff5050, 1), rgba(0x5050ff, 1), Vertex2D::PackRadii(30, 4, 30, 4), 0, 0, 0);
  quads.Shadow({40, 130, 140, 40}, rgba(0x000000, 0.6f), 12, 0, 0, 4, 10, 10, 10, 10);
  quads.Quad({40, 130, 140, 40}, rgba(0x4a4aff, 1), rgba(0x4a4aff, 1), Vertex2D::PackRadii(10.0f), 0, 0, 0);
  quads.Close(full, kInvalidTexture);
  quads.Quad({340, 100, 64, 64}, rgba(0xffffff, 1), rgba(0xffffff, 1), Vertex2D::PackRadii(8.0f), 0, 0, 0);
  quads.Close(full, tex.checker);
  quads.Quad({416, 100, 64, 64}, rgba(0xffffff, 1), rgba(0xffffff, 1), 0, 0, 0, 0);
  quads.Close(full, tex.smooth);
  quads.Quad({492, 100, 128, 96}, rgba(0xffffff, 1), rgba(0xffffff, 1), Vertex2D::PackRadii(6.0f), 0, 0, 0);
  quads.Close(full, tex.target);
  Rect clip = {40, 230, 150, 60};
  quads.Quad({20, 210, 300, 120}, rgba(0x30c070, 1), rgba(0x108040, 1), Vertex2D::PackRadii(20.0f), 0, 4, rgba(0xffffff, 1));
  quads.Close(clip, kInvalidTexture);
  quads.Quad({340, 220, 280, 150}, rgba(0xffffff, 0.15f), rgba(0xffffff, 0.45f), Vertex2D::PackRadii(24, 24, 4, 4), 0, 1,
             rgba(0xffffff, 0.8f));
  quads.Close(full, kInvalidTexture);

  text.Text(te, font, 36, 32, "Hello, ultragui", 24, rgba(0xffffff, 1));
  text.Text(te, font, 36, 66, "Rounded rects, SDF borders", 14, rgba(0xa8c0ff, 1));
  text.Text(te, font, 36, 90, "gradient / shadow / texture / text", 12, rgba(0xffd070, 1));
  text.Close(full, kInvalidTexture);
  text.Text(te, font, 28, 250, "clipped text overflows the scissor rectangle", 16, rgba(0x000000, 1));
  text.Close(clip, kInvalidTexture);
  text.Text(te, font, 356, 236, "alpha blended panel", 18, rgba(0xffffff, 1));
  text.Close(full, kInvalidTexture);

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
