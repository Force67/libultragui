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

/// Create a checkbox entity: a generic Widget tagged kCheckbox with a
/// CheckboxContent component.
Widget* CreateCheckbox(u32 id);

/// Set the checkbox label. No-op if `w` is null or not a checkbox.
void SetCheckboxLabel(Widget* w, const String& label);

/// Set the checkbox on_change handler (run with the new checked value when the
/// user toggles it). No-op if `w` is null or not a checkbox.
void SetCheckboxChange(Widget* w, Function<void(bool)> handler);

/// Set the checked state (toggles the kChecked state bit). Does not fire
/// on_change (that fires only on a user click). Works on any widget.
void SetChecked(Widget* w, bool checked);

/// Whether the widget's kChecked state bit is set.
bool IsChecked(const Widget* w);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_CHECKBOX_H_
