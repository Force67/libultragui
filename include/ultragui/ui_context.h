#pragma once

#include <ultragui/animation/animator.h>
#include <ultragui/layout/layout_tree.h>
#include <ultragui/render/paint.h>
#include <ultragui/widgets/widget_tree.h>
#if ULTRAGUI_AUDIO
#include <ultragui/audio/audio.h>
#endif
#include <ultragui/anim/vector_animation.h>
#if ULTRAGUI_LOTTIE
#include <ultragui/lottie/lottie.h>
#endif
#include <ultragui/core/color.h>
#include <ultragui/idl/builder.h>
#include <ultragui/idl/parser.h>
#include <ultragui/input/input.h>
#include <ultragui/layout/layout.h>
#include <ultragui/platform/platform.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/rhi/rhi.h>
#include <ultragui/scripting/lua_runtime.h>
#include <ultragui/text/text_engine.h>
#include <ultragui/svg/svg.h>
#include <ultragui/widgets/widget.h>

#include <functional>
#include <string>
#include <vector>

namespace ugui {

/// Configuration for UIContext initialization.
struct UIConfig {
    const char* title = "ultragui";
    i32 width = 1280;
    i32 height = 720;
    bool resizable = true;
    bool vsync = true;
    bool validation = true;
    Color clear_color = Color::from_hex(0x0f0f1a);
    const char* shader_dir = nullptr; // Falls back to ULTRAGUI_SHADER_DIR
};

/// High-level framework context. Ties together all subsystems
/// (platform, GPU, renderer, text, layout, input, animation, scripting)
/// into a single easy-to-use API.
///
/// Usage:
///   UIContext ui;
///   ui.init(config);
///   ui.load_font("path/to/font.ttf");
///   ui.load_ui("path/to/ui.ugui");
///   ui.load_script("path/to/logic.lua");
///   while (ui.running()) { ui.update(); }
///   ui.shutdown();
class UIContext {
public:
    UIContext() = default;
    ~UIContext();

    /// Initialize all subsystems. Returns false on failure.
    bool init(const UIConfig& config = {});

    /// Load a TTF/OTF font. Returns a handle, or INVALID_FONT on failure.
    FontHandle load_font(const char* path);

    /// Set the default font used by the builder for text/button widgets.
    void set_default_font(FontHandle font);

    /// Load a .ugui layout file. Builds the widget tree. Returns the root widget.
    Widget* load_ui(const char* path);

    /// Load a .ugui layout from a string.
    Widget* load_ui_string(const char* source, const char* name = "inline");

    /// Load and execute a Lua script.
    bool load_script(const char* path);

    /// Execute a Lua script string.
    bool exec_script(const char* script, const char* name = "chunk");

    /// Set the root widget directly (takes ownership for painting, not deletion).
    void set_root(Widget* root);

    /// Get the current root widget.
    Widget* root() const { return root_; }

    /// Returns true while the window is open.
    bool running() const;

    /// Run one frame: poll input, update animations, compute layout, paint.
    void update();

    /// Clean up all subsystems.
    void shutdown();

    // --- Access subsystems ---
    Platform* platform() { return platform_; }
    RHI* rhi() { return rhi_; }
    Renderer2D& renderer() { return renderer_; }
    TextEngine& text_engine() { return text_engine_; }
    InputRouter& input() { return input_; }
    Animator& animator() { return animator_; }
    LuaRuntime& lua() { return lua_; }
    UguiBuilder& builder() { return builder_; }
#if ULTRAGUI_AUDIO
    AudioEngine& audio() { return audio_; }
#endif

    /// Get current time in seconds.
    f64 time() const;

    /// Get delta time since last frame.
    f64 delta_time() const { return dt_; }

    /// Set the swapchain clear color (background).
    void set_clear_color(Color color) { config_.clear_color = color; }

    /// Find a widget by name in the tree.
    Widget* find_widget(const char* name) const;

    /// Load an SVG file and create a GPU texture.
    /// If width/height are 0, uses the SVG's native dimensions.
    RHITextureHandle load_svg(const char* path, u32 width = 0, u32 height = 0);

    /// Load a .uganim vector animation. The returned animation is owned by
    /// UIContext and automatically updated each frame. Returns nullptr on failure.
    VectorAnimation* load_anim(const char* path, u32 width, u32 height);

#if ULTRAGUI_LOTTIE
    /// Load a Lottie animation. The returned animation is owned by UIContext
    /// and automatically updated each frame. Returns nullptr on failure.
    LottieAnimation* load_lottie(const char* path, u32 width, u32 height);
#endif

    /// Create an offscreen render target (delegates to RHI).
    RHITextureHandle create_render_target(u32 width, u32 height);

    /// Queue a widget tree to be rendered to an offscreen target during the
    /// next update() call, before the main swapchain pass.
    void queue_offscreen(RHITextureHandle target, Widget* root, Color clear_color);

    /// Custom paint callback for the swapchain pass. When set, replaces the
    /// default compute_layout() + paint_tree() with the callback.
    using PaintCallback = std::function<void(Renderer2D&, RHI*)>;
    void set_on_paint(PaintCallback cb);

private:
    Platform* platform_ = nullptr;
    RHI* rhi_ = nullptr;
    Renderer2D renderer_;
    TextEngine text_engine_;
    LayoutEngine layout_engine_;
    InputRouter input_;
    Animator animator_;
    LuaRuntime lua_;
    UguiBuilder builder_;
#if ULTRAGUI_AUDIO
    AudioEngine audio_;
#endif
    std::vector<VectorAnimation*> vector_anims_;
#if ULTRAGUI_LOTTIE
    std::vector<LottieAnimation*> lottie_anims_;
#endif

    Widget* root_ = nullptr;
    FontHandle default_font_ = INVALID_FONT;
    UIConfig config_;
    WidgetContext widget_ctx_;

    f64 last_time_ = 0.0;
    f64 dt_ = 0.0;

    std::vector<LayoutNode> layout_nodes_;
    bool owns_root_ = false; // true if root was created by load_ui

    struct OffscreenPass {
        RHITextureHandle target;
        Widget* root;
        Color clear_color;
    };
    std::vector<OffscreenPass> offscreen_queue_;
    PaintCallback on_paint_cb_;
};

} // namespace ugui
