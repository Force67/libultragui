#ifndef UGUI_WIDGETS_COMPONENTS_H_
#define UGUI_WIDGETS_COMPONENTS_H_

#include <ugui/core/config.h>
#include <ugui/core/math.h>
#include <ugui/core/types.h>
#include <ugui/style/style.h>
#include <ugui/style/transition.h>

namespace ugui {

// Built-in widget components. Behaviour that used to live as members + virtuals
// on the Widget base is migrating here, so it becomes opt-in per entity instead
// of a cost every widget pays. Attach with world.Add<T>(id, {...}); a system
// reads it back. Host engines define their own component structs the same way.

/// Hover help text. The tooltip system shows it after a short delay.
struct Tooltip {
  String text;
};

/// Notifier fired during a drag with the widget's new top-left in screen
/// pixels, so the application can persist a dragged position across rebuilds.
using DragHandler = Function<void(Vec2 /*top_left*/)>;

/// Marks an entity as user-movable: dragging it (or a DragHandle that resolves
/// to it) rewrites its style offsets so the next layout pass follows the
/// cursor. origin/press are scratch the drag system fills in on drag start.
struct Movable {
  f32 origin_x = 0;
  f32 origin_y = 0;
  Vec2 press = Vec2::Zero();
  DragHandler on_drag;
};

/// Tag: clicking inside this widget and dragging routes the drag to the nearest
/// Movable ancestor (e.g. a panel header grabs the whole panel).
struct DragHandle {};

/// A state -> transition mapping (e.g. fade when :hover activates).
struct StateTransitionRule {
  WidgetState state;
  Transition transition;
};

/// Per-state style overrides (:hover, :pressed, :selected, ...) and their
/// transitions. Only widgets that declare state styles carry this, so a plain
/// panel no longer pays for two empty vectors.
struct StateStyle {
  Vector<StyleOverride> overrides;
  Vector<StateTransitionRule> transitions;
};

/// Active animated style override written by the animator. Its presence means
/// the widget is currently showing an animated style; removing the component
/// reverts to the resolved base/state style. Kept off the base so non-animating
/// widgets do not each reserve a full Style.
struct AnimStyle {
  Style style;
};

}  // namespace ugui

#endif  // UGUI_WIDGETS_COMPONENTS_H_
