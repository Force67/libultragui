#ifndef ULTRAGUI_WIDGETS_DROPDOWN_H_
#define ULTRAGUI_WIDGETS_DROPDOWN_H_

#include <ultragui/widgets/widget.h>

namespace ugui {

/// Interactive dropdown/select widget. Displays the currently selected option
/// and expands inline to show a list of options when clicked.
class Dropdown : public Widget {
 public:
  using Widget::Widget;

  void set_options(const Vector<String>& opts) {
    options_ = opts;
    MarkDirty();
  }
  const Vector<String>& options() const { return options_; }

  i32 selected_index() const { return selected_; }
  void set_selected_index(i32 idx);

  String selected_text() const {
    return (selected_ >= 0 && selected_ < static_cast<i32>(options_.size()))
               ? options_[selected_]
               : "";
  }

  bool open() const { return open_; }

  void set_font(FontHandle font) { font_override_ = font; }
  FontHandle font() const { return font_override_; }

  using ChangeHandler = Function<void(i32, const String&)>;
  void set_on_change(ChangeHandler handler) { on_change_ = std::move(handler); }

  bool OnClick() override;
  Widget* HitTest(Vec2 point) override;
  void Measure(f32& out_width, f32& out_height) override;
  void OnPaint(Renderer2D& renderer) override;
  void OnUpdate(f64 dt) override;

 private:
  TextEngine* text_engine() const {
    return context_ ? context_->text_engine : nullptr;
  }
  FontHandle effective_font() const {
    if (font_override_ != kInvalidFont) return font_override_;
    return context_ ? context_->default_font : kInvalidFont;
  }

  Vector<String> options_;
  i32 selected_ = -1;
  bool open_ = false;
  i32 hover_index_ = -1;
  FontHandle font_override_ = kInvalidFont;
  ChangeHandler on_change_;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_DROPDOWN_H_
