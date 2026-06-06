#ifndef ULTRAGUI_WIDGETS_BUTTON_H_
#define ULTRAGUI_WIDGETS_BUTTON_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for a button widget (WidgetKind::kButton): its centered label and an
/// optional click handler. Behaviour lives in ButtonVTable(); a button is a
/// generic widget entity carrying this component, not a subclass.
struct ButtonContent {
  String label;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
  Function<void()> on_click;
};

/// Behaviour table (draw + measure + click) for button widgets.
UGUI_API WidgetVTable ButtonVTable();

/// Create a button entity in the active registry: a widget tagged kButton with
/// a ButtonContent component. Returns the new entity handle.
UGUI_API wid CreateButton(u32 id);

/// Set the button label. No-op if `e` is not a button.
UGUI_API void SetButtonLabel(wid e, const String& label);

/// Set the button click handler (run when the button is clicked). No-op if `e`
/// is not a button.
UGUI_API void SetButtonClick(wid e, Function<void()> handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_BUTTON_H_
