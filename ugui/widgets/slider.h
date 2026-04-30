#ifndef ULTRAGUI_WIDGETS_SLIDER_H_
#define ULTRAGUI_WIDGETS_SLIDER_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a horizontal slider widget (WidgetKind::kSlider): the current
/// value, its range and an on_change callback. `dragging` is transient
/// interaction state used while the user drags the thumb. Behaviour lives in
/// SliderVTable().
struct SliderContent {
  f32 value = 0.0f;
  f32 min = 0.0f;
  f32 max = 1.0f;
  bool dragging = false;
  Function<void(f32)> on_change;
};

/// Behaviour table (draw + measure + click + update) for slider widgets.
WidgetVTable SliderVTable();

/// Create a slider entity: a generic Widget tagged kSlider with a
/// SliderContent component.
Widget* CreateSlider(u32 id);

/// Set the slider value, clamped to [min, max]. Does not fire on_change (that
/// fires only on user interaction). No-op if `w` is null or not a slider.
void SetSliderValue(Widget* w, f32 value);

/// The slider's current value, or 0 if `w` is null or not a slider.
f32 SliderValue(const Widget* w);

/// The slider's range minimum / maximum, or 0 if `w` is null or not a slider.
f32 SliderMin(const Widget* w);
f32 SliderMax(const Widget* w);

/// Set the slider's range. No-op if `w` is null or not a slider.
void SetSliderMin(Widget* w, f32 min);
void SetSliderMax(Widget* w, f32 max);

/// Set the slider's on_change handler (run with the new value when the user
/// drags it). No-op if `w` is null or not a slider.
void SetSliderChange(Widget* w, Function<void(f32)> handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_SLIDER_H_
