/// libultragui showcase - a beautiful dashboard UI demonstrating the framework.
///
/// Usage:
///   ./ultragui_showcase [font_path]
///
/// If no font path is given, it tries common locations for Inter or other modern fonts.

#include <ultragui/ultragui.h>
#include <ultragui/widgets/button.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static const char* find_font() {
    static std::string found_path;

    // Check env var first
    const char* env_font = std::getenv("ULTRAGUI_FONT");
    if (env_font && fs::exists(env_font))
        return env_font;

    // Use fc-match to find the best sans-serif font (works on NixOS and all Linux).
    // fc-match does proper font matching - much better than scanning fc-list.
    FILE* pipe = popen("fc-match -f '%{file}' 'sans:style=Regular' 2>/dev/null", "r");
    if (pipe) {
        char line[1024];
        if (std::fgets(line, sizeof(line), pipe)) {
            // Strip trailing newline
            std::string path(line);
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
                path.pop_back();
            if (!path.empty() && fs::exists(path)) {
                pclose(pipe);
                found_path = path;
                return found_path.c_str();
            }
        }
        pclose(pipe);
    }

    // Fallback: try specific known paths
    static const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    };

    for (auto* path : candidates) {
        if (fs::exists(path))
            return path;
    }

    return nullptr;
}

int main(int argc, char* argv[]) {
    // Find font
    const char* font_path = argc > 1 ? argv[1] : find_font();
    if (!font_path) {
        std::fprintf(stderr,
                     "No font found. Provide a TTF path as argument or set ULTRAGUI_FONT env var.\n"
                     "  Usage: %s [path/to/font.ttf]\n",
                     argv[0]);
        return 1;
    }
    std::printf("Using font: %s\n", font_path);

    // Initialize framework
    ugui::UIConfig config;
    config.title = "ultragui showcase";
    config.width = 1280;
    config.height = 800;
    config.clear_color = ugui::Color::from_hex(0x0f0f1a);
    config.shader_dir = ULTRAGUI_SHADER_DIR;

    ugui::UIContext ui;
    if (!ui.init(config)) {
        std::fprintf(stderr, "Failed to initialize ultragui\n");
        return 1;
    }

    // Load font
    auto font = ui.load_font(font_path);
    if (font == ugui::INVALID_FONT) {
        std::fprintf(stderr, "Failed to load font: %s\n", font_path);
        ui.shutdown();
        return 1;
    }
    ui.set_default_font(font);

    // Load UI layout
    if (!ui.load_ui(ULTRAGUI_SHOWCASE_DIR "/ui.ugui")) {
        std::fprintf(stderr, "Failed to load UI layout\n");
        ui.shutdown();
        return 1;
    }

    // Wire up button click handlers via Lua
    ui.load_script(ULTRAGUI_SHOWCASE_DIR "/logic.lua");

    // Set up click handler to call Lua functions by widget name
    ui.input().set_on_click([&ui](ugui::Widget* widget, ugui::MouseButton btn) {
        if (btn != ugui::MouseButton::Left)
            return;
        if (widget->name().empty())
            return;

        std::string handler = "on_" + widget->name();
        ui.lua().call_handler(handler.c_str(), widget);
    });

    std::printf("ultragui showcase running (%dx%d)\n", config.width, config.height);

    // Main loop
    while (ui.running()) {
        ui.update();
    }

    ui.shutdown();
    return 0;
}
