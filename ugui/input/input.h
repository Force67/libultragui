#ifndef ULTRAGUI_INPUT_INPUT_H_
#define ULTRAGUI_INPUT_INPUT_H_

#include <ugui/core/handle.h>
#include <ugui/core/math.h>
#include <ugui/core/types.h>
#include <ugui/input/input_queue.h>

namespace ugui {

class Platform;

/// Routes input events from an InputQueue to the widget tree.
/// Manages hover, press, and focus state. Platform-agnostic.
class InputRouter {
 public:
  void Init(Platform* platform);

  /// Process all pending input from the queue. Call once per frame.
  /// Returns true if any input was consumed.
  bool Process(wid root);

  /// Clear cached hover/press/focus state. Call when replacing the widget tree.
  void ResetState();

  wid hovered_widget() const { return hovered_; }
  wid focused_widget() const { return focused_; }
  wid pressed_widget() const { return pressed_; }
  /// True between OnDragStart and OnDragEnd. Defer tree rebuilds while
  /// dragging, or the drag target gets destroyed mid-move.
  bool is_dragging() const { return dragging_; }

  /// Re-run hit-testing at the cached mouse position. Call after replacing
  /// the widget tree so hover survives the rebuild instead of flickering
  /// off until the next mouse-move.
  void RefreshHover(wid root);

  void set_focus(wid widget);
  /// Set the hovered widget directly. For apps that rebuild the tree on a
  /// dirty flag and want hover state to survive without a mouse-move.
  void set_hover(wid widget);
  Vec2 mouse_position() const { return mouse_pos_; }

  // Event callbacks (optional, for the application layer)
  using ClickHandler = Function<void(wid, MouseButton)>;
  using HoverHandler = Function<void(wid, bool)>;
  void set_on_click(ClickHandler handler) { on_click_ = std::move(handler); }
  void set_on_hover(HoverHandler handler) { on_hover_ = std::move(handler); }

  /// Register a global keyboard shortcut. Checked before dispatching to focused
  /// widget.
  using ShortcutHandler = Function<void()>;
  void RegisterShortcut(i32 key, i32 mods, ShortcutHandler handler);
  void ClearShortcuts();

  /// Gamepad B-button callback for "back" / "cancel" navigation.
  using GamepadBackHandler = Function<void()>;
  void set_on_gamepad_back(GamepadBackHandler handler) {
    on_gamepad_back_ = std::move(handler);
  }

  /// Whether gamepad navigation is currently active (last input was from
  /// gamepad).
  bool gamepad_nav_active() const { return gamepad_nav_active_; }

  /// Keyboard navigation: arrows move focus, Enter/Space activate. On by
  /// default; disable for UIs that use arrow keys themselves (text editors,
  /// game views).
  ///
  /// Focus ring: widgets with `tab-index` form the ring when present;
  /// otherwise an implicit ring of interactive widgets (buttons, checkboxes,
  /// sliders, dropdowns, `cursor: pointer`). Off: only explicit `tab-index`
  /// is focusable.
  void set_keyboard_navigation(bool enabled) { keyboard_nav_ = enabled; }
  bool keyboard_navigation() const { return keyboard_nav_; }

 private:
  Platform* platform_ = nullptr;

  // Handles, not raw pointers: a tree rebuild nulls them instead of
  // leaving them dangling.
  WidgetId hovered_;
  WidgetId focused_;
  WidgetId pressed_;
  /// Usually `pressed_`; a draggable ancestor if the press landed on a
  /// drag handle (see input.cc).
  WidgetId drag_target_;

  Vec2 mouse_pos_ = Vec2::Zero();
  Vec2 drag_start_ = Vec2::Zero();
  Vec2 drag_prev_ = Vec2::Zero();
  bool dragging_ = false;
  bool gamepad_nav_active_ = false;
  bool keyboard_nav_ = true;

  // Gamepad stick repeat navigation
  f32 gamepad_nav_timer_ = 0.0f;
  static constexpr f32 kGamepadNavInitialDelay = 0.35f;
  static constexpr f32 kGamepadNavRepeatRate = 0.12f;
  i8 gamepad_nav_dir_x_ = 0;
  i8 gamepad_nav_dir_y_ = 0;

  static constexpr f32 kDragThreshold = 4.0f;

  struct Shortcut {
    i32 key;
    i32 mods;
    ShortcutHandler handler;
  };
  Vector<Shortcut> shortcuts_;

  ClickHandler on_click_;
  HoverHandler on_hover_;
  GamepadBackHandler on_gamepad_back_;

  void ProcessGamepadNavigation(wid root, f32 delta_time);
  void NavigateFocus(wid root, i8 dir_x, i8 dir_y);
};

}  // namespace ugui

#endif  // ULTRAGUI_INPUT_INPUT_H_
