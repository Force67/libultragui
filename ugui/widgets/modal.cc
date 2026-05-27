#include <ugui/ui_context.h>
#include <ugui/widgets/modal.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

#include <utility>

namespace ugui {
namespace {

// Resolve the ModalContent for any widget that carries one (a kModal, or a
// kMessageBox that composes a modal). Gates on the component, not the kind.
ModalContent* content(WidgetRegistry& world, wid e) {
  return world.Get<ModalContent>(e);
}

}  // namespace

wid CreateModal(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kModal;
  world.Add<ModalContent>(e, ModalContent{});
  return e;
}

void ShowModal(wid e, UIContext* ctx) {
  if (!ctx) return;
  WidgetRegistry& world = ctx->world();
  ModalContent* c = content(world, e);
  if (!c || c->visible) return;
  c->visible = true;

  // Full-viewport backdrop panel in the backdrop color.
  wid b = CreatePanel(0);
  Style bs;
  bs.background = c->backdrop_color;
  bs.width = Length::Vw(100);
  bs.height = Length::Vh(100);
  SetStyle(world, b, bs);
  c = content(world, e);  // re-resolve: CreatePanel may have grown the store
  c->backdrop = b;

  // Show backdrop first, then the modal content on top.
  ctx->ShowOverlay(b, {0, 0});
  ctx->ShowOverlay(e, {0, 0});
}

void HideModal(wid e, UIContext* ctx) {
  if (!ctx) return;
  WidgetRegistry& world = ctx->world();
  ModalContent* c = content(world, e);
  if (!c || !c->visible) return;
  c->visible = false;

  ctx->HideOverlay(e);
  wid backdrop = c->backdrop;
  if (backdrop.valid()) {
    ctx->HideOverlay(backdrop);
    DestroyWidget(world, backdrop);
    if (ModalContent* mc = content(world, e)) mc->backdrop = kNullWidget;
  }
  if (ModalContent* mc = content(world, e); mc && mc->on_dismiss)
    mc->on_dismiss();
}

void SetModalBackdropColor(wid e, Color c) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (ModalContent* mc = content(world, e)) mc->backdrop_color = c;
}

void SetModalDismiss(wid e, Function<void()> handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (ModalContent* mc = content(world, e)) mc->on_dismiss = std::move(handler);
}

bool ModalVisible(wid e) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  ModalContent* c = content(world, e);
  return c && c->visible;
}

}  // namespace ugui
