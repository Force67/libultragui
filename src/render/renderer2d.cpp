#include <ultragui/render/renderer2d.h>

namespace ugui {

bool Renderer2D::init(RHI* rhi) {
    rhi_ = rhi;
    vertices_.reserve(4096);
    indices_.reserve(8192);
    batches_.reserve(64);
    return true;
}

void Renderer2D::shutdown() {
    rhi_ = nullptr;
    vertices_.clear();
    indices_.clear();
    batches_.clear();
}

void Renderer2D::begin_frame() {
    vertices_.clear();
    indices_.clear();
    batches_.clear();
    scissor_stack_.clear();
    current_texture_ = INVALID_TEXTURE;
    current_scissor_ = Rect{0, 0, rhi_->display_size().x, rhi_->display_size().y};
}

void Renderer2D::end_frame() {
    flush_batch();

    for (auto& batch : batches_) {
        rhi_->set_scissor(batch.scissor);
        rhi_->draw_triangles(vertices_.data(), static_cast<u32>(vertices_.size()),
                             indices_.data() + batch.index_offset, batch.index_count,
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
    scissor_stack_.push_back(current_scissor_);
    current_scissor_ = current_scissor_.intersected(rect);
}

void Renderer2D::pop_scissor() {
    flush_batch();
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

} // namespace ugui
