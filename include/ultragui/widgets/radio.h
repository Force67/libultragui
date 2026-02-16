#ifndef ULTRAGUI_WIDGETS_RADIO_H_
#define ULTRAGUI_WIDGETS_RADIO_H_

#include <ultragui/widgets/widget.h>

#include <functional>
#include <string>

namespace ugui {

/// Interactive radio button widget with group-based mutual exclusion.
class Radio : public Widget {
 public:
  using Widget::Widget;

  bool selected() const { return HasState(state_, WidgetState::kChecked); }
  void set_selected(bool v);

  void set_group(const std::string& g) { group_ = g; }
  const std::string& group() const { return group_; }

  void set_label(const std::string& label) {
    label_ = label;
    MarkDirty();
  }
  const std::string& label() const { return label_; }

  void set_font(FontHandle font) { font_override_ = font; }
  FontHandle font() const { return font_override_; }

  using ChangeHandler = std::function<void(bool)>;
  void set_on_change(ChangeHandler handler) { on_change_ = std::move(handler); }

  bool OnClick() override;
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
  void DeselectSiblings();

  std::string group_;
  std::string label_;
  FontHandle font_override_ = kInvalidFont;
  ChangeHandler on_change_;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_RADIO_H_
