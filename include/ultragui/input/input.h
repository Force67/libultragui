#pragma once

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>
#include <ultragui/platform/platform.h>

#include <functional>

namespace ugui {

class Widget;

/// Mouse button identifiers
enum class MouseButton : u8 {
    Left = 0,
    Right = 1,
    Middle = 2,
};

/// Input event types
struct MouseMoveEvent {
    Vec2 position;
};

struct MouseButtonEvent {
    MouseButton button;
    bool pressed;
    Vec2 position;
};

struct MouseScrollEvent {
    Vec2 delta;
    Vec2 position;
};

struct KeyEvent {
    i32 key; // GLFW key code
    i32 scancode;
    bool pressed;
    bool repeat;
    i32 mods; // Modifier flags
};

struct CharEvent {
    u32 codepoint;
};

/// Manages input routing to the widget tree.
/// Tracks hover, press, focus state and dispatches events.
class InputRouter {
public:
    void init(Platform* platform);

    /// Process all pending input. Call once per frame before rendering.
    /// Returns true if any input was consumed.
    bool process(Widget* root);

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

private:
    void install_callbacks();

    static void glfw_mouse_pos_callback(struct GLFWwindow* window, double x, double y);
    static void glfw_mouse_button_callback(struct GLFWwindow* window, int button, int action,
                                           int mods);
    static void glfw_scroll_callback(struct GLFWwindow* window, double x, double y);
    static void glfw_key_callback(struct GLFWwindow* window, int key, int scancode, int action,
                                  int mods);
    static void glfw_char_callback(struct GLFWwindow* window, unsigned int codepoint);

    Platform* platform_ = nullptr;
    Widget* hovered_ = nullptr;
    Widget* focused_ = nullptr;
    Widget* pressed_ = nullptr;
    Vec2 mouse_pos_ = Vec2::zero();

    // Event queues (filled by GLFW callbacks, consumed by process())
    static constexpr u32 MAX_EVENTS = 64;
    MouseMoveEvent move_events_[MAX_EVENTS];
    u32 move_count_ = 0;
    MouseButtonEvent button_events_[MAX_EVENTS];
    u32 button_count_ = 0;
    MouseScrollEvent scroll_events_[MAX_EVENTS];
    u32 scroll_count_ = 0;
    KeyEvent key_events_[MAX_EVENTS];
    u32 key_count_ = 0;

    ClickHandler on_click_;
    HoverHandler on_hover_;
};

} // namespace ugui
