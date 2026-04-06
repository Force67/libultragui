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
/// RTTI (see widget_cast). Subclasses set their own value via kKind/kind().
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
};

/// Base class for all UI widgets. Provides lifecycle, tree structure,
/// hit-testing, and dirty-flag propagation.
class Widget {
  friend class WidgetRegistry;

 public:
  explicit Widget(u32 id = 0);

  /// Generate a unique widget ID (thread-local counter).
  static u32 NextAutoId() {
    static u32 s_counter = 1;  // 0 is reserved / sentinel
    return s_counter++;
  }
  virtual ~Widget();

  // --- Tree management (links are stable handles, not raw pointers) ---
  void AddChild(Widget* child);
  void RemoveChild(Widget* child);
  void ClearChildren();
  wid ChildAt(u32 index) const;
  u32 child_count() const { return static_cast<u32>(children_.size()); }
  wid parent() const { return parent_; }
  const Vector<wid>& children() const { return children_; }

  /// Transient resolved views for traversal. The returned pointers are valid
  /// only until the tree changes; never store them (store the wid instead).
  Widget* parent_ptr() const;
  Vector<Widget*> child_ptrs() const;

  // --- Identity ---
  u32 id() const { return id_; }
  void set_id(u32 id) { id_ = id; }
  const String& name() const { return name_; }
  void set_name(const String& name) { name_ = name; }

  /// Stable type tag for handle-safe casts (no RTTI). Subclasses override.
  virtual WidgetKind kind() const { return WidgetKind::kWidget; }

  /// Stable handle to this widget. Prefer storing this over a raw Widget*:
  /// once the widget is destroyed the handle resolves to null instead of
  /// dangling. The slot is allocated eagerly in the constructor.
  WidgetId handle();

  // --- Style ---
  Style& style() { return style_; }
  const Style& style() const { return style_; }
  void set_style(const Style& s) {
    style_ = s;
    MarkDirty();
  }

  void AddStateOverride(WidgetState state, const Style& override_style,
                        u64 mask);
  void AddStateTransition(WidgetState state, const Transition& transition);
  WidgetState widget_state() const { return state_; }
  void set_widget_state(WidgetState state);

  /// Toggle the kSelected bit on this widget. Use this for
  /// application-driven highlighting (active sidebar entry, current
  /// list selection, etc.) so the widget's :selected/:active style
  /// override declared in the .ugui file kicks in. This keeps the
  /// visual rules in the IDL instead of having C++ rewrite the base
  /// style imperatively each time the active row changes.
  void set_selected(bool v) {
    WidgetState s = state_;
    if (v)
      s = s | WidgetState::kSelected;
    else
      s = static_cast<WidgetState>(static_cast<u16>(s) &
                                   ~static_cast<u16>(WidgetState::kSelected));
    set_widget_state(s);
  }
  bool selected() const { return HasState(state_, WidgetState::kSelected); }

  /// Get the effective style (base + active overrides + animation)
  Style ComputedStyle() const;

  /// Set an animated style override (used by Animator callback)
  void SetAnimationStyle(const Style& s);
  void ClearAnimationStyle();

  // --- Layout ---
  Rect rect() const { return rect_; }
  Rect content_rect() const { return content_rect_; }

  /// Set intrinsic content size (for text, images, etc.)
  void set_intrinsic_size(f32 w, f32 h) {
    intrinsic_w_ = w;
    intrinsic_h_ = h;
  }

  // --- Dirty flags ---
  void MarkDirty();
  void MarkPaintDirty() { paint_dirty_ = true; }
  bool IsDirty() const { return layout_dirty_; }
  bool IsPaintDirty() const { return paint_dirty_; }

  // --- Hit testing ---
  bool contains(Vec2 point) const { return rect_.contains(point); }
  virtual wid HitTest(Vec2 point);

  /// Convert a screen/input point into this widget's layout space by
  /// accounting for ancestor scroll offsets.
  Vec2 InputToLayoutPoint(Vec2 point) const;

  // --- Context propagation ---
  void SetContext(const WidgetContext* ctx);
  const WidgetContext* context() const { return context_; }

  // --- Lifecycle (override in subclasses) ---
  virtual void OnMount() {}
  virtual void OnUnmount() {}
  virtual void OnUpdate(f64 dt) {}
  virtual void OnLayout(const Rect& rect, const Rect& content_rect);
  virtual void OnPaint(Renderer2D& renderer);
  virtual void Measure(f32& out_width, f32& out_height);

  // --- Tooltip (stored as a Tooltip component on this entity) ---
  void set_tooltip(const String& text);
  const String& tooltip() const;

  // --- Tab navigation ---
  void set_tab_index(i32 idx) { tab_index_ = idx; }
  i32 tab_index() const { return tab_index_; }
  bool focusable() const { return tab_index_ >= 0; }

  /// Viewport scale factor (1.0 at the design resolution).
  f32 ui_scale() const { return context_ ? context_->ui_scale : 1.0f; }

  // --- Event dispatch (override in subclasses) ---
  virtual bool OnClick() { return false; }
  virtual bool OnScroll(Vec2 delta) { return false; }
  virtual bool OnKeyDown(i32 key, i32 mods) { return false; }
  virtual bool OnKeyUp(i32 key, i32 mods) { return false; }
  virtual bool OnCharInput(u32 codepoint) { return false; }

  /// Whether this widget swallows printable text while focused. Text
  /// inputs override this to true so the InputRouter doesn't treat
  /// Enter/Space as "activate focused widget" - that shim is for
  /// keyboard/gamepad nav on buttons, and would otherwise call
  /// TextInput::OnClick which repositions the caret to the last
  /// mouse position whenever the user types a space.
  virtual bool consumes_text_input() const { return false; }
  /// Called when an overlay is dismissed (click outside).
  virtual void OnDismiss() {}

  // Drag-to-move now lives in the Movable / DragHandle components (see
  // components.h); attach them via the World and the input system handles it.

  // --- Layout integration ---
  void PopulateLayoutNode(LayoutNode& node) const;
  void ApplyLayoutResult(const LayoutNode& node);

 protected:
  u32 id_ = 0;
  wid self_;                            // this widget's handle (set in ctor)
  WidgetRegistry* registry_ = nullptr;  // owning registry (resolves links)
  String name_;
  wid parent_;
  Vector<wid> children_;
  const WidgetContext* context_ = nullptr;

  Style style_;
  Vector<StyleOverride> state_overrides_;
  WidgetState state_ = WidgetState::kNone;

  struct StateTransitionConfig {
    WidgetState state;
    Transition transition;
  };
  Vector<StateTransitionConfig> state_transitions_;

  Optional<Style> animation_style_;

  Rect rect_;
  Rect content_rect_;
  f32 intrinsic_w_ = 0, intrinsic_h_ = 0;
  i32 tab_index_ = -1;  // -1 = not focusable via tab, 0+ = tab order
  bool layout_dirty_ = true;
  bool paint_dirty_ = true;
};

/// Handle-safe downcast: returns p as T* when its kind matches, else nullptr.
/// Drop-in replacement for dynamic_cast that needs no RTTI.
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
