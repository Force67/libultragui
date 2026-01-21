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
    if (s.background.a > 0.0f) {
        renderer.draw_rect(rect_, s.background.with_alpha(s.background.a * s.opacity),
                           s.corner_radius);
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
