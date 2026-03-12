#ifndef ULTRAGUI_VIDEO_VIDEO_H_
#define ULTRAGUI_VIDEO_VIDEO_H_

#include <ultragui/ultragui_config.h>
#include <ultragui/core/types.h>
#include <ultragui/rhi/rhi_types.h>

namespace ugui {

class RHI;
class AudioEngine;

/// MPEG-1 video player backed by pl_mpeg.
/// Decodes video frames on CPU, uploads YCbCr planes as R8 textures, and
/// converts to RGBA on the GPU via the video shader pipeline.
/// Optionally decodes MP2 audio and routes PCM to miniaudio for playback.
///
/// Usage:
///   VideoPlayer vid;
///   vid.Load(rhi, "cutscene.mpg");
///   vid.Play();
///   // each frame:
///   vid.Update(dt);             // decode + upload planes
///   vid.ConvertFrame();         // GPU YCbCr -> RGBA (call after AcquireFrame)
///   // use vid.texture() on an Image widget
class VideoPlayer {
 public:
  VideoPlayer();
  ~VideoPlayer();

  VideoPlayer(VideoPlayer&&) noexcept;
  VideoPlayer& operator=(VideoPlayer&&) noexcept;
  VideoPlayer(const VideoPlayer&) = delete;
  VideoPlayer& operator=(const VideoPlayer&) = delete;

  /// Load an MPEG-1 (.mpg) file and prepare for playback.
  /// If audio is non-null and the file has an audio stream, audio will
  /// be decoded and played back via miniaudio.
  bool Load(RHI* rhi, const char* path, AudioEngine* audio = nullptr);

  /// Advance playback by dt seconds. Decodes video frame and uploads to GPU.
  void Update(f64 dt);

  /// Release all CPU and GPU resources.
  void Unload();

  /// Returns true if successfully loaded.
  bool IsLoaded() const;

  // ----- Playback control -----

  void Play();
  void Pause();
  void Stop();  // reset to beginning
  void Seek(f64 seconds);
  void set_loop(bool loop);
  void set_speed(f32 speed);  // 1.0 = normal

  // ----- State queries -----

  bool IsPlaying() const;
  bool IsFinished() const;  // true when non-looping video has ended
  bool IsLooping() const;
  f32 speed() const;
  f64 duration() const;  // total duration in seconds
  f64 position() const;  // current time in seconds

  u32 width() const;
  u32 height() const;
  f64 frame_rate() const;

  /// The GPU texture containing the current decoded frame (RGBA).
  RHITextureHandle texture() const;

  /// Returns true if a new video frame has been decoded and needs GPU
  /// conversion.
  bool NeedsConvert() const;

  /// Run the GPU YCbCr -> RGBA conversion pass. Must be called after
  /// RHI::AcquireFrame() and outside any render pass. Called automatically
  /// by UIContext between animation updates and the main render pass.
  void ConvertFrame();

  /// Direct access to the YCbCr plane textures (R8 format).
  RHITextureHandle y_texture() const;
  RHITextureHandle cb_texture() const;
  RHITextureHandle cr_texture() const;

  // ----- Audio control -----

  void set_volume(f32 volume);
  void set_muted(bool muted);
  f32 volume() const;
  bool muted() const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace ugui

#endif  // ULTRAGUI_VIDEO_VIDEO_H_
