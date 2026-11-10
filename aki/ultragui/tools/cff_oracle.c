/* Differential oracle for font/cff. Build inside the dev shell:
 * clang -O2 tools/cff_oracle.c $(pkg-config --cflags --libs freetype2) -o build/cff_oracle/oracle
 * oracle FONT OUTPUT [broad|quick|SIZE] [unhinted]
 * Little-endian words, no native padding. Never linked into the Aki engine.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
static FILE *out;
static void word(int64_t v) { uint32_t n=(uint32_t)v; for(int i=0;i<4;i++) fputc((n>>(i*8))&255,out); }
static void list(const char *dir) {
  DIR *d=opendir(dir); if(!d) return; struct dirent *e;
  while((e=readdir(d))) { if(e->d_name[0]=='.') continue; char path[4096];
    snprintf(path,sizeof(path),"%s/%s",dir,e->d_name); struct stat s;
    if(stat(path,&s)) continue; if(S_ISDIR(s.st_mode)) { list(path); continue; }
    FILE *f=fopen(path,"rb"); if(!f) continue; char sig[4]; size_t n=fread(sig,1,4,f); fclose(f);
    if(n==4 && !memcmp(sig,"OTTO",4)) puts(path);
  } closedir(d);
}
int main(int argc,char **argv) {
  if(argc==2 && !strcmp(argv[1],"--list")) { list("/usr/share/fonts"); return 0; }
  if(argc<3) return 2;
  FT_Library lib; FT_Face face;
  if(FT_Init_FreeType(&lib) || FT_New_Face(lib,argv[1],0,&face)) return 3;
  int major,minor,patch; FT_Library_Version(lib,&major,&minor,&patch);
  if(major!=2 || minor!=14 || patch!=3) { fprintf(stderr,"need FreeType 2.14.3, got %d.%d.%d\n",major,minor,patch); return 4; }
  out=fopen(argv[2],"wb"); if(!out) return 5;
  word(0x43464631); word(face->num_glyphs); word(face->units_per_EM);
  word(face->ascender); word(face->descender); word(face->height); word(face->max_advance_width);
  int sizes[64],n=0;
  if(argc>3 && !strcmp(argv[3],"broad")) { int b[]={7,9,11,12,13,14,16,18,20,24,32,48,72}; n=13; memcpy(sizes,b,sizeof(b)); }
  else if(argc>3 && !strcmp(argv[3],"quick")) { int b[]={7,12,24,48}; n=4; memcpy(sizes,b,sizeof(b)); }
  else if(argc>3 && atoi(argv[3])>0) { sizes[n++]=atoi(argv[3]); }
  else { for(int i=6;i<=48;i++) sizes[n++]=i; sizes[n++]=64; sizes[n++]=96; }
  word(n);
  int modes=argc>4 && !strcmp(argv[4],"unhinted") ? 1 : 2; word(modes);
  for(int s=0;s<n;s++) {
    word(sizes[s]); word(FT_Set_Pixel_Sizes(face,0,sizes[s]));
    FT_Size_Metrics m=face->size->metrics;
    word(m.x_ppem); word(m.y_ppem); word(m.x_scale); word(m.y_scale);
    word(m.ascender); word(m.descender); word(m.height); word(m.max_advance);
    for(int gid=0;gid<face->num_glyphs;gid++) for(int h=0;h<modes;h++) {
      int err=FT_Load_Glyph(face,gid,h ? FT_LOAD_DEFAULT : FT_LOAD_NO_HINTING); word(err);
      if(err) continue; FT_GlyphSlot g=face->glyph;
      word(g->advance.x); word(g->linearHoriAdvance);
      word(g->metrics.width); word(g->metrics.height); word(g->metrics.horiBearingX); word(g->metrics.horiBearingY); word(g->metrics.horiAdvance);
      word(g->outline.flags); word(g->outline.n_points); word(g->outline.n_contours);
      for(int i=0;i<g->outline.n_points;i++) { word(g->outline.points[i].x); word(g->outline.points[i].y); word((unsigned char)g->outline.tags[i]); }
      for(int i=0;i<g->outline.n_contours;i++) word(g->outline.contours[i]);
    }
  }
  int err=ferror(out); if(fclose(out)) err=1;
  FT_Done_Face(face); FT_Done_FreeType(lib); return err ? 6 : 0;
}
