/// Minimal demo: opens a window via the GLFW platform layer.
/// Later phases will add rendering, widgets, etc.

#include <ultragui/ultragui.h>

#include <cstdio>

int main() {
    auto* platform = ugui::create_glfw_platform();

    ugui::Platform::WindowConfig cfg;
    cfg.width = 1280;
    cfg.height = 720;
    cfg.title = "libultragui demo";

    if (!platform->init(cfg)) {
        std::fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }

    std::printf("libultragui demo — window opened (%.0fx%.0f @ %.1fx DPI)\n",
                platform->window_size().x, platform->window_size().y, platform->dpi_scale());

    while (!platform->should_close()) {
        platform->poll_events();
        // Future: context.begin_frame() -> build UI -> render -> present
    }

    platform->shutdown();
    delete platform;
    return 0;
}
