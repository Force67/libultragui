#include <algorithm>
#include <ranges>
#include <ugui/animation/animator.h>
#include <ugui/layout/layout.h>
#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

/// Compute packed per-corner radii from a resolved style.
/// Uses individual corner values if any are non-zero, otherwise falls back to
/// uniform corner_radius.
static u32 style_corner_radii(const Style& s) {
  if (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
      s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f) {
    return Vertex2D::PackRadii(s.corner_radius_tl, s.corner_radius_tr,
                               s.corner_radius_br, s.corner_radius_bl);
  }
  return Vertex2D::PackRadii(s.corner_radius);
}

Widget::~Widget() {
  // Invalidate any outstanding handles to this widget before it goes away.
  if (registry_ && self_.valid()) registry_->Release(self_);
  for (auto* child : children_) {
    delete child;
  }
}

WidgetId Widget::handle() {
  if (self_.valid()) return self_;
  if (context_ && context_->registry) return context_->registry->Acquire(this);
  return kNullWidget;
}

void Widget::SetContext(const WidgetContext* ctx) {
  context_ = ctx;
  for (auto* child : children_) {
    child->SetContext(ctx);
  }
}

void Widget::AddChild(Widget* child) {
  if (child->parent_) child->parent_->RemoveChild(child);
  child->parent_ = this;
  children_.push_back(child);
  if (context_) child->SetContext(context_);
  MarkDirty();
}

void Widget::RemoveChild(Widget* child) {
  auto it = std::find(children_.begin(), children_.end(), child);
  if (it != children_.end()) {
    (*it)->parent_ = nullptr;
    children_.erase(it);
    MarkDirty();
  }
}

void Widget::ClearChildren() {
  for (auto* child : children_) {
    child->parent_ = nullptr;
    delete child;
  }
  children_.clear();
  MarkDirty();
}

Widget* Widget::ChildAt(u32 index) const {
  return index < children_.size() ? children_[index] : nullptr;
}

void Widget::AddStateOverride(WidgetState state, const Style& override_style,
                              u64 mask) {
  state_overrides_.push_back({state, override_style, mask});
}

void Widget::AddStateTransition(WidgetState state,
                                const Transition& transition) {
  state_transitions_.push_back({state, transition});
}

void Widget::set_widget_state(WidgetState state) {
  if (state_ != state) {
    WidgetState old_state = state_;
    state_ = state;

    // Trigger transitions if configured
    if (context_ && context_->animator && context_->current_time &&
        !state_transitions_.empty()) {
      Style from =
          ResolveStyle(style_, state_overrides_.data(),
                       static_cast<u32>(state_overrides_.size()), old_state);
      Style to = ResolveStyle(style_, state_overrides_.data(),
                              static_cast<u32>(state_overrides_.size()), state);

      // Find the best matching transition config
      for (auto& stc : state_transitions_) {
        u16 activated = static_cast<u16>(state) & ~static_cast<u16>(old_state);
        u16 deactivated =
            static_cast<u16>(old_state) & ~static_cast<u16>(state);
        bool relevant =
            (static_cast<u16>(stc.state) & (activated | deactivated)) != 0;
        if (relevant && !stc.transition.IsInstant()) {
          context_->animator->StartTransition(id_, from, to, stc.transition,
                                              *context_->current_time);
          break;
        }
      }
    }

    MarkPaintDirty();
  }
}

void Widget::SetAnimationStyle(const Style& s) {
  animation_style_ = s;
  MarkPaintDirty();
}

void Widget::ClearAnimationStyle() {
  animation_style_.reset();
  MarkPaintDirty();
}

Style Widget::ComputedStyle() const {
  if (animation_style_) return *animation_style_;
  if (state_overrides_.empty()) return style_;
  return ResolveStyle(style_, state_overrides_.data(),
                      static_cast<u32>(state_overrides_.size()), state_);
}

void Widget::MarkDirty() {
  layout_dirty_ = true;
  paint_dirty_ = true;
  // Propagate up to root
  if (parent_) parent_->MarkDirty();
}

Widget* Widget::HitTest(Vec2 point) {
  if (!rect_.contains(point)) return nullptr;

  // Check children in reverse (top-most first)
  for (auto* child : children_ | std::views::reverse) {
    if (auto* hit = child->HitTest(point)) return hit;
  }
  return this;
}

Vec2 Widget::InputToLayoutPoint(Vec2 point) const {
  const Widget* p = parent_;
  while (p) {
    if (auto* sv = widget_cast<ScrollView>(p)) point += sv->scroll_offset();
    p = p->parent();
  }
  return point;
}

void Widget::OnLayout(const Rect& rect, const Rect& content_rect) {
  rect_ = rect;
  content_rect_ = content_rect;
  layout_dirty_ = false;
}

// Drag-to-move default implementation
//
// When draggable_ is set, the first OnDragStart pins the widget to its
// current screen-space rect via style.left_offset / style.top in pixels
// and clears any right/bottom anchoring. Subsequent OnDragMove calls
// offset that anchor by (cursor - press), so the widget tracks the
// cursor exactly even after a layout pass repositions it.

void Widget::OnDragStart(Vec2 pos) {
  if (!draggable_) return;
  drag_origin_x_ = rect_.x;
  drag_origin_y_ = rect_.y;
  drag_press_ = pos;
  // Convert anchoring to top/left in screen pixels regardless of how
  // the widget was originally positioned (left/top, right/bottom, or
  // a mix). The layout engine will resolve these on the next pass.
  style_.left_offset = Length::Px(rect_.x);
  style_.top = Length::Px(rect_.y);
  style_.right_offset = Length::Auto();
  style_.bottom = Length::Auto();
  // Make sure the widget actually participates in absolute positioning.
  if (style_.position != Position::kAbsolute)
    style_.position = Position::kAbsolute;
  MarkDirty();
}

void Widget::OnDragMove(Vec2 pos, Vec2 /*delta*/) {
  if (!draggable_) return;
  f32 nx = drag_origin_x_ + (pos.x - drag_press_.x);
  f32 ny = drag_origin_y_ + (pos.y - drag_press_.y);
  style_.left_offset = Length::Px(nx);
  style_.top = Length::Px(ny);
  MarkDirty();
  if (on_drag_) on_drag_(Vec2{nx, ny});
}

void Widget::OnDragEnd(Vec2 /*pos*/) {
  // No-op by default; the in-flight position is already committed via
  // OnDragMove. Subclasses can override for snap-back / persistence.
}

void Widget::OnPaint(Renderer2D& renderer) {
  auto s = ComputedStyle();
  s.Scale(ui_scale());
  f32 alpha = s.opacity;
  u32 radii = style_corner_radii(s);

  // Outer box shadow (drawn before background)
  if (s.HasShadow() && !s.shadow.inset) {
    Color sc = s.shadow.color.WithAlpha(s.shadow.color.a * alpha);
    renderer.DrawShadow(rect_, sc, s.shadow.blur, s.shadow.spread,
                        s.shadow.offset, radii);
  }

  // Backdrop blur (frosted glass approximation: true Kawase blur requires a
  // separate render pass)
  if (s.backdrop_blur > 0.0f) {
    f32 blur_alpha = Clamp(s.backdrop_blur / 40.0f, 0.1f, 0.6f);
    Color frost =
        s.background.a > 0.0f ? s.background : Color{0.1f, 0.1f, 0.15f, 1.0f};
    frost.a = Clamp(frost.a + blur_alpha, 0.0f, 0.95f) * alpha;
    renderer.DrawRect(rect_, frost, radii);
  }

  // Background (with optional gradient and border)
  if (s.background.a > 0.0f || s.border_width > 0.0f ||
      s.HasMultiStopGradient()) {
    Color bg = s.background.WithAlpha(s.background.a * alpha);

    if (s.border_width > 0.0f && s.border_color.a > 0.0f) {
      Color bc = s.border_color.WithAlpha(s.border_color.a * alpha);
      if (s.HasMultiStopGradient()) {
        // Border with multi-stop gradient: draw border first, then gradient
        // fill inset
        renderer.DrawBorderedRect(rect_, Color::Transparent(), bc,
                                  s.border_width, radii);
        Rect inner = rect_.Shrunk(s.border_width);
        f32 tl = (radii & 0xFFu);
        f32 tr = ((radii >> 8) & 0xFFu);
        f32 br = ((radii >> 16) & 0xFFu);
        f32 bl = ((radii >> 24) & 0xFFu);
        u32 inner_radii = Vertex2D::PackRadii(
            tl > s.border_width ? tl - s.border_width : 0.0f,
            tr > s.border_width ? tr - s.border_width : 0.0f,
            br > s.border_width ? br - s.border_width : 0.0f,
            bl > s.border_width ? bl - s.border_width : 0.0f);
        renderer.DrawMultiStopGradient(inner, s.gradient_stops,
                                       s.gradient_stop_count, s.gradient_type,
                                       s.gradient_angle, inner_radii);
      } else if (s.HasGradient()) {
        // Border with gradient: draw border first, then gradient fill inset
        renderer.DrawBorderedRect(rect_, Color::Transparent(), bc,
                                  s.border_width, radii);
        Rect inner = rect_.Shrunk(s.border_width);
        // Shrink each corner radius by border_width
        f32 tl = (radii & 0xFFu);
        f32 tr = ((radii >> 8) & 0xFFu);
        f32 br = ((radii >> 16) & 0xFFu);
        f32 bl = ((radii >> 24) & 0xFFu);
        u32 inner_radii = Vertex2D::PackRadii(
            tl > s.border_width ? tl - s.border_width : 0.0f,
            tr > s.border_width ? tr - s.border_width : 0.0f,
            br > s.border_width ? br - s.border_width : 0.0f,
            bl > s.border_width ? bl - s.border_width : 0.0f);
        Color bg2 = s.background_end.WithAlpha(s.background_end.a * alpha);
        if (s.gradient_type == GradientType::kRadial)
          renderer.DrawRadialGradient(inner, bg, bg2, inner_radii);
        else
          renderer.DrawRectGradient(inner, bg, bg2, inner_radii,
                                    s.gradient_angle);
      } else {
        renderer.DrawBorderedRect(rect_, bg, bc, s.border_width, radii);
      }
    } else if (s.HasMultiStopGradient()) {
      renderer.DrawMultiStopGradient(rect_, s.gradient_stops,
                                     s.gradient_stop_count, s.gradient_type,
                                     s.gradient_angle, radii);
    } else if (s.HasGradient()) {
      Color bg2 = s.background_end.WithAlpha(s.background_end.a * alpha);
      if (s.gradient_type == GradientType::kRadial)
        renderer.DrawRadialGradient(rect_, bg, bg2, radii);
      else
        renderer.DrawRectGradient(rect_, bg, bg2, radii, s.gradient_angle);
    } else {
      renderer.DrawRect(rect_, bg, radii);
    }
  }

  // Inset shadow (drawn after background, on top of fill)
  if (s.HasShadow() && s.shadow.inset) {
    Color sc = s.shadow.color.WithAlpha(s.shadow.color.a * alpha);
    renderer.PushScissor(rect_);
    renderer.DrawInsetShadow(rect_, sc, s.shadow.blur, s.shadow.spread,
                             s.shadow.offset, radii);
    renderer.PopScissor();
  }

  // Focus ring (only for tab-focusable widgets)
  if (HasState(state_, WidgetState::kFocused) && tab_index_ >= 0) {
    f32 sc = ui_scale();
    Rect focus_rect = {rect_.x - 2.0f * sc, rect_.y - 2.0f * sc,
                       rect_.w + 4.0f * sc, rect_.h + 4.0f * sc};
    // Use border color if set, otherwise text color, fallback to subtle white
    Color base = s.border_color.a > 0.1f ? s.border_color
                 : s.text_color.a > 0.1f ? s.text_color
                                         : Color{0.6f, 0.6f, 0.7f, 1.0f};
    Color focus_color = base.WithAlpha(0.5f * alpha);
    renderer.DrawBorderedRect(focus_rect, Color::Transparent(), focus_color,
                              2.0f * sc, radii);
  }
}

void Widget::Measure(f32& out_width, f32& out_height) {
  out_width = intrinsic_w_;
  out_height = intrinsic_h_;
}

void Widget::PopulateLayoutNode(LayoutNode& node) const {
  node.style = style_;
  node.id = id_;
  node.intrinsic_width = intrinsic_w_;
  node.intrinsic_height = intrinsic_h_;
}

void Widget::ApplyLayoutResult(const LayoutNode& node) {
  rect_ = node.computed_rect;
  content_rect_ = node.content_rect;
  layout_dirty_ = false;
  paint_dirty_ = true;
}

}  // namespace ugui
