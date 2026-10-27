/* SPDX-License-Identifier: MIT */
#include "lt_engine_view.h"
#include <string.h>
int lte_view_bind(lte_view *v, unsigned mode,
                   unsigned char LTE_FAR *pixels, unsigned bytes)
{
    lte_view next;
    unsigned need;
    if (!v || !pixels) return 0;
    next.pixels = pixels; next.mode = (unsigned char)mode;
    next.height = 200; next.banks = 0;
    if (mode == LTE_VIEW_TEXT40 || mode == LTE_VIEW_TEXT80) {
        next.width = mode == LTE_VIEW_TEXT40 ? 40 : 80;
        next.height = 25; next.stride = next.width * 2;
        need = next.stride * next.height;
    } else if (mode == LTE_VIEW_TANDY160 || mode == LTE_VIEW_TANDY320) {
        next.width = mode == LTE_VIEW_TANDY160 ? 160 : 320;
        next.banks = mode == LTE_VIEW_TANDY160 ? 2 : 4;
        next.stride = next.width / 2;
        need = next.banks * 8192U;
    } else return 0;
    if (bytes < need) return 0;
    *v = next;
    return 1;
}
void lte_view_rect(lte_view *v, short x, short y, short w, short h,
                    unsigned char ink, unsigned char glyph)
{
    long right = (long)x + w, bottom = (long)y + h;
    unsigned row, col, x0, y0, x1, y1, offset;
    if (!v || !v->pixels || w <= 0 || h <= 0 || right <= 0 || bottom <= 0 ||
        x >= (short)v->width || y >= (short)v->height) return;
    x0 = x < 0 ? 0 : (unsigned)x;
    y0 = y < 0 ? 0 : (unsigned)y;
    x1 = right > (long)v->width ? v->width : (unsigned)right;
    y1 = bottom > (long)v->height ? v->height : (unsigned)bottom;
    for (row = y0; row < y1; ++row) {
        if (!v->banks) {
            offset = row * v->stride + x0 * 2;
            for (col = x0; col < x1; ++col) {
                v->pixels[offset++] = glyph; v->pixels[offset++] = ink;
            }
        } else {
            unsigned char LTE_FAR *p;
            unsigned end;
            if (v->banks == 2)
                offset = (row & 1U) * 8192U + (row >> 1) * v->stride;
            else
                offset = (row & 3U) * 8192U + (row >> 2) * v->stride;
            p = v->pixels + offset;
            ink &= 15;
            col = x0;
            if (col & 1U) {
                p[col / 2] = (unsigned char)((p[col / 2] & 240U) | ink);
                ++col;
            }
            end = x1 & ~1U;
            if (col < end) {
#if defined(__WATCOMC__) && defined(__I86__)
                _fmemset(p + col / 2, ink * 17U, (end - col) / 2);
#else
                memset(p + col / 2, ink * 17U, (end - col) / 2);
#endif
                col = end;
            }
            if (col < x1)
                p[col / 2] = (unsigned char)((p[col / 2] & 15U) | (ink << 4));
        }
    }
}
