#include <ultragui/ui_context.h>
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
        shutdown();
}

bool UIContext::init(const UIConfig& config) {
    config_ = config;

    // Platform
    platform_ = create_glfw_platform();
    Platform::WindowConfig wcfg;
    wcfg.width = config.width;
    wcfg.height = config.height;
    wcfg.title = config.title;
    wcfg.resizable = config.resizable;
    wcfg.vsync = config.vsync;

    if (!platform_->init(wcfg)) {
        std::fprintf(stderr, "ultragui: failed to initialize platform\n");
        return false;
    }

    // RHI
    rhi_ = create_vulkan_rhi();
    RHIConfig rcfg;
    rcfg.platform = platform_;
    rcfg.validation = config.validation;
    rcfg.vsync = config.vsync;
    rcfg.shader_dir = config.shader_dir;

    if (!rhi_->init(rcfg)) {
        std::fprintf(stderr, "ultragui: failed to initialize Vulkan RHI\n");
        platform_->shutdown();
        delete platform_;
        platform_ = nullptr;
        return false;
    }

    // Renderer
    renderer_.init(rhi_);

    // Text engine
    if (!text_engine_.init(rhi_)) {
        std::fprintf(stderr, "ultragui: failed to initialize text engine\n");
    }

    // Input
    input_.init(platform_);

    // Lua
    lua_.init();

#if ULTRAGUI_AUDIO
    if (!audio_.init()) {
        std::fprintf(stderr, "ultragui: audio init failed (non-fatal)\n");
    }
    register_audio_lua(lua_, audio_);
#endif

#if ULTRAGUI_LOTTIE
    register_lottie_lua(
        lua_,
        [this](const char* path, unsigned w, unsigned h) -> LottieAnimation* {
            return load_lottie(path, w, h);
        },
        [this](const char* name) -> Widget* { return find_widget(name); });
#endif

    // Widget context
    widget_ctx_.text_engine = &text_engine_;

    last_time_ = platform_->time();
    return true;
}

FontHandle UIContext::load_font(const char* path) {
    FontHandle font = text_engine_.load_font(path);
    if (font == INVALID_FONT) {
        std::fprintf(stderr, "ultragui: failed to load font '%s'\n", path);
    }
    return font;
}

void UIContext::set_default_font(FontHandle font) {
    default_font_ = font;
    widget_ctx_.default_font = font;
}

Widget* UIContext::load_ui(const char* path) {
    UguiDocument doc;
    std::vector<ParseError> errors;

    if (!parse_ugui_file(path, doc, errors)) {
        for (auto& e : errors) {
            std::fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(), e.line,
                         e.column, e.message.c_str());
        }
        return nullptr;
    }

    if (owns_root_)
        delete root_;

    root_ = builder_.build(doc);
    owns_root_ = true;

    if (root_) {
        root_->set_context(&widget_ctx_);
        register_widget_tree_lua(lua_, root_);
    }

    return root_;
}

Widget* UIContext::load_ui_string(const char* source, const char* name) {
    UguiDocument doc;
    std::vector<ParseError> errors;

    if (!parse_ugui(source, std::strlen(source), name, doc, errors)) {
        for (auto& e : errors) {
            std::fprintf(stderr, "ultragui: parse error in %s:%u:%u: %s\n", e.file.c_str(), e.line,
                         e.column, e.message.c_str());
        }
        return nullptr;
    }

    if (owns_root_)
        delete root_;

    root_ = builder_.build(doc);
    owns_root_ = true;

    if (root_) {
        root_->set_context(&widget_ctx_);
        register_widget_tree_lua(lua_, root_);
    }

    return root_;
}

bool UIContext::load_script(const char* path) {
    return lua_.exec_file(path);
}

bool UIContext::exec_script(const char* script, const char* name) {
    return lua_.exec(script, name);
}

void UIContext::set_root(Widget* root) {
    if (owns_root_)
        delete root_;
    root_ = root;
    owns_root_ = false;
    if (root_) {
        root_->set_context(&widget_ctx_);
        register_widget_tree_lua(lua_, root_);
    }
}

bool UIContext::running() const {
    return platform_ && !platform_->should_close();
}

f64 UIContext::time() const {
    return platform_ ? platform_->time() : 0.0;
}

void UIContext::update() {
    // Timing
    f64 now = platform_->time();
    dt_ = now - last_time_;
    last_time_ = now;

    // Poll input
    platform_->poll_events();

    // Route input to widget tree
    if (root_)
        input_.process(root_);

    // Update animations
    if (root_)
        animator_.update(now, [](u32, const Style&, void*) {}, nullptr);

    // Update widgets (scroll momentum, etc.)
    if (root_)
        update_widget_tree(root_, dt_);

#if ULTRAGUI_LOTTIE
    for (auto* anim : lottie_anims_) {
        if (anim)
            anim->update(dt_);
    }
#endif

    // --- Text shaping and atlas management (BEFORE render pass) ---
    text_engine_.begin_frame();
    if (root_)
        measure_widget_tree(root_);
    for (auto& pass : offscreen_queue_) {
        if (pass.root)
            measure_widget_tree(pass.root);
    }
    text_engine_.flush_atlas();

    // --- Offscreen render passes ---
    if (!offscreen_queue_.empty()) {
        if (!rhi_->acquire_frame()) {
            offscreen_queue_.clear();
            return;
        }

        LayoutViewport vp{rhi_->display_size().x, rhi_->display_size().y};
        for (auto& pass : offscreen_queue_) {
            if (!rhi_->begin_offscreen(pass.target, pass.clear_color))
                continue;

            renderer_.begin_frame();
            if (pass.root) {
                compute_widget_layout(pass.root, vp, layout_engine_, layout_nodes_);
                paint_widget_tree(pass.root, renderer_);
            }
            text_engine_.flush_atlas();
            renderer_.end_frame();
            rhi_->end_offscreen(pass.target);
        }
        offscreen_queue_.clear();
    }

    // --- Main swapchain render pass ---
    if (!rhi_->begin_frame(config_.clear_color))
        return;

    renderer_.begin_frame();

    if (on_paint_cb_) {
        on_paint_cb_(renderer_, rhi_);
    } else if (root_) {
        LayoutViewport vp{rhi_->display_size().x, rhi_->display_size().y};
        compute_widget_layout(root_, vp, layout_engine_, layout_nodes_);
        paint_widget_tree(root_, renderer_);
    }

    // Flush any new glyphs from paint pass
    text_engine_.flush_atlas();

    renderer_.end_frame();
    rhi_->end_frame();
}

void UIContext::shutdown() {
    if (owns_root_) {
        delete root_;
        root_ = nullptr;
        owns_root_ = false;
    }

#if ULTRAGUI_LOTTIE
    for (auto* anim : lottie_anims_)
        delete anim;
    lottie_anims_.clear();
#endif
#if ULTRAGUI_AUDIO
    audio_.shutdown();
#endif
    lua_.shutdown();
    renderer_.shutdown();
    text_engine_.shutdown();

    if (rhi_) {
        rhi_->shutdown();
        delete rhi_;
        rhi_ = nullptr;
    }

    if (platform_) {
        platform_->shutdown();
        delete platform_;
        platform_ = nullptr;
    }
}

RHITextureHandle UIContext::load_svg(const char* path, u32 width, u32 height) {
    return load_svg_texture(rhi_, path, width, height);
}

#if ULTRAGUI_LOTTIE
LottieAnimation* UIContext::load_lottie(const char* path, u32 width, u32 height) {
    auto* anim = new LottieAnimation();
    if (!anim->load(rhi_, path, width, height)) {
        delete anim;
        return nullptr;
    }
    lottie_anims_.push_back(anim);
    return anim;
}
#endif

RHITextureHandle UIContext::create_render_target(u32 width, u32 height) {
    return rhi_ ? rhi_->create_render_target(width, height) : INVALID_TEXTURE;
}

void UIContext::queue_offscreen(RHITextureHandle target, Widget* root, Color clear_color) {
    offscreen_queue_.push_back({target, root, clear_color});
}

void UIContext::set_on_paint(PaintCallback cb) {
    on_paint_cb_ = std::move(cb);
}

Widget* UIContext::find_widget(const char* name) const {
    return ugui::find_widget(root_, name);
}

} // namespace ugui
