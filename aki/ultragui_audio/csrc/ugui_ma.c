// The C side of ultragui_audio's miniaudio binding: what an aki extern
// declaration cannot say portably. miniaudio's structs (ma_engine, ma_sound,
// ma_device, ma_pcm_rb) are big, differ per platform and are only ever used
// behind pointers, so aki allocates them by the size reported here; the
// config structs miniaudio takes by value are filled in here too.
#include <miniaudio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// The aki side hardcodes these (src/sys/miniaudio.aki).
_Static_assert(MA_SOUND_FLAG_DECODE == 0x00000002, "MA_SOUND_FLAG_DECODE");
_Static_assert(MA_SOUND_FLAG_NO_SPATIALIZATION == 0x00004000, "MA_SOUND_FLAG_NO_SPATIALIZATION");
_Static_assert(MA_SUCCESS == 0, "MA_SUCCESS");

size_t ugui_ma_engine_size(void) { return sizeof(ma_engine); }
size_t ugui_ma_sound_size(void) { return sizeof(ma_sound); }
size_t ugui_ma_pcm_rb_size(void) { return sizeof(ma_pcm_rb); }

// ma_engine_config_init() with channels and sampleRate set, then ma_engine_init.
int32_t ugui_ma_engine_init(ma_engine* engine, uint32_t channels, uint32_t sample_rate) {
  ma_engine_config config = ma_engine_config_init();
  config.channels = channels;
  config.sampleRate = sample_rate;
  return (int32_t)ma_engine_init(&config, engine);
}

// A stereo f32 ring buffer, as the video bridge's ma_pcm_rb_init call.
int32_t ugui_ma_pcm_rb_init_f32(ma_pcm_rb* rb, uint32_t channels, uint32_t frames) {
  return (int32_t)ma_pcm_rb_init(ma_format_f32, channels, frames, NULL, NULL, rb);
}

// A playback device and the callback it pulls f32 frames through.
typedef void (*ugui_ma_pull_fn)(void* user, float* out, uint32_t frames);

typedef struct ugui_ma_playback {
  ma_device device;
  ugui_ma_pull_fn pull;
  void* user;
} ugui_ma_playback;

static void ugui_ma_data_callback(ma_device* dev, void* output, const void* input, ma_uint32 frame_count) {
  ugui_ma_playback* self = (ugui_ma_playback*)dev->pUserData;
  (void)input;
  self->pull(self->user, (float*)output, frame_count);
}

// ma_device_init for a playback device (default device, f32, `channels`,
// `sample_rate`, `period_frames`). NULL on failure.
ugui_ma_playback* ugui_ma_playback_create(uint32_t sample_rate, uint32_t channels, uint32_t period_frames,
                                          ugui_ma_pull_fn pull, void* user) {
  ugui_ma_playback* self = (ugui_ma_playback*)calloc(1, sizeof(ugui_ma_playback));
  if (!self) return NULL;
  self->pull = pull;
  self->user = user;
  ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.format = ma_format_f32;
  cfg.playback.channels = channels;
  cfg.sampleRate = sample_rate;
  cfg.dataCallback = ugui_ma_data_callback;
  cfg.pUserData = self;
  cfg.periodSizeInFrames = period_frames;
  if (ma_device_init(NULL, &cfg, &self->device) != MA_SUCCESS) {
    free(self);
    return NULL;
  }
  return self;
}

void ugui_ma_playback_start(ugui_ma_playback* self) { ma_device_start(&self->device); }
void ugui_ma_playback_stop(ugui_ma_playback* self) { ma_device_stop(&self->device); }

void ugui_ma_playback_destroy(ugui_ma_playback* self) {
  ma_device_uninit(&self->device);
  free(self);
}
