#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/image.h>

namespace ugui {

void Image::measure(f32& out_width, f32& out_height) {
    out_width = natural_w_;
    out_height = natural_h_;
}

void Image::on_paint(Renderer2D& renderer) {
    auto s = computed_style();
    if (texture_ != INVALID_TEXTURE) {
        u32 radii = (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
                     s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f)
                        ? Vertex2D::pack_radii(s.corner_radius_tl, s.corner_radius_tr,
                                               s.corner_radius_br, s.corner_radius_bl)
                        : Vertex2D::pack_radii(s.corner_radius);
        renderer.draw_textured_rect(rect_, texture_, Color::white().with_alpha(s.opacity),
                                    radii);
    }
}

} // namespace ugui
