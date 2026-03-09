#ifndef ULTRAGUI_WIDGETS_WIDGET_TREE_H_
#define ULTRAGUI_WIDGETS_WIDGET_TREE_H_

#include <ultragui/core/types.h>

namespace ugui {

class Widget;

/// Find a widget by name in a tree. Returns nullptr if not found.
Widget* FindWidget(Widget* root, const char* name);

/// Find a widget by ID in a tree. Returns nullptr if not found.
Widget* FindWidgetById(Widget* root, u32 id);

/// Recursively update all widgets (scroll momentum, animations, etc.)
void UpdateWidgetTree(Widget* root, f64 dt);

/// Bottom-up measure pass: measures all widgets and sets intrinsic sizes.
void MeasureWidgetTree(Widget* root);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_WIDGET_TREE_H_
