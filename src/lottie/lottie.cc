#include <ultragui/core/config.h>
#include <ultragui/lottie/lottie.h>
#include <ultragui/rhi/rhi.h>

#include <rlottie.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace ugui {

struct LottieAnimation::Impl {
    std::unique_ptr<rlottie::Animation> anim;
    RHI* rhi = nullptr;
    RHITextureHandle texture = kInvalidTexture;
    u32 width = 0;
    u32 height = 0;

    Vector<u32> pixel_buf; // ARGB32 premultiplied (rlottie native format)
    Vector<u8> rgba_buf;   // RGBA8 straight alpha (for GPU upload)

    // Playback state
    bool playing = false;
    bool looping = true;
    f32 speed = 1.0f;
    f64 current_time = 0.0;
    u32 last_rendered_frame = ~0u;

    // Animation info (cached from rlottie)
    usize total_frames = 0;
    f64 frame_rate = 0.0;
    f64 duration = 0.0;

    void render_frame(u32 frame_no) {
        if (!anim || !rhi || frame_no == last_rendered_frame)
            return;

        // Clear buffer to transparent
        std::memset(pixel_buf.data(), 0, pixel_buf.size() * sizeof(u32));

        rlottie::Surface surface(pixel_buf.data(), width, height, width * sizeof(u32));
        anim->renderSync(frame_no, surface);

        // Convert ARGB32 premultiplied -> RGBA8 straight alpha
        for (u32 i = 0; i < width * height; ++i) {
            u32 argb = pixel_buf[i];
            u8 a = static_cast<u8>((argb >> 24) & 0xFF);
            u8 r = static_cast<u8>((argb >> 16) & 0xFF);
            u8 g = static_cast<u8>((argb >> 8) & 0xFF);
            u8 b = static_cast<u8>(argb & 0xFF);

            // Un-premultiply
            if (a > 0 && a < 255) {
                f32 inv_a = 255.0f / static_cast<f32>(a);
                r = static_cast<u8>(std::min(255.0f, r * inv_a));
                g = static_cast<u8>(std::min(255.0f, g * inv_a));
                b = static_cast<u8>(std::min(255.0f, b * inv_a));
            }

            u32 idx = i * 4;
            rgba_buf[idx + 0] = r;
            rgba_buf[idx + 1] = g;
            rgba_buf[idx + 2] = b;
            rgba_buf[idx + 3] = a;
        }

        // Upload to GPU
        if (texture == kInvalidTexture) {
            texture = rhi->CreateTexture(width, height, RHIFormat::kRgba8Unorm, rgba_buf.data());
        } else {
            rhi->UpdateTexture(texture, rgba_buf.data());
        }

        last_rendered_frame = frame_no;
    }
};

// ============================================================================
// LottieAnimation
// ============================================================================

LottieAnimation::LottieAnimation() = default;

LottieAnimation::~LottieAnimation() {
    if (impl_)
        Unload();
}

LottieAnimation::LottieAnimation(LottieAnimation&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

LottieAnimation& LottieAnimation::operator=(LottieAnimation&& other) noexcept {
    if (this != &other) {
        if (impl_)
            Unload();
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

bool LottieAnimation::Load(RHI* rhi, const char* path, u32 w, u32 h) {
    if (impl_)
        Unload();

    auto anim = rlottie::Animation::loadFromFile(path);
    if (!anim) {
        std::fprintf(stderr, "ultragui/lottie: failed to load '%s'\n", path);
        return false;
    }

    impl_ = new Impl();
    impl_->anim = std::move(anim);
    impl_->rhi = rhi;
    impl_->width = w;
    impl_->height = h;
    impl_->pixel_buf.resize(w * h, 0);
    impl_->rgba_buf.resize(w * h * 4, 0);

    impl_->total_frames = impl_->anim->totalFrame();
    impl_->frame_rate = impl_->anim->frameRate();
    impl_->duration = impl_->anim->duration();

    // Render first frame immediately
    impl_->render_frame(0);
    return true;
}

bool LottieAnimation::LoadData(RHI* rhi, const char* json_data, const char* key, u32 w, u32 h) {
    if (impl_)
        Unload();

    auto anim = rlottie::Animation::loadFromData(String(json_data), String(key));
    if (!anim) {
        std::fprintf(stderr, "ultragui/lottie: failed to parse lottie data\n");
        return false;
    }

    impl_ = new Impl();
    impl_->anim = std::move(anim);
    impl_->rhi = rhi;
    impl_->width = w;
    impl_->height = h;
    impl_->pixel_buf.resize(w * h, 0);
    impl_->rgba_buf.resize(w * h * 4, 0);

    impl_->total_frames = impl_->anim->totalFrame();
    impl_->frame_rate = impl_->anim->frameRate();
    impl_->duration = impl_->anim->duration();

    impl_->render_frame(0);
    return true;
}

void LottieAnimation::Update(f64 dt) {
    if (!impl_ || !impl_->playing || impl_->total_frames == 0)
        return;

    impl_->current_time += dt * impl_->speed;

    f64 dur = impl_->duration;
    if (dur <= 0)
        return;

    if (impl_->looping) {
        // Wrap time
        while (impl_->current_time >= dur)
            impl_->current_time -= dur;
        while (impl_->current_time < 0)
            impl_->current_time += dur;
    } else {
        if (impl_->current_time >= dur) {
            impl_->current_time = dur;
            impl_->playing = false;
        }
        if (impl_->current_time < 0) {
            impl_->current_time = 0;
            impl_->playing = false;
        }
    }

    // Map time to frame
    f64 pos = impl_->current_time / dur;
    pos = std::clamp(pos, 0.0, 1.0);
    usize frame = impl_->anim->frameAtPos(pos);
    impl_->render_frame(static_cast<u32>(frame));
}

bool LottieAnimation::IsLoaded() const {
    return impl_ && impl_->anim;
}

RHITextureHandle LottieAnimation::texture() const {
    return impl_ ? impl_->texture : kInvalidTexture;
}

u32 LottieAnimation::width() const {
    return impl_ ? impl_->width : 0;
}

u32 LottieAnimation::height() const {
    return impl_ ? impl_->height : 0;
}

void LottieAnimation::Play() {
    if (impl_)
        impl_->playing = true;
}

void LottieAnimation::Pause() {
    if (impl_)
        impl_->playing = false;
}

void LottieAnimation::Stop() {
    if (!impl_)
        return;
    impl_->playing = false;
    impl_->current_time = 0.0;
    impl_->render_frame(0);
}

void LottieAnimation::set_loop(bool loop) {
    if (impl_)
        impl_->looping = loop;
}

void LottieAnimation::set_speed(f32 spd) {
    if (impl_)
        impl_->speed = spd;
}

void LottieAnimation::Seek(f32 prog) {
    if (!impl_ || impl_->total_frames == 0)
        return;
    prog = std::clamp(prog, 0.0f, 1.0f);
    impl_->current_time = prog * impl_->duration;
    usize frame = impl_->anim->frameAtPos(prog);
    impl_->render_frame(static_cast<u32>(frame));
}

bool LottieAnimation::IsPlaying() const {
    return impl_ && impl_->playing;
}

bool LottieAnimation::IsLooping() const {
    return impl_ && impl_->looping;
}

f32 LottieAnimation::speed() const {
    return impl_ ? impl_->speed : 1.0f;
}

f64 LottieAnimation::duration() const {
    return impl_ ? impl_->duration : 0.0;
}

f32 LottieAnimation::progress() const {
    if (!impl_ || impl_->duration <= 0)
        return 0.0f;
    return static_cast<f32>(impl_->current_time / impl_->duration);
}

u32 LottieAnimation::total_frames() const {
    return impl_ ? static_cast<u32>(impl_->total_frames) : 0;
}

u32 LottieAnimation::current_frame() const {
    if (!impl_ || impl_->total_frames == 0)
        return 0;
    f64 pos = impl_->duration > 0 ? impl_->current_time / impl_->duration : 0;
    pos = std::clamp(pos, 0.0, 1.0);
    return static_cast<u32>(impl_->anim->frameAtPos(pos));
}

f64 LottieAnimation::frame_rate() const {
    return impl_ ? impl_->frame_rate : 0.0;
}

void LottieAnimation::Unload() {
    if (!impl_)
        return;
    if (impl_->texture != kInvalidTexture && impl_->rhi) {
        impl_->rhi->DestroyTexture(impl_->texture);
    }
    delete impl_;
    impl_ = nullptr;
}

} // namespace ugui
