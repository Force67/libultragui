#ifndef ULTRAGUI_AUDIO_AUDIO_BACKEND_H_
#define ULTRAGUI_AUDIO_AUDIO_BACKEND_H_

#include <ugui/core/types.h>

namespace ugui {

using SoundHandle = u32;
static constexpr SoundHandle kInvalidSound = 0;

/// Abstract audio backend interface.
///
/// `UIContext` talks to audio exclusively through this interface. Like the
/// renderer backends, the implementation is wired in by the application, not
/// baked into the library: compile a backend .cc into your app and hand an
/// instance to `UIContext::set_audio()` before `Init()` (the host owns its
/// lifetime). The bundled implementation is the miniaudio `AudioEngine` in
/// ugui/backends/ugui_impl_miniaudio. Until one is wired, audio is a silent
/// no-op (see NullAudioBackend).
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

/// No-op audio backend. The default until a real backend is wired via
/// UIContext::set_audio(), so audio calls are silent rather than crashing.
/// Compile a backend (e.g. ugui/backends/ugui_impl_miniaudio.cc) into your app
/// and wire it in to get sound.
class NullAudioBackend final : public AudioBackend {
 public:
  bool Init() override { return true; }
  void Shutdown() override {}
  bool IsInitialized() const override { return false; }
  SoundHandle Play(const char*, f32, bool) override { return kInvalidSound; }
  SoundHandle Load(const char*) override { return kInvalidSound; }
  SoundHandle PlayLoaded(SoundHandle, f32, bool) override {
    return kInvalidSound;
  }
  void Stop(SoundHandle) override {}
  bool IsPlaying(SoundHandle) const override { return false; }
  void set_volume(SoundHandle, f32) override {}
  void set_pan(SoundHandle, f32) override {}
  void set_pitch(SoundHandle, f32) override {}
  void Seek(SoundHandle, f32) override {}
  void set_master_volume(f32) override {}
  f32 master_volume() const override { return 1.0f; }
  void StopAll() override {}
  void PauseAll() override {}
  void ResumeAll() override {}
};

}  // namespace ugui

#endif  // ULTRAGUI_AUDIO_AUDIO_BACKEND_H_
