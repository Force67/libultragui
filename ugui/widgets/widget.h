#ifndef ULTRAGUI_WIDGETS_WIDGET_H_
#define ULTRAGUI_WIDGETS_WIDGET_H_

#include <ugui/core/export.h>
#include <ugui/core/handle.h>
#include <ugui/core/rect.h>
#include <ugui/core/types.h>
#include <ugui/style/style.h>
#include <ugui/style/transition.h>
#include <ugui/widgets/widget_context.h>

namespace ugui {

class Renderer2D;
class WidgetRegistry;
struct LayoutNode;

/// Stable type tag for every widget, used for per-kind vtable dispatch. Lives
/// in the WidgetNode component.
enum class WidgetKind : u8 {
  kWidget,
  kPanel,
  kText,
  kButton,
  kImage,
  kCheckbox,
  kRadio,
  kToggle,
  kSlider,
  kDropdown,
  kTextInput,
  kScrollView,
  kContextMenu,
  kModal,
  kRichText,
  kMessageBox,
  kCount,  // number of kinds; not a real widget
};

// Core components carried by every widget entity. A widget IS its set of
// components, keyed by its WidgetId; there is no widget object.

/// Geometry + dirty flags (written by layout, read by paint/hit-testing).
struct Transform {
  Rect rect;
  Rect content_rect;
  f32 intrinsic_w = 0;
  f32 intrinsic_h = 0;
  bool layout_dirty = true;
  bool paint_dirty = true;
};

/// Tree links, stored as stable handles.
struct Hierarchy {
  wid parent;
  Vector<wid> children;
};

/// Base style and current interaction-state bitset. State overrides and the
/// animation override live in the StateStyle / AnimStyle components.
struct StyleC {
  Style style;
  WidgetState state = WidgetState::kNone;
};

/// Identity + misc per-widget metadata.
struct WidgetNode {
  u32 id = 0;
  WidgetKind kind = WidgetKind::kWidget;
  String name;
  i32 tab_index = -1;  // -1 = not focusable via tab, 0+ = tab order
  const WidgetContext* context = nullptr;
};

/// Generate a unique widget id (thread-local counter; 0 is reserved).
UGUI_API u32 NextWidgetId();

// --- Entity free-function API -----------------------------------------------
// A widget is a plain entity id (wid) in a component World. Data is read
// directly via world.Get<Component>(e); behaviour and tree ops are these free
// functions. Host code uses CreateText(...) etc. (per-widget headers) to build
// entities and DestroyWidget() to tear a subtree down.

/// Recursively destroy a widget subtree: releases the entity and all its
/// descendants (and their components).
UGUI_API void DestroyWidget(WidgetRegistry& world, wid e);

// Tree.
UGUI_API void AddChild(WidgetRegistry& world, wid parent, wid child);
UGUI_API void RemoveChild(WidgetRegistry& world, wid parent, wid child);
UGUI_API void SetContext(WidgetRegistry& world, wid e,
                         const WidgetContext* ctx);

// Style / state (mutators with side effects; plain data is read via Get<C>).
UGUI_API void SetStyle(WidgetRegistry& world, wid e, const Style& s);
UGUI_API Style ComputedStyle(WidgetRegistry& world, wid e);
UGUI_API WidgetState WidgetStateOf(WidgetRegistry& world, wid e);
UGUI_API void SetWidgetState(WidgetRegistry& world, wid e, WidgetState state);
UGUI_API void SetSelected(WidgetRegistry& world, wid e, bool v);
UGUI_API void AddStateOverride(WidgetRegistry& world, wid e, WidgetState state,
                               const Style& override_style, u64 mask);
UGUI_API void AddStateTransition(WidgetRegistry& world, wid e,
                                 WidgetState state,
                                 const Transition& transition);
UGUI_API void SetAnimationStyle(WidgetRegistry& world, wid e, const Style& s);
UGUI_API void ClearAnimationStyle(WidgetRegistry& world, wid e);

// Misc.
UGUI_API f32 UiScale(WidgetRegistry& world, wid e);
UGUI_API const WidgetContext* WidgetContextOf(WidgetRegistry& world, wid e);
UGUI_API void MarkDirty(WidgetRegistry& world, wid e);
UGUI_API void MarkPaintDirty(WidgetRegistry& world, wid e);
UGUI_API void SetTooltip(WidgetRegistry& world, wid e, const String& text);
UGUI_API const String& TooltipText(WidgetRegistry& world, wid e);
UGUI_API Vec2 InputToLayoutPoint(WidgetRegistry& world, wid e, Vec2 point);
UGUI_API wid HitTest(WidgetRegistry& world, wid e, Vec2 point);

// Per-kind dispatch (the vtable, see widget_vtable.h).
UGUI_API void PaintWidget(WidgetRegistry& world, wid e, Renderer2D& renderer);
UGUI_API void MeasureWidget(WidgetRegistry& world, wid e, f32& out_w,
                            f32& out_h);
UGUI_API void LayoutWidget(WidgetRegistry& world, wid e, const Rect& rect,
                           const Rect& content_rect);
UGUI_API void UpdateWidget(WidgetRegistry& world, wid e, f64 dt);
UGUI_API bool ClickWidget(WidgetRegistry& world, wid e);
UGUI_API bool ScrollWidget(WidgetRegistry& world, wid e, Vec2 delta);
UGUI_API bool KeyDownWidget(WidgetRegistry& world, wid e, i32 key, i32 mods);
UGUI_API bool CharInputWidget(WidgetRegistry& world, wid e, u32 codepoint);
UGUI_API bool ConsumesTextInput(WidgetRegistry& world, wid e);
UGUI_API void DismissWidget(WidgetRegistry& world, wid e);
UGUI_API void PopulateLayoutNode(WidgetRegistry& world, wid e,
                                 LayoutNode& node);
UGUI_API void ApplyLayoutResult(WidgetRegistry& world, wid e,
                                const LayoutNode& node);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_WIDGET_H_
