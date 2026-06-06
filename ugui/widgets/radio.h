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
UGUI_API WidgetVTable RadioVTable();

/// Create a radio entity: a generic widget tagged kRadio with a RadioContent
/// component.
UGUI_API wid CreateRadio(u32 id);

/// Set the radio label.
UGUI_API void SetRadioLabel(wid e, const String& label);

/// Set the radio group name (groups are mutually exclusive).
UGUI_API void SetRadioGroup(wid e, const String& group);

/// Set the radio on_change handler (run with true when the user selects it).
UGUI_API void SetRadioChange(wid e, Function<void(bool)> handler);

/// Set the selected state (toggles the kChecked state bit). Does not fire
/// on_change and does not deselect siblings (that happens only on a user
/// click). Works on any widget.
UGUI_API void SetRadioSelected(wid e, bool selected);

/// Whether the widget's kChecked state bit is set.
UGUI_API bool IsRadioSelected(wid e);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_RADIO_H_
