/* SPDX-License-Identifier: MIT */
#ifndef LT_GFX_SPRITE8_H
#define LT_GFX_SPRITE8_H

#include "lt_gfx.h"

/* Draw an 8x8 two-bitplane sprite.
 *
 * Each source row is MSB-first. Source index zero is transparent; palette[0] is ignored. palette[1..3]
 * map nonzero pixel indices to RGBI colors (low four bits). RGBI color zero
 * is valid and remains opaque when selected by a nonzero source index.
 *
 * Planes and palette are near, read-only caller data for the whole call. They
 * and the surface descriptor must not overlap the destination framebuffer.
 * Destination sprites may overlap: later nonzero pixels replace earlier
 * pixels; transparent pixels preserve the current destination. No implicit
 * erase, background restore, flip or arbitrary clip rectangle is performed.
 * The caller
 * supplies a complete LT_GFX_BYTES surface in one valid far-pointer window.
 * Coordinates clip to 320x200; signed 16-bit extremes are accepted.
 * Physical framebuffer ownership and mode are caller responsibilities.
 * Returns 1 for valid arguments, 0 when any pointer argument is null.
 */
int lt_gfx_sprite8_planes(lt_gfx_surface *surface, lt_gfx_i16 x,
                          lt_gfx_i16 y, const lt_gfx_u8 plane0[8],
                          const lt_gfx_u8 plane1[8],
                          const lt_gfx_u8 palette[4]);

#endif
