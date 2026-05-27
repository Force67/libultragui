#ifndef ULTRAGUI_WIDGETS_CHECKBOX_H_
#define ULTRAGUI_WIDGETS_CHECKBOX_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a checkbox widget (WidgetKind::kCheckbox): label, font and an
/// on_change callback. The checked flag lives in the widget's state bit
/// (WidgetState::kChecked), not here. Behaviour is in CheckboxVTable().
struct CheckboxContent {
  String label;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
  Function<void(bool)> on_change;
};

/// Behaviour table (draw + measure + click) for checkbox widgets.
WidgetVTable CheckboxVTable();

/// Create a checkbox entity: a generic widget tagged kCheckbox with a
/// CheckboxContent component.
wid CreateCheckbox(u32 id);

/// Set the checkbox label. No-op if `e` is not a checkbox.
void SetCheckboxLabel(wid e, const String& label);

/// Set the checkbox on_change handler (run with the new checked value when the
/// user toggles it). No-op if `e` is not a checkbox.
void SetCheckboxChange(wid e, Function<void(bool)> handler);

/// Set the checked state (toggles the kChecked state bit). Does not fire
/// on_change (that fires only on a user click). Works on any widget.
void SetChecked(wid e, bool checked);

/// Whether the widget's kChecked state bit is set.
bool IsChecked(wid e);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_CHECKBOX_H_
