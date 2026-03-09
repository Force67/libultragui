#ifndef ULTRAGUI_ANIM_VECTOR_ANIMATION_H_
#define ULTRAGUI_ANIM_VECTOR_ANIMATION_H_

#include <ultragui/core/types.h>
#include <ultragui/rhi/rhi_types.h>

namespace ugui {

class RHI;

/// Lightweight vector animation player. Parses .uganim JSON files and
/// renders frames via the built-in SVG rasterizer: zero external dependencies.
/// Same API as LottieAnimation for drop-in replacement.
class VectorAnimation {
 public:
  VectorAnimation() = default;
  ~VectorAnimation();

  VectorAnimation(const VectorAnimation&) = delete;
  VectorAnimation& operator=(const VectorAnimation&) = delete;

  /// Load from a .uganim file.
  bool Load(RHI* rhi, const char* path, u32 width, u32 height);

  /// Load from JSON data in memory.
  bool LoadData(RHI* rhi, const char* json_data, usize length, u32 width,
                u32 height);

  /// Advance playback by dt seconds. Re-rasterizes if the frame changed.
  void Update(f64 dt);

  /// Unload and free resources.
  void Unload();

  bool IsLoaded() const;
  RHITextureHandle texture() const;
  u32 width() const;
  u32 height() const;

  void Play();
  void Pause();
  void Stop();
  void set_loop(bool loop);
  void set_speed(f32 speed);
  void Seek(f32 progress);

  bool IsPlaying() const;
  f32 speed() const;
  f64 duration() const;
  f32 progress() const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace ugui

#endif  // ULTRAGUI_ANIM_VECTOR_ANIMATION_H_
