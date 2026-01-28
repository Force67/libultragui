#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/panel.h>

namespace ugui {

void Panel::on_paint(Renderer2D& renderer) {
    // Use base Widget paint for shadow, background, gradient, border
    Widget::on_paint(renderer);
    // Children paint themselves via UIContext traversal
}

} // namespace ugui
