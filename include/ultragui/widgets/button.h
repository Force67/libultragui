#pragma once

#include <ultragui/text/text_engine.h>
#include <ultragui/widgets/widget.h>

#include <functional>
#include <string>

namespace ugui {

class TextEngine;

/// Interactive button widget with text label.
class Button : public Widget {
public:
    using Widget::Widget;

    void set_label(const std::string& label) {
        label_ = label;
        mark_dirty();
    }
    const std::string& label() const { return label_; }

    void set_font(FontHandle font) { font_ = font; }
    void set_text_engine(TextEngine* engine) { text_engine_ = engine; }

    using ClickHandler = std::function<void()>;
    void set_on_click(ClickHandler handler) { on_click_ = std::move(handler); }

    void click() {
        if (on_click_)
            on_click_();
    }

    void measure(f32& out_width, f32& out_height) override;
    void on_paint(Renderer2D& renderer) override;

private:
    std::string label_;
    FontHandle font_ = INVALID_FONT;
    TextEngine* text_engine_ = nullptr;
    ClickHandler on_click_;
};

} // namespace ugui
