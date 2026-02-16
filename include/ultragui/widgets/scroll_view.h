#ifndef ULTRAGUI_WIDGETS_SCROLL_VIEW_H_
#define ULTRAGUI_WIDGETS_SCROLL_VIEW_H_

#include <ultragui/widgets/widget.h>

namespace ugui {

/// Scrollable container. Content can exceed the viewport;
/// the scroll view clips and translates its children.
class ScrollView : public Widget {
public:
    using Widget::Widget;

    Vec2 scroll_offset() const { return scroll_offset_; }
    void set_scroll_offset(Vec2 offset) { scroll_offset_ = offset; }
    void ScrollBy(Vec2 delta);

    Vec2 content_size() const { return content_size_; }

    bool OnScroll(Vec2 delta) override;

    void OnLayout(const Rect& rect, const Rect& content_rect) override;
    void OnPaint(Renderer2D& renderer) override;
    void OnUpdate(f64 dt) override;

private:
    Vec2 scroll_offset_ = Vec2::Zero();
    Vec2 scroll_velocity_ = Vec2::Zero();
    Vec2 content_size_ = Vec2::Zero();
    f32 deceleration_ = 0.95f;
};

} // namespace ugui

#endif  // ULTRAGUI_WIDGETS_SCROLL_VIEW_H_
