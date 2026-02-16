#ifndef ULTRAGUI_WIDGETS_MODAL_H_
#define ULTRAGUI_WIDGETS_MODAL_H_

#include <ultragui/widgets/widget.h>

#include <functional>

namespace ugui {

class UIContext;

/// Modal/dialog widget. When shown, displays a semi-transparent backdrop
/// covering the entire viewport with the modal content centered on top.
/// Uses the UIContext overlay system for z-ordering.
class Modal : public Widget {
public:
    using Widget::Widget;

    bool visible() const { return visible_; }

    /// Show the modal: creates a backdrop overlay and shows this widget on top.
    void Show(UIContext* ctx);

    /// Hide the modal: removes both backdrop and this widget from the overlay system.
    void Hide(UIContext* ctx);

    void set_backdrop_color(Color c) { backdrop_color_ = c; }
    Color backdrop_color() const { return backdrop_color_; }

    using DismissHandler = std::function<void()>;
    void set_on_dismiss(DismissHandler handler) { on_dismiss_ = std::move(handler); }

    void OnPaint(Renderer2D& renderer) override;
    void Measure(f32& out_width, f32& out_height) override;

private:
    bool visible_ = false;
    Color backdrop_color_ = {0.0f, 0.0f, 0.0f, 0.5f};
    Widget* backdrop_ = nullptr;
    DismissHandler on_dismiss_;
};

} // namespace ugui

#endif  // ULTRAGUI_WIDGETS_MODAL_H_
