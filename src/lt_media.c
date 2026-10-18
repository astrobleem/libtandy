/* SPDX-License-Identifier: MIT */
#include "lt_media.h"
static const lts_u8 rgbi[16][3] = {
 {0,0,0},{0,0,170},{0,170,0},{0,170,170},
 {170,0,0},{170,0,170},{170,85,0},{170,170,170},
 {85,85,85},{85,85,255},{85,255,85},{85,255,255},
 {255,85,85},{255,85,255},{255,255,85},{255,255,255}
};
void ltm_palette_init(ltm_palette *p)
{
 unsigned i,j;
 if (!p) return;
 for(i=0;i<256U;i++) for(j=0;j<3U;j++) p->rgb[i][j]=rgbi[i&15U][j];
}
lts_status ltm_palette_set(ltm_palette *p,lts_u16 i,
                           lts_u8 r,lts_u8 g,lts_u8 b)
{
 if(!p || i>255U) return LTS_INVALID;
 p->rgb[i][0]=r;p->rgb[i][1]=g;p->rgb[i][2]=b;
 return LTS_OK;
}
lts_status ltm_palette_map(const ltm_palette *p,lts_u8 map[256])
{
 unsigned i,j,k,best;
 long d;
 unsigned long score,minimum;
 if(!p || !map) return LTS_INVALID;
 for(i=0;i<256U;i++) {
  minimum=0xffffffffUL;best=0;
  for(j=0;j<16U;j++) {
   score=0;
   for(k=0;k<3U;k++) {d=(long)p->rgb[i][k]-rgbi[j][k];score+=(unsigned long)(d*d);}
   if(score<minimum) {minimum=score;best=j;}
  }
  map[i]=(lts_u8)best;
 }
 return LTS_OK;
}
lts_status ltm_pack(const lts_surface *s,const lts_rect *r,
                     lts_storage *target,lts_i16 dx,lts_i16 dy,
                     const lts_u8 map[256])
{
 lts_surface checked;
 lts_storage checked_target;
 lts_status status;
 long sx,sy,tx,ty,w,h,n,y,so;
 unsigned off,i,left,count;
 lts_u8 LTS_FAR *output;
 const lts_u8 LTS_FAR *input;
 lts_u8 value,old;
 if(!s || !target || !map) return LTS_INVALID;
 checked=*s;
 status=lts_key(&checked,0,0);if(status!=LTS_OK) return status;
 status=lts_storage_init(&checked_target,target->pixels,target->capacity);
 if(status!=LTS_OK) return status;
 if(target->capacity<32768U) return LTS_NOSPACE;
 if(s->storage==target || s->storage->pixels==target->pixels) return LTS_OVERLAP;
 for(i=0;i<256U;i++) if(map[i]>15U) return LTS_INVALID;
 sx=r?r->x:0;sy=r?r->y:0;w=r?r->w:s->width;h=r?r->h:s->height;
 tx=dx;ty=dy;
 if(sx<0) {n=-sx;sx=0;tx+=n;w-=n;}
 if(sy<0) {n=-sy;sy=0;ty+=n;h-=n;}
 if(sx+w>s->width) w=(long)s->width-sx;
 if(sy+h>s->height) h=(long)s->height-sy;
 if(tx<0) {n=-tx;tx=0;sx+=n;w-=n;}
 if(ty<0) {n=-ty;ty=0;sy+=n;h-=n;}
 if(tx+w>320L) w=320L-tx;
 if(ty+h>200L) h=200L-ty;
 if(w<=0 || h<=0) return LTS_OK;
 for(y=0;y<h;y++) {
  so=(long)s->offset+(sy+y)*s->pitch+sx;
  off=(unsigned)(((ty+y)&3L)*8192L+((ty+y)>>2)*160L+(tx>>1));
  input=s->storage->pixels+(lts_u16)so;
  output=target->pixels+off;
  left=(unsigned)w;
  if(tx&1L) {
   old=*output;value=map[*input++];
   *output++=(lts_u8)((old&240U)|value);left--;
  }
  count=left>>1;
  for(i=0;i<count;i++) {
   value=(lts_u8)(map[*input++]<<4);
   *output++=(lts_u8)(value|map[*input++]);
  }
  if(left&1U) {
   old=*output;value=map[*input];
   *output=(lts_u8)((old&15U)|(value<<4));
  }
 }
 return LTS_OK;
}
