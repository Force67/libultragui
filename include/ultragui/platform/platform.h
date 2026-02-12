#pragma once

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

    virtual bool init(const WindowConfig& config) = 0;
    virtual void shutdown() = 0;

    virtual bool should_close() const = 0;
    virtual void poll_events() = 0;
    virtual Vec2 window_size() const = 0;
    virtual Vec2 framebuffer_size() const = 0;
    virtual f32 dpi_scale() const = 0;
    virtual f64 time() const = 0;

    /// Returns native window handle for RHI backend initialization
    virtual void* native_handle() const = 0;

    /// Set the mouse cursor style
    virtual void set_cursor(Cursor cursor) = 0;

    /// Access the platform's input event queue.
    /// Filled during poll_events(), consumed by InputRouter.
    virtual InputQueue& input_queue() = 0;
};

/// Create the default GLFW-based platform
Platform* create_glfw_platform();

} // namespace ugui
