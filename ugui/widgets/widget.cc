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
static u32 style_corner_radii(const Style& s) {
  if (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
      s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f) {
    return Vertex2D::PackRadii(s.corner_radius_tl, s.corner_radius_tr,
                               s.corner_radius_br, s.corner_radius_bl);
  }
  return Vertex2D::PackRadii(s.corner_radius);
}

Widget::Widget(u32 id) {
  // Register eagerly so the entity has a stable handle from birth, then attach
  // the core components that hold all of its data.
  registry_ = WidgetRegistry::Active();
  registry_->Acquire(this);  // sets self_ (and registry_)
  registry_->Add<WidgetNode>(self_, WidgetNode{id == 0 ? NextAutoId() : id});
  registry_->Add<Transform>(self_, Transform{});
  registry_->Add<StyleC>(self_, StyleC{});
  registry_->Add<Hierarchy>(self_, Hierarchy{});
}

Widget::~Widget() {
  for (wid c : hier()->children) {
    if (Widget* child = registry_->Get(c)) delete child;
  }
  // Invalidate outstanding handles (and drop all components) for this entity.
  if (self_.valid()) registry_->Release(self_);
}

Transform* Widget::xf() const { return registry_->Get<Transform>(self_); }
Hierarchy* Widget::hier() const { return registry_->Get<Hierarchy>(self_); }
StyleC* Widget::sc() const { return registry_->Get<StyleC>(self_); }
WidgetNode* Widget::node() const { return registry_->Get<WidgetNode>(self_); }

// --- Identity ---------------------------------------------------------------

u32 Widget::id() const { return node()->id; }
void Widget::set_id(u32 id) { node()->id = id; }
const String& Widget::name() const { return node()->name; }
void Widget::set_name(const String& name) { node()->name = name; }
WidgetKind Widget::kind() const { return node()->kind; }
void Widget::set_kind(WidgetKind k) { node()->kind = k; }

const WidgetContext* Widget::context() const { return node()->context; }
f32 Widget::ui_scale() const {
  const WidgetContext* c = context();
  return c ? c->ui_scale : 1.0f;
}
void Widget::set_tab_index(i32 idx) { node()->tab_index = idx; }
i32 Widget::tab_index() const { return node()->tab_index; }
bool Widget::focusable() const { return tab_index() >= 0; }

// --- Tooltip ----------------------------------------------------------------

void Widget::set_tooltip(const String& text) {
  registry_->Add<Tooltip>(self_, Tooltip{text});
}

const String& Widget::tooltip() const {
  static const String kEmpty;
  Tooltip* t = registry_->Get<Tooltip>(self_);
  return t ? t->text : kEmpty;
}

// --- Tree -------------------------------------------------------------------

Widget* Widget::parent_ptr() const { return registry_->Get(hier()->parent); }

Vector<Widget*> Widget::child_ptrs() const {
  Vector<Widget*> out;
  const Vector<wid>& kids = hier()->children;
  out.reserve(kids.size());
  for (wid c : kids) {
    if (Widget* w = registry_->Get(c)) out.push_back(w);
  }
  return out;
}

void Widget::SetContext(const WidgetContext* ctx) {
  node()->context = ctx;
  for (Widget* child : child_ptrs()) child->SetContext(ctx);
}

void Widget::AddChild(Widget* child) {
  if (!child) return;
  if (Widget* old = child->parent_ptr()) old->RemoveChild(child);
  child->hier()->parent = self_;
  hier()->children.push_back(child->self_);
  if (context()) child->SetContext(context());
  MarkDirty();
}

void Widget::RemoveChild(Widget* child) {
  if (!child) return;
  Vector<wid>& kids = hier()->children;
  auto it = std::find(kids.begin(), kids.end(), child->self_);
  if (it != kids.end()) {
    child->hier()->parent = kNullWidget;
    kids.erase(it);
    MarkDirty();
  }
}

void Widget::ClearChildren() {
  for (wid c : hier()->children) {
    if (Widget* child = registry_->Get(c)) {
      child->hier()->parent = kNullWidget;
      delete child;
    }
  }
  hier()->children.clear();
  MarkDirty();
}

wid Widget::ChildAt(u32 index) const {
  const Vector<wid>& kids = hier()->children;
  return index < kids.size() ? kids[index] : kNullWidget;
}

u32 Widget::child_count() const {
  return static_cast<u32>(hier()->children.size());
}

wid Widget::parent() const { return hier()->parent; }

const Vector<wid>& Widget::children() const { return hier()->children; }

// --- Style ------------------------------------------------------------------

Style& Widget::style() { return sc()->style; }
const Style& Widget::style() const { return sc()->style; }
void Widget::set_style(const Style& s) {
  sc()->style = s;
  MarkDirty();
}

WidgetState Widget::widget_state() const { return sc()->state; }

void Widget::set_selected(bool v) {
  WidgetState s = widget_state();
  if (v)
    s = s | WidgetState::kSelected;
  else
    s = static_cast<WidgetState>(static_cast<u16>(s) &
                                 ~static_cast<u16>(WidgetState::kSelected));
  set_widget_state(s);
}
bool Widget::selected() const {
  return HasState(widget_state(), WidgetState::kSelected);
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
  StyleC* style_c = sc();
  if (style_c->state == state) return;
  WidgetState old_state = style_c->state;
  style_c->state = state;

  // Trigger transitions if configured.
  const WidgetContext* ctx = context();
  StateStyle* ss = registry_->Get<StateStyle>(self_);
  if (ctx && ctx->animator && ctx->current_time && ss &&
      !ss->transitions.empty()) {
    const Style& base = style_c->style;
    u32 n = static_cast<u32>(ss->overrides.size());
    Style from = ResolveStyle(base, ss->overrides.data(), n, old_state);
    Style to = ResolveStyle(base, ss->overrides.data(), n, state);

    for (auto& stc : ss->transitions) {
      u16 activated = static_cast<u16>(state) & ~static_cast<u16>(old_state);
      u16 deactivated = static_cast<u16>(old_state) & ~static_cast<u16>(state);
      bool relevant =
          (static_cast<u16>(stc.state) & (activated | deactivated)) != 0;
      if (relevant && !stc.transition.IsInstant()) {
        ctx->animator->StartTransition(id(), from, to, stc.transition,
                                       *ctx->current_time);
        break;
      }
    }
  }

  MarkPaintDirty();
}

void Widget::SetAnimationStyle(const Style& s) {
  registry_->Add<AnimStyle>(self_, AnimStyle{s});
  MarkPaintDirty();
}

void Widget::ClearAnimationStyle() {
  registry_->Remove<AnimStyle>(self_);
  MarkPaintDirty();
}

Style Widget::ComputedStyle() const {
  if (AnimStyle* a = registry_->Get<AnimStyle>(self_)) return a->style;
  StyleC* style_c = sc();
  StateStyle* ss = registry_->Get<StateStyle>(self_);
  if (!ss || ss->overrides.empty()) return style_c->style;
  return ResolveStyle(style_c->style, ss->overrides.data(),
                      static_cast<u32>(ss->overrides.size()), style_c->state);
}

// --- Layout / dirty ---------------------------------------------------------

Rect Widget::rect() const { return xf()->rect; }
Rect Widget::content_rect() const { return xf()->content_rect; }
void Widget::set_intrinsic_size(f32 w, f32 h) {
  Transform* t = xf();
  t->intrinsic_w = w;
  t->intrinsic_h = h;
}

void Widget::MarkDirty() {
  Transform* t = xf();
  t->layout_dirty = true;
  t->paint_dirty = true;
  if (Widget* p = parent_ptr()) p->MarkDirty();
}
void Widget::MarkPaintDirty() { xf()->paint_dirty = true; }
bool Widget::IsDirty() const { return xf()->layout_dirty; }
bool Widget::IsPaintDirty() const { return xf()->paint_dirty; }

bool Widget::contains(Vec2 point) const { return rect().contains(point); }

wid Widget::HitTest(Vec2 point) {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.hit_test) return vt.hit_test(*registry_, *this, point);

  if (!rect().contains(point)) return kNullWidget;

  // Check children in reverse (top-most first).
  const Vector<wid>& kids = hier()->children;
  for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
    if (Widget* child = registry_->Get(*it)) {
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
  Transform* t = xf();
  t->rect = rect;
  t->content_rect = content_rect;
  t->layout_dirty = false;
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.on_layout) vt.on_layout(*registry_, *this, rect, content_rect);
}

void Widget::OnUpdate(f64 dt) {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.on_update) vt.on_update(*registry_, *this, dt);
}

bool Widget::OnScroll(Vec2 delta) {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.on_scroll) return vt.on_scroll(*registry_, *this, delta);
  return false;
}

bool Widget::OnKeyDown(i32 key, i32 mods) {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.on_key_down) return vt.on_key_down(*registry_, *this, key, mods);
  return false;
}

bool Widget::OnCharInput(u32 codepoint) {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.on_char_input) return vt.on_char_input(*registry_, *this, codepoint);
  return false;
}

bool Widget::consumes_text_input() const {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  return vt.consumes_text_input ? vt.consumes_text_input(*this) : false;
}

void Widget::OnDismiss() {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.on_dismiss) vt.on_dismiss(*registry_, *this);
}

void Widget::OnPaint(Renderer2D& renderer) {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.custom_paint) {
    // The widget owns its entire paint; skip the base box.
    if (vt.draw) vt.draw(*registry_, *this, renderer);
    return;
  }

  Rect rect = this->rect();
  auto s = ComputedStyle();
  s.Scale(ui_scale());
  f32 alpha = s.opacity;
  u32 radii = style_corner_radii(s);

  // Outer box shadow (drawn before background).
  if (s.HasShadow() && !s.shadow.inset) {
    Color sc = s.shadow.color.WithAlpha(s.shadow.color.a * alpha);
    renderer.DrawShadow(rect, sc, s.shadow.blur, s.shadow.spread,
                        s.shadow.offset, radii);
  }

  // Backdrop blur (frosted-glass approximation).
  if (s.backdrop_blur > 0.0f) {
    f32 blur_alpha = Clamp(s.backdrop_blur / 40.0f, 0.1f, 0.6f);
    Color frost =
        s.background.a > 0.0f ? s.background : Color{0.1f, 0.1f, 0.15f, 1.0f};
    frost.a = Clamp(frost.a + blur_alpha, 0.0f, 0.95f) * alpha;
    renderer.DrawRect(rect, frost, radii);
  }

  // Background (with optional gradient and border).
  if (s.background.a > 0.0f || s.border_width > 0.0f ||
      s.HasMultiStopGradient()) {
    Color bg = s.background.WithAlpha(s.background.a * alpha);

    if (s.border_width > 0.0f && s.border_color.a > 0.0f) {
      Color bc = s.border_color.WithAlpha(s.border_color.a * alpha);
      if (s.HasMultiStopGradient()) {
        renderer.DrawBorderedRect(rect, Color::Transparent(), bc,
                                  s.border_width, radii);
        Rect inner = rect.Shrunk(s.border_width);
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
        renderer.DrawBorderedRect(rect, Color::Transparent(), bc,
                                  s.border_width, radii);
        Rect inner = rect.Shrunk(s.border_width);
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
        renderer.DrawBorderedRect(rect, bg, bc, s.border_width, radii);
      }
    } else if (s.HasMultiStopGradient()) {
      renderer.DrawMultiStopGradient(rect, s.gradient_stops,
                                     s.gradient_stop_count, s.gradient_type,
                                     s.gradient_angle, radii);
    } else if (s.HasGradient()) {
      Color bg2 = s.background_end.WithAlpha(s.background_end.a * alpha);
      if (s.gradient_type == GradientType::kRadial)
        renderer.DrawRadialGradient(rect, bg, bg2, radii);
      else
        renderer.DrawRectGradient(rect, bg, bg2, radii, s.gradient_angle);
    } else {
      renderer.DrawRect(rect, bg, radii);
    }
  }

  // Inset shadow (drawn after background).
  if (s.HasShadow() && s.shadow.inset) {
    Color sc = s.shadow.color.WithAlpha(s.shadow.color.a * alpha);
    renderer.PushScissor(rect);
    renderer.DrawInsetShadow(rect, sc, s.shadow.blur, s.shadow.spread,
                             s.shadow.offset, radii);
    renderer.PopScissor();
  }

  // Focus ring (only for tab-focusable widgets).
  if (HasState(widget_state(), WidgetState::kFocused) && tab_index() >= 0) {
    f32 sc = ui_scale();
    Rect focus_rect = {rect.x - 2.0f * sc, rect.y - 2.0f * sc,
                       rect.w + 4.0f * sc, rect.h + 4.0f * sc};
    Color base = s.border_color.a > 0.1f ? s.border_color
                 : s.text_color.a > 0.1f ? s.text_color
                                         : Color{0.6f, 0.6f, 0.7f, 1.0f};
    Color focus_color = base.WithAlpha(0.5f * alpha);
    renderer.DrawBorderedRect(focus_rect, Color::Transparent(), focus_color,
                              2.0f * sc, radii);
  }

  // Kind-specific overlay (text glyphs, image texture, ...).
  if (vt.draw) vt.draw(*registry_, *this, renderer);
}

void Widget::Measure(f32& out_width, f32& out_height) {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.measure) {
    vt.measure(*registry_, *this, out_width, out_height);
    return;
  }
  Transform* t = xf();
  out_width = t->intrinsic_w;
  out_height = t->intrinsic_h;
}

bool Widget::OnClick() {
  const WidgetVTable& vt = WidgetVTableFor(kind());
  if (vt.on_click) return vt.on_click(*registry_, *this);
  return false;
}

void Widget::PopulateLayoutNode(LayoutNode& node) const {
  Transform* t = xf();
  node.style = sc()->style;
  node.id = this->node()->id;
  node.intrinsic_width = t->intrinsic_w;
  node.intrinsic_height = t->intrinsic_h;
}

void Widget::ApplyLayoutResult(const LayoutNode& node) {
  Transform* t = xf();
  t->rect = node.computed_rect;
  t->content_rect = node.content_rect;
  t->layout_dirty = false;
  t->paint_dirty = true;
}

}  // namespace ugui
