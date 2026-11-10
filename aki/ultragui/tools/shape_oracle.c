/* The shaper's oracle: shapes strings with the dev shell's FreeType and
 * HarfBuzz exactly as text/text_engine does (FT_Open_Face on the path, face
 * index 0, FT_Set_Pixel_Sizes(face, 0, px), hb_ft_font_create_referenced +
 * hb_ft_font_changed, hb_buffer_add_utf8 of the whole text with no context,
 * hb_buffer_guess_segment_properties, hb_shape with no features) and prints
 * what HarfBuzz returns. tests/shape_test.aki builds it with the dev shell's
 * clang and compares the aki shaper's output with it.
 *
 *   clang -O2 tools/shape_oracle.c $(pkg-config --cflags --libs freetype2 harfbuzz) -o shape_oracle
 *   shape_oracle FONT STRINGS SIZE...
 *
 * STRINGS has one string per line, hex encoded (any bytes, invalid UTF-8
 * included). Output, per size and string in that order:
 *   "S <size> <x_scale> <y_scale>" once per size, then per string
 *   "<n> <gid> <cluster> <x_advance> <y_advance> <x_offset> <y_offset> ..."
 * and first a line "F <num_glyphs> <shaper>" (the shaper HarfBuzz picked for
 * the first string, e.g. "ot" or "graphite2").
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hexval(int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int main(int argc, char **argv)
{
  if (argc < 4) {
    fprintf(stderr, "usage: %s FONT STRINGS SIZE...\n", argv[0]);
    return 2;
  }
  FT_Library lib;
  if (FT_Init_FreeType(&lib)) return 1;
  FT_Face face;
  if (FT_New_Face(lib, argv[1], 0, &face)) {
    fprintf(stderr, "cannot open %s\n", argv[1]);
    return 1;
  }
  FILE *in = fopen(argv[2], "rb");
  if (!in) { perror(argv[2]); return 1; }
  char **strings = NULL;
  int *lengths = NULL;
  int count = 0, cap = 0;
  char *line = NULL;
  size_t line_cap = 0;
  ssize_t got;
  while ((got = getline(&line, &line_cap, in)) >= 0) {
    while (got > 0 && (line[got - 1] == '\n' || line[got - 1] == '\r')) got--;
    if (count == cap) {
      cap = cap ? cap * 2 : 64;
      strings = realloc(strings, cap * sizeof *strings);
      lengths = realloc(lengths, cap * sizeof *lengths);
    }
    char *s = malloc(got / 2 + 1);
    int n = 0;
    for (ssize_t i = 0; i + 1 < got; i += 2) s[n++] = (char) (hexval(line[i]) * 16 + hexval(line[i + 1]));
    strings[count] = s;
    lengths[count] = n;
    count++;
  }
  fclose(in);

  hb_font_t *font = hb_ft_font_create_referenced(face);
  int printed_shaper = 0;
  for (int a = 3; a < argc; a++) {
    int px = atoi(argv[a]);
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt) px);
    hb_ft_font_changed(font);
    int xs, ys;
    hb_font_get_scale(font, &xs, &ys);
    if (!printed_shaper) {
      /* Which shaper runs: plan a buffer like the first string's. */
      hb_buffer_t *buf = hb_buffer_create();
      if (count) hb_buffer_add_utf8(buf, strings[0], lengths[0], 0, lengths[0]);
      hb_buffer_guess_segment_properties(buf);
      hb_segment_properties_t props;
      hb_buffer_get_segment_properties(buf, &props);
      hb_shape_plan_t *plan = hb_shape_plan_create_cached(hb_font_get_face(font), &props, NULL, 0, NULL);
      printf("F %ld %s\n", (long) face->num_glyphs, hb_shape_plan_get_shaper(plan));
      hb_shape_plan_destroy(plan);
      hb_buffer_destroy(buf);
      printed_shaper = 1;
    }
    printf("S %d %d %d\n", px, xs, ys);
    for (int i = 0; i < count; i++) {
      hb_buffer_t *buf = hb_buffer_create();
      hb_buffer_add_utf8(buf, strings[i], lengths[i], 0, lengths[i]);
      hb_buffer_guess_segment_properties(buf);
      hb_shape(font, buf, NULL, 0);
      unsigned n = 0;
      hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, &n);
      hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, &n);
      printf("%u", n);
      for (unsigned j = 0; j < n; j++)
        printf(" %u %u %d %d %d %d", info[j].codepoint, info[j].cluster, pos[j].x_advance, pos[j].y_advance,
               pos[j].x_offset, pos[j].y_offset);
      printf("\n");
      hb_buffer_destroy(buf);
    }
  }
  hb_font_destroy(font);
  FT_Done_FreeType(lib);
  return 0;
}
