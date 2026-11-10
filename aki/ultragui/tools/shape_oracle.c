/* The shaper's oracle: shapes strings with the dev shell's FreeType and
 * HarfBuzz exactly as text/text_engine does (FT_Open_Face on the path, face
 * index 0, FT_Set_Pixel_Sizes(face, 0, px), hb_ft_font_create_referenced +
 * hb_ft_font_changed, hb_buffer_add_utf8 of the whole text with no context,
 * hb_buffer_guess_segment_properties, hb_shape with no features) and prints
 * what HarfBuzz returns. tests/shape_test.aki builds it with the dev shell's
 * clang and compares the aki shaper's output with it.
 *
 *   clang -O2 tools/shape_oracle.c $(pkg-config --cflags --libs freetype2 harfbuzz) -o shape_oracle
 *   SHAPE_LOCALE=tr_TR.UTF-8 shape_oracle FONT STRINGS SIZE...
 *   shape_oracle --languages CASES
 *
 * SHAPE_LOCALE sets LC_CTYPE before the first HarfBuzz language read. If
 * unavailable, its string is used as an explicit buffer language instead.
 * Without it, the oracle keeps the initial C locale.
 *
 * STRINGS has one string per line, hex encoded (any bytes, invalid UTF-8
 * included). Output, per size and string in that order:
 *   "S <size> <x_scale> <y_scale>" once per size, then per string
 *   "<n> <gid> <cluster> <x_advance> <y_advance> <x_offset> <y_offset> ..."
 * preceded by "L <locale-setlocale-succeeded> <canonical-language>",
 * then a line "F <num_glyphs> <shaper> <gsub> <gpos> <gdef>" (the shaper
 * HarfBuzz picked for the first string, e.g. "ot" or "graphite2", and whether
 * HarfBuzz kept GSUB and GPOS and has GDEF glyph classes, after its sanitizer).
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>
#include <locale.h>
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

/* Independent language/tag oracle. Each row: length, hb_script_t, hex bytes.
 * Prints canonical bytes and the ordered script/language tags. */
static int languages(const char *path)
{
  FILE *in = fopen(path, "rb");
  if (!in) return 1;
  char *line = NULL;
  size_t cap = 0;
  while (getline(&line, &cap, in) >= 0) {
    char *end;
    int length = strtol(line, &end, 10);
    unsigned script = strtoul(end, &end, 10);
    while (*end == ' ') end++;
    size_t n = strcspn(end, "\r\n");
    char *raw = calloc(n / 2 + 1, 1);
    for (size_t i = 0; i + 1 < n; i += 2)
      raw[i / 2] = (char) (hexval(end[i]) * 16 + hexval(end[i + 1]));
    hb_language_t lang = hb_language_from_string(raw, length);
    const char *canon = hb_language_to_string(lang);
    size_t len = canon ? strlen(canon) : 0;
    printf("%zu", len);
    for (size_t i = 0; i < len; i++) printf(" %u", (unsigned char) canon[i]);
    unsigned sn = 3, ln = 3;
    hb_tag_t st[3], lt[3];
    hb_ot_tags_from_script_and_language((hb_script_t) script, lang, &sn, st, &ln, lt);
    printf(" %u", sn);
    for (unsigned i = 0; i < sn; i++) printf(" %u", st[i]);
    printf(" %u", ln);
    for (unsigned i = 0; i < ln; i++) printf(" %u", lt[i]);
    printf("\n");
    free(raw);
  }
  free(line);
  fclose(in);
  return 0;
}

int main(int argc, char **argv)
{
  if (argc == 3 && !strcmp(argv[1], "--languages")) return languages(argv[2]);
  const char *requested = getenv("SHAPE_LOCALE");
  hb_language_t language = HB_LANGUAGE_INVALID;
  int locale_ok = requested && setlocale(LC_CTYPE, requested);
  if (requested && !locale_ok) language = hb_language_from_string(requested, -1);
  if (!language) language = hb_language_get_default();
  const char *canonical = hb_language_to_string(language);
  printf("L %d %s\n", locale_ok, canonical && *canonical ? canonical : "-");
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
      hb_buffer_set_language(buf, language);
      hb_buffer_guess_segment_properties(buf);
      hb_segment_properties_t props;
      hb_buffer_get_segment_properties(buf, &props);
      hb_shape_plan_t *plan = hb_shape_plan_create_cached(hb_font_get_face(font), &props, NULL, 0, NULL);
      hb_face_t *hface = hb_font_get_face(font);
      printf("F %ld %s %d %d %d\n", (long) face->num_glyphs, hb_shape_plan_get_shaper(plan),
             hb_ot_layout_has_substitution(hface), hb_ot_layout_has_positioning(hface),
             hb_ot_layout_has_glyph_classes(hface));
      hb_shape_plan_destroy(plan);
      hb_buffer_destroy(buf);
      printed_shaper = 1;
    }
    printf("S %d %d %d\n", px, xs, ys);
    for (int i = 0; i < count; i++) {
      hb_buffer_t *buf = hb_buffer_create();
      hb_buffer_add_utf8(buf, strings[i], lengths[i], 0, lengths[i]);
      hb_buffer_set_language(buf, language);
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
