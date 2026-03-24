#ifndef ULTRAGUI_AUDIO_AUDIO_H_
#define ULTRAGUI_AUDIO_AUDIO_H_

#include <ugui/audio/audio_backend.h>

namespace ugui {

/// Lightweight audio backend backed by miniaudio. This is the default
/// AudioBackend used when none is injected (see CreateDefaultAudioBackend).
/// Supports WAV, MP3, and FLAC out of the box (decoded by miniaudio).
///
/// Usage:
///   AudioEngine audio;
///   audio.Init();
///   auto snd = audio.Play("click.wav");
///   audio.set_volume(snd, 0.5f);
///   // ...
///   audio.Shutdown();
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

#endif  // ULTRAGUI_AUDIO_AUDIO_H_
