/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_engine.h"
#define CHECK(v) do { if (!(v)) { printf("FAIL engine line %d\n", __LINE__); return 1; } } while (0)
static char logbuf[256];
static unsigned used, mutations, recursion_ok;
static lte_object *victim, *child;
static const lte_behavior behavior;
static void record(lte_object *o, char c)
{
    if (used < sizeof(logbuf) - 2) {
        logbuf[used++] = c; logbuf[used++] = *(char *)o->user;
    }
    logbuf[used] = 0;
}
static void awake(lte_world *w, lte_object *o)
{
    (void)w; record(o, 'A');
}
static void start(lte_world *w, lte_object *o)
{
    (void)w; record(o, 'S');
}
static void fixed(lte_world *w, lte_object *o)
{
    (void)w; record(o, 'F');
}
static void update(lte_world *w, lte_object *o)
{
    static char name = 'c';
    record(o, 'U');
    recursion_ok = lte_frame(w, 0) == -1;
    if (mutations && *(char *)o->user == 'a') {
        lte_remove(w, victim);
        child = lte_add(w, &behavior, &name);
        mutations = 0;
    }
}
static void late(lte_world *w, lte_object *o)
{
    (void)w; record(o, 'L');
}
static const lte_behavior behavior = {awake, start, fixed, update, late};
static unsigned fixed_count;
static void count_tick(lte_world *w, lte_object *o)
{
    (void)w; (void)o; ++fixed_count;
}
static const lte_behavior counter = {0, 0, count_tick, 0, 0};
static unsigned child_ticks, child_awakes, child_starts, chain_ticks;
static void chain_fixed(lte_world *w, lte_object *o)
{ (void)w; (void)o; ++chain_ticks; }
static const lte_behavior chain = {0, 0, chain_fixed, 0, 0};
static void child_awake(lte_world *w, lte_object *o)
{ (void)o; ++child_awakes; lte_add(w, &chain, 0); }
static void child_start(lte_world *w, lte_object *o)
{ (void)w; (void)o; ++child_starts; }
static void child_fixed(lte_world *w, lte_object *o)
{ (void)w; (void)o; ++child_ticks; }
static const lte_behavior spawned = {child_awake, child_start, child_fixed, 0, 0};
static void spawn_fixed(lte_world *w, lte_object *o)
{
    if (!w->ticks) { lte_add(w, &spawned, 0); lte_remove(w, o); }
}
static const lte_behavior spawner = {0, 0, spawn_fixed, 0, 0};
static unsigned visits;
static short first_x, first_y;
static void tile(void *u, unsigned char value, short x, short y, short w, short h)
{
    (void)u; (void)value; (void)w; (void)h;
    if (!visits) { first_x = x; first_y = y; }
    ++visits;
}
int main(void)
{
    lte_world world, fast, slow;
    lte_object objects[3], a[1], b[1], *first;
    char names[2] = {'a', 'b'};
    unsigned i, fast_count;
    unsigned char cells[16] = {0,0,1,0, 0,0,1,0, 1,1,1,1, 0,0,0,0};
    lte_tilemap map;
    lte_box box = {0,0,8,8}, other = {8,0,8,8}, saved;
    lte_camera camera, before;
    CHECK(!lte_init(&world, objects, 3, 0, 4));
    CHECK(lte_init(&world, objects, 3, 10, 2));
    first = lte_add(&world, &behavior, &names[0]);
    victim = lte_add(&world, &behavior, &names[1]);
    CHECK(first && victim);
    CHECK(lte_frame(&world, 20) == 2);
    CHECK(!strcmp(logbuf, "AaAbSaSbFaFbFaFbUaUbLaLb"));
    CHECK(recursion_ok);
    used = 0; logbuf[0] = 0; mutations = 1;
    CHECK(lte_frame(&world, 0) == 0 && child);
    CHECK(!strcmp(logbuf, "UaLa"));
    CHECK(!lte_add(&world, &behavior, names)); /* removed slot not reused yet */
    CHECK(!lte_remove(&world, victim));
    used = 0; logbuf[0] = 0;
    CHECK(lte_frame(&world, 10) == 1);
    CHECK(!strcmp(logbuf, "AcScFaFcUaUcLaLc"));
    CHECK(lte_add(&world, &behavior, names) == victim);
    CHECK(lte_remove(&world, victim)); /* remove pending: never Awake/Start */
    CHECK(lte_remove(&world, first));
    used = 0; logbuf[0] = 0;
    CHECK(lte_frame(&world, 0) == 0 && !strcmp(logbuf, "UcLc"));
    CHECK(lte_init(&fast, a, 1, 10, 3));
    CHECK(lte_init(&slow, b, 1, 10, 3));
    CHECK(lte_add(&fast, &counter, 0) && lte_add(&slow, &counter, 0));
    fixed_count = 0;
    for (i = 0; i < 30; ++i) CHECK(lte_frame(&fast, 10) == 1);
    fast_count = fixed_count; fixed_count = 0;
    CHECK(lte_frame(&slow, 300) == 3 && slow.debt == 270);
    for (i = 0; i < 9; ++i) CHECK(lte_frame(&slow, 0) == 3);
    CHECK(fixed_count == fast_count && slow.debt == 0 && slow.ticks == fast.ticks);
    CHECK(lte_frame(&slow, 0xffffffffUL) == 3);
    CHECK(lte_frame(&slow, 100) == 3 && slow.saturated);
    CHECK(lte_init(&world, objects, 3, 10, 3));
    CHECK(lte_add(&world, &spawner, 0));
    for (i = 0; i < 3; ++i) {
        CHECK(lte_frame(&world, 10) == 1);
        CHECK(lte_frame(&world, 0) == 0);
        CHECK(lte_frame(&world, 0) == 0);
    }
    CHECK(child_ticks == 2 && child_awakes == 1 && child_starts == 1);
    CHECK(chain_ticks == 1);
    child_ticks = child_awakes = child_starts = chain_ticks = 0;
    CHECK(lte_init(&world, objects, 3, 10, 3));
    CHECK(lte_add(&world, &spawner, 0));
    CHECK(lte_frame(&world, 30) == 3);
    CHECK(child_ticks == 2 && child_awakes == 1 && child_starts == 1);
    CHECK(chain_ticks == 1);
    map.cells = cells; map.width = map.height = 4;
    map.tile_w = map.tile_h = 8; map.solid_mask = 1;
    CHECK(lte_map_valid(&map));
    CHECK(!lte_overlap(&box, &other));
    other.x = 7; CHECK(lte_overlap(&box, &other));
    CHECK(lte_move_tiles(&map, &box, 32, 32) == (LTE_HIT_X | LTE_HIT_Y));
    CHECK(box.x == 8 && box.y == 8); /* cannot tunnel through 1-cell wall */
    CHECK(lte_move_tiles(&map, &box, -32, -32) == (LTE_HIT_X | LTE_HIT_Y));
    CHECK(box.x == 0 && box.y == 0);
    saved = box;
    CHECK(lte_move_tiles(&map, &box, 257, 0) == LTE_MOVE_INVALID);
    CHECK(!memcmp(&saved, &box, sizeof(box)));
    box.x = 16; saved = box;
    CHECK(lte_move_tiles(&map, &box, -1, 0) == LTE_MOVE_INVALID);
    CHECK(!memcmp(&saved, &box, sizeof(box)));
    box.x = -1; CHECK(lte_blocked(&map, &box));
    CHECK(lte_camera_set(&camera, -10, 99, 16, 16, 32, 32));
    CHECK(camera.x == 0 && camera.y == 16);
    CHECK(lte_camera_set(&camera, 99, 99, 64, 64, 32, 32));
    CHECK(camera.x == 0 && camera.y == 0);
    before = camera;
    CHECK(!lte_camera_set(&camera, 0, 0, 0, 10, 32, 32));
    CHECK(!memcmp(&camera, &before, sizeof(camera)));
    CHECK(lte_camera_set(&camera, 3, 7, 16, 16, 32, 32));
    CHECK(lte_visible_tiles(&map, &camera, tile, 0));
    CHECK(visits == 9 && first_x == -3 && first_y == -7);
    camera.x = 100; visits = 0;
    CHECK(lte_visible_tiles(&map, &camera, tile, 0) && visits == 0);
    map.width = 65535U; CHECK(!lte_map_valid(&map));
    printf("engine passed lifecycle, mutation, foreground debt, collision, camera; "
           "world=%u object=%u box=%u camera=%u map=%u behavior=%u bytes\n",
           (unsigned)sizeof(world), (unsigned)sizeof(objects[0]),
           (unsigned)sizeof(box), (unsigned)sizeof(camera),
           (unsigned)sizeof(map), (unsigned)sizeof(behavior));
    return 0;
}
