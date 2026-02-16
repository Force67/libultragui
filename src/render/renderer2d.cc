#include <ultragui/render/renderer2d.h>

#include <cmath>

namespace ugui {

bool Renderer2D::Init(RHI* rhi) {
    rhi_ = rhi;
    vertices_.reserve(4096);
    indices_.reserve(8192);
    batches_.reserve(64);
    text_vertices_.reserve(4096);
    text_indices_.reserve(8192);
    text_batches_.reserve(16);
    return true;
}

void Renderer2D::Shutdown() {
    rhi_ = nullptr;
    vertices_.clear();
    indices_.clear();
    batches_.clear();
    text_vertices_.clear();
    text_indices_.clear();
    text_batches_.clear();
}

void Renderer2D::BeginFrame() {
    vertices_.clear();
    indices_.clear();
    batches_.clear();
    text_vertices_.clear();
    text_indices_.clear();
    text_batches_.clear();
    scissor_stack_.clear();
    current_texture_ = kInvalidTexture;
    current_text_atlas_ = kInvalidTexture;
    current_scissor_ = Rect{0, 0, rhi_->display_size().x, rhi_->display_size().y};
}

void Renderer2D::EndFrame() {
    FlushBatch();
    FlushTextBatch();

    // Draw quads first
    for (auto& batch : batches_) {
        rhi_->SetScissor(batch.scissor);
        rhi_->DrawTriangles(vertices_.data(), static_cast<u32>(vertices_.size()),
                             indices_.data() + batch.index_offset, batch.index_count,
                             batch.texture);
    }

    // Then draw text on top
    for (auto& batch : text_batches_) {
        rhi_->SetScissor(batch.scissor);
        rhi_->DrawTextTriangles(text_vertices_.data(), static_cast<u32>(text_vertices_.size()),
                                  text_indices_.data() + batch.index_offset, batch.index_count,
                                  batch.texture);
    }
}

void Renderer2D::DrawRect(Rect rect, Color color, u32 corner_radii) {
    u32 packed = Vertex2D::PackColor(color.r, color.g, color.b, color.a);
    EmitQuad(rect, packed, packed, corner_radii, 0.0f, 0.0f, 0, kInvalidTexture);
}

void Renderer2D::DrawTexturedRect(Rect rect, RHITextureHandle texture, Color tint,
                                    u32 corner_radii) {
    u32 packed = Vertex2D::PackColor(tint.r, tint.g, tint.b, tint.a);
    EmitQuad(rect, packed, packed, corner_radii, 0.0f, 0.0f, 0, texture);
}

void Renderer2D::DrawRectGradient(Rect rect, Color top_color, Color bottom_color,
                                    u32 corner_radii) {
    u32 c1 = Vertex2D::PackColor(top_color.r, top_color.g, top_color.b, top_color.a);
    u32 c2 = Vertex2D::PackColor(bottom_color.r, bottom_color.g, bottom_color.b, bottom_color.a);
    EmitQuad(rect, c1, c2, corner_radii, 0.0f, 0.0f, 0, kInvalidTexture);
}

void Renderer2D::DrawShadow(Rect rect, Color shadow_color, f32 blur, f32 spread, Vec2 offset,
                             u32 corner_radii) {
    Rect shadow_rect = {
        rect.x + offset.x - spread - blur,
        rect.y + offset.y - spread - blur,
        rect.w + (spread + blur) * 2.0f,
        rect.h + (spread + blur) * 2.0f,
    };
    u32 packed = Vertex2D::PackColor(shadow_color.r, shadow_color.g, shadow_color.b,
                                      shadow_color.a);
    // Add spread to each corner radius
    u32 tl = (corner_radii & 0xFFu);
    u32 tr = ((corner_radii >> 8) & 0xFFu);
    u32 br = ((corner_radii >> 16) & 0xFFu);
    u32 bl = ((corner_radii >> 24) & 0xFFu);
    u32 shadow_radii = Vertex2D::PackRadii(
        static_cast<f32>(tl) + spread, static_cast<f32>(tr) + spread,
        static_cast<f32>(br) + spread, static_cast<f32>(bl) + spread);
    EmitQuad(shadow_rect, packed, packed, shadow_radii, blur, 0.0f, 0, kInvalidTexture);
}

void Renderer2D::DrawBorderedRect(Rect rect, Color fill, Color border_color, f32 border_width,
                                    u32 corner_radii) {
    u32 fill_packed = Vertex2D::PackColor(fill.r, fill.g, fill.b, fill.a);
    u32 border_packed = Vertex2D::PackColor(border_color.r, border_color.g, border_color.b,
                                             border_color.a);
    EmitQuad(rect, fill_packed, fill_packed, corner_radii, 0.0f, border_width, border_packed,
              kInvalidTexture);
}

void Renderer2D::PushScissor(Rect rect) {
    FlushBatch();
    FlushTextBatch();
    scissor_stack_.push_back(current_scissor_);
    current_scissor_ = current_scissor_.Intersected(rect);
}

void Renderer2D::PopScissor() {
    FlushBatch();
    FlushTextBatch();
    if (!scissor_stack_.empty()) {
        current_scissor_ = scissor_stack_.back();
        scissor_stack_.pop_back();
    }
}

void Renderer2D::EmitQuad(Rect rect, u32 color, u32 color2, u32 corner_radii, f32 softness,
                           f32 border_width, u32 border_color, RHITextureHandle texture) {
    // Start a new batch if texture changed
    if (texture != current_texture_) {
        FlushBatch();
        current_texture_ = texture;
    }

    // Snap rect edges to physical pixel boundaries for crisp edges on fractional DPI.
    // Without this, a panel at x=10.3 on 1.65x DPI straddles framebuffer pixels,
    // causing the SDF anti-aliasing to blur across an extra pixel.
    f32 dpi = rhi_ ? rhi_->dpi_scale() : 1.0f;
    if (dpi != 1.0f) {
        f32 inv = 1.0f / dpi;
        f32 x0 = std::round(rect.x * dpi) * inv;
        f32 y0 = std::round(rect.y * dpi) * inv;
        f32 x1 = std::round((rect.x + rect.w) * dpi) * inv;
        f32 y1 = std::round((rect.y + rect.h) * dpi) * inv;
        rect = {x0, y0, x1 - x0, y1 - y0};
    }

    f32 hw = rect.w * 0.5f;
    f32 hh = rect.h * 0.5f;

    u32 base = static_cast<u32>(vertices_.size());

    // Top-left
    vertices_.push_back({{rect.x, rect.y}, {0, 0}, color, color2, corner_radii, softness,
                         {hw, hh}, border_width, border_color});
    // Top-right
    vertices_.push_back({{rect.x + rect.w, rect.y}, {1, 0}, color, color2, corner_radii,
                         softness, {hw, hh}, border_width, border_color});
    // Bottom-right
    vertices_.push_back({{rect.x + rect.w, rect.y + rect.h}, {1, 1}, color, color2,
                         corner_radii, softness, {hw, hh}, border_width, border_color});
    // Bottom-left
    vertices_.push_back({{rect.x, rect.y + rect.h}, {0, 1}, color, color2, corner_radii,
                         softness, {hw, hh}, border_width, border_color});

    // Two triangles: 0-1-2, 0-2-3
    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
    indices_.push_back(base + 0);
    indices_.push_back(base + 2);
    indices_.push_back(base + 3);
}

void Renderer2D::FlushBatch() {
    u32 idx_offset = 0;
    if (!batches_.empty()) {
        auto& prev = batches_.back();
        idx_offset = prev.index_offset + prev.index_count;
    }

    u32 total_indices = static_cast<u32>(indices_.size());
    if (total_indices <= idx_offset)
        return;

    batches_.push_back({
        current_texture_,
        current_scissor_,
        idx_offset,
        total_indices - idx_offset,
    });
}

void Renderer2D::FlushTextBatch() {
    u32 idx_offset = 0;
    if (!text_batches_.empty()) {
        auto& prev = text_batches_.back();
        idx_offset = prev.index_offset + prev.index_count;
    }

    u32 total_indices = static_cast<u32>(text_indices_.size());
    if (total_indices <= idx_offset)
        return;

    text_batches_.push_back({
        current_text_atlas_,
        current_scissor_,
        idx_offset,
        total_indices - idx_offset,
    });
}

void Renderer2D::DrawText(Vec2 pos, const TextRun& run, Color color,
                           RHITextureHandle atlas_texture) {
    if (atlas_texture != current_text_atlas_) {
        FlushTextBatch();
        current_text_atlas_ = atlas_texture;
    }

    u32 packed_color = Vertex2D::PackColor(color.r, color.g, color.b, color.a);
    f32 cursor_x = pos.x;
    f32 baseline_y = pos.y + run.ascent;

    // Snap glyph positions to physical pixel boundaries to prevent sub-pixel blur.
    // On fractional DPI (e.g., 1.65x), unsnapped positions straddle framebuffer
    // pixels, causing the GPU to bilinear-filter the glyph texture -> fuzz.
    f32 dpi = rhi_ ? rhi_->dpi_scale() : 1.0f;
    f32 inv_dpi = 1.0f / dpi;

    for (u32 i = 0; i < run.glyph_count; ++i) {
        auto& g = run.glyphs[i];
        if (g.bmp_w <= 0 || g.bmp_h <= 0) {
            cursor_x += g.x_advance;
            continue;
        }

        // Snap glyph position AND size to physical pixel grid.
        // Position snap prevents sub-pixel blur from bilinear interpolation.
        // Size snap ensures the glyph quad covers an integer number of physical pixels.
        f32 x = std::round((cursor_x + g.bearing_x + g.x_offset) * dpi) * inv_dpi;
        f32 y = std::round((baseline_y - g.bearing_y + g.y_offset) * dpi) * inv_dpi;
        f32 w = std::round(g.bmp_w * dpi) * inv_dpi;
        f32 h = std::round(g.bmp_h * dpi) * inv_dpi;

        u32 base = static_cast<u32>(text_vertices_.size());
        text_vertices_.push_back({{x, y}, {g.u0, g.v0}, packed_color, packed_color,
                                  0, 0, {0, 0}, 0, 0});
        text_vertices_.push_back({{x + w, y}, {g.u1, g.v0}, packed_color, packed_color,
                                  0, 0, {0, 0}, 0, 0});
        text_vertices_.push_back({{x + w, y + h}, {g.u1, g.v1}, packed_color, packed_color,
                                  0, 0, {0, 0}, 0, 0});
        text_vertices_.push_back({{x, y + h}, {g.u0, g.v1}, packed_color, packed_color,
                                  0, 0, {0, 0}, 0, 0});

        text_indices_.push_back(base + 0);
        text_indices_.push_back(base + 1);
        text_indices_.push_back(base + 2);
        text_indices_.push_back(base + 0);
        text_indices_.push_back(base + 2);
        text_indices_.push_back(base + 3);

        cursor_x += g.x_advance;
    }
}

void Renderer2D::DrawTextLayout(Vec2 pos, const TextRun& run, const TextLayout& layout,
                                  Color color, RHITextureHandle atlas_texture, f32 max_width) {
    if (layout.line_count == 0)
        return;

    for (u32 li = 0; li < layout.line_count; ++li) {
        auto& line = layout.lines[li];

        f32 x_offset = 0.0f;
        if (max_width > 0.0f) {
            f32 slack = max_width - line.width;
            x_offset = 0.0f;
            (void)slack;
        }

        TextRun line_run = run;
        line_run.glyphs = run.glyphs + line.glyph_start;
        line_run.glyph_count = line.glyph_count;

        DrawText(Vec2{pos.x + x_offset, pos.y + line.y_offset}, line_run, color, atlas_texture);
    }
}

} // namespace ugui
