#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/radio.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <string>

namespace ugui {
namespace {

TextEngine* text_engine(WidgetRegistry& world, wid e) {
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->text_engine : nullptr;
}

FontHandle effective_font(WidgetRegistry& world, wid e, const RadioContent& c) {
  if (c.font != kInvalidFont) return c.font;
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->default_font : kInvalidFont;
}

void DeselectSiblings(WidgetRegistry& world, wid e, const RadioContent& c) {
  wid parent = world.Get<Hierarchy>(e)->parent;
  if (!parent.valid()) return;
  for (wid sib : world.Get<Hierarchy>(parent)->children) {
    if (sib == e) continue;
    if (world.Get<WidgetNode>(sib)->kind != WidgetKind::kRadio) continue;
    RadioContent* sc = world.Get<RadioContent>(sib);
    if (sc && sc->group == c.group) SetRadioSelected(sib, false);
  }
}

void RadioMeasure(WidgetRegistry& world, wid e, f32& out_width,
                  f32& out_height) {
  RadioContent* c = world.Get<RadioContent>(e);
  const Style& st = world.Get<StyleC>(e)->style;
  f32 sc = UiScale(world, e);
  f32 circle_size = st.font_size * 1.2f;
  constexpr f32 kGap = 8.0f;

  TextEngine* te = text_engine(world, e);
  FontHandle fh = c ? effective_font(world, e, *c) : kInvalidFont;
  if (c && te && fh != kInvalidFont && !c->label.empty()) {
    auto run = te->Shape(fh, c->label.c_str(), static_cast<u32>(c->label.size()),
                         st.font_size * sc, st.letter_spacing * sc,
                         st.line_height_multiplier);
    out_width = circle_size + kGap + run.total_advance;
    out_height = std::max(circle_size, run.line_height);
  } else {
    out_width = circle_size;
    out_height = circle_size;
  }
}

void RadioDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  // PaintWidget already drew background / shadow / border.
  RadioContent* c = world.Get<RadioContent>(e);
  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;
  f32 circle_size = s.font_size * 1.0f;
  constexpr f32 kGap = 8.0f;
  f32 radius = circle_size * 0.5f;
  u32 radii = Vertex2D::PackRadii(radius);

  Rect content = world.Get<Transform>(e)->content_rect;
  f32 circle_x = content.x;
  f32 circle_y = content.y + (content.h - circle_size) * 0.5f;

  bool is_selected = IsRadioSelected(e);

  Color border_color = (s.border_color.a > 0.0f)
                           ? s.border_color.WithAlpha(s.border_color.a * alpha)
                           : Color(0.6f, 0.6f, 0.6f, alpha);
  f32 bw = std::max(s.border_width, 1.5f);

  if (is_selected) {
    Color accent = Color::FromHex(0x4a7aff, alpha);
    renderer.DrawBorderedRect({circle_x, circle_y, circle_size, circle_size},
                              Color::Transparent(), accent.WithAlpha(alpha), bw,
                              radii);

    f32 inner_size = circle_size * 0.6f;
    f32 inner_x = circle_x + (circle_size - inner_size) * 0.5f;
    f32 inner_y = circle_y + (circle_size - inner_size) * 0.5f;
    f32 inner_radius = inner_size * 0.5f;
    u32 inner_radii = Vertex2D::PackRadii(inner_radius);
    renderer.DrawRect({inner_x, inner_y, inner_size, inner_size}, accent,
                      inner_radii);
  } else {
    renderer.DrawBorderedRect({circle_x, circle_y, circle_size, circle_size},
                              Color::Transparent(), border_color, bw, radii);
  }

  // Label to the right of the circle.
  TextEngine* te = text_engine(world, e);
  FontHandle fh = c ? effective_font(world, e, *c) : kInvalidFont;
  if (c && te && fh != kInvalidFont && !c->label.empty()) {
    auto run =
        te->Shape(fh, c->label.c_str(), static_cast<u32>(c->label.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);

    f32 text_x = circle_x + circle_size + kGap;
    f32 text_y = content.y + (content.h - run.line_height) * 0.5f;

    Color text_color = s.text_color.WithAlpha(s.text_color.a * alpha);

    if (s.text_shadow_color.a > 0.0f) {
      Vec2 shadow_pos = {text_x + s.text_shadow_offset.x,
                         text_y + s.text_shadow_offset.y};
      Color shadow_color =
          s.text_shadow_color.WithAlpha(s.text_shadow_color.a * alpha);
      renderer.DrawText(shadow_pos, run, shadow_color, te->atlas_texture());
    }

    renderer.DrawText(Vec2{text_x, text_y}, run, text_color,
                      te->atlas_texture());
  }
}

bool RadioClick(WidgetRegistry& world, wid e) {
  RadioContent* c = world.Get<RadioContent>(e);
  if (c) DeselectSiblings(world, e, *c);
  SetRadioSelected(e, true);
  if (c && c->on_change) c->on_change(true);
  return true;
}

}  // namespace

WidgetVTable RadioVTable() {
  WidgetVTable vt;
  vt.draw = RadioDraw;
  vt.measure = RadioMeasure;
  vt.on_click = RadioClick;
  return vt;
}

wid CreateRadio(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kRadio;
  world.Add<RadioContent>(e, RadioContent{});
  return e;
}

void SetRadioLabel(wid e, const String& label) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  world.GetOrAdd<RadioContent>(e).label = label;
  MarkDirty(world, e);
}

void SetRadioGroup(wid e, const String& group) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  world.GetOrAdd<RadioContent>(e).group = group;
}

void SetRadioChange(wid e, Function<void(bool)> handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  world.GetOrAdd<RadioContent>(e).on_change = std::move(handler);
}

void SetRadioSelected(wid e, bool selected) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetState s = WidgetStateOf(world, e);
  if (selected)
    s = s | WidgetState::kChecked;
  else
    s = static_cast<WidgetState>(static_cast<u16>(s) &
                                 ~static_cast<u16>(WidgetState::kChecked));
  SetWidgetState(world, e, s);
}

bool IsRadioSelected(wid e) {
  return HasState(WidgetStateOf(*WidgetRegistry::Active(), e),
                  WidgetState::kChecked);
}

}  // namespace ugui
