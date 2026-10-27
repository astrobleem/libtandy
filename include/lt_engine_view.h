/* SPDX-License-Identifier: MIT */
#ifndef LT_ENGINE_VIEW_H
#define LT_ENGINE_VIEW_H
#include "lt_engine.h"
/* Native B800 layouts only. Binding does not set a BIOS mode. The caller
 * supplies the matching aperture or a RAM buffer of at least bytes length.
 * Text units are character cells. Mode 8/9 units are pixels. No mode claims
 * for INDEX8 RAM surfaces, VGA, CGA 4/6, or Tandy mode 0Ah. */
#define LTE_VIEW_TEXT40 1
#define LTE_VIEW_TEXT80 3
#define LTE_VIEW_TANDY160 8
#define LTE_VIEW_TANDY320 9
typedef struct {
    unsigned char LTE_FAR *pixels;
    unsigned width, height, stride, banks;
    unsigned char mode;
} lte_view;
int lte_view_bind(lte_view *, unsigned mode,
                   unsigned char LTE_FAR *pixels, unsigned bytes);
/* Half-open, clipped rectangle. In graphics ink low nibble is color;
 * in text ink is the complete attribute and glyph the character byte.
 * No mode setting, port I/O, page flipping, palette ownership or heap. */
void lte_view_rect(lte_view *, short x, short y, short w, short h,
                    unsigned char ink, unsigned char glyph);
#endif
