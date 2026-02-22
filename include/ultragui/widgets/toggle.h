#ifndef ULTRAGUI_WIDGETS_TOGGLE_H_
#define ULTRAGUI_WIDGETS_TOGGLE_H_

#include <ultragui/widgets/widget.h>

namespace ugui {

/// Interactive toggle/switch widget with smooth thumb animation.
class Toggle : public Widget {
 public:
  using Widget::Widget;

  bool on() const { return HasState(state_, WidgetState::kChecked); }
  void set_on(bool v);

  using ChangeHandler = Function<void(bool)>;
  void set_on_change(ChangeHandler handler) { on_change_ = std::move(handler); }

  bool OnClick() override;
  void Measure(f32& out_width, f32& out_height) override;
  void OnPaint(Renderer2D& renderer) override;
  void OnUpdate(f64 dt) override;

 private:
  f32 thumb_anim_ = 0.0f;  // 0 = off (left), 1 = on (right)
  ChangeHandler on_change_;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_TOGGLE_H_
