#ifndef UGUI_BACKENDS_UGUI_IMPL_MINIAUDIO_H_
#define UGUI_BACKENDS_UGUI_IMPL_MINIAUDIO_H_

#include <ugui/audio/audio_backend.h>

namespace ugui {

/// miniaudio-backed AudioBackend, the bundled audio backend. Wire it in the
/// same way as the renderer backends (ugui_impl_vulkan / ugui_impl_opengl3):
/// compile ugui/backends/ugui_impl_miniaudio.cc into your application, then
/// hand an instance to the context before Init():
///
///   ugui::AudioEngine audio;
///   ui.set_audio(&audio);   // before ui.Init(); UIContext init/shuts it down
///   ui.Init(cfg);
///
/// (Standalone use without UIContext also works: Init(); Play(...);
/// Shutdown().) Supports WAV, MP3, and FLAC out of the box (decoded by
/// miniaudio). Without a wired backend the context uses a silent no-op (see
/// NullAudioBackend).
class AudioEngine final : public AudioBackend {
 public:
  AudioEngine();
  ~AudioEngine() override;

  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;

  /// Initialize the audio device and engine.
  bool Init() override;

  /// Shut down and release all resources.
  void Shutdown() override;

  /// Returns true if the engine was successfully initialized.
  bool IsInitialized() const override;

  // ----- Playback -----

  /// Play a sound file from disk. Returns a handle for control.
  /// Supports WAV, MP3, FLAC.
  SoundHandle Play(const char* path, f32 volume = 1.0f,
                   bool loop = false) override;

  /// Preload a sound into memory without playing it.
  SoundHandle Load(const char* path) override;

  /// Play a previously loaded sound. Returns a new playback handle.
  SoundHandle PlayLoaded(SoundHandle preloaded, f32 volume = 1.0f,
                         bool loop = false) override;

  /// Stop a sound. The handle becomes invalid after this.
  void Stop(SoundHandle handle) override;

  /// Check if a sound is currently playing.
  bool IsPlaying(SoundHandle handle) const override;

  // ----- Per-sound control -----

  void set_volume(SoundHandle handle, f32 volume) override;
  void set_pan(SoundHandle handle,
               f32 pan) override;  // -1.0 left, 0.0 center, 1.0 right
  void set_pitch(SoundHandle handle, f32 pitch) override;  // 1.0 = normal
  void Seek(SoundHandle handle, f32 seconds) override;

  // ----- Global control -----

  void set_master_volume(f32 volume) override;
  f32 master_volume() const override;

  void StopAll() override;
  void PauseAll() override;
  void ResumeAll() override;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace ugui

#endif  // UGUI_BACKENDS_UGUI_IMPL_MINIAUDIO_H_
