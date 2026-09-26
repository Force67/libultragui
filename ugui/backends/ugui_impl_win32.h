#ifndef UGUI_BACKENDS_UGUI_IMPL_WIN32_H_
#define UGUI_BACKENDS_UGUI_IMPL_WIN32_H_

// ugui_impl_win32: Win32 window messages as ultragui input, for the host
// platform (platform_host.h), modeled on Dear ImGui's imgui_impl_win32.
//
// Call from the host's window procedure, or a subclass of someone else's:
//
//   if (ugui::win32::HandleMessage(*ui.platform(), hwnd, msg, wparam, lparam,
//                                  scale) && ui_wants_input)
//     return 0;  // the UI had it
//
// Keys become GLFW key codes, which is what ultragui's widgets read.

#include <ugui/core/math.h>
#include <ugui/core/types.h>

#include <stdint.h>

namespace ugui {

class Platform;

namespace win32 {

/// Feeds mouse, wheel, key and character messages to `platform`. `scale`
/// maps the window's client pixels to the UI's units: {1, 1} when the UI lays
/// out in client pixels, back buffer size over client size when it lays out
/// in the back buffer's. True when `message` was input of a kind handled
/// here; whether to keep it from the window is the caller's choice.
bool HandleMessage(Platform& platform, void* window, u32 message,
                   uintptr_t wparam, intptr_t lparam,
                   Vec2 scale = {1.0f, 1.0f});

/// The GLFW key code for a Win32 virtual key; `lparam` tells the keypad
/// Enter and the right-hand modifiers apart. -1 (GLFW_KEY_UNKNOWN) for a key
/// GLFW has no code for.
i32 KeyFromVirtualKey(u32 virtual_key, intptr_t lparam);

/// GLFW modifier bits for the keyboard's state right now.
i32 CurrentMods();

}  // namespace win32
}  // namespace ugui

#endif  // UGUI_BACKENDS_UGUI_IMPL_WIN32_H_
