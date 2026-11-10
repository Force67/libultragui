/* Differential oracle for autofit's outline-analysis stages. Build with
 * build_autofit_oracle.sh: it compiles the actual FreeType 2.14.3 sources,
 * not a second implementation of the algorithms under test.
 * Both FreeType and HarfBuzz are linked and used in this process. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hb.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include "afglobal.h"
#include "aflatin.h"

/* Feature analysis only consults this flag; actual face coverage and blue
 * metrics are deliberately not represented by this stage oracle. */
static const AF_ScriptClassRec script = { .top_to_bottom_hinting = 0 };
const AF_ScriptClass af_script_classes[] = { &script };
static const AF_StyleClassRec style = { .script = 0 };

static FILE *out;
static void word(uint32_t v) { unsigned char b[4]; for (int i=0;i<4;i++) b[i]=(unsigned char)(v>>(8*i)); if(fwrite(b,1,4,out)!=4) exit(4); }
static void num(int64_t v) { word((uint32_t)v); word((uint32_t)((uint64_t)v>>32)); }
static uint32_t pi(AF_GlyphHints h, AF_Point p) { return p ? (uint32_t)(p-h->points) : UINT32_MAX; }
static uint32_t si(AF_AxisHints a, AF_Segment p) { return p ? (uint32_t)(p-a->segments) : UINT32_MAX; }
static uint32_t ei(AF_AxisHints a, AF_Edge p) { return p ? (uint32_t)(p-a->edges) : UINT32_MAX; }
static void points(AF_GlyphHints h, int reload) {
  for(int i=0;i<h->num_points;i++) {
    AF_Point p=h->points+i;
    num(p->flags); num(p->in_dir); num(p->out_dir);
    num(p->fx); num(p->fy); num(p->ox); num(p->oy); num(p->x); num(p->y); num(reload && (p->flags & AF_FLAG_WEAK_INTERPOLATION) ? 0 : p->u); num(reload && (p->flags & AF_FLAG_WEAK_INTERPOLATION) ? 0 : p->v);
    num(pi(h,p->next)); num(pi(h,p->prev));
  }
}
static void features(AF_GlyphHints h,int dim) {
  AF_AxisHints a=h->axis+dim;
  word(a->num_segments); word(a->num_edges); num(a->major_dir);
  for(unsigned i=0;i<a->num_segments;i++) {
    AF_Segment s=a->segments+i;
    num(s->flags); num(s->dir); num(s->pos); num(s->delta); num(s->min_coord); num(s->max_coord); num(s->height);
    num(ei(a,s->edge)); num(si(a,s->edge_next)); num(si(a,s->link)); num(si(a,s->serif)); num(s->score); num(s->len); num(pi(h,s->first)); num(pi(h,s->last));
  }
  for(unsigned i=0;i<a->num_edges;i++) {
    AF_Edge e=a->edges+i;
    num(e->fpos); num(e->opos); num(e->pos); num(e->flags); num(e->dir); num(e->scale);
    num(ei(a,e->link)); num(ei(a,e->serif)); num(si(a,e->first)); num(si(a,e->last));
  }
}
static int record(FT_Face face, unsigned gid, unsigned px) {
  if(FT_Set_Pixel_Sizes(face,0,px) || FT_Load_Glyph(face,gid,FT_LOAD_NO_SCALE)) return 1;
  FT_Outline *o=&face->glyph->outline;
  AF_GlyphHintsRec h;
  AF_LatinMetricsRec metrics;
  memset(&metrics,0,sizeof(metrics));
  metrics.root.style_class=&style;
  metrics.root.scaler.face=face;
  metrics.units_per_em=face->units_per_EM;
  af_glyph_hints_init(&h,face->memory);
  h.metrics=(AF_StyleMetrics)&metrics;
  h.x_scale=face->size->metrics.x_scale;
  h.y_scale=face->size->metrics.y_scale;
  if(af_glyph_hints_reload(&h,o)) return 1;
  word(1); word(gid); word(px); word(face->units_per_EM); num(h.x_scale); num(h.y_scale);
  word(o->n_points); word(o->n_contours); word(o->flags);
  for(int i=0;i<o->n_points;i++) { num(o->points[i].x); num(o->points[i].y); word((unsigned char)o->tags[i]); }
  for(int i=0;i<o->n_contours;i++) word(o->contours[i]);
  points(&h,1);
  for(int d=0;d<2;d++) {
    af_latin_hints_compute_segments(&h,(AF_Dimension)d);
    /* Exercise adjusted scoring and the no-width standard-stem path. */
    AF_WidthRec widths[1]={{0}};
    widths[0].org=(gid&1) ? face->units_per_EM/12 : 0;
    af_latin_hints_link_segments(&h,widths[0].org?1:0,widths,(AF_Dimension)d);
    metrics.axis[d].edge_distance_threshold=face->units_per_EM/100;
    af_latin_hints_compute_edges(&h,(AF_Dimension)d);
    features(&h,d);
    for(unsigned i=0;i<h.axis[d].num_edges;i++) {
      AF_Edge e=h.axis[d].edges+i;
      e->pos=((e->opos+32)&~63L)+(long)((i*17+gid*7)%41)-20;
    }
    af_glyph_hints_align_edge_points(&h,(AF_Dimension)d);
    af_glyph_hints_align_strong_points(&h,(AF_Dimension)d);
    af_glyph_hints_align_weak_points(&h,(AF_Dimension)d);
    points(&h,0);
  }
  af_glyph_hints_save(&h,o);
  for(int i=0;i<o->n_points;i++) { num(o->points[i].x); num(o->points[i].y); word((unsigned char)o->tags[i]); }
  af_glyph_hints_done(&h);
  return 0;
}
int main(int argc,char **argv) {
  if(argc<4) { fprintf(stderr,"usage: %s font output stride [sizes...]\n",argv[0]); return 2; }
  FT_Library lib; FT_Face face;
  if(FT_Init_FreeType(&lib)||FT_New_Face(lib,argv[1],0,&face)) return 3;
  int major,minor,patch; FT_Library_Version(lib,&major,&minor,&patch);
  if(major!=2||minor!=14||patch!=3) return 5;
  hb_font_t *hb=hb_ft_font_create_referenced(face);
  if(!hb || !hb_version_atleast(13,2,1)) return 5;
  out=fopen(argv[2],"wb"); if(!out) return 4;
  word(0x41464831); word(major); word(minor); word(patch); word(face->num_glyphs);
  unsigned stride=(unsigned)atoi(argv[3]); if(!stride) stride=1;
  for(int i=4;i<argc;i++) for(unsigned gid=0;gid<(unsigned)face->num_glyphs;gid+=stride) if(record(face,gid,(unsigned)atoi(argv[i]))) return 6;
  word(0);
  int error=ferror(out)||fclose(out);
  hb_font_destroy(hb); FT_Done_Face(face); FT_Done_FreeType(lib);
  return error?4:0;
}
