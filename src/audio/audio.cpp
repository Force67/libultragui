#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <ultragui/audio/audio.h>

#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ugui {

// ============================================================================
// Sound slot - wraps a single ma_sound
// ============================================================================

struct SoundSlot {
    ma_sound sound{};
    SoundHandle handle = INVALID_SOUND;
    bool active = false;
    bool preloaded = false; // true if loaded but not yet played
    std::string path;       // for play_loaded cloning
};

static constexpr u32 MAX_SOUNDS = 128;

// ============================================================================
// Impl
// ============================================================================

struct AudioEngine::Impl {
    ma_engine engine{};
    bool engine_initialized = false;

    SoundSlot slots[MAX_SOUNDS]{};
    u32 next_handle = 1;
    f32 master_vol = 1.0f;

    SoundSlot* find_slot(SoundHandle h) {
        for (auto& s : slots)
            if (s.active && s.handle == h)
                return &s;
        return nullptr;
    }

    const SoundSlot* find_slot(SoundHandle h) const {
        for (auto& s : slots)
            if (s.active && s.handle == h)
                return &s;
        return nullptr;
    }

    SoundSlot* alloc_slot() {
        // First: find an inactive slot
        for (auto& s : slots)
            if (!s.active)
                return &s;

        // Second: reclaim a finished non-looping sound
        for (auto& s : slots) {
            if (s.active && !s.preloaded && !ma_sound_is_looping(&s.sound) &&
                !ma_sound_is_playing(&s.sound)) {
                ma_sound_uninit(&s.sound);
                s.active = false;
                return &s;
            }
        }

        return nullptr; // pool full
    }
};

// ============================================================================
// AudioEngine
// ============================================================================

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    if (impl_)
        shutdown();
}

bool AudioEngine::init() {
    if (impl_)
        return true; // already initialized

    impl_ = new Impl();

    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;
    config.sampleRate = 44100;

    if (ma_engine_init(&config, &impl_->engine) != MA_SUCCESS) {
        std::fprintf(stderr, "ultragui/audio: failed to initialize audio engine\n");
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    impl_->engine_initialized = true;
    return true;
}

void AudioEngine::shutdown() {
    if (!impl_)
        return;

    // Uninit all active sounds
    for (auto& s : impl_->slots) {
        if (s.active) {
            ma_sound_uninit(&s.sound);
            s.active = false;
        }
    }

    if (impl_->engine_initialized) {
        ma_engine_uninit(&impl_->engine);
        impl_->engine_initialized = false;
    }

    delete impl_;
    impl_ = nullptr;
}

bool AudioEngine::is_initialized() const {
    return impl_ && impl_->engine_initialized;
}

// ----- Playback -----

SoundHandle AudioEngine::play(const char* path, f32 volume, bool loop) {
    if (!impl_)
        return INVALID_SOUND;

    SoundSlot* slot = impl_->alloc_slot();
    if (!slot) {
        std::fprintf(stderr, "ultragui/audio: sound pool full\n");
        return INVALID_SOUND;
    }

    u32 flags = MA_SOUND_FLAG_DECODE; // decode to memory for low-latency playback
    if (ma_sound_init_from_file(&impl_->engine, path, flags, nullptr, nullptr, &slot->sound) !=
        MA_SUCCESS) {
        std::fprintf(stderr, "ultragui/audio: failed to load '%s'\n", path);
        return INVALID_SOUND;
    }

    slot->handle = impl_->next_handle++;
    slot->active = true;
    slot->preloaded = false;
    slot->path = path;

    ma_sound_set_volume(&slot->sound, volume);
    ma_sound_set_looping(&slot->sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(&slot->sound);

    return slot->handle;
}

SoundHandle AudioEngine::load(const char* path) {
    if (!impl_)
        return INVALID_SOUND;

    SoundSlot* slot = impl_->alloc_slot();
    if (!slot)
        return INVALID_SOUND;

    u32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (ma_sound_init_from_file(&impl_->engine, path, flags, nullptr, nullptr, &slot->sound) !=
        MA_SUCCESS) {
        std::fprintf(stderr, "ultragui/audio: failed to load '%s'\n", path);
        return INVALID_SOUND;
    }

    slot->handle = impl_->next_handle++;
    slot->active = true;
    slot->preloaded = true;
    slot->path = path;

    return slot->handle;
}

SoundHandle AudioEngine::play_loaded(SoundHandle preloaded, f32 volume, bool loop) {
    if (!impl_)
        return INVALID_SOUND;

    SoundSlot* src = impl_->find_slot(preloaded);
    if (!src)
        return INVALID_SOUND;

    SoundSlot* slot = impl_->alloc_slot();
    if (!slot)
        return INVALID_SOUND;

    // Create a copy from the same data source
    u32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (ma_sound_init_copy(&impl_->engine, &src->sound, flags, nullptr, &slot->sound) !=
        MA_SUCCESS) {
        return INVALID_SOUND;
    }

    slot->handle = impl_->next_handle++;
    slot->active = true;
    slot->preloaded = false;
    slot->path = src->path;

    ma_sound_set_volume(&slot->sound, volume);
    ma_sound_set_looping(&slot->sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(&slot->sound);

    return slot->handle;
}

void AudioEngine::stop(SoundHandle handle) {
    if (!impl_)
        return;
    SoundSlot* s = impl_->find_slot(handle);
    if (!s)
        return;
    ma_sound_stop(&s->sound);
    ma_sound_uninit(&s->sound);
    s->active = false;
}

bool AudioEngine::is_playing(SoundHandle handle) const {
    if (!impl_)
        return false;
    const SoundSlot* s = impl_->find_slot(handle);
    if (!s)
        return false;
    return ma_sound_is_playing(&s->sound) != 0;
}

// ----- Per-sound control -----

void AudioEngine::set_volume(SoundHandle handle, f32 volume) {
    if (!impl_)
        return;
    SoundSlot* s = impl_->find_slot(handle);
    if (s)
        ma_sound_set_volume(&s->sound, volume);
}

void AudioEngine::set_pan(SoundHandle handle, f32 pan) {
    if (!impl_)
        return;
    SoundSlot* s = impl_->find_slot(handle);
    if (s)
        ma_sound_set_pan(&s->sound, pan);
}

void AudioEngine::set_pitch(SoundHandle handle, f32 pitch) {
    if (!impl_)
        return;
    SoundSlot* s = impl_->find_slot(handle);
    if (s)
        ma_sound_set_pitch(&s->sound, pitch);
}

void AudioEngine::seek(SoundHandle handle, f32 seconds) {
    if (!impl_)
        return;
    SoundSlot* s = impl_->find_slot(handle);
    if (s) {
        u64 frame = static_cast<u64>(seconds * ma_engine_get_sample_rate(&impl_->engine));
        ma_sound_seek_to_pcm_frame(&s->sound, frame);
    }
}

// ----- Global control -----

void AudioEngine::set_master_volume(f32 volume) {
    if (!impl_)
        return;
    impl_->master_vol = volume;
    ma_engine_set_volume(&impl_->engine, volume);
}

f32 AudioEngine::master_volume() const {
    return impl_ ? impl_->master_vol : 1.0f;
}

void AudioEngine::stop_all() {
    if (!impl_)
        return;
    for (auto& s : impl_->slots) {
        if (s.active && !s.preloaded) {
            ma_sound_stop(&s.sound);
            ma_sound_uninit(&s.sound);
            s.active = false;
        }
    }
}

void AudioEngine::pause_all() {
    if (!impl_)
        return;
    for (auto& s : impl_->slots) {
        if (s.active && !s.preloaded && ma_sound_is_playing(&s.sound))
            ma_sound_stop(&s.sound);
    }
}

void AudioEngine::resume_all() {
    if (!impl_)
        return;
    for (auto& s : impl_->slots) {
        if (s.active && !s.preloaded)
            ma_sound_start(&s.sound);
    }
}

} // namespace ugui
