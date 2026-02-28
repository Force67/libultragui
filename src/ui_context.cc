#include <ultragui/ui_context.h>
#include <ultragui/scripting/lua_widgets.h>
#include <ultragui/widgets/panel.h>
#include <ultragui/widgets/text.h>

#if ULTRAGUI_LUA
#include <ultragui/scripting/lua_anim.h>
#if ULTRAGUI_AUDIO
#include <ultragui/scripting/lua_audio.h>
#endif
#if ULTRAGUI_LOTTIE
#include <ultragui/scripting/lua_lottie.h>
#endif
#if ULTRAGUI_VIDEO
#include <ultragui/scripting/lua_video.h>
#endif
#endif // ULTRAGUI_LUA

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ugui {

static f32 ComputeViewportScale(const UIConfig& cfg, Vec2 display) {
    switch (cfg.scale_mode) {
    case ViewportScaleMode::kWidth:
        return (cfg.design_width > 0.0f) ? display.x / cfg.design_width : 1.0f;
    case ViewportScaleMode::kHeight:
        return (cfg.design_height > 0.0f) ? display.y / cfg.design_height : 1.0f;
    case ViewportScaleMode::kContain: {
        f32 sw = (cfg.design_width > 0.0f) ? display.x / cfg.design_width : 1.0f;
        f32 sh = (cfg.design_height > 0.0f) ? display.y / cfg.design_height : 1.0f;
        return std::fmin(sw, sh);
    }
    case ViewportScaleMode::kCover: {
        f32 sw = (cfg.design_width > 0.0f) ? display.x / cfg.design_width : 1.0f;
        f32 sh = (cfg.design_height > 0.0f) ? display.y / cfg.design_height : 1.0f;
        return std::fmax(sw, sh);
    }
    default:
        return 1.0f;
    }
}

UIContext::~UIContext() {
    if (initialized_)
        Shutdown();
}

bool UIContext::Init(const UIConfig& config) {
    config_ = config;

    // Platform
    Platform::WindowConfig wcfg;
    wcfg.width = config.width;
    wcfg.height = config.height;
    wcfg.title = config.title;
    wcfg.resizable = config.resizable;
    wcfg.vsync = config.vsync;
#if ULTRAGUI_BACKEND_OPENGL
    wcfg.opengl = true;
#endif

    if (!platform_.Init(wcfg)) {
        std::fprintf(stderr, "ultragui: failed to initialize platform\n");
        return false;
    }

    // RHI
    RHIConfig rcfg;
    rcfg.platform = &platform_;
    rcfg.validation = config.validation;
    rcfg.vsync = config.vsync;
    rcfg.shader_dir = config.shader_dir;

    if (!rhi_.Init(rcfg)) {
        std::fprintf(stderr, "ultragui: failed to initialize Vulkan RHI\n");
        platform_.Shutdown();
        return false;
    }

    // Renderer
    renderer_.Init(&rhi_);

    // Text engine
    if (!text_engine_.Init(&rhi_)) {
        std::fprintf(stderr, "ultragui: failed to initialize text engine\n");
    }

    // Input
    input_.Init(&platform_);

    // Scripting runtime
    script_.Init();

#if ULTRAGUI_LUA
    {
#if ULTRAGUI_AUDIO
        RegisterAudioLua(script_, audio_);
#endif
#if ULTRAGUI_LOTTIE
        RegisterLottieLua(
            script_,
            [this](const char* path, unsigned w, unsigned h) -> LottieAnimation* {
                return LoadLottie(path, w, h);
            },
            [this](const char* name) -> Widget* { return FindWidget(name); });
#endif
        RegisterAnimLua(
            script_,
            [this](const char* path, unsigned w, unsigned h) -> VectorAnimation* {
                return LoadAnim(path, w, h);
            },
            [this](const char* name) -> Widget* { return FindWidget(name); });
#if ULTRAGUI_VIDEO
        RegisterVideoLua(
            script_,
            [this](const char* path) -> VideoPlayer* {
                return LoadVideo(path);
            },
            [this](const char* name) -> Widget* { return FindWidget(name); });
#endif
    }
#endif

#if ULTRAGUI_AUDIO
    if (!audio_.Init()) {
        std::fprintf(stderr, "ultragui: audio init failed (non-fatal)\n");
    }
#endif

    // Widget context
    widget_ctx_.text_engine = &text_engine_;
    widget_ctx_.animator = &animator_;
    widget_ctx_.current_time = &last_time_;
    widget_ctx_.platform = &platform_;
    widget_ctx_.ui_scale = ComputeViewportScale(
        config_, {static_cast<f32>(config.width), static_cast<f32>(config.height)});

    // Builder
    builder_.set_animator(&animator_);
    builder_.set_viewport_size({static_cast<f32>(config.width), static_cast<f32>(config.height)});

    last_time_ = platform_.time();
    initialized_ = true;
    return true;
}

FontHandle UIContext::LoadFont(const char* path) {
    FontHandle font = text_engine_.LoadFont(path);
    if (font == kInvalidFont) {
        std::fprintf(stderr, "ultragui: failed to load font '%s'\n", path);
    }
    return font;
}

void UIContext::set_default_font(FontHandle font) {
    default_font_ = font;
    widget_ctx_.default_font = font;
}

Widget* UIContext::LoadUi(const char* path) {
    UguiDocument doc;
    Vector<ParseError> errors;

    if (!ParseUguiFile(path, doc, errors)) {
        for (auto& e : errors) {
            std::fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(), e.line,
                         e.column, e.message.c_str());
        }
        return nullptr;
    }

    if (root_) {
        input_.ResetState();
        tooltip_target_ = nullptr;
        tooltip_visible_ = false;
        script_.ClearWidgetRegistry();
        if (owns_root_)
            delete root_;
    }

    root_ = builder_.Build(doc);
    owns_root_ = true;

    if (root_) {
        root_->SetContext(&widget_ctx_);
        RegisterWidgetTree(script_, root_);
    }

    return root_;
}

Widget* UIContext::LoadUiString(const char* source, const char* name) {
    UguiDocument doc;
    Vector<ParseError> errors;

    if (!ParseUgui(source, std::strlen(source), name, doc, errors)) {
        for (auto& e : errors) {
            std::fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(), e.line,
                         e.column, e.message.c_str());
        }
        return nullptr;
    }

    if (root_) {
        input_.ResetState();
        tooltip_target_ = nullptr;
        tooltip_visible_ = false;
        script_.ClearWidgetRegistry();
        if (owns_root_)
            delete root_;
    }

    root_ = builder_.Build(doc);
    owns_root_ = true;

    if (root_) {
        root_->SetContext(&widget_ctx_);
        RegisterWidgetTree(script_, root_);
    }

    return root_;
}

bool UIContext::LoadScript(const char* path) {
    return script_.ExecFile(path);
}

bool UIContext::ExecScript(const char* script, const char* name) {
    return script_.Exec(script, name);
}

void UIContext::set_root(Widget* root) {
    input_.ResetState();
    tooltip_target_ = nullptr;
    tooltip_visible_ = false;
    script_.ClearWidgetRegistry();

    if (owns_root_)
        delete root_;
    root_ = root;
    owns_root_ = false;
    if (root_) {
        root_->SetContext(&widget_ctx_);
        RegisterWidgetTree(script_, root_);
    }
}

bool UIContext::Running() const {
    return initialized_ && !platform_.ShouldClose();
}

f64 UIContext::time() const {
    return initialized_ ? platform_.time() : 0.0;
}

void UIContext::Update() {
    // Timing
    f64 now = platform_.time();
    dt_ = now - last_time_;
    last_time_ = now;

    // Poll input (processes window resize events)
    platform_.PollEvents();

    // Use platform window_size() for viewport: it reflects resizes immediately,
    // unlike rhi_->display_size() which waits for swapchain recreation in BeginFrame.
    Vec2 viewport = platform_.window_size();

    // Update builder viewport size so media queries reflect current window dimensions
    builder_.set_viewport_size(viewport);

    // Recompute viewport scale factor each frame
    widget_ctx_.ui_scale = ComputeViewportScale(config_, viewport);

    // Route input to widget tree
    if (root_)
        input_.Process(root_);

    // Update tooltip display
    UpdateTooltip();

    // Update animations
    if (root_) {
        animator_.Update(
            now,
            [](u32 widget_id, const Style& animated_style, void* user_data) {
                auto* root = static_cast<Widget*>(user_data);
                Widget* w = FindWidgetById(root, widget_id);
                if (w)
                    w->SetAnimationStyle(animated_style);
            },
            root_,
            [](u32 widget_id, void* user_data) {
                auto* root = static_cast<Widget*>(user_data);
                Widget* w = FindWidgetById(root, widget_id);
                if (w)
                    w->ClearAnimationStyle();
            });
    }

    // Update widgets (scroll momentum, etc.)
    if (root_)
        UpdateWidgetTree(root_, dt_);

    // Update all animations
    auto update_anim = [dt = dt_](auto* anim) {
        if (anim) anim->Update(dt);
    };
    std::for_each(vector_anims_.begin(), vector_anims_.end(), update_anim);
#if ULTRAGUI_LOTTIE
    std::for_each(lottie_anims_.begin(), lottie_anims_.end(), update_anim);
#endif
#if ULTRAGUI_VIDEO
    std::for_each(video_players_.begin(), video_players_.end(), update_anim);
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
                if (vid && vid->NeedsConvert())
                    vid->ConvertFrame();
            }
        }
    }
#endif

    // --- Text shaping and atlas management (BEFORE render pass) ---
    text_engine_.BeginFrame();
    if (root_)
        MeasureWidgetTree(root_);
    for (auto& pass : offscreen_queue_) {
        if (pass.root)
            MeasureWidgetTree(pass.root);
    }
    for (auto& overlay : overlays_) {
        if (overlay.widget)
            MeasureWidgetTree(overlay.widget);
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
            if (!rhi_.BeginOffscreen(pass.target, pass.clear_color))
                continue;

            renderer_.BeginFrame();
            if (pass.root) {
                ComputeWidgetLayout(pass.root, vp, layout_engine_, layout_nodes_);
                PaintWidgetTree(pass.root, renderer_);
            }
            text_engine_.FlushAtlas();
            renderer_.EndFrame();
            rhi_.EndOffscreen(pass.target);
        }
        offscreen_queue_.clear();
    }

    // --- Main swapchain render pass ---
    if (!rhi_.BeginFrame(config_.clear_color))
        return;

    renderer_.BeginFrame();

    if (on_paint_cb_) {
        on_paint_cb_(renderer_, &rhi_);
    } else if (root_) {
        LayoutViewport vp{viewport.x, viewport.y, widget_ctx_.ui_scale};
        ComputeWidgetLayout(root_, vp, layout_engine_, layout_nodes_);
        PaintWidgetTree(root_, renderer_);
    }

    // Paint overlays on top of everything
    for (auto& overlay : overlays_) {
        if (overlay.widget) {
            LayoutViewport ovp{viewport.x, viewport.y, widget_ctx_.ui_scale};
            ComputeWidgetLayout(overlay.widget, ovp, layout_engine_, layout_nodes_);
            PaintWidgetTree(overlay.widget, renderer_);
        }
    }

    // Flush any new glyphs from paint pass
    text_engine_.FlushAtlas();

    // Tooltip (drawn last, on top of everything)
    DrawTooltip();

    renderer_.EndFrame();
    rhi_.EndFrame();
}

void UIContext::UpdateTooltip() {
    Widget* hovered = input_.hovered_widget();

    // Find the nearest ancestor with a tooltip (walk up the tree)
    Widget* tip_target = hovered;
    while (tip_target && tip_target->tooltip().empty())
        tip_target = tip_target->parent();

    if (tip_target != tooltip_target_) {
        tooltip_target_ = tip_target;
        tooltip_visible_ = false;
        tooltip_hover_start_ = last_time_;
    }

    // Show tooltip after delay: we set a flag; actual drawing happens in Update()
    if (tooltip_target_ && !tooltip_visible_ &&
        (last_time_ - tooltip_hover_start_) >= kTooltipDelay) {
        tooltip_visible_ = true;
    }
}

void UIContext::DrawTooltip() {
    if (!tooltip_visible_ || !tooltip_target_ || tooltip_target_->tooltip().empty())
        return;

    const auto& tip = tooltip_target_->tooltip();
    FontHandle fh = default_font_;
    if (fh == kInvalidFont)
        return;

    f32 sc = widget_ctx_.ui_scale;
    f32 font_size = 12.0f * sc;
    auto run = text_engine_.Shape(fh, tip.c_str(), static_cast<u32>(tip.size()),
                                   font_size, 0.0f, 1.0f);

    f32 pad_x = 10.0f * sc, pad_y = 6.0f * sc;
    f32 w = run.total_advance + pad_x * 2.0f;
    f32 h = run.line_height + pad_y * 2.0f;

    Rect target_rect = tooltip_target_->rect();
    f32 x = target_rect.x;
    f32 y = target_rect.y + target_rect.h + 6.0f;

    // Clamp to viewport
    Vec2 vp = rhi_.display_size();
    if (x + w > vp.x) x = vp.x - w - 4.0f;
    if (y + h > vp.y) y = target_rect.y - h - 6.0f;

    u32 radii = Vertex2D::PackRadii(6.0f);

    // Shadow
    renderer_.DrawShadow({x, y, w, h}, Color::FromHex(0x000000, 0.4f),
                          6.0f, 0.0f, {0, 2}, radii);
    // Background
    renderer_.DrawBorderedRect({x, y, w, h}, Color::FromHex(0x181828, 0.95f),
                                Color::FromHex(0xffffff, 0.08f), 1.0f, radii);
    // Text
    text_engine_.FlushAtlas();
    renderer_.DrawText({x + pad_x, y + pad_y}, run,
                        Color::FromHex(0xd0d0e0), text_engine_.atlas_texture());
}

void UIContext::Shutdown() {
    if (!initialized_)
        return;

    input_.ResetState();
    tooltip_target_ = nullptr;
    tooltip_visible_ = false;
    overlays_.clear();
    script_.ClearWidgetRegistry();

    if (owns_root_) {
        delete root_;
        root_ = nullptr;
        owns_root_ = false;
    }

    for (auto* anim : vector_anims_)
        delete anim;
    vector_anims_.clear();

#if ULTRAGUI_LOTTIE
    for (auto* anim : lottie_anims_)
        delete anim;
    lottie_anims_.clear();
#endif
#if ULTRAGUI_VIDEO
    for (auto* vid : video_players_)
        delete vid;
    video_players_.clear();
#endif
#if ULTRAGUI_AUDIO
    audio_.Shutdown();
#endif
    script_.Shutdown();
    renderer_.Shutdown();
    text_engine_.Shutdown();
    rhi_.Shutdown();
    platform_.Shutdown();

    initialized_ = false;
}

RHITextureHandle UIContext::LoadSvg(const char* path, u32 width, u32 height) {
    return LoadSvgTexture(&rhi_, path, width, height);
}

VectorAnimation* UIContext::LoadAnim(const char* path, u32 width, u32 height) {
    auto* anim = new VectorAnimation();
    if (!anim->Load(&rhi_, path, width, height)) {
        delete anim;
        return nullptr;
    }
    vector_anims_.push_back(anim);
    return anim;
}

#if ULTRAGUI_LOTTIE
LottieAnimation* UIContext::LoadLottie(const char* path, u32 width, u32 height) {
    auto* anim = new LottieAnimation();
    if (!anim->Load(&rhi_, path, width, height)) {
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
    AudioEngine* audio_ptr = nullptr;
#if ULTRAGUI_AUDIO
    audio_ptr = &audio_;
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

void UIContext::QueueOffscreen(RHITextureHandle target, Widget* root, Color clear_color) {
    offscreen_queue_.push_back({target, root, clear_color});
}

void UIContext::ShowOverlay(Widget* widget, Vec2 position) {
    // Remove if already shown
    HideOverlay(widget);
    widget->SetContext(&widget_ctx_);
    overlays_.push_back({widget, position});
}

void UIContext::HideOverlay(Widget* widget) {
    overlays_.erase(
        std::remove_if(overlays_.begin(), overlays_.end(),
                       [widget](const OverlayEntry& e) { return e.widget == widget; }),
        overlays_.end());
}

void UIContext::SetOnPaint(PaintCallback cb) {
    on_paint_cb_ = std::move(cb);
}

void UIContext::SetTheme(const Theme& theme) {
    current_theme_name_ = theme.name;

    // Apply all theme tokens as CSS variables on the builder.
    for (const auto& [name, value] : theme.tokens) {
        builder_.SetVariable(name, value);
    }

    // Variables take effect on the next LoadUi/LoadUiString call.
    // A full hot-reload would require re-parsing and rebuilding the tree.
}

Widget* UIContext::FindWidget(const char* name) const {
    return ugui::FindWidget(root_, name);
}

} // namespace ugui
