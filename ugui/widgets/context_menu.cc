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

TextEngine* text_engine(WidgetRegistry& world, wid e) {
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->text_engine : nullptr;
}

FontHandle effective_font(WidgetRegistry& world, wid e) {
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->default_font : kInvalidFont;
}

void ContextMenuMeasure(WidgetRegistry& world, wid e, f32& out_width,
                        f32& out_height) {
  ContextMenuContent* c = world.Get<ContextMenuContent>(e);
  const Style& st = world.Get<StyleC>(e)->style;
  TextEngine* te = text_engine(world, e);
  FontHandle fh = effective_font(world, e);
  f32 sc = UiScale(world, e);
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

void ContextMenuDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  // PaintWidget already drew background / shadow / border.
  ContextMenuContent* c = world.Get<ContextMenuContent>(e);
  TextEngine* te = text_engine(world, e);
  FontHandle fh = effective_font(world, e);
  if (!c || !te || fh == kInvalidFont) return;

  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;
  f32 font_size = s.font_size > 0.0f ? s.font_size : 14.0f;
  f32 row_height = font_size * 1.8f;

  Rect rect = world.Get<Transform>(e)->rect;
  Rect content = world.Get<Transform>(e)->content_rect;

  // Determine hover index from mouse position.
  c->hover_index = -1;
  const WidgetContext* wctx = WidgetContextOf(world, e);
  if (wctx && wctx->platform) {
    Vec2 mouse =
        InputToLayoutPoint(world, e, wctx->platform->input_queue().mouse_pos);
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

bool ContextMenuClick(WidgetRegistry& world, wid e) {
  ContextMenuContent* c = world.Get<ContextMenuContent>(e);
  const WidgetContext* wctx = WidgetContextOf(world, e);
  if (!c || !wctx || !wctx->platform || c->items.empty()) return false;

  Vec2 mouse =
      InputToLayoutPoint(world, e, wctx->platform->input_queue().mouse_pos);

  Style s = ComputedStyle(world, e);
  f32 font_size = s.font_size > 0.0f ? s.font_size : 14.0f;
  f32 row_height = font_size * 1.8f;

  Rect rect = world.Get<Transform>(e)->rect;
  Rect content = world.Get<Transform>(e)->content_rect;

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

void ContextMenuDismiss(WidgetRegistry& world, wid e) {
  ContextMenuContent* c = world.Get<ContextMenuContent>(e);
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

wid CreateContextMenu(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kContextMenu;
  world.Add<ContextMenuContent>(e, ContextMenuContent{});
  return e;
}

void AddContextMenuItem(wid e, const String& label, Function<void()> action) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kContextMenu) return;
  world.GetOrAdd<ContextMenuContent>(e).items.push_back(
      {label, std::move(action), false});
  MarkDirty(world, e);
}

void AddContextMenuSeparator(wid e) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kContextMenu) return;
  world.GetOrAdd<ContextMenuContent>(e).items.push_back({"", nullptr, true});
  MarkDirty(world, e);
}

void ClearContextMenuItems(wid e) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kContextMenu) return;
  world.GetOrAdd<ContextMenuContent>(e).items.clear();
  MarkDirty(world, e);
}

void ShowContextMenuAt(wid e, UIContext* ctx, Vec2 position) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!ctx || !n || n->kind != WidgetKind::kContextMenu) return;
  ContextMenuContent& c = world.GetOrAdd<ContextMenuContent>(e);
  c.visible = true;

  // Register the overlay first so the widget is bound to the UI context
  // (ShowOverlay sets context); otherwise Measure runs without a text engine.
  ctx->ShowOverlay(e, position);

  Style& s = world.Get<StyleC>(e)->style;
  if (s.font_size <= 0.0f) s.font_size = 13.0f;

  f32 mw = 0, mh = 0;
  ContextMenuMeasure(world, e, mw, mh);
  s.width = Length::Px(mw);
  s.height = Length::Px(mh);
  s.margin = EdgeInsets(position.y, 0, 0, position.x);
}

void HideContextMenu(wid e, UIContext* ctx) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!ctx || !n || n->kind != WidgetKind::kContextMenu) return;
  ContextMenuContent& c = world.GetOrAdd<ContextMenuContent>(e);
  if (!c.visible) return;
  c.visible = false;
  c.hover_index = -1;
  ctx->HideOverlay(e);
}

}  // namespace ugui
