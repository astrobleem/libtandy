/* SPDX-License-Identifier: MIT */
#ifndef LT_SURFACE_H
#define LT_SURFACE_H

/* Original, bounded INDEX8 software surfaces. C89, caller-owned storage.
 * No allocation, operating-system state, timer use, or SDL code is involved. */
#include <limits.h>
#if CHAR_BIT != 8 || UCHAR_MAX != 255 || USHRT_MAX != 65535U || \
    ULONG_MAX < 0xFFFFFFFFUL
#error libtandy surfaces require 8-bit char, 16-bit short, and 32-bit long
#endif
#if defined(__WATCOMC__)
#define LTS_FAR __far
#else
#define LTS_FAR
#endif

typedef unsigned char lts_u8;
typedef unsigned short lts_u16;
typedef signed short lts_i16;

typedef enum {
    LTS_OK = 0,
    LTS_INVALID = 1,
    LTS_UNSUPPORTED = 2,
    LTS_NOSPACE = 3,
    LTS_OVERLAP = 4
} lts_status;

#define LTS_INDEX8 1U
#define LTS_MAX_CAPACITY 65520U

typedef struct {
    /* Different storage objects used in one operation must name disjoint
     * memory ranges. Views that alias must share the same storage object. */
    lts_u8 LTS_FAR *pixels;
    lts_u16 capacity;
} lts_storage;

typedef struct {
    /* Rectangles address [x,x+w) by [y,y+h). */
    lts_i16 x, y;
    lts_u16 w, h;
} lts_rect;

typedef struct {
    lts_storage *storage;
    lts_u16 offset, width, height, pitch;
    lts_rect clip;
    lts_u8 format;
    lts_u8 key_enabled, key;
} lts_surface;

/* Failed initialization leaves the destination object unchanged. Capacity is
 * at most 65520 bytes, and every view must fit wholly inside it. */
lts_status lts_storage_init(lts_storage *storage,
                            lts_u8 LTS_FAR *pixels, lts_u16 capacity);
lts_status lts_surface_init(lts_surface *surface, lts_storage *storage,
                            lts_u16 offset, lts_u16 width, lts_u16 height,
                            lts_u16 pitch, lts_u8 format);
lts_status lts_clip(lts_surface *surface, const lts_rect *rect);
lts_status lts_key(lts_surface *surface, int enabled, lts_u8 key);
/* NULL fill rectangle means the whole destination surface, then its clip. */
lts_status lts_fill(lts_surface *surface, const lts_rect *rect, lts_u8 color);
/* NULL source rectangle means the whole source. Source clip is ignored;
 * source bounds and destination bounds/clip shift the copied area. Keying
 * compares raw source indices. Same-store, equal-pitch copies have snapshot
 * semantics; overlapping unequal-pitch bounds return LTS_OVERLAP unchanged. */
lts_status lts_blit(const lts_surface *source, const lts_rect *source_rect,
                    lts_surface *destination, lts_i16 dx, lts_i16 dy);

#endif
