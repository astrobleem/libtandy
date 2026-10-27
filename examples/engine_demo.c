/* SPDX-License-Identifier: MIT */
/* ENGDEMO [/low] [/auto] [/slow], TXTDEMO [/40] [/auto] [/slow].
 * Same world and physics in both executables; BIOS clock/keyboard only.
 * Arrows/A/D set horizontal velocity, Space jumps, Escape quits.
 * Automatic route stops after 300 fixed ticks and prints evidence in DOS. */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include "lt_engine_dos.h"
#include "lt_time_dos.h"
#define MAP_W 96
#define MAP_H 25
static unsigned char cells[MAP_W * MAP_H];
static lte_tilemap map = {cells, MAP_W, MAP_H, 8, 8, 128};
static lte_dos_video video = LTE_DOS_VIDEO_INITIALIZER;
static lte_world world;
static lte_object objects[2];
static lte_box player = {16,160,8,8};
static lte_camera camera;
static short vx = 2, vy;
static unsigned scale = 1, automatic, grounded, hit_x, hit_y, max_camera;
static unsigned awake_count, start_count, updates, late_count;
static void awake(lte_world *w, lte_object *o)
{ (void)w; (void)o; ++awake_count; }
static void start(lte_world *w, lte_object *o)
{ (void)w; (void)o; ++start_count; }
static void fixed(lte_world *w, lte_object *o)
{
    int hit;
    (void)o;
    if (automatic && grounded && w->ticks % 20UL == 0) vy = -6;
    if (vy < 4) ++vy;
    hit = lte_move_tiles(&map, &player, vx, vy);
    grounded = (hit & LTE_HIT_Y) && vy > 0;
    if (hit & LTE_HIT_X) { ++hit_x; if (automatic) vx = (short)-vx; }
    if (hit & LTE_HIT_Y) { ++hit_y; vy = 0; }
}
static void update(lte_world *w, lte_object *o)
{ (void)w; (void)o; ++updates; }
static void late(lte_world *w, lte_object *o)
{
    short x, width = (short)(video.view.width * scale);
    (void)w; (void)o; ++late_count;
    x = (short)(player.x + player.w / 2 - width / 2);
    /* Text camera is cell-aligned. Simulation coordinates remain unchanged. */
    if (x > 0) x = (short)(x / (short)scale * (short)scale);
    lte_camera_set(&camera, x, 0, width, 200, MAP_W * 8, MAP_H * 8);
    if ((unsigned)camera.x > max_camera) max_camera = (unsigned)camera.x;
}
static const lte_behavior behavior = {awake, start, fixed, update, late};
static void tile(void *user, unsigned char value, short x, short y,
                  short w, short h)
{
    lte_view *v = (lte_view *)user;
    unsigned char ink = value & 15;
    if (value == 1) return; /* common sky is one large span fill per frame */
    if (scale == 1) lte_view_rect(v, x, y, w, h, ink, 0);
    else lte_view_rect(v, (short)(x / 8), (short)(y / 8),
                      (short)(w / 8), (short)(h / 8),
                      (unsigned char)(ink << 4), ' ');
}
int main(int argc, char **argv)
{
    unsigned mode, slow = 0, x, y;
    int i, quit = 0, key, ok;
    unsigned long previous, now, elapsed;
#ifdef ENGINE_TEXT
    mode = LTE_VIEW_TEXT80; scale = 8;
#else
    mode = LTE_VIEW_TANDY320;
#endif
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "/auto")) automatic = 1;
        else if (!strcmp(argv[i], "/slow")) slow = 1;
#ifdef ENGINE_TEXT
        else if (!strcmp(argv[i], "/40")) mode = LTE_VIEW_TEXT40;
#else
        else if (!strcmp(argv[i], "/low")) mode = LTE_VIEW_TANDY160;
#endif
        else { puts("Use /auto /slow and /low (graphics) or /40 (text)"); return 1; }
    }
    for (y = 0; y < MAP_H; ++y)
        for (x = 0; x < MAP_W; ++x)
            cells[y * MAP_W + x] = (unsigned char)(
                y >= 23 ? 128U | (2U + (x % 3U)) :
                (x == 60 && y >= 16 ? 128U | 6U : 1U));
    /* Decorative stripes expose camera movement without external assets. */
    for (x = 0; x < MAP_W; x += 8) cells[10 * MAP_W + x] = 3;
    if (!lte_init(&world, objects, 2, 1, 4) ||
        !lte_add(&world, &behavior, 0)) return 1;
    if (!lte_dos_video_open(&video, mode)) {
        puts("Requested video mode unavailable (8/9 require Tandy hardware)");
        return 1;
    }
    previous = ltd_raw_ticks();
    while (!quit && (!automatic || world.ticks < 300UL)) {
        now = ltd_raw_ticks();
        elapsed = now >= previous ? now - previous : LTT_DAY_TICKS - previous + now;
        if (elapsed < (slow ? 3UL : 1UL)) continue;
        previous = now;
        if (automatic) {
            unsigned long remaining = 300UL - world.ticks;
            if (world.debt >= remaining) elapsed = 0;
            else if (elapsed > remaining - world.debt) elapsed = remaining - world.debt;
        }
        while (kbhit()) {
            key = getch();
            if (!key) key = 256 + getch();
            if (key == 27) quit = 1;
            if (key == 'a' || key == 256 + 75) vx = -2;
            if (key == 'd' || key == 256 + 77) vx = 2;
            if (key == ' ' && grounded) vy = -6;
        }
        lte_frame(&world, elapsed);
        lte_view_rect(&video.view, 0, 0, (short)video.view.width,
            (short)video.view.height, (unsigned char)(scale == 1 ? 1 : 0x10), ' ');
        lte_visible_tiles(&map, &camera, tile, &video.view);
        lte_view_rect(&video.view,
            (short)((player.x - camera.x) / (short)scale),
            (short)(player.y / (short)scale),
            (short)(player.w / (short)scale), (short)(player.h / (short)scale),
            (unsigned char)(scale == 1 ? 15 : 0x1f), '@');
    }
    lte_dos_video_close(&video);
    ok = !automatic || (awake_count == 1 && start_count == 1 &&
        updates == late_count && hit_x && hit_y && max_camera && !world.saturated);
    printf("ENGINE mode=%u ticks=%lu frames=%u debt=%lu awake/start=%u/%u "
           "hits=%u/%u camera=%u xy=%d,%d %s\n", mode, world.ticks, updates,
           world.debt, awake_count, start_count, hit_x, hit_y, max_camera,
           player.x, player.y, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
