#include <ugui/layout/layout_tree.h>
#include <ugui/render/paint.h>
#include <ugui/render/renderer2d.h>
#include <ugui/style/style.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/widget.h>

namespace ugui {

static void PaintWidgetTreeImpl(Widget* widget, Renderer2D& renderer,
                                Vec2 scroll_offset) {
  if (!widget) return;

  auto s = widget->ComputedStyle();
  if (s.visibility == Visibility::kHidden ||
      s.visibility == Visibility::kCollapsed)
    return;

  // Apply accumulated scroll offset to this widget's paint position
  // by temporarily shifting its rect
  Rect original_rect = widget->rect();
  Rect original_content = widget->content_rect();
  if (scroll_offset.x != 0.0f || scroll_offset.y != 0.0f) {
    Rect shifted = {original_rect.x - scroll_offset.x,
                    original_rect.y - scroll_offset.y, original_rect.w,
                    original_rect.h};
    Rect shifted_content = {original_content.x - scroll_offset.x,
                            original_content.y - scroll_offset.y,
                            original_content.w, original_content.h};
    LayoutNode tmp;
    tmp.computed_rect = shifted;
    tmp.content_rect = shifted_content;
    widget->ApplyLayoutResult(tmp);
  }

  bool is_scroll_view = widget_cast<ScrollView>(widget) != nullptr;
  bool clip = is_scroll_view || s.overflow == Overflow::kHidden ||
              s.overflow == Overflow::kScroll;
  Rect clip_rect = is_scroll_view ? widget->content_rect() : widget->rect();
  if (clip) renderer.PushScissor(clip_rect);

  widget->OnPaint(renderer);

  // If this widget is a ScrollView, add its scroll offset for children
  Vec2 child_offset = scroll_offset;
  if (auto* sv = widget_cast<ScrollView>(widget))
    child_offset = child_offset + sv->scroll_offset();

  for (auto* child : widget->child_ptrs())
    PaintWidgetTreeImpl(child, renderer, child_offset);

  if (clip) renderer.PopScissor();

  // Restore original rect
  if (scroll_offset.x != 0.0f || scroll_offset.y != 0.0f) {
    LayoutNode tmp;
    tmp.computed_rect = original_rect;
    tmp.content_rect = original_content;
    widget->ApplyLayoutResult(tmp);
  }
}

void PaintWidgetTree(Widget* widget, Renderer2D& renderer) {
  PaintWidgetTreeImpl(widget, renderer, Vec2::Zero());
}

}  // namespace ugui
