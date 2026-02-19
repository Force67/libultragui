#include <ultragui/render/paint.h>
#include <ultragui/layout/layout_tree.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/scroll_view.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

static void PaintWidgetTreeImpl(Widget* widget, Renderer2D& renderer, Vec2 scroll_offset) {
    if (!widget)
        return;

    auto s = widget->ComputedStyle();
    if (s.visibility == Visibility::kHidden || s.visibility == Visibility::kCollapsed)
        return;

    // Apply accumulated scroll offset to this widget's paint position
    // by temporarily shifting its rect
    Rect original_rect = widget->rect();
    Rect original_content = widget->content_rect();
    if (scroll_offset.x != 0.0f || scroll_offset.y != 0.0f) {
        Rect shifted = {original_rect.x - scroll_offset.x,
                        original_rect.y - scroll_offset.y,
                        original_rect.w, original_rect.h};
        Rect shifted_content = {original_content.x - scroll_offset.x,
                                original_content.y - scroll_offset.y,
                                original_content.w, original_content.h};
        LayoutNode tmp;
        tmp.computed_rect = shifted;
        tmp.content_rect = shifted_content;
        widget->ApplyLayoutResult(tmp);
    }

    bool clip = s.overflow == Overflow::kHidden || s.overflow == Overflow::kScroll;
    if (clip)
        renderer.PushScissor(widget->rect());

    widget->OnPaint(renderer);

    // If this widget is a ScrollView, add its scroll offset for children
    Vec2 child_offset = scroll_offset;
    if (auto* sv = dynamic_cast<ScrollView*>(widget))
        child_offset = child_offset + sv->scroll_offset();

    for (auto* child : widget->children())
        PaintWidgetTreeImpl(child, renderer, child_offset);

    if (clip)
        renderer.PopScissor();

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

} // namespace ugui
