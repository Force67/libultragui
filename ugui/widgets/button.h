#ifndef ULTRAGUI_WIDGETS_BUTTON_H_
#define ULTRAGUI_WIDGETS_BUTTON_H_

#include <ugui/widgets/widget.h>

namespace ugui {

/// Interactive button widget with text label.
class Button : public Widget {
 public:
  static constexpr WidgetKind kKind = WidgetKind::kButton;
  WidgetKind kind() const override { return kKind; }

 public:
  using Widget::Widget;

  void set_label(const String& label) {
    label_ = label;
    MarkDirty();
  }
  const String& label() const { return label_; }

  void set_font(FontHandle font) { font_override_ = font; }
  FontHandle font() const { return font_override_; }

  using ClickHandler = Function<void()>;
  void set_on_click(ClickHandler handler) {
    on_click_handler_ = std::move(handler);
  }

  void Click() {
    if (on_click_handler_) on_click_handler_();
  }

  bool OnClick() override {
    if (on_click_handler_) {
      on_click_handler_();
      return true;
    }
    return false;
  }

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

  String label_;
  FontHandle font_override_ = kInvalidFont;
  ClickHandler on_click_handler_;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_BUTTON_H_
