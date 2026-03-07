#ifndef ULTRAGUI_INPUT_INPUT_H_
#define ULTRAGUI_INPUT_INPUT_H_

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>
#include <ultragui/input/input_queue.h>

namespace ugui {

class Widget;
class Platform;

/// Routes input events from an InputQueue to the widget tree.
/// Manages hover, press, and focus state. Platform-agnostic.
class InputRouter {
public:
    void Init(Platform* platform);

    /// Process all pending input from the queue. Call once per frame.
    /// Returns true if any input was consumed.
    bool Process(Widget* root);

    /// Clear cached hover/press/focus state. Call when replacing the widget tree.
    void ResetState();

    Widget* hovered_widget() const { return hovered_; }
    Widget* focused_widget() const { return focused_; }
    Widget* pressed_widget() const { return pressed_; }
    /// True between OnDragStart and OnDragEnd. The application can use
    /// this to defer destructive widget-tree rebuilds while a drag is
    /// in flight (otherwise the drag target gets destroyed mid-move).
    bool is_dragging() const { return dragging_; }

    /// Re-run hit-testing at the cached mouse position and update the
    /// hovered widget + cursor. Call this after replacing the widget
    /// tree (e.g. via LoadUi) so the hover state survives the rebuild
    /// - otherwise stationary cursors lose their hover styling until
    /// the user wiggles the mouse, which appears as flicker on
    /// FPS/animation-driven dirty rebuilds.
    void RefreshHover(Widget* root);

    void set_focus(Widget* widget);
    /// Set the hovered widget directly. Mirrors set_focus - used by
    /// applications that destroy + rebuild the widget tree on a dirty
    /// flag and want hover state to survive without waiting for the
    /// next mouse-move event. Updates the kHovered widget-state bit on
    /// both the old and new widgets and updates the cursor.
    void set_hover(Widget* widget);
    Vec2 mouse_position() const { return mouse_pos_; }

    // Event callbacks (optional, for the application layer)
    using ClickHandler = Function<void(Widget*, MouseButton)>;
    using HoverHandler = Function<void(Widget*, bool)>;
    void set_on_click(ClickHandler handler) { on_click_ = std::move(handler); }
    void set_on_hover(HoverHandler handler) { on_hover_ = std::move(handler); }

    /// Register a global keyboard shortcut. Checked before dispatching to focused widget.
    using ShortcutHandler = Function<void()>;
    void RegisterShortcut(i32 key, i32 mods, ShortcutHandler handler);
    void ClearShortcuts();

    /// Gamepad B-button callback for "back" / "cancel" navigation.
    using GamepadBackHandler = Function<void()>;
    void set_on_gamepad_back(GamepadBackHandler handler) { on_gamepad_back_ = std::move(handler); }

    /// Whether gamepad navigation is currently active (last input was from gamepad).
    bool gamepad_nav_active() const { return gamepad_nav_active_; }

private:
    Platform* platform_ = nullptr;
    Widget* hovered_ = nullptr;
    Widget* focused_ = nullptr;
    Widget* pressed_ = nullptr;
    /// Resolved drag target - usually `pressed_`, but may be a draggable
    /// ancestor if the press landed on a drag handle (see input.cc).
    Widget* drag_target_ = nullptr;
    Vec2 mouse_pos_ = Vec2::Zero();
    Vec2 drag_start_ = Vec2::Zero();
    Vec2 drag_prev_ = Vec2::Zero();
    bool dragging_ = false;
    bool gamepad_nav_active_ = false;

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

    void ProcessGamepadNavigation(Widget* root, f32 delta_time);
    void NavigateFocus(Widget* root, i8 dir_x, i8 dir_y);
};

} // namespace ugui

#endif  // ULTRAGUI_INPUT_INPUT_H_
