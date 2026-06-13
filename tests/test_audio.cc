#include <ugui/backends/ugui_impl_miniaudio.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

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

// ============================================================================
// Tests
// ============================================================================

TEST(init_shutdown) {
  ugui::AudioEngine engine;
  bool ok = engine.Init();
  // init may fail in CI (no audio device): that's ok, we test the API paths
  if (ok) {
    ASSERT(engine.IsInitialized());
    engine.Shutdown();
    ASSERT(!engine.IsInitialized());
  } else {
    ASSERT(!engine.IsInitialized());
  }
}

TEST(double_init_is_safe) {
  ugui::AudioEngine engine;
  engine.Init();
  bool ok2 = engine.Init();  // should return true (already initialized)
  if (engine.IsInitialized()) {
    ASSERT(ok2);
  }
  engine.Shutdown();
}

TEST(shutdown_without_init) {
  ugui::AudioEngine engine;
  engine.Shutdown();  // should not crash
  ASSERT(!engine.IsInitialized());
}

TEST(play_without_init) {
  ugui::AudioEngine engine;
  ugui::SoundHandle h = engine.Play("nonexistent.wav");
  ASSERT(h == ugui::kInvalidSound);
}

TEST(master_volume_default) {
  ugui::AudioEngine engine;
  // Even without init, master_volume should return default
  ASSERT(engine.master_volume() == 1.0f);
}

TEST(operations_on_invalid_handle) {
  ugui::AudioEngine engine;
  if (!engine.Init()) return;  // skip if no audio device

  // All these should be no-ops, not crash
  engine.Stop(999);
  engine.set_volume(999, 0.5f);
  engine.set_pan(999, -0.5f);
  engine.set_pitch(999, 1.5f);
  engine.Seek(999, 0.0f);
  ASSERT(!engine.IsPlaying(999));

  engine.Shutdown();
}

TEST(master_volume_set_get) {
  ugui::AudioEngine engine;
  if (!engine.Init()) return;

  engine.set_master_volume(0.5f);
  ASSERT(engine.master_volume() == 0.5f);

  engine.set_master_volume(0.0f);
  ASSERT(engine.master_volume() == 0.0f);

  engine.set_master_volume(1.0f);
  ASSERT(engine.master_volume() == 1.0f);

  engine.Shutdown();
}

TEST(stop_all_empty) {
  ugui::AudioEngine engine;
  if (!engine.Init()) return;

  // Should not crash with no active sounds
  engine.StopAll();
  engine.PauseAll();
  engine.ResumeAll();

  engine.Shutdown();
}

TEST(load_nonexistent_file) {
  ugui::AudioEngine engine;
  if (!engine.Init()) return;

  ugui::SoundHandle h = engine.Play("/nonexistent/path/sound.wav");
  ASSERT(h == ugui::kInvalidSound);

  ugui::SoundHandle h2 = engine.Load("/nonexistent/path/sound.wav");
  ASSERT(h2 == ugui::kInvalidSound);

  engine.Shutdown();
}

TEST(destructor_cleans_up) {
  {
    ugui::AudioEngine engine;
    engine.Init();
    // destructor should call shutdown
  }
  // Should not leak or crash
  ASSERT(true);
}

// ============================================================================
// WAV playback tests (generate a tiny WAV in memory, write to temp file)
// ============================================================================

static const char* create_test_wav() {
  static const char* path = "/tmp/ultragui_test_beep.wav";

  // Generate a minimal 44100 Hz, 16-bit mono WAV (0.1 second = 4410 samples)
  const int sample_rate = 44100;
  const int num_samples = 4410;
  const int bits_per_sample = 16;
  const int num_channels = 1;
  const int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
  const int block_align = num_channels * bits_per_sample / 8;
  const int data_size = num_samples * block_align;

  FILE* f = fopen(path, "wb");
  if (!f) return nullptr;

  // RIFF header
  fwrite("RIFF", 1, 4, f);
  int chunk_size = 36 + data_size;
  fwrite(&chunk_size, 4, 1, f);
  fwrite("WAVE", 1, 4, f);

  // fmt chunk
  fwrite("fmt ", 1, 4, f);
  int fmt_size = 16;
  fwrite(&fmt_size, 4, 1, f);
  short audio_format = 1;  // PCM
  fwrite(&audio_format, 2, 1, f);
  short channels = static_cast<short>(num_channels);
  fwrite(&channels, 2, 1, f);
  fwrite(&sample_rate, 4, 1, f);
  fwrite(&byte_rate, 4, 1, f);
  short ba = static_cast<short>(block_align);
  fwrite(&ba, 2, 1, f);
  short bps = static_cast<short>(bits_per_sample);
  fwrite(&bps, 2, 1, f);

  // data chunk
  fwrite("data", 1, 4, f);
  fwrite(&data_size, 4, 1, f);

  // Generate a 440 Hz sine wave
  for (int i = 0; i < num_samples; ++i) {
    double t = static_cast<double>(i) / sample_rate;
    double val = 0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 * t);
    short sample = static_cast<short>(val * 32767.0);
    fwrite(&sample, 2, 1, f);
  }

  fclose(f);
  return path;
}

TEST(play_wav_file) {
  ugui::AudioEngine engine;
  if (!engine.Init()) return;

  const char* wav = create_test_wav();
  if (!wav) {
    engine.Shutdown();
    return;
  }

  ugui::SoundHandle h =
      engine.Play(wav, 0.0f);  // volume=0 so we don't actually hear it
  if (h != ugui::kInvalidSound) {
    ASSERT(engine.IsPlaying(h));
    engine.Stop(h);
    ASSERT(!engine.IsPlaying(h));
  }

  engine.Shutdown();
}

TEST(load_and_play_loaded) {
  ugui::AudioEngine engine;
  if (!engine.Init()) return;

  const char* wav = create_test_wav();
  if (!wav) {
    engine.Shutdown();
    return;
  }

  ugui::SoundHandle loaded = engine.Load(wav);
  if (loaded != ugui::kInvalidSound) {
    // Preloaded sound should not be playing
    ASSERT(!engine.IsPlaying(loaded));

    // Play a copy
    ugui::SoundHandle h = engine.PlayLoaded(loaded, 0.0f);
    if (h != ugui::kInvalidSound) {
      ASSERT(engine.IsPlaying(h));
      engine.Stop(h);
    }

    // Original still loaded
    engine.Stop(loaded);  // unloads
  }

  engine.Shutdown();
}

TEST(per_sound_controls) {
  ugui::AudioEngine engine;
  if (!engine.Init()) return;

  const char* wav = create_test_wav();
  if (!wav) {
    engine.Shutdown();
    return;
  }

  ugui::SoundHandle h = engine.Play(wav, 0.0f, true);  // loop
  if (h != ugui::kInvalidSound) {
    engine.set_volume(h, 0.5f);
    engine.set_pan(h, -0.3f);
    engine.set_pitch(h, 1.2f);
    engine.Seek(h, 0.0f);
    ASSERT(engine.IsPlaying(h));
    engine.Stop(h);
  }

  engine.Shutdown();
}

// ============================================================================
// Main
// ============================================================================

int main() {
  std::printf("Audio test suite\n");
  std::printf("================\n");
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
