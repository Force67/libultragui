#ifndef ULTRAGUI_WIDGETS_SLIDER_H_
#define ULTRAGUI_WIDGETS_SLIDER_H_

#include <ultragui/widgets/widget.h>

#include <functional>

namespace ugui {

/// Horizontal slider widget for selecting a numeric value within a range.
/// Supports mouse drag interaction and an on-change callback.
class Slider : public Widget {
 public:
  using Widget::Widget;

  f32 value() const { return value_; }
  void set_value(f32 v);

  f32 min() const { return min_; }
  void set_min(f32 v) { min_ = v; }

  f32 max() const { return max_; }
  void set_max(f32 v) { max_ = v; }

  using ChangeHandler = std::function<void(f32)>;
  void set_on_change(ChangeHandler handler) { on_change_ = std::move(handler); }

  bool OnClick() override;
  void Measure(f32& out_width, f32& out_height) override;
  void OnPaint(Renderer2D& renderer) override;
  void OnUpdate(f64 dt) override;

 private:
  f32 value_ = 0.0f;
  f32 min_ = 0.0f;
  f32 max_ = 1.0f;
  bool dragging_ = false;
  ChangeHandler on_change_;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_SLIDER_H_
