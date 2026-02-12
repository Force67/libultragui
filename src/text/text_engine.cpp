#include <ultragui/rhi/rhi.h>
#include <ultragui/text/text_engine.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <cassert>
#include <cstdio>
#include <cstring>
#include <hb-ft.h>
#include <hb.h>
#include <unordered_map>
#include <vector>

namespace ugui {

static constexpr u32 ATLAS_SIZE = 2048;
static constexpr u32 MAX_FONTS = 32;

struct GlyphKey {
    FontHandle font;
    u32 glyph_id;
    u32 pixel_size;

    bool operator==(const GlyphKey& o) const {
        return font == o.font && glyph_id == o.glyph_id && pixel_size == o.pixel_size;
    }
};

struct GlyphKeyHash {
    size_t operator()(const GlyphKey& k) const {
        size_t h = k.font;
        h = h * 31 + k.glyph_id;
        h = h * 31 + k.pixel_size;
        return h;
    }
};

struct CachedGlyph {
    f32 u0, v0, u1, v1;
    f32 bmp_w, bmp_h;
    f32 bearing_x, bearing_y;
    f32 advance_x;
};

struct FontSlot {
    FT_Face ft_face = nullptr;
    hb_font_t* hb_font = nullptr;
    bool in_use = false;
};

struct TextEngine::Impl {
    FT_Library ft_library = nullptr;
    FontSlot fonts[MAX_FONTS] = {};

    // Glyph atlas
    u8 atlas_pixels[ATLAS_SIZE * ATLAS_SIZE] = {};
    bool atlas_dirty = false;
    u32 atlas_cursor_x = 1; // current packing position
    u32 atlas_cursor_y = 1;
    u32 atlas_row_height = 0;

    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> glyph_cache;

    // Per-shape glyph buffers. Each shape() call gets its own vector.
    // Moving inner vectors (when the outer vector grows) doesn't move heap data,
    // so TextRun::glyphs pointers into inner vectors remain valid for the frame.
    std::vector<std::vector<TextRun::Glyph>> glyph_runs;
    std::vector<TextLayout::Line> scratch_lines;

    CachedGlyph* rasterize_glyph(FontHandle font, u32 glyph_id, u32 pixel_size);
};

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

bool TextEngine::init(RHI* rhi) {
    rhi_ = rhi;
    impl_ = new Impl();

    if (FT_Init_FreeType(&impl_->ft_library) != 0) {
        std::fprintf(stderr, "ultragui: FT_Init_FreeType failed\n");
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    // Initialize atlas with transparent pixels. The GPU texture is created
    // on the first flush_atlas() call, which happens before the render pass.
    std::memset(impl_->atlas_pixels, 0, sizeof(impl_->atlas_pixels));
    impl_->atlas_dirty = true;

    return true;
}

void TextEngine::shutdown() {
    if (!impl_)
        return;

    for (u32 i = 0; i < MAX_FONTS; ++i) {
        if (impl_->fonts[i].in_use) {
            if (impl_->fonts[i].hb_font)
                hb_font_destroy(impl_->fonts[i].hb_font);
            if (impl_->fonts[i].ft_face)
                FT_Done_Face(impl_->fonts[i].ft_face);
        }
    }

    FT_Done_FreeType(impl_->ft_library);

    if (atlas_texture_ != INVALID_TEXTURE)
        rhi_->destroy_texture(atlas_texture_);

    delete impl_;
    impl_ = nullptr;
}

// ---------------------------------------------------------------------------
// Font loading
// ---------------------------------------------------------------------------

FontHandle TextEngine::load_font(const char* path) {
    FontHandle handle = INVALID_FONT;
    for (u32 i = 0; i < MAX_FONTS; ++i) {
        if (!impl_->fonts[i].in_use) {
            handle = i;
            break;
        }
    }
    if (handle == INVALID_FONT)
        return INVALID_FONT;

    auto& slot = impl_->fonts[handle];
    if (FT_New_Face(impl_->ft_library, path, 0, &slot.ft_face) != 0) {
        std::fprintf(stderr, "ultragui: failed to load font '%s'\n", path);
        return INVALID_FONT;
    }

    slot.hb_font = hb_ft_font_create_referenced(slot.ft_face);
    slot.in_use = true;

    std::printf("ultragui: loaded font '%s' (%s)\n", path, slot.ft_face->family_name);
    return handle;
}

// ---------------------------------------------------------------------------
// Glyph rasterization
// ---------------------------------------------------------------------------

CachedGlyph* TextEngine::Impl::rasterize_glyph(FontHandle font, u32 glyph_id, u32 pixel_size) {
    GlyphKey key{font, glyph_id, pixel_size};
    auto it = glyph_cache.find(key);
    if (it != glyph_cache.end())
        return &it->second;

    auto& face = fonts[font].ft_face;
    FT_Set_Pixel_Sizes(face, 0, pixel_size);
    if (FT_Load_Glyph(face, glyph_id, FT_LOAD_RENDER) != 0)
        return nullptr;

    auto& bmp = face->glyph->bitmap;
    u32 bw = bmp.width;
    u32 bh = bmp.rows;

    // Pack into atlas
    if (atlas_cursor_x + bw + 1 >= ATLAS_SIZE) {
        atlas_cursor_x = 1;
        atlas_cursor_y += atlas_row_height + 1;
        atlas_row_height = 0;
    }
    if (atlas_cursor_y + bh + 1 >= ATLAS_SIZE) {
        std::fprintf(stderr, "ultragui: glyph atlas full!\n");
        return nullptr;
    }

    // Copy bitmap into atlas
    for (u32 row = 0; row < bh; ++row) {
        u8* dst = atlas_pixels + (atlas_cursor_y + row) * ATLAS_SIZE + atlas_cursor_x;
        u8* src = bmp.buffer + row * bmp.pitch;
        std::memcpy(dst, src, bw);
    }

    CachedGlyph cg{};
    cg.u0 = static_cast<f32>(atlas_cursor_x) / ATLAS_SIZE;
    cg.v0 = static_cast<f32>(atlas_cursor_y) / ATLAS_SIZE;
    cg.u1 = static_cast<f32>(atlas_cursor_x + bw) / ATLAS_SIZE;
    cg.v1 = static_cast<f32>(atlas_cursor_y + bh) / ATLAS_SIZE;
    cg.bmp_w = static_cast<f32>(bw);
    cg.bmp_h = static_cast<f32>(bh);
    cg.bearing_x = static_cast<f32>(face->glyph->bitmap_left);
    cg.bearing_y = static_cast<f32>(face->glyph->bitmap_top);
    cg.advance_x = static_cast<f32>(face->glyph->advance.x) / 64.0f;

    atlas_cursor_x += bw + 1;
    if (bh > atlas_row_height)
        atlas_row_height = bh;

    atlas_dirty = true;
    auto [inserted, _] = glyph_cache.emplace(key, cg);
    return &inserted->second;
}

// ---------------------------------------------------------------------------
// Text shaping (HarfBuzz)
// ---------------------------------------------------------------------------

void TextEngine::begin_frame() {
    if (impl_) {
        impl_->glyph_runs.clear();
        impl_->scratch_lines.clear();
    }
}

TextRun TextEngine::shape(FontHandle font, const char* text, u32 text_len, f32 font_size,
                          f32 letter_spacing, f32 line_height_mult) {
    assert(impl_ && font < MAX_FONTS && impl_->fonts[font].in_use);

    // Rasterize glyphs at physical pixel size for sharpness on HiDPI displays.
    // All returned metrics (bmp_w, bearing, advance, ascent, etc.) are scaled
    // back to window coordinates so the vertex layout is unchanged.
    f32 dpi = rhi_ ? rhi_->dpi_scale() : 1.0f;
    f32 inv_dpi = 1.0f / dpi;

    auto& slot = impl_->fonts[font];
    u32 pixel_size = static_cast<u32>(font_size * dpi + 0.5f);
    FT_Set_Pixel_Sizes(slot.ft_face, 0, pixel_size);
    hb_ft_font_changed(slot.hb_font);

    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text, static_cast<int>(text_len), 0, static_cast<int>(text_len));
    hb_buffer_guess_segment_properties(buf);
    hb_shape(slot.hb_font, buf, nullptr, 0);

    u32 glyph_count;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    // Each shape() call gets its own glyph vector. The outer vector may
    // reallocate on push_back, but that only moves the inner vector metadata -
    // the heap-allocated glyph array stays in place, so pointers remain valid.
    impl_->glyph_runs.emplace_back(glyph_count);
    auto& glyphs = impl_->glyph_runs.back();

    f32 cursor_x = 0.0f;
    for (u32 i = 0; i < glyph_count; ++i) {
        auto& g = glyphs[i];
        g.glyph_id = glyph_info[i].codepoint;
        // HarfBuzz positions are in FreeType 26.6 fixed-point at the DPI-scaled size;
        // scale back to window coords.
        g.x_offset = static_cast<f32>(glyph_pos[i].x_offset) / 64.0f * inv_dpi;
        g.y_offset = static_cast<f32>(glyph_pos[i].y_offset) / 64.0f * inv_dpi;
        g.x_advance = static_cast<f32>(glyph_pos[i].x_advance) / 64.0f * inv_dpi + letter_spacing;

        auto* cached = impl_->rasterize_glyph(font, g.glyph_id, pixel_size);
        if (cached) {
            g.u0 = cached->u0;
            g.v0 = cached->v0;
            g.u1 = cached->u1;
            g.v1 = cached->v1;
            // Atlas stores full-resolution bitmaps; scale dimensions to window coords
            // so the vertex quad covers the right screen area.
            g.bmp_w = cached->bmp_w * inv_dpi;
            g.bmp_h = cached->bmp_h * inv_dpi;
            g.bearing_x = cached->bearing_x * inv_dpi;
            g.bearing_y = cached->bearing_y * inv_dpi;
        }

        cursor_x += g.x_advance;
    }

    hb_buffer_destroy(buf);

    // Font metrics from FreeType are at the DPI-scaled size; convert to window coords.
    f32 ascent = static_cast<f32>(slot.ft_face->size->metrics.ascender) / 64.0f * inv_dpi;
    f32 descent = static_cast<f32>(slot.ft_face->size->metrics.descender) / 64.0f * inv_dpi;
    f32 line_height =
        static_cast<f32>(slot.ft_face->size->metrics.height) / 64.0f * inv_dpi * line_height_mult;

    TextRun run{};
    run.glyphs = glyphs.data();
    run.glyph_count = glyph_count;
    run.total_advance = cursor_x;
    run.ascent = ascent;
    run.descent = descent;
    run.line_height = line_height;
    return run;
}

// ---------------------------------------------------------------------------
// Text layout (line breaking, alignment)
// ---------------------------------------------------------------------------

TextLayout TextEngine::layout(const TextRun& run, const TextLayoutConfig& config) {
    impl_->scratch_lines.clear();

    if (run.glyph_count == 0)
        return {};

    // Simple greedy line-breaking
    f32 line_x = 0.0f;
    u32 line_start = 0;
    u32 last_break = 0;
    f32 last_break_x = 0.0f;

    for (u32 i = 0; i < run.glyph_count; ++i) {
        f32 advance = run.glyphs[i].x_advance;

        // Check for word break opportunity (space glyph)
        if (run.glyphs[i].bmp_w == 0) {
            last_break = i + 1;
            last_break_x = line_x + advance;
        }

        if (line_x + advance > config.max_width && i > line_start) {
            u32 break_at = (last_break > line_start) ? last_break : i;
            f32 break_width = (break_at == last_break) ? last_break_x : line_x;

            impl_->scratch_lines.push_back({line_start, break_at - line_start, break_width, 0});

            line_start = break_at;
            line_x = (break_at == last_break) ? (line_x - last_break_x + advance) : advance;
            last_break = line_start;
            last_break_x = 0.0f;
            continue;
        }

        line_x += advance;
    }

    // Final line
    if (line_start < run.glyph_count) {
        impl_->scratch_lines.push_back({line_start, run.glyph_count - line_start, line_x, 0});
    }

    // Compute y offsets
    f32 y = 0.0f;
    f32 max_width = 0.0f;
    for (auto& line : impl_->scratch_lines) {
        line.y_offset = y;
        y += run.line_height;
        if (line.width > max_width)
            max_width = line.width;
    }

    TextLayout result{};
    result.lines = impl_->scratch_lines.data();
    result.line_count = static_cast<u32>(impl_->scratch_lines.size());
    result.total_width = max_width;
    result.total_height = y;
    return result;
}

// ---------------------------------------------------------------------------
// Atlas upload
// ---------------------------------------------------------------------------

void TextEngine::flush_atlas() {
    if (!impl_->atlas_dirty)
        return;

    if (atlas_texture_ == INVALID_TEXTURE) {
        // First time: create the texture (includes upload).
        // NEAREST filter prevents bilinear bleed between packed glyphs.
        atlas_texture_ = rhi_->create_texture(ATLAS_SIZE, ATLAS_SIZE, RHIFormat::R8_UNORM,
                                              impl_->atlas_pixels, RHIFilter::Nearest);
    } else {
        // Subsequent: update in place (handle stays stable).
        rhi_->update_texture(atlas_texture_, impl_->atlas_pixels);
    }
    impl_->atlas_dirty = false;
}

Vec2 TextEngine::atlas_size() const {
    return {static_cast<f32>(ATLAS_SIZE), static_cast<f32>(ATLAS_SIZE)};
}

} // namespace ugui
