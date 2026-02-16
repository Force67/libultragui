#include <ultragui/ui_context.h>
#include <ultragui/scripting/lua_anim.h>
#include <ultragui/scripting/lua_widgets.h>

#if ULTRAGUI_AUDIO
#include <ultragui/scripting/lua_audio.h>
#endif

#if ULTRAGUI_LOTTIE
#include <ultragui/scripting/lua_lottie.h>
#endif

#include <cstdio>
#include <cstring>

namespace ugui {

UIContext::~UIContext() {
    if (platform_)
        Shutdown();
}

bool UIContext::Init(const UIConfig& config) {
    config_ = config;

    // Platform
    platform_ = CreateGlfwPlatform();
    Platform::WindowConfig wcfg;
    wcfg.width = config.width;
    wcfg.height = config.height;
    wcfg.title = config.title;
    wcfg.resizable = config.resizable;
    wcfg.vsync = config.vsync;

    if (!platform_->Init(wcfg)) {
        std::fprintf(stderr, "ultragui: failed to initialize platform\n");
        return false;
    }

    // RHI
    rhi_ = CreateVulkanRhi();
    RHIConfig rcfg;
    rcfg.platform = platform_;
    rcfg.validation = config.validation;
    rcfg.vsync = config.vsync;
    rcfg.shader_dir = config.shader_dir;

    if (!rhi_->Init(rcfg)) {
        std::fprintf(stderr, "ultragui: failed to initialize Vulkan RHI\n");
        platform_->Shutdown();
        delete platform_;
        platform_ = nullptr;
        return false;
    }

    // Renderer
    renderer_.Init(rhi_);

    // Text engine
    if (!text_engine_.Init(rhi_)) {
        std::fprintf(stderr, "ultragui: failed to initialize text engine\n");
    }

    // Input
    input_.Init(platform_);

    // Lua
    lua_.Init();

#if ULTRAGUI_AUDIO
    if (!audio_.Init()) {
        std::fprintf(stderr, "ultragui: audio init failed (non-fatal)\n");
    }
    RegisterAudioLua(lua_, audio_);
#endif

#if ULTRAGUI_LOTTIE
    RegisterLottieLua(
        lua_,
        [this](const char* path, unsigned w, unsigned h) -> LottieAnimation* {
            return LoadLottie(path, w, h);
        },
        [this](const char* name) -> Widget* { return FindWidget(name); });
#endif

    // Vector animations lua bindings (always available - zero deps)
    RegisterAnimLua(
        lua_,
        [this](const char* path, unsigned w, unsigned h) -> VectorAnimation* {
            return LoadAnim(path, w, h);
        },
        [this](const char* name) -> Widget* { return FindWidget(name); });

    // Widget context
    widget_ctx_.text_engine = &text_engine_;
    widget_ctx_.animator = &animator_;
    widget_ctx_.current_time = &last_time_;

    // Builder
    builder_.set_animator(&animator_);

    last_time_ = platform_->time();
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
    std::vector<ParseError> errors;

    if (!ParseUguiFile(path, doc, errors)) {
        for (auto& e : errors) {
            std::fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(), e.line,
                         e.column, e.message.c_str());
        }
        return nullptr;
    }

    if (owns_root_)
        delete root_;

    root_ = builder_.Build(doc);
    owns_root_ = true;

    if (root_) {
        root_->SetContext(&widget_ctx_);
        RegisterWidgetTreeLua(lua_, root_);
    }

    return root_;
}

Widget* UIContext::LoadUiString(const char* source, const char* name) {
    UguiDocument doc;
    std::vector<ParseError> errors;

    if (!ParseUgui(source, std::strlen(source), name, doc, errors)) {
        for (auto& e : errors) {
            std::fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(), e.line,
                         e.column, e.message.c_str());
        }
        return nullptr;
    }

    if (owns_root_)
        delete root_;

    root_ = builder_.Build(doc);
    owns_root_ = true;

    if (root_) {
        root_->SetContext(&widget_ctx_);
        RegisterWidgetTreeLua(lua_, root_);
    }

    return root_;
}

bool UIContext::LoadScript(const char* path) {
    return lua_.ExecFile(path);
}

bool UIContext::ExecScript(const char* script, const char* name) {
    return lua_.Exec(script, name);
}

void UIContext::set_root(Widget* root) {
    if (owns_root_)
        delete root_;
    root_ = root;
    owns_root_ = false;
    if (root_) {
        root_->SetContext(&widget_ctx_);
        RegisterWidgetTreeLua(lua_, root_);
    }
}

bool UIContext::Running() const {
    return platform_ && !platform_->ShouldClose();
}

f64 UIContext::time() const {
    return platform_ ? platform_->time() : 0.0;
}

void UIContext::Update() {
    // Timing
    f64 now = platform_->time();
    dt_ = now - last_time_;
    last_time_ = now;

    // Poll input
    platform_->PollEvents();

    // Route input to widget tree
    if (root_)
        input_.Process(root_);

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
            root_);
    }

    // Update widgets (scroll momentum, etc.)
    if (root_)
        UpdateWidgetTree(root_, dt_);

    // Update vector animations
    for (auto* anim : vector_anims_) {
        if (anim)
            anim->Update(dt_);
    }

#if ULTRAGUI_LOTTIE
    for (auto* anim : lottie_anims_) {
        if (anim)
            anim->Update(dt_);
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
    text_engine_.FlushAtlas();

    // --- Offscreen render passes ---
    if (!offscreen_queue_.empty()) {
        if (!rhi_->AcquireFrame()) {
            offscreen_queue_.clear();
            return;
        }

        LayoutViewport vp{rhi_->display_size().x, rhi_->display_size().y};
        for (auto& pass : offscreen_queue_) {
            if (!rhi_->BeginOffscreen(pass.target, pass.clear_color))
                continue;

            renderer_.BeginFrame();
            if (pass.root) {
                ComputeWidgetLayout(pass.root, vp, layout_engine_, layout_nodes_);
                PaintWidgetTree(pass.root, renderer_);
            }
            text_engine_.FlushAtlas();
            renderer_.EndFrame();
            rhi_->EndOffscreen(pass.target);
        }
        offscreen_queue_.clear();
    }

    // --- Main swapchain render pass ---
    if (!rhi_->BeginFrame(config_.clear_color))
        return;

    renderer_.BeginFrame();

    if (on_paint_cb_) {
        on_paint_cb_(renderer_, rhi_);
    } else if (root_) {
        LayoutViewport vp{rhi_->display_size().x, rhi_->display_size().y};
        ComputeWidgetLayout(root_, vp, layout_engine_, layout_nodes_);
        PaintWidgetTree(root_, renderer_);
    }

    // Flush any new glyphs from paint pass
    text_engine_.FlushAtlas();

    renderer_.EndFrame();
    rhi_->EndFrame();
}

void UIContext::Shutdown() {
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
#if ULTRAGUI_AUDIO
    audio_.Shutdown();
#endif
    lua_.Shutdown();
    renderer_.Shutdown();
    text_engine_.Shutdown();

    if (rhi_) {
        rhi_->Shutdown();
        delete rhi_;
        rhi_ = nullptr;
    }

    if (platform_) {
        platform_->Shutdown();
        delete platform_;
        platform_ = nullptr;
    }
}

RHITextureHandle UIContext::LoadSvg(const char* path, u32 width, u32 height) {
    return LoadSvgTexture(rhi_, path, width, height);
}

VectorAnimation* UIContext::LoadAnim(const char* path, u32 width, u32 height) {
    auto* anim = new VectorAnimation();
    if (!anim->Load(rhi_, path, width, height)) {
        delete anim;
        return nullptr;
    }
    vector_anims_.push_back(anim);
    return anim;
}

#if ULTRAGUI_LOTTIE
LottieAnimation* UIContext::LoadLottie(const char* path, u32 width, u32 height) {
    auto* anim = new LottieAnimation();
    if (!anim->Load(rhi_, path, width, height)) {
        delete anim;
        return nullptr;
    }
    lottie_anims_.push_back(anim);
    return anim;
}
#endif

RHITextureHandle UIContext::CreateRenderTarget(u32 width, u32 height) {
    return rhi_ ? rhi_->CreateRenderTarget(width, height) : kInvalidTexture;
}

void UIContext::QueueOffscreen(RHITextureHandle target, Widget* root, Color clear_color) {
    offscreen_queue_.push_back({target, root, clear_color});
}

void UIContext::SetOnPaint(PaintCallback cb) {
    on_paint_cb_ = std::move(cb);
}

Widget* UIContext::FindWidget(const char* name) const {
    return ugui::FindWidget(root_, name);
}

} // namespace ugui
