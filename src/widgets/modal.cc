#include <ultragui/render/renderer2d.h>
#include <ultragui/ui_context.h>
#include <ultragui/widgets/modal.h>
#include <ultragui/widgets/panel.h>

namespace ugui {

void Modal::Show(UIContext* ctx) {
  if (visible_) return;
  visible_ = true;

  // Create a full-viewport backdrop panel with the backdrop color.
  backdrop_ = new Panel(0);
  backdrop_->set_name("_modal_backdrop");
  Style bs;
  bs.background = backdrop_color_;
  bs.width = Length::Vw(100);
  bs.height = Length::Vh(100);
  backdrop_->set_style(bs);

  // Show backdrop first, then the modal content on top.
  ctx->ShowOverlay(backdrop_, {0, 0});
  ctx->ShowOverlay(this, {0, 0});
}

void Modal::Hide(UIContext* ctx) {
  if (!visible_) return;
  visible_ = false;

  ctx->HideOverlay(this);
  if (backdrop_) {
    ctx->HideOverlay(backdrop_);
    delete backdrop_;
    backdrop_ = nullptr;
  }
  if (on_dismiss_) on_dismiss_();
}

void Modal::OnPaint(Renderer2D& renderer) {
  // Draw the modal panel background, border, shadow via base Widget paint.
  Widget::OnPaint(renderer);
  // Children are painted by the tree walker (UIContext traversal).
}

void Modal::Measure(f32& out_width, f32& out_height) {
  out_width = intrinsic_w_;
  out_height = intrinsic_h_;
}

}  // namespace ugui
