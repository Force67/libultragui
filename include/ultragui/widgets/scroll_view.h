#pragma once

#include <ultragui/widgets/widget.h>

namespace ugui {

/// Scrollable container. Content can exceed the viewport;
/// the scroll view clips and translates its children.
class ScrollView : public Widget {
public:
    using Widget::Widget;

    Vec2 scroll_offset() const { return scroll_offset_; }
    void set_scroll_offset(Vec2 offset) { scroll_offset_ = offset; }
    void scroll_by(Vec2 delta);

    Vec2 content_size() const { return content_size_; }

    bool on_scroll(Vec2 delta) override;

    void on_layout(const Rect& rect, const Rect& content_rect) override;
    void on_paint(Renderer2D& renderer) override;
    void on_update(f64 dt) override;

private:
    Vec2 scroll_offset_ = Vec2::zero();
    Vec2 scroll_velocity_ = Vec2::zero();
    Vec2 content_size_ = Vec2::zero();
    f32 deceleration_ = 0.95f;
};

} // namespace ugui
