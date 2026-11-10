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
static AF_ScriptClassRec script = { .top_to_bottom_hinting = 0 };
const AF_ScriptClass af_script_classes[] = { &script };
static const AF_StyleClassRec style = { .script = 0 };
#include "aflatin.c"

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
static void metric_values(AF_LatinMetrics m) {
  num(m->root.scaler.x_scale); num(m->root.scaler.y_scale);
  for(int d=0;d<2;d++) {
    AF_LatinAxis a=m->axis+d;
    num(a->scale); num(a->delta); num(a->org_scale); num(a->org_delta); num(a->extra_light);
    word(a->width_count);
    for(unsigned j=0;j<a->width_count;j++) { num(a->widths[j].org); num(a->widths[j].cur); num(a->widths[j].fit); }
    word(a->blue_count);
    for(unsigned j=0;j<a->blue_count;j++) {
      AF_LatinBlue b=a->blues+j;
      num(b->ref.org); num(b->ref.cur); num(b->ref.fit); num(b->shoot.org); num(b->shoot.cur); num(b->shoot.fit);
      num(b->ascender); num(b->descender); num(b->flags);
    }
  }
}
static void standard_widths(FT_Face face, FT_Outline *outline) {
  AF_LatinMetricsRec m;
  AF_GlyphHintsRec h;
  memset(&m,0,sizeof(m));
  m.units_per_em=face->units_per_EM; m.root.scaler.face=face;
  af_glyph_hints_init(&h,face->memory);
  h.metrics=(AF_StyleMetrics)&m; h.x_scale=0x10000; h.y_scale=0x10000;
  if(af_glyph_hints_reload(&h,outline)) exit(6);
  for(int d=0;d<2;d++) {
    af_latin_hints_compute_segments(&h,(AF_Dimension)d);
    af_latin_hints_link_segments(&h,0,NULL,(AF_Dimension)d);
    AF_AxisHints a=h.axis+d;
    unsigned count=0;
    for(unsigned j=0;j<a->num_segments;j++) {
      AF_Segment s=a->segments+j;
      if(s->link && s->link->link==s && s->link>s && count<16) {
        long dist=s->pos-s->link->pos;
        m.axis[d].widths[count++].org=dist<0?-dist:dist;
      }
    }
    if(outline->n_points>0) af_sort_and_quantize_widths(&count,m.axis[d].widths,m.units_per_em/100);
    long standard=count?m.axis[d].widths[0].org:50*m.units_per_em/2048;
    word(count); num(standard); num(standard/5);
    for(unsigned j=0;j<count;j++) num(m.axis[d].widths[j].org);
  }
  af_glyph_hints_done(&h);
}
static int record(FT_Face face, unsigned gid, unsigned px) {
  script.top_to_bottom_hinting=0;
  if(FT_Set_Pixel_Sizes(face,0,px) || FT_Load_Glyph(face,gid,FT_LOAD_NO_SCALE|FT_LOAD_LINEAR_DESIGN)) return 1;
  FT_Outline *o=&face->glyph->outline;
  AF_GlyphHintsRec h;
  AF_LatinMetricsRec metrics;
  memset(&metrics,0,sizeof(metrics));
  metrics.root.style_class=&style;
  metrics.root.scaler.face=face;
  metrics.units_per_em=face->units_per_EM;
  AF_FaceGlobalsRec globals;
  memset(&globals,0,sizeof(globals));
  globals.increase_x_height=(gid%3)==0?16:0;
  metrics.root.globals=&globals;
  for(int d=0;d<2;d++) {
    metrics.axis[d].width_count=2;
    metrics.axis[d].widths[0].org=face->units_per_EM/12;
    metrics.axis[d].widths[1].org=face->units_per_EM/8;
    metrics.axis[d].standard_width=face->units_per_EM/12;
  }
  metrics.axis[1].blue_count=5;
  for(unsigned i=0;i<5;i++) {
    AF_LatinBlue b=metrics.axis[1].blues+i;
    long u=face->units_per_EM;
    b->ref.org=i==0?0:i==1?u*7/10:i==2?u/2:i==3?u/4:u/2-1;
    b->shoot.org=b->ref.org+(i==0?-u/100:u/100);
    b->flags=i==0?AF_LATIN_BLUE_BOTTOM:i==1?AF_LATIN_BLUE_TOP:i==2?(AF_LATIN_BLUE_TOP|AF_LATIN_BLUE_ADJUSTMENT):i==3?AF_LATIN_BLUE_NEUTRAL:AF_LATIN_BLUE_SUB_TOP;
    b->ascender=u*9/10; b->descender=-u/4;
  }
  AF_ScalerRec scaler;
  memset(&scaler,0,sizeof(scaler));
  scaler.face=face; scaler.x_scale=face->size->metrics.x_scale; scaler.y_scale=face->size->metrics.y_scale;
  scaler.render_mode=FT_RENDER_MODE_NORMAL;
  af_latin_metrics_scale((AF_StyleMetrics)&metrics,&scaler);
  af_glyph_hints_init(&h,face->memory);
  h.metrics=(AF_StyleMetrics)&metrics;
  h.other_flags=AF_LATIN_HINTS_STEM_ADJUST;
  h.x_scale=metrics.axis[0].scale;
  h.y_scale=metrics.axis[1].scale;
  if(af_glyph_hints_reload(&h,o)) return 1;
  word(1); word(gid); word(px); word(face->units_per_EM); num(h.x_scale); num(h.y_scale);
  word(o->n_points); word(o->n_contours); word(o->flags);
  for(int i=0;i<o->n_points;i++) { num(o->points[i].x); num(o->points[i].y); word((unsigned char)o->tags[i]); }
  for(int i=0;i<o->n_contours;i++) word(o->contours[i]);
  metric_values(&metrics);
  if(getenv("AUTOFIT_STAGE_WIDTHS")) {
    FT_Glyph_Metrics *g=&face->glyph->metrics;
    num(g->width); num(g->height); num(g->horiBearingX); num(g->horiBearingY); num(g->horiAdvance);
    num(g->vertBearingX); num(g->vertBearingY); num(g->vertAdvance);
    num(face->glyph->advance.x); num(face->glyph->linearHoriAdvance); num(face->glyph->linearVertAdvance);
    standard_widths(face,o);
  }
  points(&h,1);
  for(int d=0;d<2;d++) {
    af_latin_hints_compute_segments(&h,(AF_Dimension)d);
    /* Exercise adjusted scoring and the no-width standard-stem path. */
    AF_WidthRec widths[1]={{0}};
    widths[0].org=(gid&1) ? face->units_per_EM/12 : 0;
    af_latin_hints_link_segments(&h,widths[0].org?1:0,widths,(AF_Dimension)d);
    metrics.axis[d].edge_distance_threshold=face->units_per_EM/100;
    af_latin_hints_compute_edges(&h,(AF_Dimension)d);
    if(d==1) af_latin_hints_compute_blue_edges(&h,&metrics);
    features(&h,d);
    af_latin_hint_edges(&h,(AF_Dimension)d);
    for(unsigned i=0;i<h.axis[d].num_edges;i++) {
      AF_Edge e=h.axis[d].edges+i;
      num(e->pos); num(e->flags);
      unsigned bi=UINT32_MAX,shoot=0;
      for(unsigned j=0;j<metrics.axis[1].blue_count;j++) {
        if(e->blue_edge==&metrics.axis[1].blues[j].ref) {bi=j;break;}
        if(e->blue_edge==&metrics.axis[1].blues[j].shoot) {bi=j;shoot=1;break;}
      }
      num(bi); num(shoot);
    }
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
  if(getenv("AUTOFIT_STAGE_WIDTHS")) {
    if(FT_Load_Glyph(face,gid,FT_LOAD_NO_SCALE|FT_LOAD_LINEAR_DESIGN)) return 1;
    o=&face->glyph->outline;
    FT_HashRec reverse;
    if(ft_hash_num_init(&reverse,face->memory)) return 1;
    metrics.root.reverse_charmap=&reverse;
    globals.glyph_styles=calloc((size_t)face->num_glyphs,sizeof(FT_UShort));
    if(!globals.glyph_styles) return 1;
    if((gid%7)==0) globals.glyph_styles[gid]=AF_NONBASE;
    af_latin_hints_init(&h,(AF_StyleMetrics)&metrics);
    /* Vary style policy independently of the real face's native flags. */
    h.scaler_flags=(gid%11)==0?AF_SCALER_FLAG_NO_HORIZONTAL:0;
    script.top_to_bottom_hinting=(gid%13)==0;
    if(af_latin_hints_apply(gid,&h,o,(AF_StyleMetrics)&metrics)) return 1;
    for(int i=0;i<o->n_points;i++) { num(o->points[i].x); num(o->points[i].y); word((unsigned char)o->tags[i]); }
    free(globals.glyph_styles);
    ft_hash_num_free(&reverse,face->memory);
  }
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
  word(0x41464832); word(major); word(minor); word(patch); word(face->num_glyphs);
  unsigned stride=(unsigned)atoi(argv[3]); if(!stride) stride=1;
  for(int i=4;i<argc;i++) for(unsigned gid=0;gid<(unsigned)face->num_glyphs;gid+=stride) if(record(face,gid,(unsigned)atoi(argv[i]))) return 6;
  word(0);
  int error=ferror(out)||fclose(out);
  hb_font_destroy(hb); FT_Done_Face(face); FT_Done_FreeType(lib);
  return error?4:0;
}
