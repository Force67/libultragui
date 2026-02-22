#include <ultragui/platform/platform.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/ui_context.h>
#include <ultragui/widgets/context_menu.h>

#include <algorithm>

namespace ugui {

void ContextMenu::AddItem(const String& label,
                          Function<void()> action) {
  items_.push_back({label, std::move(action), false});
  MarkDirty();
}

void ContextMenu::AddSeparator() {
  items_.push_back({"", nullptr, true});
  MarkDirty();
}

void ContextMenu::ClearItems() {
  items_.clear();
  MarkDirty();
}

void ContextMenu::ShowAt(UIContext* ctx, Vec2 position) {
  visible_ = true;

  // Style the menu: dark background, subtle border, rounded corners, padding
  Style s;
  s.background = Color::FromHex(0x222222);
  s.border_color = Color(1.0f, 1.0f, 1.0f, 0.12f);
  s.border_width = 1.0f;
  s.corner_radius = s.corner_radius_tl = s.corner_radius_tr =
      s.corner_radius_br = s.corner_radius_bl = 6.0f;
  s.padding = EdgeInsets(4);
  s.font_size = style_.font_size > 0.0f ? style_.font_size : 14.0f;
  s.text_color =
      style_.text_color.a > 0.0f ? style_.text_color : Color::White();
  // Position via margin (same pattern as tooltip overlay)
  s.margin = EdgeInsets(position.y, 0, 0, position.x);
  set_style(s);

  ctx->ShowOverlay(this, position);
}

void ContextMenu::Hide(UIContext* ctx) {
  if (!visible_) return;
  visible_ = false;
  hover_index_ = -1;
  ctx->HideOverlay(this);
}

bool ContextMenu::OnClick() {
  if (!context_ || !context_->platform || items_.empty()) return false;

  Vec2 mouse = InputToLayoutPoint(context_->platform->input_queue().mouse_pos);

  auto s = ComputedStyle();
  f32 font_size = s.font_size > 0.0f ? s.font_size : 14.0f;
  f32 row_height = font_size * 1.8f;
  constexpr f32 kSeparatorHeight = 1.0f;
  constexpr f32 kSeparatorPad = 4.0f;  // vertical padding around separator

  // Walk items to find which row was clicked
  f32 y = content_rect_.y;
  for (i32 i = 0; i < static_cast<i32>(items_.size()); ++i) {
    f32 item_h =
        items_[i].separator ? (kSeparatorHeight + kSeparatorPad * 2.0f)
                            : row_height;
    if (!items_[i].separator && mouse.y >= y && mouse.y < y + item_h &&
        mouse.x >= rect_.x && mouse.x <= rect_.x + rect_.w) {
      // Execute the action
      if (items_[i].action) {
        items_[i].action();
      }
      return true;
    }
    y += item_h;
  }

  return true;  // Consume click even if nothing was hit
}

void ContextMenu::Measure(f32& out_width, f32& out_height) {
  auto* te = text_engine();
  FontHandle fh = effective_font();
  f32 sc = ui_scale();
  f32 font_size = (style_.font_size > 0.0f ? style_.font_size : 14.0f) * sc;
  f32 letter_sp = style_.letter_spacing * sc;
  f32 row_height = font_size * 1.8f;
  constexpr f32 kSeparatorHeight = 1.0f;
  constexpr f32 kSeparatorPad = 4.0f;
  constexpr f32 kHPadding = 24.0f;  // horizontal padding per side

  f32 max_width = 0.0f;
  f32 total_height = 0.0f;

  for (auto& item : items_) {
    if (item.separator) {
      total_height += kSeparatorHeight + kSeparatorPad * 2.0f;
    } else {
      total_height += row_height;
      if (te && fh != kInvalidFont && !item.label.empty()) {
        auto run = te->Shape(fh, item.label.c_str(),
                             static_cast<u32>(item.label.size()), font_size,
                             letter_sp,
                             style_.line_height_multiplier);
        max_width = std::max(max_width, run.total_advance);
      }
    }
  }

  out_width = max_width + kHPadding * 2.0f + style_.padding.horizontal();
  out_height = total_height + style_.padding.vertical();
}

void ContextMenu::OnPaint(Renderer2D& renderer) {
  // Base widget draws shadow, background, border
  Widget::OnPaint(renderer);

  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (!te || fh == kInvalidFont) return;

  auto s = ComputedStyle();
  s.Scale(ui_scale());
  f32 alpha = s.opacity;
  f32 font_size = s.font_size > 0.0f ? s.font_size : 14.0f;
  f32 row_height = font_size * 1.8f;
  constexpr f32 kSeparatorHeight = 1.0f;
  constexpr f32 kSeparatorPad = 4.0f;
  constexpr f32 kHPadding = 12.0f;

  // Determine hover index from mouse position
  hover_index_ = -1;
  if (context_ && context_->platform) {
    Vec2 mouse = InputToLayoutPoint(context_->platform->input_queue().mouse_pos);
    f32 y = content_rect_.y;
    for (i32 i = 0; i < static_cast<i32>(items_.size()); ++i) {
      f32 item_h =
          items_[i].separator ? (kSeparatorHeight + kSeparatorPad * 2.0f)
                              : row_height;
      if (!items_[i].separator && mouse.y >= y && mouse.y < y + item_h &&
          mouse.x >= rect_.x && mouse.x <= rect_.x + rect_.w) {
        hover_index_ = i;
        break;
      }
      y += item_h;
    }
  }

  // Draw each item
  f32 y = content_rect_.y;
  for (i32 i = 0; i < static_cast<i32>(items_.size()); ++i) {
    if (items_[i].separator) {
      // Draw horizontal separator line
      f32 sep_y = y + kSeparatorPad;
      Color sep_color = Color(1.0f, 1.0f, 1.0f, 0.1f * alpha);
      renderer.DrawRect(
          {content_rect_.x + kHPadding * 0.5f, sep_y,
           content_rect_.w - kHPadding, kSeparatorHeight},
          sep_color);
      y += kSeparatorHeight + kSeparatorPad * 2.0f;
      continue;
    }

    // Highlight hovered row
    if (i == hover_index_) {
      Color highlight = Color(1.0f, 1.0f, 1.0f, 0.08f * alpha);
      u32 row_radii = Vertex2D::PackRadii(4.0f);
      renderer.DrawRect(
          {content_rect_.x + 2.0f, y, content_rect_.w - 4.0f, row_height},
          highlight, row_radii);
    }

    // Draw label text
    auto run = te->Shape(fh, items_[i].label.c_str(),
                         static_cast<u32>(items_[i].label.size()), font_size,
                         s.letter_spacing, s.line_height_multiplier);

    f32 text_x = content_rect_.x + kHPadding;
    f32 text_y = y + (row_height - run.line_height) * 0.5f;

    Color text_color = s.text_color.WithAlpha(s.text_color.a * alpha);
    renderer.DrawText(Vec2{text_x, text_y}, run, text_color,
                      te->atlas_texture());

    y += row_height;
  }
}

}  // namespace ugui
