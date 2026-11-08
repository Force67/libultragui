// Shared by rhi_scene.cc and draw_data_scene.cc: the geometry of the scene of
// aki/ultragui_scenes/src/scene.aki, built as Renderer2D builds it.
#ifndef AKI_TOOLS_SCENE_COMMON_H_
#define AKI_TOOLS_SCENE_COMMON_H_

#include <math.h>
#include <string.h>

#include <vector>

#include <ugui/render/vertex.h>
#include <ugui/rhi/rhi.h>
#include <ugui/text/text_engine.h>

namespace scene {
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


// The main pass: panel, gradient, shadow, textures, clipped and translucent
// panels, and the text over them. `big` is the texture of the 128x96 quad.
static void BuildMain(Geometry& quads, Geometry& text, TextEngine& te, FontHandle font, RHITextureHandle checker,
                      RHITextureHandle smooth, RHITextureHandle big, Rect full) {
  quads.Quad({20, 20, 300, 170}, rgba(0x2a3a5a, 1), rgba(0x2a3a5a, 1), Vertex2D::PackRadii(16, 16, 16, 16), 0, 2, rgba(0x8fb0ff, 1));
  quads.Quad({340, 20, 280, 60}, rgba(0xff5050, 1), rgba(0x5050ff, 1), Vertex2D::PackRadii(30, 4, 30, 4), 0, 0, 0);
  quads.Shadow({40, 130, 140, 40}, rgba(0x000000, 0.6f), 12, 0, 0, 4, 10, 10, 10, 10);
  quads.Quad({40, 130, 140, 40}, rgba(0x4a4aff, 1), rgba(0x4a4aff, 1), Vertex2D::PackRadii(10.0f), 0, 0, 0);
  quads.Close(full, kInvalidTexture);
  quads.Quad({340, 100, 64, 64}, rgba(0xffffff, 1), rgba(0xffffff, 1), Vertex2D::PackRadii(8.0f), 0, 0, 0);
  quads.Close(full, checker);
  quads.Quad({416, 100, 64, 64}, rgba(0xffffff, 1), rgba(0xffffff, 1), 0, 0, 0, 0);
  quads.Close(full, smooth);
  quads.Quad({492, 100, 128, 96}, rgba(0xffffff, 1), rgba(0xffffff, 1), Vertex2D::PackRadii(6.0f), 0, 0, 0);
  quads.Close(full, big);
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
}

}  // namespace scene

#endif  // AKI_TOOLS_SCENE_COMMON_H_
