#ifndef ULTRAGUI_UI_CONTEXT_H_
#define ULTRAGUI_UI_CONTEXT_H_

#include <ugui/animation/animator.h>
#include <ugui/layout/layout_tree.h>
#include <ugui/render/paint.h>
#include <ugui/ultragui_config.h>
#include <ugui/widgets/widget_tree.h>
#if ULTRAGUI_AUDIO
#include <ugui/audio/audio_backend.h>
#endif
#include <ugui/anim/vector_animation.h>
#if ULTRAGUI_LOTTIE
#include <ugui/lottie/lottie.h>
#endif
#if ULTRAGUI_VIDEO
#include <ugui/video/video.h>
#endif
#include <ugui/core/color.h>
#include <ugui/idl/builder.h>
#include <ugui/idl/parser.h>
#include <ugui/input/input.h>
#include <ugui/layout/layout.h>
#include <ugui/platform/platform.h>
#include <ugui/render/renderer2d.h>
#include <ugui/rhi/rhi.h>
#include <ugui/scripting/script_runtime.h>
#include <ugui/style/theme.h>
#include <ugui/svg/svg.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

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
  const char* shader_dir = nullptr;  // Falls back to ULTRAGUI_SHADER_DIR

  /// Viewport scaling mode. When set to anything other than kNone, all pixel
  /// sizes (font-size, border, corner-radius, shadow, padding, margin, gap,
  /// and fixed-px dimensions) scale proportionally as the window is resized.
  /// The design_width/design_height define the reference resolution at which
  /// the scale factor is 1.0. Behaves like CSS rem-based responsive design.
  ViewportScaleMode scale_mode = ViewportScaleMode::kNone;
  f32 design_width = 1280.0f;
  f32 design_height = 720.0f;

  /// Embedding: attach to a host-created window (GLFWwindow*) instead of
  /// creating one, and run the RHI in embedded mode (host clears and presents).
  /// Set both to drop ultragui on top of an application's own render pipeline.
  /// See examples/embed_gl for a worked example. (OpenGL backend.)
  void* external_window = nullptr;
  bool embedded = false;

  /// Draw-data mode (Dear ImGui style): ultragui creates NO graphics device.
  /// Each frame, call RenderDrawData() to get a renderer-agnostic draw list
  /// that your own backend renders (see ugui_impl_vulkan.h / examples/
  /// embed_vulkan). Requires external_window for input/timing. The host owns
  /// the GPU entirely and uploads the glyph atlas (TextEngine::atlas_pixels()).
  bool draw_data = false;
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

  /// Load a .ugui layout file. Builds the widget tree. Returns the root entity.
  wid LoadUi(const char* path);

  /// Load a .ugui layout from a string.
  wid LoadUiString(const char* source, const char* name = "inline");

  /// Load and execute a script file. No-op if no scripting runtime is set.
  bool LoadScript(const char* path);

  /// Execute a script string. No-op if no scripting runtime is set.
  bool ExecScript(const char* script, const char* name = "chunk");

  /// Get the scripting runtime.
  ScriptRuntime& script() { return script_; }

  /// Set the root widget directly (takes ownership for painting, not deletion).
  void set_root(wid root);

  /// Get the current root entity.
  wid root() const { return root_; }

  /// Returns true while the window is open.
  bool Running() const;

  /// Drain the OS input queue and dispatch click / hover / drag /
  /// keyboard events through the widget tree. Idempotent within a
  /// frame: a flag prevents double-processing if Update() is called
  /// later in the same frame, and Update() will auto-call PumpInput
  /// if the application hasn't already.
  ///
  /// Why expose this separately? Applications that rebuild their
  /// widget tree on a dirty flag (like the xeed editor) want click
  /// handlers to fire BEFORE the rebuild check, so the rebuild can
  /// happen in the same frame as the click. Calling PumpInput at the
  /// start of the application's render function and Update later
  /// gives same-frame click->rebuild->render latency. Otherwise the
  /// click handler runs inside Update, sets the dirty flag too late,
  /// and the rebuild only happens on the NEXT frame.
  void PumpInput();

  /// Run one frame: poll input (if not already pumped), update
  /// animations, compute layout, paint.
  void Update();

  /// Draw-data mode only (UIConfig::draw_data): poll input, update, compute
  /// layout, and paint into a renderer-agnostic draw list, returning it WITHOUT
  /// touching any GPU. Your backend (e.g. ugui_impl_vulkan) renders the result
  /// into your own command buffer. The returned reference is valid until the
  /// next RenderDrawData() call. Upload the glyph atlas from text_engine()
  /// when its atlas_revision() changes.
  const DrawData& RenderDrawData();

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
  /// The active audio backend. Always valid; a silent NullAudioBackend until a
  /// real backend is wired via set_audio().
  AudioBackend& audio() { return *audio_; }

  /// Wire in an audio backend (e.g. ugui::AudioEngine from
  /// ugui/backends/ugui_impl_miniaudio). Call before Init(); UIContext will
  /// Init()/Shutdown() it but never deletes it (the caller owns it).
  void set_audio(AudioBackend* backend);
#endif

  /// Get current time in seconds.
  f64 time() const;

  /// Get delta time since last frame.
  f64 delta_time() const { return dt_; }

  /// Set the swapchain clear color (background).
  void set_clear_color(Color color) { config_.clear_color = color; }

  /// Find a widget by name (O(1) cached lookup). Returns a stable handle;
  /// resolve it via widgets().Get(id) right before use. Prefer this over a raw
  /// pointer so a stale reference safely becomes null after a tree rebuild.
  WidgetId FindWidget(const char* name) const;

  /// The entity-and-component world. widgets().Get<C>(e) resolves a component
  /// on entity e (or nullptr); widgets().Alive(e) checks liveness.
  WidgetRegistry& widgets() { return widget_registry_; }

  /// The entity-and-component world (same object as widgets()). Attach custom
  /// components to any widget: ui.world().Add<MyComponent>(id, {...}).
  World& world() { return widget_registry_; }

  /// Invalidate the widget name cache (call after dynamically adding children).
  void InvalidateWidgetCache() { widget_cache_dirty_ = true; }

  /// Load an SVG file and create a GPU texture.
  /// If width/height are 0, uses the SVG's native dimensions.
  RHITextureHandle LoadSvg(const char* path, u32 width = 0, u32 height = 0);

  /// Load a .uganim vector animation. The returned animation is owned by
  /// UIContext and automatically updated each frame. Returns nullptr on
  /// failure.
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
  void QueueOffscreen(RHITextureHandle target, wid root, Color clear_color);

  // --- Overlay system ---

  /// Show a widget as an overlay at the given screen position.
  /// The widget floats above the normal widget tree.
  void ShowOverlay(wid widget, Vec2 position);

  /// Hide a previously shown overlay widget.
  void HideOverlay(wid widget);

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
  NullAudioBackend null_audio_;         // silent default
  AudioBackend* audio_ = &null_audio_;  // wired via set_audio(); not owned
#endif
  Vector<VectorAnimation*> vector_anims_;
#if ULTRAGUI_LOTTIE
  Vector<LottieAnimation*> lottie_anims_;
#endif
#if ULTRAGUI_VIDEO
  Vector<VideoPlayer*> video_players_;
#endif

  wid root_;
  FontHandle default_font_ = kInvalidFont;
  UIConfig config_;
  WidgetRegistry widget_registry_;
  // Makes widget_registry_ the active registry for this thread for the entire
  // lifetime of the context, so every widget (built from IDL, created lazily,
  // or by the application) registers into it and tree links resolve correctly.
  WidgetRegistry::ScopedActive registry_scope_{&widget_registry_};
  WidgetContext widget_ctx_;

  /// Resolve a cached widget name to a live entity (transient, internal use).
  wid FindWidgetEntity(const char* name) const;

  f64 last_time_ = 0.0;
  f64 dt_ = 0.0;

  Vector<LayoutNode> layout_nodes_;
  bool owns_root_ = false;  // true if root was created by load_ui
  bool initialized_ = false;
  // Flips true on PumpInput, false at the end of Update. Lets the
  // application call PumpInput early in its frame for same-frame
  // click->rebuild latency without double-processing input.
  bool input_pumped_this_frame_ = false;
  String current_theme_name_;

  struct OffscreenPass {
    RHITextureHandle target;
    wid root;
    Color clear_color;
  };
  Vector<OffscreenPass> offscreen_queue_;
  PaintCallback on_paint_cb_;

  struct OverlayEntry {
    wid widget;
    Vec2 position;
  };
  Vector<OverlayEntry> overlays_;

  // Widget name -> entity cache (O(1) lookup, rebuilt lazily).
  mutable HashMap<String, wid> widget_cache_;
  mutable bool widget_cache_dirty_ = true;
  void RebuildWidgetCache() const;
  static void CacheWidgetTree(wid w, HashMap<String, wid>& cache);

  // Tooltip state
  wid tooltip_target_;
  bool tooltip_visible_ = false;
  f64 tooltip_hover_start_ = 0.0;
  static constexpr f64 kTooltipDelay = 0.5;

  void UpdateTooltip();
  void DrawTooltip();
};

}  // namespace ugui

#endif  // ULTRAGUI_UI_CONTEXT_H_
