#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/toggle.h>

#include <algorithm>
#include <cmath>

namespace ugui {

void Toggle::set_on(bool v) {
  if (v) {
    state_ = state_ | WidgetState::kChecked;
  } else {
    state_ = static_cast<WidgetState>(static_cast<u16>(state_) &
                                      ~static_cast<u16>(WidgetState::kChecked));
  }
  MarkPaintDirty();
}

bool Toggle::OnClick() {
  bool new_state = !on();
  set_on(new_state);
  if (on_change_) {
    on_change_(new_state);
  }
  return true;
}

void Toggle::OnUpdate(f64 dt) {
  f32 target = on() ? 1.0f : 0.0f;
  f32 prev = thumb_anim_;
  f32 speed = Clamp(static_cast<f32>(dt) * 12.0f, 0.0f, 1.0f);
  thumb_anim_ += (target - thumb_anim_) * speed;

  // Snap when close enough to avoid endless tiny updates
  if (std::abs(thumb_anim_ - target) < 0.001f) {
    thumb_anim_ = target;
  }

  if (thumb_anim_ != prev) {
    MarkPaintDirty();
  }
}

void Toggle::Measure(f32& out_width, f32& out_height) {
  f32 track_w = 44.0f;
  f32 track_h = 24.0f;
  out_width = track_w + style_.padding.horizontal();
  out_height = track_h + style_.padding.vertical();
}

void Toggle::OnPaint(Renderer2D& renderer) {
  // Skip Widget::OnPaint - we draw our own custom track background
  auto s = ComputedStyle();
  s.Scale(ui_scale());
  f32 alpha = s.opacity;

  // Track dimensions (content area)
  f32 track_x = content_rect_.x;
  f32 track_y = content_rect_.y;
  f32 track_w = content_rect_.w;
  f32 track_h = content_rect_.h;

  // Pill-shaped track (corner radius = half height)
  f32 track_radius = track_h * 0.5f;
  u32 track_radii = Vertex2D::PackRadii(track_radius);

  // Interpolate track color: off = dark gray, on = accent blue
  Color off_color = Color::FromHex(0x3a3a3a);
  Color on_color = Color::FromHex(0x4a7aff);

  // Use style background if explicitly set (non-zero alpha), otherwise use defaults
  if (s.background.a > 0.0f) {
    off_color = s.background;
    if (s.background_end.a > 0.0f) {
      on_color = s.background_end;
    }
  }

  Color track_color = Lerp(off_color, on_color, thumb_anim_);
  track_color = track_color.WithAlpha(track_color.a * alpha);

  Rect track_rect = {track_x, track_y, track_w, track_h};
  renderer.DrawRect(track_rect, track_color, track_radii);

  // Thumb: circle inset 2px from track edges
  f32 inset = 2.0f;
  f32 thumb_size = track_h - inset * 2.0f;
  f32 thumb_radius = thumb_size * 0.5f;
  u32 thumb_radii = Vertex2D::PackRadii(thumb_radius);

  f32 thumb_x = track_x + inset + thumb_anim_ * (track_w - thumb_size - inset * 2.0f);
  f32 thumb_y = track_y + inset;

  Color thumb_color = Color::White().WithAlpha(alpha);

  Rect thumb_rect = {thumb_x, thumb_y, thumb_size, thumb_size};
  renderer.DrawRect(thumb_rect, thumb_color, thumb_radii);
}

}  // namespace ugui
