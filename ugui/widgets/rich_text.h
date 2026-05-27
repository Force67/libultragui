#ifndef ULTRAGUI_WIDGETS_RICH_TEXT_H_
#define ULTRAGUI_WIDGETS_RICH_TEXT_H_

#include <ugui/style/enums.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// A styled inline text span within a rich-text widget.
struct TextSpan {
  String text;
  Color color = Color::White();
  f32 font_size = 0.0f;  // 0 = inherit from widget style
  FontWeight font_weight = FontWeight::kRegular;
  FontStyle font_style = FontStyle::kNormal;
  TextDecoration decoration = TextDecoration::kNone;
};

/// Data for a rich-text widget (WidgetKind::kRichText): the inline spans and an
/// optional font override. Behaviour (inline layout with wrapping) lives in
/// RichTextVTable(); a rich-text widget is a generic widget carrying this
/// component.
struct RichTextContent {
  Vector<TextSpan> spans;
  FontHandle font = kInvalidFont;  // override; kInvalidFont -> context default
};

/// Behaviour table (draw + measure) for rich-text widgets.
WidgetVTable RichTextVTable();

/// Create a rich-text entity: a generic widget tagged kRichText with a
/// RichTextContent component.
wid CreateRichText(u32 id);

/// Replace all spans of a rich-text widget. No-op if `e` is not a rich-text
/// widget.
void SetRichTextSpans(wid e, const Vector<TextSpan>& spans);

/// Append a span to a rich-text widget. No-op if `e` is not a rich-text widget.
void AddRichTextSpan(wid e, const TextSpan& span);

/// Remove all spans from a rich-text widget. No-op if `e` is not a rich-text
/// widget.
void ClearRichTextSpans(wid e);

/// Set the font override used to shape every span. No-op if `e` is not a
/// rich-text widget.
void SetRichTextFont(wid e, FontHandle font);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_RICH_TEXT_H_
