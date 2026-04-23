#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/checkbox.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <string>

namespace ugui {
namespace {

TextEngine* text_engine(const Widget& w) {
  return w.context() ? w.context()->text_engine : nullptr;
}

FontHandle effective_font(const Widget& w, const CheckboxContent& c) {
  if (c.font != kInvalidFont) return c.font;
  return w.context() ? w.context()->default_font : kInvalidFont;
}

void CheckboxMeasure(WidgetRegistry& world, Widget& w, f32& out_width,
                     f32& out_height) {
  CheckboxContent* c = world.Get<CheckboxContent>(w.handle());
  const Style& st = w.style();
  f32 sc = w.ui_scale();
  f32 box_size = st.font_size * 1.2f;
  constexpr f32 kGap = 8.0f;

  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
  if (c && te && fh != kInvalidFont && !c->label.empty()) {
    auto run = te->Shape(fh, c->label.c_str(), static_cast<u32>(c->label.size()),
                         st.font_size * sc, st.letter_spacing * sc,
                         st.line_height_multiplier);
    out_width = box_size + kGap + run.total_advance;
    out_height = std::max(box_size, run.line_height);
  } else {
    out_width = box_size;
    out_height = box_size;
  }
}

void CheckboxDraw(WidgetRegistry& world, Widget& w, Renderer2D& renderer) {
  // The base Widget::OnPaint already drew background / shadow / border.
  CheckboxContent* c = world.Get<CheckboxContent>(w.handle());
  Style s = w.ComputedStyle();
  s.Scale(w.ui_scale());
  f32 alpha = s.opacity;
  f32 box_size = s.font_size * 1.0f;
  constexpr f32 kGap = 8.0f;
  f32 corner = std::min(box_size * 0.2f, 4.0f);
  u32 radii = Vertex2D::PackRadii(corner);

  Rect content = w.content_rect();
  f32 box_x = content.x;
  f32 box_y = content.y + (content.h - box_size) * 0.5f;

  bool is_checked = IsChecked(&w);

  if (is_checked) {
    Color accent = (s.background.a > 0.0f)
                       ? s.background.WithAlpha(s.background.a * alpha)
                       : Color::FromHex(0x4a4aff, alpha);
    Color border = (s.border_color.a > 0.0f)
                       ? s.border_color.WithAlpha(s.border_color.a * alpha)
                       : accent;
    f32 bw = std::max(s.border_width, 1.5f);
    renderer.DrawBorderedRect({box_x, box_y, box_size, box_size}, accent, border,
                              bw, radii);

    f32 inner_size = box_size * 0.45f;
    f32 inner_x = box_x + (box_size - inner_size) * 0.5f;
    f32 inner_y = box_y + (box_size - inner_size) * 0.5f;
    f32 inner_corner = std::min(inner_size * 0.2f, 2.0f);
    u32 inner_radii = Vertex2D::PackRadii(inner_corner);
    renderer.DrawRect({inner_x, inner_y, inner_size, inner_size},
                      Color(1.0f, 1.0f, 1.0f, alpha), inner_radii);
  } else {
    Color border = (s.border_color.a > 0.0f)
                       ? s.border_color.WithAlpha(s.border_color.a * alpha)
                       : Color(0.6f, 0.6f, 0.6f, alpha);
    f32 bw = std::max(s.border_width, 1.5f);
    renderer.DrawBorderedRect({box_x, box_y, box_size, box_size},
                              Color::Transparent(), border, bw, radii);
  }

  // Label to the right of the box.
  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
  if (c && te && fh != kInvalidFont && !c->label.empty()) {
    auto run =
        te->Shape(fh, c->label.c_str(), static_cast<u32>(c->label.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);

    f32 text_x = box_x + box_size + kGap;
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

bool CheckboxClick(WidgetRegistry& world, Widget& w) {
  bool now = !IsChecked(&w);
  SetChecked(&w, now);
  CheckboxContent* c = world.Get<CheckboxContent>(w.handle());
  if (c && c->on_change) c->on_change(now);
  return true;
}

}  // namespace

WidgetVTable CheckboxVTable() {
  WidgetVTable vt;
  vt.draw = CheckboxDraw;
  vt.measure = CheckboxMeasure;
  vt.on_click = CheckboxClick;
  return vt;
}

Widget* CreateCheckbox(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kCheckbox);
  WidgetRegistry::Active()->Add<CheckboxContent>(w->handle(), CheckboxContent{});
  return w;
}

void SetCheckboxLabel(Widget* w, const String& label) {
  if (!w || w->kind() != WidgetKind::kCheckbox || !w->registry()) return;
  w->registry()->GetOrAdd<CheckboxContent>(w->handle()).label = label;
  w->MarkDirty();
}

void SetCheckboxChange(Widget* w, Function<void(bool)> handler) {
  if (!w || w->kind() != WidgetKind::kCheckbox || !w->registry()) return;
  w->registry()->GetOrAdd<CheckboxContent>(w->handle()).on_change =
      std::move(handler);
}

void SetChecked(Widget* w, bool checked) {
  if (!w) return;
  WidgetState s = w->widget_state();
  if (checked)
    s = s | WidgetState::kChecked;
  else
    s = static_cast<WidgetState>(static_cast<u16>(s) &
                                 ~static_cast<u16>(WidgetState::kChecked));
  w->set_widget_state(s);
}

bool IsChecked(const Widget* w) {
  return w && HasState(w->widget_state(), WidgetState::kChecked);
}

}  // namespace ugui
