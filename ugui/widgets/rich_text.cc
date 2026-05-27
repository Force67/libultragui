#include <ugui/render/renderer2d.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/rich_text.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>

namespace ugui {
namespace {

TextEngine* text_engine(WidgetRegistry& world, wid e) {
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->text_engine : nullptr;
}

FontHandle effective_font(WidgetRegistry& world, wid e,
                          const RichTextContent& c) {
  if (c.font != kInvalidFont) return c.font;
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->default_font : kInvalidFont;
}

/// A shaped and positioned span ready for rendering.
struct ShapedSpan {
  TextRun run;
  Color color;
  TextDecoration decoration;
  f32 font_size;  // for decoration thickness
  f32 x;          // x position within the line
  f32 y;          // y position (accumulated line offset)
};

/// Shape all spans and lay them out inline with wrapping. Returns total height.
f32 LayoutSpans(WidgetRegistry& world, wid e, const RichTextContent& c,
                Vector<ShapedSpan>& out, f32 max_width) {
  TextEngine* te = text_engine(world, e);
  FontHandle base_fh = effective_font(world, e, c);
  if (!te || base_fh == kInvalidFont) return 0.0f;

  const Style& s = world.Get<StyleC>(e)->style;
  f32 sc = UiScale(world, e);
  f32 line_x = 0.0f;
  f32 line_y = 0.0f;
  f32 line_height = 0.0f;

  for (const auto& span : c.spans) {
    f32 size = (span.font_size > 0.0f ? span.font_size : s.font_size) * sc;

    FontHandle fh = base_fh;
    if (span.font_weight != FontWeight::kRegular ||
        span.font_style != FontStyle::kNormal) {
      fh = te->ResolveFont(base_fh, span.font_weight, span.font_style);
    }

    auto run =
        te->Shape(fh, span.text.c_str(), static_cast<u32>(span.text.size()),
                  size, s.letter_spacing * sc, s.line_height_multiplier);

    if (line_x > 0.0f && max_width > 0.0f &&
        line_x + run.total_advance > max_width) {
      line_y += line_height;
      line_x = 0.0f;
      line_height = 0.0f;
    }

    out.push_back(
        ShapedSpan{run, span.color, span.decoration, size, line_x, line_y});

    line_x += run.total_advance;
    line_height = std::max(line_height, run.line_height);
  }

  return line_y + line_height;
}

void RichTextMeasure(WidgetRegistry& world, wid e, f32& out_width,
                     f32& out_height) {
  RichTextContent* c = world.Get<RichTextContent>(e);
  TextEngine* te = text_engine(world, e);
  if (!c || !te || effective_font(world, e, *c) == kInvalidFont ||
      c->spans.empty()) {
    out_width = 0;
    out_height = 0;
    return;
  }

  Vector<ShapedSpan> shaped;
  f32 total_h = LayoutSpans(world, e, *c, shaped, 1e6f);  // measure no wrapping

  f32 max_w = 0;
  for (const auto& ss : shaped)
    max_w = std::max(max_w, ss.x + ss.run.total_advance);
  out_width = max_w;
  out_height = total_h;
}

void RichTextDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  // PaintWidget already drew background / shadow / border.
  RichTextContent* c = world.Get<RichTextContent>(e);
  TextEngine* te = text_engine(world, e);
  if (!c || !te || c->spans.empty()) return;

  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;

  Rect content = world.Get<Transform>(e)->content_rect;

  Vector<ShapedSpan> shaped;
  LayoutSpans(world, e, *c, shaped, content.w);

  for (const auto& ss : shaped) {
    f32 x = content.x + ss.x;
    f32 y = content.y + ss.y;
    Color col = ss.color.WithAlpha(ss.color.a * alpha);

    renderer.DrawText(Vec2{x, y}, ss.run, col, te->atlas_texture());

    if (ss.decoration != TextDecoration::kNone) {
      f32 thickness = std::max(1.0f, ss.font_size / 14.0f);
      f32 baseline = y + ss.run.ascent;

      if (HasDecoration(ss.decoration, TextDecoration::kUnderline)) {
        f32 line_y = baseline + ss.font_size * 0.15f;
        renderer.DrawRect({x, line_y, ss.run.total_advance, thickness}, col);
      }
      if (HasDecoration(ss.decoration, TextDecoration::kStrikethrough)) {
        f32 line_y = baseline - ss.font_size * 0.3f;
        renderer.DrawRect({x, line_y, ss.run.total_advance, thickness}, col);
      }
    }
  }
}

}  // namespace

WidgetVTable RichTextVTable() {
  WidgetVTable vt;
  vt.draw = RichTextDraw;
  vt.measure = RichTextMeasure;
  return vt;
}

wid CreateRichText(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kRichText;
  world.Add<RichTextContent>(e, RichTextContent{});
  return e;
}

void SetRichTextSpans(wid e, const Vector<TextSpan>& spans) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (!world.Alive(e) || world.Get<WidgetNode>(e)->kind != WidgetKind::kRichText)
    return;
  world.GetOrAdd<RichTextContent>(e).spans = spans;
  MarkDirty(world, e);
}

void AddRichTextSpan(wid e, const TextSpan& span) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (!world.Alive(e) || world.Get<WidgetNode>(e)->kind != WidgetKind::kRichText)
    return;
  world.GetOrAdd<RichTextContent>(e).spans.push_back(span);
  MarkDirty(world, e);
}

void ClearRichTextSpans(wid e) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (!world.Alive(e) || world.Get<WidgetNode>(e)->kind != WidgetKind::kRichText)
    return;
  world.GetOrAdd<RichTextContent>(e).spans.clear();
  MarkDirty(world, e);
}

void SetRichTextFont(wid e, FontHandle font) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (!world.Alive(e) || world.Get<WidgetNode>(e)->kind != WidgetKind::kRichText)
    return;
  world.GetOrAdd<RichTextContent>(e).font = font;
}

}  // namespace ugui
