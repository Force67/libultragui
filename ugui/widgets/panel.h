#ifndef ULTRAGUI_WIDGETS_PANEL_H_
#define ULTRAGUI_WIDGETS_PANEL_H_

#include <ugui/widgets/widget.h>

namespace ugui {

/// Create a plain container (WidgetKind::kPanel): a generic Widget that lays out
/// its children by flex style and draws only the base box. It has no data
/// component or vtable; the base Widget handles everything.
Widget* CreatePanel(u32 id);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_PANEL_H_
