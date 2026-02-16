#ifndef ULTRAGUI_INPUT_INPUT_H_
#define ULTRAGUI_INPUT_INPUT_H_

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>
#include <ultragui/input/input_queue.h>

#include <functional>

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

    Widget* hovered_widget() const { return hovered_; }
    Widget* focused_widget() const { return focused_; }
    Widget* pressed_widget() const { return pressed_; }

    void set_focus(Widget* widget);
    Vec2 mouse_position() const { return mouse_pos_; }

    // Event callbacks (optional, for the application layer)
    using ClickHandler = std::function<void(Widget*, MouseButton)>;
    using HoverHandler = std::function<void(Widget*, bool)>;
    void set_on_click(ClickHandler handler) { on_click_ = std::move(handler); }
    void set_on_hover(HoverHandler handler) { on_hover_ = std::move(handler); }

    /// Register a global keyboard shortcut. Checked before dispatching to focused widget.
    using ShortcutHandler = std::function<void()>;
    void RegisterShortcut(i32 key, i32 mods, ShortcutHandler handler);
    void ClearShortcuts();

private:
    Platform* platform_ = nullptr;
    Widget* hovered_ = nullptr;
    Widget* focused_ = nullptr;
    Widget* pressed_ = nullptr;
    Vec2 mouse_pos_ = Vec2::Zero();
    Vec2 drag_start_ = Vec2::Zero();
    Vec2 drag_prev_ = Vec2::Zero();
    bool dragging_ = false;

    static constexpr f32 kDragThreshold = 4.0f;

    struct Shortcut {
        i32 key;
        i32 mods;
        ShortcutHandler handler;
    };
    std::vector<Shortcut> shortcuts_;

    ClickHandler on_click_;
    HoverHandler on_hover_;
};

} // namespace ugui

#endif  // ULTRAGUI_INPUT_INPUT_H_
