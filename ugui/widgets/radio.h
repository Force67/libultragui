#ifndef ULTRAGUI_WIDGETS_RADIO_H_
#define ULTRAGUI_WIDGETS_RADIO_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a radio button widget (WidgetKind::kRadio): label, font, group name
/// and an on_change callback. The selected flag lives in the widget's state bit
/// (WidgetState::kChecked), not here. Radios with the same group are mutually
/// exclusive. Behaviour is in RadioVTable().
struct RadioContent {
  String label;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
  String group;
  Function<void(bool)> on_change;
};

/// Behaviour table (draw + measure + click) for radio widgets.
WidgetVTable RadioVTable();

/// Create a radio entity: a generic Widget tagged kRadio with a RadioContent
/// component.
Widget* CreateRadio(u32 id);

/// Set the radio label. No-op if `w` is null or not a radio.
void SetRadioLabel(Widget* w, const String& label);

/// Set the radio group name (groups are mutually exclusive). No-op if `w` is
/// null or not a radio.
void SetRadioGroup(Widget* w, const String& group);

/// Set the radio on_change handler (run with true when the user selects it).
/// No-op if `w` is null or not a radio.
void SetRadioChange(Widget* w, Function<void(bool)> handler);

/// Set the selected state (toggles the kChecked state bit). Does not fire
/// on_change and does not deselect siblings (that happens only on a user
/// click). Works on any widget.
void SetRadioSelected(Widget* w, bool selected);

/// Whether the widget's kChecked state bit is set.
bool IsRadioSelected(const Widget* w);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_RADIO_H_
