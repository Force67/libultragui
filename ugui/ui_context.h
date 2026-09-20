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
#include <ugui/rhi/rhi_texture_backend.h>
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

  /// Pixel sizes (font, border, padding, ...) scale by viewport_size /
  /// design_size. kNone disables scaling.
  ViewportScaleMode scale_mode = ViewportScaleMode::kNone;
  f32 design_width = 1280.0f;
  f32 design_height = 720.0f;

  /// Attach to a host-created window (GLFWwindow*) instead of creating one.
  /// With embedded=true the host clears and presents. See examples/embed_gl.
  void* external_window = nullptr;
  bool embedded = false;

  /// No graphics device; RenderDrawData() returns a draw list for the host's
  /// backend (see ugui_impl_vulkan.h). Requires external_window. Host owns the
  /// GPU and uploads the glyph atlas (TextEngine::atlas_pixels()).
  bool draw_data = false;
};

/// What RenderDrawData() should do when a frame would come out identical to
/// the last one.
enum class FrameReuse {
  kOff,     ///< always rebuild (the default)
  kOn,      ///< hand back the previous draw list
  kVerify,  ///< rebuild anyway and report any frame reuse would have got wrong
};

/// Where one frame's CPU time went, and how much work each stage did. Filled
/// by RenderDrawData(); a handful of clock reads, so it is always on rather
/// than behind a build flag.
struct FrameStats {
  f64 input_ms = 0.0;
  f64 update_ms = 0.0;  // timers, animation, per-widget update
  f64 measure_ms = 0.0;
  f64 layout_ms = 0.0;
  f64 paint_ms = 0.0;
  f64 total_ms = 0.0;
  u32 widgets = 0;       // widgets walked by measure
  u32 layout_nodes = 0;  // nodes handed to the layout engine
  u32 shape_calls = 0;   // text runs shaped (see TextEngine::shape_calls)
  u32 shape_hits = 0;    // of those, served from the shaping cache
  u32 draw_commands = 0;
  bool reused = false;  ///< the previous draw list was handed back unchanged
};

/// Owns all subsystems and drives one frame of the UI.
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

  /// Load a font from bytes rather than a path, for fonts that ship inside an
  /// archive or the binary. UIContext copies them; the caller's buffer is its
  /// own again on return.
  FontHandle LoadFontMemory(const char* data, usize length);

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

  /// Dispatch input events through the widget tree. Safe to call before
  /// Update() in the same frame: handlers then run before Update, so a
  /// dirty-flag rebuild triggered by a click renders in the same frame.
  void PumpInput();

  /// Run one frame: poll input (if not already pumped), update
  /// animations, compute layout, paint.
  void Update();

  /// Draw-data mode only: full frame into a renderer-agnostic draw list, no
  /// GPU work. Valid until the next call. Upload the glyph atlas when
  /// text_engine().atlas_revision() changes.
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

  /// Override the scale factor applied to all pixel sizes. 0 returns to
  /// scale_mode. Use on high-DPI hosts, where scale_mode would rescale on
  /// every resize; the viewport/desktop unit ratio is constant there.
  void set_ui_scale(f32 scale) { ui_scale_override_ = scale; }
  f32 ui_scale() const { return widget_ctx_.ui_scale; }

  /// Find a widget by name (cached, O(1)). Handles stay valid across tree
  /// rebuilds; resolve via widgets().Get(id) right before use.
  WidgetId FindWidget(const char* name) const;

  /// Resolve a widget id through the same cache (O(1)). The animator holds
  /// ids rather than handles and asks once per running animation per frame,
  /// which a tree search would make O(widgets) each time.
  wid WidgetById(u32 id) const;

  /// Entity registry. Get<C>(e) resolves a component (or nullptr);
  /// Alive(e) checks liveness.
  WidgetRegistry& widgets() { return widget_registry_; }

  /// Same registry as widgets(). Attach custom components:
  /// ui.world().Add<MyComponent>(id, {...}).
  World& world() { return widget_registry_; }

  /// Invalidate the widget name cache (call after dynamically adding children).
  void InvalidateWidgetCache() { widget_cache_dirty_ = true; }

  /// Texture sink used by SVG, Image, gradients, animations. Null in draw-data
  /// mode until the host sets one.
  TextureBackend* texture_backend() { return texture_backend_; }

  /// Draw-data mode: register the host's texture backend before loading any
  /// textured assets. No effect on the font atlas (host-owned).
  void set_texture_backend(TextureBackend* backend);

  /// Load an SVG file and create a GPU texture through the active backend.
  /// If width/height are 0, uses the SVG's native dimensions. Returns
  /// kNullTextureId if no texture backend is set.
  TextureId LoadSvg(const char* path, u32 width = 0, u32 height = 0);

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

  /// Where the last frame's CPU time went. See FrameStats.
  const FrameStats& frame_stats() const { return stats_; }

  /// Skip measure/layout/paint on frames that would redraw the same picture.
  /// Off by default: it trades a rebuild for trusting that everything which
  /// changes what is drawn goes through MarkDirty/MarkPaintDirty. kVerify
  /// rebuilds regardless and reports where that trust was misplaced, which is
  /// how to qualify it on a new UI before turning it on.
  void set_frame_reuse(FrameReuse mode) { frame_reuse_ = mode; }
  FrameReuse frame_reuse() const { return frame_reuse_; }

  /// Frames kVerify found a difference on. Non-zero means reuse is unsafe for
  /// this UI and something it draws from is changing without marking dirty.
  u64 frame_reuse_mismatches() const { return reuse_mismatches_; }
  /// Frames the predicate called a repeat. A mismatch count of zero only means
  /// something if this is not zero too.
  u64 frame_reuse_candidates() const { return reuse_candidates_; }

 private:
  Platform platform_;
  RHI rhi_;
  // Texture seam: legacy mode points texture_backend_ at this RHI adapter;
  // draw-data mode leaves it null until the host calls set_texture_backend().
  RHITextureBackend rhi_texture_backend_{&rhi_};
  TextureBackend* texture_backend_ = nullptr;
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
  f32 ui_scale_override_ = 0.0f;  // set_ui_scale(); 0 defers to scale_mode
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

  FrameStats stats_;
  f64 last_input_ms_ = 0.0;

  // Frame reuse. Everything here is "what the last built frame was built
  // from"; a frame whose inputs all match may hand back that frame's list.
  FrameReuse frame_reuse_ = FrameReuse::kOff;
  bool have_built_frame_ = false;
  u64 last_widget_revision_ = 0;
  u64 last_draw_hash_ = 0;
  u64 reuse_mismatches_ = 0;
  u64 reuse_candidates_ = 0;
  Vec2 last_reuse_viewport_ = {-1.0f, -1.0f};
  f32 last_reuse_scale_ = -1.0f;
  wid last_tooltip_target_;
  bool last_tooltip_visible_ = false;
  Vector<wid> last_overlay_widgets_;
  /// True when nothing that feeds the draw list has moved since it was built.
  bool FrameWouldRepeat() const;
  /// Remember what the frame just built was built from.
  void RecordBuiltFrame(const DrawData& dd);
  bool owns_root_ = false;  // true if root was created by load_ui
  bool initialized_ = false;
  // Flips true on PumpInput, false at the end of Update. Lets the
  // application call PumpInput early in its frame for same-frame
  // click->rebuild latency without double-processing input.
  bool input_pumped_this_frame_ = false;
  // Last viewport @media breakpoints were resolved against; a mismatch in
  // PumpInput triggers UguiBuilder::ReapplyMediaQueries.
  Vec2 media_viewport_ = {-1.0f, -1.0f};
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

  // Widget name/id -> entity caches (O(1) lookup, rebuilt lazily together).
  mutable HashMap<String, wid> widget_cache_;
  mutable HashMap<u32, wid> id_cache_;
  mutable bool widget_cache_dirty_ = true;
  void RebuildWidgetCache() const;
  static void CacheWidgetTree(wid w, HashMap<String, wid>& cache,
                              HashMap<u32, wid>& by_id);

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
