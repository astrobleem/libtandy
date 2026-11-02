/* SPDX-License-Identifier: MIT */
/* Independent per-pixel rectangles and clear guards, including odd edges. */
#include <stdio.h>
#include "lt_gfx.h"
#define G 16U
static lt_gfx_u8 LT_GFX_FAR actual[LT_GFX_BYTES+2U*G];
static lt_gfx_u8 LT_GFX_FAR expect[LT_GFX_BYTES+2U*G];
static unsigned long failures,cases;
static lt_gfx_surface surface;
static void compare(void)
{
    unsigned i;for(i=0;i<LT_GFX_BYTES+2U*G;++i)if(actual[i]!=expect[i]) {
        printf("FAIL fill case=%lu at=%u got=%u want=%u\n",cases,i,(unsigned)actual[i],(unsigned)expect[i]);++failures;return;
    }
}
static void rect(short x0,short y0,short x1,short y1,unsigned c)
{
    unsigned x,y,at;unsigned char v=(unsigned char)(c&15U);
    ++cases;lt_gfx_rect(&surface,x0,y0,x1,y1,(unsigned char)c);
    for(y=0;y<200U;++y)for(x=0;x<320U;++x)
        if((long)x>=x0&&(long)x<x1&&(long)y>=y0&&(long)y<y1) {
            at=G+(y%4U)*8192U+(y/4U)*160U+x/2U;
            expect[at]=(unsigned char)((x&1U)?((expect[at]&240U)|v):((expect[at]&15U)|(v<<4)));
        }
    compare();
}
int main(void)
{
    unsigned i,c,x;surface.pixels=actual+G;
    for(i=0;i<LT_GFX_BYTES+2U*G;++i)actual[i]=expect[i]=0xa5U;
    for(c=0;c<256U;++c) {
        lt_gfx_clear(&surface,(unsigned char)c);++cases;
        for(i=0;i<LT_GFX_BYTES;++i)expect[G+i]=(unsigned char)((c&15U)*17U);
        compare();
    }
    for(x=0;x<32U;++x) {
        rect((short)x,0,(short)(x+1U),200,x);
        rect((short)x,1,(short)(x+16U),199,x+3U);
        rect((short)x,2,(short)(x+17U),198,x+7U);
        rect((short)x,3,(short)(319U-x),197,x+11U);
    }
    rect(-32768,-32768,32767,32767,15);
    rect(-32768,0,-32760,200,2);rect(32760,0,32767,200,2);
    rect(0,32760,320,32767,2);rect(0,-32768,320,-32760,2);
    rect(319,199,32767,32767,0);rect(-1,-1,1,1,0);
    rect(10,20,9,19,7);rect(20,20,20,21,7);
    printf("gfx_fill: %s cases=%lu failures=%lu int=%u pointer=%u\n",failures?"FAIL":"PASS",cases,failures,(unsigned)sizeof(int),(unsigned)sizeof(void*));
    return failures?1:0;
}
