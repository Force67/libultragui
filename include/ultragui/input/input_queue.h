#pragma once

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>

namespace ugui {

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
    i32 key;
    i32 scancode;
    bool pressed;
    bool repeat;
    i32 mods;
};

struct CharEvent {
    u32 codepoint;
};

/// Platform-agnostic input event queue. Filled by the platform backend
/// (e.g. GLFW callbacks), consumed by InputRouter::process().
struct InputQueue {
    static constexpr u32 MAX_EVENTS = 64;

    MouseMoveEvent move_events[MAX_EVENTS];
    u32 move_count = 0;

    MouseButtonEvent button_events[MAX_EVENTS];
    u32 button_count = 0;

    MouseScrollEvent scroll_events[MAX_EVENTS];
    u32 scroll_count = 0;

    KeyEvent key_events[MAX_EVENTS];
    u32 key_count = 0;

    Vec2 mouse_pos = {};

    void push_move(Vec2 pos) {
        if (move_count < MAX_EVENTS)
            move_events[move_count++] = {pos};
        mouse_pos = pos;
    }

    void push_button(MouseButton btn, bool pressed) {
        if (button_count < MAX_EVENTS)
            button_events[button_count++] = {btn, pressed, mouse_pos};
    }

    void push_scroll(Vec2 delta) {
        if (scroll_count < MAX_EVENTS)
            scroll_events[scroll_count++] = {delta, mouse_pos};
    }

    void push_key(i32 key, i32 scancode, bool pressed, bool repeat, i32 mods) {
        if (key_count < MAX_EVENTS)
            key_events[key_count++] = {key, scancode, pressed, repeat, mods};
    }

    void clear() {
        move_count = button_count = scroll_count = key_count = 0;
    }
};

} // namespace ugui
