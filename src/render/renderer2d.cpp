#include <ultragui/render/renderer2d.h>

namespace ugui {

bool Renderer2D::init(RHI* rhi) {
    rhi_ = rhi;
    vertices_.reserve(4096);
    indices_.reserve(8192);
    batches_.reserve(64);
    text_vertices_.reserve(4096);
    text_indices_.reserve(8192);
    text_batches_.reserve(16);
    return true;
}

void Renderer2D::shutdown() {
    rhi_ = nullptr;
    vertices_.clear();
    indices_.clear();
    batches_.clear();
    text_vertices_.clear();
    text_indices_.clear();
    text_batches_.clear();
}

void Renderer2D::begin_frame() {
    vertices_.clear();
    indices_.clear();
    batches_.clear();
    text_vertices_.clear();
    text_indices_.clear();
    text_batches_.clear();
    scissor_stack_.clear();
    current_texture_ = INVALID_TEXTURE;
    current_text_atlas_ = INVALID_TEXTURE;
    current_scissor_ = Rect{0, 0, rhi_->display_size().x, rhi_->display_size().y};
}

void Renderer2D::end_frame() {
    flush_batch();
    flush_text_batch();

    // Draw quads first
    for (auto& batch : batches_) {
        rhi_->set_scissor(batch.scissor);
        rhi_->draw_triangles(vertices_.data(), static_cast<u32>(vertices_.size()),
                             indices_.data() + batch.index_offset, batch.index_count,
                             batch.texture);
    }

    // Then draw text on top
    for (auto& batch : text_batches_) {
        rhi_->set_scissor(batch.scissor);
        rhi_->draw_text_triangles(text_vertices_.data(), static_cast<u32>(text_vertices_.size()),
                                  text_indices_.data() + batch.index_offset, batch.index_count,
                                  batch.texture);
    }
}

void Renderer2D::draw_rect(Rect rect, Color color, f32 corner_radius) {
    emit_quad(rect, Vertex2D::pack_color(color.r, color.g, color.b, color.a), corner_radius,
              INVALID_TEXTURE);
}

void Renderer2D::draw_textured_rect(Rect rect, RHITextureHandle texture, Color tint,
                                    f32 corner_radius) {
    emit_quad(rect, Vertex2D::pack_color(tint.r, tint.g, tint.b, tint.a), corner_radius, texture);
}

void Renderer2D::push_scissor(Rect rect) {
    flush_batch();
    flush_text_batch();
    scissor_stack_.push_back(current_scissor_);
    current_scissor_ = current_scissor_.intersected(rect);
}

void Renderer2D::pop_scissor() {
    flush_batch();
    flush_text_batch();
    if (!scissor_stack_.empty()) {
        current_scissor_ = scissor_stack_.back();
        scissor_stack_.pop_back();
    }
}

void Renderer2D::emit_quad(Rect rect, u32 color, f32 corner_radius, RHITextureHandle texture) {
    // Start a new batch if texture or scissor changed
    if (texture != current_texture_) {
        flush_batch();
        current_texture_ = texture;
    }

    f32 hw = rect.w * 0.5f;
    f32 hh = rect.h * 0.5f;

    u32 base = static_cast<u32>(vertices_.size());

    // Top-left
    vertices_.push_back({{rect.x, rect.y}, {0, 0}, color, corner_radius, {hw, hh}});
    // Top-right
    vertices_.push_back({{rect.x + rect.w, rect.y}, {1, 0}, color, corner_radius, {hw, hh}});
    // Bottom-right
    vertices_.push_back(
        {{rect.x + rect.w, rect.y + rect.h}, {1, 1}, color, corner_radius, {hw, hh}});
    // Bottom-left
    vertices_.push_back({{rect.x, rect.y + rect.h}, {0, 1}, color, corner_radius, {hw, hh}});

    // Two triangles: 0-1-2, 0-2-3
    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
    indices_.push_back(base + 0);
    indices_.push_back(base + 2);
    indices_.push_back(base + 3);
}

void Renderer2D::flush_batch() {
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

void Renderer2D::flush_text_batch() {
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

void Renderer2D::draw_text(Vec2 pos, const TextRun& run, Color color,
                           RHITextureHandle atlas_texture) {
    if (atlas_texture != current_text_atlas_) {
        flush_text_batch();
        current_text_atlas_ = atlas_texture;
    }

    u32 packed_color = Vertex2D::pack_color(color.r, color.g, color.b, color.a);
    f32 cursor_x = pos.x;
    f32 baseline_y = pos.y + run.ascent;

    for (u32 i = 0; i < run.glyph_count; ++i) {
        auto& g = run.glyphs[i];
        if (g.bmp_w <= 0 || g.bmp_h <= 0) {
            cursor_x += g.x_advance;
            continue;
        }

        f32 x = cursor_x + g.bearing_x + g.x_offset;
        f32 y = baseline_y - g.bearing_y + g.y_offset;
        f32 w = g.bmp_w;
        f32 h = g.bmp_h;

        u32 base = static_cast<u32>(text_vertices_.size());
        text_vertices_.push_back({{x, y}, {g.u0, g.v0}, packed_color, 0, {0, 0}});
        text_vertices_.push_back({{x + w, y}, {g.u1, g.v0}, packed_color, 0, {0, 0}});
        text_vertices_.push_back({{x + w, y + h}, {g.u1, g.v1}, packed_color, 0, {0, 0}});
        text_vertices_.push_back({{x, y + h}, {g.u0, g.v1}, packed_color, 0, {0, 0}});

        text_indices_.push_back(base + 0);
        text_indices_.push_back(base + 1);
        text_indices_.push_back(base + 2);
        text_indices_.push_back(base + 0);
        text_indices_.push_back(base + 2);
        text_indices_.push_back(base + 3);

        cursor_x += g.x_advance;
    }
}

void Renderer2D::draw_text_layout(Vec2 pos, const TextRun& run, const TextLayout& layout,
                                  Color color, RHITextureHandle atlas_texture, f32 max_width) {
    if (layout.line_count == 0)
        return;

    for (u32 li = 0; li < layout.line_count; ++li) {
        auto& line = layout.lines[li];

        // Text alignment offset
        f32 x_offset = 0.0f;
        if (max_width > 0.0f) {
            f32 slack = max_width - line.width;
            // We don't have config here, so just left-align
            // Proper alignment handled by caller
            x_offset = 0.0f;
            (void)slack;
        }

        // Build a sub-run for this line
        TextRun line_run = run;
        line_run.glyphs = run.glyphs + line.glyph_start;
        line_run.glyph_count = line.glyph_count;

        draw_text(Vec2{pos.x + x_offset, pos.y + line.y_offset}, line_run, color, atlas_texture);
    }
}

} // namespace ugui
