/* SPDX-License-Identifier: MIT */
/* SMSBOOT.EXE plays once. TANDYLOG.EXE adds the supplied logo reveal.
 * /auto returns after playback; otherwise logo remains until a key.
 * Escape cancels. Exclusive foreground Tandy DOS; no install/startup hook.
 */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include "lt_sms_boot.h"
#include "lt_tick_dos.h"
#ifdef SMS_LOGO
#include <dos.h>
#include "lt_gfx.h"
#include "smsboot/logo.h"
#endif

extern void lt_dos_emit(void *context, lt_u8 byte);
static lt_u16 port = 0xC0U;
static lt_chip chip;
static lt_sms_boot effect;
static volatile unsigned finished, failed;
static void idle(void);
#pragma aux idle = "hlt" modify exact []

#ifdef SMS_LOGO
static lt_gfx_surface screen;
static unsigned saved_mode, saved_page, saved_cursor, saved_shape;
static unsigned band;

/* Tandy gate-array palette: separate address/data ports, no BIOS in IRQ.
 * No other video register writer runs while this example owns the display.
 */
static void palette(unsigned index, unsigned color)
{
    (void)inp(0x3DA);
    outp(0x3DA, 0x10U + index);
    outp(0x3DE, color);
}

static void video_open(void)
{
    union REGS r;
    unsigned i, y, x, pos = 0U, left = 0U, value = 0U;
    lt_gfx_u8 LT_GFX_FAR *row;
    memset(&r, 0, sizeof(r));
    r.h.ah = 15U; int86(0x10, &r, &r);
    saved_mode = r.h.al; saved_page = r.h.bh;
    r.h.ah = 3U; r.h.bh = (unsigned char)saved_page;
    int86(0x10, &r, &r);
    saved_cursor = r.x.dx; saved_shape = r.x.cx;
    r.x.ax = 9U; int86(0x10, &r, &r);
    for (i = 0; i < 16U; ++i) palette(i, 15U);
    lt_gfx_init(&screen, (lt_gfx_u8 __far *)MK_FP(0xB800, 0));
    lt_gfx_clear(&screen, 15U);
    /* Pre-draw the exact source silhouettes, all palette entries white.
     * RLE expands only once before the sound/timeline starts.
     */
    for (y = 0; y < LOGO_HEIGHT; ++y) {
        row = lt_gfx_row(&screen, (lt_gfx_i16)(LOGO_Y + y));
        for (x = 0; x < LOGO_WIDTH / 2U; ++x) {
            if (!left) {
                left = logo_runs[pos++]; value = logo_runs[pos++];
            }
            row[LOGO_X / 2U + x] = (lt_gfx_u8)value;
            --left;
        }
    }
}

static void video_close(void)
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.x.ax = saved_mode; int86(0x10, &r, &r);
    r.h.ah = 5U; r.h.al = (unsigned char)saved_page;
    int86(0x10, &r, &r);
    r.h.ah = 1U; r.x.cx = saved_shape; int86(0x10, &r, &r);
    r.h.ah = 2U; r.h.bh = (unsigned char)saved_page;
    r.x.dx = saved_cursor; int86(0x10, &r, &r);
}

static void reveal(unsigned tick)
{
    /* 13 blue bands, white untouched. Last band + red tagline change on
     * the same service frame as the source's final attack (sample 88291).
     */
    while (band < 13U && tick >= (unsigned)
           (((unsigned long)(band + 1U) * LTSB_FINAL_TICK + 12UL) / 13UL)) {
        ++band;
        palette(band, 1U);
    }
    if (tick == LTSB_FINAL_TICK) palette(14U, 4U);
}
#endif

static void update(void *context)
{
    (void)context;
    if (finished) return;
#ifdef SMS_LOGO
    /* Palette first, then PSG in this callback: no independent sleep clock.
     * Work on the final frame is three palette register writes + 10 PSG
     * bytes, not a framebuffer redraw or a foreground polling race.
     */
    reveal(effect.tick);
#endif
    if (!lt_sms_boot_tick(&effect)) { failed = 1U; finished = 1U; }
    if (!effect.active) finished = 1U;
}

int main(int argc, char **argv)
{
    int opened = 0, started = 0;
    int automatic = argc > 1 && strcmp(argv[1], "/auto") == 0;
#ifdef SMS_LOGO
    video_open();
#else
    (void)automatic;
    puts("SMS BIOS Sega Tune - optional PSG effect; Escape cancels.");
#endif
    lt_init(&chip, lt_dos_emit, &port);
    if (!lt_sms_boot_init(&effect, &chip, 1U)) failed = 1U;
    else if (lttk_open(LTSB_PIT_DIVISOR, update, 0) != LTTK_OK) failed = 1U;
    else {
        opened = 1;
        if (!lt_sms_boot_start(&effect)) failed = 1U;
        else {
            started = 1;
            /* Establish tick zero now, then run periodic ticks 1 onward. */
            update(0);
            lttk_run(1);
            while (!finished) {
                if (kbhit() && getch() == 27) break;
                idle();
            }
            lttk_run(0);
        }
    }
    if (started && !lt_sms_boot_stop(&effect)) failed = 1U;
    if (opened && lttk_close() != LTTK_OK) failed = 1U;
#ifdef SMS_LOGO
    if (finished && !failed && !automatic) getch();
    video_close();
#endif
    puts(failed ? "SMS boot: cleanup/playback failed" : "SMS boot: stopped, PSG muted");
    return failed ? 1 : 0;
}
