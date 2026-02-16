/// libultragui showcase - demonstrates multiple visual identities.
///
/// Usage:
///   ./ultragui_showcase [scene] [font_path]
///
/// Scenes: dashboard (default), neon, glass, terminal, rpg, nerv
/// Press 1-6 to switch scenes at runtime.

#include <ultragui/ultragui.h>
#include <ultragui/widgets/button.h>

#include <algorithm>
#include <cmath>
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
     ugui::Color::FromHex(0x0a0a1a)},
    {"neon", "scenes/neon.ugui", "scenes/neon.lua",
     ugui::Color::FromHex(0x000000)},
    {"glass", "scenes/glass.ugui", "scenes/glass.lua",
     ugui::Color::FromHex(0x0d0d14)},
    {"terminal", "scenes/terminal.ugui", "scenes/terminal.lua",
     ugui::Color::FromHex(0x000000)},
    {"rpg", "scenes/rpg_menu.ugui", "scenes/rpg_menu.lua",
     ugui::Color::FromHex(0x000000)},
    {"nerv", "scenes/nerv.ugui", "scenes/nerv.lua",
     ugui::Color::FromHex(0x050510)},
};
static constexpr int SCENE_COUNT = 6;
static constexpr int NERV_SCENE = 5;

static int current_scene = 0;
static bool scene_dirty = true;
static bool key_was_down[16] = {};

// --- NERV render-to-texture state ---
static ugui::RHITextureHandle nerv_target = ugui::kInvalidTexture;
static constexpr ugui::u32 NERV_W = 640;
static constexpr ugui::u32 NERV_H = 400;

static void nerv_setup(ugui::UIContext& ui) {
    nerv_target = ui.CreateRenderTarget(NERV_W, NERV_H);
    if (nerv_target == ugui::kInvalidTexture) {
        std::fprintf(stderr, "Failed to create NERV render target\n");
        return;
    }

    std::printf("NERV: render target %u (%ux%u)\n", nerv_target, NERV_W, NERV_H);
    ui.SetOnPaint([](ugui::Renderer2D& r, ugui::RHI* rhi) {
        ugui::Vec2 ds = rhi->display_size();
        ugui::f32 cx = ds.x * 0.5f;
        ugui::f32 cy = ds.y * 0.5f;
        ugui::f32 hw = NERV_W * 0.5f;
        ugui::f32 hh = NERV_H * 0.5f;

        // 3D perspective transform - tilt the monitor
        float angle_y = 0.35f;  // ~20 deg around Y axis
        float angle_x = -0.12f; // slight tilt back
        float focal = 900.0f;
        float cy_sin = std::sin(angle_y), cy_cos = std::cos(angle_y);
        float cx_sin = std::sin(angle_x), cx_cos = std::cos(angle_x);

        // Project a 3D corner to 2D screen coords
        auto project = [&](float lx, float ly) -> std::pair<float, float> {
            // Rotate around Y
            float rx = lx * cy_cos;
            float rz = -lx * cy_sin;
            // Rotate around X
            float ry = ly * cx_cos - rz * cx_sin;
            float rz2 = ly * cx_sin + rz * cx_cos;
            // Perspective divide
            float s = focal / (focal + rz2);
            return {cx + rx * s, cy + ry * s};
        };

        // Quad corners: TL, TR, BR, BL
        auto [tlx, tly] = project(-hw, -hh);
        auto [trx, try_] = project( hw, -hh);
        auto [brx, bry] = project( hw,  hh);
        auto [blx, bly] = project(-hw,  hh);

        // Ambient glow - use the projected bounding box
        float gx = std::min({tlx, trx, brx, blx}) - 30.0f;
        float gy = std::min({tly, try_, bry, bly}) - 30.0f;
        float gw = std::max({tlx, trx, brx, blx}) - gx + 60.0f;
        float gh = std::max({tly, try_, bry, bly}) - gy + 60.0f;
        r.DrawShadow({gx, gy, gw, gh}, ugui::Color::FromHex(0xff6600, 0.08f),
                      50.0f, 0.0f, {0, 0}, ugui::Vertex2D::PackRadii(12, 12, 12, 12));

        // Build a perspective-projected textured quad using raw vertices.
        // half_size = {0,0} bypasses the SDF rounded-rect logic in the shader,
        // giving us pure texture * color output.
        ugui::u32 white = ugui::Vertex2D::PackColor(1, 1, 1, 1);
        ugui::Vertex2D verts[4] = {
            {{tlx, tly}, {0, 0}, white, white, 0, 0, {0, 0}, 0, 0},
            {{trx, try_}, {1, 0}, white, white, 0, 0, {0, 0}, 0, 0},
            {{brx, bry}, {1, 1}, white, white, 0, 0, {0, 0}, 0, 0},
            {{blx, bly}, {0, 1}, white, white, 0, 0, {0, 0}, 0, 0},
        };
        ugui::u32 indices[6] = {0, 1, 2, 0, 2, 3};

        rhi->DrawTriangles(verts, 4, indices, 6, nerv_target);
    });
}

static void nerv_teardown(ugui::UIContext& ui) {
    ui.SetOnPaint(nullptr);
    if (nerv_target != ugui::kInvalidTexture) {
        ui.rhi()->DestroyRenderTarget(nerv_target);
        nerv_target = ugui::kInvalidTexture;
    }
}

static void load_scene(ugui::UIContext& ui, int idx) {
    // Tear down NERV state if switching away
    if (nerv_target != ugui::kInvalidTexture)
        nerv_teardown(ui);

    auto& s = scenes[idx];
    std::string base = ULTRAGUI_SHOWCASE_DIR;
    std::string ugui_path = base + "/" + s.ugui_file;
    std::string lua_path = base + "/" + s.lua_file;

    if (!ui.LoadUi(ugui_path.c_str())) {
        std::fprintf(stderr, "Failed to load scene '%s'\n", s.name);
        return;
    }
    ui.LoadScript(lua_path.c_str());

    // Wire click handler
    ui.input().set_on_click([&ui](ugui::Widget* widget, ugui::MouseButton btn) {
        if (btn != ugui::MouseButton::kLeft || widget->name().empty())
            return;
        std::string handler = "on_" + widget->name();
        ui.lua().CallHandler(handler.c_str(), widget);
    });

    // Set up NERV render-to-texture if this is the NERV scene
    if (idx == NERV_SCENE)
        nerv_setup(ui);

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
                     "  Usage: %s [dashboard|neon|glass|terminal|rpg|nerv] [font.ttf]\n",
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
    if (!ui.Init(config)) {
        std::fprintf(stderr, "Failed to initialize\n");
        return 1;
    }

    auto font = ui.LoadFont(font_path);
    if (font == ugui::kInvalidFont) {
        ui.Shutdown();
        return 1;
    }
    ui.set_default_font(font);

    auto* window = static_cast<GLFWwindow*>(ui.platform()->native_handle());
    std::printf("Press 1-6 to switch scenes: dashboard, neon, glass, terminal, rpg, nerv\n");

    while (ui.Running()) {
        // Poll scene-switch keys (1-6) without hijacking GLFW callbacks
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
            ui.set_clear_color(scenes[current_scene].clear_color);
            load_scene(ui, current_scene);
        }

        // Queue NERV offscreen render before update
        if (current_scene == NERV_SCENE && nerv_target != ugui::kInvalidTexture) {
            ui.QueueOffscreen(nerv_target, ui.root(),
                               ugui::Color::FromHex(0x0a0a18));
        }

        ui.Update();
    }

    ui.Shutdown();
    return 0;
}
