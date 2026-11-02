/* SPDX-License-Identifier: MIT */
/* Relative tile-copy benchmark, not a calibrated physical EX speed claim. */
#include <stdio.h>
#include <time.h>
#include "lt_gfx_tile8.h"
#ifndef LT_TILE8_BENCH_FRAMES
#define LT_TILE8_BENCH_FRAMES 200U
#endif
static lt_gfx_u8 LT_GFX_FAR pixels[LT_GFX_BYTES];
static lt_gfx_u8 LT_GFX_FAR tile[32];
static lt_gfx_surface surface;
static unsigned long hash(void)
{
    unsigned i;unsigned long h=2166136261UL;
    for(i=0;i<LT_GFX_BYTES;++i) h=(h^pixels[i])*16777619UL;
    return h;
}
int main(void)
{
    unsigned method,frame,x,y,i;clock_t start,stop;unsigned long checksums[2];
    surface.pixels=pixels;
    for(i=0;i<32U;++i)tile[i]=(lt_gfx_u8)(i*23U+3U);
    for(method=0;method<2U;++method) {
        for(i=0;i<LT_GFX_BYTES;++i)pixels[i]=0;
        start=clock();
        for(frame=0;frame<LT_TILE8_BENCH_FRAMES;++frame)
            for(y=0;y<=192U;y+=8U)for(x=0;x<=312U;x+=8U) {
                if(method)lt_gfx_tile8(&surface,(short)x,(short)y,tile);
                else lt_gfx_blit_packed(&surface,(short)(x/2U),(short)y,tile,4U,4,8);
            }
        stop=clock();checksums[method]=hash();
        printf("TILEBEN method=%s tiles=%lu clocks=%lu clock_hz=%lu hash=%08lx\n",
               method?"tile8":"packed",1000UL*LT_TILE8_BENCH_FRAMES,(unsigned long)(stop-start),
               (unsigned long)CLOCKS_PER_SEC,checksums[method]);
    }
    puts(checksums[0]==checksums[1]?"PASS tile benchmark checksums":"FAIL tile benchmark checksums");
    return checksums[0]!=checksums[1];
}
