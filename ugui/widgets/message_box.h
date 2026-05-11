#ifndef ULTRAGUI_WIDGETS_MESSAGE_BOX_H_
#define ULTRAGUI_WIDGETS_MESSAGE_BOX_H_

#include <ugui/widgets/modal.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

class UIContext;

/// Result returned by a MessageBox when the user clicks a button.
enum class MessageBoxResult : u8 {
  kOk,
  kCancel,
  kYes,
  kNo,
  kDismissed,  // Closed via ESC or click-outside
};

/// Button configuration for a MessageBox.
enum class MessageBoxButtons : u8 {
  kOk,
  kOkCancel,
  kYesNo,
  kYesNoCancel,
};

/// Data for a message-box widget (WidgetKind::kMessageBox): the result callback
/// and which buttons it shows. A message box composes a modal: the same widget
/// also carries a ModalContent component (for the backdrop), so it reuses
/// ShowModal/HideModal instead of inheriting from Modal. Behaviour (ESC ->
/// dismiss) lives in MessageBoxVTable().
struct MessageBoxContent {
  Function<void(MessageBoxResult)> on_result;
  MessageBoxButtons buttons = MessageBoxButtons::kOk;
};

/// Behaviour table (on_key_down) for message-box widgets.
WidgetVTable MessageBoxVTable();

/// Create a message-box entity: a generic Widget tagged kMessageBox carrying
/// both a ModalContent (for the backdrop) and a MessageBoxContent.
///
/// Usage:
///   auto* mb = CreateMessageBox(0);
///   SetupMessageBox(mb, "Confirm", "Delete this entity?",
///                   MessageBoxButtons::kYesNo);
///   SetMessageBoxResult(mb, [](MessageBoxResult r) { ... });
///   ShowMessageBox(mb, ctx);
Widget* CreateMessageBox(u32 id);

/// Configure the message-box style, title, message and buttons. Call before
/// ShowMessageBox(). No-op if `w` is null or not a message box.
void SetupMessageBox(Widget* w, const char* title, const char* message,
                     MessageBoxButtons buttons = MessageBoxButtons::kOk);

/// Set the result handler (run when a button is clicked or on ESC).
void SetMessageBoxResult(Widget* w, Function<void(MessageBoxResult)> handler);

/// Show the message box centered in the viewport, then show it as a modal.
void ShowMessageBox(Widget* w, UIContext* ctx);

/// Dismiss with a specific result: fires on_result(result) then hides the
/// modal.
void DismissMessageBox(Widget* w, UIContext* ctx, MessageBoxResult result);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_MESSAGE_BOX_H_
