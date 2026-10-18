/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_gfx.h"

static lt_gfx_u8 guarded[LT_GFX_BYTES + 2U];
static lt_gfx_surface surface;
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
    lt_gfx_init(&surface, 0);
    lt_gfx_line(&surface, -32768, 32767, 32767, -32768, 1U);
    check_guards();
    if (failures != 0) return 1;
    puts("graphics host tests passed");
    return 0;
}
