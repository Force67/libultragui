#pragma once

#include <ultragui/text/text_engine.h>
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

    void set_font(FontHandle font) { font_ = font; }
    FontHandle font() const { return font_; }

    void measure(f32& out_width, f32& out_height) override;
    void on_paint(Renderer2D& renderer) override;

    /// Must be set before measure/paint
    void set_text_engine(TextEngine* engine) { text_engine_ = engine; }

private:
    std::string text_;
    FontHandle font_ = INVALID_FONT;
    TextEngine* text_engine_ = nullptr;
};

} // namespace ugui
