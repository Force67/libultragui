#ifndef ULTRAGUI_PLATFORM_PLATFORM_H_
#define ULTRAGUI_PLATFORM_PLATFORM_H_

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>
#include <ultragui/input/input_queue.h>
#include <ultragui/style/enums.h>

namespace ugui {

/// Concrete platform class with link-time swappable implementation.
/// The default implementation uses GLFW; swap the .cc file via CMake
/// (ULTRAGUI_PLATFORM_SOURCE) to provide your own backend.
class Platform {
 public:
  Platform();
  ~Platform();
  Platform(const Platform&) = delete;
  Platform& operator=(const Platform&) = delete;

  struct WindowConfig {
    i32 width = 1280;
    i32 height = 720;
    const char* title = "ultragui";
    bool resizable = true;
    bool vsync = true;
    bool opengl =
        false;  ///< Create OpenGL 3.3 core context instead of no-API window
    /// Attach to a host-created window (GLFWwindow*) instead of creating one.
    /// When set, Platform does not init/terminate GLFW or create/destroy the
    /// window; the host owns its lifetime. Used for embedding ultragui in an
    /// application that already owns its window and GPU context.
    void* external_window = nullptr;
  };

  bool Init(const WindowConfig& config);
  void Shutdown();

  bool ShouldClose() const;
  void PollEvents();
  Vec2 window_size() const;
  Vec2 framebuffer_size() const;
  f32 dpi_scale() const;
  f64 time() const;

  /// Returns native window handle for RHI backend initialization
  void* native_handle() const;

  /// Set the mouse cursor style
  void SetCursor(Cursor cursor);

  /// Clipboard access
  const char* clipboard_text() const;
  void set_clipboard_text(const char* text);

  /// Open a URL in the system's default browser. A default implementation is
  /// compiled when the ULTRAGUI_DEFAULT_OPEN_URL CMake option is ON (default);
  /// turn it off to link your own ugui::Platform::OpenURL.
  static void OpenURL(const char* url);

  /// Access the platform's input event queue.
  /// Filled during PollEvents(), consumed by InputRouter.
  InputQueue& input_queue();

  struct Impl;

 private:
  Impl* impl_;
};

}  // namespace ugui

#endif  // ULTRAGUI_PLATFORM_PLATFORM_H_
