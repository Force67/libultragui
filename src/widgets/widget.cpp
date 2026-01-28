#include <ultragui/layout/layout.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/widget.h>

#include <algorithm>

namespace ugui {

Widget::~Widget() {
    for (auto* child : children_) {
        child->parent_ = nullptr;
    }
}

void Widget::add_child(Widget* child) {
    if (child->parent_)
        child->parent_->remove_child(child);
    child->parent_ = this;
    children_.push_back(child);
    mark_dirty();
}

void Widget::remove_child(Widget* child) {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        (*it)->parent_ = nullptr;
        children_.erase(it);
        mark_dirty();
    }
}

Widget* Widget::child_at(u32 index) const {
    return index < children_.size() ? children_[index] : nullptr;
}

void Widget::add_state_override(WidgetState state, const Style& override_style, u64 mask) {
    state_overrides_.push_back({state, override_style, mask});
}

void Widget::set_widget_state(WidgetState state) {
    if (state_ != state) {
        state_ = state;
        mark_paint_dirty();
    }
}

Style Widget::computed_style() const {
    if (state_overrides_.empty())
        return style_;
    return resolve_style(style_, state_overrides_.data(), static_cast<u32>(state_overrides_.size()),
                         state_);
}

void Widget::mark_dirty() {
    layout_dirty_ = true;
    paint_dirty_ = true;
    // Propagate up to root
    if (parent_)
        parent_->mark_dirty();
}

Widget* Widget::hit_test(Vec2 point) {
    if (!rect_.contains(point))
        return nullptr;

    // Check children in reverse (top-most first)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Widget* hit = (*it)->hit_test(point);
        if (hit)
            return hit;
    }
    return this;
}

void Widget::on_layout(const Rect& rect, const Rect& content_rect) {
    rect_ = rect;
    content_rect_ = content_rect;
    layout_dirty_ = false;
}

void Widget::on_paint(Renderer2D& renderer) {
    auto s = computed_style();
    f32 alpha = s.opacity;

    // Box shadow (drawn before background)
    if (s.has_shadow()) {
        Color sc = s.shadow.color.with_alpha(s.shadow.color.a * alpha);
        renderer.draw_shadow(rect_, sc, s.shadow.blur, s.shadow.spread,
                             s.shadow.offset, s.corner_radius);
    }

    // Background (with optional gradient and border)
    if (s.background.a > 0.0f || s.border_width > 0.0f) {
        Color bg = s.background.with_alpha(s.background.a * alpha);

        if (s.border_width > 0.0f && s.border_color.a > 0.0f) {
            Color bc = s.border_color.with_alpha(s.border_color.a * alpha);
            if (s.has_gradient()) {
                // Border with gradient: draw border first, then gradient fill inset
                renderer.draw_bordered_rect(rect_, Color::transparent(), bc, s.border_width,
                                            s.corner_radius);
                Rect inner = rect_.shrunk(s.border_width);
                f32 inner_radius = s.corner_radius > s.border_width
                                       ? s.corner_radius - s.border_width
                                       : 0.0f;
                Color bg2 = s.background_end.with_alpha(s.background_end.a * alpha);
                renderer.draw_rect_gradient(inner, bg, bg2, inner_radius);
            } else {
                renderer.draw_bordered_rect(rect_, bg, bc, s.border_width, s.corner_radius);
            }
        } else if (s.has_gradient()) {
            Color bg2 = s.background_end.with_alpha(s.background_end.a * alpha);
            renderer.draw_rect_gradient(rect_, bg, bg2, s.corner_radius);
        } else {
            renderer.draw_rect(rect_, bg, s.corner_radius);
        }
    }
}

void Widget::measure(f32& out_width, f32& out_height) {
    out_width = intrinsic_w_;
    out_height = intrinsic_h_;
}

void Widget::populate_layout_node(LayoutNode& node) const {
    node.style = style_;
    node.id = id_;
    node.intrinsic_width = intrinsic_w_;
    node.intrinsic_height = intrinsic_h_;
}

void Widget::apply_layout_result(const LayoutNode& node) {
    rect_ = node.computed_rect;
    content_rect_ = node.content_rect;
    layout_dirty_ = false;
    paint_dirty_ = true;
}

} // namespace ugui
