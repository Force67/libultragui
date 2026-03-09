#include <ultragui/lottie/lottie.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                         \
  static void test_##name();               \
  static struct Register_##name {          \
    Register_##name() {                    \
      ++tests_run;                         \
      std::printf("  %-50s", #name "..."); \
      test_##name();                       \
      std::printf(" PASS\n");              \
      ++tests_passed;                      \
    }                                      \
  } reg_##name;                            \
  static void test_##name()

#define ASSERT(cond)                                                        \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::printf(" FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond, \
                  __FILE__, __LINE__);                                      \
      std::exit(1);                                                         \
    }                                                                       \
  } while (0)

// Minimal Lottie JSON: a red circle that moves across the frame
// 30fps, 60 frames (2 seconds), 100x100 viewport
static const char* LOTTIE_JSON = R"JSON({
  "v": "5.5.2",
  "fr": 30,
  "ip": 0,
  "op": 60,
  "w": 100,
  "h": 100,
  "layers": [{
    "ty": 4,
    "nm": "circle",
    "ip": 0,
    "op": 60,
    "st": 0,
    "ks": {
      "o": {"a": 0, "k": 100},
      "r": {"a": 0, "k": 0},
      "p": {"a": 0, "k": [50, 50]},
      "a": {"a": 0, "k": [0, 0]},
      "s": {"a": 0, "k": [100, 100]}
    },
    "shapes": [{
      "ty": "el",
      "p": {"a": 0, "k": [0, 0]},
      "s": {"a": 0, "k": [60, 60]},
      "nm": "ellipse"
    }, {
      "ty": "fl",
      "c": {"a": 0, "k": [1, 0, 0, 1]},
      "o": {"a": 0, "k": 100},
      "nm": "fill"
    }]
  }]
})JSON";

static const char* write_test_lottie() {
  static const char* path = "/tmp/ultragui_test.json";
  FILE* f = fopen(path, "w");
  if (!f) return nullptr;
  fputs(LOTTIE_JSON, f);
  fclose(f);
  return path;
}

// ============================================================================
// Tests (no RHI needed - we test the API without GPU)
// ============================================================================

TEST(load_from_data_no_rhi) {
  // LottieAnimation needs an RHI for texture creation, so load without RHI
  // should work for parsing but texture will be kInvalidTexture
  ugui::LottieAnimation anim;
  // Load() requires RHI, so passing nullptr should fail gracefully
  bool ok = anim.Load(nullptr, "/nonexistent.json", 64, 64);
  ASSERT(!ok);
  ASSERT(!anim.IsLoaded());
}

TEST(load_nonexistent_file) {
  ugui::LottieAnimation anim;
  bool ok = anim.Load(nullptr, "/tmp/does_not_exist_lottie.json", 64, 64);
  ASSERT(!ok);
}

TEST(default_state) {
  ugui::LottieAnimation anim;
  ASSERT(!anim.IsLoaded());
  ASSERT(!anim.IsPlaying());
  ASSERT(anim.texture() == ugui::kInvalidTexture);
  ASSERT(anim.width() == 0);
  ASSERT(anim.height() == 0);
  ASSERT(anim.duration() == 0.0);
  ASSERT(anim.progress() == 0.0f);
  ASSERT(anim.total_frames() == 0);
  ASSERT(anim.current_frame() == 0);
  ASSERT(anim.frame_rate() == 0.0);
}

TEST(playback_controls_without_load) {
  ugui::LottieAnimation anim;
  // These should all be no-ops, not crash
  anim.Play();
  anim.Pause();
  anim.Stop();
  anim.set_loop(true);
  anim.set_speed(2.0f);
  anim.Seek(0.5f);
  anim.Update(0.016);
  ASSERT(!anim.IsPlaying());
}

TEST(move_semantics) {
  ugui::LottieAnimation a;
  ugui::LottieAnimation b = std::move(a);
  ASSERT(!b.IsLoaded());

  ugui::LottieAnimation c;
  c = std::move(b);
  ASSERT(!c.IsLoaded());
}

TEST(unload_without_load) {
  ugui::LottieAnimation anim;
  anim.Unload();  // should not crash
  ASSERT(!anim.IsLoaded());
}

TEST(load_from_data) {
  // load_data with nullptr RHI - rlottie parses fine, no texture created
  ugui::LottieAnimation anim;
  bool ok = anim.LoadData(nullptr, LOTTIE_JSON, "test", 64, 64);
  ASSERT(ok);
  ASSERT(anim.IsLoaded());
  ASSERT(anim.texture() == ugui::kInvalidTexture);  // no RHI -> no texture
  ASSERT(anim.width() == 64);
  ASSERT(anim.height() == 64);
  ASSERT(anim.total_frames() == 61);  // rlottie: frames 0..op inclusive
  ASSERT(std::fabs(anim.frame_rate() - 30.0) < 0.01);
  ASSERT(std::fabs(anim.duration() - 2.0) < 0.01);
}

TEST(playback_state_machine) {
  ugui::LottieAnimation anim;
  anim.LoadData(nullptr, LOTTIE_JSON, "test2", 32, 32);
  ASSERT(!anim.IsPlaying());

  anim.Play();
  ASSERT(anim.IsPlaying());

  anim.Pause();
  ASSERT(!anim.IsPlaying());

  anim.Play();
  anim.Stop();
  ASSERT(!anim.IsPlaying());
  ASSERT(anim.progress() == 0.0f);
}

TEST(speed_and_loop) {
  ugui::LottieAnimation anim;
  anim.LoadData(nullptr, LOTTIE_JSON, "test3", 32, 32);

  ASSERT(anim.speed() == 1.0f);
  anim.set_speed(2.5f);
  ASSERT(anim.speed() == 2.5f);

  ASSERT(anim.IsLooping());
  anim.set_loop(false);
  ASSERT(!anim.IsLooping());
}

TEST(seek_progress) {
  ugui::LottieAnimation anim;
  anim.LoadData(nullptr, LOTTIE_JSON, "test4", 32, 32);

  anim.Seek(0.5f);
  ASSERT(std::fabs(anim.progress() - 0.5f) < 0.05f);

  anim.Seek(0.0f);
  ASSERT(anim.progress() < 0.01f);

  anim.Seek(1.0f);
  ASSERT(anim.progress() > 0.95f);
}

TEST(update_advances_time) {
  ugui::LottieAnimation anim;
  anim.LoadData(nullptr, LOTTIE_JSON, "test5", 32, 32);

  anim.Play();
  anim.Update(1.0);  // advance 1 second into a 2 second animation
  ASSERT(std::fabs(anim.progress() - 0.5f) < 0.05f);
  ASSERT(anim.current_frame() > 0);
}

TEST(loop_wraps_around) {
  ugui::LottieAnimation anim;
  anim.LoadData(nullptr, LOTTIE_JSON, "test6", 32, 32);

  anim.set_loop(true);
  anim.Play();
  anim.Update(3.0);  // 3 seconds into a 2 second animation
  // Should wrap: 3.0 - 2.0 = 1.0 second -> ~50% progress
  ASSERT(anim.IsPlaying());
  ASSERT(std::fabs(anim.progress() - 0.5f) < 0.05f);
}

TEST(no_loop_stops_at_end) {
  ugui::LottieAnimation anim;
  anim.LoadData(nullptr, LOTTIE_JSON, "test7", 32, 32);

  anim.set_loop(false);
  anim.Play();
  anim.Update(3.0);           // past the end
  ASSERT(!anim.IsPlaying());  // should auto-stop
  ASSERT(anim.progress() > 0.95f);
}

// ============================================================================
// Main
// ============================================================================

int main() {
  (void)write_test_lottie;  // may use later with real RHI tests
  std::printf("Lottie test suite\n");
  std::printf("=================\n");
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
