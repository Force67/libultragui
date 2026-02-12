#include <ultragui/render/paint.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

void paint_widget_tree(Widget* widget, Renderer2D& renderer) {
    if (!widget)
        return;

    auto s = widget->computed_style();
    if (s.visibility == Visibility::Hidden || s.visibility == Visibility::Collapsed)
        return;

    bool clip = s.overflow == Overflow::Hidden || s.overflow == Overflow::Scroll;
    if (clip) {
        renderer.push_scissor(widget->rect());
    }

    widget->on_paint(renderer);

    for (u32 i = 0; i < widget->child_count(); ++i) {
        paint_widget_tree(widget->child_at(i), renderer);
    }

    if (clip) {
        renderer.pop_scissor();
    }
}

} // namespace ugui
