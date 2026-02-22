#ifndef ULTRAGUI_WIDGETS_TEXT_INPUT_H_
#define ULTRAGUI_WIDGETS_TEXT_INPUT_H_

#include <ultragui/widgets/widget.h>

namespace ugui {

/// Editable single-line text input widget.
class TextInput : public Widget {
public:
    using Widget::Widget;

    void set_text(const String& text);
    const String& text() const { return text_; }

    void set_placeholder(const String& ph) { placeholder_ = ph; }
    const String& placeholder() const { return placeholder_; }

    void set_font(FontHandle font) { font_override_ = font; }

    using ChangeHandler = Function<void(const String&)>;
    void set_on_change(ChangeHandler handler) { on_change_ = std::move(handler); }

    bool OnCharInput(u32 codepoint) override;
    bool OnKeyDown(i32 key, i32 mods) override;
    bool OnClick() override;
    void Measure(f32& out_width, f32& out_height) override;
    void OnPaint(Renderer2D& renderer) override;
    void OnUpdate(f64 dt) override;

private:
    TextEngine* text_engine() const {
        return context_ ? context_->text_engine : nullptr;
    }
    FontHandle effective_font() const {
        if (font_override_ != kInvalidFont)
            return font_override_;
        return context_ ? context_->default_font : kInvalidFont;
    }

    u32 NextPos(u32 pos) const;
    u32 PrevPos(u32 pos) const;
    f32 CursorXFromPos(u32 byte_pos, const TextRun& run) const;
    u32 PosFromX(f32 local_x, const TextRun& run) const;
    void DeleteSelection();
    void ResetBlink();

    String text_;
    String placeholder_;
    FontHandle font_override_ = kInvalidFont;
    u32 cursor_ = 0;
    u32 sel_start_ = 0;
    u32 sel_end_ = 0;
    f64 blink_timer_ = 0.0;
    bool cursor_visible_ = true;
    f32 scroll_x_ = 0.0f;
    ChangeHandler on_change_;
};

} // namespace ugui

#endif  // ULTRAGUI_WIDGETS_TEXT_INPUT_H_
