#include <ultragui/platform/platform.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cassert>
#include <cstdio>

namespace ugui {

class GlfwPlatform : public Platform {
public:
    bool init(const WindowConfig& config) override {
        if (!glfwInit()) {
            std::fprintf(stderr, "ultragui: glfwInit() failed\n");
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

        window_ = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);
        if (!window_) {
            std::fprintf(stderr, "ultragui: glfwCreateWindow() failed\n");
            glfwTerminate();
            return false;
        }

        return true;
    }

    void shutdown() override {
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        glfwTerminate();
    }

    bool should_close() const override { return glfwWindowShouldClose(window_); }

    void poll_events() override { glfwPollEvents(); }

    Vec2 window_size() const override {
        int w, h;
        glfwGetWindowSize(window_, &w, &h);
        return {static_cast<f32>(w), static_cast<f32>(h)};
    }

    Vec2 framebuffer_size() const override {
        int w, h;
        glfwGetFramebufferSize(window_, &w, &h);
        return {static_cast<f32>(w), static_cast<f32>(h)};
    }

    f32 dpi_scale() const override {
        float xscale, yscale;
        glfwGetWindowContentScale(window_, &xscale, &yscale);
        return xscale;
    }

    f64 time() const override { return glfwGetTime(); }

    void* native_handle() const override { return window_; }

    void set_cursor(Cursor cursor) override {
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
            glfwSetCursor(window_, cursors[idx]);
        else
            glfwSetCursor(window_, nullptr); // default
    }

private:
    GLFWwindow* window_ = nullptr;
};

Platform* create_glfw_platform() {
    return new GlfwPlatform();
}

} // namespace ugui
