#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/checkbox.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace ugui {

void Checkbox::set_checked(bool v) {
  if (v) {
    state_ = state_ | WidgetState::kChecked;
  } else {
    state_ = static_cast<WidgetState>(static_cast<u16>(state_) &
                                      ~static_cast<u16>(WidgetState::kChecked));
  }
  MarkPaintDirty();
}

bool Checkbox::OnClick() {
  set_checked(!checked());
  if (on_change_) {
    on_change_(checked());
  }
  return true;
}

void Checkbox::Measure(f32& out_width, f32& out_height) {
  f32 box_size = style_.font_size * 1.2f;
  constexpr f32 kGap = 8.0f;

  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (te && fh != kInvalidFont && !label_.empty()) {
    auto run = te->Shape(fh, label_.c_str(), static_cast<u32>(label_.size()),
                         style_.font_size, style_.letter_spacing,
                         style_.line_height_multiplier);
    out_width = box_size + kGap + run.total_advance;
    out_height = std::max(box_size, run.line_height);
  } else {
    out_width = box_size;
    out_height = box_size;
  }
}

void Checkbox::OnPaint(Renderer2D& renderer) {
  // Base widget paint for shadow, background, border, gradient
  Widget::OnPaint(renderer);

  auto s = ComputedStyle();
  f32 alpha = s.opacity;
  f32 box_size = s.font_size * 1.0f;
  constexpr f32 kGap = 8.0f;
  f32 corner = std::min(box_size * 0.2f, 4.0f);
  u32 radii = Vertex2D::PackRadii(corner);

  // Position the box at the left of content_rect, vertically centered
  f32 box_x = content_rect_.x;
  f32 box_y = content_rect_.y + (content_rect_.h - box_size) * 0.5f;

  bool is_checked = checked();

  if (is_checked) {
    // Checked: filled box with accent color
    Color accent = (s.background.a > 0.0f)
                       ? s.background.WithAlpha(s.background.a * alpha)
                       : Color::FromHex(0x4a4aff, alpha);
    Color border = (s.border_color.a > 0.0f)
                       ? s.border_color.WithAlpha(s.border_color.a * alpha)
                       : accent;
    f32 bw = std::max(s.border_width, 1.5f);
    renderer.DrawBorderedRect({box_x, box_y, box_size, box_size}, accent,
                              border, bw, radii);

    // Checkmark indicator: smaller white rect centered inside
    f32 inner_size = box_size * 0.45f;
    f32 inner_x = box_x + (box_size - inner_size) * 0.5f;
    f32 inner_y = box_y + (box_size - inner_size) * 0.5f;
    f32 inner_corner = std::min(inner_size * 0.2f, 2.0f);
    u32 inner_radii = Vertex2D::PackRadii(inner_corner);
    renderer.DrawRect({inner_x, inner_y, inner_size, inner_size},
                      Color(1.0f, 1.0f, 1.0f, alpha), inner_radii);
  } else {
    // Unchecked: transparent fill with border
    Color border = (s.border_color.a > 0.0f)
                       ? s.border_color.WithAlpha(s.border_color.a * alpha)
                       : Color(0.6f, 0.6f, 0.6f, alpha);
    f32 bw = std::max(s.border_width, 1.5f);
    renderer.DrawBorderedRect({box_x, box_y, box_size, box_size},
                              Color::Transparent(), border, bw, radii);
  }

  // Draw label text to the right of the box
  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (te && fh != kInvalidFont && !label_.empty()) {
    auto run = te->Shape(fh, label_.c_str(), static_cast<u32>(label_.size()),
                         s.font_size, s.letter_spacing,
                         s.line_height_multiplier);

    f32 text_x = box_x + box_size + kGap;
    f32 text_y = content_rect_.y + (content_rect_.h - run.line_height) * 0.5f;

    Color text_color = s.text_color.WithAlpha(s.text_color.a * alpha);

    // Text shadow
    if (s.text_shadow_color.a > 0.0f) {
      Vec2 shadow_pos = {text_x + s.text_shadow_offset.x,
                         text_y + s.text_shadow_offset.y};
      Color shadow_color =
          s.text_shadow_color.WithAlpha(s.text_shadow_color.a * alpha);
      renderer.DrawText(shadow_pos, run, shadow_color, te->atlas_texture());
    }

    renderer.DrawText(Vec2{text_x, text_y}, run, text_color, te->atlas_texture());
  }
}

}  // namespace ugui
