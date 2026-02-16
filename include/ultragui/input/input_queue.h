#ifndef ULTRAGUI_INPUT_INPUT_QUEUE_H_
#define ULTRAGUI_INPUT_INPUT_QUEUE_H_

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>

namespace ugui {

/// Mouse button identifiers
enum class MouseButton : u8 {
    kLeft = 0,
    kRight = 1,
    kMiddle = 2,
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
/// (e.g. GLFW callbacks), consumed by InputRouter::Process().
struct InputQueue {
    static constexpr u32 kMaxEvents = 64;

    MouseMoveEvent move_events[kMaxEvents];
    u32 move_count = 0;

    MouseButtonEvent button_events[kMaxEvents];
    u32 button_count = 0;

    MouseScrollEvent scroll_events[kMaxEvents];
    u32 scroll_count = 0;

    KeyEvent key_events[kMaxEvents];
    u32 key_count = 0;

    CharEvent char_events[kMaxEvents];
    u32 char_count = 0;

    Vec2 mouse_pos = {};

    void PushMove(Vec2 pos) {
        if (move_count < kMaxEvents)
            move_events[move_count++] = {pos};
        mouse_pos = pos;
    }

    void PushButton(MouseButton btn, bool pressed) {
        if (button_count < kMaxEvents)
            button_events[button_count++] = {btn, pressed, mouse_pos};
    }

    void PushScroll(Vec2 delta) {
        if (scroll_count < kMaxEvents)
            scroll_events[scroll_count++] = {delta, mouse_pos};
    }

    void PushKey(i32 key, i32 scancode, bool pressed, bool repeat, i32 mods) {
        if (key_count < kMaxEvents)
            key_events[key_count++] = {key, scancode, pressed, repeat, mods};
    }

    void PushChar(u32 codepoint) {
        if (char_count < kMaxEvents)
            char_events[char_count++] = {codepoint};
    }

    void clear() {
        move_count = button_count = scroll_count = key_count = char_count = 0;
    }
};

} // namespace ugui

#endif  // ULTRAGUI_INPUT_INPUT_QUEUE_H_
