/* SPDX-License-Identifier: MIT */
#include "lt_engine.h"
#include <string.h>
#define FREE 0
#define PENDING 1
#define READY 2
#define ACTIVE 3
#define DEAD 4
#define MAX_TIME 0xffffffffUL

int lte_init(lte_world *w, lte_object *objects, unsigned n,
             unsigned period, unsigned budget)
{
    if (!w || !objects || !n || n > 65535U / sizeof(*objects) ||
        !period || !budget || budget > 255U) return 0;
#if UINT_MAX > 65535U
    if (period > 65535U) return 0;
#endif
    memset(w, 0, sizeof(*w));
    memset(objects, 0, n * sizeof(*objects));
    w->objects = objects; w->capacity = n;
    w->period = period; w->budget = budget;
    return 1;
}
lte_object *lte_add(lte_world *w, const lte_behavior *b, void *user)
{
    unsigned i;
    if (!w || !w->objects || !b) return 0;
    for (i = 0; i < w->capacity; ++i) {
        lte_object *o = &w->objects[i];
        if (o->state == FREE) {
            o->behavior = b; o->user = user; o->state = PENDING;
            return o;
        }
    }
    return 0;
}
int lte_remove(lte_world *w, lte_object *o)
{
    unsigned i;
    if (!w || !w->objects || !o) return 0;
    for (i = 0; i < w->capacity; ++i) {
        if (&w->objects[i] == o && o->state != FREE && o->state != DEAD) {
            o->state = DEAD; return 1;
        }
    }
    return 0;
}
static void callbacks(lte_world *w, unsigned phase)
{
    unsigned i;
    for (i = 0; i < w->capacity; ++i) {
        lte_object *o = &w->objects[i];
        lte_callback cb = 0;
        const lte_behavior *b = o->behavior;
        if ((phase < 2 && o->state != READY) ||
            (phase >= 2 && o->state != ACTIVE)) continue;
        if (phase == 0) cb = b->awake;
        else if (phase == 1) { o->state = ACTIVE; cb = b->start; }
        else if (phase == 2) cb = b->fixed_update;
        else if (phase == 3) cb = b->update;
        else cb = b->late_update;
        if (cb) cb(w, o);
    }
}
static void boundary(lte_world *w)
{
    unsigned i;
    for (i = 0; i < w->capacity; ++i) {
        if (w->objects[i].state == DEAD) {
            memset(&w->objects[i], 0, sizeof(w->objects[i]));
        } else if (w->objects[i].state == PENDING)
            w->objects[i].state = READY;
    }
    callbacks(w, 0); callbacks(w, 1);
}
int lte_frame(lte_world *w, unsigned long elapsed)
{
    unsigned steps = 0;
    if (!w || !w->objects || !w->period || !w->budget || w->busy) return -1;
#if ULONG_MAX > 0xffffffffUL
    if (elapsed > MAX_TIME) return -1;
#endif
    w->busy = 1;
    if (elapsed > MAX_TIME - w->debt) {
        w->debt = MAX_TIME; w->saturated = 1;
    } else w->debt += elapsed;
    while (w->debt >= w->period && steps < w->budget) {
        boundary(w);
        w->debt -= w->period;
        callbacks(w, 2);
        w->ticks = (w->ticks + 1UL) & MAX_TIME;
        ++steps;
    }
    callbacks(w, 3); callbacks(w, 4);
    w->busy = 0;
    return (int)steps;
}
static int box_valid(const lte_box *b)
{
    return b && b->x >= -16384 && b->x <= 16383 &&
        b->y >= -16384 && b->y <= 16383 &&
        b->w > 0 && b->w <= 1024 && b->h > 0 && b->h <= 1024;
}
int lte_overlap(const lte_box *a, const lte_box *b)
{
    return box_valid(a) && box_valid(b) &&
        (long)a->x < (long)b->x + b->w &&
        (long)b->x < (long)a->x + a->w &&
        (long)a->y < (long)b->y + b->h &&
        (long)b->y < (long)a->y + a->h;
}
int lte_map_valid(const lte_tilemap *m)
{
    return m && m->cells && m->width && m->height &&
        m->tile_w && m->tile_h && m->width <= 16384U &&
        m->height <= 16384U && m->tile_w <= 16384U &&
        m->tile_h <= 16384U &&
        (unsigned long)m->width * m->height <= 65535UL &&
        (unsigned long)m->width * m->tile_w <= 16384UL &&
        (unsigned long)m->height * m->tile_h <= 16384UL;
}
static int blocked(const lte_tilemap *m, const lte_box *b)
{
    unsigned x, y, x0, y0, x1, y1;
    if (b->x < 0 || b->y < 0 ||
        (long)b->x + b->w > (long)m->width * (long)m->tile_w ||
        (long)b->y + b->h > (long)m->height * (long)m->tile_h) return 1;
    x0 = (unsigned)b->x / m->tile_w;
    y0 = (unsigned)b->y / m->tile_h;
    x1 = (unsigned)(b->x + b->w - 1) / m->tile_w;
    y1 = (unsigned)(b->y + b->h - 1) / m->tile_h;
    for (y = y0; y <= y1; ++y)
        for (x = x0; x <= x1; ++x)
            if (m->cells[y * m->width + x] & m->solid_mask) return 1;
    return 0;
}
int lte_blocked(const lte_tilemap *m, const lte_box *b)
{
    if (!lte_map_valid(m) || !box_valid(b)) return 1;
    return blocked(m, b);
}
int lte_move_tiles(const lte_tilemap *m, lte_box *b, short dx, short dy)
{
    lte_box next, edge;
    int axis, hit = 0;
    if (!lte_map_valid(m) || !box_valid(b) || dx < -256 || dx > 256 ||
        dy < -256 || dy > 256 || blocked(m, b)) return LTE_MOVE_INVALID;
    for (axis = 0; axis < 2; ++axis) {
        int delta = axis ? dy : dx;
        int direction = delta < 0 ? -1 : 1;
        unsigned count = (unsigned)(delta < 0 ? -delta : delta);
        while (count--) {
            next = *b;
            if (axis) next.y = (short)(next.y + direction);
            else next.x = (short)(next.x + direction);
            edge = next;
            if (axis) {
                if (direction > 0) edge.y = (short)(edge.y + edge.h - 1);
                edge.h = 1;
            } else {
                if (direction > 0) edge.x = (short)(edge.x + edge.w - 1);
                edge.w = 1;
            }
            if (blocked(m, &edge)) { hit |= axis ? LTE_HIT_Y : LTE_HIT_X; break; }
            *b = next;
        }
    }
    return hit;
}
int lte_camera_set(lte_camera *c, short x, short y, short w, short h,
                   short world_w, short world_h)
{
    short max_x, max_y;
    if (!c || w <= 0 || h <= 0 || w > 16384 || h > 16384 ||
        world_w <= 0 || world_h <= 0 || world_w > 16384 || world_h > 16384)
        return 0;
    max_x = world_w > w ? (short)(world_w - w) : 0;
    max_y = world_h > h ? (short)(world_h - h) : 0;
    c->x = x < 0 ? 0 : (x > max_x ? max_x : x);
    c->y = y < 0 ? 0 : (y > max_y ? max_y : y);
    c->w = w; c->h = h;
    return 1;
}
int lte_visible_tiles(const lte_tilemap *m, const lte_camera *c,
                      lte_tile_visit visit, void *user)
{
    unsigned x0, y0, x1, y1, x, y;
    if (!lte_map_valid(m) || !c || !visit || c->x < 0 || c->y < 0 ||
        c->w <= 0 || c->h <= 0 || c->w > 16384 || c->h > 16384) return 0;
    x0 = (unsigned)c->x / m->tile_w;
    y0 = (unsigned)c->y / m->tile_h;
    x1 = (unsigned)(((long)c->x + c->w - 1) / m->tile_w);
    y1 = (unsigned)(((long)c->y + c->h - 1) / m->tile_h);
    if (x1 >= m->width) x1 = m->width - 1;
    if (y1 >= m->height) y1 = m->height - 1;
    for (y = y0; y <= y1; ++y)
        for (x = x0; x <= x1; ++x)
            visit(user, m->cells[y * m->width + x],
                  (short)((long)x * (long)m->tile_w - c->x),
                  (short)((long)y * (long)m->tile_h - c->y),
                  (short)m->tile_w, (short)m->tile_h);
    return 1;
}
