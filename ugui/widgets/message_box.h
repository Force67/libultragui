#ifndef ULTRAGUI_WIDGETS_MESSAGE_BOX_H_
#define ULTRAGUI_WIDGETS_MESSAGE_BOX_H_

#include <ugui/widgets/modal.h>

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

/// Pre-built modal dialog with title, message, and action buttons.
/// Sits on top of all other UI via the overlay system.
///
/// Usage:
///   auto* mb = new MessageBox(0);
///   mb->Setup("Confirm", "Delete this entity?", MessageBoxButtons::kYesNo);
///   mb->set_on_result([](MessageBoxResult r) { if (r ==
///   MessageBoxResult::kYes) ... }); mb->Show(ctx);
class MessageBox : public Modal {
 public:
  using Modal::Modal;
  ~MessageBox() override;

  /// Configure the message box content and buttons.
  /// Call before Show().
  void Setup(const char* title, const char* message,
             MessageBoxButtons buttons = MessageBoxButtons::kOk);

  using ResultHandler = Function<void(MessageBoxResult)>;
  void set_on_result(ResultHandler handler) { on_result_ = std::move(handler); }

  /// Show the message box centered in the viewport.
  void Show(UIContext* ctx) override;

  /// Dismiss with a specific result.
  void Dismiss(UIContext* ctx, MessageBoxResult result);

  bool OnKeyDown(i32 key, i32 mods) override;

 private:
  ResultHandler on_result_;
  MessageBoxButtons buttons_ = MessageBoxButtons::kOk;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_MESSAGE_BOX_H_
