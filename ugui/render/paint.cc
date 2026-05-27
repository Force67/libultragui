#include <ugui/layout/layout_tree.h>
#include <ugui/render/paint.h>
#include <ugui/render/renderer2d.h>
#include <ugui/style/style.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

static void PaintWidgetTreeImpl(WidgetRegistry& world, wid e,
                                Renderer2D& renderer, Vec2 scroll_offset) {
  Transform* t = world.Get<Transform>(e);
  if (!t) return;

  Style s = ComputedStyle(world, e);
  if (s.visibility == Visibility::kHidden ||
      s.visibility == Visibility::kCollapsed)
    return;

  // Temporarily shift this widget's rect by the accumulated scroll offset.
  Rect original_rect = t->rect;
  Rect original_content = t->content_rect;
  if (scroll_offset.x != 0.0f || scroll_offset.y != 0.0f) {
    t->rect = {original_rect.x - scroll_offset.x,
               original_rect.y - scroll_offset.y, original_rect.w,
               original_rect.h};
    t->content_rect = {original_content.x - scroll_offset.x,
                       original_content.y - scroll_offset.y,
                       original_content.w, original_content.h};
  }

  bool is_scroll_view =
      world.Get<WidgetNode>(e)->kind == WidgetKind::kScrollView;
  bool clip = is_scroll_view || s.overflow == Overflow::kHidden ||
              s.overflow == Overflow::kScroll;
  Rect clip_rect = is_scroll_view ? t->content_rect : t->rect;
  if (clip) renderer.PushScissor(clip_rect);

  PaintWidget(world, e, renderer);

  // If this widget is a scroll view, add its scroll offset for children.
  Vec2 child_offset = scroll_offset + ScrollOffset(world, e);

  for (wid child : world.Get<Hierarchy>(e)->children)
    PaintWidgetTreeImpl(world, child, renderer, child_offset);

  if (clip) renderer.PopScissor();

  // Restore original rect.
  if (scroll_offset.x != 0.0f || scroll_offset.y != 0.0f) {
    t = world.Get<Transform>(e);
    t->rect = original_rect;
    t->content_rect = original_content;
  }
}

void PaintWidgetTree(wid root, Renderer2D& renderer) {
  if (!root.valid()) return;
  PaintWidgetTreeImpl(*WidgetRegistry::Active(), root, renderer, Vec2::Zero());
}

}  // namespace ugui
