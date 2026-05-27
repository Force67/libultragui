#ifndef ULTRAGUI_WIDGETS_WIDGET_TREE_H_
#define ULTRAGUI_WIDGETS_WIDGET_TREE_H_

#include <ugui/core/handle.h>
#include <ugui/core/types.h>

namespace ugui {

/// Find a widget by name in a tree. Returns kNullWidget if not found.
wid FindWidget(wid root, const char* name);

/// Find a widget by ID in a tree. Returns kNullWidget if not found.
wid FindWidgetById(wid root, u32 id);

/// Recursively update all widgets (scroll momentum, animations, etc.)
void UpdateWidgetTree(wid root, f64 dt);

/// Bottom-up measure pass: measures all widgets and sets intrinsic sizes.
void MeasureWidgetTree(wid root);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_WIDGET_TREE_H_
