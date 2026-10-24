/* SPDX-License-Identifier: MIT */
/* Row access and block moves (lt_gfx.h), separate from lt_gfx.c so that
 * programs which do not use them do not link them. */
#include "lt_gfx.h"

static int lt_gfx_valid(const lt_gfx_surface *s)
{
    return s != 0 && s->pixels != 0;
}

static unsigned int lt_gfx_offset(unsigned int y)
{
    return (y & 3U) * 8192U + (y >> 2) * 160U;
}

/* Far block copy: the compiler's far memcpy on 16-bit Watcom (REP MOVS),
 * a plain loop elsewhere so host builds need no far support. */
#if defined(__WATCOMC__) && defined(__I86__)
#include <string.h>
#define LT_GFX_COPY(d, s, n) _fmemcpy((d), (s), (n))
#else
static void lt_gfx_copy(lt_gfx_u8 *d, const lt_gfx_u8 *s, unsigned n)
{
    while (n--) *d++ = *s++;
}
#define LT_GFX_COPY(d, s, n) lt_gfx_copy((d), (s), (n))
#endif

lt_gfx_u8 LT_GFX_FAR *lt_gfx_row(const lt_gfx_surface *s, lt_gfx_i16 y)
{
    if (!lt_gfx_valid(s) || y < 0 || y >= LT_GFX_HEIGHT) return 0;
    return s->pixels + lt_gfx_offset((unsigned int)y);
}

void lt_gfx_copy_rows(lt_gfx_surface *dst, const lt_gfx_surface *src,
                      lt_gfx_i16 y0, lt_gfx_i16 y1,
                      lt_gfx_i16 b0, lt_gfx_i16 b1)
{
    unsigned int off;
    if (!lt_gfx_valid(dst) || !lt_gfx_valid(src)) return;
    if (y0 < 0) y0 = 0;
    if (y1 > LT_GFX_HEIGHT) y1 = LT_GFX_HEIGHT;
    if (b0 < 0) b0 = 0;
    if (b1 > (lt_gfx_i16)LT_GFX_ROW_BYTES) b1 = (lt_gfx_i16)LT_GFX_ROW_BYTES;
    if (y0 >= y1 || b0 >= b1 || dst->pixels == src->pixels) return;
    for (; y0 < y1; ++y0) {
        off = lt_gfx_offset((unsigned int)y0) + (unsigned int)b0;
        LT_GFX_COPY(dst->pixels + off, src->pixels + off,
                    (unsigned int)(b1 - b0));
    }
}

void lt_gfx_blit_packed(lt_gfx_surface *s, lt_gfx_i16 bx, lt_gfx_i16 y,
                        const lt_gfx_u8 LT_GFX_FAR *src,
                        unsigned src_stride, lt_gfx_i16 width,
                        lt_gfx_i16 rows)
{
    lt_gfx_i16 skip = 0, r;
    if (!lt_gfx_valid(s) || src == 0 || width <= 0 || rows <= 0) return;
    if (bx < 0) { skip = (lt_gfx_i16)-bx; width -= skip; bx = 0; }
    if (bx + width > (lt_gfx_i16)LT_GFX_ROW_BYTES)
        width = (lt_gfx_i16)((lt_gfx_i16)LT_GFX_ROW_BYTES - bx);
    if (width <= 0) return;
    for (r = 0; r < rows; ++r) {
        lt_gfx_i16 ty = (lt_gfx_i16)(y + r);
        if (ty < 0) continue;
        if (ty >= LT_GFX_HEIGHT) break;
        LT_GFX_COPY(s->pixels + lt_gfx_offset((unsigned int)ty) +
                    (unsigned int)bx,
                    src + (unsigned long)r * src_stride + (unsigned int)skip,
                    (unsigned int)width);
    }
}

void lt_gfx_copy_wrapped(lt_gfx_u8 LT_GFX_FAR *dst,
                         const lt_gfx_u8 LT_GFX_FAR *src, unsigned width,
                         unsigned rotate)
{
    if (dst == 0 || src == 0 || width == 0U || width > LT_GFX_ROW_BYTES)
        return;
    rotate %= width;
    LT_GFX_COPY(dst, src + rotate, width - rotate);
    if (rotate) LT_GFX_COPY(dst + (width - rotate), src, rotate);
}
