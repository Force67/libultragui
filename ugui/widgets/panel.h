#ifndef ULTRAGUI_WIDGETS_PANEL_H_
#define ULTRAGUI_WIDGETS_PANEL_H_

#include <ugui/core/handle.h>
#include <ugui/core/types.h>

namespace ugui {

/// Create a plain container (WidgetKind::kPanel): a generic widget entity that
/// lays out its children by flex style and draws only the base box. It has no
/// data component or vtable; the base PaintWidget handles everything.
wid CreatePanel(u32 id);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_PANEL_H_
