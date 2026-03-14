#ifndef ULTRAGUI_AUDIO_AUDIO_BACKEND_H_
#define ULTRAGUI_AUDIO_AUDIO_BACKEND_H_

#include <ultragui/core/types.h>

namespace ugui {

using SoundHandle = u32;
static constexpr SoundHandle kInvalidSound = 0;

/// Abstract audio backend interface.
///
/// `UIContext` talks to audio exclusively through this interface, so a host
/// application can supply its own implementation in two ways:
///
///   1. Runtime injection: pass an instance to `UIContext::set_audio()` before
///      `Init()`. The host owns its lifetime.
///   2. Link-time swap: point the `ULTRAGUI_AUDIO_SOURCE` CMake variable at a
///      .cc that defines `CreateDefaultAudioBackend()` returning your backend.
///
/// The bundled miniaudio implementation is `AudioEngine` (see audio.h).
class AudioBackend {
 public:
  AudioBackend() = default;
  virtual ~AudioBackend() = default;
  AudioBackend(const AudioBackend&) = delete;
  AudioBackend& operator=(const AudioBackend&) = delete;

  virtual bool Init() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsInitialized() const = 0;

  // ----- Playback -----
  virtual SoundHandle Play(const char* path, f32 volume = 1.0f,
                           bool loop = false) = 0;
  virtual SoundHandle Load(const char* path) = 0;
  virtual SoundHandle PlayLoaded(SoundHandle preloaded, f32 volume = 1.0f,
                                 bool loop = false) = 0;
  virtual void Stop(SoundHandle handle) = 0;
  virtual bool IsPlaying(SoundHandle handle) const = 0;

  // ----- Per-sound control -----
  virtual void set_volume(SoundHandle handle, f32 volume) = 0;
  virtual void set_pan(SoundHandle handle, f32 pan) = 0;
  virtual void set_pitch(SoundHandle handle, f32 pitch) = 0;
  virtual void Seek(SoundHandle handle, f32 seconds) = 0;

  // ----- Global control -----
  virtual void set_master_volume(f32 volume) = 0;
  virtual f32 master_volume() const = 0;
  virtual void StopAll() = 0;
  virtual void PauseAll() = 0;
  virtual void ResumeAll() = 0;
};

/// Create the default audio backend. Defined in ULTRAGUI_AUDIO_SOURCE
/// (src/audio/audio.cc by default, returning the miniaudio AudioEngine).
/// Caller owns the returned instance.
AudioBackend* CreateDefaultAudioBackend();

}  // namespace ugui

#endif  // ULTRAGUI_AUDIO_AUDIO_BACKEND_H_
