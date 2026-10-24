/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_gfx.h"

static lt_gfx_u8 guarded[LT_GFX_BYTES + 2U];
static lt_gfx_surface surface;
static lt_gfx_u8 LT_GFX_FAR second[LT_GFX_BYTES];
static lt_gfx_u8 packed[4 * 20];
#define BLIT_GUARD 64U
static lt_gfx_u8 LT_GFX_FAR blitbuf[LT_GFX_BYTES + 2U * BLIT_GUARD];
static int failures;

#define CHECK(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++failures; \
} } while (0)

static unsigned int ref_offset(unsigned int x, unsigned int y)
{
    return (y & 3U) * 8192U + (y >> 2) * 160U + (x >> 1);
}

static void check_guards(void)
{
    CHECK(guarded[0] == 0xA5U);
    CHECK(guarded[LT_GFX_BYTES + 1U] == 0x5AU);
}

int main(void)
{
    lt_gfx_u8 *fb = guarded + 1;
    lt_gfx_u8 rows[8] = { 0x81U,0x42U,0x24U,0x18U,0x18U,0x24U,0x42U,0x81U };
    unsigned int x, y, i;
    memset(guarded, 0, sizeof(guarded));
    guarded[0] = 0xA5U; guarded[LT_GFX_BYTES + 1U] = 0x5AU;
    lt_gfx_init(&surface, fb);
    CHECK(sizeof(lt_gfx_i16) == 2U);

    /* Explicit plane/bank edges and high-even/low-odd nibble order. */
    lt_gfx_clear(&surface, 3U);
    lt_gfx_put(&surface, 0, 0, 0xEU);
    CHECK(fb[0] == 0xE3U && lt_gfx_get(&surface, 0, 0) == 14);
    lt_gfx_put(&surface, 1, 0, 5U);
    CHECK(fb[0] == 0xE5U && lt_gfx_get(&surface, 1, 0) == 5);
    for (y = 0; y < LT_GFX_HEIGHT; ++y) {
        for (x = 0; x < LT_GFX_WIDTH; ++x) {
            lt_gfx_put(&surface, (lt_gfx_i16)x, (lt_gfx_i16)y,
                       (lt_gfx_u8)((x + 3U*y) & 15U));
            CHECK(lt_gfx_get(&surface, (lt_gfx_i16)x, (lt_gfx_i16)y) ==
                  (int)((x + 3U*y) & 15U));
        }
    }
    CHECK(fb[0] == 0x01U); /* (0,0)=0, (1,0)=1 */
    CHECK(fb[8192] == 0x34U); /* first byte of bank 1: row 1 pixels 3,4 */
    CHECK(fb[160] == 0xCDU); /* row 4 starts after 160 bytes in bank 0 */
    CHECK(ref_offset(319U,199U) == 32575U);
    CHECK(lt_gfx_get(&surface, 320, 10) == -1);
    CHECK(lt_gfx_get(&surface, -1, 10) == -1);
    lt_gfx_put(&surface, 320, 10, 7U);
    lt_gfx_put(&surface, -1, 10, 7U);
    check_guards();

    /* Half-open clipping and empty rectangles. */
    lt_gfx_clear(&surface, 0U);
    lt_gfx_rect(&surface, -3, 198, 3, 204, 9U);
    for (y = 0; y < LT_GFX_HEIGHT; ++y)
        for (x = 0; x < LT_GFX_WIDTH; ++x)
            CHECK(lt_gfx_get(&surface, (lt_gfx_i16)x, (lt_gfx_i16)y) ==
                  ((x < 3U && y >= 198U) ? 9 : 0));
    lt_gfx_rect(&surface, 20, 20, 20, 30, 7U);
    CHECK(lt_gfx_get(&surface, 20, 20) == 0);

    /* Bresenham, rejection, clipping from signed-16 endpoints. */
    lt_gfx_clear(&surface, 0U);
    lt_gfx_line(&surface, 0, 0, 4, 2, 6U);
    CHECK(lt_gfx_get(&surface, 0, 0) == 6);
    CHECK(lt_gfx_get(&surface, 1, 0) == 6);
    CHECK(lt_gfx_get(&surface, 2, 1) == 6);
    CHECK(lt_gfx_get(&surface, 4, 2) == 6);
    lt_gfx_line(&surface, -32768, 100, 32767, 100, 11U);
    for (x = 0; x < LT_GFX_WIDTH; ++x)
        CHECK(lt_gfx_get(&surface, (lt_gfx_i16)x, 100) == 11);
    lt_gfx_line(&surface, -32768, -32768, -30000, -30000, 12U);
    CHECK(lt_gfx_get(&surface, 0, 0) == 6);

    /* Midpoint circle, filled circle, and out-of-range-radius no-op. */
    lt_gfx_clear(&surface, 0U);
    lt_gfx_circle(&surface, 10, 10, 0, 13U, 0);
    CHECK(lt_gfx_get(&surface, 10, 10) == 13);
    lt_gfx_circle(&surface, 20, 20, 3, 4U, 0);
    CHECK(lt_gfx_get(&surface, 23, 20) == 4);
    CHECK(lt_gfx_get(&surface, 20, 23) == 4);
    CHECK(lt_gfx_get(&surface, 20, 20) == 0);
    lt_gfx_circle(&surface, 30, 30, 2, 8U, 1);
    CHECK(lt_gfx_get(&surface, 30, 30) == 8);
    CHECK(lt_gfx_get(&surface, 32, 30) == 8);
    lt_gfx_clear(&surface, 2U);
    lt_gfx_circle(&surface, 20, 20, 513, 9U, 1);
    for (i = 0; i < LT_GFX_BYTES; ++i) CHECK(fb[i] == 0x22U);

    /* Caller-defined 8x8 glyph, transparency and clipping. */
    lt_gfx_clear(&surface, 3U);
    lt_gfx_glyph8(&surface, 4, 5, rows, 14U, 1U, 1);
    CHECK(lt_gfx_get(&surface, 4, 5) == 14);
    CHECK(lt_gfx_get(&surface, 5, 5) == 3);
    lt_gfx_glyph8(&surface, 4, 5, rows, 14U, 1U, 0);
    CHECK(lt_gfx_get(&surface, 5, 5) == 1);
    lt_gfx_glyph8(&surface, 319, 199, rows, 7U, 0U, 0);
    CHECK(lt_gfx_get(&surface, 319, 199) == 7);

    /* Signed extremes must not wrap around into the visible surface. */
    lt_gfx_clear(&surface, 2U);
    lt_gfx_circle(&surface, 32767, 32767, 512, 5U, 1);
    lt_gfx_circle(&surface, -32768, -32768, 512, 5U, 0);
    lt_gfx_glyph8(&surface, 32767, 32767, rows, 5U, 0U, 0);
    lt_gfx_glyph8(&surface, -32768, -32768, rows, 5U, 0U, 0);
    lt_gfx_rect(&surface, 32767, 32767, -32768, -32768, 5U);
    for (i = 0; i < LT_GFX_BYTES; ++i) CHECK(fb[i] == 0x22U);
    /* Circle profile tie: radius one is four cardinal neighbours. */
    lt_gfx_circle(&surface, 50, 50, 1, 7U, 0);
    CHECK(lt_gfx_get(&surface, 51, 50) == 7);
    CHECK(lt_gfx_get(&surface, 51, 51) == 2);
    /* Row addresses follow the four-bank layout. */
    CHECK(lt_gfx_row(&surface, 0) == fb);
    CHECK(lt_gfx_row(&surface, 5) == fb + ref_offset(0U, 5U));
    CHECK(lt_gfx_row(&surface, 199) == fb + ref_offset(0U, 199U));
    CHECK(lt_gfx_row(&surface, 200) == 0 && lt_gfx_row(&surface, -1) == 0);
    /* Box copy between pages touches only the box. */
    {
        lt_gfx_surface other;
        lt_gfx_init(&other, second);
        lt_gfx_clear(&surface, 1U);
        lt_gfx_clear(&other, 9U);
        lt_gfx_copy_rows(&surface, &other, 3, 7, 10, 12);
        for (y = 0; y < 200U; ++y)
            for (x = 0; x < 160U; ++x)
                CHECK(fb[ref_offset(x * 2U, y)] ==
                      ((y >= 3U && y < 7U && x >= 10U && x < 12U)
                       ? 0x99U : 0x11U));
        lt_gfx_copy_rows(&surface, &other, -5, 300, -5, 400);
        for (i = 0; i < LT_GFX_BYTES; ++i)
            if (i % 8192U < 8000U) CHECK(fb[i] == 0x99U);
    }
    /* Packed blit with clipping on every edge. */
    for (i = 0; i < sizeof(packed); ++i) packed[i] = (lt_gfx_u8)i;
    lt_gfx_clear(&surface, 0U);
    lt_gfx_blit_packed(&surface, 150, 197, packed, 20U, 20, 4);
    CHECK(fb[ref_offset(300U, 197U)] == 0U && fb[ref_offset(318U, 197U)] == 9U);
    CHECK(fb[ref_offset(300U, 199U)] == 40U);
    lt_gfx_blit_packed(&surface, -2, -1, packed, 20U, 4, 2);
    CHECK(fb[ref_offset(0U, 0U)] == 22U && fb[ref_offset(2U, 0U)] == 23U);
    check_guards();
    /* Wrapped row scroll. */
    {
        lt_gfx_u8 row[8], out[8];
        for (i = 0; i < 8U; ++i) row[i] = (lt_gfx_u8)(i + 1U);
        lt_gfx_copy_wrapped(out, row, 8U, 3U);
        CHECK(out[0] == 4U && out[4] == 8U && out[5] == 1U && out[7] == 3U);
        lt_gfx_copy_wrapped(out, row, 8U, 8U);
        CHECK(out[0] == 1U && out[7] == 8U);
    }
    /* 16-bit extremes in the packed blit must clip, never wrap: write into
     * a guarded copy of the frame and check the guards on both sides. */
    {
        static const lt_gfx_i16 xs[] = { 32760, 32767, 159, 160, -32768,
                                         -32760, -21, -20 };
        static const lt_gfx_i16 ys[] = { 0, 199, 200, 32767, -32768, -1 };
        unsigned a, b;
        lt_gfx_surface g;
        for (i = 0; i < sizeof(blitbuf); ++i) blitbuf[i] = 0xC3U;
        lt_gfx_init(&g, blitbuf + BLIT_GUARD);
        for (a = 0; a < sizeof(xs) / sizeof(xs[0]); ++a)
            for (b = 0; b < sizeof(ys) / sizeof(ys[0]); ++b) {
                lt_gfx_blit_packed(&g, xs[a], ys[b], packed, 20U, 20, 1);
                lt_gfx_blit_packed(&g, xs[a], ys[b], packed, 20U, 32767, 4);
                lt_gfx_blit_packed(&g, xs[a], ys[b], packed, 20U, 20, 32767);
            }
        for (i = 0; i < BLIT_GUARD; ++i) {
            CHECK(blitbuf[i] == 0xC3U);
            CHECK(blitbuf[BLIT_GUARD + LT_GFX_BYTES + i] == 0xC3U);
        }
        /* edges: bx 159 width 20 writes exactly one byte; bx -20 none */
        for (i = 0; i < sizeof(blitbuf); ++i) blitbuf[i] = 0xC3U;
        lt_gfx_blit_packed(&g, 159, 0, packed, 20U, 20, 1);
        lt_gfx_blit_packed(&g, -20, 1, packed, 20U, 20, 1);
        for (i = 0; i < sizeof(blitbuf); ++i)
            CHECK(blitbuf[i] == (i == BLIT_GUARD + 159U ? 0U : 0xC3U));
    }
    lt_gfx_init(&surface, 0);
    CHECK(lt_gfx_row(&surface, 0) == 0);
    lt_gfx_line(&surface, -32768, 32767, 32767, -32768, 1U);
    check_guards();
    if (failures != 0) return 1;
    puts("graphics host tests passed");
    return 0;
}
