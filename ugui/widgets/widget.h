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

/// Stable type tag for every widget, enabling handle-safe downcasts without
/// RTTI and per-kind vtable dispatch. Lives in the WidgetNode component.
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

// Core components carried by every widget entity. The Widget handle (below)
// holds no data of its own; all widget state lives in these component stores,
// keyed by the entity's WidgetId.

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

/// Lightweight handle to a widget entity: just the stable id and its World.
/// Carries no data (it all lives in components) and exists to give the existing
/// call sites an object-oriented facade while the layer is data-oriented under
/// the hood. Created on the heap by the Create<Kind> factories; the registry
/// slot keeps the pointer so handles resolve back to it.
class Widget {
  friend class WidgetRegistry;

 public:
  explicit Widget(u32 id = 0);

  /// Generate a unique widget ID (thread-local counter).
  static u32 NextAutoId() {
    static u32 s_counter = 1;  // 0 is reserved / sentinel
    return s_counter++;
  }
  ~Widget();

  // --- Tree management (links are stable handles, not raw pointers) ---
  void AddChild(Widget* child);
  void RemoveChild(Widget* child);
  void ClearChildren();
  wid ChildAt(u32 index) const;
  u32 child_count() const;
  wid parent() const;
  const Vector<wid>& children() const;

  /// Transient resolved views for traversal. Valid only until the tree changes.
  Widget* parent_ptr() const;
  Vector<Widget*> child_ptrs() const;

  // --- Identity ---
  u32 id() const;
  void set_id(u32 id);
  const String& name() const;
  void set_name(const String& name);
  WidgetKind kind() const;
  void set_kind(WidgetKind k);

  /// The World this widget belongs to (resolves its handles and components).
  WidgetRegistry* registry() const { return registry_; }

  /// Stable handle to this widget entity.
  WidgetId handle() const { return self_; }

  // --- Style ---
  Style& style();
  const Style& style() const;
  void set_style(const Style& s);

  void AddStateOverride(WidgetState state, const Style& override_style,
                        u64 mask);
  void AddStateTransition(WidgetState state, const Transition& transition);
  WidgetState widget_state() const;
  void set_widget_state(WidgetState state);

  /// Toggle the kSelected bit (application-driven highlighting; drives the
  /// :selected/:active style override declared in the IDL).
  void set_selected(bool v);
  bool selected() const;

  /// Effective style (base + active overrides + animation).
  Style ComputedStyle() const;

  /// Animated style override (used by the Animator callback).
  void SetAnimationStyle(const Style& s);
  void ClearAnimationStyle();

  // --- Layout ---
  Rect rect() const;
  Rect content_rect() const;
  void set_intrinsic_size(f32 w, f32 h);

  // --- Dirty flags ---
  void MarkDirty();
  void MarkPaintDirty();
  bool IsDirty() const;
  bool IsPaintDirty() const;

  // --- Hit testing ---
  bool contains(Vec2 point) const;
  wid HitTest(Vec2 point);

  /// Convert a screen/input point into this widget's layout space by accounting
  /// for ancestor scroll offsets.
  Vec2 InputToLayoutPoint(Vec2 point) const;

  // --- Context propagation ---
  void SetContext(const WidgetContext* ctx);
  const WidgetContext* context() const;

  // --- Lifecycle (dispatch to the per-kind vtable; see widget_vtable.h) ---
  void OnMount() {}
  void OnUnmount() {}
  void OnUpdate(f64 dt);
  void OnLayout(const Rect& rect, const Rect& content_rect);
  void OnPaint(Renderer2D& renderer);
  void Measure(f32& out_width, f32& out_height);

  // --- Tooltip (stored as a Tooltip component on this entity) ---
  void set_tooltip(const String& text);
  const String& tooltip() const;

  // --- Tab navigation ---
  void set_tab_index(i32 idx);
  i32 tab_index() const;
  bool focusable() const;

  /// Viewport scale factor (1.0 at the design resolution).
  f32 ui_scale() const;

  // --- Event dispatch (dispatch to the per-kind vtable) ---
  bool OnClick();
  bool OnScroll(Vec2 delta);
  bool OnKeyDown(i32 key, i32 mods);
  bool OnKeyUp(i32 key, i32 mods) { return false; }
  bool OnCharInput(u32 codepoint);
  bool consumes_text_input() const;
  void OnDismiss();

  // --- Layout integration ---
  void PopulateLayoutNode(LayoutNode& node) const;
  void ApplyLayoutResult(const LayoutNode& node);

 private:
  // Resolve this entity's core components (always present after construction).
  Transform* xf() const;
  Hierarchy* hier() const;
  StyleC* sc() const;
  WidgetNode* node() const;

  wid self_;                            // this widget's handle (set in ctor)
  WidgetRegistry* registry_ = nullptr;  // owning world (resolves components)
};

/// Handle-safe downcast: returns p as T* when its kind matches, else nullptr.
/// No-RTTI replacement for dynamic_cast (used by WidgetRegistry::GetAs).
template <class T>
T* widget_cast(Widget* p) {
  return (p && p->kind() == T::kKind) ? static_cast<T*>(p) : nullptr;
}
template <class T>
const T* widget_cast(const Widget* p) {
  return (p && p->kind() == T::kKind) ? static_cast<const T*>(p) : nullptr;
}

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_WIDGET_H_
