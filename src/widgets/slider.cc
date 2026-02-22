#include <ultragui/platform/platform.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/slider.h>

namespace ugui {

void Slider::set_value(f32 v) {
  value_ = Clamp(v, min_, max_);
  MarkPaintDirty();
}

bool Slider::OnClick() {
  dragging_ = true;

  if (context_ && context_->platform) {
    f32 mouse_x = InputToLayoutPoint(context_->platform->input_queue().mouse_pos).x;
    f32 track_x = content_rect_.x;
    f32 track_w = content_rect_.w;

    if (track_w > 0.0f) {
      f32 t = Clamp((mouse_x - track_x) / track_w, 0.0f, 1.0f);
      f32 new_value = min_ + t * (max_ - min_);
      if (new_value != value_) {
        value_ = new_value;
        MarkPaintDirty();
        if (on_change_)
          on_change_(value_);
      }
    }
  }

  return true;
}

void Slider::OnUpdate(f64 dt) {
  if (HasState(state_, WidgetState::kPressed) && context_ && context_->platform) {
    f32 mouse_x = InputToLayoutPoint(context_->platform->input_queue().mouse_pos).x;
    f32 track_x = content_rect_.x;
    f32 track_w = content_rect_.w;

    if (track_w > 0.0f) {
      f32 t = Clamp((mouse_x - track_x) / track_w, 0.0f, 1.0f);
      f32 new_value = min_ + t * (max_ - min_);
      if (new_value != value_) {
        value_ = new_value;
        MarkPaintDirty();
        if (on_change_)
          on_change_(value_);
      }
    }
  } else if (dragging_) {
    dragging_ = false;
  }
}

void Slider::Measure(f32& out_width, f32& out_height) {
  out_width = 200.0f + style_.padding.horizontal();
  out_height = 24.0f + style_.padding.vertical();
}

void Slider::OnPaint(Renderer2D& renderer) {
  // Skip Widget::OnPaint - we draw our own custom visuals.
  auto s = ComputedStyle();
  f32 alpha = s.opacity;

  f32 range = max_ - min_;
  f32 normalized = (range > 0.0f) ? Clamp((value_ - min_) / range, 0.0f, 1.0f) : 0.0f;

  constexpr f32 kTrackHeight = 4.0f;
  constexpr f32 kTrackRadius = 2.0f;
  constexpr f32 kThumbSize = 16.0f;
  constexpr f32 kThumbRadius = 8.0f;

  f32 track_x = content_rect_.x;
  f32 track_w = content_rect_.w;
  f32 track_y = content_rect_.y + (content_rect_.h - kTrackHeight) * 0.5f;
  u32 track_radii = Vertex2D::PackRadii(kTrackRadius);

  // Use style colors: border_color for track, background for fill, text_color for thumb
  Color track_color = s.border_color.a > 0.0f
                          ? s.border_color.WithAlpha(s.border_color.a * alpha)
                          : Color(0.16f, 0.16f, 0.16f, alpha);
  Color fill_color = s.background.a > 0.0f
                         ? s.background.WithAlpha(s.background.a * alpha)
                         : Color(0.29f, 0.48f, 1.0f, alpha);
  Color thumb_color = s.text_color.WithAlpha(s.text_color.a * alpha);

  renderer.DrawRect({track_x, track_y, track_w, kTrackHeight}, track_color, track_radii);

  f32 fill_w = normalized * track_w;
  if (fill_w > 0.0f)
    renderer.DrawRect({track_x, track_y, fill_w, kTrackHeight}, fill_color, track_radii);

  f32 thumb_x = track_x + fill_w - kThumbSize * 0.5f;
  f32 thumb_y = content_rect_.y + (content_rect_.h - kThumbSize) * 0.5f;
  u32 thumb_radii = Vertex2D::PackRadii(kThumbRadius);
  renderer.DrawRect({thumb_x, thumb_y, kThumbSize, kThumbSize}, thumb_color, thumb_radii);
}

}  // namespace ugui
