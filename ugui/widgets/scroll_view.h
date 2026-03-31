#ifndef ULTRAGUI_WIDGETS_SCROLL_VIEW_H_
#define ULTRAGUI_WIDGETS_SCROLL_VIEW_H_

#include <ugui/widgets/widget.h>

namespace ugui {

/// Scrollable container. Content can exceed the viewport;
/// the scroll view clips and translates its children.
class ScrollView : public Widget {
 public:
  static constexpr WidgetKind kKind = WidgetKind::kScrollView;
  WidgetKind kind() const override { return kKind; }

 public:
  using Widget::Widget;

  Vec2 scroll_offset() const { return scroll_offset_; }
  void set_scroll_offset(Vec2 offset) { scroll_offset_ = offset; }
  void ScrollBy(Vec2 delta);

  Vec2 content_size() const { return content_size_; }

  /// Normalized scroll progress (0 = top, 1 = bottom)
  f32 scroll_progress() const {
    f32 max_y = content_size_.y - content_rect_.h;
    return (max_y > 0.0f) ? scroll_offset_.y / max_y : 0.0f;
  }

  bool OnScroll(Vec2 delta) override;

  void OnLayout(const Rect& rect, const Rect& content_rect) override;
  Widget* HitTest(Vec2 point) override;
  void OnPaint(Renderer2D& renderer) override;
  void OnUpdate(f64 dt) override;

 private:
  Vec2 scroll_offset_ = Vec2::Zero();
  Vec2 scroll_velocity_ = Vec2::Zero();
  Vec2 content_size_ = Vec2::Zero();
  f32 deceleration_ = 0.88f;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_SCROLL_VIEW_H_
