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

u32 NextWidgetId() {
  static u32 s_counter = 1;  // 0 is reserved / sentinel
  return s_counter++;
}

/// Compute packed per-corner radii from a resolved style.
static u32 style_corner_radii(const Style& s) {
  if (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
      s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f) {
    return Vertex2D::PackRadii(s.corner_radius_tl, s.corner_radius_tr,
                               s.corner_radius_br, s.corner_radius_bl);
  }
  return Vertex2D::PackRadii(s.corner_radius);
}

void DestroyWidget(WidgetRegistry& world, wid e) {
  if (!world.Alive(e)) return;
  if (Hierarchy* h = world.Get<Hierarchy>(e)) {
    Vector<wid> kids = h->children;  // copy: Release mutates the stores
    for (wid c : kids) DestroyWidget(world, c);
  }
  world.Release(e);
}

// --- Tree -------------------------------------------------------------------

void RemoveChild(WidgetRegistry& world, wid parent, wid child) {
  Hierarchy* ph = world.Get<Hierarchy>(parent);
  if (!ph) return;
  auto& kids = ph->children;
  auto it = std::find(kids.begin(), kids.end(), child);
  if (it != kids.end()) {
    if (Hierarchy* ch = world.Get<Hierarchy>(child)) ch->parent = kNullWidget;
    kids.erase(it);
    MarkDirty(world, parent);
  }
}

void AddChild(WidgetRegistry& world, wid parent, wid child) {
  if (!parent.valid() || !child.valid()) return;
  Hierarchy* ch = world.Get<Hierarchy>(child);
  if (!ch || !world.Get<Hierarchy>(parent)) return;
  if (ch->parent.valid()) RemoveChild(world, ch->parent, child);
  world.Get<Hierarchy>(child)->parent = parent;
  world.Get<Hierarchy>(parent)->children.push_back(child);
  if (const WidgetContext* ctx = world.Get<WidgetNode>(parent)->context)
    SetContext(world, child, ctx);
  MarkDirty(world, parent);
}

void SetContext(WidgetRegistry& world, wid e, const WidgetContext* ctx) {
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n) return;
  n->context = ctx;
  Vector<wid> kids = world.Get<Hierarchy>(e)->children;  // copy: recursion safe
  for (wid c : kids) SetContext(world, c, ctx);
}

const WidgetContext* WidgetContextOf(WidgetRegistry& world, wid e) {
  WidgetNode* n = world.Get<WidgetNode>(e);
  return n ? n->context : nullptr;
}

f32 UiScale(WidgetRegistry& world, wid e) {
  const WidgetContext* c = WidgetContextOf(world, e);
  return c ? c->ui_scale : 1.0f;
}

// --- Tooltip ----------------------------------------------------------------

void SetTooltip(WidgetRegistry& world, wid e, const String& text) {
  world.Add<Tooltip>(e, Tooltip{text});
}
const String& TooltipText(WidgetRegistry& world, wid e) {
  static const String kEmpty;
  Tooltip* t = world.Get<Tooltip>(e);
  return t ? t->text : kEmpty;
}

// --- Style / state ----------------------------------------------------------

void SetStyle(WidgetRegistry& world, wid e, const Style& s) {
  world.Get<StyleC>(e)->style = s;
  MarkDirty(world, e);
}

WidgetState WidgetStateOf(WidgetRegistry& world, wid e) {
  return world.Get<StyleC>(e)->state;
}

void AddStateOverride(WidgetRegistry& world, wid e, WidgetState state,
                      const Style& override_style, u64 mask) {
  world.GetOrAdd<StateStyle>(e).overrides.push_back(
      {state, override_style, mask});
}

void AddStateTransition(WidgetRegistry& world, wid e, WidgetState state,
                        const Transition& transition) {
  world.GetOrAdd<StateStyle>(e).transitions.push_back({state, transition});
}

void SetWidgetState(WidgetRegistry& world, wid e, WidgetState state) {
  StyleC* sc = world.Get<StyleC>(e);
  if (sc->state == state) return;
  WidgetState old_state = sc->state;
  sc->state = state;

  const WidgetContext* ctx = world.Get<WidgetNode>(e)->context;
  StateStyle* ss = world.Get<StateStyle>(e);
  if (ctx && ctx->animator && ctx->current_time && ss &&
      !ss->transitions.empty()) {
    sc = world.Get<StyleC>(e);
    u32 n = static_cast<u32>(ss->overrides.size());
    Style from = ResolveStyle(sc->style, ss->overrides.data(), n, old_state);
    Style to = ResolveStyle(sc->style, ss->overrides.data(), n, state);
    for (auto& stc : ss->transitions) {
      u16 activated = static_cast<u16>(state) & ~static_cast<u16>(old_state);
      u16 deactivated = static_cast<u16>(old_state) & ~static_cast<u16>(state);
      bool relevant =
          (static_cast<u16>(stc.state) & (activated | deactivated)) != 0;
      if (relevant && !stc.transition.IsInstant()) {
        ctx->animator->StartTransition(world.Get<WidgetNode>(e)->id, from, to,
                                       stc.transition, *ctx->current_time);
        break;
      }
    }
  }
  MarkPaintDirty(world, e);
}

void SetSelected(WidgetRegistry& world, wid e, bool v) {
  WidgetState s = WidgetStateOf(world, e);
  if (v)
    s = s | WidgetState::kSelected;
  else
    s = static_cast<WidgetState>(static_cast<u16>(s) &
                                 ~static_cast<u16>(WidgetState::kSelected));
  SetWidgetState(world, e, s);
}

void SetAnimationStyle(WidgetRegistry& world, wid e, const Style& s) {
  world.Add<AnimStyle>(e, AnimStyle{s});
  MarkPaintDirty(world, e);
}
void ClearAnimationStyle(WidgetRegistry& world, wid e) {
  world.Remove<AnimStyle>(e);
  MarkPaintDirty(world, e);
}

Style ComputedStyle(WidgetRegistry& world, wid e) {
  if (AnimStyle* a = world.Get<AnimStyle>(e)) return a->style;
  StyleC* sc = world.Get<StyleC>(e);
  StateStyle* ss = world.Get<StateStyle>(e);
  if (!ss || ss->overrides.empty()) return sc->style;
  return ResolveStyle(sc->style, ss->overrides.data(),
                      static_cast<u32>(ss->overrides.size()), sc->state);
}

// --- Dirty / hit-testing ----------------------------------------------------

void MarkDirty(WidgetRegistry& world, wid e) {
  Transform* t = world.Get<Transform>(e);
  if (!t) return;
  t->layout_dirty = true;
  t->paint_dirty = true;
  wid p = world.Get<Hierarchy>(e)->parent;
  if (p.valid()) MarkDirty(world, p);
}
void MarkPaintDirty(WidgetRegistry& world, wid e) {
  if (Transform* t = world.Get<Transform>(e)) t->paint_dirty = true;
}

Vec2 InputToLayoutPoint(WidgetRegistry& world, wid e, Vec2 point) {
  wid p = world.Get<Hierarchy>(e)->parent;
  while (p.valid()) {
    point += ScrollOffset(world, p);  // (0,0) for non-scroll-view ancestors
    p = world.Get<Hierarchy>(p)->parent;
  }
  return point;
}

wid HitTest(WidgetRegistry& world, wid e, Vec2 point) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  if (vt.hit_test) return vt.hit_test(world, e, point);

  if (!world.Get<Transform>(e)->rect.contains(point)) return kNullWidget;
  const Vector<wid>& kids = world.Get<Hierarchy>(e)->children;
  for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
    wid hit = HitTest(world, *it, point);
    if (hit.valid()) return hit;
  }
  return e;
}

// --- Lifecycle dispatch -----------------------------------------------------

void LayoutWidget(WidgetRegistry& world, wid e, const Rect& rect,
                  const Rect& content_rect) {
  Transform* t = world.Get<Transform>(e);
  t->rect = rect;
  t->content_rect = content_rect;
  t->layout_dirty = false;
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  if (vt.on_layout) vt.on_layout(world, e, rect, content_rect);
}

void UpdateWidget(WidgetRegistry& world, wid e, f64 dt) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  if (vt.on_update) vt.on_update(world, e, dt);
}
bool ScrollWidget(WidgetRegistry& world, wid e, Vec2 delta) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  return vt.on_scroll ? vt.on_scroll(world, e, delta) : false;
}
bool KeyDownWidget(WidgetRegistry& world, wid e, i32 key, i32 mods) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  return vt.on_key_down ? vt.on_key_down(world, e, key, mods) : false;
}
bool CharInputWidget(WidgetRegistry& world, wid e, u32 codepoint) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  return vt.on_char_input ? vt.on_char_input(world, e, codepoint) : false;
}
bool ConsumesTextInput(WidgetRegistry& world, wid e) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  return vt.consumes_text_input ? vt.consumes_text_input(world, e) : false;
}
void DismissWidget(WidgetRegistry& world, wid e) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  if (vt.on_dismiss) vt.on_dismiss(world, e);
}
bool ClickWidget(WidgetRegistry& world, wid e) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  return vt.on_click ? vt.on_click(world, e) : false;
}

void MeasureWidget(WidgetRegistry& world, wid e, f32& out_w, f32& out_h) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  if (vt.measure) {
    vt.measure(world, e, out_w, out_h);
    return;
  }
  Transform* t = world.Get<Transform>(e);
  out_w = t->intrinsic_w;
  out_h = t->intrinsic_h;
}

void PaintWidget(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  const WidgetVTable& vt = WidgetVTableFor(world.Get<WidgetNode>(e)->kind);
  if (vt.custom_paint) {
    if (vt.draw) vt.draw(world, e, renderer);
    return;
  }

  Rect rect = world.Get<Transform>(e)->rect;
  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;
  u32 radii = style_corner_radii(s);

  if (s.HasShadow() && !s.shadow.inset) {
    Color sc = s.shadow.color.WithAlpha(s.shadow.color.a * alpha);
    renderer.DrawShadow(rect, sc, s.shadow.blur, s.shadow.spread,
                        s.shadow.offset, radii);
  }

  if (s.backdrop_blur > 0.0f) {
    // Real backdrop blur: emit a quad flagged with the blur radius so a backend
    // with a captured, blurred copy of what is behind the UI fills it (frosted
    // glass), respecting the rounded-rect shape. The widget's own translucent
    // background then composites on top for the tint. Backends without a
    // backdrop simply skip it, so this degrades to no fill rather than a slab.
    renderer.set_next_blur(s.backdrop_blur);
    renderer.DrawRect(rect, Color{1.0f, 1.0f, 1.0f, alpha}, radii);
    renderer.set_next_blur(0.0f);
  }

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

  if (s.HasShadow() && s.shadow.inset) {
    Color sc = s.shadow.color.WithAlpha(s.shadow.color.a * alpha);
    renderer.PushScissor(rect);
    renderer.DrawInsetShadow(rect, sc, s.shadow.blur, s.shadow.spread,
                             s.shadow.offset, radii);
    renderer.PopScissor();
  }

  if (HasState(WidgetStateOf(world, e), WidgetState::kFocused) &&
      world.Get<WidgetNode>(e)->tab_index >= 0) {
    f32 sc = UiScale(world, e);
    Rect focus_rect = {rect.x - 2.0f * sc, rect.y - 2.0f * sc,
                       rect.w + 4.0f * sc, rect.h + 4.0f * sc};
    Color base = s.border_color.a > 0.1f ? s.border_color
                 : s.text_color.a > 0.1f ? s.text_color
                                         : Color{0.6f, 0.6f, 0.7f, 1.0f};
    Color focus_color = base.WithAlpha(0.5f * alpha);
    renderer.DrawBorderedRect(focus_rect, Color::Transparent(), focus_color,
                              2.0f * sc, radii);
  }

  if (vt.draw) vt.draw(world, e, renderer);
}

void PopulateLayoutNode(WidgetRegistry& world, wid e, LayoutNode& node) {
  Transform* t = world.Get<Transform>(e);
  node.style = world.Get<StyleC>(e)->style;
  node.id = world.Get<WidgetNode>(e)->id;
  node.intrinsic_width = t->intrinsic_w;
  node.intrinsic_height = t->intrinsic_h;
}

void ApplyLayoutResult(WidgetRegistry& world, wid e, const LayoutNode& node) {
  Transform* t = world.Get<Transform>(e);
  t->rect = node.computed_rect;
  t->content_rect = node.content_rect;
  t->layout_dirty = false;
  t->paint_dirty = true;
}

}  // namespace ugui
