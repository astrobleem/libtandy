/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "lt_gfx_tile8.h"
#define GUARD 16U
static lt_gfx_u8 LT_GFX_FAR actual[LT_GFX_BYTES + GUARD*2U];
static lt_gfx_u8 LT_GFX_FAR wanted[LT_GFX_BYTES + GUARD*2U];
static lt_gfx_u8 LT_GFX_FAR source[64];
static unsigned long cases, failures;
static lt_gfx_surface surface;

static void reset(void)
{
    unsigned i;
    for (i=0; i<LT_GFX_BYTES+GUARD*2U; ++i)
        actual[i]=wanted[i]=(lt_gfx_u8)(i*29U+11U);
}
static void compare(void)
{
    unsigned i;
    for(i=0;i<LT_GFX_BYTES+GUARD*2U;++i) if(actual[i]!=wanted[i]) {
        printf("FAIL case=%lu offset=%u got=%u want=%u\n",cases,i,
               (unsigned)actual[i],(unsigned)wanted[i]);++failures;return;
    }
}
static void draw(short x,short y,unsigned bias)
{
    unsigned i,j,at;long xx,yy;unsigned char pixel,old;
    ++cases;
    for(i=0;i<32U;++i)source[16U+i]=(unsigned char)(i*23U+bias);
    if(!lt_gfx_tile8(&surface,x,y,source+16U)){++failures;return;}
    for(j=0;j<8U;++j)for(i=0;i<8U;++i) {
        xx=(long)x+(long)i;yy=(long)y+(long)j;
        if(xx<0L||xx>=320L||yy<0L||yy>=200L)continue;
        pixel=source[16U+j*4U+i/2U];
        pixel=(unsigned char)((i&1U)?(pixel&15U):(pixel>>4));
        at=GUARD+((unsigned)yy%4U)*8192U+((unsigned)yy/4U)*160U+(unsigned)xx/2U;
        old=wanted[at];wanted[at]=(unsigned char)((xx&1L)?((old&240U)|pixel):((old&15U)|(pixel<<4)));
    }
}
int main(void)
{
    static const short edge_x[]={-32768,-32760,-9,-8,-7,-4,-1,0,1,2,7,8,310,311,312,313,316,319,320,32760,32767};
    static const short edge_y[]={-32768,-32760,-9,-8,-7,-4,-1,0,1,2,3,4,7,8,190,191,192,193,196,199,200,32760,32767};
    unsigned x,y,bias,i;lt_gfx_surface empty;
    surface.pixels=actual+GUARD;empty.pixels=0;reset();
    if(lt_gfx_tile8(0,0,0,source)||lt_gfx_tile8(&empty,0,0,source)||lt_gfx_tile8(&surface,0,0,0))++failures;
    compare();
    for(x=0;x<sizeof edge_x/sizeof edge_x[0];++x)
        for(y=0;y<sizeof edge_y/sizeof edge_y[0];++y){reset();draw(edge_x[x],edge_y[y],x+y);compare();}
    /* Every in-bounds even-column/bank-zero fast-path start. Compare each
     * band after cumulative draws; boundary cases above compare per call. */
    reset();for(y=0;y<=192U;y+=4U){for(x=0;x<=312U;x+=2U)draw((short)x,(short)y,x+y);compare();}
    /* Every source byte value, all destination phase combinations. */
    for(bias=0;bias<256U;++bias){reset();for(y=0;y<4U;++y)for(x=0;x<2U;++x)draw((short)(80U+x),(short)(80U+y),bias);compare();}
    for(i=0;i<16U;++i)if(source[i]||source[48U+i])++failures;
    printf("tile8: %s cases=%lu failures=%lu int=%u pointer=%u\n",failures?"FAIL":"PASS",cases,failures,(unsigned)sizeof(int),(unsigned)sizeof(void*));
    return failures?1:0;
}
