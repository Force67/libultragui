#ifndef ULTRAGUI_WIDGETS_DROPDOWN_H_
#define ULTRAGUI_WIDGETS_DROPDOWN_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a dropdown/select widget (WidgetKind::kDropdown): the option list,
/// current selection, open/hover state, font override and an on_change
/// callback. Behaviour lives in DropdownVTable(); a dropdown is a generic
/// Widget carrying this component, not a subclass.
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

/// Create a dropdown entity: a generic Widget tagged kDropdown with a
/// DropdownContent component.
Widget* CreateDropdown(u32 id);

/// Set the option list. No-op if `w` is null or not a dropdown.
void SetDropdownOptions(Widget* w, const Vector<String>& options);

/// Set the selected index, clamped to [0, options-1] (or -1 when empty). No-op
/// if `w` is null or not a dropdown.
void SetDropdownSelected(Widget* w, i32 index);

/// Currently selected index, or -1 if none / not a dropdown.
i32 DropdownSelected(const Widget* w);

/// Text of the currently selected option, or "" if none / not a dropdown.
String DropdownSelectedText(const Widget* w);

/// Set the on_change handler (run with the new index and text when the user
/// picks an option). No-op if `w` is null or not a dropdown.
void SetDropdownChange(Widget* w, Function<void(i32, const String&)> handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_DROPDOWN_H_
