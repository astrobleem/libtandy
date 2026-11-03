/* SPDX-License-Identifier: MIT */
#ifndef LT_GFX_TILE8_H
#define LT_GFX_TILE8_H
#include "lt_gfx.h"

/* Opaque 8x8 tile: 32 packed bytes, four per row, high nibble leftmost.
 * RGBI index zero is opaque. Coordinates accept all signed-16 values and
 * clip to the 320x200 surface. Returns 0 for null arguments, otherwise 1
 * (also for a wholly clipped tile). No heap, I/O, timer or hidden cache.
 * Source is FAR: all32 bytes must fit in one 64KiB pointer window
 * (16-bit Watcom offset <=0xFFE0). Odd source offsets are supported;
 * it and the surface descriptor must not overlap the destination buffer.
 * Aligned in-bounds tiles (even x, y divisible by four) use an optional
 * audited 8088 word-copy kernel. LT_GFX_TILE8_C_ONLY selects portable C. */
int lt_gfx_tile8(lt_gfx_surface *surface, lt_gfx_i16 x, lt_gfx_i16 y,
                 const lt_gfx_u8 LT_GFX_FAR *packed32);
#endif
