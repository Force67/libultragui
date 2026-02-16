#ifndef ULTRAGUI_LOTTIE_LOTTIE_H_
#define ULTRAGUI_LOTTIE_LOTTIE_H_

#include <ultragui/core/types.h>
#include <ultragui/rhi/rhi_types.h>

namespace ugui {

class RHI;

using LottieHandle = u32;
static constexpr LottieHandle kInvalidLottie = 0;

/// A Lottie animation backed by rlottie.
/// Renders frames on-demand to a GPU texture each Update().
///
/// Usage:
///   LottieAnimation anim;
///   anim.Load(rhi, "spinner.json", 64, 64);
///   anim.Play();
///   // each frame:
///   anim.Update(dt);
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
    bool Load(RHI* rhi, const char* path, u32 width, u32 height);

    /// Load from a JSON string in memory.
    bool LoadData(RHI* rhi, const char* json_data, const char* key, u32 width, u32 height);

    /// Advance the animation by dt seconds and re-render if the frame changed.
    void Update(f64 dt);

    /// Returns true if successfully loaded.
    bool IsLoaded() const;

    /// The GPU texture containing the current frame. Updated by Update().
    RHITextureHandle texture() const;

    /// Rasterized dimensions.
    u32 width() const;
    u32 height() const;

    // ----- Playback control -----

    void Play();
    void Pause();
    void Stop();          // resets to frame 0
    void set_loop(bool loop);
    void set_speed(f32 speed);  // 1.0 = normal, 2.0 = double speed
    void Seek(f32 progress);    // 0.0 - 1.0

    bool IsPlaying() const;
    bool IsLooping() const;
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
    void Unload();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace ugui

#endif  // ULTRAGUI_LOTTIE_LOTTIE_H_
