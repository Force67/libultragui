// Rasterizes the corpus in tests/data/svg/ with the C++ library and prints a
// hash of the RGBA8 pixels of each render, which tests/svg_exact_test.aki
// must reproduce byte for byte. Build and run from the repository root in the
// dev shell:
//
//   clang++ -std=c++20 -O2 -I. aki/ultragui/tools/gen_svg_reference.cc \
//     ugui/svg/svg.cc ugui/svg/svg_parser.cc ugui/svg/svg_rasterizer.cc \
//     -o /tmp/gen_svg_reference && /tmp/gen_svg_reference aki/ultragui/tests/data/svg
//
// Output: `SVG <file> <w> <h> <ok> 0x<fnv1a64>` per render (w and h are the
// requested size, 0 for the SVG's own). `python3 aki/ultragui/tools/
// gen_svg_table.py` turns the output into tests/support/svg_corpus.aki.
// With UGUI_DUMP=<dir> the pixels of each render are also written there as
// <file>.<w>x<h>.rgba, to diff a failing case against the aki run.
#include <ugui/svg/svg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

using namespace ugui;

struct Render {
  const char* file;
  u32 w, h;
};

static const Render kRenders[] = {
    {"rect_basic", 0, 0},        {"rect_basic", 128, 96},
    {"circles", 0, 0},           {"circles", 50, 33},
    {"rounded", 0, 0},           {"rounded", 160, 160},
    {"paths_lines", 0, 0},       {"paths_lines", 37, 91},
    {"cubics", 0, 0},            {"cubics", 240, 180},
    {"quads", 0, 0},             {"quads", 300, 240},
    {"arcs", 0, 0},              {"arcs", 77, 61},
    {"arcs", 256, 256},          {"joins", 0, 0},
    {"joins", 64, 48},           {"joins", 256, 192},
    {"grad_linear", 0, 0},       {"grad_linear", 256, 256},
    {"grad_linear", 90, 130},    {"grad_radial", 0, 0},
    {"grad_radial", 200, 150},   {"transforms", 0, 0},
    {"transforms", 100, 140},    {"opacity", 0, 0},
    {"opacity", 160, 120},       {"fill_rules", 0, 0},
    {"fill_rules", 256, 128},    {"viewbox", 0, 0},
    {"viewbox", 64, 32},         {"viewbox", 200, 77},
    {"viewbox_wh", 0, 0},        {"viewbox_wh", 120, 40},
    {"noviewbox_default", 0, 0}, {"noviewbox_default", 150, 75},
    {"syntax_zoo", 0, 0},        {"syntax_zoo", 180, 120},
    {"ties", 0, 0},              {"ties", 96, 96},
    {"ties", 31, 29},            {"gear_icon", 24, 24},
    {"gear_icon", 64, 64},       {"gear_icon", 33, 47},
    {"gear_icon", 128, 128},     {"malformed", 0, 0},
    {"scene_1", 0, 0},           {"scene_1", 211, 97},
    {"scene_2", 0, 0},           {"scene_2", 211, 97},
    {"scene_3", 0, 0},           {"scene_3", 320, 240},
    {"scene_4", 0, 0},           {"scene_4", 64, 64},
    {"scene_5", 0, 0},           {"scene_5", 320, 240},
    {"scene_6", 0, 0},           {"scene_6", 99, 150},
    {"scene_7", 0, 0},           {"scene_7", 320, 240},
    {"scene_8", 0, 0},           {"scene_8", 123, 45},
};

static std::vector<char> ReadFile(const std::string& path) {
  std::vector<char> data;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return data;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + n);
  fclose(f);
  return data;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <corpus dir>\n", argv[0]);
    return 2;
  }
  std::string dir = argv[1];
  const char* dump = getenv("UGUI_DUMP");
  for (const Render& r : kRenders) {
    std::vector<char> data = ReadFile(dir + "/" + r.file + ".svg");
    SvgImage img;
    bool ok = LoadSvgMemory(data.data(), data.size(), img, r.w, r.h);
    u64 h = 0xcbf29ce484222325ull;
    if (ok) {
      for (u8 b : img.pixels) {
        h ^= b;
        h *= 0x100000001b3ull;
      }
    }
    printf("SVG %s %u %u %d 0x%016llx\n", r.file, r.w, r.h, ok ? 1 : 0, (unsigned long long)h);
    if (ok && dump) {
      std::string out = std::string(dump) + "/" + r.file + "." + std::to_string(r.w) + "x" +
                        std::to_string(r.h) + ".rgba";
      FILE* f = fopen(out.c_str(), "wb");
      if (f) {
        fwrite(img.pixels.data(), 1, img.pixels.size(), f);
        fclose(f);
      }
    }
  }
  return 0;
}
