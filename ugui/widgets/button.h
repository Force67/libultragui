#ifndef ULTRAGUI_WIDGETS_BUTTON_H_
#define ULTRAGUI_WIDGETS_BUTTON_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a button widget (WidgetKind::kButton): its centered label and an
/// optional click handler. Behaviour lives in ButtonVTable(); a button is a
/// generic Widget carrying this component, not a subclass.
struct ButtonContent {
  String label;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
  Function<void()> on_click;
};

/// Behaviour table (draw + measure + click) for button widgets.
WidgetVTable ButtonVTable();

/// Create a button entity: a generic Widget tagged kButton with a
/// ButtonContent component.
Widget* CreateButton(u32 id);

/// Set the button label. No-op if `w` is null or not a button.
void SetButtonLabel(Widget* w, const String& label);

/// Set the button click handler (run when the button is clicked). No-op if `w`
/// is null or not a button.
void SetButtonClick(Widget* w, Function<void()> handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_BUTTON_H_
