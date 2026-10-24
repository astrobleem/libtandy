/* SPDX-License-Identifier: MIT */
/*
 * TICKDEMO: fixed-rate updates (lt_tick_dos.h) beside lt_audio. A 60 Hz
 * IRQ0 callback moves a dot one pixel per update and services a short beep
 * once a second through lt_audio; the foreground only draws. Hold Space to
 * make drawing slow (a busy loop per frame): the dot keeps its speed and
 * the beeps their rhythm, because updates do not depend on drawing. Escape
 * quits; the BIOS clock was kept running throughout (see the printout).
 */
#include <stdio.h>
#include <dos.h>
#include "lt_audio.h"
#include "lt_dos_app.h"
#include "lt_keys_dos.h"
#include "lt_tick_dos.h"

static lta_handle audio;
static volatile unsigned long updates;
static volatile lt_gfx_i16 dot;

static void beep(lt_u8 op, lta_u32 value)
{
    lta_request r;
    lta_request_init(&r);
    r.op = op;
    r.channel = 0;
    r.mask = 1U;
    r.period = 254U;
    r.level = 6U;
    r.value = value;
    (void)lta_command(&audio, &r);
}

static void update(void *context)
{
    (void)context;
    ++updates;
    dot = (lt_gfx_i16)((dot + 1) % 320);
    if (updates % 60UL == 0UL) {
        beep(LTA_OP_TONE_START, 0);
        beep(LTA_OP_ARM_STOP, 6UL);             /* six updates long */
    }
    beep(LTA_OP_SERVICE, (lta_u32)updates);
}

int main(void)
{
    lt_dos_app app = LT_DOS_APP_INITIALIZER;
    ltkb_state keys;
    unsigned long bios0, bios1, frames = 0, n, done;
    unsigned long t0, t1, k0, k1;
    volatile unsigned spin;
    lt_gfx_i16 shown = -1;
    int ok;

    if (lt_dos_app_init(&app) != LT_DOS_APP_OK) {
        puts("TICKDEMO: no mode 9");
        return 1;
    }
    /* lt_dos_app claimed the channels for owner 1; hand them to lt_audio */
    (void)lt_stop(&app.chip, LT_DOS_APP_SOUND_OWNER, LT_DOS_APP_SOUND_MASK);
    if (lta_init(&audio, &app.chip, 2U) != LTA_OK) {
        lt_dos_app_shutdown(&app);
        return 1;
    }
    beep(LTA_OP_ACQUIRE, 0);
    ok = ltkb_open(0) == LTKB_OK;
    if (ok && lttk_open(LTTK_DIVISOR_60HZ, update, 0) != LTTK_OK) {
        (void)ltkb_close();
        ok = 0;
    }
    if (!ok) {
        beep(LTA_OP_RELEASE, 0);
        lt_dos_app_shutdown(&app);
        puts("TICKDEMO: keyboard or timer busy");
        return 1;
    }
    lt_gfx_clear(&app.screen, 0U);
    bios0 = *(unsigned long __far *)MK_FP(0x40, 0x6C);
    t0 = lttk_now();
    k0 = lttk_ticks();
    lttk_run(1);
    for (;;) {
        ltkb_poll(&keys);
        if (ltkb_released(&keys, 1U)) break;
        if (dot != shown) {
            if (shown >= 0)
                lt_gfx_rect(&app.screen, shown, 96, (lt_gfx_i16)(shown + 1),
                            104, 0U);
            shown = dot;
            lt_gfx_rect(&app.screen, shown, 96, (lt_gfx_i16)(shown + 1),
                        104, 15U);
            ++frames;
        }
        if (ltkb_held(&keys, 0x39U))                    /* slow drawing */
            for (spin = 0; spin < 20000U; ++spin) ;
    }
    lttk_run(0);
    t1 = lttk_now();
    k1 = lttk_ticks();
    n = updates;
    bios1 = *(unsigned long __far *)MK_FP(0x40, 0x6C);
    lttk_defer(1);
    beep(LTA_OP_RELEASE, 0);
    lttk_defer(0);
    done = lttk_backlog_max();
    ok = lttk_close() == LTTK_OK;
    ok = (ltkb_close() == LTKB_OK) && ok;
    lt_dos_app_shutdown(&app);
    printf("TICKDEMO updates=%lu ticks=%lu over %lu PIT cycles (%.4f Hz) "
           "BIOS ticks=%lu (%.3f updates/BIOS tick, expected 3.296) "
           "frames=%lu backlog max=%lu restored=%d\n", n, k1 - k0, t1 - t0,
           t1 != t0 ? (double)(k1 - k0) * 1193182.0 / (double)(t1 - t0)
                    : 0.0,
           bios1 - bios0,
           bios1 != bios0 ? (double)n / (double)(bios1 - bios0) : 0.0,
           frames, done, ok);
    return 0;
}
