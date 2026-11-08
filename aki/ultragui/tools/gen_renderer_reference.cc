// Prints the DrawData the C++ Renderer2D builds for a fixed scene (draw-data
// mode, a recording texture backend), for tests/renderer2d_test.aki. Build
// and run inside the repo's dev shell, from the repository root:
//
//   clang++ -std=c++20 -O2 -I. $(pkg-config --cflags freetype2 harfbuzz) \
//     aki/ultragui/tools/gen_renderer_reference.cc ugui/render/renderer2d.cc \
//     ugui/core/from_chars.cc ugui/style/style.cc ugui/style/transition.cc \
//     ugui/text/text_engine.cc ugui/rhi/null/null_rhi.cc \
//     $(pkg-config --libs freetype2 harfbuzz) -o /tmp/gen_renderer_reference \
//     && /tmp/gen_renderer_reference
//
// Lines: TEX w h format fnv (each CreateTexture), V (one per quad vertex,
// then one per text vertex: pos, uv, color, color2, radii, softness,
// half_size, border width, border color; floats as bits), IQ/IT (index
// buffers), CMD (clip bits, texture, offset, count, is_text, blur bits).
// The scene must match tests/renderer2d_test.aki's `scene` step for step.
#include <ugui/render/renderer2d.h>
#include <ugui/style/style.h>
#include <ugui/text/text_engine.h>

#include <stdio.h>
#include <string.h>

using namespace ugui;

static u32 Bits(f32 v) {
  u32 u;
  memcpy(&u, &v, 4);
  return u;
}

struct RecordingBackend : TextureBackend {
  u64 next = 100;
  TextureId CreateTexture(u32 w, u32 h, RHIFormat format, const void* pixels,
                          RHIFilter) override {
    u64 hash = 1469598103934665603ull;
    const u8* p = static_cast<const u8*>(pixels);
    for (u32 i = 0; i < w * h * 4; ++i) {
      hash ^= p[i];
      hash *= 1099511628211ull;
    }
    printf("TEX %u %u %d 0x%016llx\n", w, h, (int)format,
           (unsigned long long)hash);
    return next++;
  }
  void UpdateTexture(TextureId, const void*) override {}
  void DestroyTexture(TextureId) override {}
};

static void PrintVertex(const Vertex2D& v) {
  printf("V 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x "
         "0x%08x 0x%08x 0x%08x\n",
         Bits(v.pos[0]), Bits(v.pos[1]), Bits(v.uv[0]), Bits(v.uv[1]), v.color,
         v.color2, v.corner_radii, Bits(v.softness), Bits(v.half_size[0]),
         Bits(v.half_size[1]), Bits(v.border_width), v.border_color);
}

int main() {
  TextEngine te;
  if (!te.Init(nullptr)) return 1;
  FontHandle f = te.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
  te.BeginFrame();
  const char* hi = "Hi, Wy!";
  TextRun run = te.Shape(f, hi, (u32)strlen(hi), 16.0f);
  const char* para = "The quick brown fox jumps";
  TextRun prun = te.Shape(f, para, (u32)strlen(para), 14.0f);
  TextLayoutConfig cfg;
  cfg.max_width = 90.0f;
  TextLayout lay = te.Layout(prun, cfg);

  RecordingBackend backend;
  Renderer2D r;
  r.Init(nullptr);
  r.set_display_size({800.0f, 600.0f});
  r.set_texture_backend(&backend);
  r.BeginFrame();

  // The scene (keep in step with renderer2d_test.aki).
  r.DrawRect({10.5f, 20.25f, 100.0f, 50.0f}, Color{0.2f, 0.4f, 0.6f, 0.8f},
             Vertex2D::PackRadii(8.0f));
  r.DrawRectGradient({30.0f, 40.0f, 120.0f, 60.0f},
                     Color{1.0f, 0.0f, 0.0f, 1.0f}, Color{0.0f, 0.0f, 1.0f, 0.5f},
                     Vertex2D::PackRadii(4.0f, 6.0f, 8.0f, 10.0f), 135.0f);
  r.DrawShadow({50.0f, 60.0f, 80.0f, 40.0f}, Color{0.0f, 0.0f, 0.0f, 0.4f},
               6.0f, 2.0f, Vec2{3.0f, 4.0f}, Vertex2D::PackRadii(5.0f));
  r.DrawText({12.0f, 100.0f}, run, Color{1.0f, 1.0f, 0.9f, 1.0f}, kFontTextureId);
  r.PushScissor({0.0f, 0.0f, 200.0f, 150.0f});
  r.DrawInsetShadow({20.0f, 20.0f, 100.0f, 60.0f}, Color{0.1f, 0.0f, 0.2f, 0.7f},
                    5.0f, 3.0f, Vec2{1.0f, 2.0f}, Vertex2D::PackRadii(2.0f, 9.0f, 3.0f, 12.0f));
  r.DrawBorderedRect({150.0f, 100.0f, 90.0f, 70.0f}, Color{0.9f, 0.9f, 0.9f, 1.0f},
                     Color{0.3f, 0.3f, 0.3f, 1.0f}, 2.5f, Vertex2D::PackRadii(6.0f));
  r.PushScissor({100.0f, 50.0f, 300.0f, 300.0f});
  r.DrawRect({120.0f, 60.0f, 10.0f, 10.0f}, Color{1, 1, 1, 1});
  r.PopScissor();
  r.PopScissor();
  r.PushTransform({200.0f, 200.0f}, 30.0f);
  r.DrawRect({180.0f, 190.0f, 40.0f, 20.0f}, Color{0.0f, 1.0f, 0.0f, 1.0f}, Vertex2D::PackRadii(3.0f));
  r.PushTransform({210.0f, 205.0f}, -12.5f);
  r.DrawText({190.0f, 195.0f}, run, Color{0.0f, 0.0f, 0.0f, 1.0f}, kFontTextureId);
  r.PopTransform();
  r.PopTransform();
  r.DrawRadialGradient({300.0f, 300.0f, 64.0f, 64.0f}, Color{1.0f, 1.0f, 0.0f, 1.0f},
                       Color{0.0f, 0.5f, 0.5f, 0.0f}, Vertex2D::PackRadii(32.0f));
  GradientStop stops[3] = {{0.0f, Color{1, 0, 0, 1}}, {0.4f, Color{0, 1, 0, 1}},
                           {1.0f, Color{0, 0, 1, 0.5f}}};
  r.DrawMultiStopGradient({400.0f, 300.0f, 100.0f, 40.0f}, stops, 3,
                          GradientType::kLinear, 60.0f, Vertex2D::PackRadii(4.0f));
  r.DrawMultiStopGradient({400.0f, 350.0f, 50.0f, 50.0f}, stops, 3,
                          GradientType::kRadial, 0.0f);
  // Cached: no new texture.
  r.DrawRadialGradient({370.0f, 300.0f, 20.0f, 20.0f}, Color{1.0f, 1.0f, 0.0f, 1.0f},
                       Color{0.0f, 0.5f, 0.5f, 0.0f});
  r.set_next_blur(12.0f);
  r.DrawRect({0.0f, 400.0f, 300.0f, 100.0f}, Color{1, 1, 1, 0.5f}, Vertex2D::PackRadii(10.0f));
  r.set_next_blur(0.0f);
  r.DrawRect({10.0f, 410.0f, 30.0f, 30.0f}, Color{0.5f, 0.5f, 0.5f, 1.0f});
  r.DrawTextLayout({500.0f, 100.0f}, prun, lay, Color{0.8f, 0.8f, 0.8f, 1.0f},
                   kFontTextureId, 90.0f);
  r.DrawTexturedRect({600.0f, 500.0f, 32.0f, 32.0f}, 7, Color{1.0f, 0.5f, 1.0f, 1.0f}, 0);

  const DrawData& dd = r.GetDrawData();
  printf("COUNTS %u %u %u %u %u\n", dd.quad_vertex_count, dd.quad_index_count,
         dd.text_vertex_count, dd.text_index_count, dd.command_count);
  for (u32 i = 0; i < dd.quad_vertex_count; ++i) PrintVertex(dd.quad_vertices[i]);
  for (u32 i = 0; i < dd.text_vertex_count; ++i) PrintVertex(dd.text_vertices[i]);
  printf("IQ");
  for (u32 i = 0; i < dd.quad_index_count; ++i) printf(" %u", dd.quad_indices[i]);
  printf("\nIT");
  for (u32 i = 0; i < dd.text_index_count; ++i) printf(" %u", dd.text_indices[i]);
  printf("\n");
  for (u32 i = 0; i < dd.command_count; ++i) {
    const DrawCmd& c = dd.commands[i];
    printf("CMD 0x%08x 0x%08x 0x%08x 0x%08x %llu %u %u %d 0x%08x\n",
           Bits(c.clip_rect.x), Bits(c.clip_rect.y), Bits(c.clip_rect.w),
           Bits(c.clip_rect.h), (unsigned long long)c.texture_id, c.index_offset,
           c.elem_count, c.is_text ? 1 : 0, Bits(c.blur));
  }
  te.Shutdown();
  return 0;
}
