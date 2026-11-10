// Dumps the Unicode and shaper data HarfBuzz 13.2.1 shapes with, for
// tools/gen_shape_tables.py to pack into src/shape/data/ and the generated
// src/shape/*_tables.aki modules. It compiles HarfBuzz's own amalgamated
// source, so every value comes from the very tables the C++ library uses
// (hb-ucd-table.hh, the Arabic joining and fallback tables, the Indic and USE
// category tables, hb-ot-tag.cc), not from a separate copy of the UCD.
//
// Build and run (the source tree: `nix build --inputs-from <repo>
// nixpkgs#harfbuzz.src --print-out-paths`, then untar it):
//
//   clang++ -std=c++17 -O1 -I<harfbuzz-13.2.1>/src tools/gen_shape_tables.cc -o /tmp/gen_shape_tables
//   /tmp/gen_shape_tables /tmp/shape_dump
//   python3 tools/gen_shape_tables.py /tmp/shape_dump
//
// Output files in the given directory:
//   ucd.bin       one 24-byte record per code point 0..0x10FFFF, little endian:
//                 u8 general category, u8 combining class, u8 joining type,
//                 u8 USE category, u16 Indic categories (category | position << 8),
//                 u8 flags (1 default ignorable, 2 Extended_Pictographic),
//                 u8 0, u32 mirroring, u32 script tag, u32 decomposition a,
//                 u32 decomposition b (both 0 when there is none)
//   compose.txt   "a b ab" for every pair the canonical composition joins
//   scripts.txt   per script: "tag direction n script-tags... m language-tags... shaper"
//                 (OpenType tags for language "c", the C locale's; the shaper
//                 hb_ot_shaper_categorize picks for a GSUB script of the script's own)
//   mcc.txt       the 256 modified combining classes
//   arabic.txt    the fallback shaping table and its ligature tables
//   pua.txt       "u simp trad" for the symbol-font Arabic PUA maps (nonzero only)
//   misc.txt      hb_language_get_default() and a few constants

#include "harfbuzz.cc"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

static void put_u8(FILE *f, unsigned v) { fputc((int) (v & 0xFF), f); }
static void put_u16(FILE *f, unsigned v) { put_u8(f, v); put_u8(f, v >> 8); }
static void put_u32(FILE *f, unsigned v) { put_u16(f, v); put_u16(f, v >> 16); }

static FILE *open_out(const std::string &dir, const char *name)
{
  std::string path = dir + "/" + name;
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) { perror(path.c_str()); exit(1); }
  return f;
}

int main(int argc, char **argv)
{
  if (argc != 2) { fprintf(stderr, "usage: %s OUT_DIR\n", argv[0]); return 2; }
  std::string dir = argv[1];
  hb_unicode_funcs_t *uf = hb_unicode_funcs_get_default();

  std::set<unsigned> scripts;
  FILE *ucd = open_out(dir, "ucd.bin");
  FILE *comp = open_out(dir, "compose.txt");
  for (unsigned u = 0; u <= 0x10FFFFu; u++)
  {
    unsigned gc = (unsigned) uf->general_category(u);
    unsigned ccc = (unsigned) uf->combining_class(u);
    unsigned jt = joining_type(u);
    unsigned use = hb_use_get_category(u);
    unsigned indic = hb_indic_get_categories(u);
    unsigned flags = (hb_unicode_funcs_t::is_default_ignorable(u) ? 1 : 0) |
		     (_hb_unicode_is_emoji_Extended_Pictographic(u) ? 2 : 0);
    unsigned mirror = uf->mirroring(u);
    unsigned script = (unsigned) uf->script(u);
    scripts.insert(script);
    hb_codepoint_t a = 0, b = 0;
    bool hangul = u >= 0xAC00u && u < 0xAC00u + 11172u;
    if (hangul || !uf->decompose(u, &a, &b)) { a = 0; b = 0; }
    put_u8(ucd, gc); put_u8(ucd, ccc); put_u8(ucd, jt); put_u8(ucd, use);
    put_u16(ucd, indic); put_u8(ucd, flags); put_u8(ucd, 0);
    put_u32(ucd, mirror); put_u32(ucd, script); put_u32(ucd, a); put_u32(ucd, b);
    if (a && b)
    {
      hb_codepoint_t ab = 0;
      if (uf->compose(a, b, &ab) && ab == u)
	fprintf(comp, "%u %u %u\n", a, b, u);
    }
  }
  fclose(ucd);
  fclose(comp);

  scripts.insert((unsigned) HB_SCRIPT_COMMON);
  scripts.insert((unsigned) HB_SCRIPT_INHERITED);
  scripts.insert((unsigned) HB_SCRIPT_UNKNOWN);
  scripts.insert((unsigned) HB_SCRIPT_INVALID);
  hb_language_t c_lang = hb_language_from_string("c", -1);
  FILE *sc = open_out(dir, "scripts.txt");
  for (unsigned s : scripts)
  {
    hb_tag_t stags[HB_OT_MAX_TAGS_PER_SCRIPT], ltags[HB_OT_MAX_TAGS_PER_LANGUAGE];
    unsigned sn = HB_OT_MAX_TAGS_PER_SCRIPT, ln = HB_OT_MAX_TAGS_PER_LANGUAGE;
    hb_ot_tags_from_script_and_language((hb_script_t) s, c_lang, &sn, stags, &ln, ltags);
    fprintf(sc, "%u %u %u", s, (unsigned) hb_script_get_horizontal_direction((hb_script_t) s), sn);
    for (unsigned i = 0; i < sn; i++) fprintf(sc, " %u", stags[i]);
    fprintf(sc, " %u", ln);
    for (unsigned i = 0; i < ln; i++) fprintf(sc, " %u", ltags[i]);
    /* The shaper hb_ot_shaper_categorize picks when the font's GSUB has a
     * script tag of its own (neither DFLT, latn, mymr nor a '3' tag). */
    const hb_ot_shaper_t *sh = hb_ot_shaper_categorize((hb_script_t) s, HB_DIRECTION_LTR, HB_TAG('z','z','z','z'));
    const hb_ot_shaper_t *list[] = {&_hb_ot_shaper_default, &_hb_ot_shaper_arabic, &_hb_ot_shaper_thai,
				    &_hb_ot_shaper_hangul, &_hb_ot_shaper_hebrew, &_hb_ot_shaper_indic,
				    &_hb_ot_shaper_khmer, &_hb_ot_shaper_myanmar, &_hb_ot_shaper_use,
				    &_hb_ot_shaper_myanmar_zawgyi};
    unsigned id = 99;
    for (unsigned k = 0; k < 10; k++) if (list[k] == sh) id = k;
    fprintf(sc, " %u\n", id);
  }
  fclose(sc);

  FILE *mcc = open_out(dir, "mcc.txt");
  for (unsigned i = 0; i < 256; i++) fprintf(mcc, "%u\n", _hb_modified_combining_class[i]);
  fclose(mcc);

  FILE *ar = open_out(dir, "arabic.txt");
  fprintf(ar, "shaping %u %u\n", SHAPING_TABLE_FIRST, SHAPING_TABLE_LAST);
  for (unsigned u = SHAPING_TABLE_FIRST; u <= SHAPING_TABLE_LAST; u++)
  {
    const uint16_t *r = shaping_table[u - SHAPING_TABLE_FIRST];
    fprintf(ar, "%u %u %u %u\n", r[0], r[1], r[2], r[3]);
  }
  for (const auto &set : ligature_table)
    for (const auto &lig : set.ligatures)
      if (lig.ligature)
	fprintf(ar, "lig2 %u %u %u\n", set.first, lig.components[0], lig.ligature);
  for (const auto &set : ligature_mark_table)
    for (const auto &lig : set.ligatures)
      if (lig.ligature)
	fprintf(ar, "ligmark %u %u %u\n", set.first, lig.components[0], lig.ligature);
  for (const auto &set : ligature_3_table)
    for (const auto &lig : set.ligatures)
      if (lig.ligature)
	fprintf(ar, "lig3 %u %u %u %u\n", set.first, lig.components[0], lig.components[1], lig.ligature);
  fclose(ar);

  FILE *pua = open_out(dir, "pua.txt");
  for (unsigned u = 0; u < 0x10000u; u++)
  {
    unsigned s = _hb_arabic_pua_simp_map(u), t = _hb_arabic_pua_trad_map(u);
    if (s || t) fprintf(pua, "%u %u %u\n", u, s, t);
  }
  fclose(pua);

  FILE *misc = open_out(dir, "misc.txt");
  fprintf(misc, "default_language %s\n", hb_language_to_string(hb_language_get_default()));
  fclose(misc);
  return 0;
}
