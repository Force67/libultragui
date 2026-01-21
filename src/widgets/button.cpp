#include <ultragui/render/renderer2d.h>
#include <ultragui/text/text_engine.h>
#include <ultragui/widgets/button.h>

namespace ugui {

void Button::measure(f32& out_width, f32& out_height) {
    if (!text_engine_ || font_ == INVALID_FONT || label_.empty()) {
        out_width = 0;
        out_height = style_.font_size + style_.padding.vertical();
        return;
    }

    auto run = text_engine_->shape(font_, label_.c_str(), static_cast<u32>(label_.size()),
                                   style_.font_size);
    out_width = run.total_advance + style_.padding.horizontal();
    out_height = run.line_height + style_.padding.vertical();
}

void Button::on_paint(Renderer2D& renderer) {
    auto s = computed_style();

    // Button background
    f32 alpha = s.opacity;
    renderer.draw_rect(rect_, s.background.with_alpha(s.background.a * alpha), s.corner_radius);

    // Border
    if (s.border_width > 0.0f && s.border_color.a > 0.0f) {
        // Draw border as a slightly larger rect behind (simple approach)
        // TODO: proper border rendering in shader
    }

    // Label
    if (text_engine_ && font_ != INVALID_FONT && !label_.empty()) {
        auto run = text_engine_->shape(font_, label_.c_str(), static_cast<u32>(label_.size()),
                                       s.font_size);

        // Center text in button
        f32 x = content_rect_.x + (content_rect_.w - run.total_advance) * 0.5f;
        f32 y = content_rect_.y + (content_rect_.h - run.line_height) * 0.5f;

        renderer.draw_text(Vec2{x, y}, run, s.text_color.with_alpha(s.text_color.a * alpha),
                           text_engine_->atlas_texture());
    }
}

} // namespace ugui
