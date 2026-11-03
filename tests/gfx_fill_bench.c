/* SPDX-License-Identifier: MIT */
/* Compare LT_GFX_C_FILL versus native block fill. RAM only, not VRAM timing. */
#include <stdio.h>
#include <time.h>
#include "lt_gfx.h"
#ifndef LT_FILL_BENCH_FRAMES
#define LT_FILL_BENCH_FRAMES 100U
#endif
static lt_gfx_u8 LT_GFX_FAR pixels[LT_GFX_BYTES];
static lt_gfx_surface surface;
int main(void)
{
    unsigned i,j;clock_t start,end;unsigned long h;
    surface.pixels=pixels;
    for(j=0;j<2U;++j) {
        start=clock();
        for(i=0;i<LT_FILL_BENCH_FRAMES;++i) {
            if(j)lt_gfx_rect(&surface,1,0,319,200,(lt_gfx_u8)i);
            else lt_gfx_clear(&surface,(lt_gfx_u8)i);
        }
        end=clock();h=2166136261UL;
        for(i=0;i<LT_GFX_BYTES;++i)h=(h^pixels[i])*16777619UL;
        printf("FILLBEN op=%s count=%u clocks=%lu clock_hz=%lu hash=%08lx\n",j?"rect":"clear",(unsigned)LT_FILL_BENCH_FRAMES,(unsigned long)(end-start),(unsigned long)CLOCKS_PER_SEC,h);
    }
    return 0;
}
