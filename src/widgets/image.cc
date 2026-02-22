#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/image.h>

namespace ugui {

void Image::Measure(f32& out_width, f32& out_height) {
    out_width = natural_w_;
    out_height = natural_h_;
}

void Image::OnPaint(Renderer2D& renderer) {
    auto s = ComputedStyle();
    s.Scale(ui_scale());
    if (texture_ != kInvalidTexture) {
        u32 radii = (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
                     s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f)
                        ? Vertex2D::PackRadii(s.corner_radius_tl, s.corner_radius_tr,
                                               s.corner_radius_br, s.corner_radius_bl)
                        : Vertex2D::PackRadii(s.corner_radius);
        renderer.DrawTexturedRect(rect_, texture_, Color::White().WithAlpha(s.opacity),
                                    radii);
    }
}

} // namespace ugui
