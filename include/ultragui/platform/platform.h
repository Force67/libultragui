#ifndef ULTRAGUI_PLATFORM_PLATFORM_H_
#define ULTRAGUI_PLATFORM_PLATFORM_H_

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>
#include <ultragui/input/input_queue.h>
#include <ultragui/style/enums.h>

namespace ugui {

/// Abstract window/platform interface.
/// The GLFW implementation is the default; engines will provide their own.
class Platform {
public:
    virtual ~Platform() = default;

    struct WindowConfig {
        i32 width = 1280;
        i32 height = 720;
        const char* title = "ultragui";
        bool resizable = true;
        bool vsync = true;
    };

    virtual bool Init(const WindowConfig& config) = 0;
    virtual void Shutdown() = 0;

    virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;
    virtual Vec2 window_size() const = 0;
    virtual Vec2 framebuffer_size() const = 0;
    virtual f32 dpi_scale() const = 0;
    virtual f64 time() const = 0;

    /// Returns native window handle for RHI backend initialization
    virtual void* native_handle() const = 0;

    /// Set the mouse cursor style
    virtual void SetCursor(Cursor cursor) = 0;

    /// Clipboard access
    virtual const char* clipboard_text() const = 0;
    virtual void set_clipboard_text(const char* text) = 0;

    /// Access the platform's input event queue.
    /// Filled during PollEvents(), consumed by InputRouter.
    virtual InputQueue& input_queue() = 0;
};

/// Create the default GLFW-based platform
Platform* CreateGlfwPlatform();

} // namespace ugui

#endif  // ULTRAGUI_PLATFORM_PLATFORM_H_
