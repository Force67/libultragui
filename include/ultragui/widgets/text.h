#pragma once

#include <ultragui/widgets/widget.h>

#include <string>

namespace ugui {

/// Text display widget.
class Text : public Widget {
public:
    using Widget::Widget;

    void set_text(const std::string& text) {
        text_ = text;
        mark_dirty();
    }
    const std::string& text() const { return text_; }

    void set_font(FontHandle font) { font_override_ = font; }
    FontHandle font() const { return font_override_; }

    void measure(f32& out_width, f32& out_height) override;
    void on_paint(Renderer2D& renderer) override;

private:
    TextEngine* text_engine() const {
        return context_ ? context_->text_engine : nullptr;
    }
    FontHandle effective_font() const {
        if (font_override_ != INVALID_FONT) return font_override_;
        return context_ ? context_->default_font : INVALID_FONT;
    }

    std::string text_;
    FontHandle font_override_ = INVALID_FONT;
};

} // namespace ugui
