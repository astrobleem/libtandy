/* SPDX-License-Identifier: MIT */
#include "lt_surface.h"

#if defined(__WATCOMC__)
#include <dos.h>
#endif

static lts_status lts_storage_check(const lts_storage *storage)
{
    if (storage == 0 || storage->pixels == 0 || storage->capacity == 0U)
        return LTS_INVALID;
    if (storage->capacity > LTS_MAX_CAPACITY) return LTS_NOSPACE;
#if defined(__WATCOMC__)
    if ((unsigned long)FP_OFF(storage->pixels) +
        (unsigned long)storage->capacity > 65536UL) return LTS_NOSPACE;
#endif
    return LTS_OK;
}

static lts_status lts_surface_check(const lts_surface *s)
{
    lts_status status;
    unsigned long end;
    if (s == 0) return LTS_INVALID;
    status = lts_storage_check(s->storage);
    if (status != LTS_OK) return status;
    if (s->width == 0U || s->height == 0U ||
        s->width > 32767U || s->height > 32767U || s->pitch < s->width)
        return LTS_INVALID;
    end = (unsigned long)s->offset + (unsigned long)s->pitch *
          (unsigned long)s->height;
    if (end > (unsigned long)s->storage->capacity) return LTS_NOSPACE;
    if (s->clip.x < 0 || s->clip.y < 0 ||
        (unsigned long)s->clip.x > (unsigned long)s->width ||
        (unsigned long)s->clip.y > (unsigned long)s->height ||
        (unsigned long)s->clip.x + (unsigned long)s->clip.w >
            (unsigned long)s->width ||
        (unsigned long)s->clip.y + (unsigned long)s->clip.h >
            (unsigned long)s->height) return LTS_INVALID;
    if (s->format != LTS_INDEX8) return LTS_UNSUPPORTED;
    return LTS_OK;
}

static long lts_max(long a, long b) { return a > b ? a : b; }
static long lts_min(long a, long b) { return a < b ? a : b; }

lts_status lts_storage_init(lts_storage *storage,
                            lts_u8 LTS_FAR *pixels, lts_u16 capacity)
{
    lts_storage next;
    if (storage == 0 || pixels == 0 || capacity == 0U) return LTS_INVALID;
    if (capacity > LTS_MAX_CAPACITY) return LTS_NOSPACE;
#if defined(__WATCOMC__)
    if ((unsigned long)FP_OFF(pixels) + (unsigned long)capacity > 65536UL)
        return LTS_NOSPACE;
#endif
    next.pixels = pixels;
    next.capacity = capacity;
    *storage = next;
    return LTS_OK;
}

lts_status lts_surface_init(lts_surface *surface, lts_storage *storage,
                            lts_u16 offset, lts_u16 width, lts_u16 height,
                            lts_u16 pitch, lts_u8 format)
{
    lts_surface next;
    unsigned long end;
    lts_status status;
    if (surface == 0) return LTS_INVALID;
    status = lts_storage_check(storage);
    if (status != LTS_OK) return status;
    if (format != LTS_INDEX8) return LTS_UNSUPPORTED;
    if (width == 0U || height == 0U || width > 32767U || height > 32767U ||
        pitch < width) return LTS_INVALID;
    end = (unsigned long)offset + (unsigned long)pitch * (unsigned long)height;
    if (end > (unsigned long)storage->capacity) return LTS_NOSPACE;
    next.storage = storage;
    next.offset = offset;
    next.width = width;
    next.height = height;
    next.pitch = pitch;
    next.clip.x = 0;
    next.clip.y = 0;
    next.clip.w = width;
    next.clip.h = height;
    next.format = LTS_INDEX8;
    next.key_enabled = 0;
    next.key = 0;
    *surface = next;
    return LTS_OK;
}

lts_status lts_clip(lts_surface *surface, const lts_rect *rect)
{
    lts_rect next;
    long x0, y0, x1, y1;
    {
        lts_status status = lts_surface_check(surface);
        if (status != LTS_OK) return status;
    }
    if (rect == 0) {
        next.x = 0; next.y = 0;
        next.w = surface->width; next.h = surface->height;
        surface->clip = next;
        return LTS_OK;
    }
    x0 = lts_max(0L, rect->x);
    y0 = lts_max(0L, rect->y);
    x1 = lts_min((long)surface->width, (long)rect->x + rect->w);
    y1 = lts_min((long)surface->height, (long)rect->y + rect->h);
    if (x0 > surface->width) x0 = surface->width;
    if (y0 > surface->height) y0 = surface->height;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    next.x = (lts_i16)x0; next.y = (lts_i16)y0;
    next.w = (lts_u16)(x1 - x0); next.h = (lts_u16)(y1 - y0);
    surface->clip = next;
    return LTS_OK;
}

lts_status lts_key(lts_surface *surface, int enabled, lts_u8 key)
{
    lts_status status = lts_surface_check(surface);
    if (status != LTS_OK) return status;
    surface->key_enabled = enabled != 0;
    surface->key = key;
    return LTS_OK;
}

static void lts_bounds(const lts_surface *surface, const lts_rect *rect,
                       long *x, long *y, long *w, long *h)
{
    long x0, y0, x1, y1;
    if (rect == 0) {
        x0 = 0; y0 = 0; x1 = surface->width; y1 = surface->height;
    } else {
        x0 = rect->x; y0 = rect->y;
        x1 = x0 + rect->w; y1 = y0 + rect->h;
    }
    x0 = lts_max(x0, surface->clip.x);
    y0 = lts_max(y0, surface->clip.y);
    x1 = lts_min(x1, (long)surface->clip.x + surface->clip.w);
    y1 = lts_min(y1, (long)surface->clip.y + surface->clip.h);
    x0 = lts_max(x0, 0L); y0 = lts_max(y0, 0L);
    x1 = lts_min(x1, surface->width); y1 = lts_min(y1, surface->height);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    *x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
}

lts_status lts_fill(lts_surface *surface, const lts_rect *rect, lts_u8 color)
{
    long x, y, w, h, row;
    long yy, xx;
    lts_status status = lts_surface_check(surface);
    if (status != LTS_OK) return status;
    lts_bounds(surface, rect, &x, &y, &w, &h);
    for (yy = 0; yy < h; ++yy) {
        row = (long)surface->offset + (y + yy) * surface->pitch + x;
        for (xx = 0; xx < w; ++xx)
            surface->storage->pixels[row + xx] = color;
    }
    return LTS_OK;
}

lts_status lts_blit(const lts_surface *source, const lts_rect *source_rect,
                    lts_surface *destination, lts_i16 dx, lts_i16 dy)
{
    long sx, sy, tx, ty, w, h, amount;
    long src_first, src_last, dst_first, dst_last;
    long row_start, row_end, row_step, col_start, col_end, col_step;
    int backwards, same_storage;
    lts_status status = lts_surface_check(source);
    if (status != LTS_OK) return status;
    status = lts_surface_check(destination);
    if (status != LTS_OK) return status;

    if (source_rect == 0) {
        sx = 0; sy = 0; w = source->width; h = source->height;
    } else {
        sx = source_rect->x; sy = source_rect->y;
        w = source_rect->w; h = source_rect->h;
    }
    tx = dx; ty = dy;
    if (sx < 0) { amount = -sx; sx = 0; tx += amount; w -= amount; }
    if (sy < 0) { amount = -sy; sy = 0; ty += amount; h -= amount; }
    if (sx + w > source->width) w = (long)source->width - sx;
    if (sy + h > source->height) h = (long)source->height - sy;

    if (tx < destination->clip.x) {
        amount = (long)destination->clip.x - tx;
        tx += amount; sx += amount; w -= amount;
    }
    if (ty < destination->clip.y) {
        amount = (long)destination->clip.y - ty;
        ty += amount; sy += amount; h -= amount;
    }
    if (tx + w > (long)destination->clip.x + (long)destination->clip.w)
        w = (long)destination->clip.x + (long)destination->clip.w - tx;
    if (ty + h > (long)destination->clip.y + (long)destination->clip.h)
        h = (long)destination->clip.y + (long)destination->clip.h - ty;
    if (tx < 0) { amount = -tx; tx += amount; sx += amount; w -= amount; }
    if (ty < 0) { amount = -ty; ty += amount; sy += amount; h -= amount; }
    if (tx + w > destination->width) w = (long)destination->width - tx;
    if (ty + h > destination->height) h = (long)destination->height - ty;
    if (w <= 0 || h <= 0) return LTS_OK;

    src_first = (long)source->offset + sy * source->pitch + sx;
    src_last = (long)source->offset + (sy + h - 1L) * source->pitch + sx + w;
    dst_first = (long)destination->offset + ty * destination->pitch + tx;
    dst_last = (long)destination->offset + (ty + h - 1L) * destination->pitch + tx + w;
    same_storage = source->storage == destination->storage;
    if (same_storage && source->pitch != destination->pitch &&
        src_first < dst_last && dst_first < src_last) return LTS_OVERLAP;
    backwards = same_storage && dst_first > src_first;

    if (backwards) {
        row_start = h - 1L; row_end = -1L; row_step = -1L;
        col_start = w - 1L; col_end = -1L; col_step = -1L;
    } else {
        row_start = 0; row_end = h; row_step = 1;
        col_start = 0; col_end = w; col_step = 1;
    }
    for (row_start = row_start; row_start != row_end; row_start += row_step) {
        long col;
        long src_row = (long)source->offset + (sy + row_start) * source->pitch + sx;
        long dst_row = (long)destination->offset +
                       (ty + row_start) * destination->pitch + tx;
        for (col = col_start; col != col_end; col += col_step) {
            long so = src_row + col;
            long doff = dst_row + col;
            lts_u8 pixel = source->storage->pixels[so];
            if (!source->key_enabled || pixel != source->key)
                destination->storage->pixels[doff] = pixel;
        }
    }
    return LTS_OK;
}
