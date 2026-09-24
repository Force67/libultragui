#ifndef ULTRAGUI_PLATFORM_PLATFORM_HOST_H_
#define ULTRAGUI_PLATFORM_PLATFORM_HOST_H_

// The host platform (platform_host.cc): a ugui::Platform with no window of
// its own, for embedding ultragui where the host already has one it does not
// hand over, a game's say. Select it with -DULTRAGUI_PLATFORM_HOST=ON and run
// UIContext in draw-data mode; external_window is then not needed.
//
// The host reports the viewport and feeds input; the context takes both when
// it polls, at the start of its frame. Every call here may come from any
// thread, so a window procedure on one thread can feed a UI rendered on
// another.
//
// Keys are GLFW key codes, as everywhere in ultragui. ugui_impl_win32 turns
// Win32 window messages into these calls.

#include <ugui/core/math.h>
#include <ugui/input/input_queue.h>
#include <ugui/style/enums.h>

namespace ugui {

class Platform;

namespace host {

/// The size the UI lays out in, and the render target's size in pixels. They
/// differ on a scaled display, or when the host draws at another resolution
/// than its window; the ratio becomes the draw data's framebuffer_scale.
void SetViewport(Platform& platform, Vec2 window_size, Vec2 framebuffer_size);

/// Positions are in the units of `window_size`.
void PushMouseMove(Platform& platform, Vec2 position);
void PushMouseButton(Platform& platform, MouseButton button, bool pressed);
void PushScroll(Platform& platform, Vec2 delta);
void PushKey(Platform& platform, i32 key, i32 scancode, bool pressed,
             bool repeat, i32 mods);
void PushChar(Platform& platform, u32 codepoint);

/// The cursor the UI asked for last, for a host that draws its own.
Cursor RequestedCursor(const Platform& platform);

}  // namespace host
}  // namespace ugui

#endif  // ULTRAGUI_PLATFORM_PLATFORM_HOST_H_
