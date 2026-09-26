#include <ugui/scripting/lua_widgets.h>
#include <ugui/core/algorithm.h>
#include <ugui/ui_context.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/text.h>

#if ULTRAGUI_LUA
#include <ugui/scripting/lua_anim.h>
#if ULTRAGUI_AUDIO
#include <ugui/scripting/lua_audio.h>
#endif
#if ULTRAGUI_LOTTIE
#include <ugui/scripting/lua_lottie.h>
#endif
#if ULTRAGUI_VIDEO
#include <ugui/scripting/lua_video.h>
#endif
#endif  // ULTRAGUI_LUA

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace ugui {

static f32 ComputeViewportScale(const UIConfig& cfg, Vec2 display, f32 override_scale) {
  if (override_scale > 0.0f) return override_scale;  // set_ui_scale() wins
  switch (cfg.scale_mode) {
    case ViewportScaleMode::kWidth:
      return (cfg.design_width > 0.0f) ? display.x / cfg.design_width : 1.0f;
    case ViewportScaleMode::kHeight:
      return (cfg.design_height > 0.0f) ? display.y / cfg.design_height : 1.0f;
    case ViewportScaleMode::kContain: {
      f32 sw = (cfg.design_width > 0.0f) ? display.x / cfg.design_width : 1.0f;
      f32 sh =
          (cfg.design_height > 0.0f) ? display.y / cfg.design_height : 1.0f;
      return fminf(sw, sh);
    }
    case ViewportScaleMode::kCover: {
      f32 sw = (cfg.design_width > 0.0f) ? display.x / cfg.design_width : 1.0f;
      f32 sh =
          (cfg.design_height > 0.0f) ? display.y / cfg.design_height : 1.0f;
      return fmaxf(sw, sh);
    }
    default:
      return 1.0f;
  }
}

UIContext::~UIContext() {
  if (initialized_) Shutdown();
}

bool UIContext::Init(const UIConfig& config) {
  config_ = config;

  // The GLFW platform attaches to the host's window; the host platform has
  // none, and is fed by the host instead.
  if (config.draw_data && !config.external_window && !ULTRAGUI_PLATFORM_HOST) {
    fprintf(stderr, "ultragui: draw_data mode requires external_window\n");
    return false;
  }

  // Platform
  Platform::WindowConfig wcfg;
  wcfg.width = config.width;
  wcfg.height = config.height;
  wcfg.title = config.title;
  wcfg.resizable = config.resizable;
  wcfg.vsync = config.vsync;
  wcfg.external_window = config.external_window;
#if ULTRAGUI_BACKEND_OPENGL
  wcfg.opengl = true;
#endif

  if (!platform_.Init(wcfg)) {
    fprintf(stderr, "ultragui: failed to initialize platform\n");
    return false;
  }

  // RHI. Skipped in draw-data mode: the host owns the GPU and renders the
  // draw list returned by RenderDrawData() with its own backend.
  RHI* rhi_ptr = nullptr;
  if (!config.draw_data) {
    RHIConfig rcfg;
    rcfg.platform = &platform_;
    rcfg.validation = config.validation;
    rcfg.vsync = config.vsync;
    rcfg.shader_dir = config.shader_dir;
    rcfg.embedded = config.embedded;

    if (!rhi_.Init(rcfg)) {
      fprintf(stderr, "ultragui: failed to initialize RHI\n");
      platform_.Shutdown();
      return false;
    }
    rhi_ptr = &rhi_;
  }

  // Renderer
  renderer_.Init(rhi_ptr);
  renderer_.set_display_size(
      {static_cast<f32>(config.width), static_cast<f32>(config.height)});

  // Texture seam. Legacy mode renders through the bundled RHI, so wire its
  // adapter as the default sink. Draw-data mode leaves it null; the host
  // registers its own backend via set_texture_backend().
  if (rhi_ptr) set_texture_backend(&rhi_texture_backend_);

  // Text engine
  if (!text_engine_.Init(rhi_ptr)) {
    fprintf(stderr, "ultragui: failed to initialize text engine\n");
  }

  // Input
  input_.Init(&platform_);

  // Scripting runtime
  script_.Init();

#if ULTRAGUI_LUA
  {
#if ULTRAGUI_AUDIO
    RegisterAudioLua(script_, *audio_);
#endif
#if ULTRAGUI_LOTTIE
    RegisterLottieLua(
        script_,
        [this](const char* path, unsigned w, unsigned h) -> LottieAnimation* {
          return LoadLottie(path, w, h);
        },
        [this](const char* name) -> wid { return FindWidgetEntity(name); });
#endif
    RegisterAnimLua(
        script_,
        [this](const char* path, unsigned w, unsigned h) -> VectorAnimation* {
          return LoadAnim(path, w, h);
        },
        [this](const char* name) -> wid { return FindWidgetEntity(name); });
#if ULTRAGUI_VIDEO
    RegisterVideoLua(
        script_,
        [this](const char* path) -> VideoPlayer* { return LoadVideo(path); },
        [this](const char* name) -> wid { return FindWidgetEntity(name); });
#endif
  }
#endif

#if ULTRAGUI_AUDIO
  if (!audio_->Init()) {
    fprintf(stderr, "ultragui: audio init failed (non-fatal)\n");
  }
#endif

  // Widget context
  widget_ctx_.text_engine = &text_engine_;
  widget_ctx_.animator = &animator_;
  widget_ctx_.current_time = &last_time_;
  widget_ctx_.platform = &platform_;
  widget_ctx_.registry = &widget_registry_;
  widget_ctx_.ui_scale = ComputeViewportScale(
      config_, {static_cast<f32>(config.width), static_cast<f32>(config.height)},
      ui_scale_override_);

  // Builder
  builder_.set_animator(&animator_);
  builder_.set_viewport_size(
      {static_cast<f32>(config.width), static_cast<f32>(config.height)});

  last_time_ = platform_.time();
  initialized_ = true;
  return true;
}

FontHandle UIContext::LoadFont(const char* path) {
  FontHandle font = text_engine_.LoadFont(path);
  if (font == kInvalidFont) {
    fprintf(stderr, "ultragui: failed to load font '%s'\n", path);
  }
  return font;
}

FontHandle UIContext::LoadFontMemory(const char* data, usize length) {
  FontHandle font = text_engine_.LoadFontMemory(data, length);
  if (font == kInvalidFont) {
    fprintf(stderr, "ultragui: failed to load font from %zu bytes\n",
            static_cast<size_t>(length));
  }
  return font;
}

void UIContext::set_default_font(FontHandle font) {
  default_font_ = font;
  widget_ctx_.default_font = font;
}

wid UIContext::LoadUi(const char* path) {
  UguiDocument doc;
  Vector<ParseError> errors;

  if (!ParseUguiFile(path, doc, errors)) {
    for (auto& e : errors) {
      fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(),
              e.line, e.column, e.message.c_str());
    }
    return kNullWidget;
  }

  if (root_.valid()) {
    input_.ResetState();
    tooltip_target_ = kNullWidget;
    tooltip_visible_ = false;
    script_.ClearTimersAndTweens();
    script_.ClearWidgetRegistry();
    if (owns_root_) DestroyWidget(widget_registry_, root_);
  }

  root_ = builder_.Build(doc);
  owns_root_ = true;

  if (root_.valid()) {
    SetContext(widget_registry_, root_, &widget_ctx_);
    RegisterWidgetTree(script_, root_);
    script_.WireChangeHandlers(root_);
  }
  // The layout engine holds a Yoga tree per root; the widgets it was built
  // from are gone.
  layout_engine_.Reset();
  widget_cache_dirty_ = true;

  return root_;
}

wid UIContext::LoadUiString(const char* source, const char* name) {
  UguiDocument doc;
  Vector<ParseError> errors;

  if (!ParseUgui(source, strlen(source), name, doc, errors)) {
    for (auto& e : errors) {
      fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(),
              e.line, e.column, e.message.c_str());
    }
    return kNullWidget;
  }

  if (root_.valid()) {
    input_.ResetState();
    tooltip_target_ = kNullWidget;
    tooltip_visible_ = false;
    script_.ClearTimersAndTweens();
    script_.ClearWidgetRegistry();
    if (owns_root_) DestroyWidget(widget_registry_, root_);
  }

  root_ = builder_.Build(doc);
  owns_root_ = true;

  if (root_.valid()) {
    SetContext(widget_registry_, root_, &widget_ctx_);
    RegisterWidgetTree(script_, root_);
    script_.WireChangeHandlers(root_);
  }
  // The layout engine holds a Yoga tree per root; the widgets it was built
  // from are gone.
  layout_engine_.Reset();
  widget_cache_dirty_ = true;

  return root_;
}

bool UIContext::LoadScript(const char* path) {
  script_.SyncTimerClock(platform_.time());
  return script_.ExecFile(path);
}

bool UIContext::ExecScript(const char* script, const char* name) {
  script_.SyncTimerClock(platform_.time());
  return script_.Exec(script, name);
}

void UIContext::set_root(wid root) {
  input_.ResetState();
  tooltip_target_ = kNullWidget;
  tooltip_visible_ = false;
  script_.ClearTimersAndTweens();
  script_.ClearWidgetRegistry();

  if (owns_root_ && root_.valid()) DestroyWidget(widget_registry_, root_);
  root_ = root;
  owns_root_ = false;
  if (root_.valid()) {
    SetContext(widget_registry_, root_, &widget_ctx_);
    RegisterWidgetTree(script_, root_);
  }
  // The layout engine holds a Yoga tree per root; the widgets it was built
  // from are gone.
  layout_engine_.Reset();
  widget_cache_dirty_ = true;
}

bool UIContext::Running() const {
  return initialized_ && !platform_.ShouldClose();
}

f64 UIContext::time() const { return initialized_ ? platform_.time() : 0.0; }

void UIContext::PumpInput() {
  if (input_pumped_this_frame_) return;
  input_pumped_this_frame_ = true;
  const f64 input_start = platform_.time();

  // Poll OS input (processes window resize events, fires platform
  // callbacks that may push events into the queue).
  platform_.PollEvents();

  // Use platform window_size() for viewport: it reflects resizes
  // immediately, unlike rhi_->display_size() which waits for
  // swapchain recreation in BeginFrame.
  Vec2 viewport = platform_.window_size();
  builder_.set_viewport_size(viewport);
  widget_ctx_.ui_scale = ComputeViewportScale(config_, viewport, ui_scale_override_);

  // Viewport changed: re-resolve @media breakpoints so responsive layouts
  // react to resizes live, not only at load time.
  if (viewport.x != media_viewport_.x || viewport.y != media_viewport_.y) {
    media_viewport_ = viewport;
    if (root_.valid()) builder_.ReapplyMediaQueries(root_);
  }

  // Route input - overlays intercept clicks before the root tree.
  // Press on an overlay dispatches OnClick; press outside dismisses all.
  if (!overlays_.empty()) {
    auto& queue = platform_.input_queue();
    for (u32 i = 0; i < queue.button_count; ++i) {
      auto& evt = queue.button_events[i];
      if (!evt.pressed) continue;

      wid hit;
      for (i32 j = static_cast<i32>(overlays_.size()) - 1; j >= 0; --j) {
        wid w = overlays_[j].widget;
        if (!w.valid()) continue;
        wid deepest = HitTest(widget_registry_, w, evt.position);
        if (deepest.valid()) {
          hit = deepest;
          break;
        }
      }

      if (hit.valid()) {
        // Dispatch click to overlay widget, then consume the event.
        ClickWidget(widget_registry_, hit);
        for (u32 k = i + 1; k < queue.button_count; ++k)
          queue.button_events[k - 1] = queue.button_events[k];
        --queue.button_count;
        --i;
      } else {
        // Click outside - dismiss every overlay and consume the
        // press so it doesn't also trigger something in the root
        // tree (standard menu-dismiss behavior).
        auto snap = overlays_;
        overlays_.clear();
        for (auto& entry : snap) {
          if (entry.widget.valid()) DismissWidget(widget_registry_, entry.widget);
        }
        for (u32 k = i + 1; k < queue.button_count; ++k)
          queue.button_events[k - 1] = queue.button_events[k];
        --queue.button_count;
        --i;
      }
    }
  }

  if (root_.valid()) input_.Process(root_);

  // Kept aside rather than written into stats_: an application that pumps
  // early (to route a click before the values it changes are read) does so
  // before the frame's stats are reset.
  last_input_ms_ = (platform_.time() - input_start) * 1000.0;
}

void UIContext::Update() {
  // Timing
  f64 now = platform_.time();
  dt_ = now - last_time_;
  last_time_ = now;

  // Drain input first if the application hasn't already done so via
  // an early PumpInput call. Idempotent - re-entry is a no-op until
  // the end of this Update resets the flag.
  PumpInput();

  // The layout / paint stages below need the live viewport size; the
  // PumpInput call has already pushed it into builder_, but we still
  // need a local copy here for the LayoutViewport struct.
  Vec2 viewport = platform_.window_size();

  // Fire pending Lua timers
  script_.UpdateTimers(now);

  // Update tooltip display
  UpdateTooltip();

  // Update animations
  if (root_.valid()) {
    animator_.Update(
        now,
        [](u32 widget_id, const Style& animated_style, void* user_data) {
          auto* ctx = static_cast<UIContext*>(user_data);
          wid w = ctx->WidgetById(widget_id);
          if (w.valid())
            SetAnimationStyle(ctx->widget_registry_, w, animated_style);
        },
        this,
        [](u32 widget_id, void* user_data) {
          auto* ctx = static_cast<UIContext*>(user_data);
          wid w = ctx->WidgetById(widget_id);
          if (w.valid()) ClearAnimationStyle(ctx->widget_registry_, w);
        });
  }

  // Update widgets (scroll momentum, etc.)
  if (root_.valid()) UpdateWidgetTree(root_, dt_);

  // Update all animations
  auto update_anim = [dt = dt_](auto* anim) {
    if (anim) anim->Update(dt);
  };
  for (auto* anim : vector_anims_) update_anim(anim);
#if ULTRAGUI_LOTTIE
  for (auto* anim : lottie_anims_) update_anim(anim);
#endif
#if ULTRAGUI_VIDEO
  for (auto* anim : video_players_) update_anim(anim);
#endif

#if ULTRAGUI_VIDEO
  // GPU YCbCr -> RGBA conversion for videos with new frames.
  // Must happen after AcquireFrame (need a command buffer) and before
  // the main render pass (so texture() returns the converted RGBA).
  {
    bool need_convert = false;
    for (auto* vid : video_players_) {
      if (vid && vid->NeedsConvert()) {
        need_convert = true;
        break;
      }
    }
    if (need_convert && rhi_.AcquireFrame()) {
      for (auto* vid : video_players_) {
        if (vid && vid->NeedsConvert()) vid->ConvertFrame();
      }
    }
  }
#endif

  // --- Text shaping and atlas management (BEFORE render pass) ---
  text_engine_.BeginFrame();
  if (root_.valid()) MeasureWidgetTree(root_);
  for (auto& pass : offscreen_queue_) {
    if (pass.root.valid()) MeasureWidgetTree(pass.root);
  }
  for (auto& overlay : overlays_) {
    if (overlay.widget.valid()) MeasureWidgetTree(overlay.widget);
  }
  text_engine_.FlushAtlas();

  // --- Offscreen render passes ---
  if (!offscreen_queue_.empty()) {
    if (!rhi_.AcquireFrame()) {
      offscreen_queue_.clear();
      return;
    }

    LayoutViewport vp{viewport.x, viewport.y, widget_ctx_.ui_scale};
    for (auto& pass : offscreen_queue_) {
      if (!rhi_.BeginOffscreen(pass.target, pass.clear_color)) continue;

      renderer_.BeginFrame();
      if (pass.root.valid()) {
        ComputeWidgetLayout(pass.root, vp, layout_engine_);
        PaintWidgetTree(pass.root, renderer_);
      }
      text_engine_.FlushAtlas();
      renderer_.EndFrame();
      rhi_.EndOffscreen(pass.target);
    }
    offscreen_queue_.clear();
  }

  // --- Main swapchain render pass ---
  if (!rhi_.BeginFrame(config_.clear_color)) return;

  renderer_.BeginFrame();

  if (on_paint_cb_) {
    on_paint_cb_(renderer_, &rhi_);
  } else if (root_.valid()) {
    LayoutViewport vp{viewport.x, viewport.y, widget_ctx_.ui_scale};
    ComputeWidgetLayout(root_, vp, layout_engine_);
    PaintWidgetTree(root_, renderer_);
  }

  // Paint overlays on top of everything
  for (auto& overlay : overlays_) {
    if (overlay.widget.valid()) {
      LayoutViewport ovp{viewport.x, viewport.y, widget_ctx_.ui_scale};
      ComputeWidgetLayout(overlay.widget, ovp, layout_engine_);
      PaintWidgetTree(overlay.widget, renderer_);
    }
  }

  // Flush any new glyphs from paint pass
  text_engine_.FlushAtlas();

  // Tooltip (drawn last, on top of everything)
  DrawTooltip();

  renderer_.EndFrame();
  rhi_.EndFrame();

  // Reset the per-frame input-pumped flag so the next frame can
  // PumpInput again.
  input_pumped_this_frame_ = false;
}


// --- Frame reuse ------------------------------------------------------------

// FNV-1a over the bytes a backend would actually upload. Used only by
// kVerify, to tell a frame that genuinely repeated from one the predicate
// merely believed had.
static u64 HashDrawData(const DrawData& dd) {
  u64 h = 1469598103934665603ull;
  auto feed = [&h](const void* p, usize bytes) {
    const u8* b = static_cast<const u8*>(p);
    for (usize i = 0; i < bytes; ++i) {
      h ^= b[i];
      h *= 1099511628211ull;
    }
  };
  feed(&dd.display_size, sizeof(dd.display_size));
  feed(&dd.display_pos, sizeof(dd.display_pos));
  feed(&dd.framebuffer_scale, sizeof(dd.framebuffer_scale));
  if (dd.commands)
    feed(dd.commands, dd.command_count * sizeof(DrawCmd));
  if (dd.quad_vertices)
    feed(dd.quad_vertices, dd.quad_vertex_count * sizeof(Vertex2D));
  if (dd.quad_indices) feed(dd.quad_indices, dd.quad_index_count * sizeof(u32));
  if (dd.text_vertices)
    feed(dd.text_vertices, dd.text_vertex_count * sizeof(Vertex2D));
  if (dd.text_indices) feed(dd.text_indices, dd.text_index_count * sizeof(u32));
  return h;
}

bool UIContext::FrameWouldRepeat() const {
  if (!have_built_frame_) return false;
  // Every mutation that changes what is drawn marks the widget dirty, and
  // every mark bumps this. Animations included: the animator writes each step
  // through SetAnimationStyle, which marks paint-dirty.
  if (WidgetRevision() != last_widget_revision_) return false;
  const Vec2 viewport = platform_.window_size();
  if (viewport.x != last_reuse_viewport_.x ||
      viewport.y != last_reuse_viewport_.y)
    return false;
  if (widget_ctx_.ui_scale != last_reuse_scale_) return false;
  // The tooltip is drawn straight into the list by DrawTooltip rather than
  // being a widget, so its state is not covered by the revision.
  if (tooltip_visible_ != last_tooltip_visible_) return false;
  if (tooltip_target_ != last_tooltip_target_) return false;
  if (overlays_.size() != last_overlay_widgets_.size()) return false;
  for (usize i = 0; i < overlays_.size(); ++i)
    if (overlays_[i].widget != last_overlay_widgets_[i]) return false;
  return true;
}

void UIContext::RecordBuiltFrame(const DrawData& dd) {
  have_built_frame_ = true;
  last_widget_revision_ = WidgetRevision();
  last_reuse_viewport_ = platform_.window_size();
  last_reuse_scale_ = widget_ctx_.ui_scale;
  last_tooltip_visible_ = tooltip_visible_;
  last_tooltip_target_ = tooltip_target_;
  last_overlay_widgets_.clear();
  for (const auto& o : overlays_) last_overlay_widgets_.push_back(o.widget);
  if (frame_reuse_ == FrameReuse::kVerify) last_draw_hash_ = HashDrawData(dd);
}

const DrawData& UIContext::RenderDrawData() {
  // Timing
  f64 now = platform_.time();
  dt_ = now - last_time_;
  last_time_ = now;
  const f64 frame_start = now;
  stats_ = {};

  // Input (via the attached host window) + viewport
  PumpInput();  // no-op if the application already pumped this frame
  Vec2 viewport = platform_.window_size();
  renderer_.set_display_size(viewport);
  f64 mark = platform_.time();
  stats_.input_ms = last_input_ms_;

  script_.UpdateTimers(now);
  UpdateTooltip();

  // Animations + per-widget update (texture-backed anims (lottie/video) are not
  // produced in draw-data mode, since the host owns the GPU).
  if (root_.valid()) {
    animator_.Update(
        now,
        [](u32 widget_id, const Style& animated_style, void* user_data) {
          auto* ctx = static_cast<UIContext*>(user_data);
          wid w = ctx->WidgetById(widget_id);
          if (w.valid())
            SetAnimationStyle(ctx->widget_registry_, w, animated_style);
        },
        this,
        [](u32 widget_id, void* user_data) {
          auto* ctx = static_cast<UIContext*>(user_data);
          wid w = ctx->WidgetById(widget_id);
          if (w.valid()) ClearAnimationStyle(ctx->widget_registry_, w);
        });
    UpdateWidgetTree(root_, dt_);
  }
  stats_.update_ms = (platform_.time() - mark) * 1000.0;
  mark = platform_.time();

  // Everything above can itself change the tree - a timer firing, an animation
  // stepping, scroll momentum - so the question is only worth asking now.
  const bool would_repeat = frame_reuse_ != FrameReuse::kOff && FrameWouldRepeat();
  if (would_repeat && frame_reuse_ == FrameReuse::kOn) {
    // The renderer's buffers still hold the last list; nothing has touched
    // them, so handing it back is handing back that exact frame.
    input_pumped_this_frame_ = false;
    const DrawData& dd = renderer_.GetDrawData();
    stats_.reused = true;
    stats_.draw_commands = dd.command_count;
    stats_.total_ms = (platform_.time() - frame_start) * 1000.0;
    return dd;
  }

  // Text shaping (no GPU upload: the host uploads from text_engine().)
  text_engine_.BeginFrame();
  if (root_.valid()) stats_.widgets += MeasureWidgetTree(root_);
  for (auto& overlay : overlays_) {
    if (overlay.widget.valid()) stats_.widgets += MeasureWidgetTree(overlay.widget);
  }
  stats_.measure_ms = (platform_.time() - mark) * 1000.0;

  // Paint into the renderer; collect as a draw list instead of submitting.
  renderer_.BeginFrame();
  LayoutViewport vp{viewport.x, viewport.y, widget_ctx_.ui_scale};
  if (root_.valid()) {
    mark = platform_.time();
    ComputeWidgetLayout(root_, vp, layout_engine_);
    stats_.layout_ms += (platform_.time() - mark) * 1000.0;
    stats_.layout_nodes += static_cast<u32>(
        layout_engine_.StoreFor(widget_registry_.Get<WidgetNode>(root_)->id).nodes.size());
    mark = platform_.time();
    PaintWidgetTree(root_, renderer_);
    stats_.paint_ms += (platform_.time() - mark) * 1000.0;
  }
  for (auto& overlay : overlays_) {
    if (overlay.widget.valid()) {
      mark = platform_.time();
      ComputeWidgetLayout(overlay.widget, vp, layout_engine_);
      stats_.layout_ms += (platform_.time() - mark) * 1000.0;
      stats_.layout_nodes += static_cast<u32>(
          layout_engine_.StoreFor(widget_registry_.Get<WidgetNode>(overlay.widget)->id)
              .nodes.size());
      mark = platform_.time();
      PaintWidgetTree(overlay.widget, renderer_);
      stats_.paint_ms += (platform_.time() - mark) * 1000.0;
    }
  }
  DrawTooltip();

  input_pumped_this_frame_ = false;
  const DrawData& dd = renderer_.GetDrawData();
  stats_.draw_commands = dd.command_count;
  stats_.shape_calls = text_engine_.shape_calls();
  stats_.shape_hits = text_engine_.shape_hits();

  // kVerify: the frame was rebuilt whatever the predicate said, so the two can
  // be held against each other. A frame the predicate called a repeat, that
  // did not come out byte-identical, is something changing what is drawn
  // without marking the widget dirty.
  if (frame_reuse_ == FrameReuse::kVerify && would_repeat) {
    ++reuse_candidates_;
    stats_.reused = true;  // "would have been", in this mode
    if (HashDrawData(dd) != last_draw_hash_) {
      ++reuse_mismatches_;
      fprintf(stderr,
              "ultragui: frame reuse would have shown a stale frame "
              "(mismatch %llu)\n",
              static_cast<unsigned long long>(reuse_mismatches_));
    }
  }
  RecordBuiltFrame(dd);

  stats_.total_ms = (platform_.time() - frame_start) * 1000.0;
  return dd;
}

void UIContext::UpdateTooltip() {
  wid hovered = input_.hovered_widget();

  // Find the nearest ancestor with a tooltip (walk up the tree)
  wid tip_target = hovered;
  while (tip_target.valid() &&
         TooltipText(widget_registry_, tip_target).empty()) {
    Hierarchy* h = widget_registry_.Get<Hierarchy>(tip_target);
    tip_target = h ? h->parent : kNullWidget;
  }

  if (tip_target != tooltip_target_) {
    tooltip_target_ = tip_target;
    tooltip_visible_ = false;
    tooltip_hover_start_ = last_time_;
  }

  // Show tooltip after delay: we set a flag; actual drawing happens in Update()
  if (tooltip_target_.valid() && !tooltip_visible_ &&
      (last_time_ - tooltip_hover_start_) >= kTooltipDelay) {
    tooltip_visible_ = true;
  }
}

void UIContext::DrawTooltip() {
  if (!tooltip_visible_ || !tooltip_target_.valid()) return;
  const String& tip = TooltipText(widget_registry_, tooltip_target_);
  if (tip.empty()) return;

  FontHandle fh = default_font_;
  if (fh == kInvalidFont) return;

  f32 sc = widget_ctx_.ui_scale;
  f32 font_size = 12.0f * sc;
  auto run = text_engine_.Shape(fh, tip.c_str(), static_cast<u32>(tip.size()),
                                font_size, 0.0f, 1.0f);

  f32 pad_x = 10.0f * sc, pad_y = 6.0f * sc;
  f32 w = run.total_advance + pad_x * 2.0f;
  f32 h = run.line_height + pad_y * 2.0f;

  Rect target_rect = widget_registry_.Get<Transform>(tooltip_target_)->rect;
  f32 x = target_rect.x;
  f32 y = target_rect.y + target_rect.h + 6.0f;

  // Clamp to viewport
  Vec2 vp = rhi_.display_size();
  if (x + w > vp.x) x = vp.x - w - 4.0f;
  if (y + h > vp.y) y = target_rect.y - h - 6.0f;

  u32 radii = Vertex2D::PackRadii(6.0f);

  // Shadow
  renderer_.DrawShadow({x, y, w, h}, Color::FromHex(0x000000, 0.4f), 6.0f, 0.0f,
                       {0, 2}, radii);
  // Background
  renderer_.DrawBorderedRect({x, y, w, h}, Color::FromHex(0x181828, 0.95f),
                             Color::FromHex(0xffffff, 0.08f), 1.0f, radii);
  // Text
  text_engine_.FlushAtlas();
  renderer_.DrawText({x + pad_x, y + pad_y}, run, Color::FromHex(0xd0d0e0),
                     text_engine_.atlas_texture());
}

void UIContext::Shutdown() {
  if (!initialized_) return;

  input_.ResetState();
  tooltip_target_ = kNullWidget;
  tooltip_visible_ = false;
  overlays_.clear();
  script_.ClearWidgetRegistry();

  if (owns_root_ && root_.valid()) DestroyWidget(widget_registry_, root_);
  root_ = kNullWidget;
  owns_root_ = false;

  for (auto* anim : vector_anims_) delete anim;
  vector_anims_.clear();

#if ULTRAGUI_LOTTIE
  for (auto* anim : lottie_anims_) delete anim;
  lottie_anims_.clear();
#endif
#if ULTRAGUI_VIDEO
  for (auto* vid : video_players_) delete vid;
  video_players_.clear();
#endif
#if ULTRAGUI_AUDIO
  audio_->Shutdown();  // wired backend is owned by the caller; never deleted
#endif
  script_.Shutdown();
  renderer_.Shutdown();
  text_engine_.Shutdown();
  if (!config_.draw_data) rhi_.Shutdown();
  platform_.Shutdown();

  initialized_ = false;
}

#if ULTRAGUI_AUDIO
void UIContext::set_audio(AudioBackend* backend) {
  audio_ = backend ? backend : &null_audio_;
}
#endif

void UIContext::set_texture_backend(TextureBackend* backend) {
  texture_backend_ = backend;
  renderer_.set_texture_backend(backend);
}

TextureId UIContext::LoadSvg(const char* path, u32 width, u32 height) {
  return LoadSvgTexture(texture_backend_, path, width, height);
}

VectorAnimation* UIContext::LoadAnim(const char* path, u32 width, u32 height) {
  auto* anim = new VectorAnimation();
  if (!anim->Load(texture_backend_, path, width, height)) {
    delete anim;
    return nullptr;
  }
  vector_anims_.push_back(anim);
  return anim;
}

#if ULTRAGUI_LOTTIE
LottieAnimation* UIContext::LoadLottie(const char* path, u32 width,
                                       u32 height) {
  auto* anim = new LottieAnimation();
  if (!anim->Load(texture_backend_, path, width, height)) {
    delete anim;
    return nullptr;
  }
  lottie_anims_.push_back(anim);
  return anim;
}
#endif

#if ULTRAGUI_VIDEO
VideoPlayer* UIContext::LoadVideo(const char* path) {
  auto* vid = new VideoPlayer();
  AudioBackend* audio_ptr = nullptr;
#if ULTRAGUI_AUDIO
  audio_ptr = audio_;
#endif
  if (!vid->Load(&rhi_, path, audio_ptr)) {
    delete vid;
    return nullptr;
  }
  video_players_.push_back(vid);
  return vid;
}
#endif

RHITextureHandle UIContext::CreateRenderTarget(u32 width, u32 height) {
  return rhi_.CreateRenderTarget(width, height);
}

void UIContext::QueueOffscreen(RHITextureHandle target, wid root,
                               Color clear_color) {
  offscreen_queue_.push_back({target, root, clear_color});
}

void UIContext::ShowOverlay(wid widget, Vec2 position) {
  // Remove if already shown
  HideOverlay(widget);
  SetContext(widget_registry_, widget, &widget_ctx_);
  overlays_.push_back({widget, position});
}

void UIContext::HideOverlay(wid widget) {
  EraseIf(overlays_,
          [widget](const OverlayEntry& e) { return e.widget == widget; });
}

void UIContext::SetOnPaint(PaintCallback cb) { on_paint_cb_ = ugui::move(cb); }

void UIContext::SetTheme(const Theme& theme) {
  current_theme_name_ = theme.name;

  // Apply all theme tokens as CSS variables on the builder.
  for (const auto& [name, value] : theme.tokens) {
    builder_.SetVariable(name, value);
  }

  // Variables take effect on the next LoadUi/LoadUiString call.
  // A full hot-reload would require re-parsing and rebuilding the tree.
}

wid UIContext::FindWidgetEntity(const char* name) const {
  if (widget_cache_dirty_) RebuildWidgetCache();
  const wid* hit = widget_cache_.find(name);
  return hit ? *hit : kNullWidget;
}

WidgetId UIContext::FindWidget(const char* name) const {
  return FindWidgetEntity(name);
}

void UIContext::CacheWidgetTree(wid w, HashMap<String, wid>& cache,
                                HashMap<u32, wid>& by_id) {
  if (!w.valid()) return;
  World& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(w);
  if (n) {
    if (!n->name.empty()) cache[n->name] = w;
    by_id[n->id] = w;
  }
  if (Hierarchy* h = world.Get<Hierarchy>(w))
    for (wid child : h->children) CacheWidgetTree(child, cache, by_id);
}

void UIContext::RebuildWidgetCache() const {
  widget_cache_.clear();
  id_cache_.clear();
  CacheWidgetTree(root_, widget_cache_, id_cache_);
  widget_cache_dirty_ = false;
}

wid UIContext::WidgetById(u32 id) const {
  if (widget_cache_dirty_) RebuildWidgetCache();
  const wid* hit = id_cache_.find(id);
  return hit ? *hit : kNullWidget;
}

}  // namespace ugui
