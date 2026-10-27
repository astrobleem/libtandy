/* SPDX-License-Identifier: MIT */
#ifndef LT_ENGINE_H
#define LT_ENGINE_H
#include <limits.h>
#if UCHAR_MAX != 255 || USHRT_MAX != 65535U || LONG_MAX < 2147483647L
#error lt_engine requires 8-bit bytes, 16-bit shorts and 32-bit longs
#endif
#if defined(__WATCOMC__)
#define LTE_FAR __far
#else
#define LTE_FAR
#endif

typedef struct lte_world lte_world;
typedef struct lte_object lte_object;
typedef void (*lte_callback)(lte_world *, lte_object *);
typedef struct {
    lte_callback awake, start, fixed_update, update, late_update;
} lte_behavior;
struct lte_object {
    const lte_behavior *behavior;
    void *user;
    unsigned char state; /* private: use add/remove, never change directly */
};
struct lte_world {
    lte_object *objects;
    unsigned capacity, period, budget;
    unsigned long debt, ticks;
    unsigned char busy, saturated;
};
/* Storage remains at this address; near data must fit the caller's memory
 * model. Init erases storage. period is in caller-defined time units, 1..65535;
 * budget is 1..255 fixed passes per frame. No allocation or timer/ISR. */
int lte_init(lte_world *, lte_object *, unsigned capacity,
             unsigned period, unsigned budget);
/* Pending objects activate at the NEXT fixed-tick boundary, including during
 * catch-up. Zero-step frames never activate objects. Slots are stable until
 * removal; do not retain a pointer after removal. Dead slots are reclaimed
 * at the next fixed-tick boundary. Behavior/user must outlive the object. */
lte_object *lte_add(lte_world *, const lte_behavior *, void *user);
int lte_remove(lte_world *, lte_object *);
/* Boundary order: reclaim, all Awake, all Start. Frame order: 0..budget
 * complete [boundary, FixedUpdate] passes, all Update, all LateUpdate.
 * Awake/Start run once, before the object's first FixedUpdate; no Update or
 * LateUpdate for pending objects. Removal suppresses remaining callbacks
 * immediately. Recursion is rejected. Debt is retained under slow rendering;
 * 32-bit saturation is sticky/observable, never an unbounded catch-up loop.
 * elapsed <= 0xffffffff; returns steps run, or -1 for invalid/reentrant call. */
int lte_frame(lte_world *, unsigned long elapsed);

/* World units are integers, independent of pixels/cells. Valid boxes have
 * x/y in -16384..16383 and w/h in 1..1024. Half-open: touching is not overlap. */
typedef struct { short x, y, w, h; } lte_box;
typedef struct {
    const unsigned char LTE_FAR *cells;
    unsigned width, height, tile_w, tile_h;
    unsigned char solid_mask; /* nonzero masked bits mean solid */
} lte_tilemap;
typedef struct { short x, y, w, h; } lte_camera;
#define LTE_HIT_X 1
#define LTE_HIT_Y 2
#define LTE_MOVE_INVALID 4
int lte_overlap(const lte_box *, const lte_box *);
/* Maps: nonempty, <=65535 cells, each axis <=16384 world units. Outside is
 * solid. Movement resolves X then Y in unit steps (no tunneling), <=256 units
 * per axis per call. Caller integrates velocity/gravity in FixedUpdate.
 * Invalid input leaves box unchanged, returns LTE_MOVE_INVALID. */
int lte_map_valid(const lte_tilemap *);
int lte_blocked(const lte_tilemap *, const lte_box *);
int lte_move_tiles(const lte_tilemap *, lte_box *, short dx, short dy);
/* Clamp requested top-left to world; a viewport larger than world pins to 0.
 * All sizes positive <=16384, world sizes <=16384. Invalid: unchanged/0. */
int lte_camera_set(lte_camera *, short x, short y, short w, short h,
                   short world_w, short world_h);
typedef void (*lte_tile_visit)(void *, unsigned char, short, short,
                                short, short);
/* Visits visible cells only, row-major, with camera-relative rectangle.
 * Partial tiles may extend past viewport: renderer clips. No allocation. */
int lte_visible_tiles(const lte_tilemap *, const lte_camera *,
                      lte_tile_visit, void *);
#endif
