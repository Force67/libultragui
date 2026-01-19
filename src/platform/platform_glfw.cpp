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

private:
    GLFWwindow* window_ = nullptr;
};

Platform* create_glfw_platform() {
    return new GlfwPlatform();
}

} // namespace ugui
