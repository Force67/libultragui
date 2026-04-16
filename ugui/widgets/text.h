#ifndef ULTRAGUI_WIDGETS_TEXT_H_
#define ULTRAGUI_WIDGETS_TEXT_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Text shown by a text widget (WidgetKind::kText). Its behaviour lives in
/// TextVTable(); a text widget is a generic Widget carrying this component.
struct TextContent {
  String text;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
};

/// Behaviour table (draw + measure) for text widgets.
WidgetVTable TextVTable();

/// Create a text entity: a generic Widget tagged kText with a TextContent.
Widget* CreateText(u32 id);

/// Set the string a text widget displays. No-op if `w` is null or not a text
/// widget. Component-based replacement for the old Text::set_text.
void SetText(Widget* w, const String& text);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_TEXT_H_
