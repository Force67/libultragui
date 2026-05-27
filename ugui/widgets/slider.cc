#include <ugui/platform/platform.h>
#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/slider.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {
namespace {

// Map the current mouse position onto the slider track and, if the value
// changed, store it and fire on_change.
void UpdateValueFromMouse(WidgetRegistry& world, wid e, SliderContent& c) {
  const WidgetContext* ctx = WidgetContextOf(world, e);
  if (!ctx || !ctx->platform) return;
  f32 mouse_x =
      InputToLayoutPoint(world, e, ctx->platform->input_queue().mouse_pos).x;
  Rect content = world.Get<Transform>(e)->content_rect;
  f32 track_x = content.x;
  f32 track_w = content.w;
  if (track_w <= 0.0f) return;

  f32 t = Clamp((mouse_x - track_x) / track_w, 0.0f, 1.0f);
  f32 new_value = c.min + t * (c.max - c.min);
  if (new_value != c.value) {
    c.value = new_value;
    MarkPaintDirty(world, e);
    if (c.on_change) c.on_change(c.value);
  }
}

void SliderMeasure(WidgetRegistry& world, wid e, f32& out_width,
                   f32& out_height) {
  const Style& st = world.Get<StyleC>(e)->style;
  out_width = 200.0f + st.padding.horizontal();
  out_height = 24.0f + st.padding.vertical();
}

void SliderDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  SliderContent* c = world.Get<SliderContent>(e);
  if (!c) return;

  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;

  f32 range = c->max - c->min;
  f32 normalized =
      (range > 0.0f) ? Clamp((c->value - c->min) / range, 0.0f, 1.0f) : 0.0f;

  constexpr f32 kTrackHeight = 4.0f;
  constexpr f32 kTrackRadius = 2.0f;
  constexpr f32 kThumbSize = 16.0f;
  constexpr f32 kThumbRadius = 8.0f;

  Rect content = world.Get<Transform>(e)->content_rect;
  f32 track_x = content.x;
  f32 track_w = content.w;
  f32 track_y = content.y + (content.h - kTrackHeight) * 0.5f;
  u32 track_radii = Vertex2D::PackRadii(kTrackRadius);

  // border_color drives the track, background the fill, text_color the thumb.
  Color track_color = s.border_color.a > 0.0f
                          ? s.border_color.WithAlpha(s.border_color.a * alpha)
                          : Color(0.16f, 0.16f, 0.16f, alpha);
  Color fill_color = s.background.a > 0.0f
                         ? s.background.WithAlpha(s.background.a * alpha)
                         : Color(0.29f, 0.48f, 1.0f, alpha);
  Color thumb_color = s.text_color.WithAlpha(s.text_color.a * alpha);

  renderer.DrawRect({track_x, track_y, track_w, kTrackHeight}, track_color,
                    track_radii);

  f32 fill_w = normalized * track_w;
  if (fill_w > 0.0f)
    renderer.DrawRect({track_x, track_y, fill_w, kTrackHeight}, fill_color,
                      track_radii);

  f32 thumb_x = track_x + fill_w - kThumbSize * 0.5f;
  f32 thumb_y = content.y + (content.h - kThumbSize) * 0.5f;
  u32 thumb_radii = Vertex2D::PackRadii(kThumbRadius);
  renderer.DrawRect({thumb_x, thumb_y, kThumbSize, kThumbSize}, thumb_color,
                    thumb_radii);
}

bool SliderClick(WidgetRegistry& world, wid e) {
  SliderContent* c = world.Get<SliderContent>(e);
  if (!c) return true;
  c->dragging = true;
  UpdateValueFromMouse(world, e, *c);
  return true;
}

void SliderUpdate(WidgetRegistry& world, wid e, f64 dt) {
  SliderContent* c = world.Get<SliderContent>(e);
  if (!c) return;
  const WidgetContext* ctx = WidgetContextOf(world, e);
  if (HasState(WidgetStateOf(world, e), WidgetState::kPressed) && ctx &&
      ctx->platform) {
    UpdateValueFromMouse(world, e, *c);
  } else if (c->dragging) {
    c->dragging = false;
  }
}

}  // namespace

WidgetVTable SliderVTable() {
  WidgetVTable vt;
  vt.draw = SliderDraw;
  vt.measure = SliderMeasure;
  vt.on_click = SliderClick;
  vt.on_update = SliderUpdate;
  vt.custom_paint = true;  // draws its own track instead of the base box
  return vt;
}

wid CreateSlider(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kSlider;
  world.Add<SliderContent>(e, SliderContent{});
  return e;
}

void SetSliderValue(wid e, f32 value) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  SliderContent& c = world.GetOrAdd<SliderContent>(e);
  c.value = Clamp(value, c.min, c.max);
  MarkPaintDirty(world, e);
}

f32 SliderValue(wid e) {
  SliderContent* c = WidgetRegistry::Active()->Get<SliderContent>(e);
  return c ? c->value : 0.0f;
}

f32 SliderMin(wid e) {
  SliderContent* c = WidgetRegistry::Active()->Get<SliderContent>(e);
  return c ? c->min : 0.0f;
}

f32 SliderMax(wid e) {
  SliderContent* c = WidgetRegistry::Active()->Get<SliderContent>(e);
  return c ? c->max : 0.0f;
}

void SetSliderMin(wid e, f32 min) {
  WidgetRegistry::Active()->GetOrAdd<SliderContent>(e).min = min;
}

void SetSliderMax(wid e, f32 max) {
  WidgetRegistry::Active()->GetOrAdd<SliderContent>(e).max = max;
}

void SetSliderChange(wid e, Function<void(f32)> handler) {
  WidgetRegistry::Active()->GetOrAdd<SliderContent>(e).on_change =
      std::move(handler);
}

}  // namespace ugui
