#ifndef ULTRAGUI_AUDIO_AUDIO_H_
#define ULTRAGUI_AUDIO_AUDIO_H_

#include <ultragui/core/types.h>

namespace ugui {

using SoundHandle = u32;
static constexpr SoundHandle kInvalidSound = 0;

/// Lightweight audio engine backed by miniaudio.
/// Supports WAV, MP3, and FLAC out of the box (decoded by miniaudio).
///
/// Usage:
///   AudioEngine audio;
///   audio.Init();
///   auto snd = audio.Play("click.wav");
///   audio.set_volume(snd, 0.5f);
///   // ...
///   audio.Shutdown();
class AudioEngine {
 public:
  AudioEngine();
  ~AudioEngine();

  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;

  /// Initialize the audio device and engine.
  bool Init();

  /// Shut down and release all resources.
  void Shutdown();

  /// Returns true if the engine was successfully initialized.
  bool IsInitialized() const;

  // ----- Playback -----

  /// Play a sound file from disk. Returns a handle for control.
  /// Supports WAV, MP3, FLAC.
  SoundHandle Play(const char* path, f32 volume = 1.0f, bool loop = false);

  /// Preload a sound into memory without playing it.
  SoundHandle Load(const char* path);

  /// Play a previously loaded sound. Returns a new playback handle.
  SoundHandle PlayLoaded(SoundHandle preloaded, f32 volume = 1.0f,
                         bool loop = false);

  /// Stop a sound. The handle becomes invalid after this.
  void Stop(SoundHandle handle);

  /// Check if a sound is currently playing.
  bool IsPlaying(SoundHandle handle) const;

  // ----- Per-sound control -----

  void set_volume(SoundHandle handle, f32 volume);
  void set_pan(SoundHandle handle,
               f32 pan);  // -1.0 left, 0.0 center, 1.0 right
  void set_pitch(SoundHandle handle, f32 pitch);  // 1.0 = normal
  void Seek(SoundHandle handle, f32 seconds);

  // ----- Global control -----

  void set_master_volume(f32 volume);
  f32 master_volume() const;

  void StopAll();
  void PauseAll();
  void ResumeAll();

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace ugui

#endif  // ULTRAGUI_AUDIO_AUDIO_H_
