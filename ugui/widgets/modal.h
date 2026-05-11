#ifndef ULTRAGUI_WIDGETS_MODAL_H_
#define ULTRAGUI_WIDGETS_MODAL_H_

#include <ugui/core/color.h>
#include <ugui/widgets/widget.h>

namespace ugui {

class UIContext;

/// Data for a modal widget (WidgetKind::kModal): the backdrop overlay, its
/// color, the shown flag and an on_dismiss callback. A modal is a generic
/// Widget carrying this component, not a subclass. Its lifecycle (show/hide the
/// backdrop + content overlays) lives in the free functions below; the base
/// Widget handles paint and measure. Other widgets (e.g. a message box) can
/// also carry a ModalContent to reuse the modal lifecycle by composition.
struct ModalContent {
  Color backdrop_color = {0.0f, 0.0f, 0.0f, 0.5f};
  Widget* backdrop = nullptr;
  bool visible = false;
  Function<void()> on_dismiss;
};

/// Create a modal entity: a generic Widget tagged kModal with a ModalContent
/// component. No vtable: paint and measure use the base Widget.
Widget* CreateModal(u32 id);

/// Show the modal: creates a full-viewport backdrop overlay and shows the
/// widget on top of it. Works on any widget carrying a ModalContent.
void ShowModal(Widget* w, UIContext* ctx);

/// Hide the modal: removes the widget and backdrop from the overlay system,
/// deletes the backdrop, and fires on_dismiss.
void HideModal(Widget* w, UIContext* ctx);

/// Set the backdrop color. No-op if `w` has no ModalContent.
void SetModalBackdropColor(Widget* w, Color c);

/// Set the on_dismiss handler (run from HideModal). No-op if `w` has no
/// ModalContent.
void SetModalDismiss(Widget* w, Function<void()> handler);

/// Whether the modal is currently shown.
bool ModalVisible(const Widget* w);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_MODAL_H_
