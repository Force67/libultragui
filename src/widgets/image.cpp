#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/image.h>

namespace ugui {

void Image::measure(f32& out_width, f32& out_height) {
    out_width = natural_w_;
    out_height = natural_h_;
}

void Image::on_paint(Renderer2D& renderer) {
    auto s = computed_style();
    if (texture_ != INVALID_TEXTURE) {
        renderer.draw_textured_rect(rect_, texture_, Color::white().with_alpha(s.opacity),
                                    s.corner_radius);
    }
}

} // namespace ugui
