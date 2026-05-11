#ifndef ULTRAGUI_WIDGETS_CONTEXT_MENU_H_
#define ULTRAGUI_WIDGETS_CONTEXT_MENU_H_

#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

class UIContext;

/// A single entry in a context menu. A separator is a thin divider with no
/// label or action.
struct ContextMenuItem {
  String label;
  Function<void()> action;
  bool separator = false;
};

/// Data for a context-menu widget (WidgetKind::kContextMenu): the item list and
/// the menu's transient visibility / hover state. A context menu is a generic
/// Widget carrying this component, displayed as an overlay at the cursor and
/// dismissed on outside click or item selection. Behaviour is in
/// ContextMenuVTable().
struct ContextMenuContent {
  Vector<ContextMenuItem> items;
  bool visible = false;
  i32 hover_index = -1;
};

/// Behaviour table (draw + measure + click + dismiss) for context menus.
WidgetVTable ContextMenuVTable();

/// Create a context-menu entity: a generic Widget tagged kContextMenu with a
/// ContextMenuContent component.
Widget* CreateContextMenu(u32 id);

/// Append an item with a label and action. No-op if `w` is null or not a
/// context menu.
void AddContextMenuItem(Widget* w, const String& label, Function<void()> action);

/// Append a separator. No-op if `w` is null or not a context menu.
void AddContextMenuSeparator(Widget* w);

/// Remove all items. No-op if `w` is null or not a context menu.
void ClearContextMenuItems(Widget* w);

/// Show the menu as an overlay at `position`, sizing it to fit its items.
void ShowContextMenuAt(Widget* w, UIContext* ctx, Vec2 position);

/// Hide the menu and clear its hover state.
void HideContextMenu(Widget* w, UIContext* ctx);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_CONTEXT_MENU_H_
