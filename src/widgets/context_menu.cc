#include <ultragui/platform/platform.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/ui_context.h>
#include <ultragui/widgets/context_menu.h>

#include <algorithm>

namespace ugui {

namespace {

// Shared between Measure and OnPaint so the menu's intrinsic width
// always matches where the text is actually drawn (and where the hover
// highlight extends to). Splitting these used to give the highlight a
// 12px-vs-24px asymmetry that looked broken on hover.
constexpr f32 kRowHPadding = 16.0f;
constexpr f32 kSeparatorHeight = 1.0f;
constexpr f32 kSeparatorPad = 4.0f;

}  // namespace

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

  // Library policy: ContextMenu has NO visual defaults. Application
  // code is expected to set the widget's style (background, border,
  // padding, font_size, text_color, shadow) once after construction
  // - typically by applying a style class declared in a .ugui file.
  // We only manage layout-affecting properties (intrinsic size +
  // position via margin) here.

  // Register the overlay first so the widget is bound to the UI
  // context (ShowOverlay calls SetContext). Otherwise Measure would
  // run with context_ == nullptr, the text_engine() is null, and the
  // shaping branch in Measure is skipped - leaving max_width at 0.
  ctx->ShowOverlay(this, position);

  Style s = style_;

  // Sensible fallback for font_size only - without a positive font
  // size Measure can't shape any text. Everything else is the app's
  // responsibility.
  if (s.font_size <= 0.0f) s.font_size = 13.0f;
  set_style(s);

  // Now measure with the context (and font size) wired up.
  f32 mw = 0, mh = 0;
  Measure(mw, mh);
  s.width = Length::Px(mw);
  s.height = Length::Px(mh);
  s.margin = EdgeInsets(position.y, 0, 0, position.x);
  set_style(s);
}

void ContextMenu::Hide(UIContext* ctx) {
  if (!visible_) return;
  visible_ = false;
  hover_index_ = -1;
  ctx->HideOverlay(this);
}

void ContextMenu::OnDismiss() {
  visible_ = false;
  hover_index_ = -1;
}

bool ContextMenu::OnClick() {
  if (!context_ || !context_->platform || items_.empty()) return false;

  Vec2 mouse = InputToLayoutPoint(context_->platform->input_queue().mouse_pos);

  auto s = ComputedStyle();
  f32 font_size = s.font_size > 0.0f ? s.font_size : 14.0f;
  f32 row_height = font_size * 1.8f;

  // Walk items to find which row was clicked
  f32 y = content_rect_.y;
  for (i32 i = 0; i < static_cast<i32>(items_.size()); ++i) {
    f32 item_h =
        items_[i].separator ? (kSeparatorHeight + kSeparatorPad * 2.0f)
                            : row_height;
    if (!items_[i].separator && mouse.y >= y && mouse.y < y + item_h &&
        mouse.x >= rect_.x && mouse.x <= rect_.x + rect_.w) {
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

  out_width = max_width + kRowHPadding * 2.0f + style_.padding.horizontal();
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

  // Theme-aware accent: derive hover/separator colors from text_color so
  // light themes get a dark wash and dark themes get a light wash. This
  // is what makes the menu look at home on Latte without looking washed
  // out on a hypothetical dark theme.
  Color text_color = s.text_color.WithAlpha(s.text_color.a * alpha);
  Color highlight = s.text_color.WithAlpha(0.10f * alpha);
  Color sep_color = s.text_color.WithAlpha(0.18f * alpha);

  // Draw each item
  f32 y = content_rect_.y;
  for (i32 i = 0; i < static_cast<i32>(items_.size()); ++i) {
    if (items_[i].separator) {
      f32 sep_y = y + kSeparatorPad;
      renderer.DrawRect(
          {content_rect_.x + kRowHPadding * 0.5f, sep_y,
           content_rect_.w - kRowHPadding, kSeparatorHeight},
          sep_color);
      y += kSeparatorHeight + kSeparatorPad * 2.0f;
      continue;
    }

    // Highlight hovered row - full content width so it covers the text
    // edge to edge instead of stopping short.
    if (i == hover_index_) {
      u32 row_radii = Vertex2D::PackRadii(4.0f);
      renderer.DrawRect(
          {content_rect_.x, y, content_rect_.w, row_height},
          highlight, row_radii);
    }

    // Draw label text aligned with the same kRowHPadding the menu was
    // measured against, so the menu's intrinsic width matches.
    auto run = te->Shape(fh, items_[i].label.c_str(),
                         static_cast<u32>(items_[i].label.size()), font_size,
                         s.letter_spacing, s.line_height_multiplier);

    f32 text_x = content_rect_.x + kRowHPadding;
    f32 text_y = y + (row_height - run.line_height) * 0.5f;

    renderer.DrawText(Vec2{text_x, text_y}, run, text_color,
                      te->atlas_texture());

    y += row_height;
  }
}

}  // namespace ugui
