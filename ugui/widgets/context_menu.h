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
/// widget carrying this component, displayed as an overlay at the cursor and
/// dismissed on outside click or item selection. Behaviour is in
/// ContextMenuVTable().
struct ContextMenuContent {
  Vector<ContextMenuItem> items;
  bool visible = false;
  i32 hover_index = -1;
};

/// Behaviour table (draw + measure + click + dismiss) for context menus.
UGUI_API WidgetVTable ContextMenuVTable();

/// Create a context-menu entity: a generic widget tagged kContextMenu with a
/// ContextMenuContent component.
UGUI_API wid CreateContextMenu(u32 id);

/// Append an item with a label and action. No-op if `e` is not a context menu.
UGUI_API void AddContextMenuItem(wid e, const String& label,
                                 Function<void()> action);

/// Append a separator. No-op if `e` is not a context menu.
UGUI_API void AddContextMenuSeparator(wid e);

/// Remove all items. No-op if `e` is not a context menu.
UGUI_API void ClearContextMenuItems(wid e);

/// Show the menu as an overlay at `position`, sizing it to fit its items.
UGUI_API void ShowContextMenuAt(wid e, UIContext* ctx, Vec2 position);

/// Hide the menu and clear its hover state.
UGUI_API void HideContextMenu(wid e, UIContext* ctx);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_CONTEXT_MENU_H_
