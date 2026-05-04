#include <ugui/widgets/widget_vtable.h>

#include <ugui/widgets/button.h>
#include <ugui/widgets/checkbox.h>
#include <ugui/widgets/dropdown.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/radio.h>
#include <ugui/widgets/rich_text.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/slider.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/text_input.h>
#include <ugui/widgets/toggle.h>

namespace ugui {
namespace {

WidgetVTable g_table[static_cast<usize>(WidgetKind::kCount)];

// Register the behaviour tables for converted built-in widgets. Listed here
// (rather than self-registered per file) so a static library keeps the object
// files: this function references each one, so the linker cannot drop them.
void InstallBuiltins() {
  SetWidgetVTable(WidgetKind::kImage, ImageVTable());
  SetWidgetVTable(WidgetKind::kText, TextVTable());
  SetWidgetVTable(WidgetKind::kButton, ButtonVTable());
  SetWidgetVTable(WidgetKind::kCheckbox, CheckboxVTable());
  SetWidgetVTable(WidgetKind::kRadio, RadioVTable());
  SetWidgetVTable(WidgetKind::kToggle, ToggleVTable());
  SetWidgetVTable(WidgetKind::kSlider, SliderVTable());
  SetWidgetVTable(WidgetKind::kDropdown, DropdownVTable());
  SetWidgetVTable(WidgetKind::kRichText, RichTextVTable());
  SetWidgetVTable(WidgetKind::kTextInput, TextInputVTable());
  SetWidgetVTable(WidgetKind::kScrollView, ScrollViewVTable());
}

}  // namespace

void SetWidgetVTable(WidgetKind kind, const WidgetVTable& vt) {
  g_table[static_cast<usize>(kind)] = vt;
}

const WidgetVTable& WidgetVTableFor(WidgetKind kind) {
  static const bool installed = (InstallBuiltins(), true);
  (void)installed;
  return g_table[static_cast<usize>(kind)];
}

}  // namespace ugui
