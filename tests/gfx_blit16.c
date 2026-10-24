/* SPDX-License-Identifier: MIT */
/* Guarded-buffer check of lt_gfx_blit_packed at 16-bit extremes, meant
 * for the real 16-bit build (where int is 16 bits): the frame sits between
 * two guard areas, and every blit must leave them untouched. */
#include <stdio.h>
#include "lt_gfx.h"

#define GUARD 64U
static lt_gfx_u8 LT_GFX_FAR buf[LT_GFX_BYTES + 2U * GUARD];
static lt_gfx_u8 src[4 * 20];

static unsigned damaged(void)
{
    unsigned i, n = 0;
    for (i = 0; i < GUARD; ++i) {
        if (buf[i] != 0xC3U) ++n;
        if (buf[GUARD + LT_GFX_BYTES + i] != 0xC3U) ++n;
    }
    return n;
}

static int one(lt_gfx_i16 bx, lt_gfx_i16 y, lt_gfx_i16 w, lt_gfx_i16 rows)
{
    lt_gfx_surface g;
    unsigned i, n;
    for (i = 0; i < sizeof(buf); ++i) buf[i] = 0xC3U;
    lt_gfx_init(&g, buf + GUARD);
    lt_gfx_blit_packed(&g, bx, y, src, 20U, w, rows);
    n = damaged();
    printf("%s bx=%d y=%d width=%d rows=%d guard bytes changed=%u\n",
           n ? "FAIL" : "ok", bx, y, w, rows, n);
    return n != 0;
}

int main(void)
{
    static const lt_gfx_i16 xs[] = { 32760, 32767, 160, -32768, -32760,
                                     -20, 159 };
    static const lt_gfx_i16 ys[] = { 0, 199, 200, 32767, -32768, -1 };
    unsigned a, b;
    int bad;
    for (a = 0; a < sizeof(src); ++a) src[a] = (lt_gfx_u8)a;
    /* the reported case first */
    bad = one(32760, 0, 20, 1);
    if (bad) {
        puts("gfx_blit16: FAIL (stopped before the wider sweep)");
        return 1;
    }
    for (a = 0; a < sizeof(xs) / sizeof(xs[0]); ++a)
        for (b = 0; b < sizeof(ys) / sizeof(ys[0]); ++b) {
            bad |= one(xs[a], ys[b], 20, 1);
            bad |= one(xs[a], ys[b], 32767, 4);
            bad |= one(xs[a], ys[b], 20, 32767);
        }
    puts(bad ? "gfx_blit16: FAIL" : "gfx_blit16: all guards intact");
    return bad;
}
