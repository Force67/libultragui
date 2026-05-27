#ifndef ULTRAGUI_WIDGETS_WIDGET_H_
#define ULTRAGUI_WIDGETS_WIDGET_H_

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
u32 NextWidgetId();

// --- Entity free-function API -----------------------------------------------
// A widget is a plain entity id (wid) in a component World. Data is read
// directly via world.Get<Component>(e); behaviour and tree ops are these free
// functions. Host code uses CreateText(...) etc. (per-widget headers) to build
// entities and DestroyWidget() to tear a subtree down.

/// Recursively destroy a widget subtree: releases the entity and all its
/// descendants (and their components).
void DestroyWidget(WidgetRegistry& world, wid e);

// Tree.
void AddChild(WidgetRegistry& world, wid parent, wid child);
void RemoveChild(WidgetRegistry& world, wid parent, wid child);
void SetContext(WidgetRegistry& world, wid e, const WidgetContext* ctx);

// Style / state (mutators with side effects; plain data is read via Get<C>).
void SetStyle(WidgetRegistry& world, wid e, const Style& s);
Style ComputedStyle(WidgetRegistry& world, wid e);
WidgetState WidgetStateOf(WidgetRegistry& world, wid e);
void SetWidgetState(WidgetRegistry& world, wid e, WidgetState state);
void SetSelected(WidgetRegistry& world, wid e, bool v);
void AddStateOverride(WidgetRegistry& world, wid e, WidgetState state,
                      const Style& override_style, u64 mask);
void AddStateTransition(WidgetRegistry& world, wid e, WidgetState state,
                        const Transition& transition);
void SetAnimationStyle(WidgetRegistry& world, wid e, const Style& s);
void ClearAnimationStyle(WidgetRegistry& world, wid e);

// Misc.
f32 UiScale(WidgetRegistry& world, wid e);
const WidgetContext* WidgetContextOf(WidgetRegistry& world, wid e);
void MarkDirty(WidgetRegistry& world, wid e);
void MarkPaintDirty(WidgetRegistry& world, wid e);
void SetTooltip(WidgetRegistry& world, wid e, const String& text);
const String& TooltipText(WidgetRegistry& world, wid e);
Vec2 InputToLayoutPoint(WidgetRegistry& world, wid e, Vec2 point);
wid HitTest(WidgetRegistry& world, wid e, Vec2 point);

// Per-kind dispatch (the vtable, see widget_vtable.h).
void PaintWidget(WidgetRegistry& world, wid e, Renderer2D& renderer);
void MeasureWidget(WidgetRegistry& world, wid e, f32& out_w, f32& out_h);
void LayoutWidget(WidgetRegistry& world, wid e, const Rect& rect,
                  const Rect& content_rect);
void UpdateWidget(WidgetRegistry& world, wid e, f64 dt);
bool ClickWidget(WidgetRegistry& world, wid e);
bool ScrollWidget(WidgetRegistry& world, wid e, Vec2 delta);
bool KeyDownWidget(WidgetRegistry& world, wid e, i32 key, i32 mods);
bool CharInputWidget(WidgetRegistry& world, wid e, u32 codepoint);
bool ConsumesTextInput(WidgetRegistry& world, wid e);
void DismissWidget(WidgetRegistry& world, wid e);
void PopulateLayoutNode(WidgetRegistry& world, wid e, LayoutNode& node);
void ApplyLayoutResult(WidgetRegistry& world, wid e, const LayoutNode& node);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_WIDGET_H_
