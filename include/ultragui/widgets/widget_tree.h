#pragma once

#include <ultragui/core/types.h>

namespace ugui {

class Widget;

/// Find a widget by name in a tree. Returns nullptr if not found.
Widget* find_widget(Widget* root, const char* name);

/// Recursively update all widgets (scroll momentum, animations, etc.)
void update_widget_tree(Widget* root, f64 dt);

/// Bottom-up measure pass: measures all widgets and sets intrinsic sizes.
void measure_widget_tree(Widget* root);

} // namespace ugui
