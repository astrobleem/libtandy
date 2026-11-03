/* SPDX-License-Identifier: MIT */
/* Synthetic DOS buffers: no video hardware or display writes. */
#include <dos.h>
#include <stdio.h>
#include "lt_gfx_tile8.h"
static unsigned long cases, failures;
static unsigned char __far *source;
static unsigned char __far *actual;
static unsigned char __far *expected;
static lt_gfx_surface surface;
static unsigned base, source_offset;
static unsigned char packed[32];
static void reset(void)
{
    unsigned i = 0;
    do { actual[i]=expected[i]=0xa5U; ++i; } while(i);
}
static void compare(void)
{
    unsigned i = 0;
    ++cases;
    do {
        if(actual[i]!=expected[i]) {
            printf("FAIL case=%lu base=%04x source=%04x at=%04x got=%u want=%u\n",
                cases,base,source_offset,i,(unsigned)actual[i],(unsigned)expected[i]);
            ++failures;return;
        }
        ++i;
    } while(i);
}
static void pixel(unsigned x,unsigned y,unsigned char v)
{
    unsigned at=(unsigned)(base+(y&3U)*8192U+(y>>2)*160U+(x>>1));
    unsigned char old=expected[at];
    expected[at]=(unsigned char)((x&1U)?((old&240U)|(v&15U)):((old&15U)|(v<<4)));
}
static void tile(short x,short y)
{
    unsigned r,c;long xx,yy;unsigned char v;
    reset();
    if(!lt_gfx_tile8(&surface,x,y,source+source_offset))++failures;
    for(r=0;r<8;++r)for(c=0;c<8;++c) {
        xx=(long)x+c;yy=(long)y+r;
        if(xx<0||xx>=320||yy<0||yy>=200)continue;
        v=packed[r*4U+(c>>1)];
        v=(unsigned char)((c&1U)?(v&15U):(v>>4));
        pixel((unsigned)xx,(unsigned)yy,v);
    }
    compare();
}
static void rect(short x0,short y0,short x1,short y1,unsigned char color)
{
    unsigned x,y;
    reset();lt_gfx_rect(&surface,x0,y0,x1,y1,color);
    for(y=0;y<200;++y)for(x=0;x<320;++x)
        if((long)x>=x0&&(long)x<x1&&(long)y>=y0&&(long)y<y1)pixel(x,y,(unsigned char)(color&15U));
    compare();
}
int main(void)
{
    static const short xy[][2]={{0,0},{2,4},{312,192},{310,196},{1,0},
        {0,1},{311,191},{319,199},{-1,-1},{-7,192},{312,-7},
        {-32768,0},{32767,199}};
    unsigned ss=0,as=0,es=0,i,b,k,j;
    /* Three 64KiB real DOS allocations permit explicit non-normalized far
     * windows and full-buffer comparisons without near static-data limits. */
    if(_dos_allocmem(4096U,&ss)||_dos_allocmem(4096U,&as)||_dos_allocmem(4096U,&es)) {
        puts("FAIL allocation: need three 64KiB DOS blocks");
        if(ss)_dos_freemem(ss);if(as)_dos_freemem(as);if(es)_dos_freemem(es);
        return 2;
    }
    source=(unsigned char __far *)MK_FP(ss,0);
    actual=(unsigned char __far *)MK_FP(as,0);
    expected=(unsigned char __far *)MK_FP(es,0);
    for(b=0;b<2;++b) {
        base=b?0x7fffU:0x8000U;surface.pixels=actual+base;
        for(k=0;k<2;++k) {
            source_offset=k?0xffdfU:0xffe0U;
            i=0;do {source[i]=0x69U;++i;}while(i);
            for(i=0;i<32;++i)source[source_offset+i]=packed[i]=(unsigned char)(i*23U+7U);
            for(j=0;j<sizeof xy/sizeof xy[0];++j)tile(xy[j][0],xy[j][1]);
            /* Verify every source byte, including all available guard space. */
            i=0;do {
                unsigned char want=0x69U;
                if(i>=source_offset && (unsigned)(i-source_offset)<32U)want=packed[i-source_offset];
                if(source[i]!=want){++failures;break;}++i;
            }while(i);
        }
        reset();lt_gfx_clear(&surface,0x3eU);
        for(i=0;i<32768U;++i)expected[base+i]=0xeeU;
        compare();
        rect(0,0,320,200,3);rect(1,1,319,199,9);
        rect(319,199,320,200,7);rect(301,196,318,200,12);
        rect(-32768,-32768,32767,32767,0);
    }
    _dos_freemem(es);_dos_freemem(as);_dos_freemem(ss);
    printf("%s far graphics cases=%lu failures=%lu data_ptr=%u; FFE0/FFDF sources, 8000/7FFF surfaces, full64KiB guards\n",
        failures?"FAIL":"PASS",cases,failures,(unsigned)sizeof(void*));
    return failures?1:0;
}
