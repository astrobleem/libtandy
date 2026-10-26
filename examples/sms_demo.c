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
#include "smsboot/TRACE_RT.H"
#define LOGO_WIDTH 288U
#define LOGO_HEIGHT 136U
#define LOGO_X 16U
#define LOGO_Y 32U
#endif

extern void lt_dos_emit(void *context, lt_u8 byte);
static lt_u16 port = 0xC0U;
static lt_chip chip;
static lt_sms_boot effect;
static volatile unsigned finished, failed;
static unsigned want_stats, capturing, capture_count, capture_overflow;
static unsigned char capture_bytes[714U * 3U];
static unsigned long irq_late;

static void demo_emit(void *context, lt_u8 byte)
{
    lt_dos_emit(context, byte);
    if (want_stats && capturing) {
        if (capture_count < 714U) {
            unsigned at = capture_count * 3U;
            capture_bytes[at] = (unsigned char)effect.tick;
            capture_bytes[at + 1U] = (unsigned char)(effect.tick >> 8);
            capture_bytes[at + 2U] = byte;
            ++capture_count;
        } else capture_overflow = 1U;
    }
}
static void idle(void);
#pragma aux idle = "hlt" modify exact []

#ifdef SMS_LOGO
static lt_gfx_surface screen;
static unsigned saved_mode, saved_page, saved_cursor, saved_shape;
static volatile unsigned trace_phase, need_handoff, trace_step;
static volatile unsigned handoff_misses, settled_tick, tagline_tick;
static unsigned char palette_now[16];
static unsigned long handoff_clocks[3];
static unsigned long max_audio_delay;

/* Tandy gate-array palette: separate address/data ports, no BIOS in IRQ.
 * No other video register writer runs while this example owns the display.
 */
static void palette(unsigned index, unsigned color)
{
    if (palette_now[index] == color) return;
    palette_now[index] = (unsigned char)color;
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
    r.h.ah = 1U; r.x.cx = 0x2000U; int86(0x10, &r, &r);
    for (i = 0; i < 16U; ++i) palette(i, 15U);
    lt_gfx_init(&screen, (lt_gfx_u8 __far *)MK_FP(0xB800, 0));
    lt_gfx_clear(&screen, 15U);
    /* Initial source-derived mask; all active indices remain white. */
    for (y = 0; y < LOGO_HEIGHT; ++y) {
        row = lt_gfx_row(&screen, (lt_gfx_i16)(LOGO_Y + y));
        for (x = 0; x < LOGO_WIDTH / 2U; ++x) {
            if (!left) {
                left = trace_initial[pos++]; value = trace_initial[pos++];
            }
            row[LOGO_X / 2U + x] = (lt_gfx_u8)value;
            --left;
        }
    }
    palette(0U, 1U); /* Permanent completed strokes; unused in mask zero. */
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

/* Apply sparse byte patches with interrupts enabled. Every commit byte is
 * blue -> blue; every install byte is white -> white under the current
 * palette. No framebuffer transfer or wait happens in the audio callback.
 */
/* Tight 8088 transfer, still fully interruptible. DS:SI is near static
 * data; ES is saved/restored. The ISR preserves interrupted registers.
 * No CLI, timer work, or palette changes occur inside this loop. */
static void patch_bytes(const unsigned char *data, unsigned count);
#pragma aux patch_bytes = \
    "push es" \
    "mov ax,0b800h" \
    "mov es,ax" \
    "cld" \
    "test cx,cx" \
    "jz done" \
    "again:" \
    "lodsw" \
    "mov di,ax" \
    "lodsb" \
    "stosb" \
    "loop again" \
    "done:" \
    "pop es" \
    parm [si] [cx] modify exact [ax di si cx];

static void patch(unsigned pass)
{
    patch_bytes(trace_patches + trace_patch_offsets[pass],
                trace_patch_counts[pass]);
}

static void foreground_handoff(void)
{
    unsigned p, i;
    unsigned long begin;
    if (!need_handoff) return;
    p = trace_phase;
    if (p >= 3U) { failed = 1U; return; }
    begin = lttk_now();
    patch(p * 2U);                  /* Active blue indices -> permanent 0. */
    for (i = 1U; i <= 13U; ++i) palette(i, 15U);
    patch(p * 2U + 1U);            /* Install next mask under white colors. */
    handoff_clocks[p] = lttk_now() - begin;
    trace_phase = p + 1U;
    need_handoff = 0U;              /* Publish only after every byte is ready. */
}

static void reveal(unsigned tick)
{
    unsigned i, born, age, color, settled;
    if (tick == LTSB_FINAL_TICK) {
        palette(14U, 4U);
        tagline_tick = tick;
    }
    if (trace_step >= 52U || tick < trace_deadlines[trace_step]) return;
    if (need_handoff || trace_phase != trace_step / 13U) {
        ++handoff_misses;           /* Audio continues on time even on failure. */
        return;
    }
    settled = trace_step % 13U == 12U;
    for (i = 1U; i <= 13U; ++i) {
        born = trace_phase * 13U + i - 1U;
        color = 15U;
        if (born <= trace_step) {
            age = trace_step - born;
            color = settled ? 1U : age == 0U ? 3U : age == 1U ? 9U : 1U;
        }
        palette(i, color);
    }
    ++trace_step;
    if (trace_step == 52U) settled_tick = tick;
    else if (settled) need_handoff = 1U;
}

#endif

static void update(void *context)
{
    (void)context;
    if (finished) return;
#ifdef SMS_LOGO
    /* Palette before PSG in the same service callback. Sparse bitmap
     * transfers run only in foreground while this callback keeps sounding. */
    if (want_stats && effect.tick) {
        unsigned long delay = lttk_now() % LTSB_PIT_DIVISOR;
        if (delay > max_audio_delay) max_audio_delay = delay;
    }
    reveal(effect.tick);
#endif
    if (!lt_sms_boot_tick(&effect)) { failed = 1U; finished = 1U; }
    if (!effect.active) finished = 1U;
}

int main(int argc, char **argv)
{
    int opened = 0, started = 0;
    int automatic = 0;
    int arg;
    for (arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "/auto") == 0) automatic = 1;
        if (strcmp(argv[arg], "/stats") == 0) want_stats = 1U;
    }
#ifdef SMS_LOGO
    video_open();
#else
    (void)automatic;
    puts("SMS BIOS Sega Tune - optional PSG effect; Escape cancels.");
#endif
    lt_init(&chip, demo_emit, &port);
    if (!lt_sms_boot_init(&effect, &chip, 1U)) failed = 1U;
    else if (lttk_open(LTSB_PIT_DIVISOR, update, 0) != LTTK_OK) failed = 1U;
    else {
        opened = 1;
        if (!lt_sms_boot_start(&effect)) failed = 1U;
        else {
            started = 1;
            /* Establish tick zero now, then run periodic ticks 1 onward. */
            capturing = 1U;
            update(0);
            lttk_run(1);
            while (!finished) {
                if (kbhit() && getch() == 27) break;
#ifdef SMS_LOGO
                foreground_handoff();
#endif
                idle();
            }
            lttk_run(0);
            irq_late = lttk_late();
        }
    }
    if (started && !lt_sms_boot_stop(&effect)) failed = 1U;
    capturing = 0U;
    if (opened && lttk_close() != LTTK_OK) failed = 1U;
#ifdef SMS_LOGO
    if (finished && !failed && !automatic) getch();
    video_close();
    if (handoff_misses) failed = 1U;
    if (want_stats) {
        printf("TRACE settled=%u tagline=%u misses=%u steps=%u phases=%u\n",
               settled_tick, tagline_tick, handoff_misses, trace_step, trace_phase);
        printf("TRACE clocks=%lu,%lu,%lu budget_ticks=23 max_audio_delay=%lu\n",
               handoff_clocks[0], handoff_clocks[1], handoff_clocks[2], max_audio_delay);
    }
#endif
    if (want_stats) {
        FILE *f = fopen("SMSPSG.BIN", "wb");
        if (!f || fwrite(capture_bytes, 3U, capture_count, f) != capture_count)
            failed = 1U;
        if (f && fclose(f)) failed = 1U;
        printf("PSG records=%u overflow=%u irq_late=%lu\n", capture_count, capture_overflow, irq_late);
    }
    puts(failed ? "SMS boot: cleanup/playback failed" : "SMS boot: stopped, PSG muted");
    return failed ? 1 : 0;
}
