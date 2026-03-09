#include <cstddef>
#include <cstdio>
#define PL_MPEG_IMPLEMENTATION
#include <ultragui/core/config.h>
#include <ultragui/rhi/rhi.h>
#include <ultragui/video/video.h>

#include <pl_mpeg.h>

#if ULTRAGUI_AUDIO
#include <miniaudio.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ugui {

#if ULTRAGUI_AUDIO
struct AudioBridge {
  ma_pcm_rb ring_buffer;
  ma_device device;
  bool device_initialized = false;
  f32 volume = 1.0f;
  bool muted = false;
  u32 sample_rate = 0;

  bool Init(u32 rate) {
    sample_rate = rate;
    constexpr u32 kChannels = 2;

    // Ring buffer: ~0.5s of stereo audio
    u32 rb_frames = rate / 2;
    if (ma_pcm_rb_init(ma_format_f32, kChannels, rb_frames, nullptr, nullptr,
                       &ring_buffer) != MA_SUCCESS) {
      return false;
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = kChannels;
    cfg.sampleRate = rate;
    cfg.dataCallback = DataCallback;
    cfg.pUserData = this;
    cfg.periodSizeInFrames = 512;

    if (ma_device_init(nullptr, &cfg, &device) != MA_SUCCESS) {
      ma_pcm_rb_uninit(&ring_buffer);
      return false;
    }

    device_initialized = true;
    return true;
  }

  void Start() {
    if (device_initialized) ma_device_start(&device);
  }

  void Stop() {
    if (device_initialized) ma_device_stop(&device);
  }

  void Flush() {
    if (device_initialized) ma_pcm_rb_reset(&ring_buffer);
  }

  void Shutdown() {
    if (device_initialized) {
      ma_device_uninit(&device);
      device_initialized = false;
    }
    ma_pcm_rb_uninit(&ring_buffer);
  }

  void PushSamples(const float* interleaved, u32 frame_count) {
    void* write_buf;
    ma_uint32 frames = frame_count;
    if (ma_pcm_rb_acquire_write(&ring_buffer, &frames, &write_buf) !=
        MA_SUCCESS)
      return;
    if (frames > 0) {
      std::memcpy(write_buf, interleaved,
                  frames * 2 * sizeof(float));  // 2 channels
      ma_pcm_rb_commit_write(&ring_buffer, frames);
    }
  }

  static void DataCallback(ma_device* dev, void* output, const void*,
                           ma_uint32 frame_count) {
    auto* self = static_cast<AudioBridge*>(dev->pUserData);
    auto* dst = static_cast<float*>(output);

    void* read_buf;
    ma_uint32 frames = frame_count;
    if (ma_pcm_rb_acquire_read(&self->ring_buffer, &frames, &read_buf) ==
            MA_SUCCESS &&
        frames > 0) {
      f32 vol = self->muted ? 0.0f : self->volume;
      auto* src = static_cast<float*>(read_buf);
      for (ma_uint32 i = 0; i < frames * 2; ++i) dst[i] = src[i] * vol;
      ma_pcm_rb_commit_read(&self->ring_buffer, frames);

      // Zero remaining if we didn't get enough
      if (frames < frame_count) {
        std::memset(dst + frames * 2, 0,
                    (frame_count - frames) * 2 * sizeof(float));
      }
    } else {
      std::memset(dst, 0, frame_count * 2 * sizeof(float));
    }
  }
};
#endif  // ULTRAGUI_AUDIO

// ============================================================================
// VideoPlayer::Impl
// ============================================================================

struct VideoPlayer::Impl {
  plm_t* plm = nullptr;
  RHI* rhi = nullptr;

  // RGBA render target (output of GPU YCbCr -> RGBA conversion)
  RHITextureHandle rgba_target = kInvalidTexture;

  // YCbCr plane textures (R8)
  RHITextureHandle y_tex = kInvalidTexture;
  RHITextureHandle cb_tex = kInvalidTexture;
  RHITextureHandle cr_tex = kInvalidTexture;

  u32 width = 0;
  u32 height = 0;
  // pl_mpeg plane sizes (rounded up to macroblock boundary)
  u32 plane_w = 0;
  u32 plane_h = 0;
  u32 chroma_w = 0;
  u32 chroma_h = 0;
  f64 duration = 0.0;
  f64 frame_rate = 0.0;

  // Playback state
  bool playing = false;
  bool looping = false;
  bool finished = false;
  bool needs_convert = false;
  f32 speed = 1.0f;

#if ULTRAGUI_AUDIO
  AudioBridge* audio_bridge = nullptr;
#endif

  static void OnVideoFrame(plm_t*, plm_frame_t* frame, void* user) {
    auto* self = static_cast<Impl*>(user);

    // Upload Y plane (full resolution)
    if (self->y_tex == kInvalidTexture) {
      self->y_tex = self->rhi->CreateTexture(
          self->plane_w, self->plane_h, RHIFormat::kR8Unorm, frame->y.data);
    } else {
      self->rhi->UpdateTexture(self->y_tex, frame->y.data);
    }

    // Upload Cb plane (half resolution in each dimension for 4:2:0)
    if (self->cb_tex == kInvalidTexture) {
      self->cb_tex = self->rhi->CreateTexture(
          self->chroma_w, self->chroma_h, RHIFormat::kR8Unorm, frame->cb.data);
    } else {
      self->rhi->UpdateTexture(self->cb_tex, frame->cb.data);
    }

    // Upload Cr plane
    if (self->cr_tex == kInvalidTexture) {
      self->cr_tex = self->rhi->CreateTexture(
          self->chroma_w, self->chroma_h, RHIFormat::kR8Unorm, frame->cr.data);
    } else {
      self->rhi->UpdateTexture(self->cr_tex, frame->cr.data);
    }

    self->needs_convert = true;
  }

  static void OnAudioSamples(plm_t*, plm_samples_t* samples, void* user) {
#if ULTRAGUI_AUDIO
    auto* self = static_cast<Impl*>(user);
    if (self->audio_bridge)
      self->audio_bridge->PushSamples(samples->interleaved, samples->count);
#else
    (void)samples;
    (void)user;
#endif
  }
};

// ============================================================================
// VideoPlayer
// ============================================================================

VideoPlayer::VideoPlayer() = default;

VideoPlayer::~VideoPlayer() {
  if (impl_) Unload();
}

VideoPlayer::VideoPlayer(VideoPlayer&& other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

VideoPlayer& VideoPlayer::operator=(VideoPlayer&& other) noexcept {
  if (this != &other) {
    if (impl_) Unload();
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

bool VideoPlayer::Load(RHI* rhi, const char* path, AudioEngine* audio) {
  if (impl_) Unload();

  plm_t* plm = plm_create_with_filename(path);
  if (!plm) {
    std::fprintf(stderr, "ultragui/video: failed to load '%s'\n", path);
    return false;
  }

  if (plm_get_num_video_streams(plm) == 0) {
    std::fprintf(stderr, "ultragui/video: no video stream in '%s'\n", path);
    plm_destroy(plm);
    return false;
  }

  impl_ = new Impl();
  impl_->plm = plm;
  impl_->rhi = rhi;
  impl_->width = static_cast<u32>(plm_get_width(plm));
  impl_->height = static_cast<u32>(plm_get_height(plm));
  impl_->duration = plm_get_duration(plm);
  impl_->frame_rate = plm_get_framerate(plm);

  // pl_mpeg plane sizes are rounded up to macroblock (16px) boundaries
  impl_->plane_w = (impl_->width + 15) & ~15u;
  impl_->plane_h = (impl_->height + 15) & ~15u;
  impl_->chroma_w = impl_->plane_w / 2;
  impl_->chroma_h = impl_->plane_h / 2;

  // Create RGBA render target for GPU YCbCr conversion output
  impl_->rgba_target = rhi->CreateRenderTarget(impl_->width, impl_->height);

  // Configure pl_mpeg: no internal looping, we handle it ourselves
  plm_set_loop(plm, FALSE);

  // Set up video callback
  plm_set_video_decode_callback(plm, Impl::OnVideoFrame, impl_);

  // Set up audio
  bool has_audio = plm_get_num_audio_streams(plm) > 0;
#if ULTRAGUI_AUDIO
  if (has_audio && audio) {
    u32 sample_rate = static_cast<u32>(plm_get_samplerate(plm));
    impl_->audio_bridge = new AudioBridge();
    if (impl_->audio_bridge->Init(sample_rate)) {
      plm_set_audio_decode_callback(plm, Impl::OnAudioSamples, impl_);
      plm_set_audio_lead_time(plm, 1024.0 / sample_rate);
    } else {
      std::fprintf(stderr,
                   "ultragui/video: audio bridge init failed (non-fatal)\n");
      delete impl_->audio_bridge;
      impl_->audio_bridge = nullptr;
      plm_set_audio_enabled(plm, FALSE);
    }
  } else {
    plm_set_audio_enabled(plm, FALSE);
  }
#else
  (void)audio;
  if (has_audio) {
    std::fprintf(
        stderr,
        "ultragui/video: '%s' has audio but ULTRAGUI_AUDIO is disabled\n",
        path);
  }
  plm_set_audio_enabled(plm, FALSE);
#endif

  // Decode first frame so texture is available immediately
  plm_frame_t* frame = plm_decode_video(plm);
  if (frame) Impl::OnVideoFrame(plm, frame, impl_);

  // Rewind after decoding first frame
  plm_rewind(plm);

  return true;
}

void VideoPlayer::Update(f64 dt) {
  if (!impl_ || !impl_->playing || impl_->finished) return;

  f64 advance = dt * impl_->speed;
  plm_decode(impl_->plm, advance);

  if (plm_has_ended(impl_->plm)) {
    if (impl_->looping) {
      plm_rewind(impl_->plm);
#if ULTRAGUI_AUDIO
      if (impl_->audio_bridge) impl_->audio_bridge->Flush();
#endif
    } else {
      impl_->playing = false;
      impl_->finished = true;
#if ULTRAGUI_AUDIO
      if (impl_->audio_bridge) impl_->audio_bridge->Stop();
#endif
    }
  }
}

bool VideoPlayer::IsLoaded() const { return impl_ && impl_->plm; }

RHITextureHandle VideoPlayer::texture() const {
  return impl_ ? impl_->rgba_target : kInvalidTexture;
}

bool VideoPlayer::NeedsConvert() const { return impl_ && impl_->needs_convert; }

void VideoPlayer::ConvertFrame() {
  if (!impl_ || !impl_->needs_convert) return;
  if (impl_->rgba_target == kInvalidTexture ||
      impl_->y_tex == kInvalidTexture || impl_->cb_tex == kInvalidTexture ||
      impl_->cr_tex == kInvalidTexture)
    return;
  impl_->rhi->ConvertVideoFrame(impl_->rgba_target, impl_->y_tex, impl_->cb_tex,
                                impl_->cr_tex);
  impl_->needs_convert = false;
}

RHITextureHandle VideoPlayer::y_texture() const {
  return impl_ ? impl_->y_tex : kInvalidTexture;
}

RHITextureHandle VideoPlayer::cb_texture() const {
  return impl_ ? impl_->cb_tex : kInvalidTexture;
}

RHITextureHandle VideoPlayer::cr_texture() const {
  return impl_ ? impl_->cr_tex : kInvalidTexture;
}

u32 VideoPlayer::width() const { return impl_ ? impl_->width : 0; }

u32 VideoPlayer::height() const { return impl_ ? impl_->height : 0; }

f64 VideoPlayer::frame_rate() const { return impl_ ? impl_->frame_rate : 0.0; }

void VideoPlayer::Play() {
  if (!impl_) return;
  if (impl_->finished) {
    // Restart from beginning
    plm_rewind(impl_->plm);
    impl_->finished = false;
#if ULTRAGUI_AUDIO
    if (impl_->audio_bridge) impl_->audio_bridge->Flush();
#endif
  }
  impl_->playing = true;
#if ULTRAGUI_AUDIO
  if (impl_->audio_bridge) impl_->audio_bridge->Start();
#endif
}

void VideoPlayer::Pause() {
  if (!impl_) return;
  impl_->playing = false;
#if ULTRAGUI_AUDIO
  if (impl_->audio_bridge) impl_->audio_bridge->Stop();
#endif
}

void VideoPlayer::Stop() {
  if (!impl_) return;
  impl_->playing = false;
  impl_->finished = false;
  plm_rewind(impl_->plm);
#if ULTRAGUI_AUDIO
  if (impl_->audio_bridge) {
    impl_->audio_bridge->Stop();
    impl_->audio_bridge->Flush();
  }
#endif

  // Decode and display first frame
  plm_frame_t* frame = plm_decode_video(impl_->plm);
  if (frame) Impl::OnVideoFrame(impl_->plm, frame, impl_);
  plm_rewind(impl_->plm);
}

void VideoPlayer::Seek(f64 seconds) {
  if (!impl_) return;
  seconds = std::clamp(seconds, 0.0, impl_->duration);
  plm_seek(impl_->plm, seconds, TRUE);
  impl_->finished = false;
#if ULTRAGUI_AUDIO
  if (impl_->audio_bridge) impl_->audio_bridge->Flush();
#endif
}

void VideoPlayer::set_loop(bool loop) {
  if (impl_) impl_->looping = loop;
}

void VideoPlayer::set_speed(f32 spd) {
  if (impl_) impl_->speed = spd;
}

bool VideoPlayer::IsPlaying() const { return impl_ && impl_->playing; }

bool VideoPlayer::IsFinished() const { return impl_ && impl_->finished; }

bool VideoPlayer::IsLooping() const { return impl_ && impl_->looping; }

f32 VideoPlayer::speed() const { return impl_ ? impl_->speed : 1.0f; }

f64 VideoPlayer::duration() const { return impl_ ? impl_->duration : 0.0; }

f64 VideoPlayer::position() const {
  return impl_ ? plm_get_time(impl_->plm) : 0.0;
}

void VideoPlayer::set_volume(f32 vol) {
#if ULTRAGUI_AUDIO
  if (impl_ && impl_->audio_bridge) impl_->audio_bridge->volume = vol;
#else
  (void)vol;
#endif
}

void VideoPlayer::set_muted(bool m) {
#if ULTRAGUI_AUDIO
  if (impl_ && impl_->audio_bridge) impl_->audio_bridge->muted = m;
#else
  (void)m;
#endif
}

f32 VideoPlayer::volume() const {
#if ULTRAGUI_AUDIO
  if (impl_ && impl_->audio_bridge) return impl_->audio_bridge->volume;
#endif
  return 1.0f;
}

bool VideoPlayer::muted() const {
#if ULTRAGUI_AUDIO
  if (impl_ && impl_->audio_bridge) return impl_->audio_bridge->muted;
#endif
  return false;
}

void VideoPlayer::Unload() {
  if (!impl_) return;

#if ULTRAGUI_AUDIO
  if (impl_->audio_bridge) {
    impl_->audio_bridge->Shutdown();
    delete impl_->audio_bridge;
  }
#endif

  if (impl_->rhi) {
    if (impl_->y_tex != kInvalidTexture)
      impl_->rhi->DestroyTexture(impl_->y_tex);
    if (impl_->cb_tex != kInvalidTexture)
      impl_->rhi->DestroyTexture(impl_->cb_tex);
    if (impl_->cr_tex != kInvalidTexture)
      impl_->rhi->DestroyTexture(impl_->cr_tex);
    if (impl_->rgba_target != kInvalidTexture)
      impl_->rhi->DestroyRenderTarget(impl_->rgba_target);
  }

  if (impl_->plm) plm_destroy(impl_->plm);

  delete impl_;
  impl_ = nullptr;
}

}  // namespace ugui
