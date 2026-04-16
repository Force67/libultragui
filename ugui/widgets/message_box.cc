#include <ugui/platform/platform.h>
#include <ugui/ui_context.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/message_box.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/text.h>

namespace ugui {

MessageBox::~MessageBox() = default;

void MessageBox::Setup(const char* title, const char* message,
                       MessageBoxButtons buttons) {
  buttons_ = buttons;

  // Style the MessageBox itself as the dialog panel.
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
  set_style(ds);

  // Title
  auto* titleWidget = CreateText(0);
  SetText(titleWidget, title);
  Style ts;
  ts.font_size = 18;
  ts.font_weight = FontWeight::kBold;
  ts.text_color = Color::White();
  titleWidget->set_style(ts);
  AddChild(titleWidget);

  // Message
  auto* messageWidget = CreateText(0);
  SetText(messageWidget, message);
  Style ms;
  ms.font_size = 14;
  ms.text_color = Color::FromRgba8(200, 200, 210, 255);
  messageWidget->set_style(ms);
  AddChild(messageWidget);

  // Button row
  auto* buttonRow = new Panel(0);
  Style rs;
  rs.flex_direction = FlexDirection::kRow;
  rs.justify_content = JustifyContent::kEnd;
  rs.gap = 8;
  rs.margin.top = 8;
  buttonRow->set_style(rs);

  auto MakeBtn = [&](const char* label, MessageBoxResult result, bool primary) {
    auto* btn = new Button(0);
    btn->set_label(label);
    btn->set_tab_index(0);
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
    btn->set_style(bs);
    Style hover = bs;
    hover.background =
        primary ? Color::FromHex(0x60a5fa) : Color::FromRgba8(80, 80, 100, 255);
    btn->AddStateOverride(WidgetState::kHovered, hover, StyleMask::kBackground);
    btn->set_on_click([this, result]() {
      if (on_result_) on_result_(result);
    });
    buttonRow->AddChild(btn);
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

  AddChild(buttonRow);
}

void MessageBox::Show(UIContext* ctx) {
  // Center the dialog on screen via margin (same pattern as ContextMenu).
  // Measure to get intrinsic size, then offset.
  f32 mw = 0, mh = 0;
  Measure(mw, mh);

  Vec2 vp = ctx->platform()->window_size();
  f32 left = (vp.x - mw) * 0.5f;
  f32 top = (vp.y - mh) * 0.5f;
  if (left < 0) left = 0;
  if (top < 0) top = 0;

  Style s = style_;
  s.margin = EdgeInsets(top, 0, 0, left);
  set_style(s);

  Modal::Show(ctx);
}

void MessageBox::Dismiss(UIContext* ctx, MessageBoxResult result) {
  if (on_result_) on_result_(result);
  Hide(ctx);
}

bool MessageBox::OnKeyDown(i32 key, i32 /*mods*/) {
  if (key == 256) {  // GLFW_KEY_ESCAPE
    if (on_result_) on_result_(MessageBoxResult::kDismissed);
    return true;
  }
  return false;
}

}  // namespace ugui
