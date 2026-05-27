#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/checkbox.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <string>

namespace ugui {
namespace {

TextEngine* text_engine(WidgetRegistry& world, wid e) {
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->text_engine : nullptr;
}

FontHandle effective_font(WidgetRegistry& world, wid e,
                          const CheckboxContent& c) {
  if (c.font != kInvalidFont) return c.font;
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->default_font : kInvalidFont;
}

void CheckboxMeasure(WidgetRegistry& world, wid e, f32& out_width,
                     f32& out_height) {
  CheckboxContent* c = world.Get<CheckboxContent>(e);
  const Style& st = world.Get<StyleC>(e)->style;
  f32 sc = UiScale(world, e);
  f32 box_size = st.font_size * 1.2f;
  constexpr f32 kGap = 8.0f;

  TextEngine* te = text_engine(world, e);
  FontHandle fh = c ? effective_font(world, e, *c) : kInvalidFont;
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

void CheckboxDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  // PaintWidget already drew background / shadow / border.
  CheckboxContent* c = world.Get<CheckboxContent>(e);
  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;
  f32 box_size = s.font_size * 1.0f;
  constexpr f32 kGap = 8.0f;
  f32 corner = std::min(box_size * 0.2f, 4.0f);
  u32 radii = Vertex2D::PackRadii(corner);

  Rect content = world.Get<Transform>(e)->content_rect;
  f32 box_x = content.x;
  f32 box_y = content.y + (content.h - box_size) * 0.5f;

  bool is_checked = IsChecked(e);

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
  TextEngine* te = text_engine(world, e);
  FontHandle fh = c ? effective_font(world, e, *c) : kInvalidFont;
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

bool CheckboxClick(WidgetRegistry& world, wid e) {
  bool now = !IsChecked(e);
  SetChecked(e, now);
  CheckboxContent* c = world.Get<CheckboxContent>(e);
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

wid CreateCheckbox(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kCheckbox;
  world.Add<CheckboxContent>(e, CheckboxContent{});
  return e;
}

void SetCheckboxLabel(wid e, const String& label) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kCheckbox) return;
  world.GetOrAdd<CheckboxContent>(e).label = label;
  MarkDirty(world, e);
}

void SetCheckboxChange(wid e, Function<void(bool)> handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kCheckbox) return;
  world.GetOrAdd<CheckboxContent>(e).on_change = std::move(handler);
}

void SetChecked(wid e, bool checked) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetState s = WidgetStateOf(world, e);
  if (checked)
    s = s | WidgetState::kChecked;
  else
    s = static_cast<WidgetState>(static_cast<u16>(s) &
                                 ~static_cast<u16>(WidgetState::kChecked));
  SetWidgetState(world, e, s);
}

bool IsChecked(wid e) {
  return HasState(WidgetStateOf(*WidgetRegistry::Active(), e),
                  WidgetState::kChecked);
}

}  // namespace ugui
