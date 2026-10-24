/* SPDX-License-Identifier: MIT */
#ifndef LT_GFX_H
#define LT_GFX_H

#include <limits.h>
#if UCHAR_MAX != 255 || USHRT_MAX != 65535U || LONG_MAX < 2147483647L
#error libtandy graphics requires 8-bit char, 16-bit short and at least 32-bit long
#endif

/* Portable drawing primitives for the Tandy 320x200 four-bit framebuffer.
 * C89; no allocation, BIOS calls, timers, or interrupt handling. */
#if defined(__WATCOMC__)
#define LT_GFX_FAR __far
#else
#define LT_GFX_FAR
#endif

typedef unsigned char lt_gfx_u8;
typedef signed short lt_gfx_i16;

#define LT_GFX_WIDTH 320
#define LT_GFX_HEIGHT 200
#define LT_GFX_BYTES 32768U

typedef struct {
    lt_gfx_u8 LT_GFX_FAR *pixels;
} lt_gfx_surface;

/* The caller supplies at least LT_GFX_BYTES bytes. A NULL pointer makes the
 * surface invalid; drawing operations then do nothing. Colors use low 4 bits. */
void lt_gfx_init(lt_gfx_surface *surface, lt_gfx_u8 LT_GFX_FAR *pixels);
void lt_gfx_clear(lt_gfx_surface *surface, lt_gfx_u8 color);
int lt_gfx_get(const lt_gfx_surface *surface, lt_gfx_i16 x, lt_gfx_i16 y);
void lt_gfx_put(lt_gfx_surface *surface, lt_gfx_i16 x, lt_gfx_i16 y,
                lt_gfx_u8 color);

/* Rectangles use half-open bounds [x0,x1) x [y0,y1), clipped to the screen. */
void lt_gfx_rect(lt_gfx_surface *surface, lt_gfx_i16 x0, lt_gfx_i16 y0,
                 lt_gfx_i16 x1, lt_gfx_i16 y1, lt_gfx_u8 color);
/* Lines use Cohen-Sutherland clipping to the closed pixel-coordinate screen
 * [0,319] x [0,199]. Intersection coordinates divide toward zero, then the
 * clipped segment is rasterized by integer Bresenham. */
void lt_gfx_line(lt_gfx_surface *surface, lt_gfx_i16 x0, lt_gfx_i16 y0,
                 lt_gfx_i16 x1, lt_gfx_i16 y1, lt_gfx_u8 color);
/* Midpoint circle. Radius 0..512 is accepted; larger radii are ignored before
 * mutation. A nonnegative midpoint decision decreases the major radius. Circle symmetry is clipped by pixel writes to the surface. */
void lt_gfx_circle(lt_gfx_surface *surface, lt_gfx_i16 cx, lt_gfx_i16 cy,
                   lt_gfx_i16 radius, lt_gfx_u8 color, int filled);
/* Eight row bytes, most significant bit is the left pixel. In transparent
 * mode zero bits preserve the destination; otherwise they use background. */
void lt_gfx_glyph8(lt_gfx_surface *surface, lt_gfx_i16 x, lt_gfx_i16 y,
                   const lt_gfx_u8 rows[8], lt_gfx_u8 foreground,
                   lt_gfx_u8 background, int transparent);

/* Row access and block moves for frame composition (src/lt_gfx_rows.c;
 * C89, no BIOS). Rows
 * are 160 bytes, two pixels per byte (left pixel in the high nibble), in
 * the mode-9 layout of four 8 KiB banks: row y starts at
 * (y & 3) * 8192 + (y >> 2) * 160. All byte columns are 0..159. */
#define LT_GFX_ROW_BYTES 160U

/* Far pointer to byte 0 of row y, or NULL for an invalid surface/row. */
lt_gfx_u8 LT_GFX_FAR *lt_gfx_row(const lt_gfx_surface *surface,
                                 lt_gfx_i16 y);
/* Copy byte columns [b0,b1) of rows [y0,y1) from src to dst, both in the
 * mode-9 layout (e.g. restore a dirty box from a background page). Bounds
 * are clipped; src and dst must not overlap unless they are equal. */
void lt_gfx_copy_rows(lt_gfx_surface *dst, const lt_gfx_surface *src,
                      lt_gfx_i16 y0, lt_gfx_i16 y1,
                      lt_gfx_i16 b0, lt_gfx_i16 b1);
/* Copy a packed linear image (rows of src_stride bytes, two pixels per
 * byte) into the surface: rows rows of width bytes to byte column bx, row
 * y. Clipped to the screen; source pixels are opaque. */
void lt_gfx_blit_packed(lt_gfx_surface *surface, lt_gfx_i16 bx,
                        lt_gfx_i16 y, const lt_gfx_u8 LT_GFX_FAR *src,
                        unsigned src_stride, lt_gfx_i16 width,
                        lt_gfx_i16 rows);
/* Wrapped horizontal scroll of one row: dst[i] = src[(i + rotate) % width]
 * for i in [0,width), width 1..160 bytes (a 2-pixel step per byte). dst and
 * src must not overlap. Used to scroll a repeating background band. */
void lt_gfx_copy_wrapped(lt_gfx_u8 LT_GFX_FAR *dst,
                         const lt_gfx_u8 LT_GFX_FAR *src, unsigned width,
                         unsigned rotate);

#endif
