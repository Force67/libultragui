// Differential oracle only: the engine never calls FreeType.
// Built by tests/raster_test.aki using the dev shell's FreeType 2.14.3.
// raster_oracle list [all]          -> newline-delimited font paths
// raster_oracle FONT PX HINT STRIDE -> binary outline + rendered bitmap records
// raster_oracle synthetic          -> deterministic edge cases, including errors
// Format: little-endian u32 words (signed quantities use two's complement),
// header magic/version; records: marker,gid,flags,npoints,ncontours,points
// (x,y,tag),contour ends,error,width,rows,pitch,left,top,nbytes,raw bytes.
// End marker 0, then counts of loaded / skipped glyphs. No pixel hashes.
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

static void word(uint32_t x) {
  unsigned char bytes[4] = {static_cast<unsigned char>(x), static_cast<unsigned char>(x >> 8),
                            static_cast<unsigned char>(x >> 16), static_cast<unsigned char>(x >> 24)};
  if (fwrite(bytes, 1, 4, stdout) != 4) std::exit(2);
}
static void header(FT_Library lib) {
  int a, b, c;
  FT_Library_Version(lib, &a, &b, &c);
  word(0x52545331); word(a); word(b); word(c);
}
static void record(FT_GlyphSlot slot, unsigned gid) {
  const auto& o = slot->outline;
  word(1); word(gid); word(o.flags); word(o.n_points); word(o.n_contours);
  for (int i = 0; i < o.n_points; ++i) {
    word(o.points[i].x); word(o.points[i].y); word(static_cast<unsigned char>(o.tags[i]));
  }
  for (int i = 0; i < o.n_contours; ++i) word(o.contours[i]);
  const auto error = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
  word(error);
  const auto& b = slot->bitmap;
  word(b.width); word(b.rows); word(b.pitch); word(slot->bitmap_left); word(slot->bitmap_top);
  auto size = !error && b.buffer ? static_cast<unsigned>(b.rows * std::abs(b.pitch)) : 0;
  word(size);
  if (size && fwrite(b.buffer, 1, size, stdout) != size) std::exit(2);
}

static const char* dejavu = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
static void synthetic(FT_Library lib) {
  FT_Face face;
  if (FT_New_Face(lib, dejavu, 0, &face)) std::exit(3);
  auto slot = face->glyph;
  // Lift the face-specific 10*ppem bound above the renderer's own limits.
  face->size->metrics.x_ppem = face->size->metrics.y_ppem = 65535;
  unsigned count = 0;
  auto emit = [&](std::vector<FT_Vector> p, std::vector<unsigned char> tags, std::vector<unsigned short> ends, int flags) {
    slot->format = FT_GLYPH_FORMAT_OUTLINE;
    slot->outline = {static_cast<unsigned short>(ends.size()), static_cast<unsigned short>(p.size()),
                      p.data(), tags.data(), ends.data(), flags};
    record(slot, count++);
    slot->outline = {}; // vectors belong to this invocation
  };
  auto polygon = [&](std::vector<FT_Vector> p, int flags = 0) {
    std::vector<unsigned char> tags(p.size(), FT_CURVE_TAG_ON);
    std::vector<unsigned short> ends{static_cast<unsigned short>(p.size() - 1)};
    emit(p, tags, ends, flags);
  };
  emit({}, {}, {}, 0);
  emit({{0,0}}, {1}, {0}, 0);
  emit({{1,1}}, {1}, {0}, 0);
  emit({{-17,-31}}, {1}, {0}, 0);
  emit({{-17,-31},{127,97}}, {1,1}, {}, 0); // no contours, blank bitmap
  polygon({{0,0},{640,0},{640,640},{0,640}});
  polygon({{-65,-63},{1024,193},{-1,800},{567,-300}}); // bow tie
  polygon({{1,1},{79,107},{161,215},{81,108}}); // narrow sliver
  polygon({{-100,64},{100,64},{300,64}}); // zero height
  polygon({{64,-100},{64,100},{64,300}}); // zero width
  polygon({{9,9},{100,100},{200,200}}); // zero area, nonzero control box
  for (int flags : {0, FT_OUTLINE_EVEN_ODD_FILL, FT_OUTLINE_REVERSE_FILL,
                   FT_OUTLINE_HIGH_PRECISION, FT_OUTLINE_OVERLAP,
                   FT_OUTLINE_OVERLAP | FT_OUTLINE_EVEN_ODD_FILL}) {
    emit({{-129,-71},{601,-71},{601,659},{-129,659},
          {17,19},{747,19},{747,749},{17,749}}, std::vector<unsigned char>(8,1), {3,7}, flags);
    emit({{-129,-71},{601,-71},{601,659},{-129,659},
          {17,19},{17,749},{747,749},{747,19}}, std::vector<unsigned char>(8,1), {3,7}, flags);
    emit({{-65,-97},{-31,799},{899,-9}}, {0,0,0}, {2}, flags); // implicit start
    emit({{-63,-63},{128,411},{377,-21},{601,899},{899,1}}, {1,0,0,0,1}, {4}, flags);
    emit({{-63,-63},{128,411},{377,-21},{601,899}}, {1,2,2,1}, {3}, flags);
    emit({{-63,-63},{128,411},{377,-21}}, {1,2,2}, {2}, flags); // cubic closure
    emit({{-63,-63},{128,411},{377,-21}}, {0,0,1}, {2}, flags); // conic start at end
  }
  // Random closed polygons, curves and flags; deterministic and independent
  // of the font corpus, with many grid-boundary and near-degenerate cases.
  uint32_t seed = 0x7932b563;
  auto rand = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; };
  for (unsigned k = 0; k < 4000; ++k) {
    const int flags = (k & 1 ? FT_OUTLINE_OVERLAP : 0) |
                      (k & 2 ? FT_OUTLINE_EVEN_ODD_FILL : 0) |
                      (k & 4 ? FT_OUTLINE_REVERSE_FILL : 0) |
                      (k & 8 ? FT_OUTLINE_HIGH_PRECISION : 0);
    std::vector<FT_Vector> p;
    std::vector<unsigned char> tags;
    const unsigned n = 3 + rand() % 8;
    for (unsigned i = 0; i < n; ++i) {
      p.push_back({static_cast<int>(rand() % 2049) - 1024, static_cast<int>(rand() % 2049) - 1024});
      tags.push_back(k % 3 == 0 ? 0 : 1);
      if (k % 3 == 2 && i + 2 < n && i != 0) {
        tags.back() = 2;
        p.push_back({static_cast<int>(rand() % 2049) - 1024, static_cast<int>(rand() % 2049) - 1024});
        tags.push_back(2);
      }
    }
    emit(p, tags, {static_cast<unsigned short>(p.size() - 1)}, flags);
  }
  // Large absolute positions with small bitmaps and coordinate rejection.
  polygon({{1073741700,0},{1073741823,127},{1073741600,193}});
  polygon({{-1073741824,-9},{-1073741700,127},{-1073741600,193}});
  polygon({{2147483500,0},{2147483600,127},{2147483647,193}});
  polygon({{0,0},{4194304,64},{0,128}}); // >= 65536 pixels wide
  polygon({{0,0},{64,4194304},{128,0}});
  polygon({{0,0},{524288,64},{0,128}}, FT_OUTLINE_OVERLAP); // span limit
  // Wide / tall successful bitmaps exercise scratch bands and max dimensions.
  polygon({{0,0},{4194239,47},{0,129}});
  polygon({{0,0},{47,4194239},{129,0}});
  // Safe malformed outlines: no out-of-bounds reads in FreeType's checks.
  emit({{0,0},{64,64},{127,1}}, {2,2,1}, {2}, 0);
  emit({{0,0},{64,64},{127,1}}, {1,0,2}, {2}, 0);
  emit({{0,0},{64,64},{127,1}}, {1,2,1}, {2}, 0);
  emit({{0,0},{64,64},{127,1}}, {1,1,1}, {1}, 0);
  word(0); word(count); word(0);
  FT_Done_Face(face);
}

int main(int argc, char** argv) {
  FT_Library lib;
  if (FT_Init_FreeType(&lib)) return 3;
  if (argc >= 2 && std::string(argv[1]) == "list") {
    std::vector<std::string> fonts;
    if (argc >= 3 && std::string(argv[2]) == "all") {
      for (const auto& entry : std::filesystem::recursive_directory_iterator("/usr/share/fonts")) {
        if (!entry.is_regular_file()) continue;
        FT_Face face;
        if (!FT_New_Face(lib, entry.path().c_str(), 0, &face)) {
          if (FT_IS_SCALABLE(face)) fonts.push_back(entry.path());
          FT_Done_Face(face);
        }
      }
    } else {
      fonts = {dejavu, "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
               "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
               "/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf",
               "/usr/share/fonts/opentype/urw-base35/NimbusRoman-Regular.otf"};
      // This machine has NotoSansMono but no NotoSans-Regular.
      if (!std::filesystem::exists(fonts[2])) fonts[2] = "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf";
    }
    std::sort(fonts.begin(), fonts.end());
    for (const auto& p : fonts) std::puts(p.c_str());
  } else if (argc == 2 && std::string(argv[1]) == "synthetic") {
    header(lib); synthetic(lib);
  } else if (argc == 5) {
    FT_Face face;
    if (FT_New_Face(lib, argv[1], 0, &face) || FT_Set_Pixel_Sizes(face, 0, std::atoi(argv[2]))) return 3;
    header(lib);
    unsigned loaded = 0, skipped = 0;
    unsigned stride = std::max(1, std::atoi(argv[4]));
    for (unsigned gid = 0; gid < static_cast<unsigned>(face->num_glyphs); ++gid) {
      if (stride > 1 && gid >= 128 && gid % stride != 0) continue;
      if (FT_Load_Glyph(face, gid, std::atoi(argv[3]) ? FT_LOAD_DEFAULT : FT_LOAD_NO_HINTING) ||
          face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) { ++skipped; continue; }
      record(face->glyph, gid); ++loaded;
    }
    word(0); word(loaded); word(skipped);
    FT_Done_Face(face);
  } else {
    std::fprintf(stderr, "usage: raster_oracle list [all] | synthetic | FONT PX HINT STRIDE\n");
    return 2;
  }
  FT_Done_FreeType(lib);
  return std::ferror(stdout) ? 2 : 0;
}
