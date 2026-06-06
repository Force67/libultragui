#ifndef ULTRAGUI_WIDGETS_TEXT_H_
#define ULTRAGUI_WIDGETS_TEXT_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Text shown by a text widget (WidgetKind::kText). Its behaviour lives in
/// TextVTable(); a text widget is a generic widget entity carrying this
/// component.
struct TextContent {
  String text;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
};

/// Behaviour table (draw + measure) for text widgets.
UGUI_API WidgetVTable TextVTable();

/// Create a text entity: a widget entity tagged kText with a TextContent.
UGUI_API wid CreateText(u32 id);

/// Set the string a text widget displays. No-op if `e` is not a text widget.
UGUI_API void SetText(wid e, const String& text);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_TEXT_H_
