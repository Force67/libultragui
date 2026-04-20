#include <ugui/render/renderer2d.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace ugui {
namespace {

String apply_transform(const String& s, TextTransform t) {
  if (t == TextTransform::kNone) return s;
  String out = s;
  if (t == TextTransform::kUppercase) {
    for (auto& c : out)
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  } else if (t == TextTransform::kLowercase) {
    for (auto& c : out)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  } else if (t == TextTransform::kCapitalize) {
    bool next = true;
    for (auto& c : out) {
      if (next && std::isalpha(static_cast<unsigned char>(c))) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        next = false;
      }
      if (c == ' ') next = true;
    }
  }
  return out;
}

TextEngine* text_engine(const Widget& w) {
  return w.context() ? w.context()->text_engine : nullptr;
}

FontHandle effective_font(const Widget& w, const ButtonContent& c) {
  if (c.font != kInvalidFont) return c.font;
  return w.context() ? w.context()->default_font : kInvalidFont;
}

void ButtonMeasure(WidgetRegistry& world, Widget& w, f32& out_width,
                   f32& out_height) {
  ButtonContent* c = world.Get<ButtonContent>(w.handle());
  const Style& st = w.style();
  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
  if (!c || !te || fh == kInvalidFont || c->label.empty()) {
    out_width = 0;
    out_height = st.font_size + st.padding.vertical();
    return;
  }

  FontHandle resolved = fh;
  if (st.font_weight != FontWeight::kRegular ||
      st.font_style != FontStyle::kNormal)
    resolved = te->ResolveFont(fh, st.font_weight, st.font_style);

  String display_label = apply_transform(c->label, st.text_transform);
  f32 sc = w.ui_scale();
  auto run =
      te->Shape(resolved, display_label.c_str(),
                static_cast<u32>(display_label.size()), st.font_size * sc,
                st.letter_spacing * sc, st.line_height_multiplier);
  out_width = run.total_advance + st.padding.horizontal();
  out_height = run.line_height + st.padding.vertical();
}

void ButtonDraw(WidgetRegistry& world, Widget& w, Renderer2D& renderer) {
  // The base Widget::OnPaint already drew background / shadow / border.
  ButtonContent* c = world.Get<ButtonContent>(w.handle());
  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
  if (!c || !te || fh == kInvalidFont || c->label.empty()) return;

  Style s = w.ComputedStyle();
  s.Scale(w.ui_scale());
  f32 alpha = s.opacity;

  FontHandle resolved = fh;
  if (s.font_weight != FontWeight::kRegular ||
      s.font_style != FontStyle::kNormal)
    resolved = te->ResolveFont(fh, s.font_weight, s.font_style);

  String display_label = apply_transform(c->label, s.text_transform);
  auto run = te->Shape(resolved, display_label.c_str(),
                       static_cast<u32>(display_label.size()), s.font_size,
                       s.letter_spacing, s.line_height_multiplier);

  // Center the label in the content rect.
  Rect content = w.content_rect();
  f32 x = content.x + (content.w - run.total_advance) * 0.5f;
  f32 y = content.y + (content.h - run.line_height) * 0.5f;

  Color text_color = s.text_color.WithAlpha(s.text_color.a * alpha);

  if (s.text_shadow_color.a > 0.0f) {
    Vec2 shadow_pos = {x + s.text_shadow_offset.x, y + s.text_shadow_offset.y};
    Color shadow_color =
        s.text_shadow_color.WithAlpha(s.text_shadow_color.a * alpha);
    renderer.DrawText(shadow_pos, run, shadow_color, te->atlas_texture());
  }

  renderer.DrawText(Vec2{x, y}, run, text_color, te->atlas_texture());

  if (s.text_decoration != TextDecoration::kNone) {
    Color dec_color = s.text_decoration_color.a > 0.0f
                          ? s.text_decoration_color.WithAlpha(
                                s.text_decoration_color.a * alpha)
                          : text_color;
    f32 thickness = std::max(1.0f, s.font_size / 14.0f);
    f32 baseline = y + run.ascent;

    if (HasDecoration(s.text_decoration, TextDecoration::kUnderline)) {
      f32 line_y = baseline + s.font_size * 0.15f;
      renderer.DrawRect({x, line_y, run.total_advance, thickness}, dec_color);
    }
    if (HasDecoration(s.text_decoration, TextDecoration::kStrikethrough)) {
      f32 line_y = baseline - s.font_size * 0.3f;
      renderer.DrawRect({x, line_y, run.total_advance, thickness}, dec_color);
    }
  }
}

bool ButtonClick(WidgetRegistry& world, Widget& w) {
  ButtonContent* c = world.Get<ButtonContent>(w.handle());
  if (c && c->on_click) {
    c->on_click();
    return true;
  }
  return false;
}

}  // namespace

WidgetVTable ButtonVTable() {
  WidgetVTable vt;
  vt.draw = ButtonDraw;
  vt.measure = ButtonMeasure;
  vt.on_click = ButtonClick;
  return vt;
}

Widget* CreateButton(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kButton);
  WidgetRegistry::Active()->Add<ButtonContent>(w->handle(), ButtonContent{});
  return w;
}

void SetButtonLabel(Widget* w, const String& label) {
  if (!w || w->kind() != WidgetKind::kButton || !w->registry()) return;
  w->registry()->GetOrAdd<ButtonContent>(w->handle()).label = label;
  w->MarkDirty();
}

void SetButtonClick(Widget* w, Function<void()> handler) {
  if (!w || w->kind() != WidgetKind::kButton || !w->registry()) return;
  w->registry()->GetOrAdd<ButtonContent>(w->handle()).on_click =
      std::move(handler);
}

}  // namespace ugui
