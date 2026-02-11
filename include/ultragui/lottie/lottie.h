#pragma once

#include <ultragui/core/types.h>
#include <ultragui/rhi/rhi_types.h>

namespace ugui {

class RHI;

using LottieHandle = u32;
static constexpr LottieHandle INVALID_LOTTIE = 0;

/// A Lottie animation backed by rlottie.
/// Renders frames on-demand to a GPU texture each update().
///
/// Usage:
///   LottieAnimation anim;
///   anim.load(rhi, "spinner.json", 64, 64);
///   anim.play();
///   // each frame:
///   anim.update(dt);
///   // use anim.texture() on an Image widget
class LottieAnimation {
public:
    LottieAnimation();
    ~LottieAnimation();

    LottieAnimation(LottieAnimation&&) noexcept;
    LottieAnimation& operator=(LottieAnimation&&) noexcept;
    LottieAnimation(const LottieAnimation&) = delete;
    LottieAnimation& operator=(const LottieAnimation&) = delete;

    /// Load a Lottie JSON file and create a GPU texture for rendering.
    bool load(RHI* rhi, const char* path, u32 width, u32 height);

    /// Load from a JSON string in memory.
    bool load_data(RHI* rhi, const char* json_data, const char* key, u32 width, u32 height);

    /// Advance the animation by dt seconds and re-render if the frame changed.
    void update(f64 dt);

    /// Returns true if successfully loaded.
    bool is_loaded() const;

    /// The GPU texture containing the current frame. Updated by update().
    RHITextureHandle texture() const;

    /// Rasterized dimensions.
    u32 width() const;
    u32 height() const;

    // ----- Playback control -----

    void play();
    void pause();
    void stop();          // resets to frame 0
    void set_loop(bool loop);
    void set_speed(f32 speed);  // 1.0 = normal, 2.0 = double speed
    void seek(f32 progress);    // 0.0 - 1.0

    bool is_playing() const;
    bool is_looping() const;
    f32 speed() const;

    /// Total duration in seconds at speed=1.
    f64 duration() const;

    /// Current playback position (0.0 - 1.0).
    f32 progress() const;

    /// Total frame count and current frame.
    u32 total_frames() const;
    u32 current_frame() const;
    f64 frame_rate() const;

    /// Release GPU and CPU resources.
    void unload();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace ugui
