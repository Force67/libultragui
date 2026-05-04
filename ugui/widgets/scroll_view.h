#ifndef ULTRAGUI_WIDGETS_SCROLL_VIEW_H_
#define ULTRAGUI_WIDGETS_SCROLL_VIEW_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a scrollable container (WidgetKind::kScrollView): the scroll offset,
/// momentum velocity, measured content size and deceleration. Content can exceed
/// the viewport; the view clips and translates its children. Behaviour lives in
/// ScrollViewVTable().
struct ScrollViewContent {
  Vec2 offset = Vec2::Zero();
  Vec2 velocity = Vec2::Zero();
  Vec2 content_size = Vec2::Zero();
  f32 deceleration = 0.88f;
};

/// Behaviour table (draw + layout + scroll + hit-test + update) for scroll views.
WidgetVTable ScrollViewVTable();

/// Create a scroll-view entity: a generic Widget tagged kScrollView with a
/// ScrollViewContent component.
Widget* CreateScrollView(u32 id);

/// The scroll offset of a scroll view, or (0,0) if `w` is null / not a scroll
/// view. Used by the paint and layout passes to translate descendants.
Vec2 ScrollOffset(const Widget* w);

/// Set the scroll offset. No-op if `w` is null or not a scroll view.
void SetScrollOffset(Widget* w, Vec2 offset);

/// Add to the scroll offset. No-op if `w` is null or not a scroll view.
void ScrollBy(Widget* w, Vec2 delta);

/// Measured total content size, or (0,0) if `w` is null / not a scroll view.
Vec2 ScrollContentSize(const Widget* w);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_SCROLL_VIEW_H_
