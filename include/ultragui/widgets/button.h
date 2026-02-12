#pragma once

#include <ultragui/widgets/widget.h>

#include <functional>
#include <string>

namespace ugui {

/// Interactive button widget with text label.
class Button : public Widget {
public:
    using Widget::Widget;

    void set_label(const std::string& label) {
        label_ = label;
        mark_dirty();
    }
    const std::string& label() const { return label_; }

    void set_font(FontHandle font) { font_override_ = font; }
    FontHandle font() const { return font_override_; }

    using ClickHandler = std::function<void()>;
    void set_on_click(ClickHandler handler) { on_click_handler_ = std::move(handler); }

    void click() {
        if (on_click_handler_)
            on_click_handler_();
    }

    bool on_click() override {
        if (on_click_handler_) {
            on_click_handler_();
            return true;
        }
        return false;
    }

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

    std::string label_;
    FontHandle font_override_ = INVALID_FONT;
    ClickHandler on_click_handler_;
};

} // namespace ugui
