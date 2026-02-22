#include <ultragui/platform/platform.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/dropdown.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace ugui {

void Dropdown::set_selected_index(i32 idx) {
  if (options_.empty()) {
    selected_ = -1;
  } else {
    selected_ = static_cast<i32>(
        Clamp(static_cast<f32>(idx), 0.0f,
              static_cast<f32>(static_cast<i32>(options_.size()) - 1)));
  }
  MarkDirty();
}

Widget* Dropdown::HitTest(Vec2 point) {
  // When open, expand hit area to include the option list below
  if (open_ && !options_.empty()) {
    auto s = ComputedStyle();
    f32 row_height = s.font_size * 1.5f;
    f32 list_h = row_height * static_cast<f32>(options_.size());
    Rect expanded = {rect_.x, rect_.y, rect_.w, rect_.h + list_h};
    if (expanded.contains(point))
      return this;
  }
  return Widget::HitTest(point);
}

bool Dropdown::OnClick() {
  if (!open_) {
    open_ = true;
    MarkPaintDirty();
    return true;
  }
  // Click on the header while open - close
  open_ = false;
  MarkPaintDirty();
  return true;
}

void Dropdown::OnUpdate(f64) {
  if (!open_ || options_.empty() || !context_ || !context_->platform)
    return;

  auto& queue = context_->platform->input_queue();

  // Check for mouse button press events while open
  for (u32 i = 0; i < queue.button_count; ++i) {
    auto& evt = queue.button_events[i];
    if (!evt.pressed)
      continue;

    auto s = ComputedStyle();
    f32 row_height = s.font_size * 1.5f;
    f32 list_top = rect_.y + rect_.h;
    f32 list_bottom = list_top + row_height * static_cast<f32>(options_.size());

    Vec2 pos = InputToLayoutPoint(evt.position);
    if (pos.y >= list_top && pos.y < list_bottom &&
        pos.x >= rect_.x && pos.x <= rect_.x + rect_.w) {
      i32 idx = static_cast<i32>((pos.y - list_top) / row_height);
      if (idx >= 0 && idx < static_cast<i32>(options_.size())) {
        selected_ = idx;
        if (on_change_)
          on_change_(selected_, options_[selected_]);
      }
    }
    // Close on any click while open
    open_ = false;
    MarkPaintDirty();
    return;
  }
}

void Dropdown::Measure(f32& out_width, f32& out_height) {
  auto* te = text_engine();
  FontHandle fh = effective_font();
  f32 font_size = style_.font_size;
  constexpr f32 kChevronWidth = 24.0f;
  constexpr f32 kHPadding = 12.0f;

  if (!te || fh == kInvalidFont || options_.empty()) {
    // Measure placeholder "Select..."
    out_width = kChevronWidth + kHPadding * 2.0f + 60.0f;
    out_height = font_size * 1.5f + style_.padding.vertical();
    return;
  }

  // Find the widest option text
  f32 max_width = 0.0f;
  for (auto& opt : options_) {
    auto run = te->Shape(fh, opt.c_str(), static_cast<u32>(opt.size()),
                         font_size, style_.letter_spacing,
                         style_.line_height_multiplier);
    max_width = std::max(max_width, run.total_advance);
  }

  // Also measure placeholder in case no option is selected
  const char* placeholder = "Select...";
  auto ph_run = te->Shape(fh, placeholder, 9, font_size, style_.letter_spacing,
                          style_.line_height_multiplier);
  max_width = std::max(max_width, ph_run.total_advance);

  out_width = max_width + kChevronWidth + kHPadding * 2.0f +
              style_.padding.horizontal();
  out_height = font_size * 1.5f + style_.padding.vertical();
}

void Dropdown::OnPaint(Renderer2D& renderer) {
  // Base widget paint for shadow, background, border, gradient
  Widget::OnPaint(renderer);

  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (!te || fh == kInvalidFont) return;

  auto s = ComputedStyle();
  f32 alpha = s.opacity;
  f32 font_size = s.font_size;
  f32 row_height = font_size * 1.5f;
  constexpr f32 kChevronWidth = 24.0f;
  constexpr f32 kHPadding = 8.0f;

  // Determine hover index from mouse position when open
  if (open_ && context_ && context_->platform) {
    Vec2 mouse = InputToLayoutPoint(context_->platform->input_queue().mouse_pos);
    f32 list_top = rect_.y + rect_.h;
    f32 list_bottom =
        list_top + row_height * static_cast<f32>(options_.size());
    if (mouse.y >= list_top && mouse.y < list_bottom &&
        mouse.x >= rect_.x && mouse.x <= rect_.x + rect_.w) {
      hover_index_ = static_cast<i32>((mouse.y - list_top) / row_height);
    } else {
      hover_index_ = -1;
    }
  } else {
    hover_index_ = -1;
  }

  // --- Draw current selection text (or placeholder) ---
  std::string display_text =
      (selected_ >= 0 && selected_ < static_cast<i32>(options_.size()))
          ? options_[selected_]
          : "Select...";

  auto run = te->Shape(fh, display_text.c_str(),
                       static_cast<u32>(display_text.size()), font_size,
                       s.letter_spacing, s.line_height_multiplier);

  f32 text_x = content_rect_.x + kHPadding;
  f32 text_y =
      content_rect_.y + (content_rect_.h - run.line_height) * 0.5f;

  Color text_color = (selected_ >= 0)
                         ? s.text_color.WithAlpha(s.text_color.a * alpha)
                         : s.text_color.WithAlpha(s.text_color.a * alpha * 0.5f);

  renderer.DrawText(Vec2{text_x, text_y}, run, text_color,
                    te->atlas_texture());

  // --- Draw chevron indicator on the right side ---
  f32 chevron_x = content_rect_.x + content_rect_.w - kChevronWidth;
  f32 chevron_cy = content_rect_.y + content_rect_.h * 0.5f;
  f32 chevron_size = font_size * 0.3f;
  f32 line_thickness = std::max(1.5f, font_size / 10.0f);
  Color chevron_color = s.text_color.WithAlpha(s.text_color.a * alpha * 0.7f);

  if (open_) {
    // Upward chevron "^"
    // Left stroke: bottom-left to center-top
    renderer.DrawRect(
        {chevron_x + 2.0f, chevron_cy - chevron_size * 0.3f,
         chevron_size * 0.8f, line_thickness},
        chevron_color);
    // Right stroke: center-top to bottom-right
    renderer.DrawRect(
        {chevron_x + 2.0f + chevron_size * 0.8f,
         chevron_cy - chevron_size * 0.3f, chevron_size * 0.8f,
         line_thickness},
        chevron_color);
  } else {
    // Downward chevron "v"
    // Left stroke
    renderer.DrawRect(
        {chevron_x + 2.0f, chevron_cy - line_thickness * 0.5f,
         chevron_size * 0.8f, line_thickness},
        chevron_color);
    // Right stroke
    renderer.DrawRect(
        {chevron_x + 2.0f + chevron_size * 0.8f,
         chevron_cy - line_thickness * 0.5f, chevron_size * 0.8f,
         line_thickness},
        chevron_color);
  }

  // --- Draw option list when open ---
  if (!open_ || options_.empty()) return;

  f32 list_x = rect_.x;
  f32 list_y = rect_.y + rect_.h;
  f32 list_w = rect_.w;
  f32 list_h = row_height * static_cast<f32>(options_.size());

  // List background
  Color list_bg = (s.background.a > 0.0f)
                      ? s.background.WithAlpha(Clamp(s.background.a + 0.1f, 0.0f, 1.0f) * alpha)
                      : Color(0.12f, 0.12f, 0.2f, 0.95f * alpha);
  u32 list_radii = Vertex2D::PackRadii(0.0f, 0.0f, s.corner_radius_br,
                                       s.corner_radius_bl);
  renderer.DrawRect({list_x, list_y, list_w, list_h}, list_bg, list_radii);

  // List border
  if (s.border_color.a > 0.0f && s.border_width > 0.0f) {
    Color border = s.border_color.WithAlpha(s.border_color.a * alpha);
    renderer.DrawBorderedRect({list_x, list_y, list_w, list_h},
                              Color::Transparent(), border,
                              s.border_width, list_radii);
  }

  // Draw each option row
  for (i32 i = 0; i < static_cast<i32>(options_.size()); ++i) {
    f32 row_y = list_y + row_height * static_cast<f32>(i);

    // Highlight hovered or selected row
    bool is_hovered = (i == hover_index_);
    bool is_selected = (i == selected_);

    if (is_hovered || is_selected) {
      Color highlight =
          is_hovered
              ? Color(1.0f, 1.0f, 1.0f, 0.1f * alpha)
              : s.text_color.WithAlpha(0.15f * alpha);
      // Round bottom corners on last item
      u32 row_radii = 0;
      if (i == static_cast<i32>(options_.size()) - 1) {
        row_radii = Vertex2D::PackRadii(0.0f, 0.0f, s.corner_radius_br,
                                        s.corner_radius_bl);
      }
      renderer.DrawRect({list_x, row_y, list_w, row_height}, highlight,
                         row_radii);
    }

    // Draw option text
    auto opt_run =
        te->Shape(fh, options_[i].c_str(),
                  static_cast<u32>(options_[i].size()), font_size,
                  s.letter_spacing, s.line_height_multiplier);

    f32 opt_x = list_x + kHPadding + s.padding.left;
    f32 opt_y = row_y + (row_height - opt_run.line_height) * 0.5f;

    Color opt_color = s.text_color.WithAlpha(s.text_color.a * alpha);
    renderer.DrawText(Vec2{opt_x, opt_y}, opt_run, opt_color,
                      te->atlas_texture());
  }
}

}  // namespace ugui
