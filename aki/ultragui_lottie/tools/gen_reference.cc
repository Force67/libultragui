// Prints the values aki/ultragui_lottie/tests/lottie_test.aki compares against,
// computed by the C++ LottieAnimation itself: for each demo animation the
// frame chosen and an FNV-1a hash of the RGBA the texture backend received.
// Build and run inside the repo's dev shell, from the repository root, after
// building the C++ reference (it links the reference build's librlottie):
//
//   REF=/tmp/ugui-ref   # cmake -S . -B $REF -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C $REF rlottie
//   clang++ -std=c++20 -O2 -I. -Ithird_party/rlottie/inc aki/ultragui_lottie/tools/gen_reference.cc \
//     ugui/lottie/lottie.cc $REF/third_party/rlottie/librlottie.a -lpthread \
//     -o /tmp/gen_lottie_reference && /tmp/gen_lottie_reference
//
// The output is pasted into tests/lottie_test.aki as tables.
#include <ugui/lottie/lottie.h>

#include <stdio.h>
#include <string.h>

#include <map>
#include <vector>

using namespace ugui;

static u32 Bits(f32 v) {
  u32 u;
  memcpy(&u, &v, 4);
  return u;
}

static u64 Fnv(const std::vector<u8>& bytes) {
  u64 h = 0xcbf29ce484222325ull;
  for (u8 b : bytes) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  return h;
}

// A texture backend that keeps the last pixels it was given per texture.
struct Recorder final : TextureBackend {
  std::map<TextureId, std::vector<u8>> textures;
  std::map<TextureId, u32> widths, heights;
  u32 creates = 0, updates = 0, destroys = 0;
  TextureId next = 0x1000;

  TextureId CreateTexture(u32 w, u32 h, RHIFormat, const void* pixels, RHIFilter) override {
    TextureId id = next++;
    textures[id].assign(static_cast<const u8*>(pixels), static_cast<const u8*>(pixels) + w * h * 4);
    widths[id] = w;
    heights[id] = h;
    ++creates;
    return id;
  }
  void UpdateTexture(TextureId id, const void* pixels) override {
    auto& t = textures[id];
    memcpy(t.data(), pixels, t.size());
    ++updates;
  }
  void DestroyTexture(TextureId id) override {
    textures.erase(id);
    ++destroys;
  }
};

static void Report(const char* tag, const char* name, u32 w, u32 h, LottieAnimation& anim, Recorder& rec) {
  printf("%s %s %u %u %u %u 0x%08x 0x%016llx\n", tag, name, w, h, anim.total_frames(), anim.current_frame(),
         Bits(anim.progress()),
         (unsigned long long)Fnv(rec.textures[anim.texture()]));
}

int main() {
  const char* names[] = {"bounce", "pulse", "spinner", "wave"};
  struct Size { u32 w, h; };
  const Size sizes[] = {{64, 64}, {96, 48}};
  const f32 seeks[] = {0.0f, 0.1f, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 0.9f, 1.0f};

  printf("// load: name w h total_frames frame_rate_bits(hi lo) duration_bits(hi lo) first-frame hash\n");
  for (const char* name : names) {
    for (Size sz : sizes) {
      char path[256];
      snprintf(path, sizeof path, "examples/lottie_demo/anims/%s.json", name);
      Recorder rec;
      LottieAnimation anim;
      if (!anim.Load(&rec, path, sz.w, sz.h)) {
        printf("LOAD_FAILED %s\n", path);
        return 1;
      }
      f64 fr = anim.frame_rate(), du = anim.duration();
      u64 frb, dub;
      memcpy(&frb, &fr, 8);
      memcpy(&dub, &du, 8);
      printf("LOAD %s %u %u %u 0x%016llx 0x%016llx 0x%016llx\n", name, sz.w, sz.h, anim.total_frames(),
             (unsigned long long)frb, (unsigned long long)dub,
             (unsigned long long)Fnv(rec.textures[anim.texture()]));
      // name w h frames frame progress_bits hash
      for (f32 s : seeks) {
        anim.Seek(s);
        Report("SEEK", name, sz.w, sz.h, anim, rec);
      }
      // Playing: 16 ms ticks, 3x speed, looping through the end.
      anim.set_speed(3.0f);
      anim.Play();
      for (int i = 0; i < 90; ++i) {
        anim.Update(0.016);
        if (i % 6 == 5) Report("PLAY", name, sz.w, sz.h, anim, rec);
      }
      // Not looping: runs out.
      anim.Stop();
      anim.set_loop(false);
      anim.set_speed(1.0f);
      anim.Play();
      for (int i = 0; i < 200 && anim.IsPlaying(); ++i) {
        anim.Update(0.05);
        if (i % 7 == 0) Report("ONCE", name, sz.w, sz.h, anim, rec);
      }
      printf("END %s %u %u playing=%d ", name, sz.w, sz.h, anim.IsPlaying() ? 1 : 0);
      Report("ONCE_END", name, sz.w, sz.h, anim, rec);
      printf("COUNTS %s %u %u creates=%u updates=%u destroys=%u\n", name, sz.w, sz.h, rec.creates, rec.updates, rec.destroys);
      anim.Unload();
      printf("UNLOADED %s %u %u destroys=%u live=%zu\n", name, sz.w, sz.h, rec.destroys, rec.textures.size());
    }
  }
  return 0;
}
