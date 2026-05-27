#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/toggle.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <cmath>

namespace ugui {
namespace {

void ToggleMeasure(WidgetRegistry& world, wid e, f32& out_width,
                   f32& out_height) {
  const Style& st = world.Get<StyleC>(e)->style;
  f32 track_w = 44.0f;
  f32 track_h = 24.0f;
  out_width = track_w + st.padding.horizontal();
  out_height = track_h + st.padding.vertical();
}

void ToggleDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  ToggleContent* c = world.Get<ToggleContent>(e);
  f32 anim = c ? c->thumb_anim : (IsToggleOn(e) ? 1.0f : 0.0f);

  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;

  Rect content = world.Get<Transform>(e)->content_rect;
  f32 track_x = content.x;
  f32 track_y = content.y;
  f32 track_w = content.w;
  f32 track_h = content.h;

  // Pill-shaped track (corner radius = half height).
  f32 track_radius = track_h * 0.5f;
  u32 track_radii = Vertex2D::PackRadii(track_radius);

  Color off_color = Color::FromHex(0x3a3a3a);
  Color on_color = Color::FromHex(0x4a7aff);
  if (s.background.a > 0.0f) {
    off_color = s.background;
    if (s.background_end.a > 0.0f) on_color = s.background_end;
  }

  Color track_color = Lerp(off_color, on_color, anim);
  track_color = track_color.WithAlpha(track_color.a * alpha);
  renderer.DrawRect({track_x, track_y, track_w, track_h}, track_color,
                    track_radii);

  // Thumb: circle inset from the track edges.
  f32 inset = 2.0f;
  f32 thumb_size = track_h - inset * 2.0f;
  f32 thumb_radius = thumb_size * 0.5f;
  u32 thumb_radii = Vertex2D::PackRadii(thumb_radius);

  f32 thumb_x = track_x + inset + anim * (track_w - thumb_size - inset * 2.0f);
  f32 thumb_y = track_y + inset;
  Color thumb_color = Color::White().WithAlpha(alpha);
  renderer.DrawRect({thumb_x, thumb_y, thumb_size, thumb_size}, thumb_color,
                    thumb_radii);
}

bool ToggleClick(WidgetRegistry& world, wid e) {
  bool now = !IsToggleOn(e);
  SetToggleOn(e, now);
  ToggleContent* c = world.Get<ToggleContent>(e);
  if (c && c->on_change) c->on_change(now);
  return true;
}

void ToggleUpdate(WidgetRegistry& world, wid e, f64 dt) {
  ToggleContent* c = world.Get<ToggleContent>(e);
  if (!c) return;

  f32 target = IsToggleOn(e) ? 1.0f : 0.0f;
  f32 prev = c->thumb_anim;
  f32 speed = Clamp(static_cast<f32>(dt) * 12.0f, 0.0f, 1.0f);
  c->thumb_anim += (target - c->thumb_anim) * speed;

  if (std::abs(c->thumb_anim - target) < 0.001f) c->thumb_anim = target;

  if (c->thumb_anim != prev) MarkPaintDirty(world, e);
}

}  // namespace

WidgetVTable ToggleVTable() {
  WidgetVTable vt;
  vt.draw = ToggleDraw;
  vt.measure = ToggleMeasure;
  vt.on_click = ToggleClick;
  vt.on_update = ToggleUpdate;
  vt.custom_paint = true;  // draws its own track instead of the base box
  return vt;
}

wid CreateToggle(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kToggle;
  world.Add<ToggleContent>(e, ToggleContent{});
  return e;
}

void SetToggleOn(wid e, bool on) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* node = world.Get<WidgetNode>(e);
  if (!node || node->kind != WidgetKind::kToggle) return;
  WidgetState s = WidgetStateOf(world, e);
  if (on)
    s = s | WidgetState::kChecked;
  else
    s = static_cast<WidgetState>(static_cast<u16>(s) &
                                 ~static_cast<u16>(WidgetState::kChecked));
  SetWidgetState(world, e, s);
  // Snap the thumb so external state restoration shows the correct visual.
  world.GetOrAdd<ToggleContent>(e).thumb_anim = on ? 1.0f : 0.0f;
  MarkPaintDirty(world, e);
}

bool IsToggleOn(wid e) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  return HasState(WidgetStateOf(world, e), WidgetState::kChecked);
}

void SetToggleChange(wid e, Function<void(bool)> handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* node = world.Get<WidgetNode>(e);
  if (!node || node->kind != WidgetKind::kToggle) return;
  world.GetOrAdd<ToggleContent>(e).on_change = std::move(handler);
}

}  // namespace ugui
