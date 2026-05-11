#include <ugui/ui_context.h>
#include <ugui/widgets/modal.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/widget_registry.h>

#include <utility>

namespace ugui {
namespace {

// Resolve the ModalContent for any widget that carries one (a kModal, or a
// kMessageBox that composes a modal). Gates on the component, not the kind.
ModalContent* content(Widget* w) {
  if (!w || !w->registry()) return nullptr;
  return w->registry()->Get<ModalContent>(w->handle());
}

}  // namespace

Widget* CreateModal(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kModal);
  WidgetRegistry::Active()->Add<ModalContent>(w->handle(), ModalContent{});
  return w;
}

void ShowModal(Widget* w, UIContext* ctx) {
  ModalContent* c = content(w);
  if (!c || !ctx || c->visible) return;
  c->visible = true;

  // Full-viewport backdrop panel in the backdrop color.
  c->backdrop = CreatePanel(0);
  c->backdrop->set_name("_modal_backdrop");
  Style bs;
  bs.background = c->backdrop_color;
  bs.width = Length::Vw(100);
  bs.height = Length::Vh(100);
  c->backdrop->set_style(bs);

  // Show backdrop first, then the modal content on top.
  ctx->ShowOverlay(c->backdrop, {0, 0});
  ctx->ShowOverlay(w, {0, 0});
}

void HideModal(Widget* w, UIContext* ctx) {
  ModalContent* c = content(w);
  if (!c || !ctx || !c->visible) return;
  c->visible = false;

  ctx->HideOverlay(w);
  if (c->backdrop) {
    ctx->HideOverlay(c->backdrop);
    delete c->backdrop;
    c->backdrop = nullptr;
  }
  if (c->on_dismiss) c->on_dismiss();
}

void SetModalBackdropColor(Widget* w, Color c) {
  if (ModalContent* mc = content(w)) mc->backdrop_color = c;
}

void SetModalDismiss(Widget* w, Function<void()> handler) {
  if (ModalContent* mc = content(w)) mc->on_dismiss = std::move(handler);
}

bool ModalVisible(const Widget* w) {
  ModalContent* c = content(const_cast<Widget*>(w));
  return c && c->visible;
}

}  // namespace ugui
