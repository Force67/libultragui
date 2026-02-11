#pragma once

#include <ultragui/core/types.h>

namespace ugui {

using SoundHandle = u32;
static constexpr SoundHandle INVALID_SOUND = 0;

/// Lightweight audio engine backed by miniaudio.
/// Supports WAV, MP3, and FLAC out of the box (decoded by miniaudio).
///
/// Usage:
///   AudioEngine audio;
///   audio.init();
///   auto snd = audio.play("click.wav");
///   audio.set_volume(snd, 0.5f);
///   // ...
///   audio.shutdown();
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /// Initialize the audio device and engine.
    bool init();

    /// Shut down and release all resources.
    void shutdown();

    /// Returns true if the engine was successfully initialized.
    bool is_initialized() const;

    // ----- Playback -----

    /// Play a sound file from disk. Returns a handle for control.
    /// Supports WAV, MP3, FLAC.
    SoundHandle play(const char* path, f32 volume = 1.0f, bool loop = false);

    /// Preload a sound into memory without playing it.
    SoundHandle load(const char* path);

    /// Play a previously loaded sound. Returns a new playback handle.
    SoundHandle play_loaded(SoundHandle preloaded, f32 volume = 1.0f, bool loop = false);

    /// Stop a sound. The handle becomes invalid after this.
    void stop(SoundHandle handle);

    /// Check if a sound is currently playing.
    bool is_playing(SoundHandle handle) const;

    // ----- Per-sound control -----

    void set_volume(SoundHandle handle, f32 volume);
    void set_pan(SoundHandle handle, f32 pan);     // -1.0 left, 0.0 center, 1.0 right
    void set_pitch(SoundHandle handle, f32 pitch);  // 1.0 = normal
    void seek(SoundHandle handle, f32 seconds);

    // ----- Global control -----

    void set_master_volume(f32 volume);
    f32 master_volume() const;

    void stop_all();
    void pause_all();
    void resume_all();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace ugui
