#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/radio.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <string>

namespace ugui {
namespace {

TextEngine* text_engine(const Widget& w) {
  return w.context() ? w.context()->text_engine : nullptr;
}

FontHandle effective_font(const Widget& w, const RadioContent& c) {
  if (c.font != kInvalidFont) return c.font;
  return w.context() ? w.context()->default_font : kInvalidFont;
}

void DeselectSiblings(WidgetRegistry& world, Widget& w, const RadioContent& c) {
  Widget* parent = w.parent_ptr();
  if (!parent) return;
  for (Widget* sib : parent->child_ptrs()) {
    if (sib == &w) continue;
    if (sib->kind() != WidgetKind::kRadio) continue;
    RadioContent* sc = world.Get<RadioContent>(sib->handle());
    if (sc && sc->group == c.group) SetRadioSelected(sib, false);
  }
}

void RadioMeasure(WidgetRegistry& world, Widget& w, f32& out_width,
                  f32& out_height) {
  RadioContent* c = world.Get<RadioContent>(w.handle());
  const Style& st = w.style();
  f32 sc = w.ui_scale();
  f32 circle_size = st.font_size * 1.2f;
  constexpr f32 kGap = 8.0f;

  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
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

void RadioDraw(WidgetRegistry& world, Widget& w, Renderer2D& renderer) {
  // The base Widget::OnPaint already drew background / shadow / border.
  RadioContent* c = world.Get<RadioContent>(w.handle());
  Style s = w.ComputedStyle();
  s.Scale(w.ui_scale());
  f32 alpha = s.opacity;
  f32 circle_size = s.font_size * 1.0f;
  constexpr f32 kGap = 8.0f;
  f32 radius = circle_size * 0.5f;
  u32 radii = Vertex2D::PackRadii(radius);

  Rect content = w.content_rect();
  f32 circle_x = content.x;
  f32 circle_y = content.y + (content.h - circle_size) * 0.5f;

  bool is_selected = IsRadioSelected(&w);

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
  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
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

bool RadioClick(WidgetRegistry& world, Widget& w) {
  RadioContent* c = world.Get<RadioContent>(w.handle());
  if (c) DeselectSiblings(world, w, *c);
  SetRadioSelected(&w, true);
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

Widget* CreateRadio(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kRadio);
  WidgetRegistry::Active()->Add<RadioContent>(w->handle(), RadioContent{});
  return w;
}

void SetRadioLabel(Widget* w, const String& label) {
  if (!w || w->kind() != WidgetKind::kRadio || !w->registry()) return;
  w->registry()->GetOrAdd<RadioContent>(w->handle()).label = label;
  w->MarkDirty();
}

void SetRadioGroup(Widget* w, const String& group) {
  if (!w || w->kind() != WidgetKind::kRadio || !w->registry()) return;
  w->registry()->GetOrAdd<RadioContent>(w->handle()).group = group;
}

void SetRadioChange(Widget* w, Function<void(bool)> handler) {
  if (!w || w->kind() != WidgetKind::kRadio || !w->registry()) return;
  w->registry()->GetOrAdd<RadioContent>(w->handle()).on_change =
      std::move(handler);
}

void SetRadioSelected(Widget* w, bool selected) {
  if (!w) return;
  WidgetState s = w->widget_state();
  if (selected)
    s = s | WidgetState::kChecked;
  else
    s = static_cast<WidgetState>(static_cast<u16>(s) &
                                 ~static_cast<u16>(WidgetState::kChecked));
  w->set_widget_state(s);
}

bool IsRadioSelected(const Widget* w) {
  return w && HasState(w->widget_state(), WidgetState::kChecked);
}

}  // namespace ugui
