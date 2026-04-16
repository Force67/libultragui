#include <ugui/widgets/widget_vtable.h>

#include <ugui/widgets/image.h>
#include <ugui/widgets/text.h>

namespace ugui {
namespace {

WidgetVTable g_table[static_cast<usize>(WidgetKind::kCount)];

// Register the behaviour tables for converted built-in widgets. Listed here
// (rather than self-registered per file) so a static library keeps the object
// files: this function references each one, so the linker cannot drop them.
void InstallBuiltins() {
  SetWidgetVTable(WidgetKind::kImage, ImageVTable());
  SetWidgetVTable(WidgetKind::kText, TextVTable());
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
