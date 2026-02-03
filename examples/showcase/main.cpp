/// libultragui showcase - demonstrates multiple visual identities.
///
/// Usage:
///   ./ultragui_showcase [scene] [font_path]
///
/// Scenes: dashboard (default), neon, glass, terminal
/// Press 1-4 to switch scenes at runtime.

#include <ultragui/ultragui.h>
#include <ultragui/widgets/button.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace fs = std::filesystem;

static const char* find_font() {
    static std::string found_path;

    const char* env_font = std::getenv("ULTRAGUI_FONT");
    if (env_font && fs::exists(env_font))
        return env_font;

    FILE* pipe = popen("fc-match -f '%{file}' 'sans:style=Regular' 2>/dev/null", "r");
    if (pipe) {
        char line[1024];
        if (std::fgets(line, sizeof(line), pipe)) {
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

struct Scene {
    const char* name;
    const char* ugui_file;
    const char* lua_file;
    ugui::Color clear_color;
};

static const Scene scenes[] = {
    {"dashboard", "scenes/dashboard.ugui", "scenes/dashboard.lua",
     ugui::Color::from_hex(0x0a0a1a)},
    {"neon", "scenes/neon.ugui", "scenes/neon.lua",
     ugui::Color::from_hex(0x000000)},
    {"glass", "scenes/glass.ugui", "scenes/glass.lua",
     ugui::Color::from_hex(0x0d0d14)},
    {"terminal", "scenes/terminal.ugui", "scenes/terminal.lua",
     ugui::Color::from_hex(0x000000)},
    {"rpg", "scenes/rpg_menu.ugui", "scenes/rpg_menu.lua",
     ugui::Color::from_hex(0x000000)},
};
static constexpr int SCENE_COUNT = 5;

static int current_scene = 0;
static bool scene_dirty = true;
static bool key_was_down[4] = {};

static void load_scene(ugui::UIContext& ui, int idx) {
    auto& s = scenes[idx];
    std::string base = ULTRAGUI_SHOWCASE_DIR;
    std::string ugui_path = base + "/" + s.ugui_file;
    std::string lua_path = base + "/" + s.lua_file;

    if (!ui.load_ui(ugui_path.c_str())) {
        std::fprintf(stderr, "Failed to load scene '%s'\n", s.name);
        return;
    }
    ui.load_script(lua_path.c_str());

    // Wire click handler
    ui.input().set_on_click([&ui](ugui::Widget* widget, ugui::MouseButton btn) {
        if (btn != ugui::MouseButton::Left || widget->name().empty())
            return;
        std::string handler = "on_" + widget->name();
        ui.lua().call_handler(handler.c_str(), widget);
    });

    std::printf("Scene: %s\n", s.name);
}

int main(int argc, char* argv[]) {
    // Parse args
    const char* font_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        bool is_scene = false;
        for (int j = 0; j < SCENE_COUNT; ++j) {
            if (std::string(argv[i]) == scenes[j].name) {
                current_scene = j;
                is_scene = true;
                break;
            }
        }
        if (!is_scene)
            font_path = argv[i];
    }

    if (!font_path)
        font_path = find_font();
    if (!font_path) {
        std::fprintf(stderr,
                     "No font found. Set ULTRAGUI_FONT or pass a TTF path.\n"
                     "  Usage: %s [dashboard|neon|glass|terminal] [font.ttf]\n",
                     argv[0]);
        return 1;
    }
    std::printf("Font: %s\n", font_path);

    ugui::UIConfig config;
    config.title = "ultragui showcase";
    config.width = 1280;
    config.height = 800;
    config.clear_color = scenes[current_scene].clear_color;
    config.shader_dir = ULTRAGUI_SHADER_DIR;

    ugui::UIContext ui;
    if (!ui.init(config)) {
        std::fprintf(stderr, "Failed to initialize\n");
        return 1;
    }

    auto font = ui.load_font(font_path);
    if (font == ugui::INVALID_FONT) {
        ui.shutdown();
        return 1;
    }
    ui.set_default_font(font);

    auto* window = static_cast<GLFWwindow*>(ui.platform()->native_handle());
    std::printf("Press 1-5 to switch scenes: dashboard, neon, glass, terminal, rpg\n");

    while (ui.running()) {
        // Poll scene-switch keys (1-4) without hijacking GLFW callbacks
        for (int i = 0; i < SCENE_COUNT; ++i) {
            bool down = glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS;
            if (down && !key_was_down[i] && i != current_scene) {
                current_scene = i;
                scene_dirty = true;
            }
            key_was_down[i] = down;
        }

        if (scene_dirty) {
            scene_dirty = false;
            load_scene(ui, current_scene);
        }
        ui.update();
    }

    ui.shutdown();
    return 0;
}
