#ifndef ULTRAGUI_UI_CONTEXT_H_
#define ULTRAGUI_UI_CONTEXT_H_

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
#if ULTRAGUI_VIDEO
#include <ultragui/video/video.h>
#endif
#include <ultragui/core/color.h>
#include <ultragui/idl/builder.h>
#include <ultragui/idl/parser.h>
#include <ultragui/input/input.h>
#include <ultragui/layout/layout.h>
#include <ultragui/platform/platform.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/rhi/rhi.h>
#include <ultragui/scripting/script_runtime.h>
#include <ultragui/text/text_engine.h>
#include <ultragui/style/theme.h>
#include <ultragui/svg/svg.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

/// Configuration for UIContext initialization.
struct UIConfig {
    const char* title = "ultragui";
    i32 width = 1280;
    i32 height = 720;
    bool resizable = true;
    bool vsync = true;
    bool validation = true;
    Color clear_color = Color::FromHex(0x0f0f1a);
    const char* shader_dir = nullptr; // Falls back to ULTRAGUI_SHADER_DIR

    /// Viewport scaling mode. When set to anything other than kNone, all pixel
    /// sizes (font-size, border, corner-radius, shadow, padding, margin, gap,
    /// and fixed-px dimensions) scale proportionally as the window is resized.
    /// The design_width/design_height define the reference resolution at which
    /// the scale factor is 1.0. Behaves like CSS rem-based responsive design.
    ViewportScaleMode scale_mode = ViewportScaleMode::kNone;
    f32 design_width = 1280.0f;
    f32 design_height = 720.0f;
};

/// High-level framework context. Ties together all subsystems
/// (platform, GPU, renderer, text, layout, input, animation, scripting)
/// into a single easy-to-use API.
///
/// Usage:
///   UIContext ui;
///   ui.Init(config);
///   ui.LoadFont("path/to/font.ttf");
///   ui.LoadUi("path/to/ui.ugui");
///   ui.LoadScript("path/to/logic.lua");
///   while (ui.Running()) { ui.Update(); }
///   ui.Shutdown();
class UIContext {
public:
    UIContext() = default;
    ~UIContext();

    /// Initialize all subsystems. Returns false on failure.
    bool Init(const UIConfig& config = {});

    /// Load a TTF/OTF font. Returns a handle, or kInvalidFont on failure.
    FontHandle LoadFont(const char* path);

    /// Set the default font used by the builder for text/button widgets.
    void set_default_font(FontHandle font);

    /// Load a .ugui layout file. Builds the widget tree. Returns the root widget.
    Widget* LoadUi(const char* path);

    /// Load a .ugui layout from a string.
    Widget* LoadUiString(const char* source, const char* name = "inline");

    /// Load and execute a script file. No-op if no scripting runtime is set.
    bool LoadScript(const char* path);

    /// Execute a script string. No-op if no scripting runtime is set.
    bool ExecScript(const char* script, const char* name = "chunk");

    /// Get the scripting runtime.
    ScriptRuntime& script() { return script_; }

    /// Set the root widget directly (takes ownership for painting, not deletion).
    void set_root(Widget* root);

    /// Get the current root widget.
    Widget* root() const { return root_; }

    /// Returns true while the window is open.
    bool Running() const;

    /// Run one frame: poll input, update animations, compute layout, paint.
    void Update();

    /// Clean up all subsystems.
    void Shutdown();

    // --- Access subsystems ---
    Platform* platform() { return &platform_; }
    RHI* rhi() { return &rhi_; }
    Renderer2D& renderer() { return renderer_; }
    TextEngine& text_engine() { return text_engine_; }
    InputRouter& input() { return input_; }
    Animator& animator() { return animator_; }
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
    Widget* FindWidget(const char* name) const;

    /// Load an SVG file and create a GPU texture.
    /// If width/height are 0, uses the SVG's native dimensions.
    RHITextureHandle LoadSvg(const char* path, u32 width = 0, u32 height = 0);

    /// Load a .uganim vector animation. The returned animation is owned by
    /// UIContext and automatically updated each frame. Returns nullptr on failure.
    VectorAnimation* LoadAnim(const char* path, u32 width, u32 height);

#if ULTRAGUI_LOTTIE
    /// Load a Lottie animation. The returned animation is owned by UIContext
    /// and automatically updated each frame. Returns nullptr on failure.
    LottieAnimation* LoadLottie(const char* path, u32 width, u32 height);
#endif

#if ULTRAGUI_VIDEO
    /// Load an MPEG-1 video. The returned player is owned by UIContext and
    /// automatically updated each frame. Returns nullptr on failure.
    /// If ULTRAGUI_AUDIO is enabled, audio from the video will play through
    /// a dedicated miniaudio device.
    VideoPlayer* LoadVideo(const char* path);
#endif

    /// Create an offscreen render target (delegates to RHI).
    RHITextureHandle CreateRenderTarget(u32 width, u32 height);

    /// Queue a widget tree to be rendered to an offscreen target during the
    /// next Update() call, before the main swapchain pass.
    void QueueOffscreen(RHITextureHandle target, Widget* root, Color clear_color);

    // --- Overlay system ---

    /// Show a widget as an overlay at the given screen position.
    /// The widget floats above the normal widget tree.
    void ShowOverlay(Widget* widget, Vec2 position);

    /// Hide a previously shown overlay widget.
    void HideOverlay(Widget* widget);

    /// Custom paint callback for the swapchain pass. When set, replaces the
    /// default compute_layout() + paint_tree() with the callback.
    using PaintCallback = Function<void(Renderer2D&, RHI*)>;
    void SetOnPaint(PaintCallback cb);

    /// Apply a theme -- sets all theme tokens as CSS variables on the builder.
    /// Variables take effect on the next LoadUi/LoadUiString call.
    void SetTheme(const Theme& theme);

    /// Get the current theme name (empty if no theme has been applied).
    const String& theme_name() const { return current_theme_name_; }

private:
    Platform platform_;
    RHI rhi_;
    Renderer2D renderer_;
    TextEngine text_engine_;
    LayoutEngine layout_engine_;
    InputRouter input_;
    Animator animator_;
    ScriptRuntime script_;
    UguiBuilder builder_;
#if ULTRAGUI_AUDIO
    AudioEngine audio_;
#endif
    Vector<VectorAnimation*> vector_anims_;
#if ULTRAGUI_LOTTIE
    Vector<LottieAnimation*> lottie_anims_;
#endif
#if ULTRAGUI_VIDEO
    Vector<VideoPlayer*> video_players_;
#endif

    Widget* root_ = nullptr;
    FontHandle default_font_ = kInvalidFont;
    UIConfig config_;
    WidgetContext widget_ctx_;

    f64 last_time_ = 0.0;
    f64 dt_ = 0.0;

    Vector<LayoutNode> layout_nodes_;
    bool owns_root_ = false; // true if root was created by load_ui
    bool initialized_ = false;
    String current_theme_name_;

    struct OffscreenPass {
        RHITextureHandle target;
        Widget* root;
        Color clear_color;
    };
    Vector<OffscreenPass> offscreen_queue_;
    PaintCallback on_paint_cb_;

    struct OverlayEntry {
        Widget* widget = nullptr;
        Vec2 position;
    };
    Vector<OverlayEntry> overlays_;

    // Tooltip state
    Widget* tooltip_target_ = nullptr;
    bool tooltip_visible_ = false;
    f64 tooltip_hover_start_ = 0.0;
    static constexpr f64 kTooltipDelay = 0.5;

    void UpdateTooltip();
    void DrawTooltip();
};

} // namespace ugui

#endif  // ULTRAGUI_UI_CONTEXT_H_
