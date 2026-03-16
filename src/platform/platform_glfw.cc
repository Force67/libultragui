#include <ultragui/platform/platform.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cassert>
#include <cstdio>

namespace ugui {

struct Platform::Impl {
  GLFWwindow* window = nullptr;
  bool external = false;  // window owned by the host (embedding)
  InputQueue queue;

  static void glfw_mouse_pos_cb(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Impl*>(glfwGetWindowUserPointer(window));
    self->queue.PushMove({static_cast<f32>(x), static_cast<f32>(y)});
  }

  static void glfw_mouse_button_cb(GLFWwindow* window, int button, int action,
                                   int) {
    auto* self = static_cast<Impl*>(glfwGetWindowUserPointer(window));
    self->queue.PushButton(static_cast<MouseButton>(button),
                           action == GLFW_PRESS);
  }

  static void glfw_scroll_cb(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Impl*>(glfwGetWindowUserPointer(window));
    self->queue.PushScroll({static_cast<f32>(x), static_cast<f32>(y)});
  }

  static void glfw_key_cb(GLFWwindow* window, int key, int scancode, int action,
                          int mods) {
    auto* self = static_cast<Impl*>(glfwGetWindowUserPointer(window));
    self->queue.PushKey(key, scancode, action == GLFW_PRESS,
                        action == GLFW_REPEAT, mods);
  }

  static void glfw_char_cb(GLFWwindow* window, unsigned int codepoint) {
    auto* self = static_cast<Impl*>(glfwGetWindowUserPointer(window));
    self->queue.PushChar(codepoint);
  }
};

Platform::Platform() : impl_(new Impl()) {}
Platform::~Platform() { delete impl_; }

bool Platform::Init(const WindowConfig& config) {
  if (config.external_window) {
    // Embedding: the host already created and owns the window/context.
    impl_->window = static_cast<GLFWwindow*>(config.external_window);
    impl_->external = true;
  } else {
    if (!glfwInit()) {
      std::fprintf(stderr, "ultragui: glfwInit() failed\n");
      return false;
    }

    if (config.opengl) {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
      glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    } else {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    impl_->window = glfwCreateWindow(config.width, config.height, config.title,
                                     nullptr, nullptr);
    if (!impl_->window) {
      std::fprintf(stderr, "ultragui: glfwCreateWindow() failed\n");
      glfwTerminate();
      return false;
    }
  }

  // Install input callbacks
  glfwSetWindowUserPointer(impl_->window, impl_);
  glfwSetCursorPosCallback(impl_->window, Impl::glfw_mouse_pos_cb);
  glfwSetMouseButtonCallback(impl_->window, Impl::glfw_mouse_button_cb);
  glfwSetScrollCallback(impl_->window, Impl::glfw_scroll_cb);
  glfwSetKeyCallback(impl_->window, Impl::glfw_key_cb);
  glfwSetCharCallback(impl_->window, Impl::glfw_char_cb);

  return true;
}

void Platform::Shutdown() {
  // When embedding, the host owns the window and GLFW lifecycle.
  if (impl_->external) {
    impl_->window = nullptr;
    return;
  }
  if (impl_->window) {
    glfwDestroyWindow(impl_->window);
    impl_->window = nullptr;
  }
  glfwTerminate();
}

bool Platform::ShouldClose() const {
  return glfwWindowShouldClose(impl_->window);
}

void Platform::PollEvents() {
  impl_->queue.clear();
  glfwPollEvents();
}

Vec2 Platform::window_size() const {
  int w, h;
  glfwGetWindowSize(impl_->window, &w, &h);
  return {static_cast<f32>(w), static_cast<f32>(h)};
}

Vec2 Platform::framebuffer_size() const {
  int w, h;
  glfwGetFramebufferSize(impl_->window, &w, &h);
  return {static_cast<f32>(w), static_cast<f32>(h)};
}

f32 Platform::dpi_scale() const {
  float xscale, yscale;
  glfwGetWindowContentScale(impl_->window, &xscale, &yscale);
  return xscale;
}

f64 Platform::time() const { return glfwGetTime(); }

void* Platform::native_handle() const { return impl_->window; }

void Platform::SetCursor(Cursor cursor) {
  // Create cursors on first use
  static GLFWcursor* cursors[6] = {};
  static bool init = false;
  if (!init) {
    cursors[1] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    cursors[2] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    cursors[3] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    cursors[4] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    cursors[5] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
    init = true;
  }
  int idx = static_cast<int>(cursor);
  if (idx > 0 && idx < 6 && cursors[idx])
    glfwSetCursor(impl_->window, cursors[idx]);
  else
    glfwSetCursor(impl_->window, nullptr);  // default
}

const char* Platform::clipboard_text() const {
  return glfwGetClipboardString(impl_->window);
}

void Platform::set_clipboard_text(const char* text) {
  glfwSetClipboardString(impl_->window, text);
}

InputQueue& Platform::input_queue() { return impl_->queue; }

}  // namespace ugui
