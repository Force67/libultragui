#include <ultragui/anim/vector_animation.h>
#include <ultragui/anim/anim_types.h>
#include <ultragui/rhi/rhi.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace ugui {

// Declared in anim_parser.cpp
bool parse_anim_file(const char* path, AnimDocument& out);
bool parse_anim_data(const char* json_data, usize length, AnimDocument& out);

// Declared in anim_render.cpp
void render_anim_frame(const AnimDocument& doc, f32 time, u8* pixels, u32 width, u32 height);

struct VectorAnimation::Impl {
    AnimDocument doc;
    RHI* rhi = nullptr;
    RHITextureHandle texture_handle = INVALID_TEXTURE;
    u32 w = 0, h = 0;
    std::vector<u8> rgba_buf;

    bool playing = false;
    f32 speed_val = 1.0f;
    f64 current_time = 0;
    f32 last_rendered_t = -1.0f;
};

VectorAnimation::~VectorAnimation() {
    unload();
}

bool VectorAnimation::load(RHI* rhi, const char* path, u32 width, u32 height) {
    unload();
    auto* p = new Impl();
    if (!parse_anim_file(path, p->doc)) {
        std::fprintf(stderr, "ultragui/anim: failed to parse '%s'\n", path);
        delete p;
        return false;
    }
    p->rhi = rhi;
    p->w = width;
    p->h = height;
    p->rgba_buf.resize(width * height * 4, 0);

    // Render first frame
    render_anim_frame(p->doc, 0, p->rgba_buf.data(), width, height);
    p->texture_handle = rhi->create_texture(width, height, RHIFormat::RGBA8_UNORM,
                                            p->rgba_buf.data(), RHIFilter::Linear);
    p->last_rendered_t = 0;

    impl_ = p;
    return true;
}

bool VectorAnimation::load_data(RHI* rhi, const char* json_data, usize length, u32 width, u32 height) {
    unload();
    auto* p = new Impl();
    if (!parse_anim_data(json_data, length, p->doc)) {
        delete p;
        return false;
    }
    p->rhi = rhi;
    p->w = width;
    p->h = height;
    p->rgba_buf.resize(width * height * 4, 0);

    render_anim_frame(p->doc, 0, p->rgba_buf.data(), width, height);
    p->texture_handle = rhi->create_texture(width, height, RHIFormat::RGBA8_UNORM,
                                            p->rgba_buf.data(), RHIFilter::Linear);
    p->last_rendered_t = 0;

    impl_ = p;
    return true;
}

void VectorAnimation::update(f64 dt) {
    if (!impl_ || !impl_->playing) return;

    impl_->current_time += dt * impl_->speed_val;
    f64 dur = impl_->doc.duration;

    if (impl_->doc.loop) {
        while (impl_->current_time >= dur)
            impl_->current_time -= dur;
        if (impl_->current_time < 0)
            impl_->current_time += dur;
    } else {
        if (impl_->current_time >= dur) {
            impl_->current_time = dur;
            impl_->playing = false;
        }
    }

    f32 t = static_cast<f32>(impl_->current_time);

    // Only re-render if time changed enough (~60fps threshold)
    if (std::abs(t - impl_->last_rendered_t) > 0.012f) {
        render_anim_frame(impl_->doc, t, impl_->rgba_buf.data(), impl_->w, impl_->h);
        impl_->rhi->update_texture(impl_->texture_handle, impl_->rgba_buf.data());
        impl_->last_rendered_t = t;
    }
}

void VectorAnimation::unload() {
    if (impl_) {
        if (impl_->texture_handle != INVALID_TEXTURE && impl_->rhi)
            impl_->rhi->destroy_texture(impl_->texture_handle);
        delete impl_;
        impl_ = nullptr;
    }
}

bool VectorAnimation::is_loaded() const { return impl_ != nullptr; }

RHITextureHandle VectorAnimation::texture() const {
    return impl_ ? impl_->texture_handle : INVALID_TEXTURE;
}

u32 VectorAnimation::width() const { return impl_ ? impl_->w : 0; }
u32 VectorAnimation::height() const { return impl_ ? impl_->h : 0; }

void VectorAnimation::play() { if (impl_) impl_->playing = true; }
void VectorAnimation::pause() { if (impl_) impl_->playing = false; }
void VectorAnimation::stop() {
    if (impl_) {
        impl_->playing = false;
        impl_->current_time = 0;
    }
}

void VectorAnimation::set_loop(bool loop) { if (impl_) impl_->doc.loop = loop; }
void VectorAnimation::set_speed(f32 speed) { if (impl_) impl_->speed_val = speed; }

void VectorAnimation::seek(f32 progress) {
    if (impl_) {
        impl_->current_time = progress * impl_->doc.duration;
    }
}

bool VectorAnimation::is_playing() const { return impl_ && impl_->playing; }
f32 VectorAnimation::speed() const { return impl_ ? impl_->speed_val : 1.0f; }
f64 VectorAnimation::duration() const { return impl_ ? impl_->doc.duration : 0; }

f32 VectorAnimation::progress() const {
    if (!impl_ || impl_->doc.duration <= 0) return 0;
    return static_cast<f32>(impl_->current_time / impl_->doc.duration);
}

} // namespace ugui
