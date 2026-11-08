// The reference scene for the aki D3D12 backend (examples/rhi_scene.aki is the
// same program): a window, the C++ RHI (ugui/rhi/d3d12/d3d12_rhi.cc), and one
// frame drawn straight through the RHI calls: SDF rounded rects with a border,
// a gradient and a soft shadow, a textured quad, text from the TextEngine
// atlas, a scissored rect, an offscreen render target drawn back as a texture,
// and a YCbCr video frame converted into a render target. Run both under
// aki/tools/xshot.sh and compare the screenshots.
//
// Build (after configuring the reference with -DULTRAGUI_BACKEND_D3D12=ON
// -DULTRAGUI_BACKEND_VULKAN=OFF -DULTRAGUI_BUILD_EXAMPLES=OFF into
// /tmp/ugui-d3d12-ref), from the repository root, inside the dev shell:
//
//   ninja -C /tmp/ugui-d3d12-ref ultragui ultragui_hlsl_shaders
//   clang++ -std=c++20 -O1 -I. -Ithird_party/miniaudio aki/ultragui_d3d12/tools/rhi_scene.cc \
//     /tmp/ugui-d3d12-ref/libultragui.a /tmp/ugui-d3d12-ref/third_party/yoga/yoga/libyogacore.a \
//     /tmp/ugui-d3d12-ref/third_party/rlottie/librlottie.a \
//     $(pkg-config --libs glfw3 freetype2 harfbuzz libvkd3d libvkd3d-utils vulkan) -ldl -lpthread \
//     -o /tmp/d3d12_scene_cc
//   aki/tools/xshot.sh /tmp/scene_cc.png 12 /tmp/d3d12_scene_cc
//
// SCENE_MODE=stress adds what the plain scene does not reach: a batch of
// 24000 vertices / 36000 indices (the upload buffers grow), UpdateTexture on
// the checker, and creating and destroying a texture and a render target
// mid-run. SCENE_SHADER_DIR is the aki program's shader directory (the C++
// backend always loads the build's, ULTRAGUI_HLSL_SHADER_DIR).
#include <ugui/platform/platform.h>
#include <ugui/rhi/rhi.h>
#include <ugui/text/text_engine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>

using namespace ugui;

struct Batch {
  std::vector<Vertex2D> v;
  std::vector<u32> i;
};

// One quad: [x - e, x + w + e] x [y - e, y + h + e], uv over the quad. sdf
// quads carry the half extents of the quad.
static void Quad(Batch& b, f32 x, f32 y, f32 w, f32 h, f32 e, u32 c1, u32 c2,
                 u32 radii, f32 softness, f32 border_w, u32 border_c, bool sdf,
                 f32 u0 = 0, f32 v0 = 0, f32 u1 = 1, f32 v1 = 1) {
  f32 x0 = x - e, y0 = y - e, x1 = x + w + e, y1 = y + h + e;
  f32 hx = sdf ? (x1 - x0) * 0.5f : 0.0f;
  f32 hy = sdf ? (y1 - y0) * 0.5f : 0.0f;
  u32 base = static_cast<u32>(b.v.size());
  auto vert = [&](f32 px, f32 py, f32 u, f32 v) {
    Vertex2D vx = {};
    vx.pos[0] = px;
    vx.pos[1] = py;
    vx.uv[0] = u;
    vx.uv[1] = v;
    vx.color = c1;
    vx.color2 = c2;
    vx.corner_radii = radii;
    vx.softness = softness;
    vx.half_size[0] = hx;
    vx.half_size[1] = hy;
    vx.border_width = border_w;
    vx.border_color = border_c;
    b.v.push_back(vx);
  };
  vert(x0, y0, u0, v0);
  vert(x1, y0, u1, v0);
  vert(x1, y1, u1, v1);
  vert(x0, y1, u0, v1);
  u32 idx[6] = {0, 1, 2, 0, 2, 3};
  for (u32 k : idx) b.i.push_back(base + k);
}

int main() {
  Platform platform;
  Platform::WindowConfig wc;
  wc.width = 800;
  wc.height = 600;
  wc.title = "d3d12 scene";
  wc.resizable = false;
  if (!platform.Init(wc)) {
    fprintf(stderr, "platform init failed\n");
    return 1;
  }
  bool stress = getenv("SCENE_MODE") && strcmp(getenv("SCENE_MODE"), "stress") == 0;
  RHI rhi;
  RHIConfig rc;
  rc.platform = &platform;
  if (!rhi.Init(rc)) {
    fprintf(stderr, "rhi init failed\n");
    return 2;
  }

  TextEngine te;
  te.Init(&rhi);
  FontHandle font = te.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

  // Textures: a checker (RGBA), the offscreen target, the video planes.
  u32 checker_px[8 * 8];
  for (u32 y = 0; y < 8; ++y)
    for (u32 x = 0; x < 8; ++x) {
      bool on = ((x ^ y) & 1) != 0;
      checker_px[y * 8 + x] = on ? 0xFF3050F0u : 0xFFF0D030u;
    }
  RHITextureHandle checker = rhi.CreateTexture(8, 8, RHIFormat::kRgba8Unorm, checker_px, RHIFilter::kNearest);
  RHITextureHandle target = rhi.CreateRenderTarget(128, 128);

  u8 yplane[64 * 48], cbplane[32 * 24], crplane[32 * 24];
  for (u32 y = 0; y < 48; ++y)
    for (u32 x = 0; x < 64; ++x) yplane[y * 64 + x] = static_cast<u8>(16 + (x * 3 + y * 2) % 200);
  for (u32 y = 0; y < 24; ++y)
    for (u32 x = 0; x < 32; ++x) {
      cbplane[y * 32 + x] = static_cast<u8>(64 + x * 4);
      crplane[y * 32 + x] = static_cast<u8>(192 - y * 6);
    }
  RHITextureHandle ytex = rhi.CreateTexture(64, 48, RHIFormat::kR8Unorm, yplane, RHIFilter::kLinear);
  RHITextureHandle cbtex = rhi.CreateTexture(32, 24, RHIFormat::kR8Unorm, cbplane, RHIFilter::kLinear);
  RHITextureHandle crtex = rhi.CreateTexture(32, 24, RHIFormat::kR8Unorm, crplane, RHIFilter::kLinear);
  RHITextureHandle video = rhi.CreateRenderTarget(64, 48);

  // Text: shaped once, quads built per glyph from the atlas UVs.
  Batch text;
  auto add_text = [&](const char* s, f32 size, f32 x, f32 baseline, u32 color) {
    TextRun run = te.Shape(font, s, static_cast<u32>(strlen(s)), size);
    f32 pen = x;
    for (u32 g = 0; g < run.glyph_count; ++g) {
      const auto& gl = run.glyphs[g];
      if (gl.bmp_w > 0 && gl.bmp_h > 0)
        Quad(text, pen + gl.x_offset + gl.bearing_x, baseline + gl.y_offset - gl.bearing_y, gl.bmp_w, gl.bmp_h, 0.0f,
             color, color, 0, 0.0f, 0.0f, 0, false, gl.u0, gl.v0, gl.u1, gl.v1);
      pen += gl.x_advance;
    }
  };
  add_text("Hello, D3D12 backend", 28.0f, 40.0f, 470.0f, Vertex2D::PackColor(1.0f, 1.0f, 1.0f, 1.0f));
  add_text("rounded rects, gradient, shadow, scissor, offscreen, video", 14.0f, 40.0f, 500.0f,
           Vertex2D::PackColor(0.7f, 0.85f, 1.0f, 1.0f));
  te.FlushAtlas();

  u32 white = Vertex2D::PackColor(1, 1, 1, 1);

  // Static batches.
  Batch shapes;  // 0: shadow, 1: panel, 2: pill
  Quad(shapes, 44, 48, 320, 200, 12, Vertex2D::PackColor(0, 0, 0, 0.6f), Vertex2D::PackColor(0, 0, 0, 0.6f),
       Vertex2D::PackRadii(12), 12.0f, 0, 0, true);
  Quad(shapes, 40, 40, 320, 200, 0, Vertex2D::PackColor(0.95f, 0.45f, 0.2f, 1), Vertex2D::PackColor(0.3f, 0.1f, 0.6f, 1),
       Vertex2D::PackRadii(16, 4, 24, 0), 0.0f, 3.0f, Vertex2D::PackColor(1, 1, 1, 1), true);
  Quad(shapes, 70, 190, 140, 36, 0, Vertex2D::PackColor(0.1f, 0.7f, 0.4f, 1), Vertex2D::PackColor(0.1f, 0.7f, 0.4f, 1),
       Vertex2D::PackRadii(18), 0.0f, 0, 0, true);

  Batch chk;
  Quad(chk, 400, 40, 128, 128, 0, white, white, 0, 0, 0, 0, false);
  Batch off;
  Quad(off, 560, 40, 128, 128, 0, white, white, 0, 0, 0, 0, false);
  Batch vid;
  Quad(vid, 400, 200, 192, 144, 0, white, white, 0, 0, 0, 0, false);
  Batch clip;
  Quad(clip, 60, 280, 400, 120, 0, Vertex2D::PackColor(0.9f, 0.8f, 0.1f, 1), Vertex2D::PackColor(0.9f, 0.2f, 0.1f, 1),
       Vertex2D::PackRadii(30), 0.0f, 0, 0, true);
  Batch inner;  // drawn into the offscreen target, 128x128 pixels
  Quad(inner, 0, 0, 128, 128, 0, Vertex2D::PackColor(0.1f, 0.2f, 0.5f, 1), Vertex2D::PackColor(0.6f, 0.8f, 1.0f, 1), 0,
       0, 0, 0, false);
  Quad(inner, 14, 14, 100, 100, 0, Vertex2D::PackColor(1, 0.9f, 0.2f, 1), Vertex2D::PackColor(1, 0.9f, 0.2f, 1),
       Vertex2D::PackRadii(50), 0.0f, 4.0f, Vertex2D::PackColor(0, 0, 0, 1), true);

  // Stress mode: 100 x 60 small quads behind everything (24000 vertices).
  Batch grid;
  if (stress) {
    for (u32 row = 0; row < 60; ++row)
      for (u32 col = 0; col < 100; ++col) {
        u32 c = Vertex2D::PackColor(col / 100.0f, row / 60.0f, 0.5f, 1.0f);
        Quad(grid, static_cast<f32>(col * 8), static_cast<f32>(row * 8 + 100), 6.0f, 6.0f, 0.0f, c, c, 0, 0.0f, 0.0f, 0,
             false);
      }
  }

  for (int frame = 0;; ++frame) {
    platform.PollEvents();
    if (platform.ShouldClose()) break;
    if (stress && frame == 3) {
      // UpdateTexture, DestroyTexture, DestroyRenderTarget mid-run.
      u32 inverted[8 * 8];
      for (u32 i = 0; i < 64; ++i) inverted[i] = checker_px[63 - i];
      rhi.UpdateTexture(checker, inverted);
      RHITextureHandle temp = rhi.CreateTexture(8, 8, RHIFormat::kRgba8Unorm, inverted, RHIFilter::kNearest);
      RHITextureHandle temp_rt = rhi.CreateRenderTarget(32, 32);
      rhi.DestroyTexture(temp);
      rhi.DestroyTexture(temp_rt);
      printf("scene: stress updates done (%u, %u)\n", temp, temp_rt);
    }
    if (stress && frame < 2) {
      // Grow both ring slots' upload buffers before any other draw of a frame:
      // the C++ backend frees the old buffer when one grows, so growing
      // mid-frame leaves the earlier draws of that frame pointing at it.
      if (!rhi.BeginFrame(Color{0.0f, 0.0f, 0.0f, 1.0f})) continue;
      rhi.DrawTriangles(grid.v.data(), static_cast<u32>(grid.v.size()), grid.i.data(), static_cast<u32>(grid.i.size()),
                        kInvalidTexture);
      rhi.EndFrame();
      continue;
    }
    if (!rhi.AcquireFrame()) continue;

    // Outside any pass: video conversion and the offscreen target.
    rhi.ConvertVideoFrame(video, ytex, cbtex, crtex);
    if (rhi.BeginOffscreen(target, Color{0.0f, 0.0f, 0.0f, 1.0f})) {
      rhi.DrawTriangles(inner.v.data(), static_cast<u32>(inner.v.size()), inner.i.data(),
                        static_cast<u32>(inner.i.size()), kInvalidTexture);
      rhi.EndOffscreen(target);
    }

    if (!rhi.BeginFrame(Color{0.10f, 0.11f, 0.14f, 1.0f})) continue;

    if (stress)
      rhi.DrawTriangles(grid.v.data(), static_cast<u32>(grid.v.size()), grid.i.data(), static_cast<u32>(grid.i.size()),
                        kInvalidTexture);

    // The same vertex array twice: the second call takes the dedup path.
    rhi.DrawTriangles(shapes.v.data(), static_cast<u32>(shapes.v.size()), shapes.i.data(), 12, kInvalidTexture);
    rhi.DrawTriangles(shapes.v.data(), static_cast<u32>(shapes.v.size()), shapes.i.data() + 12, 6, kInvalidTexture);

    rhi.DrawTriangles(chk.v.data(), static_cast<u32>(chk.v.size()), chk.i.data(), static_cast<u32>(chk.i.size()),
                      checker);
    rhi.DrawTriangles(off.v.data(), static_cast<u32>(off.v.size()), off.i.data(), static_cast<u32>(off.i.size()),
                      target);
    rhi.DrawTriangles(vid.v.data(), static_cast<u32>(vid.v.size()), vid.i.data(), static_cast<u32>(vid.i.size()),
                      video);

    rhi.SetScissor(Rect{100.0f, 300.0f, 150.0f, 60.0f});
    rhi.DrawTriangles(clip.v.data(), static_cast<u32>(clip.v.size()), clip.i.data(), static_cast<u32>(clip.i.size()),
                      kInvalidTexture);
    rhi.ResetScissor();

    rhi.DrawTextTriangles(text.v.data(), static_cast<u32>(text.v.size()), text.i.data(), static_cast<u32>(text.i.size()),
                          RhiHandleFromTextureId(te.atlas_texture()));
    rhi.EndFrame();
    if (frame == 0) printf("scene: first frame done\n");
    usleep(16000);
  }

  te.Shutdown();
  rhi.Shutdown();
  platform.Shutdown();
  return 0;
}
