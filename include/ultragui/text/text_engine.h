#ifndef ULTRAGUI_TEXT_TEXT_ENGINE_H_
#define ULTRAGUI_TEXT_TEXT_ENGINE_H_

#include <ultragui/core/color.h>
#include <ultragui/core/math.h>
#include <ultragui/core/rect.h>
#include <ultragui/core/types.h>
#include <ultragui/rhi/rhi_types.h>
#include <ultragui/style/enums.h>

namespace ugui {

class RHI;

/// Opaque handle to a loaded font face
using FontHandle = u32;
constexpr FontHandle kInvalidFont = ~0u;

struct GlyphMetrics {
  f32 advance_x;
  f32 bearing_x;
  f32 bearing_y;
  f32 width;
  f32 height;
  // UV coordinates in the atlas
  f32 u0, v0, u1, v1;
};

/// Shaped text run: the output of shaping a string with a font.
struct TextRun {
  struct Glyph {
    u32 glyph_id;
    f32 x_offset;
    f32 y_offset;
    f32 x_advance;
    // Atlas UVs and pixel size
    f32 u0, v0, u1, v1;
    f32 bmp_w, bmp_h;
    f32 bearing_x, bearing_y;
  };

  Glyph* glyphs = nullptr;
  u32 glyph_count = 0;
  f32 total_advance = 0.0f;
  f32 ascent = 0.0f;
  f32 descent = 0.0f;
  f32 line_height = 0.0f;
};

/// Configuration for text layout
struct TextLayoutConfig {
  f32 max_width = 1e6f;
  TextAlign align = TextAlign::kLeft;
  bool ellipsize = false;
};

/// Result of laying out text into lines
struct TextLayout {
  struct Line {
    u32 glyph_start;
    u32 glyph_count;
    f32 width;
    f32 y_offset;
  };

  Line* lines = nullptr;
  u32 line_count = 0;
  f32 total_width = 0.0f;
  f32 total_height = 0.0f;
};

/// Font atlas and text shaping engine.
/// Wraps FreeType for rasterization and HarfBuzz for shaping.
class TextEngine {
 public:
  bool Init(RHI* rhi);
  void Shutdown();

  /// Load a font from a file path. Returns a handle for use with shape/draw.
  FontHandle LoadFont(const char* path);

  /// Load a font with explicit weight and style metadata.
  FontHandle LoadFont(const char* path, FontWeight weight, FontStyle style);

  /// Resolve the best font handle for the given weight and style.
  /// Falls back to the closest available weight in the same family.
  FontHandle ResolveFont(FontHandle base_font, FontWeight weight,
                         FontStyle style) const;

  /// Shape a UTF-8 string into positioned glyphs.
  /// The returned TextRun is valid until the next call to Shape() or
  /// EndFrame().
  TextRun Shape(FontHandle font, const char* text, u32 text_len, f32 font_size,
                f32 letter_spacing = 0.0f, f32 line_height_mult = 1.0f);

  /// Lay out a shaped run into wrapped, aligned lines.
  TextLayout Layout(const TextRun& run, const TextLayoutConfig& config);

  /// Call at the start of each frame to reset internal scratch buffers.
  /// All previously returned TextRun/TextLayout pointers become invalid.
  void BeginFrame();

  /// Must be called once per frame to upload any new glyphs to the GPU atlas.
  void FlushAtlas();

  /// Get the atlas texture handle for rendering.
  RHITextureHandle atlas_texture() const { return atlas_texture_; }

  /// Get atlas dimensions
  Vec2 atlas_size() const;

  /// CPU pixels of the glyph atlas, single-channel 8-bit alpha (R8), row-major,
  /// atlas_size().x * atlas_size().y bytes. For renderer backends that own the
  /// font texture themselves (the Dear ImGui io.Fonts->GetTexDataAsAlpha8
  /// analog). Valid for the lifetime of the engine; contents grow as glyphs are
  /// shaped.
  const u8* atlas_pixels() const;

  /// Monotonic counter bumped whenever the atlas contents change (a new glyph
  /// was packed). A backend re-uploads its font texture when this changes.
  u32 atlas_revision() const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
  RHI* rhi_ = nullptr;
  RHITextureHandle atlas_texture_ = kInvalidTexture;
};

}  // namespace ugui

#endif  // ULTRAGUI_TEXT_TEXT_ENGINE_H_
