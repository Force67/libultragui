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
UGUI_API WidgetVTable SliderVTable();

/// Create a slider entity: a generic widget tagged kSlider with a
/// SliderContent component.
UGUI_API wid CreateSlider(u32 id);

/// Set the slider value, clamped to [min, max]. Does not fire on_change (that
/// fires only on user interaction). No-op if `e` is not a slider.
UGUI_API void SetSliderValue(wid e, f32 value);

/// The slider's current value, or 0 if `e` is not a slider.
UGUI_API f32 SliderValue(wid e);

/// The slider's range minimum / maximum, or 0 if `e` is not a slider.
UGUI_API f32 SliderMin(wid e);
UGUI_API f32 SliderMax(wid e);

/// Set the slider's range. No-op if `e` is not a slider.
UGUI_API void SetSliderMin(wid e, f32 min);
UGUI_API void SetSliderMax(wid e, f32 max);

/// Set the slider's on_change handler (run with the new value when the user
/// drags it). No-op if `e` is not a slider.
UGUI_API void SetSliderChange(wid e, Function<void(f32)> handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_SLIDER_H_
