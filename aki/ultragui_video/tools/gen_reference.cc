// Prints the values aki/ultragui_video/tests/video_test.aki compares against,
// computed by the C++ VideoPlayer decoding examples/demo.mpg through a
// recording RHI (recording_rhi.cc). Build and run inside the repo's dev shell,
// from the repository root:
//
//   clang++ -std=c++20 -O2 -I. -Ithird_party/pl_mpeg -Ithird_party/miniaudio -DULTRAGUI_AUDIO=1 \
//     aki/ultragui_video/tools/gen_reference.cc aki/ultragui_video/tools/recording_rhi.cc \
//     ugui/video/video.cc ugui/audio/miniaudio_impl.cc -ldl -lpthread -lm \
//     -o /tmp/gen_video_reference && /tmp/gen_video_reference > /tmp/video_ref.txt
//   python3 -I aki/ultragui_video/tools/gen_tables.py /tmp/video_ref.txt \
//     > aki/ultragui_video/tests/support/video_reference.aki
//
// Each scenario runs with and without an AudioBackend (the C++ only tests the
// pointer for non-null); lines are tagged by scenario.
#include <ugui/audio/audio_backend.h>
#include <ugui/rhi/rhi.h>
#include <ugui/video/video.h>

#include <stdio.h>
#include <string.h>

#include <vector>

namespace ugui {
struct RecordingState;
extern RecordingState g_rec;
}  // namespace ugui

// The recorder's state, redeclared (same layout as recording_rhi.cc).
#include <map>
namespace ugui {
struct RecordingState {
  std::map<RHITextureHandle, std::vector<u8>> textures;
  std::map<RHITextureHandle, u32> widths, heights;
  std::map<RHITextureHandle, bool> is_target;
  RHITextureHandle next = 0;
  u32 creates = 0, updates = 0, destroys = 0, target_creates = 0, target_destroys = 0, converts = 0;
  RHITextureHandle last_convert[4] = {};
};
}  // namespace ugui

using namespace ugui;

static u64 FnvAdd(u64 h, const std::vector<u8>& bytes) {
  for (u8 b : bytes) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  return h;
}

static const u64 kFnvBasis = 0xcbf29ce484222325ull;

static u64 BitsOf(f64 v) {
  u64 u;
  memcpy(&u, &v, 8);
  return u;
}

static u32 BitsOf(f32 v) {
  u32 u;
  memcpy(&u, &v, 4);
  return u;
}

static u64 PlaneHash(VideoPlayer& v, RHITextureHandle h) {
  return h == kInvalidTexture ? 0 : FnvAdd(kFnvBasis, g_rec.textures[h]);
}

// All three planes in one hash: Y, then Cb, then Cr.
static u64 FrameHash(VideoPlayer& v) {
  u64 h = kFnvBasis;
  for (RHITextureHandle t : {v.y_texture(), v.cb_texture(), v.cr_texture()}) {
    if (t != kInvalidTexture) h = FnvAdd(h, g_rec.textures[t]);
  }
  return h;
}

// STATE tag scenario step position_bits finished playing needs_convert updates creates
//   converts frame_hash y_hash cb_hash cr_hash
static void State(const char* scenario, const char* audio, int step, VideoPlayer& v) {
  printf("STATE %s %s %d 0x%016llx %d %d %d %u %u %u 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n", scenario, audio, step,
         (unsigned long long)BitsOf(v.position()), v.IsFinished(), v.IsPlaying(), v.NeedsConvert(), g_rec.updates,
         g_rec.creates, g_rec.converts, (unsigned long long)FrameHash(v), (unsigned long long)PlaneHash(v, v.y_texture()),
         (unsigned long long)PlaneHash(v, v.cb_texture()), (unsigned long long)PlaneHash(v, v.cr_texture()));
}

// A backend that only has to be non-null.
struct Dummy final : AudioBackend {
  bool Init() override { return true; }
  void Shutdown() override {}
  bool IsInitialized() const override { return false; }
  SoundHandle Play(const char*, f32, bool) override { return 0; }
  SoundHandle Load(const char*) override { return 0; }
  SoundHandle PlayLoaded(SoundHandle, f32, bool) override { return 0; }
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

static void Run(const char* audio_name, AudioBackend* audio) {
  g_rec = RecordingState();
  RHI rhi;
  VideoPlayer v;
  if (!v.Load(&rhi, "examples/demo.mpg", audio)) {
    printf("LOAD_FAILED\n");
    return;
  }
  printf("LOAD %s %u %u 0x%016llx 0x%016llx %u %u %u %u %u %u\n", audio_name, v.width(), v.height(),
         (unsigned long long)BitsOf(v.duration()), (unsigned long long)BitsOf(v.frame_rate()), v.texture(),
         v.y_texture(), v.cb_texture(), v.cr_texture(), g_rec.widths[v.y_texture()], g_rec.heights[v.y_texture()],
         g_rec.widths[v.cb_texture()]);
  printf("LOAD2 %s %u 0x%08x 0x%08x %d\n", audio_name, g_rec.heights[v.cb_texture()], BitsOf(v.volume()),
         BitsOf(v.speed()), v.muted());
  State("load", audio_name, 0, v);

  // convert before anything: one call
  v.ConvertFrame();
  State("convert", audio_name, 0, v);
  printf("CONVERT %s %u %u %u %u\n", audio_name, g_rec.last_convert[0], g_rec.last_convert[1], g_rec.last_convert[2],
         g_rec.last_convert[3]);

  // Not playing: Update does nothing
  v.Update(0.5);
  State("idle", audio_name, 0, v);

  // Playing at 16 ms ticks, converting each frame
  v.Play();
  for (int i = 0; i < 120; ++i) {
    v.Update(0.016);
    if (v.NeedsConvert()) v.ConvertFrame();
    State("play16", audio_name, i, v);
  }
  // Pause, then big ticks
  v.Pause();
  v.Update(0.1);
  State("paused", audio_name, 0, v);
  v.set_speed(2.5f);
  v.Play();
  for (int i = 0; i < 20; ++i) {
    v.Update(0.2);
    v.ConvertFrame();
    State("fast", audio_name, i, v);
  }
  // Seek around
  v.set_speed(1.0f);
  const f64 seeks[] = {0.0, 1.0, 2.5, 0.2, -3.0, 1000.0, 3.1};
  int k = 0;
  for (f64 s : seeks) {
    v.Seek(s);
    State("seek", audio_name, k++, v);
    v.Update(0.04);
    v.ConvertFrame();
    State("seek_update", audio_name, k, v);
  }
  // Stop
  v.Stop();
  State("stop", audio_name, 0, v);
  // Loop through the end
  v.Seek(v.duration() - 0.5);
  v.set_loop(true);
  v.Play();
  for (int i = 0; i < 60; ++i) {
    v.Update(0.05);
    v.ConvertFrame();
    State("loop", audio_name, i, v);
  }
  // Run out without a loop, then restart with Play()
  v.set_loop(false);
  v.Seek(v.duration() - 0.5);
  for (int i = 0; i < 30 && !v.IsFinished(); ++i) {
    v.Update(0.05);
    v.ConvertFrame();
    State("once", audio_name, i, v);
  }
  State("finished", audio_name, 0, v);
  v.Update(0.05);
  State("finished_update", audio_name, 0, v);
  v.Play();
  State("restart", audio_name, 0, v);
  v.set_volume(0.5f);
  v.set_muted(true);
  printf("VOL %s 0x%08x %d\n", audio_name, BitsOf(v.volume()), v.muted());
  v.Unload();
  printf("UNLOAD %s %u %u %u %u %zu %d\n", audio_name, g_rec.destroys, g_rec.target_creates, g_rec.target_destroys,
         g_rec.converts, g_rec.textures.size(), v.IsLoaded());
}

int main() {
  Run("none", nullptr);
  Dummy dummy;
  Run("audio", &dummy);
  // a missing file
  RHI rhi;
  VideoPlayer v;
  printf("MISSING %d\n", v.Load(&rhi, "does_not_exist.mpg", nullptr));
  return 0;
}
