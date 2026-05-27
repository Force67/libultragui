#include <ugui/render/renderer2d.h>
#include <ugui/widgets/text.h>
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

TextEngine* text_engine(WidgetRegistry& world, wid e) {
  const WidgetContext* c = WidgetContextOf(world, e);
  return c ? c->text_engine : nullptr;
}

FontHandle effective_font(WidgetRegistry& world, wid e, const TextContent& tc) {
  if (tc.font != kInvalidFont) return tc.font;
  const WidgetContext* c = WidgetContextOf(world, e);
  return c ? c->default_font : kInvalidFont;
}

void TextMeasure(WidgetRegistry& world, wid e, f32& out_width,
                 f32& out_height) {
  TextContent* c = world.Get<TextContent>(e);
  TextEngine* te = text_engine(world, e);
  FontHandle fh = c ? effective_font(world, e, *c) : kInvalidFont;
  if (!c || !te || fh == kInvalidFont || c->text.empty()) {
    out_width = 0;
    out_height = 0;
    return;
  }

  const Style& st = world.Get<StyleC>(e)->style;
  FontHandle resolved = fh;
  if (st.font_weight != FontWeight::kRegular ||
      st.font_style != FontStyle::kNormal)
    resolved = te->ResolveFont(fh, st.font_weight, st.font_style);

  String display_text = apply_transform(c->text, st.text_transform);
  f32 sc = UiScale(world, e);
  auto run =
      te->Shape(resolved, display_text.c_str(),
                static_cast<u32>(display_text.size()), st.font_size * sc,
                st.letter_spacing * sc, st.line_height_multiplier);
  out_width = run.total_advance;
  out_height = run.line_height;
}

void TextDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  // PaintWidget already drew background / shadow / border.
  TextContent* c = world.Get<TextContent>(e);
  TextEngine* te = text_engine(world, e);
  FontHandle fh = c ? effective_font(world, e, *c) : kInvalidFont;
  if (!c || !te || fh == kInvalidFont || c->text.empty()) return;

  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;

  FontHandle resolved = fh;
  if (s.font_weight != FontWeight::kRegular ||
      s.font_style != FontStyle::kNormal)
    resolved = te->ResolveFont(fh, s.font_weight, s.font_style);

  String display_text = apply_transform(c->text, s.text_transform);

  // Always shape fresh: the scratch buffer pointer from measure may be stale.
  auto run = te->Shape(resolved, display_text.c_str(),
                       static_cast<u32>(display_text.size()), s.font_size,
                       s.letter_spacing, s.line_height_multiplier);

  Rect content = world.Get<Transform>(e)->content_rect;
  f32 x = content.x;
  f32 y = content.y;

  switch (s.text_align) {
    case TextAlign::kCenter:
      x += (content.w - run.total_advance) * 0.5f;
      break;
    case TextAlign::kRight:
      x += content.w - run.total_advance;
      break;
    default:
      break;
  }

  y += (content.h - run.line_height) * 0.5f;

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

}  // namespace

WidgetVTable TextVTable() {
  WidgetVTable vt;
  vt.draw = TextDraw;
  vt.measure = TextMeasure;
  return vt;
}

wid CreateText(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kText;
  world.Add<TextContent>(e, TextContent{});
  return e;
}

void SetText(wid e, const String& text) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kText) return;
  world.GetOrAdd<TextContent>(e).text = text;
  MarkDirty(world, e);
}

}  // namespace ugui
