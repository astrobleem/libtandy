/* SPDX-License-Identifier: MIT */
#ifndef LT_PAGE_DOS_H
#define LT_PAGE_DOS_H
#include "lt_gfx.h"

/* Two-page 320x200x16 (BIOS mode 9) presentation on Tandy 1000 video
 * (Open Watcom, 16-bit DOS). Presentation is a hardware page flip: the CRT
 * page field of the page register (port 3DFh) switches between two 32 KiB
 * frames during vertical blank; no frame is copied.
 *
 * Hardware assumptions (Tandy 1000 EX/HX/SX/TX-class video): the video
 * memory is the top 128 KiB window of system RAM; port 3DFh bits 0-2 select
 * the CRT page, bits 3-5 the CPU page seen at B800h, bits 6-7 = 11 the
 * 32 KiB addressing of modes 9/10; the BIOS keeps a copy at 0040:008Ah and
 * leaves CRT = CPU = page 6. The status register 3DAh bit 3 is vertical
 * sync. Open adds page 4 as the second frame: it finds where page 4 lies in
 * physical memory by writing a signature through B800h (saved and
 * restored; no assumption about the RAM size), and both frames are then
 * written through their physical segments, not through B800h.
 *
 * Memory cost: the 16 KiB units of pages 4 and 5 and the part of page 6
 * below the end of the DOS arena are DOS memory on a full 640 KiB machine
 * (the BIOS reserves only what the display uses). Open takes that memory
 * from DOS with a last-fit allocation so nothing else can be loaded there:
 * with the arena ending at 624 KiB (9C00h) that is 48 KiB (pages 4, 5 and
 * the lower half of 6). Call ltpg_open after setting mode 9 and before any
 * other allocation of the program, while the top of the arena is free.
 * ltpg_reserved_bytes() reports the actual amount (0 if the arena ends at or
 * below page 4).
 *
 * If open fails, both surfaces are the single frame at B800h, presenting
 * does nothing, and the program can go on drawing single-buffered. */
typedef enum {
    LTPG_OK = 0,
    LTPG_INVALID = 1,       /* null pointer, or a pair is already open */
    LTPG_NO_WINDOW = 2,     /* page 4 not found in memory */
    LTPG_NO_ARENA = 3,      /* DOS memory chain unreadable */
    LTPG_NO_MEMORY = 4      /* the top of the arena is not free */
} ltpg_status;

typedef struct {
    lt_gfx_surface front;   /* shown */
    lt_gfx_surface back;    /* draw here, then ltpg_present */
    unsigned front_seg, back_seg;
    unsigned reserved_seg, reserved_paras;
    unsigned window_top;    /* paragraph just above the 128 KiB window */
    unsigned char shown;    /* CRT page 4 or 6 */
    unsigned char two;      /* two pages in use */
    /* vertical sync prediction (see ltpg_present) */
    unsigned long last_sync;
    unsigned char have_sync;
    unsigned long flips, predicted, timeouts;
} ltpg_pages;

ltpg_status ltpg_open(ltpg_pages *p);
/* Show the back frame and make the old front the new back. clock (may be
 * 0) returns a monotonic fine time and period is one video refresh in its
 * units (e.g. lttk_now with LTTK_PIT_HZ / 59.92 = 19912). The sync pulse
 * lasts well under a millisecond and is easily missed while an IRQ0 update
 * runs, so with a clock the flip waits for the predicted next refresh
 * (re-anchored whenever the pulse is seen) and flips at the predicted time
 * if the pulse stays hidden; without a clock it polls status bit 3 a
 * bounded number of times. Either way it never waits longer than about one
 * refresh. */
void ltpg_present(ltpg_pages *p, unsigned long (*clock)(void),
                  unsigned long period);
/* Show page 6 again and give the reserved memory back. 1 if the memory was
 * released (or none was held). Safe on a failed or closed object. */
int ltpg_close(ltpg_pages *p);
unsigned long ltpg_reserved_bytes(const ltpg_pages *p);

#endif
