#ifndef ULTRAGUI_WIDGETS_CONTEXT_MENU_H_
#define ULTRAGUI_WIDGETS_CONTEXT_MENU_H_

#include <ultragui/widgets/widget.h>

namespace ugui {

class UIContext;

/// Right-click context menu widget. Displayed as an overlay at the cursor
/// position, dismissed on click outside or when an item is selected.
class ContextMenu : public Widget {
 public:
  using Widget::Widget;

  struct MenuItem {
    String label;
    Function<void()> action;
    bool separator = false;
  };

  void AddItem(const String& label, Function<void()> action);
  void AddSeparator();
  void ClearItems();

  void ShowAt(UIContext* ctx, Vec2 position);
  void Hide(UIContext* ctx);
  bool visible() const { return visible_; }

  const Vector<MenuItem>& items() const { return items_; }

  bool OnClick() override;
  void OnDismiss() override;
  void Measure(f32& out_width, f32& out_height) override;
  void OnPaint(Renderer2D& renderer) override;

 private:
  TextEngine* text_engine() const {
    return context_ ? context_->text_engine : nullptr;
  }
  FontHandle effective_font() const {
    return context_ ? context_->default_font : kInvalidFont;
  }

  Vector<MenuItem> items_;
  bool visible_ = false;
  i32 hover_index_ = -1;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_CONTEXT_MENU_H_
