#ifndef ULTRAGUI_WIDGETS_TEXT_INPUT_H_
#define ULTRAGUI_WIDGETS_TEXT_INPUT_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Data for an editable single-line text input (WidgetKind::kTextInput). Holds
/// the text value, placeholder, caret/selection state, horizontal scroll, the
/// caret-blink timer and the edit callbacks. The focused flag lives in the
/// widget's state bit (WidgetState::kFocused), not here. Behaviour is in
/// TextInputVTable().
struct TextInputContent {
  using ChangeHandler = Function<void(const String&)>;
  using SubmitHandler = Function<void(const String&)>;
  using CancelHandler = Function<void()>;
  using HistoryHandler = Function<String()>;

  String text;
  String placeholder;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
  u32 cursor = 0;
  u32 sel_start = 0;
  u32 sel_end = 0;
  f64 blink_timer = 0.0;
  bool cursor_visible = true;
  f32 scroll_x = 0.0f;
  ChangeHandler on_change;
  SubmitHandler on_submit;
  CancelHandler on_cancel;
  HistoryHandler on_history_prev;
  HistoryHandler on_history_next;
};

/// Behaviour table (draw, measure, click, key/char input, blink, text capture)
/// for text input widgets.
WidgetVTable TextInputVTable();

/// Create a text input entity: a generic Widget tagged kTextInput with a
/// TextInputContent component.
Widget* CreateTextInput(u32 id);

/// Set the text value (caret moves to the end, selection collapses). No-op if
/// `w` is null or not a text input. Replacement for the old TextInput::set_text.
void SetTextInputValue(Widget* w, const String& text);

/// The current text value, or an empty string if `w` is null or not a text
/// input. Replacement for the old TextInput::text().
String TextInputValue(const Widget* w);

/// Set the placeholder shown when the text is empty. No-op if `w` is null or not
/// a text input.
void SetTextInputPlaceholder(Widget* w, const String& placeholder);

/// Override the font (kInvalidFont -> context default). No-op if `w` is null or
/// not a text input.
void SetTextInputFont(Widget* w, FontHandle font);

/// Fired on every edit with the new text.
void SetTextInputChange(Widget* w, TextInputContent::ChangeHandler handler);

/// Fired when the user presses Enter / Keypad Enter, with the current text.
void SetTextInputSubmit(Widget* w, TextInputContent::SubmitHandler handler);

/// Fired when the user presses Escape.
void SetTextInputCancel(Widget* w, TextInputContent::CancelHandler handler);

/// Fired on Up / Down; the handler returns the replacement text.
void SetTextInputHistoryPrev(Widget* w,
                             TextInputContent::HistoryHandler handler);
void SetTextInputHistoryNext(Widget* w,
                             TextInputContent::HistoryHandler handler);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_TEXT_INPUT_H_
