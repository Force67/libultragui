#include <ultragui/animation/animator.h>
#include <ultragui/layout/layout.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/widget.h>

#include <algorithm>

namespace ugui {

/// Compute packed per-corner radii from a resolved style.
/// Uses individual corner values if any are non-zero, otherwise falls back to uniform corner_radius.
static u32 style_corner_radii(const Style& s) {
    if (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
        s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f) {
        return Vertex2D::pack_radii(s.corner_radius_tl, s.corner_radius_tr,
                                    s.corner_radius_br, s.corner_radius_bl);
    }
    return Vertex2D::pack_radii(s.corner_radius);
}

Widget::~Widget() {
    for (auto* child : children_) {
        child->parent_ = nullptr;
    }
}

void Widget::set_context(const WidgetContext* ctx) {
    context_ = ctx;
    for (auto* child : children_) {
        child->set_context(ctx);
    }
}

void Widget::add_child(Widget* child) {
    if (child->parent_)
        child->parent_->remove_child(child);
    child->parent_ = this;
    children_.push_back(child);
    if (context_)
        child->set_context(context_);
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

void Widget::add_state_transition(WidgetState state, const Transition& transition) {
    state_transitions_.push_back({state, transition});
}

void Widget::set_widget_state(WidgetState state) {
    if (state_ != state) {
        WidgetState old_state = state_;
        state_ = state;

        // Trigger transitions if configured
        if (context_ && context_->animator && context_->current_time && !state_transitions_.empty()) {
            Style from = resolve_style(style_, state_overrides_.data(),
                                       static_cast<u32>(state_overrides_.size()), old_state);
            Style to = resolve_style(style_, state_overrides_.data(),
                                     static_cast<u32>(state_overrides_.size()), state);

            // Find the best matching transition config
            for (auto& stc : state_transitions_) {
                u16 activated = static_cast<u16>(state) & ~static_cast<u16>(old_state);
                u16 deactivated = static_cast<u16>(old_state) & ~static_cast<u16>(state);
                bool relevant = (static_cast<u16>(stc.state) & (activated | deactivated)) != 0;
                if (relevant && !stc.transition.is_instant()) {
                    context_->animator->start_transition(
                        id_, from, to, stc.transition, *context_->current_time);
                    break;
                }
            }
        }

        mark_paint_dirty();
    }
}

void Widget::set_animation_style(const Style& s) {
    animation_style_ = s;
    has_animation_ = true;
    mark_paint_dirty();
}

void Widget::clear_animation_style() {
    has_animation_ = false;
    mark_paint_dirty();
}

Style Widget::computed_style() const {
    if (has_animation_)
        return animation_style_;
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
    u32 radii = style_corner_radii(s);

    // Box shadow (drawn before background)
    if (s.has_shadow()) {
        Color sc = s.shadow.color.with_alpha(s.shadow.color.a * alpha);
        renderer.draw_shadow(rect_, sc, s.shadow.blur, s.shadow.spread,
                             s.shadow.offset, radii);
    }

    // Background (with optional gradient and border)
    if (s.background.a > 0.0f || s.border_width > 0.0f) {
        Color bg = s.background.with_alpha(s.background.a * alpha);

        if (s.border_width > 0.0f && s.border_color.a > 0.0f) {
            Color bc = s.border_color.with_alpha(s.border_color.a * alpha);
            if (s.has_gradient()) {
                // Border with gradient: draw border first, then gradient fill inset
                renderer.draw_bordered_rect(rect_, Color::transparent(), bc, s.border_width,
                                            radii);
                Rect inner = rect_.shrunk(s.border_width);
                // Shrink each corner radius by border_width
                f32 tl = (radii & 0xFFu);
                f32 tr = ((radii >> 8) & 0xFFu);
                f32 br = ((radii >> 16) & 0xFFu);
                f32 bl = ((radii >> 24) & 0xFFu);
                u32 inner_radii = Vertex2D::pack_radii(
                    tl > s.border_width ? tl - s.border_width : 0.0f,
                    tr > s.border_width ? tr - s.border_width : 0.0f,
                    br > s.border_width ? br - s.border_width : 0.0f,
                    bl > s.border_width ? bl - s.border_width : 0.0f);
                Color bg2 = s.background_end.with_alpha(s.background_end.a * alpha);
                renderer.draw_rect_gradient(inner, bg, bg2, inner_radii);
            } else {
                renderer.draw_bordered_rect(rect_, bg, bc, s.border_width, radii);
            }
        } else if (s.has_gradient()) {
            Color bg2 = s.background_end.with_alpha(s.background_end.a * alpha);
            renderer.draw_rect_gradient(rect_, bg, bg2, radii);
        } else {
            renderer.draw_rect(rect_, bg, radii);
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
