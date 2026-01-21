#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/panel.h>

namespace ugui {

void Panel::on_paint(Renderer2D& renderer) {
    auto s = computed_style();
    f32 alpha = s.background.a * s.opacity;
    if (alpha > 0.0f) {
        renderer.draw_rect(rect_, s.background.with_alpha(alpha), s.corner_radius);
    }
    // Children paint themselves
}

} // namespace ugui
