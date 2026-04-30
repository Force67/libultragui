#include <ugui/platform/platform.h>
#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/dropdown.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace ugui {
namespace {

TextEngine* text_engine(const Widget& w) {
  return w.context() ? w.context()->text_engine : nullptr;
}

FontHandle effective_font(const Widget& w, const DropdownContent& c) {
  if (c.font != kInvalidFont) return c.font;
  return w.context() ? w.context()->default_font : kInvalidFont;
}

WidgetId DropdownHitTest(WidgetRegistry& world, Widget& w, Vec2 point) {
  DropdownContent* c = world.Get<DropdownContent>(w.handle());

  // When open, expand the hit area to include the option list below.
  if (c && c->open && !c->options.empty()) {
    Style s = w.ComputedStyle();
    s.Scale(w.ui_scale());
    f32 row_height = s.font_size * 1.5f;
    f32 list_h = row_height * static_cast<f32>(c->options.size());
    Rect r = w.rect();
    Rect expanded = {r.x, r.y, r.w, r.h + list_h};
    if (expanded.contains(point)) return w.handle();
  }

  // Default hit test (replicates Widget::HitTest's non-vtable path; calling the
  // base directly would re-enter this vtable entry and recurse forever).
  if (!w.rect().contains(point)) return kNullWidget;
  const Vector<wid>& children = w.children();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if (Widget* child = world.Get(*it)) {
      wid hit = child->HitTest(point);
      if (hit.valid()) return hit;
    }
  }
  return w.handle();
}

bool DropdownClick(WidgetRegistry& world, Widget& w) {
  DropdownContent* c = world.Get<DropdownContent>(w.handle());
  if (!c) return false;
  // Toggle: open when closed, close when clicking the header while open.
  c->open = !c->open;
  w.MarkPaintDirty();
  return true;
}

void DropdownUpdate(WidgetRegistry& world, Widget& w, f64) {
  DropdownContent* c = world.Get<DropdownContent>(w.handle());
  if (!c || !c->open || c->options.empty() || !w.context() ||
      !w.context()->platform)
    return;

  auto& queue = w.context()->platform->input_queue();

  // Handle the first mouse press while open: pick a row, then close.
  for (u32 i = 0; i < queue.button_count; ++i) {
    auto& evt = queue.button_events[i];
    if (!evt.pressed) continue;

    Style s = w.ComputedStyle();
    s.Scale(w.ui_scale());
    f32 row_height = s.font_size * 1.5f;
    Rect r = w.rect();
    f32 list_top = r.y + r.h;
    f32 list_bottom =
        list_top + row_height * static_cast<f32>(c->options.size());

    Vec2 pos = w.InputToLayoutPoint(evt.position);
    if (pos.y >= list_top && pos.y < list_bottom && pos.x >= r.x &&
        pos.x <= r.x + r.w) {
      i32 idx = static_cast<i32>((pos.y - list_top) / row_height);
      if (idx >= 0 && idx < static_cast<i32>(c->options.size())) {
        c->selected = idx;
        if (c->on_change) c->on_change(c->selected, c->options[c->selected]);
      }
    }
    // Close on any click while open.
    c->open = false;
    w.MarkPaintDirty();
    return;
  }
}

void DropdownMeasure(WidgetRegistry& world, Widget& w, f32& out_width,
                     f32& out_height) {
  DropdownContent* c = world.Get<DropdownContent>(w.handle());
  const Style& st = w.style();
  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
  f32 sc = w.ui_scale();
  f32 font_size = st.font_size * sc;
  f32 letter_sp = st.letter_spacing * sc;
  constexpr f32 kChevronWidth = 24.0f;
  constexpr f32 kHPadding = 12.0f;

  if (!c || !te || fh == kInvalidFont || c->options.empty()) {
    out_width = kChevronWidth + kHPadding * 2.0f + 60.0f;
    out_height = font_size * 1.5f + st.padding.vertical();
    return;
  }

  f32 max_width = 0.0f;
  for (auto& opt : c->options) {
    auto run = te->Shape(fh, opt.c_str(), static_cast<u32>(opt.size()),
                         font_size, letter_sp, st.line_height_multiplier);
    max_width = std::max(max_width, run.total_advance);
  }

  const char* placeholder = "Select...";
  auto ph_run = te->Shape(fh, placeholder, 9, font_size, letter_sp,
                          st.line_height_multiplier);
  max_width = std::max(max_width, ph_run.total_advance);

  out_width =
      max_width + kChevronWidth + kHPadding * 2.0f + st.padding.horizontal();
  out_height = font_size * 1.5f + st.padding.vertical();
}

void DropdownDraw(WidgetRegistry& world, Widget& w, Renderer2D& renderer) {
  // The base Widget::OnPaint already drew background / shadow / border.
  DropdownContent* c = world.Get<DropdownContent>(w.handle());
  TextEngine* te = text_engine(w);
  FontHandle fh = c ? effective_font(w, *c) : kInvalidFont;
  if (!c || !te || fh == kInvalidFont) return;

  Style s = w.ComputedStyle();
  s.Scale(w.ui_scale());
  f32 alpha = s.opacity;
  f32 font_size = s.font_size;
  f32 row_height = font_size * 1.5f;
  constexpr f32 kChevronWidth = 24.0f;
  constexpr f32 kHPadding = 8.0f;

  Rect rect = w.rect();
  Rect content = w.content_rect();

  // Determine hover index from mouse position when open.
  if (c->open && w.context() && w.context()->platform) {
    Vec2 mouse =
        w.InputToLayoutPoint(w.context()->platform->input_queue().mouse_pos);
    f32 list_top = rect.y + rect.h;
    f32 list_bottom =
        list_top + row_height * static_cast<f32>(c->options.size());
    if (mouse.y >= list_top && mouse.y < list_bottom && mouse.x >= rect.x &&
        mouse.x <= rect.x + rect.w) {
      c->hover_index = static_cast<i32>((mouse.y - list_top) / row_height);
    } else {
      c->hover_index = -1;
    }
  } else {
    c->hover_index = -1;
  }

  // --- Current selection text (or placeholder) ---
  String display_text =
      (c->selected >= 0 && c->selected < static_cast<i32>(c->options.size()))
          ? c->options[c->selected]
          : "Select...";

  auto run =
      te->Shape(fh, display_text.c_str(), static_cast<u32>(display_text.size()),
                font_size, s.letter_spacing, s.line_height_multiplier);

  f32 text_x = content.x + kHPadding;
  f32 text_y = content.y + (content.h - run.line_height) * 0.5f;

  Color text_color =
      (c->selected >= 0) ? s.text_color.WithAlpha(s.text_color.a * alpha)
                         : s.text_color.WithAlpha(s.text_color.a * alpha * 0.5f);

  renderer.DrawText(Vec2{text_x, text_y}, run, text_color, te->atlas_texture());

  // --- Chevron indicator on the right side ---
  f32 chevron_x = content.x + content.w - kChevronWidth;
  f32 chevron_cy = content.y + content.h * 0.5f;
  f32 chevron_size = font_size * 0.3f;
  f32 line_thickness = std::max(1.5f, font_size / 10.0f);
  Color chevron_color = s.text_color.WithAlpha(s.text_color.a * alpha * 0.7f);

  if (c->open) {
    renderer.DrawRect({chevron_x + 2.0f, chevron_cy - chevron_size * 0.3f,
                       chevron_size * 0.8f, line_thickness},
                      chevron_color);
    renderer.DrawRect(
        {chevron_x + 2.0f + chevron_size * 0.8f,
         chevron_cy - chevron_size * 0.3f, chevron_size * 0.8f, line_thickness},
        chevron_color);
  } else {
    renderer.DrawRect({chevron_x + 2.0f, chevron_cy - line_thickness * 0.5f,
                       chevron_size * 0.8f, line_thickness},
                      chevron_color);
    renderer.DrawRect({chevron_x + 2.0f + chevron_size * 0.8f,
                       chevron_cy - line_thickness * 0.5f, chevron_size * 0.8f,
                       line_thickness},
                      chevron_color);
  }

  // --- Option list when open ---
  if (!c->open || c->options.empty()) return;

  f32 list_x = rect.x;
  f32 list_y = rect.y + rect.h;
  f32 list_w = rect.w;
  f32 list_h = row_height * static_cast<f32>(c->options.size());

  Color list_bg = (s.background.a > 0.0f)
                      ? s.background.WithAlpha(
                            Clamp(s.background.a + 0.1f, 0.0f, 1.0f) * alpha)
                      : Color(0.12f, 0.12f, 0.2f, 0.95f * alpha);
  u32 list_radii =
      Vertex2D::PackRadii(0.0f, 0.0f, s.corner_radius_br, s.corner_radius_bl);
  renderer.DrawRect({list_x, list_y, list_w, list_h}, list_bg, list_radii);

  if (s.border_color.a > 0.0f && s.border_width > 0.0f) {
    Color border = s.border_color.WithAlpha(s.border_color.a * alpha);
    renderer.DrawBorderedRect({list_x, list_y, list_w, list_h},
                              Color::Transparent(), border, s.border_width,
                              list_radii);
  }

  for (i32 i = 0; i < static_cast<i32>(c->options.size()); ++i) {
    f32 row_y = list_y + row_height * static_cast<f32>(i);

    bool is_hovered = (i == c->hover_index);
    bool is_selected = (i == c->selected);

    if (is_hovered || is_selected) {
      Color highlight = is_hovered ? Color(1.0f, 1.0f, 1.0f, 0.1f * alpha)
                                   : s.text_color.WithAlpha(0.15f * alpha);
      u32 row_radii = 0;
      if (i == static_cast<i32>(c->options.size()) - 1) {
        row_radii = Vertex2D::PackRadii(0.0f, 0.0f, s.corner_radius_br,
                                        s.corner_radius_bl);
      }
      renderer.DrawRect({list_x, row_y, list_w, row_height}, highlight,
                        row_radii);
    }

    auto opt_run = te->Shape(fh, c->options[i].c_str(),
                             static_cast<u32>(c->options[i].size()), font_size,
                             s.letter_spacing, s.line_height_multiplier);

    f32 opt_x = list_x + kHPadding + s.padding.left;
    f32 opt_y = row_y + (row_height - opt_run.line_height) * 0.5f;

    Color opt_color = s.text_color.WithAlpha(s.text_color.a * alpha);
    renderer.DrawText(Vec2{opt_x, opt_y}, opt_run, opt_color,
                      te->atlas_texture());
  }
}

}  // namespace

WidgetVTable DropdownVTable() {
  WidgetVTable vt;
  vt.draw = DropdownDraw;
  vt.measure = DropdownMeasure;
  vt.hit_test = DropdownHitTest;
  vt.on_click = DropdownClick;
  vt.on_update = DropdownUpdate;
  return vt;
}

Widget* CreateDropdown(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kDropdown);
  WidgetRegistry::Active()->Add<DropdownContent>(w->handle(), DropdownContent{});
  return w;
}

void SetDropdownOptions(Widget* w, const Vector<String>& options) {
  if (!w || w->kind() != WidgetKind::kDropdown || !w->registry()) return;
  w->registry()->GetOrAdd<DropdownContent>(w->handle()).options = options;
  w->MarkDirty();
}

void SetDropdownSelected(Widget* w, i32 index) {
  if (!w || w->kind() != WidgetKind::kDropdown || !w->registry()) return;
  DropdownContent& c = w->registry()->GetOrAdd<DropdownContent>(w->handle());
  if (c.options.empty()) {
    c.selected = -1;
  } else {
    c.selected = static_cast<i32>(
        Clamp(static_cast<f32>(index), 0.0f,
              static_cast<f32>(static_cast<i32>(c.options.size()) - 1)));
  }
  w->MarkDirty();
}

i32 DropdownSelected(const Widget* w) {
  if (!w || w->kind() != WidgetKind::kDropdown || !w->registry()) return -1;
  DropdownContent* c =
      w->registry()->Get<DropdownContent>(const_cast<Widget*>(w)->handle());
  return c ? c->selected : -1;
}

String DropdownSelectedText(const Widget* w) {
  if (!w || w->kind() != WidgetKind::kDropdown || !w->registry()) return "";
  DropdownContent* c =
      w->registry()->Get<DropdownContent>(const_cast<Widget*>(w)->handle());
  if (!c || c->selected < 0 ||
      c->selected >= static_cast<i32>(c->options.size()))
    return "";
  return c->options[c->selected];
}

void SetDropdownChange(Widget* w, Function<void(i32, const String&)> handler) {
  if (!w || w->kind() != WidgetKind::kDropdown || !w->registry()) return;
  w->registry()->GetOrAdd<DropdownContent>(w->handle()).on_change =
      std::move(handler);
}

}  // namespace ugui
