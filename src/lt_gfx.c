/* SPDX-License-Identifier: MIT */
#include "lt_gfx.h"

static int lt_gfx_valid(const lt_gfx_surface *s)
{
    return s != 0 && s->pixels != 0;
}

static unsigned int lt_gfx_offset(unsigned int x, unsigned int y)
{
    return (y & 3U) * 8192U + (y >> 2) * 160U + (x >> 1);
}

static void lt_gfx_put_long(lt_gfx_surface *s, long x, long y, lt_gfx_u8 color)
{
    if (x < 0 || x >= LT_GFX_WIDTH || y < 0 || y >= LT_GFX_HEIGHT) return;
    lt_gfx_put(s, (lt_gfx_i16)x, (lt_gfx_i16)y, color);
}

static void lt_gfx_span(lt_gfx_surface *s, long x0, long y, long x1,
                        lt_gfx_u8 color);

static void lt_gfx_hline_long(lt_gfx_surface *s, long x0, long x1, long y,
                              lt_gfx_u8 color)
{
    /* Inclusive endpoints are converted to the common half-open span form. */
    lt_gfx_span(s, x0, y, x1 + 1, color);
}

void lt_gfx_init(lt_gfx_surface *s, lt_gfx_u8 LT_GFX_FAR *pixels)
{
    if (s != 0) s->pixels = pixels;
}

void lt_gfx_clear(lt_gfx_surface *s, lt_gfx_u8 color)
{
    unsigned int i;
    lt_gfx_u8 value;
    if (!lt_gfx_valid(s)) return;
    value = (lt_gfx_u8)((color & 15U) * 17U);
    for (i = 0; i < LT_GFX_BYTES; ++i) s->pixels[i] = value;
}

int lt_gfx_get(const lt_gfx_surface *s, lt_gfx_i16 x, lt_gfx_i16 y)
{
    unsigned int offset;
    if (!lt_gfx_valid(s) || x < 0 || x >= LT_GFX_WIDTH ||
        y < 0 || y >= LT_GFX_HEIGHT) return -1;
    offset = lt_gfx_offset((unsigned int)x, (unsigned int)y);
    if (((unsigned int)x & 1U) == 0U) return (s->pixels[offset] >> 4) & 15;
    return s->pixels[offset] & 15;
}

void lt_gfx_put(lt_gfx_surface *s, lt_gfx_i16 x, lt_gfx_i16 y,
                lt_gfx_u8 color)
{
    unsigned int offset;
    lt_gfx_u8 c;
    if (!lt_gfx_valid(s) || x < 0 || x >= LT_GFX_WIDTH ||
        y < 0 || y >= LT_GFX_HEIGHT) return;
    offset = lt_gfx_offset((unsigned int)x, (unsigned int)y);
    c = (lt_gfx_u8)(color & 15U);
    if (((unsigned int)x & 1U) == 0U)
        s->pixels[offset] = (lt_gfx_u8)((s->pixels[offset] & 15U) | (c << 4));
    else
        s->pixels[offset] = (lt_gfx_u8)((s->pixels[offset] & 240U) | c);
}

/* Fill [x0,x1) on one visible row using full packed bytes between the two
 * edge nibbles. This preserves every pixel outside the span. */
static void lt_gfx_span(lt_gfx_surface *s, long x0, long y, long x1,
                        lt_gfx_u8 color)
{
    unsigned int row, offset, stop;
    lt_gfx_u8 c, packed;
    if (!lt_gfx_valid(s) || y < 0 || y >= LT_GFX_HEIGHT ||
        x1 <= 0 || x0 >= LT_GFX_WIDTH) return;
    if (x0 < 0) x0 = 0;
    if (x1 > LT_GFX_WIDTH) x1 = LT_GFX_WIDTH;
    if (x0 >= x1) return;
    c = (lt_gfx_u8)(color & 15U);
    packed = (lt_gfx_u8)(c * 17U);
    row = (unsigned int)(y & 3L) * 8192U +
          (unsigned int)(y >> 2) * 160U;
    if (x0 & 1L) {
        offset = row + (unsigned int)(x0 >> 1);
        s->pixels[offset] = (lt_gfx_u8)((s->pixels[offset] & 240U) | c);
        ++x0;
    }
    if (x1 & 1L) {
        --x1;
        offset = row + (unsigned int)(x1 >> 1);
        s->pixels[offset] = (lt_gfx_u8)((s->pixels[offset] & 15U) | (c << 4));
    }
    offset = row + (unsigned int)(x0 >> 1);
    stop = row + (unsigned int)(x1 >> 1);
    while (offset < stop) s->pixels[offset++] = packed;
}

void lt_gfx_rect(lt_gfx_surface *s, lt_gfx_i16 x0, lt_gfx_i16 y0,
                 lt_gfx_i16 x1, lt_gfx_i16 y1, lt_gfx_u8 color)
{
    int y;
    if (!lt_gfx_valid(s)) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > LT_GFX_WIDTH) x1 = LT_GFX_WIDTH;
    if (y1 > LT_GFX_HEIGHT) y1 = LT_GFX_HEIGHT;
    if (x0 >= x1 || y0 >= y1) return;
    for (y = y0; y < y1; ++y)
        lt_gfx_span(s, x0, y, x1, color);
}

/* Cohen-Sutherland bits. Products in interpolation are 16x16 unsigned,
 * which fit in a 32-bit unsigned long even on 16-bit DOS compilers. */
#define LT_GFX_LEFT 1
#define LT_GFX_RIGHT 2
#define LT_GFX_TOP 4
#define LT_GFX_BOTTOM 8
static int lt_gfx_code(long x, long y)
{
    int c = 0;
    if (x < 0) c |= LT_GFX_LEFT;
    else if (x > 319) c |= LT_GFX_RIGHT;
    if (y < 0) c |= LT_GFX_TOP;
    else if (y > 199) c |= LT_GFX_BOTTOM;
    return c;
}

static long lt_gfx_muldiv(long a, long b, long d)
{
    unsigned long aa, bb, dd, product;
    int negative;
    if (d == 0) return 0;
    negative = ((a < 0) != (b < 0)) != (d < 0);
    aa = (unsigned long)(a < 0 ? -a : a);
    bb = (unsigned long)(b < 0 ? -b : b);
    dd = (unsigned long)(d < 0 ? -d : d);
    product = aa * bb;
    return negative ? -(long)(product / dd) : (long)(product / dd);
}

void lt_gfx_line(lt_gfx_surface *s, lt_gfx_i16 ix0, lt_gfx_i16 iy0,
                 lt_gfx_i16 ix1, lt_gfx_i16 iy1, lt_gfx_u8 color)
{
    long x0 = ix0, y0 = iy0, x1 = ix1, y1 = iy1;
    long dx, dy, sx, sy, err, e2;
    int c0, c1, accept = 0, iter;
    if (!lt_gfx_valid(s)) return;
    for (iter = 0; iter < 12; ++iter) {
        long x, y;
        c0 = lt_gfx_code(x0, y0);
        c1 = lt_gfx_code(x1, y1);
        if ((c0 | c1) == 0) { accept = 1; break; }
        if ((c0 & c1) != 0) return;
        if (c0 == 0) { int t = c0; c0 = c1; c1 = t; x = x0; y = y0; }
        else { x = x0; y = y0; }
        if (c0 & LT_GFX_TOP) {
            if (y1 == y0) return;
            x = x0 + lt_gfx_muldiv(x1 - x0, -y0, y1 - y0); y = 0;
        } else if (c0 & LT_GFX_BOTTOM) {
            if (y1 == y0) return;
            x = x0 + lt_gfx_muldiv(x1 - x0, 199 - y0, y1 - y0); y = 199;
        } else if (c0 & LT_GFX_RIGHT) {
            if (x1 == x0) return;
            y = y0 + lt_gfx_muldiv(y1 - y0, 319 - x0, x1 - x0); x = 319;
        } else {
            if (x1 == x0) return;
            y = y0 + lt_gfx_muldiv(y1 - y0, -x0, x1 - x0); x = 0;
        }
        if (c0 == lt_gfx_code(x0, y0)) { x0 = x; y0 = y; }
        else { x1 = x; y1 = y; }
    }
    if (!accept) return;
    dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    sx = x0 < x1 ? 1 : -1; sy = y0 < y1 ? 1 : -1;
    err = dx - dy;
    for (;;) {
        lt_gfx_put(s, (lt_gfx_i16)x0, (lt_gfx_i16)y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static void lt_gfx_circle_points(lt_gfx_surface *s, long cx, long cy,
                                 long x, long y, lt_gfx_u8 color)
{
    lt_gfx_put_long(s, cx+x, cy+y, color);
    lt_gfx_put_long(s, cx-x, cy+y, color);
    lt_gfx_put_long(s, cx+x, cy-y, color);
    lt_gfx_put_long(s, cx-x, cy-y, color);
    lt_gfx_put_long(s, cx+y, cy+x, color);
    lt_gfx_put_long(s, cx-y, cy+x, color);
    lt_gfx_put_long(s, cx+y, cy-x, color);
    lt_gfx_put_long(s, cx-y, cy-x, color);
}

void lt_gfx_circle(lt_gfx_surface *s, lt_gfx_i16 cx, lt_gfx_i16 cy,
                   lt_gfx_i16 radius, lt_gfx_u8 color, int filled)
{
    long x, y, decision;
    if (!lt_gfx_valid(s) || radius < 0 || radius > 512) return;
    if (radius == 0) { lt_gfx_put(s, cx, cy, color); return; }
    x = radius; y = 0; decision = 1 - x;
    while (x >= y) {
        if (!filled) lt_gfx_circle_points(s, cx, cy, x, y, color);
        else {
            lt_gfx_hline_long(s, cx-x, cx+x, cy+y, color);
            lt_gfx_hline_long(s, cx-x, cx+x, cy-y, color);
            lt_gfx_hline_long(s, cx-y, cx+y, cy+x, color);
            lt_gfx_hline_long(s, cx-y, cx+y, cy-x, color);
        }
        ++y;
        if (decision < 0) decision += 2 * y + 1;
        else { --x; decision += 2 * (y - x) + 1; }
    }
}

void lt_gfx_glyph8(lt_gfx_surface *s, lt_gfx_i16 x, lt_gfx_i16 y,
                   const lt_gfx_u8 rows[8], lt_gfx_u8 foreground,
                   lt_gfx_u8 background, int transparent)
{
    int row, col;
    lt_gfx_u8 fg, bg;
    if (!lt_gfx_valid(s) || rows == 0) return;
    fg = (lt_gfx_u8)(foreground & 15U);
    bg = (lt_gfx_u8)(background & 15U);
    for (row = 0; row < 8; ++row) {
        long py = (long)y + row;
        long first, end;
        unsigned int row_offset;
        const lt_gfx_u8 LT_GFX_FAR *source_row;
        if (py < 0 || py >= LT_GFX_HEIGHT) continue;
        first = x < 0 ? -(long)x : 0;
        end = (long)x + 8L > LT_GFX_WIDTH ? LT_GFX_WIDTH - (long)x : 8L;
        if (first >= end) continue;
        row_offset = (unsigned int)(py & 3L) * 8192U +
                     (unsigned int)(py >> 2) * 160U;
        source_row = (const lt_gfx_u8 LT_GFX_FAR *)(rows + row);
        if (transparent) {
            for (col = (int)first; (long)col < end; ++col) {
                lt_gfx_u8 bits = rows[row];
                int px;
                unsigned int offset;
                if ((bits & (0x80U >> col)) == 0U) continue;
                px = (int)x + col;
                offset = row_offset + ((unsigned int)px >> 1);
                if (((unsigned int)px & 1U) == 0U)
                    s->pixels[offset] = (lt_gfx_u8)(
                        (s->pixels[offset] & 15U) | (fg << 4));
                else
                    s->pixels[offset] = (lt_gfx_u8)(
                        (s->pixels[offset] & 240U) | fg);
            }
            continue;
        }
        col = (int)first;
        while ((long)col < end) {
            long px = (long)x + col;
            unsigned int offset = row_offset + (unsigned int)(px >> 1);
            int count = ((px & 1L) == 0L && (long)col + 1L < end) ? 2 : 1;
            int part;
            int dirty = 0;
            lt_gfx_u8 value = s->pixels[offset];
            for (part = 0; part < count; ++part) {
                int glyph_col = col + part;
                lt_gfx_u8 bits = rows[row]; /* per-pixel reads preserve aliasing */
                lt_gfx_u8 color;
                int selected;
                long screen_x = (long)x + glyph_col;
                selected = (bits & (0x80U >> glyph_col)) != 0U;
                if (selected) color = fg;
                else if (!transparent) color = bg;
                else continue;
                if ((screen_x & 1L) == 0L)
                    value = (lt_gfx_u8)((value & 15U) | (color << 4));
                else
                    value = (lt_gfx_u8)((value & 240U) | color);
                dirty = 1;
                /* Publish a changed source row byte before reading its next
                 * bit. Other source aliases are visible before the next row. */
                if (source_row == s->pixels + offset) {
                    s->pixels[offset] = value;
                    dirty = 0;
                }
            }
            if (dirty) s->pixels[offset] = value;
            col += count;
        }
    }
}
