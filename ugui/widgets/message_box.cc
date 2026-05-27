#include <ugui/platform/platform.h>
#include <ugui/ui_context.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/message_box.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

#include <utility>

namespace ugui {
namespace {

MessageBoxContent* content(WidgetRegistry& world, wid e) {
  WidgetNode* node = world.Get<WidgetNode>(e);
  if (!node || node->kind != WidgetKind::kMessageBox) return nullptr;
  return world.Get<MessageBoxContent>(e);
}

bool MessageBoxKeyDown(WidgetRegistry& world, wid e, i32 key, i32 /*mods*/) {
  if (key == 256) {  // GLFW_KEY_ESCAPE
    MessageBoxContent* c = world.Get<MessageBoxContent>(e);
    if (c && c->on_result) c->on_result(MessageBoxResult::kDismissed);
    return true;
  }
  return false;
}

}  // namespace

WidgetVTable MessageBoxVTable() {
  WidgetVTable vt;
  vt.on_key_down = MessageBoxKeyDown;
  return vt;
}

wid CreateMessageBox(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kMessageBox;
  world.Add<ModalContent>(e, ModalContent{});
  world.Add<MessageBoxContent>(e, MessageBoxContent{});
  return e;
}

void SetupMessageBox(wid e, const char* title, const char* message,
                     MessageBoxButtons buttons) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  MessageBoxContent* c = content(world, e);
  if (!c) return;
  c->buttons = buttons;

  // Style the message box itself as the dialog panel.
  Style ds;
  ds.flex_direction = FlexDirection::kColumn;
  ds.padding = EdgeInsets(24);
  ds.gap = 16;
  ds.width = Length::Px(400);
  ds.background = Color::FromHex(0x1e1e2e);
  ds.corner_radius = 8;
  ds.border_color = Color::FromRgba8(255, 255, 255, 30);
  ds.border_width = 1;
  ds.shadow = BoxShadow{
      .color = Color::FromRgba8(0, 0, 0, 128),
      .blur = 24,
      .spread = 4,
  };
  SetStyle(world, e, ds);

  // Title.
  wid t = CreateText(0);
  SetText(t, title);
  Style ts;
  ts.font_size = 18;
  ts.font_weight = FontWeight::kBold;
  ts.text_color = Color::White();
  SetStyle(world, t, ts);
  AddChild(world, e, t);

  // Message.
  wid m = CreateText(0);
  SetText(m, message);
  Style ms;
  ms.font_size = 14;
  ms.text_color = Color::FromRgba8(200, 200, 210, 255);
  SetStyle(world, m, ms);
  AddChild(world, e, m);

  // Button row.
  wid row = CreatePanel(0);
  Style rs;
  rs.flex_direction = FlexDirection::kRow;
  rs.justify_content = JustifyContent::kEnd;
  rs.gap = 8;
  rs.margin.top = 8;
  SetStyle(world, row, rs);

  wid self = e;
  auto MakeBtn = [&](const char* label, MessageBoxResult result, bool primary) {
    wid btn = CreateButton(0);
    SetButtonLabel(btn, label);
    world.Get<WidgetNode>(btn)->tab_index = 0;
    Style bs;
    bs.padding = EdgeInsets(10, 20);
    bs.corner_radius = 4;
    bs.font_size = 14;
    bs.cursor = Cursor::kPointer;
    if (primary) {
      bs.background = Color::FromHex(0x3b82f6);
      bs.text_color = Color::White();
    } else {
      bs.background = Color::FromRgba8(60, 60, 80, 255);
      bs.text_color = Color::FromRgba8(200, 200, 210, 255);
      bs.border_color = Color::FromRgba8(100, 100, 120, 255);
      bs.border_width = 1;
    }
    SetStyle(world, btn, bs);
    Style hover = bs;
    hover.background =
        primary ? Color::FromHex(0x60a5fa) : Color::FromRgba8(80, 80, 100, 255);
    AddStateOverride(world, btn, WidgetState::kHovered, hover,
                     StyleMask::kBackground);
    SetButtonClick(btn, [self, result]() {
      MessageBoxContent* mc =
          WidgetRegistry::Active()->Get<MessageBoxContent>(self);
      if (mc && mc->on_result) mc->on_result(result);
    });
    AddChild(world, row, btn);
  };

  switch (buttons) {
    case MessageBoxButtons::kOk:
      MakeBtn("OK", MessageBoxResult::kOk, true);
      break;
    case MessageBoxButtons::kOkCancel:
      MakeBtn("Cancel", MessageBoxResult::kCancel, false);
      MakeBtn("OK", MessageBoxResult::kOk, true);
      break;
    case MessageBoxButtons::kYesNo:
      MakeBtn("No", MessageBoxResult::kNo, false);
      MakeBtn("Yes", MessageBoxResult::kYes, true);
      break;
    case MessageBoxButtons::kYesNoCancel:
      MakeBtn("Cancel", MessageBoxResult::kCancel, false);
      MakeBtn("No", MessageBoxResult::kNo, false);
      MakeBtn("Yes", MessageBoxResult::kYes, true);
      break;
  }

  AddChild(world, e, row);
}

void SetMessageBoxResult(wid e, Function<void(MessageBoxResult)> handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (MessageBoxContent* c = content(world, e))
    c->on_result = std::move(handler);
}

void ShowMessageBox(wid e, UIContext* ctx) {
  if (!ctx) return;
  WidgetRegistry& world = ctx->world();
  if (!content(world, e)) return;

  // Center the dialog: measure, then offset via margin.
  f32 mw = 0, mh = 0;
  MeasureWidget(world, e, mw, mh);

  Vec2 vp = ctx->platform()->window_size();
  f32 left = (vp.x - mw) * 0.5f;
  f32 top = (vp.y - mh) * 0.5f;
  if (left < 0) left = 0;
  if (top < 0) top = 0;

  if (StyleC* sc = world.Get<StyleC>(e))
    sc->style.margin = EdgeInsets(top, 0, 0, left);

  ShowModal(e, ctx);
}

void DismissMessageBox(wid e, UIContext* ctx, MessageBoxResult result) {
  if (!ctx) return;
  WidgetRegistry& world = ctx->world();
  if (MessageBoxContent* c = content(world, e))
    if (c->on_result) c->on_result(result);
  HideModal(e, ctx);
}

}  // namespace ugui
