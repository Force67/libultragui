#pragma once

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
constexpr FontHandle INVALID_FONT = ~0u;

struct GlyphMetrics {
    f32 advance_x;
    f32 bearing_x;
    f32 bearing_y;
    f32 width;
    f32 height;
    // UV coordinates in the atlas
    f32 u0, v0, u1, v1;
};

/// Shaped text run - the output of shaping a string with a font.
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
    TextAlign align = TextAlign::Left;
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
    bool init(RHI* rhi);
    void shutdown();

    /// Load a font from a file path. Returns a handle for use with shape/draw.
    FontHandle load_font(const char* path);

    /// Shape a UTF-8 string into positioned glyphs.
    /// The returned TextRun is valid until the next call to shape() or end_frame().
    TextRun shape(FontHandle font, const char* text, u32 text_len, f32 font_size);

    /// Lay out a shaped run into wrapped, aligned lines.
    TextLayout layout(const TextRun& run, const TextLayoutConfig& config);

    /// Must be called once per frame to upload any new glyphs to the GPU atlas.
    void flush_atlas();

    /// Get the atlas texture handle for rendering.
    RHITextureHandle atlas_texture() const { return atlas_texture_; }

    /// Get atlas dimensions
    Vec2 atlas_size() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
    RHI* rhi_ = nullptr;
    RHITextureHandle atlas_texture_ = INVALID_TEXTURE;
};

} // namespace ugui
