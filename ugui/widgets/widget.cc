#include <algorithm>
#include <ugui/animation/animator.h>
#include <ugui/layout/layout.h>
#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/components.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>
#include <ugui/widgets/widget_vtable.h>

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

Widget::Widget(u32 id) : id_(id == 0 ? NextAutoId() : id) {
  // Register eagerly so every widget has a stable handle from birth; tree links
  // and external references store the handle instead of a raw pointer.
  WidgetRegistry::Active()->Acquire(this);  // sets self_ and registry_
}

Widget::~Widget() {
  for (wid c : children_) {
    if (Widget* child = registry_ ? registry_->Get(c) : nullptr) delete child;
  }
  // Invalidate outstanding handles to this widget before it goes away.
  if (registry_ && self_.valid()) registry_->Release(self_);
}

WidgetId Widget::handle() { return self_; }

void Widget::set_tooltip(const String& text) {
  if (registry_) registry_->Add<Tooltip>(self_, Tooltip{text});
}

const String& Widget::tooltip() const {
  static const String kEmpty;
  Tooltip* t = registry_ ? registry_->Get<Tooltip>(self_) : nullptr;
  return t ? t->text : kEmpty;
}

Widget* Widget::parent_ptr() const {
  return registry_ ? registry_->Get(parent_) : nullptr;
}

Vector<Widget*> Widget::child_ptrs() const {
  Vector<Widget*> out;
  out.reserve(children_.size());
  for (wid c : children_) {
    if (Widget* w = registry_ ? registry_->Get(c) : nullptr) out.push_back(w);
  }
  return out;
}

void Widget::SetContext(const WidgetContext* ctx) {
  context_ = ctx;
  for (Widget* child : child_ptrs()) child->SetContext(ctx);
}

void Widget::AddChild(Widget* child) {
  if (!child) return;
  if (Widget* old = child->parent_ptr()) old->RemoveChild(child);
  child->parent_ = self_;
  children_.push_back(child->self_);
  if (context_) child->SetContext(context_);
  MarkDirty();
}

void Widget::RemoveChild(Widget* child) {
  if (!child) return;
  auto it = std::find(children_.begin(), children_.end(), child->self_);
  if (it != children_.end()) {
    child->parent_ = kNullWidget;
    children_.erase(it);
    MarkDirty();
  }
}

void Widget::ClearChildren() {
  for (wid c : children_) {
    if (Widget* child = registry_ ? registry_->Get(c) : nullptr) {
      child->parent_ = kNullWidget;
      delete child;
    }
  }
  children_.clear();
  MarkDirty();
}

wid Widget::ChildAt(u32 index) const {
  return index < children_.size() ? children_[index] : kNullWidget;
}

void Widget::AddStateOverride(WidgetState state, const Style& override_style,
                              u64 mask) {
  registry_->GetOrAdd<StateStyle>(self_).overrides.push_back(
      {state, override_style, mask});
}

void Widget::AddStateTransition(WidgetState state,
                                const Transition& transition) {
  registry_->GetOrAdd<StateStyle>(self_).transitions.push_back(
      {state, transition});
}

void Widget::set_widget_state(WidgetState state) {
  if (state_ != state) {
    WidgetState old_state = state_;
    state_ = state;

    // Trigger transitions if configured
    StateStyle* ss = registry_ ? registry_->Get<StateStyle>(self_) : nullptr;
    if (context_ && context_->animator && context_->current_time && ss &&
        !ss->transitions.empty()) {
      u32 n = static_cast<u32>(ss->overrides.size());
      Style from = ResolveStyle(style_, ss->overrides.data(), n, old_state);
      Style to = ResolveStyle(style_, ss->overrides.data(), n, state);

      // Find the best matching transition config
      for (auto& stc : ss->transitions) {
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
  if (registry_) registry_->Add<AnimStyle>(self_, AnimStyle{s});
  MarkPaintDirty();
}

void Widget::ClearAnimationStyle() {
  if (registry_) registry_->Remove<AnimStyle>(self_);
  MarkPaintDirty();
}

Style Widget::ComputedStyle() const {
  if (AnimStyle* a = registry_ ? registry_->Get<AnimStyle>(self_) : nullptr)
    return a->style;
  StateStyle* ss = registry_ ? registry_->Get<StateStyle>(self_) : nullptr;
  if (!ss || ss->overrides.empty()) return style_;
  return ResolveStyle(style_, ss->overrides.data(),
                      static_cast<u32>(ss->overrides.size()), state_);
}

void Widget::MarkDirty() {
  layout_dirty_ = true;
  paint_dirty_ = true;
  // Propagate up to root
  if (Widget* p = parent_ptr()) p->MarkDirty();
}

wid Widget::HitTest(Vec2 point) {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.hit_test && registry_) return vt.hit_test(*registry_, *this, point);

  if (!rect_.contains(point)) return kNullWidget;

  // Check children in reverse (top-most first)
  for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
    if (Widget* child = registry_ ? registry_->Get(*it) : nullptr) {
      wid hit = child->HitTest(point);
      if (hit.valid()) return hit;
    }
  }
  return self_;
}

Vec2 Widget::InputToLayoutPoint(Vec2 point) const {
  Widget* p = parent_ptr();
  while (p) {
    point += ScrollOffset(p);  // (0,0) for non-scroll-view ancestors
    p = p->parent_ptr();
  }
  return point;
}

void Widget::OnLayout(const Rect& rect, const Rect& content_rect) {
  rect_ = rect;
  content_rect_ = content_rect;
  layout_dirty_ = false;
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.on_layout && registry_)
    vt.on_layout(*registry_, *this, rect, content_rect);
}

void Widget::OnUpdate(f64 dt) {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.on_update && registry_) vt.on_update(*registry_, *this, dt);
}

bool Widget::OnScroll(Vec2 delta) {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.on_scroll && registry_) return vt.on_scroll(*registry_, *this, delta);
  return false;
}

bool Widget::OnKeyDown(i32 key, i32 mods) {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.on_key_down && registry_)
    return vt.on_key_down(*registry_, *this, key, mods);
  return false;
}

bool Widget::OnCharInput(u32 codepoint) {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.on_char_input && registry_)
    return vt.on_char_input(*registry_, *this, codepoint);
  return false;
}

bool Widget::consumes_text_input() const {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  return vt.consumes_text_input ? vt.consumes_text_input(*this) : false;
}

void Widget::OnDismiss() {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.on_dismiss && registry_) vt.on_dismiss(*registry_, *this);
}

void Widget::OnPaint(Renderer2D& renderer) {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.custom_paint) {
    // The widget owns its entire paint; skip the base box.
    if (vt.draw && registry_) vt.draw(*registry_, *this, renderer);
    return;
  }

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

  // Kind-specific overlay (text glyphs, image texture, ...): the base draws the
  // box above, the vtable draws the content on top.
  if (vt.draw && registry_) vt.draw(*registry_, *this, renderer);
}

void Widget::Measure(f32& out_width, f32& out_height) {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.measure && registry_) {
    vt.measure(*registry_, *this, out_width, out_height);
    return;
  }
  out_width = intrinsic_w_;
  out_height = intrinsic_h_;
}

bool Widget::OnClick() {
  const WidgetVTable& vt = WidgetVTableFor(kind_);
  if (vt.on_click && registry_) return vt.on_click(*registry_, *this);
  return false;
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
