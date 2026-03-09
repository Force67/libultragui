#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/radio.h>

#include <algorithm>
#include <string>

namespace ugui {

void Radio::set_selected(bool v) {
  if (v) {
    state_ = state_ | WidgetState::kChecked;
  } else {
    state_ = static_cast<WidgetState>(static_cast<u16>(state_) &
                                      ~static_cast<u16>(WidgetState::kChecked));
  }
  MarkPaintDirty();
}

void Radio::DeselectSiblings() {
  if (!parent_) return;
  for (auto* child : parent_->children()) {
    if (child == this) continue;
    auto* sibling = dynamic_cast<Radio*>(child);
    if (sibling && sibling->group() == group_) {
      sibling->set_selected(false);
    }
  }
}

bool Radio::OnClick() {
  DeselectSiblings();
  set_selected(true);
  if (on_change_) {
    on_change_(true);
  }
  return true;
}

void Radio::Measure(f32& out_width, f32& out_height) {
  f32 sc = ui_scale();
  f32 circle_size = style_.font_size * 1.2f;
  constexpr f32 kGap = 8.0f;

  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (te && fh != kInvalidFont && !label_.empty()) {
    auto run = te->Shape(fh, label_.c_str(), static_cast<u32>(label_.size()),
                         style_.font_size * sc, style_.letter_spacing * sc,
                         style_.line_height_multiplier);
    out_width = circle_size + kGap + run.total_advance;
    out_height = std::max(circle_size, run.line_height);
  } else {
    out_width = circle_size;
    out_height = circle_size;
  }
}

void Radio::OnPaint(Renderer2D& renderer) {
  // Base widget paint for shadow, background, border, gradient
  Widget::OnPaint(renderer);

  auto s = ComputedStyle();
  s.Scale(ui_scale());
  f32 alpha = s.opacity;
  f32 circle_size = s.font_size * 1.0f;
  constexpr f32 kGap = 8.0f;
  f32 radius = circle_size * 0.5f;
  u32 radii = Vertex2D::PackRadii(radius);

  // Position the circle at the left of content_rect, vertically centered
  f32 circle_x = content_rect_.x;
  f32 circle_y = content_rect_.y + (content_rect_.h - circle_size) * 0.5f;

  bool is_selected = selected();

  // Outer circle: always draw with border
  Color border_color = (s.border_color.a > 0.0f)
                           ? s.border_color.WithAlpha(s.border_color.a * alpha)
                           : Color(0.6f, 0.6f, 0.6f, alpha);
  f32 bw = std::max(s.border_width, 1.5f);

  if (is_selected) {
    Color accent = Color::FromHex(0x4a7aff, alpha);
    renderer.DrawBorderedRect({circle_x, circle_y, circle_size, circle_size},
                              Color::Transparent(), accent.WithAlpha(alpha), bw,
                              radii);

    // Inner filled circle (60% of outer)
    f32 inner_size = circle_size * 0.6f;
    f32 inner_x = circle_x + (circle_size - inner_size) * 0.5f;
    f32 inner_y = circle_y + (circle_size - inner_size) * 0.5f;
    f32 inner_radius = inner_size * 0.5f;
    u32 inner_radii = Vertex2D::PackRadii(inner_radius);
    renderer.DrawRect({inner_x, inner_y, inner_size, inner_size}, accent,
                      inner_radii);
  } else {
    renderer.DrawBorderedRect({circle_x, circle_y, circle_size, circle_size},
                              Color::Transparent(), border_color, bw, radii);
  }

  // Draw label text to the right of the circle
  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (te && fh != kInvalidFont && !label_.empty()) {
    auto run =
        te->Shape(fh, label_.c_str(), static_cast<u32>(label_.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);

    f32 text_x = circle_x + circle_size + kGap;
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

    renderer.DrawText(Vec2{text_x, text_y}, run, text_color,
                      te->atlas_texture());
  }
}

}  // namespace ugui
