#ifndef ULTRAGUI_WIDGETS_MODAL_H_
#define ULTRAGUI_WIDGETS_MODAL_H_

#include <ugui/core/color.h>
#include <ugui/widgets/widget.h>

namespace ugui {

class UIContext;

/// Data for a modal widget (WidgetKind::kModal): the backdrop overlay entity,
/// its color, the shown flag and an on_dismiss callback. A modal is a generic
/// widget entity carrying this component, not a subclass. Its lifecycle
/// (show/hide the backdrop + content overlays) lives in the free functions
/// below; the base widget handles paint and measure. Other widgets (e.g. a
/// message box) can also carry a ModalContent to reuse the modal lifecycle by
/// composition.
struct ModalContent {
  Color backdrop_color = {0.0f, 0.0f, 0.0f, 0.5f};
  wid backdrop;
  bool visible = false;
  Function<void()> on_dismiss;
};

/// Create a modal entity: a generic widget tagged kModal with a ModalContent
/// component. No vtable: paint and measure use the base widget.
UGUI_API wid CreateModal(u32 id);

/// Show the modal: creates a full-viewport backdrop overlay and shows the
/// widget on top of it. Works on any widget carrying a ModalContent.
UGUI_API void ShowModal(wid e, UIContext* ctx);

/// Hide the modal: removes the widget and backdrop from the overlay system,
/// destroys the backdrop, and fires on_dismiss.
UGUI_API void HideModal(wid e, UIContext* ctx);

/// Set the backdrop color. No-op if `e` has no ModalContent.
UGUI_API void SetModalBackdropColor(wid e, Color c);

/// Set the on_dismiss handler (run from HideModal). No-op if `e` has no
/// ModalContent.
UGUI_API void SetModalDismiss(wid e, Function<void()> handler);

/// Whether the modal is currently shown.
UGUI_API bool ModalVisible(wid e);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_MODAL_H_
