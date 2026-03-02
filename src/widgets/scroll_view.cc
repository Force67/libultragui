#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/scroll_view.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>

namespace ugui {

void ScrollView::ScrollBy(Vec2 delta) {
    scroll_offset_ += delta;
    MarkDirty();
}

bool ScrollView::OnScroll(Vec2 delta) {
    // Apply immediately for zero-latency feel
    ScrollBy(delta);
    // Kick velocity for momentum coast after the scroll gesture ends
    scroll_velocity_ = delta * 6.0f;
    return true;
}

void ScrollView::OnLayout(const Rect& rect, const Rect& content_rect) {
    Widget::OnLayout(rect, content_rect);

    // Compute total content size from children (fold)
    content_size_ = std::accumulate(
        children_.begin(), children_.end(), Vec2{0, 0},
        [&content_rect](Vec2 acc, const Widget* child) {
            Rect cr = child->rect();
            return Vec2{std::max(acc.x, cr.x + cr.w - content_rect.x),
                        std::max(acc.y, cr.y + cr.h - content_rect.y)};
        });

    // Clamp scroll offset
    f32 max_scroll_x = std::max(0.0f, content_size_.x - content_rect.w);
    f32 max_scroll_y = std::max(0.0f, content_size_.y - content_rect.h);
    scroll_offset_.x = std::clamp(scroll_offset_.x, 0.0f, max_scroll_x);
    scroll_offset_.y = std::clamp(scroll_offset_.y, 0.0f, max_scroll_y);
}

Widget* ScrollView::HitTest(Vec2 point) {
    if (!rect_.contains(point))
        return nullptr;

    // Children are visually translated by scroll_offset_ in the paint pass.
    // Translate input coordinates back into the children's layout space.
    if (content_rect_.contains(point)) {
        Vec2 child_point = point + scroll_offset_;
        for (auto* child : children_ | std::views::reverse) {
            if (auto* hit = child->HitTest(child_point))
                return hit;
        }
    }
    return this;
}

void ScrollView::OnUpdate(f64 dt) {
    // Inertial scrolling with framerate-independent deceleration
    if (scroll_velocity_.LengthSq() > 0.5f) {
        scroll_offset_ += scroll_velocity_ * static_cast<f32>(dt);

        // Decay normalized to 60fps so momentum feels identical at any framerate
        f32 decay = std::pow(deceleration_, static_cast<f32>(dt) * 60.0f);
        scroll_velocity_ *= decay;

        // Clamp offset to valid range
        f32 max_scroll_x = std::max(0.0f, content_size_.x - content_rect_.w);
        f32 max_scroll_y = std::max(0.0f, content_size_.y - content_rect_.h);
        scroll_offset_.x = std::clamp(scroll_offset_.x, 0.0f, max_scroll_x);
        scroll_offset_.y = std::clamp(scroll_offset_.y, 0.0f, max_scroll_y);

        // Kill velocity at edges to prevent jitter
        if (scroll_offset_.y <= 0.0f || scroll_offset_.y >= max_scroll_y)
            scroll_velocity_.y = 0.0f;
        if (scroll_offset_.x <= 0.0f || scroll_offset_.x >= max_scroll_x)
            scroll_velocity_.x = 0.0f;

        MarkPaintDirty();
    } else {
        scroll_velocity_ = Vec2::Zero();
    }
}

void ScrollView::OnPaint(Renderer2D& renderer) {
    auto s = ComputedStyle();

    // Background
    if (s.background.a > 0.0f) {
        u32 radii = (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
                     s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f)
                        ? Vertex2D::PackRadii(s.corner_radius_tl, s.corner_radius_tr,
                                               s.corner_radius_br, s.corner_radius_bl)
                        : Vertex2D::PackRadii(s.corner_radius);
        renderer.DrawRect(rect_, s.background.WithAlpha(s.background.a * s.opacity),
                           radii);
    }

    // Clip children to content rect
    renderer.PushScissor(content_rect_);

    // Children are painted with scroll offset applied
    // (the UIContext will handle offsetting child positions)

    // Scrollbar indicator
    if (content_size_.y > content_rect_.h) {
        f32 visible_ratio = content_rect_.h / content_size_.y;
        f32 bar_height = std::max(content_rect_.h * visible_ratio, 20.0f);
        f32 scroll_ratio = scroll_offset_.y / (content_size_.y - content_rect_.h);
        f32 bar_y = content_rect_.y + scroll_ratio * (content_rect_.h - bar_height);

        renderer.DrawRect({content_rect_.right() - 4, bar_y, 4, bar_height}, Color{1, 1, 1, 0.3f},
                           Vertex2D::PackRadii(2.0f));
    }

    renderer.PopScissor();
}

} // namespace ugui
