#ifndef ULTRAGUI_WIDGETS_TEXT_H_
#define ULTRAGUI_WIDGETS_TEXT_H_

#include <ultragui/widgets/widget.h>

namespace ugui {

/// Text display widget.
class Text : public Widget {
public:
    using Widget::Widget;

    void set_text(const String& text) {
        text_ = text;
        MarkDirty();
    }
    const String& text() const { return text_; }

    void set_font(FontHandle font) { font_override_ = font; }
    FontHandle font() const { return font_override_; }

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

    String text_;
    FontHandle font_override_ = kInvalidFont;
};

} // namespace ugui

#endif  // ULTRAGUI_WIDGETS_TEXT_H_
