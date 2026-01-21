#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/text.h>

namespace ugui {

void Text::measure(f32& out_width, f32& out_height) {
    if (!text_engine_ || font_ == INVALID_FONT || text_.empty()) {
        out_width = 0;
        out_height = 0;
        return;
    }

    auto run =
        text_engine_->shape(font_, text_.c_str(), static_cast<u32>(text_.size()), style_.font_size);
    out_width = run.total_advance;
    out_height = run.line_height;
    cached_run_ = run;
    run_dirty_ = false;
}

void Text::on_paint(Renderer2D& renderer) {
    Widget::on_paint(renderer); // Background

    if (!text_engine_ || font_ == INVALID_FONT || text_.empty())
        return;

    auto s = computed_style();

    if (run_dirty_) {
        cached_run_ =
            text_engine_->shape(font_, text_.c_str(), static_cast<u32>(text_.size()), s.font_size);
        run_dirty_ = false;
    }

    // Position text within content rect
    f32 x = content_rect_.x;
    f32 y = content_rect_.y;

    // Horizontal alignment
    switch (s.text_align) {
    case TextAlign::Center:
        x += (content_rect_.w - cached_run_.total_advance) * 0.5f;
        break;
    case TextAlign::Right:
        x += content_rect_.w - cached_run_.total_advance;
        break;
    default:
        break;
    }

    // Vertical center
    y += (content_rect_.h - cached_run_.line_height) * 0.5f;

    renderer.draw_text(Vec2{x, y}, cached_run_, s.text_color.with_alpha(s.text_color.a * s.opacity),
                       text_engine_->atlas_texture());
}

} // namespace ugui
