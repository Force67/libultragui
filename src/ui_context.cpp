#include <ultragui/ui_context.h>
#include <ultragui/widgets/button.h>
#include <ultragui/widgets/text.h>

#if ULTRAGUI_AUDIO || ULTRAGUI_LOTTIE
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

#if ULTRAGUI_LOTTIE
#include <ultragui/widgets/image.h>
#endif

#include <cstdio>
#include <cstring>

namespace ugui {

// Forward declarations for internal helpers
static void set_fonts_on_tree(Widget* widget, FontHandle font, TextEngine* engine);
static void update_tree_recursive(Widget* widget, f64 dt);
static void measure_tree_recursive(Widget* widget);

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
    // Audio
    if (!audio_.init()) {
        std::fprintf(stderr, "ultragui: audio init failed (non-fatal)\n");
    }
    register_audio_lua();
#endif

#if ULTRAGUI_LOTTIE
    register_lottie_lua();
#endif

    // Builder
    builder_.set_text_engine(&text_engine_);

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
    // Apply to existing tree if any
    if (root_ && default_font_ != INVALID_FONT) {
        set_fonts_on_tree(root_, default_font_, &text_engine_);
    }
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
        if (default_font_ != INVALID_FONT)
            set_fonts_on_tree(root_, default_font_, &text_engine_);
        register_widgets_lua(root_);
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
        if (default_font_ != INVALID_FONT)
            set_fonts_on_tree(root_, default_font_, &text_engine_);
        register_widgets_lua(root_);
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
        if (default_font_ != INVALID_FONT)
            set_fonts_on_tree(root_, default_font_, &text_engine_);
        register_widgets_lua(root_);
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
    if (root_) {
        input_.process(root_);
    }

    // Update animations
    if (root_) {
        animator_.update(now, [](u32, const Style&, void*) {}, nullptr);
    }

    // Update widgets (for scroll momentum, etc.)
    if (root_) {
        update_tree_recursive(root_, dt_);
    }

#if ULTRAGUI_LOTTIE
    // Update all managed Lottie animations (re-renders changed frames to GPU)
    for (auto* anim : lottie_anims_) {
        if (anim)
            anim->update(dt_);
    }
#endif

    // --- Text shaping and atlas management (BEFORE render pass) ---
    // Measure all widgets to determine intrinsic sizes and rasterize glyphs.
    // Then flush the atlas to GPU while no render pass is active - this avoids
    // image layout transition conflicts.
    text_engine_.begin_frame();
    if (root_) {
        measure_tree_recursive(root_);
    }
    for (auto& pass : offscreen_queue_) {
        if (pass.root)
            measure_tree_recursive(pass.root);
    }
    text_engine_.flush_atlas();

    // --- Offscreen render passes ---
    if (!offscreen_queue_.empty()) {
        if (!rhi_->acquire_frame()) {
            offscreen_queue_.clear();
            return;
        }

        for (auto& pass : offscreen_queue_) {
            if (!rhi_->begin_offscreen(pass.target, pass.clear_color))
                continue;

            renderer_.begin_frame();
            if (pass.root) {
                compute_layout(pass.root);
                paint_tree(pass.root);
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
        compute_layout(root_);
        paint_tree(root_);
    }

    // If Button::on_paint() shaped text at a new size (e.g. hover state with
    // different font-size), new glyphs were rasterized. We must flush again
    // but this time inside the render pass. The atlas handle is stable (same
    // VkImage, in-place update), so batched draw commands remain valid.
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
    if (!root_)
        return nullptr;
    return find_widget_recursive(root_, name);
}

Widget* UIContext::find_widget_recursive(Widget* widget, const char* name) const {
    if (widget->name() == name)
        return widget;
    for (u32 i = 0; i < widget->child_count(); ++i) {
        Widget* found = find_widget_recursive(widget->child_at(i), name);
        if (found)
            return found;
    }
    return nullptr;
}

void UIContext::register_widgets_lua(Widget* widget) {
    if (!widget->name().empty()) {
        lua_.register_widget(widget);
    }
    for (u32 i = 0; i < widget->child_count(); ++i) {
        register_widgets_lua(widget->child_at(i));
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void UIContext::compute_layout(Widget* tree_root) {
    if (!tree_root)
        return;

    layout_nodes_.clear();
    build_layout_nodes(tree_root, ~0u);

    LayoutViewport vp;
    vp.width = rhi_->display_size().x;
    vp.height = rhi_->display_size().y;

    layout_engine_.compute(layout_nodes_.data(), static_cast<u32>(layout_nodes_.size()), vp);

    u32 idx = 0;
    apply_layout_results(tree_root, idx);
}

void UIContext::build_layout_nodes(Widget* widget, u32 parent_index) {
    u32 my_index = static_cast<u32>(layout_nodes_.size());

    LayoutNode node;
    widget->populate_layout_node(node);
    node.parent = parent_index;
    node.first_child = ~0u;
    node.next_sibling = ~0u;
    node.child_count = widget->child_count();

    layout_nodes_.push_back(node);

    // Link to parent
    if (parent_index != ~0u) {
        auto& parent_node = layout_nodes_[parent_index];
        if (parent_node.first_child == ~0u) {
            parent_node.first_child = my_index;
        } else {
            u32 sib = parent_node.first_child;
            while (layout_nodes_[sib].next_sibling != ~0u) {
                sib = layout_nodes_[sib].next_sibling;
            }
            layout_nodes_[sib].next_sibling = my_index;
        }
    }

    // Recurse children
    for (u32 i = 0; i < widget->child_count(); ++i) {
        build_layout_nodes(widget->child_at(i), my_index);
    }
}

void UIContext::apply_layout_results(Widget* widget, u32& node_index) {
    auto& node = layout_nodes_[node_index];
    widget->apply_layout_result(node);
    widget->on_layout(node.computed_rect, node.content_rect);

    ++node_index;
    for (u32 i = 0; i < widget->child_count(); ++i) {
        apply_layout_results(widget->child_at(i), node_index);
    }
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void UIContext::paint_tree(Widget* widget) {
    auto s = widget->computed_style();
    if (s.visibility == Visibility::Hidden || s.visibility == Visibility::Collapsed)
        return;

    bool clip = s.overflow == Overflow::Hidden || s.overflow == Overflow::Scroll;
    if (clip) {
        renderer_.push_scissor(widget->rect());
    }

    widget->on_paint(renderer_);

    for (u32 i = 0; i < widget->child_count(); ++i) {
        paint_tree(widget->child_at(i));
    }

    if (clip) {
        renderer_.pop_scissor();
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void set_fonts_on_tree(Widget* widget, FontHandle font, TextEngine* engine) {
    if (auto* text = dynamic_cast<Text*>(widget)) {
        if (text->font() == INVALID_FONT) {
            text->set_font(font);
            text->set_text_engine(engine);
        }
    }
    if (auto* btn = dynamic_cast<Button*>(widget)) {
        btn->set_font(font);
        btn->set_text_engine(engine);
    }
    for (u32 i = 0; i < widget->child_count(); ++i) {
        set_fonts_on_tree(widget->child_at(i), font, engine);
    }
}

static void update_tree_recursive(Widget* widget, f64 dt) {
    widget->on_update(dt);
    for (u32 i = 0; i < widget->child_count(); ++i) {
        update_tree_recursive(widget->child_at(i), dt);
    }
}

static void measure_tree_recursive(Widget* widget) {
    for (u32 i = 0; i < widget->child_count(); ++i) {
        measure_tree_recursive(widget->child_at(i));
    }
    f32 w = 0, h = 0;
    widget->measure(w, h);
    widget->set_intrinsic_size(w, h);
}

// ---------------------------------------------------------------------------
// Audio Lua bindings
// ---------------------------------------------------------------------------

#if ULTRAGUI_AUDIO
void UIContext::register_audio_lua() {
    // ugui.play_sound(path [, volume [, loop]]) -> handle
    lua_.register_function("play_sound", [this](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        f32 volume = lua_isnumber(L, 2) ? static_cast<f32>(lua_tonumber(L, 2)) : 1.0f;
        bool loop = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;
        SoundHandle h = audio_.play(path, volume, loop);
        lua_pushinteger(L, h);
        return 1;
    });

    // ugui.load_sound(path) -> handle
    lua_.register_function("load_sound", [this](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        SoundHandle h = audio_.load(path);
        lua_pushinteger(L, h);
        return 1;
    });

    // ugui.play_loaded(handle [, volume [, loop]]) -> new handle
    lua_.register_function("play_loaded", [this](lua_State* L) -> int {
        SoundHandle src = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 volume = lua_isnumber(L, 2) ? static_cast<f32>(lua_tonumber(L, 2)) : 1.0f;
        bool loop = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;
        SoundHandle h = audio_.play_loaded(src, volume, loop);
        lua_pushinteger(L, h);
        return 1;
    });

    // ugui.stop_sound(handle)
    lua_.register_function("stop_sound", [this](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        audio_.stop(h);
        return 0;
    });

    // ugui.sound_playing(handle) -> bool
    lua_.register_function("sound_playing", [this](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        lua_pushboolean(L, audio_.is_playing(h));
        return 1;
    });

    // ugui.sound_volume(handle, volume)
    lua_.register_function("sound_volume", [this](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 vol = static_cast<f32>(luaL_checknumber(L, 2));
        audio_.set_volume(h, vol);
        return 0;
    });

    // ugui.sound_pan(handle, pan)
    lua_.register_function("sound_pan", [this](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 pan = static_cast<f32>(luaL_checknumber(L, 2));
        audio_.set_pan(h, pan);
        return 0;
    });

    // ugui.sound_pitch(handle, pitch)
    lua_.register_function("sound_pitch", [this](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 pitch = static_cast<f32>(luaL_checknumber(L, 2));
        audio_.set_pitch(h, pitch);
        return 0;
    });

    // ugui.master_volume(volume) or ugui.master_volume() -> number
    lua_.register_function("master_volume", [this](lua_State* L) -> int {
        if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
            audio_.set_master_volume(static_cast<f32>(lua_tonumber(L, 1)));
            return 0;
        }
        lua_pushnumber(L, audio_.master_volume());
        return 1;
    });

    // ugui.stop_all_sounds()
    lua_.register_function("stop_all_sounds", [this](lua_State*) -> int {
        audio_.stop_all();
        return 0;
    });

    // ugui.pause_all_sounds()
    lua_.register_function("pause_all_sounds", [this](lua_State*) -> int {
        audio_.pause_all();
        return 0;
    });

    // ugui.resume_all_sounds()
    lua_.register_function("resume_all_sounds", [this](lua_State*) -> int {
        audio_.resume_all();
        return 0;
    });
}
#endif

// ---------------------------------------------------------------------------
// Lottie Lua bindings
// ---------------------------------------------------------------------------

#if ULTRAGUI_LOTTIE
void UIContext::register_lottie_lua() {
    // ugui.load_lottie(path, width, height) -> anim lightuserdata
    lua_.register_function("load_lottie", [this](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        u32 w = static_cast<u32>(luaL_checkinteger(L, 2));
        u32 h = static_cast<u32>(luaL_checkinteger(L, 3));
        LottieAnimation* anim = load_lottie(path, w, h);
        if (anim)
            lua_pushlightuserdata(L, anim);
        else
            lua_pushnil(L);
        return 1;
    });

    // ugui.lottie_play(anim)
    lua_.register_function("lottie_play", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->play();
        return 0;
    });

    // ugui.lottie_pause(anim)
    lua_.register_function("lottie_pause", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->pause();
        return 0;
    });

    // ugui.lottie_stop(anim)
    lua_.register_function("lottie_stop", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->stop();
        return 0;
    });

    // ugui.lottie_loop(anim, bool)
    lua_.register_function("lottie_loop", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->set_loop(lua_toboolean(L, 2));
        return 0;
    });

    // ugui.lottie_speed(anim, speed)
    lua_.register_function("lottie_speed", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->set_speed(static_cast<f32>(luaL_checknumber(L, 2)));
        return 0;
    });

    // ugui.lottie_seek(anim, progress)  -- 0.0 to 1.0
    lua_.register_function("lottie_seek", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->seek(static_cast<f32>(luaL_checknumber(L, 2)));
        return 0;
    });

    // ugui.lottie_playing(anim) -> bool
    lua_.register_function("lottie_playing", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        lua_pushboolean(L, a && a->is_playing());
        return 1;
    });

    // ugui.lottie_progress(anim) -> number
    lua_.register_function("lottie_progress", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        lua_pushnumber(L, a ? a->progress() : 0);
        return 1;
    });

    // ugui.lottie_duration(anim) -> number (seconds)
    lua_.register_function("lottie_duration", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        lua_pushnumber(L, a ? a->duration() : 0);
        return 1;
    });

    // ugui.lottie_attach(anim, "widget_name") - set texture on an Image widget
    lua_.register_function("lottie_attach", [this](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        const char* name = luaL_checkstring(L, 2);
        if (a) {
            Widget* w = find_widget(name);
            if (auto* img = dynamic_cast<Image*>(w)) {
                img->set_texture(a->texture(), static_cast<f32>(a->width()),
                                 static_cast<f32>(a->height()));
            }
        }
        return 0;
    });
}
#endif

} // namespace ugui
