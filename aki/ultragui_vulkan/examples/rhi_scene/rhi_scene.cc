// rhi_scene (C++): the reference for src/main.aki. Drives ugui::Platform
// (platform_glfw.cc) and ugui::RHI (vulkan_rhi.cc) directly, with no
// UIContext: SDF rounded rects (radii, border, shadow, inner softness), a
// gradient, textured quads (RGBA linear, BGRA nearest, updated each frame),
// text from the TextEngine atlas, scissored content, and an offscreen render
// target composited back. The aki program builds the same vertices in the
// same order; under aki/tools/xshot.sh the two screenshots should be equal.
//
//   rhi_scene <shader_dir> <font.ttf> [frames]
//
// frames: exit after that many frames (0 or absent: run until closed).
// Build: aki/ultragui_vulkan/examples/rhi_scene/build_cpp.sh <ref-build-dir>

#include <ugui/platform/platform.h>
#include <ugui/render/texture_backend.h>
#include <ugui/rhi/rhi.h>
#include <ugui/text/text_engine.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

using namespace ugui;

namespace {

struct Mesh {
  std::vector<Vertex2D> v;
  std::vector<u32> i;
};

// Renderer2D::EmitQuad at dpi 1 with no transform.
void Quad(Mesh& m, Rect r, Color c, Color c2, u32 radii, f32 softness,
          f32 border_width, Color border) {
  u32 color = Vertex2D::PackColor(c.r, c.g, c.b, c.a);
  u32 color2 = Vertex2D::PackColor(c2.r, c2.g, c2.b, c2.a);
  u32 bc = Vertex2D::PackColor(border.r, border.g, border.b, border.a);
  f32 hw = r.w * 0.5f;
  f32 hh = r.h * 0.5f;
  u32 base = static_cast<u32>(m.v.size());
  m.v.push_back({{r.x, r.y}, {0, 0}, color, color2, radii, softness, {hw, hh},
                 border_width, bc});
  m.v.push_back({{r.x + r.w, r.y}, {1, 0}, color, color2, radii, softness,
                 {hw, hh}, border_width, bc});
  m.v.push_back({{r.x + r.w, r.y + r.h}, {1, 1}, color, color2, radii,
                 softness, {hw, hh}, border_width, bc});
  m.v.push_back({{r.x, r.y + r.h}, {0, 1}, color, color2, radii, softness,
                 {hw, hh}, border_width, bc});
  u32 idx[6] = {0, 1, 2, 0, 2, 3};
  for (u32 k : idx) m.i.push_back(base + k);
}

void Solid(Mesh& m, Rect r, Color c, u32 radii) {
  Quad(m, r, c, c, radii, 0.0f, 0.0f, Color{0, 0, 0, 0});
}

// Renderer2D::DrawText at dpi 1 with no transform.
void Text(Mesh& m, Vec2 pos, const TextRun& run, Color color) {
  u32 packed = Vertex2D::PackColor(color.r, color.g, color.b, color.a);
  f32 cursor_x = pos.x;
  f32 baseline_y = pos.y + run.ascent;
  for (u32 i = 0; i < run.glyph_count; ++i) {
    const auto& g = run.glyphs[i];
    if (g.bmp_w <= 0 || g.bmp_h <= 0) {
      cursor_x += g.x_advance;
      continue;
    }
    f32 x = roundf(cursor_x + g.bearing_x + g.x_offset);
    f32 y = roundf(baseline_y - g.bearing_y + g.y_offset);
    f32 w = roundf(g.bmp_w);
    f32 h = roundf(g.bmp_h);
    u32 base = static_cast<u32>(m.v.size());
    m.v.push_back({{x, y}, {g.u0, g.v0}, packed, packed, 0, 0, {0, 0}, 0, 0});
    m.v.push_back(
        {{x + w, y}, {g.u1, g.v0}, packed, packed, 0, 0, {0, 0}, 0, 0});
    m.v.push_back(
        {{x + w, y + h}, {g.u1, g.v1}, packed, packed, 0, 0, {0, 0}, 0, 0});
    m.v.push_back(
        {{x, y + h}, {g.u0, g.v1}, packed, packed, 0, 0, {0, 0}, 0, 0});
    u32 idx[6] = {0, 1, 2, 0, 2, 3};
    for (u32 k : idx) m.i.push_back(base + k);
    cursor_x += g.x_advance;
  }
}

void Draw(RHI& rhi, const Mesh& m, u32 first_quad, u32 quad_count,
          RHITextureHandle tex) {
  rhi.DrawTriangles(m.v.data(), static_cast<u32>(m.v.size()),
                    m.i.data() + first_quad * 6, quad_count * 6, tex);
}

TextRun Shape(TextEngine& te, FontHandle f, const char* s, f32 size) {
  return te.Shape(f, s, static_cast<u32>(strlen(s)), size);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: rhi_scene <shader_dir> <font.ttf> [frames]\n");
    return 2;
  }
  int frames = argc > 3 ? atoi(argv[3]) : 0;

  Platform platform;
  Platform::WindowConfig wc;
  wc.width = 640;
  wc.height = 480;
  wc.title = "rhi_scene";
  wc.resizable = false;
  if (!platform.Init(wc)) return 1;

  RHI rhi;
  RHIConfig rc;
  rc.platform = &platform;
  rc.validation = false;
  rc.shader_dir = argv[1];
  if (!rhi.Init(rc)) return 1;

  TextEngine te;
  te.Init(&rhi);
  FontHandle font = te.LoadFont(argv[2]);

  // An 8x8 RGBA checkerboard, sampled linearly.
  u8 checker[8 * 8 * 4];
  for (u32 y = 0; y < 8; ++y) {
    for (u32 x = 0; x < 8; ++x) {
      u8* p = checker + (y * 8 + x) * 4;
      bool on = ((x + y) & 1) != 0;
      p[0] = on ? 240 : 40;
      p[1] = on ? 200 : 60;
      p[2] = on ? 80 : 160;
      p[3] = 255;
    }
  }
  RHITextureHandle checker_tex = rhi.CreateTexture(
      8, 8, RHIFormat::kRgba8Unorm, checker, RHIFilter::kLinear);
  // A 2x2 BGRA texture, sampled nearest.
  u8 bgra[2 * 2 * 4] = {255, 0,   0,   255, 0,   255, 0,   255,
                        0,   0,   255, 255, 255, 255, 255, 128};
  RHITextureHandle bgra_tex = rhi.CreateTexture(2, 2, RHIFormat::kBgra8Unorm,
                                                bgra, RHIFilter::kNearest);
  RHITextureHandle rt = rhi.CreateRenderTarget(128, 128);

  const Color none{0, 0, 0, 0};
  for (int frame = 0; frames == 0 || frame < frames; ++frame) {
    platform.PollEvents();
    if (platform.ShouldClose()) break;

    te.BeginFrame();
    TextRun hello = Shape(te, font, "Hello, ultragui!", 28.0f);
    TextRun clipped = Shape(te, font, "Scissored text is clipped here", 22.0f);
    TextRun rt_label = Shape(te, font, "RT", 40.0f);
    te.FlushAtlas();
    RHITextureHandle atlas = RhiHandleFromTextureId(te.atlas_texture());
    rhi.UpdateTexture(checker_tex, checker);

    // Offscreen target content.
    Mesh rq, rtext;
    Quad(rq, {16, 16, 96, 96}, {1, 1, 1, 0.85f}, {1, 1, 1, 0.85f},
         Vertex2D::PackRadii(24), 0.0f, 3.0f, {0.9f, 0.3f, 0.5f, 1});
    Text(rtext, {36, 38}, rt_label, {0.1f, 0.1f, 0.2f, 1});

    // Main pass content: quads 0..9 in one shared vertex array.
    Mesh q, text, ctext;
    Solid(q, {20, 20, 180, 100}, {0.23f, 0.51f, 0.96f, 1},
          Vertex2D::PackRadii(12));                                    // 0
    Quad(q, {220, 20, 180, 100}, {0.9f, 0.3f, 0.3f, 1},
         {0.9f, 0.3f, 0.3f, 1}, Vertex2D::PackRadii(0, 20, 40, 8), 0.0f,
         3.0f, {1, 1, 1, 0.9f});                                       // 1
    Quad(q, {410, 16, 220, 120}, {0, 0, 0, 0.6f}, {0, 0, 0, 0.6f},
         Vertex2D::PackRadii(16), 10.0f, 0.0f, none);                  // 2
    Solid(q, {420, 20, 200, 100}, {0.95f, 0.95f, 0.97f, 1},
          Vertex2D::PackRadii(16));                                    // 3
    Quad(q, {20, 140, 280, 80}, {1, 0.6f, 0.1f, 1}, {0.6f, 0.1f, 0.9f, 1},
         Vertex2D::PackRadii(10), 0.0f, 0.0f, none);                   // 4
    Quad(q, {20, 240, 120, 60}, {0.2f, 0.8f, 0.5f, 1},
         {0.2f, 0.8f, 0.5f, 1}, Vertex2D::PackRadii(12), -4.0f, 0.0f,
         none);                                                        // 5
    Solid(q, {320, 140, 128, 128}, {1, 1, 1, 1}, Vertex2D::PackRadii(20));  // 6
    Solid(q, {470, 140, 150, 150}, {1, 1, 1, 1}, 0);                   // 7
    Solid(q, {20, 320, 64, 64}, {1, 1, 1, 1}, 0);                      // 8
    Quad(q, {160, 240, 300, 200}, {0.3f, 0.3f, 0.6f, 1},
         {0.3f, 0.3f, 0.6f, 1}, Vertex2D::PackRadii(30), 0.0f, 4.0f,
         {1, 0.8f, 0.2f, 1});                                          // 9
    Text(text, {20, 420}, hello, {1, 1, 1, 1});
    Text(ctext, {170, 300}, clipped, {1, 0.95f, 0.4f, 1});

    if (!rhi.AcquireFrame()) continue;
    if (rhi.BeginOffscreen(rt, {0.1f, 0.5f, 0.4f, 1})) {
      Draw(rhi, rq, 0, 1, kInvalidTexture);
      rhi.DrawTextTriangles(rtext.v.data(), static_cast<u32>(rtext.v.size()),
                            rtext.i.data(), static_cast<u32>(rtext.i.size()),
                            atlas);
      rhi.EndOffscreen(rt);
    }

    if (!rhi.BeginFrame({0.08f, 0.09f, 0.12f, 1})) continue;
    Draw(rhi, q, 0, 6, kInvalidTexture);
    Draw(rhi, q, 6, 1, checker_tex);
    Draw(rhi, q, 7, 1, rt);
    Draw(rhi, q, 8, 1, bgra_tex);
    rhi.DrawTextTriangles(text.v.data(), static_cast<u32>(text.v.size()),
                          text.i.data(), static_cast<u32>(text.i.size()),
                          atlas);
    rhi.SetScissor({200, 280, 180, 120});
    Draw(rhi, q, 9, 1, kInvalidTexture);
    rhi.DrawTextTriangles(ctext.v.data(), static_cast<u32>(ctext.v.size()),
                          ctext.i.data(), static_cast<u32>(ctext.i.size()),
                          atlas);
    rhi.ResetScissor();
    rhi.EndFrame();
  }

  te.Shutdown();
  rhi.Shutdown();
  platform.Shutdown();
  return 0;
}
