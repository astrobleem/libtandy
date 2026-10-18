/* SPDX-License-Identifier: MIT */
/* Two-plane sprite proposal by the JAWS native owner, checkpoint f14de90.
 * Reviewed adaptation: see docs/SPRITE8.md for provenance and contract. */
#include "lt_gfx_sprite8.h"

static unsigned int sprite8_offset(unsigned int x, unsigned int y)
{
    return (y & 3U) * 8192U + (y >> 2) * 160U + (x >> 1);
}

int lt_gfx_sprite8_planes(lt_gfx_surface *surface, lt_gfx_i16 x,
                          lt_gfx_i16 y, const lt_gfx_u8 plane0[8],
                          const lt_gfx_u8 plane1[8],
                          const lt_gfx_u8 palette[4])
{
    int row;
    lt_gfx_u8 colors[4];

    if (surface == 0 || surface->pixels == 0 || plane0 == 0 ||
        plane1 == 0 || palette == 0) return 0;

    colors[0] = 0;
    colors[1] = (lt_gfx_u8)(palette[1] & 15U);
    colors[2] = (lt_gfx_u8)(palette[2] & 15U);
    colors[3] = (lt_gfx_u8)(palette[3] & 15U);

    for (row = 0; row < 8; ++row) {
        long py = (long)y + row;
        int col;
        if (py < 0L || py >= LT_GFX_HEIGHT) continue;

        col = 0;
        while (col < 8) {
            unsigned int bit = 0x80U >> col;
            unsigned int index0 =
                ((plane0[row] & bit) != 0U ? 1U : 0U) |
                ((plane1[row] & bit) != 0U ? 2U : 0U);
            long px = (long)x + col;

            if (px >= 0L && px < LT_GFX_WIDTH && index0 != 0U &&
                (px & 1L) == 0L && col + 1 < 8 && px + 1L < LT_GFX_WIDTH) {
                unsigned int next_bit = 0x80U >> (col + 1);
                unsigned int index1 =
                    ((plane0[row] & next_bit) != 0U ? 1U : 0U) |
                    ((plane1[row] & next_bit) != 0U ? 2U : 0U);

                if (index1 != 0U) {
                    unsigned int offset =
                        sprite8_offset((unsigned int)px, (unsigned int)py);
                    surface->pixels[offset] = (lt_gfx_u8)(
                        (colors[index0] << 4) | colors[index1]);
                    col += 2;
                    continue;
                }
            }

            if (px >= 0L && px < LT_GFX_WIDTH && index0 != 0U) {
                unsigned int offset =
                    sprite8_offset((unsigned int)px, (unsigned int)py);
                if ((px & 1L) == 0L)
                    surface->pixels[offset] = (lt_gfx_u8)(
                        (surface->pixels[offset] & 15U) |
                        (colors[index0] << 4));
                else
                    surface->pixels[offset] = (lt_gfx_u8)(
                        (surface->pixels[offset] & 240U) | colors[index0]);
            }
            ++col;
        }
    }
    return 1;
}
