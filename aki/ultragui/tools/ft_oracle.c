/* The font engine's oracle: what FreeType (and HarfBuzz's hb_ft on top of
 * it) give for one face, dumped as binary records that
 * tests/font_tt_test.aki replays against ultragui::font and compares field
 * by field.
 *
 *   ft_oracle <font file> <face index> <out file> <px>...
 *
 * For the face: one 'F' record. Then for each pixel size, in order, on the
 * same FT_Face (FreeType keeps hinting state across sizes and glyphs, so the
 * order matters and the test repeats it):
 *   'S'  FT_Set_Pixel_Sizes(face, 0, px) and face->size->metrics;
 *   'G'  for every glyph, FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT): the
 *        outline (points, tags, contours, flags) or bitmap, the slot
 *        metrics, advance.x and the linear advances;
 *   'H'  for every glyph, what hb_ft reports through HarfBuzz's font API:
 *        the horizontal advance, the glyph extents and contour point 0
 *        (hb_ft loads with FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING), plus
 *        FT_Get_Advance(face, gid, FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING).
 *
 * Every record is a little-endian u32 length and that many bytes; integers
 * are little-endian, 64-bit values for FreeType's FT_Pos/FT_Fixed.
 *
 * Built by the test with the dev shell's clang:
 *   clang -O2 ft_oracle.c $(pkg-config --cflags --libs freetype2 harfbuzz) -o ft_oracle
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include <hb.h>
#include <hb-ft.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *buf;
static size_t len, cap;

static void put(const void *p, size_t n) {
  if (len + n > cap) {
    cap = (len + n) * 2 + 4096;
    buf = realloc(buf, cap);
  }
  memcpy(buf + len, p, n);
  len += n;
}
static void u8(uint8_t v) { put(&v, 1); }
static void u16(uint16_t v) { put(&v, 2); }
static void u32(uint32_t v) { put(&v, 4); }
static void i32(int32_t v) { put(&v, 4); }
static void i64(int64_t v) { put(&v, 8); }

static void flush(FILE *out) {
  uint32_t n = (uint32_t)len;
  fwrite(&n, 4, 1, out);
  fwrite(buf, 1, len, out);
  len = 0;
}

static void metrics(const FT_Glyph_Metrics *m) {
  i64(m->width);
  i64(m->height);
  i64(m->horiBearingX);
  i64(m->horiBearingY);
  i64(m->horiAdvance);
  i64(m->vertBearingX);
  i64(m->vertBearingY);
  i64(m->vertAdvance);
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: ft_oracle font index out px...\n");
    return 2;
  }
  FT_Library lib;
  if (FT_Init_FreeType(&lib)) return 1;
  FILE *out = fopen(argv[3], "wb");
  if (!out) return 1;
  FT_Face face = NULL;
  FT_Error err = FT_Open_Face(lib, &(FT_Open_Args){.flags = FT_OPEN_PATHNAME, .pathname = argv[1]},
                              atol(argv[2]), &face);
  u8('F');
  i32(err);
  if (err) {
    flush(out);
    fclose(out);
    return 0;
  }
  u32((uint32_t)face->num_glyphs);
  u32((uint32_t)face->face_flags);
  u32(face->units_per_EM);
  i32(face->ascender);
  i32(face->descender);
  i32(face->height);
  i32(face->max_advance_width);
  i32(face->max_advance_height);
  u32((uint32_t)face->num_faces);
  u32((uint32_t)face->num_fixed_sizes);
  flush(out);

  /* As the C++ text engine creates it: hb_ft over the same face. */
  hb_font_t *hb = hb_ft_font_create_referenced(face);

  for (int a = 4; a < argc; a++) {
    unsigned px = (unsigned)atoi(argv[a]);
    err = FT_Set_Pixel_Sizes(face, 0, px);
    hb_ft_font_changed(hb);
    FT_Size_Metrics *sm = &face->size->metrics;
    u8('S');
    u32(px);
    i32(err);
    i64(sm->x_ppem);
    i64(sm->y_ppem);
    i64(sm->x_scale);
    i64(sm->y_scale);
    i64(sm->ascender);
    i64(sm->descender);
    i64(sm->height);
    i64(sm->max_advance);
    flush(out);

    for (FT_Long g = 0; g < face->num_glyphs; g++) {
      err = FT_Load_Glyph(face, (FT_UInt)g, FT_LOAD_DEFAULT);
      FT_GlyphSlot s = face->glyph;
      u8('G');
      u32((uint32_t)g);
      i32(err);
      if (!err) {
        u8(s->format == FT_GLYPH_FORMAT_BITMAP ? 1 : s->format == FT_GLYPH_FORMAT_OUTLINE ? 0 : 2);
        metrics(&s->metrics);
        i64(s->advance.x);
        i64(s->linearHoriAdvance);
        i64(s->linearVertAdvance);
        if (s->format == FT_GLYPH_FORMAT_OUTLINE) {
          FT_Outline *o = &s->outline;
          u32(o->n_points);
          u32(o->n_contours);
          u32((uint32_t)o->flags);
          for (int i = 0; i < o->n_points; i++) {
            i64(o->points[i].x);
            i64(o->points[i].y);
          }
          put(o->tags, o->n_points);
          for (int i = 0; i < o->n_contours; i++) u16(o->contours[i]);
        } else if (s->format == FT_GLYPH_FORMAT_BITMAP) {
          FT_Bitmap *b = &s->bitmap;
          u32(b->width);
          u32(b->rows);
          i32(b->pitch);
          u8(b->pixel_mode);
          i32(s->bitmap_left);
          i32(s->bitmap_top);
          int pitch = b->pitch < 0 ? -b->pitch : b->pitch;
          if (b->buffer) put(b->buffer, (size_t)pitch * b->rows);
        }
      }
      flush(out);
    }

    for (FT_Long g = 0; g < face->num_glyphs; g++) {
      FT_Fixed adv = 0;
      FT_Error aerr = FT_Get_Advance(face, (FT_UInt)g, FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING, &adv);
      hb_position_t hadv = hb_font_get_glyph_h_advance(hb, (hb_codepoint_t)g);
      hb_glyph_extents_t ext = {0, 0, 0, 0};
      hb_bool_t has_ext = hb_font_get_glyph_extents(hb, (hb_codepoint_t)g, &ext);
      hb_position_t cx = 0, cy = 0;
      hb_bool_t has_pt = hb_font_get_glyph_contour_point(hb, (hb_codepoint_t)g, 0, &cx, &cy);
      u8('H');
      u32((uint32_t)g);
      i32(aerr);
      i64(adv);
      i32(hadv);
      u8(has_ext ? 1 : 0);
      i32(ext.x_bearing);
      i32(ext.y_bearing);
      i32(ext.width);
      i32(ext.height);
      u8(has_pt ? 1 : 0);
      i32(cx);
      i32(cy);
      flush(out);
    }
  }
  hb_font_destroy(hb);
  fclose(out);
  return 0;
}
