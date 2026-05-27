#ifndef ULTRAGUI_WIDGETS_DROPDOWN_H_
#define ULTRAGUI_WIDGETS_DROPDOWN_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a dropdown/select widget (WidgetKind::kDropdown): the option list,
/// current selection, open/hover state, font override and an on_change
/// callback. Behaviour lives in DropdownVTable(); a dropdown is a generic
/// widget entity carrying this component, not a subclass.
struct DropdownContent {
  Vector<String> options;
  i32 selected = -1;
  bool open = false;
  i32 hover_index = -1;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
  Function<void(i32, const String&)> on_change;
};

/// Behaviour table (draw + measure + hit-test + click + update) for dropdowns.
WidgetVTable DropdownVTable();

/// Create a dropdown entity: a generic widget tagged kDropdown with a
/// DropdownContent component.
wid CreateDropdown(u32 id);

/// Set the option list. No-op if `e` is not a dropdown.
void SetDropdownOptions(wid e, const Vector<String>& options);

/// Set the selected index, clamped to [0, options-1] (or -1 when empty). No-op
/// if `e` is not a dropdown.
void SetDropdownSelected(wid e, i32 index);

/// Currently selected index, or -1 if none / not a dropdown.
i32 DropdownSelected(wid e);

/// Text of the currently selected option, or "" if none / not a dropdown.
String DropdownSelectedText(wid e);

/// Set the on_change handler (run with the new index and text when the user
/// picks an option). No-op if `e` is not a dropdown.
void SetDropdownChange(wid e, Function<void(i32, const String&)> handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_DROPDOWN_H_
