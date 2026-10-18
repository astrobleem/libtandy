/* SPDX-License-Identifier: MIT */
#ifndef LT_MEDIA_H
#define LT_MEDIA_H
#include "lt_surface.h"

/* Original software palette; RGB values do not program hardware registers. */
typedef struct { lts_u8 rgb[256][3]; } ltm_palette;
void ltm_palette_init(ltm_palette *palette);
lts_status ltm_palette_set(ltm_palette *palette, lts_u16 index,
                           lts_u8 red, lts_u8 green, lts_u8 blue);
/* Nearest default Tandy RGBI entry by squared RGB distance; ties choose the
 * lowest index. 256 output bytes. Call outside the frame loop. */
lts_status ltm_palette_map(const ltm_palette *palette, lts_u8 map[256]);

/* Pack INDEX8 into a caller-owned 32768-byte Tandy mode-9 layout. Source
 * bounds and screen bounds clip; source clip/key are ignored. Every map entry
 * must be 0..15. Odd-edge nibbles and bank padding are preserved. Source,
 * target, map and metadata must be disjoint; no framebuffer ownership is
 * conferred by this pure RAM operation. No scaling, alpha or allocation. */
lts_status ltm_pack(const lts_surface *source, const lts_rect *rect,
                     lts_storage *target, lts_i16 dx, lts_i16 dy,
                     const lts_u8 map[256]);
#endif
