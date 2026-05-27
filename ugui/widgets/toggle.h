#ifndef ULTRAGUI_WIDGETS_TOGGLE_H_
#define ULTRAGUI_WIDGETS_TOGGLE_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a toggle/switch widget (WidgetKind::kToggle): an on_change callback
/// and the thumb slide animation progress. The on/off flag lives in the
/// widget's state bit (WidgetState::kChecked), not here. Behaviour is in
/// ToggleVTable().
struct ToggleContent {
  Function<void(bool)> on_change;
  f32 thumb_anim = 0.0f;  // 0 = off (left), 1 = on (right)
};

/// Behaviour table (draw + measure + click + update) for toggle widgets.
WidgetVTable ToggleVTable();

/// Create a toggle entity: a generic widget tagged kToggle with a
/// ToggleContent component.
wid CreateToggle(u32 id);

/// Set the on/off state (toggles the kChecked state bit) and snaps the thumb
/// animation so external state restoration shows the correct visual
/// immediately. Does not fire on_change (that fires only on a user click).
/// No-op if `e` is not a toggle.
void SetToggleOn(wid e, bool on);

/// Whether the widget's kChecked state bit is set.
bool IsToggleOn(wid e);

/// Set the toggle on_change handler (run with the new on/off value when the
/// user toggles it). No-op if `e` is not a toggle.
void SetToggleChange(wid e, Function<void(bool)> handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_TOGGLE_H_
