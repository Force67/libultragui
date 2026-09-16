// Loading a font from memory: the face has to come up from bytes alone, and it
// has to keep working after the caller's buffer is gone. A font that lives in
// an archive or inside the binary has no path to hand FreeType, which is the
// whole reason LoadFontMemory exists.

#include <ugui/text/text_engine.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int tests_run = 0;
static int tests_passed = 0;

#define RUN(name)                        \
  do {                                   \
    ++tests_run;                         \
    std::printf("  %-50s", #name "..."); \
    test_##name();                       \
    std::printf(" PASS\n");              \
    ++tests_passed;                      \
  } while (0)

#define ASSERT(cond)                                                        \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::printf(" FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond, \
                  __FILE__, __LINE__);                                      \
      std::exit(1);                                                         \
    }                                                                       \
  } while (0)

// A face to test with. The repository ships no font of its own, so the test
// borrows one from the machine and skips when it finds none (main() below).
static const char* FindSystemFont() {
  static const char* kCandidates[] = {
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
      "/System/Library/Fonts/Supplemental/Arial.ttf",
  };
  for (const char* candidate : kCandidates) {
    if (std::FILE* f = std::fopen(candidate, "rb")) {
      std::fclose(f);
      return candidate;
    }
  }
  return nullptr;
}

static std::vector<char> ReadFile(const char* path) {
  std::vector<char> bytes;
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return bytes;
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (size > 0) {
    bytes.resize(static_cast<size_t>(size));
    if (std::fread(bytes.data(), 1, bytes.size(), f) != bytes.size())
      bytes.clear();
  }
  std::fclose(f);
  return bytes;
}

// No RHI: nothing here reaches the atlas texture, which is the only part that
// needs one.
static void test_load_from_memory() {
  std::vector<char> bytes = ReadFile(FindSystemFont());
  ASSERT(!bytes.empty());

  ugui::TextEngine engine;
  ASSERT(engine.Init(nullptr));
  const ugui::FontHandle font =
      engine.LoadFontMemory(bytes.data(), bytes.size());
  ASSERT(font != ugui::kInvalidFont);
  engine.Shutdown();
}

static void test_survives_the_callers_buffer() {
  std::vector<char> bytes = ReadFile(FindSystemFont());
  ASSERT(!bytes.empty());

  ugui::TextEngine engine;
  ASSERT(engine.Init(nullptr));
  const ugui::FontHandle font =
      engine.LoadFontMemory(bytes.data(), bytes.size());
  ASSERT(font != ugui::kInvalidFont);

  // The point of the copy: FreeType reads through its pointer for the life of
  // the face, so a borrowed buffer would have the engine shaping from freed
  // memory here.
  std::memset(bytes.data(), 0, bytes.size());
  std::vector<char>().swap(bytes);

  const ugui::TextRun run = engine.Shape(font, "ultragui", 8, 16.0f);
  ASSERT(run.glyph_count == 8);
  engine.Shutdown();
}

static void test_rejects_garbage() {
  ugui::TextEngine engine;
  ASSERT(engine.Init(nullptr));

  const char junk[64] = {};
  ASSERT(engine.LoadFontMemory(junk, sizeof(junk)) == ugui::kInvalidFont);
  ASSERT(engine.LoadFontMemory(nullptr, 0) == ugui::kInvalidFont);
  ASSERT(engine.LoadFontMemory("", 0) == ugui::kInvalidFont);

  // A rejected load must leave the slot free, or a bad font would cost one of
  // the MAX_FONTS forever.
  std::vector<char> bytes = ReadFile(FindSystemFont());
  ASSERT(!bytes.empty());
  ASSERT(engine.LoadFontMemory(bytes.data(), bytes.size()) !=
         ugui::kInvalidFont);
  engine.Shutdown();
}

int main() {
  if (!FindSystemFont()) {
    std::printf("test_font_memory: no system font found, skipping\n");
    return 0;
  }
  std::printf("\nRunning font-from-memory tests...\n\n");
  RUN(load_from_memory);
  RUN(survives_the_callers_buffer);
  RUN(rejects_garbage);
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
