/* SPDX-License-Identifier: MIT */
/* Tandy 1000 two-page mode-9 presentation; see lt_page_dos.h. */
#if !defined(__WATCOMC__) || !defined(__I86__)
#error lt_page_dos.c requires Open Watcom 16-bit x86
#endif
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <string.h>
#include "lt_page_dos.h"

#define LTPG_REG 0x3DFU
#define LTPG_STATUS 0x3DAU
#define LTPG_32K 0xC0U
#define LTPG_FRONT 6U
#define LTPG_BACK 4U
#define LTPG_UNIT 0x400U            /* paragraphs per 16 KiB page unit */
#define LTPG_WINDOW 0x2000U         /* paragraphs in the 128 KiB window */
#define LTPG_AFTER_SYNC 1500UL      /* still in blank this long after it */
#define LTPG_LATE 2500UL
#define LTPG_POLLS 30000U

static unsigned char ltpg_active;   /* one open page pair per program */

static void ltpg_select(ltpg_pages *p, unsigned crt)
{
    unsigned char v = (unsigned char)(LTPG_32K | crt | (LTPG_FRONT << 3));
    outp(LTPG_REG, v);
    *(unsigned char __far *)MK_FP(0x40, 0x8A) = v;
    if (p) p->shown = (unsigned char)crt;
}

/* Segment just past the last DOS memory block, 0 if the chain is bad. */
static unsigned ltpg_arena_end(void)
{
    union REGS r;
    struct SREGS s;
    unsigned seg, n;
    unsigned char kind;
    segread(&s);
    r.h.ah = 0x52;                  /* list of lists: first MCB at -2 */
    int86x(0x21, &r, &r, &s);
    seg = *(unsigned __far *)MK_FP(s.es, r.x.bx - 2U);
    for (n = 0; n < 4096U; ++n) {
        kind = *(unsigned char __far *)MK_FP(seg, 0);
        if (kind != 'M' && kind != 'Z') return 0;
        seg += 1U + *(unsigned __far *)MK_FP(seg, 3);
        if (kind == 'Z') return seg;
    }
    return 0;
}

/* Paragraph above the 128 KiB window holding CPU page 4, found by a
 * signature written through B800h with page 4 mapped there. */
static unsigned ltpg_find_window(void)
{
    unsigned char __far *w = (unsigned char __far *)MK_FP(0xB800, 0x7FF0);
    unsigned char keep[8];
    unsigned top, i, found = 0;
    _disable();
    outp(LTPG_REG, LTPG_32K | LTPG_FRONT | (LTPG_BACK << 3));
    for (i = 0; i < 8U; ++i) {
        keep[i] = w[i];
        w[i] = (unsigned char)(0xA5U ^ (i * 37U));
    }
    for (top = 0x4000U; top <= 0xA000U && !found; top += 0x1000U) {
        const unsigned char __far *q = (const unsigned char __far *)
            MK_FP(top - LTPG_WINDOW + LTPG_BACK * LTPG_UNIT + 0x7FFU, 0);
        for (i = 0; i < 8U; ++i)
            if (q[i] != (unsigned char)(0xA5U ^ (i * 37U))) break;
        if (i == 8U) found = top;
    }
    for (i = 0; i < 8U; ++i) w[i] = keep[i];
    outp(LTPG_REG, LTPG_32K | LTPG_FRONT | (LTPG_FRONT << 3));
    _enable();
    return found;
}

static unsigned ltpg_strategy(unsigned set, int write)
{
    union REGS r;
    r.x.ax = write ? 0x5801U : 0x5800U;
    r.x.bx = set;
    int86(0x21, &r, &r);
    if (r.x.cflag) return 0xFFFFU;
    return write ? 0U : r.x.ax;
}

static void ltpg_single(ltpg_pages *p)
{
    memset(p, 0, sizeof(*p));
    lt_gfx_init(&p->front, (lt_gfx_u8 __far *)MK_FP(0xB800, 0));
    p->back = p->front;
    p->front_seg = p->back_seg = 0xB800U;
    p->shown = LTPG_FRONT;
}

ltpg_status ltpg_open(ltpg_pages *p)
{
    unsigned top, end, base, need, old, seg = 0;
    if (p == 0 || ltpg_active) return LTPG_INVALID;
    ltpg_single(p);
    top = ltpg_find_window();
    if (!top) return LTPG_NO_WINDOW;
    base = top - LTPG_WINDOW;
    end = ltpg_arena_end();
    if (!end) return LTPG_NO_ARENA;
    if (end > base + LTPG_BACK * LTPG_UNIT) {
        /* The frames are DOS memory: own it from page 4 to the end. */
        need = end - (base + LTPG_BACK * LTPG_UNIT);
        old = ltpg_strategy(0, 0);
        if (old == 0xFFFFU || ltpg_strategy(2U, 1) == 0xFFFFU)
            return LTPG_NO_MEMORY;
        if (_dos_allocmem(need, &seg)) seg = 0;
        (void)ltpg_strategy(old, 1);
        if (!seg) return LTPG_NO_MEMORY;
        if (seg != base + LTPG_BACK * LTPG_UNIT) {
            _dos_freemem(seg);
            return LTPG_NO_MEMORY;
        }
        p->reserved_seg = seg;
        p->reserved_paras = need;
    }
    p->window_top = top;
    p->front_seg = base + LTPG_FRONT * LTPG_UNIT;
    p->back_seg = base + LTPG_BACK * LTPG_UNIT;
    lt_gfx_init(&p->front, (lt_gfx_u8 __far *)MK_FP(p->front_seg, 0));
    lt_gfx_init(&p->back, (lt_gfx_u8 __far *)MK_FP(p->back_seg, 0));
    p->two = 1;
    ltpg_active = 1;
    ltpg_select(p, LTPG_FRONT);
    return LTPG_OK;
}

static int ltpg_wait(ltpg_pages *p, unsigned long (*clock)(void),
                     unsigned long until)
{
    for (;;) {
        if (inp(LTPG_STATUS) & 8U) {
            p->last_sync = clock();
            p->have_sync = 1;
            return 1;
        }
        if ((long)(clock() - until) >= 0) return 0;
    }
}

void ltpg_present(ltpg_pages *p, unsigned long (*clock)(void),
                  unsigned long period)
{
    lt_gfx_surface t;
    unsigned seg, n;
    unsigned long now, phase;
    if (p == 0 || !p->two) return;
    if (clock && period) {
        now = clock();
        if (!p->have_sync) {
            if (!ltpg_wait(p, clock, now + period)) ++p->timeouts;
        } else {
            phase = (now - p->last_sync) % period;
            if (phase >= LTPG_AFTER_SYNC &&
                !ltpg_wait(p, clock, now + (period - phase) + LTPG_LATE))
                ++p->predicted;
        }
    } else {
        for (n = 0; n < LTPG_POLLS && !(inp(LTPG_STATUS) & 8U); ++n) ;
        if (n == LTPG_POLLS) ++p->timeouts;
    }
    ltpg_select(p, p->shown == LTPG_FRONT ? LTPG_BACK : LTPG_FRONT);
    t = p->front; p->front = p->back; p->back = t;
    seg = p->front_seg; p->front_seg = p->back_seg; p->back_seg = seg;
    ++p->flips;
}

int ltpg_close(ltpg_pages *p)
{
    int ok = 1;
    if (p == 0) return 1;
    if (p->two) ltpg_select(0, LTPG_FRONT);
    if (p->reserved_seg && _dos_freemem(p->reserved_seg)) ok = 0;
    if (p->two) ltpg_active = 0;
    ltpg_single(p);
    return ok;
}

unsigned long ltpg_reserved_bytes(const ltpg_pages *p)
{
    return p ? (unsigned long)p->reserved_paras * 16UL : 0UL;
}
