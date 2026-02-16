#include <ultragui/render/paint.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

void PaintWidgetTree(Widget* widget, Renderer2D& renderer) {
    if (!widget)
        return;

    auto s = widget->ComputedStyle();
    if (s.visibility == Visibility::kHidden || s.visibility == Visibility::kCollapsed)
        return;

    bool clip = s.overflow == Overflow::kHidden || s.overflow == Overflow::kScroll;
    if (clip) {
        renderer.PushScissor(widget->rect());
    }

    widget->OnPaint(renderer);

    for (u32 i = 0; i < widget->child_count(); ++i) {
        PaintWidgetTree(widget->ChildAt(i), renderer);
    }

    if (clip) {
        renderer.PopScissor();
    }
}

} // namespace ugui
