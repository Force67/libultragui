#ifndef ULTRAGUI_WIDGETS_RICH_TEXT_H_
#define ULTRAGUI_WIDGETS_RICH_TEXT_H_

#include <ultragui/style/enums.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

/// A styled inline text span within a RichText widget.
struct TextSpan {
  String text;
  Color color = Color::White();
  f32 font_size = 0.0f;  // 0 = inherit from widget style
  FontWeight font_weight = FontWeight::kRegular;
  FontStyle font_style = FontStyle::kNormal;
  TextDecoration decoration = TextDecoration::kNone;
};

/// Rich text widget that renders multiple styled spans inline with wrapping.
class RichText : public Widget {
 public:
  using Widget::Widget;

  void set_spans(const Vector<TextSpan>& spans) {
    spans_ = spans;
    MarkDirty();
  }
  const Vector<TextSpan>& spans() const { return spans_; }

  void AddSpan(const TextSpan& span) {
    spans_.push_back(span);
    MarkDirty();
  }
  void ClearSpans() {
    spans_.clear();
    MarkDirty();
  }

  void set_font(FontHandle font) { font_override_ = font; }

  void Measure(f32& out_width, f32& out_height) override;
  void OnPaint(Renderer2D& renderer) override;

 private:
  TextEngine* text_engine() const {
    return context_ ? context_->text_engine : nullptr;
  }
  FontHandle effective_font() const {
    if (font_override_ != kInvalidFont) return font_override_;
    return context_ ? context_->default_font : kInvalidFont;
  }

  /// A shaped and positioned span ready for rendering.
  struct ShapedSpan {
    TextRun run;
    Color color;
    TextDecoration decoration;
    f32 font_size;  // For decoration thickness calculation
    f32 x;          // X position within the line
    f32 y;          // Y position (accumulated line offset)
  };

  /// Shape all spans and lay them out inline with wrapping.
  /// Returns total height of all lines.
  f32 LayoutSpans(Vector<ShapedSpan>& out, f32 max_width) const;

  Vector<TextSpan> spans_;
  FontHandle font_override_ = kInvalidFont;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_RICH_TEXT_H_
