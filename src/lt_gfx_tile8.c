/* SPDX-License-Identifier: MIT */
/* Independently authored general packed-tile API; no application assets
 * or device emulation. */
#include "lt_gfx_tile8.h"

#if defined(__WATCOMC__) && defined(__I86__) && !defined(LT_GFX_TILE8_C_ONLY)
/* Actual Watcom allocation: source segment DX / offset SI; destination
 * offset AX / segment DI. Do not assume uniform ordering of register pairs.
 * DS/ES are preserved, DF follows the C ABI (clear). No source alignment
 * requirement on 8088. Destination starts in bank zero. */
static void lt_tile8_words(const lt_gfx_u8 LT_GFX_FAR *,
                           lt_gfx_u8 LT_GFX_FAR *);
#pragma aux lt_tile8_words = \
    "push ds" "push es" "mov ds,dx" "mov es,di" "mov di,ax" "cld" \
    "movsw" "movsw" "add di,8188" \
    "movsw" "movsw" "add di,8188" \
    "movsw" "movsw" "add di,8188" \
    "movsw" "movsw" "sub di,24420" \
    "movsw" "movsw" "add di,8188" \
    "movsw" "movsw" "add di,8188" \
    "movsw" "movsw" "add di,8188" \
    "movsw" "movsw" "pop es" "pop ds" \
    parm [dx si] [ax di] modify exact [si di];
#endif

int lt_gfx_tile8(lt_gfx_surface *s, lt_gfx_i16 x, lt_gfx_i16 y,
                 const lt_gfx_u8 LT_GFX_FAR *src)
{
    unsigned row, col, px, py, off;
    lt_gfx_u8 value, old;
    lt_gfx_u8 LT_GFX_FAR *dst;
    if (s == 0 || s->pixels == 0 || src == 0) return 0;
    if (x <= -8 || x >= 320 || y <= -8 || y >= 200) return 1;
    if (x >= 0 && x <= 312 && y >= 0 && y <= 192 &&
        !(x & 1) && !(y & 3)) {
        dst = s->pixels + ((unsigned)y >> 2) * 160U + ((unsigned)x >> 1);
#if defined(__WATCOMC__) && defined(__I86__) && !defined(LT_GFX_TILE8_C_ONLY)
        lt_tile8_words(src, dst);
#else
        for (row = 0; row < 8U; ++row) {
            off = (row & 3U) * 8192U + (row >> 2) * 160U;
            for (col = 0; col < 4U; ++col) dst[off + col] = src[row*4U + col];
        }
#endif
        return 1;
    }
    /* Rejection above bounds additions to -7..326 / -7..206: no signed
     * 16-bit overflow, even when callers passed extreme coordinates. */
    for (row = 0; row < 8U; ++row) {
        py = (unsigned)(y + (int)row);
        if (py >= 200U) continue;
        off = (py & 3U) * 8192U + (py >> 2) * 160U;
        for (col = 0; col < 8U; ++col) {
            px = (unsigned)(x + (int)col);
            if (px >= 320U) continue;
            value = src[row*4U + (col >> 1)];
            value = (lt_gfx_u8)((col & 1U) ? (value & 15U) : (value >> 4));
            dst = s->pixels + off + (px >> 1);
            old = *dst;
            *dst = (lt_gfx_u8)((px & 1U) ? ((old & 240U) | value) :
                                           ((old & 15U) | (value << 4)));
        }
    }
    return 1;
}
