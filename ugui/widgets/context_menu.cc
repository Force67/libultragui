#include <ugui/widgets/context_menu.h>

#include <ugui/platform/platform.h>
#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/text/text_engine.h>
#include <ugui/ui_context.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>

namespace ugui {
namespace {

constexpr f32 kRowHPadding = 16.0f;
constexpr f32 kSeparatorHeight = 1.0f;
constexpr f32 kSeparatorPad = 4.0f;

TextEngine* text_engine(const Widget& w) {
  return w.context() ? w.context()->text_engine : nullptr;
}

FontHandle effective_font(const Widget& w) {
  return w.context() ? w.context()->default_font : kInvalidFont;
}

void ContextMenuMeasure(WidgetRegistry& world, Widget& w, f32& out_width,
                        f32& out_height) {
  ContextMenuContent* c = world.Get<ContextMenuContent>(w.handle());
  const Style& st = w.style();
  TextEngine* te = text_engine(w);
  FontHandle fh = effective_font(w);
  f32 sc = w.ui_scale();
  f32 font_size = (st.font_size > 0.0f ? st.font_size : 14.0f) * sc;
  f32 letter_sp = st.letter_spacing * sc;
  f32 row_height = font_size * 1.8f;

  f32 max_width = 0.0f;
  f32 total_height = 0.0f;

  if (c) {
    for (auto& item : c->items) {
      if (item.separator) {
        total_height += kSeparatorHeight + kSeparatorPad * 2.0f;
      } else {
        total_height += row_height;
        if (te && fh != kInvalidFont && !item.label.empty()) {
          auto run = te->Shape(fh, item.label.c_str(),
                               static_cast<u32>(item.label.size()), font_size,
                               letter_sp, st.line_height_multiplier);
          max_width = std::max(max_width, run.total_advance);
        }
      }
    }
  }

  out_width = max_width + kRowHPadding * 2.0f + st.padding.horizontal();
  out_height = total_height + st.padding.vertical();
}

void ContextMenuDraw(WidgetRegistry& world, Widget& w, Renderer2D& renderer) {
  // The base Widget::OnPaint already drew background / shadow / border.
  ContextMenuContent* c = world.Get<ContextMenuContent>(w.handle());
  TextEngine* te = text_engine(w);
  FontHandle fh = effective_font(w);
  if (!c || !te || fh == kInvalidFont) return;

  Style s = w.ComputedStyle();
  s.Scale(w.ui_scale());
  f32 alpha = s.opacity;
  f32 font_size = s.font_size > 0.0f ? s.font_size : 14.0f;
  f32 row_height = font_size * 1.8f;

  Rect rect = w.rect();
  Rect content = w.content_rect();

  // Determine hover index from mouse position.
  c->hover_index = -1;
  if (w.context() && w.context()->platform) {
    Vec2 mouse =
        w.InputToLayoutPoint(w.context()->platform->input_queue().mouse_pos);
    f32 y = content.y;
    for (i32 i = 0; i < static_cast<i32>(c->items.size()); ++i) {
      f32 item_h = c->items[i].separator
                       ? (kSeparatorHeight + kSeparatorPad * 2.0f)
                       : row_height;
      if (!c->items[i].separator && mouse.y >= y && mouse.y < y + item_h &&
          mouse.x >= rect.x && mouse.x <= rect.x + rect.w) {
        c->hover_index = i;
        break;
      }
      y += item_h;
    }
  }

  Color text_color = s.text_color.WithAlpha(s.text_color.a * alpha);
  Color highlight = s.text_color.WithAlpha(0.10f * alpha);
  Color sep_color = s.text_color.WithAlpha(0.18f * alpha);

  f32 y = content.y;
  for (i32 i = 0; i < static_cast<i32>(c->items.size()); ++i) {
    if (c->items[i].separator) {
      f32 sep_y = y + kSeparatorPad;
      renderer.DrawRect({content.x + kRowHPadding * 0.5f, sep_y,
                         content.w - kRowHPadding, kSeparatorHeight},
                        sep_color);
      y += kSeparatorHeight + kSeparatorPad * 2.0f;
      continue;
    }

    if (i == c->hover_index) {
      u32 row_radii = Vertex2D::PackRadii(4.0f);
      renderer.DrawRect({content.x, y, content.w, row_height}, highlight,
                        row_radii);
    }

    auto run = te->Shape(fh, c->items[i].label.c_str(),
                         static_cast<u32>(c->items[i].label.size()), font_size,
                         s.letter_spacing, s.line_height_multiplier);

    f32 text_x = content.x + kRowHPadding;
    f32 text_y = y + (row_height - run.line_height) * 0.5f;

    renderer.DrawText(Vec2{text_x, text_y}, run, text_color,
                      te->atlas_texture());

    y += row_height;
  }
}

bool ContextMenuClick(WidgetRegistry& world, Widget& w) {
  ContextMenuContent* c = world.Get<ContextMenuContent>(w.handle());
  if (!c || !w.context() || !w.context()->platform || c->items.empty())
    return false;

  Vec2 mouse =
      w.InputToLayoutPoint(w.context()->platform->input_queue().mouse_pos);

  Style s = w.ComputedStyle();
  f32 font_size = s.font_size > 0.0f ? s.font_size : 14.0f;
  f32 row_height = font_size * 1.8f;

  Rect rect = w.rect();
  Rect content = w.content_rect();

  f32 y = content.y;
  for (i32 i = 0; i < static_cast<i32>(c->items.size()); ++i) {
    f32 item_h = c->items[i].separator
                     ? (kSeparatorHeight + kSeparatorPad * 2.0f)
                     : row_height;
    if (!c->items[i].separator && mouse.y >= y && mouse.y < y + item_h &&
        mouse.x >= rect.x && mouse.x <= rect.x + rect.w) {
      if (c->items[i].action) c->items[i].action();
      return true;
    }
    y += item_h;
  }

  return true;  // consume the click even if nothing was hit
}

void ContextMenuDismiss(WidgetRegistry& world, Widget& w) {
  ContextMenuContent* c = world.Get<ContextMenuContent>(w.handle());
  if (!c) return;
  c->visible = false;
  c->hover_index = -1;
}

}  // namespace

WidgetVTable ContextMenuVTable() {
  WidgetVTable vt;
  vt.draw = ContextMenuDraw;
  vt.measure = ContextMenuMeasure;
  vt.on_click = ContextMenuClick;
  vt.on_dismiss = ContextMenuDismiss;
  return vt;
}

Widget* CreateContextMenu(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kContextMenu);
  WidgetRegistry::Active()->Add<ContextMenuContent>(w->handle(),
                                                    ContextMenuContent{});
  return w;
}

void AddContextMenuItem(Widget* w, const String& label,
                        Function<void()> action) {
  if (!w || w->kind() != WidgetKind::kContextMenu || !w->registry()) return;
  w->registry()->GetOrAdd<ContextMenuContent>(w->handle()).items.push_back(
      {label, std::move(action), false});
  w->MarkDirty();
}

void AddContextMenuSeparator(Widget* w) {
  if (!w || w->kind() != WidgetKind::kContextMenu || !w->registry()) return;
  w->registry()->GetOrAdd<ContextMenuContent>(w->handle()).items.push_back(
      {"", nullptr, true});
  w->MarkDirty();
}

void ClearContextMenuItems(Widget* w) {
  if (!w || w->kind() != WidgetKind::kContextMenu || !w->registry()) return;
  w->registry()->GetOrAdd<ContextMenuContent>(w->handle()).items.clear();
  w->MarkDirty();
}

void ShowContextMenuAt(Widget* w, UIContext* ctx, Vec2 position) {
  if (!w || !ctx || w->kind() != WidgetKind::kContextMenu || !w->registry())
    return;
  ContextMenuContent& c =
      w->registry()->GetOrAdd<ContextMenuContent>(w->handle());
  c.visible = true;

  // Register the overlay first so the widget is bound to the UI context
  // (ShowOverlay calls SetContext); otherwise Measure runs without a text
  // engine and the shaping branch is skipped, leaving the width at 0.
  ctx->ShowOverlay(w, position);

  Style s = w->style();
  if (s.font_size <= 0.0f) s.font_size = 13.0f;
  w->set_style(s);

  f32 mw = 0, mh = 0;
  ContextMenuMeasure(*w->registry(), *w, mw, mh);
  s.width = Length::Px(mw);
  s.height = Length::Px(mh);
  s.margin = EdgeInsets(position.y, 0, 0, position.x);
  w->set_style(s);
}

void HideContextMenu(Widget* w, UIContext* ctx) {
  if (!w || !ctx || w->kind() != WidgetKind::kContextMenu || !w->registry())
    return;
  ContextMenuContent& c =
      w->registry()->GetOrAdd<ContextMenuContent>(w->handle());
  if (!c.visible) return;
  c.visible = false;
  c.hover_index = -1;
  ctx->HideOverlay(w);
}

}  // namespace ugui
