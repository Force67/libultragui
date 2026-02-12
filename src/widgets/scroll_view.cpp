#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/scroll_view.h>

#include <algorithm>
#include <cmath>

namespace ugui {

void ScrollView::scroll_by(Vec2 delta) {
    scroll_offset_ += delta;
    mark_paint_dirty();
}

bool ScrollView::on_scroll(Vec2 delta) {
    scroll_by(delta);
    return true;
}

void ScrollView::on_layout(const Rect& rect, const Rect& content_rect) {
    Widget::on_layout(rect, content_rect);

    // Compute total content size from children
    f32 max_x = 0, max_y = 0;
    for (auto* child : children_) {
        Rect cr = child->rect();
        f32 r = cr.x + cr.w - content_rect.x;
        f32 b = cr.y + cr.h - content_rect.y;
        if (r > max_x)
            max_x = r;
        if (b > max_y)
            max_y = b;
    }
    content_size_ = {max_x, max_y};

    // Clamp scroll offset
    f32 max_scroll_x = std::max(0.0f, content_size_.x - content_rect.w);
    f32 max_scroll_y = std::max(0.0f, content_size_.y - content_rect.h);
    scroll_offset_.x = std::clamp(scroll_offset_.x, 0.0f, max_scroll_x);
    scroll_offset_.y = std::clamp(scroll_offset_.y, 0.0f, max_scroll_y);
}

void ScrollView::on_update(f64 dt) {
    // Inertial scrolling
    if (scroll_velocity_.length_sq() > 0.01f) {
        scroll_offset_ += scroll_velocity_ * static_cast<f32>(dt);
        scroll_velocity_ *= deceleration_;

        // Clamp
        f32 max_scroll_x = std::max(0.0f, content_size_.x - content_rect_.w);
        f32 max_scroll_y = std::max(0.0f, content_size_.y - content_rect_.h);
        scroll_offset_.x = std::clamp(scroll_offset_.x, 0.0f, max_scroll_x);
        scroll_offset_.y = std::clamp(scroll_offset_.y, 0.0f, max_scroll_y);

        mark_paint_dirty();
    } else {
        scroll_velocity_ = Vec2::zero();
    }
}

void ScrollView::on_paint(Renderer2D& renderer) {
    auto s = computed_style();

    // Background
    if (s.background.a > 0.0f) {
        u32 radii = (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
                     s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f)
                        ? Vertex2D::pack_radii(s.corner_radius_tl, s.corner_radius_tr,
                                               s.corner_radius_br, s.corner_radius_bl)
                        : Vertex2D::pack_radii(s.corner_radius);
        renderer.draw_rect(rect_, s.background.with_alpha(s.background.a * s.opacity),
                           radii);
    }

    // Clip children to content rect
    renderer.push_scissor(content_rect_);

    // Children are painted with scroll offset applied
    // (the UIContext will handle offsetting child positions)

    // Scrollbar indicator
    if (content_size_.y > content_rect_.h) {
        f32 visible_ratio = content_rect_.h / content_size_.y;
        f32 bar_height = std::max(content_rect_.h * visible_ratio, 20.0f);
        f32 scroll_ratio = scroll_offset_.y / (content_size_.y - content_rect_.h);
        f32 bar_y = content_rect_.y + scroll_ratio * (content_rect_.h - bar_height);

        renderer.draw_rect({content_rect_.right() - 4, bar_y, 4, bar_height}, Color{1, 1, 1, 0.3f},
                           Vertex2D::pack_radii(2.0f));
    }

    renderer.pop_scissor();
}

} // namespace ugui
