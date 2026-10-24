/* SPDX-License-Identifier: MIT */
/*
 * PAGEDEMO: two-page presentation (lt_page_dos.h). A bar sweeps across the
 * screen; each frame is drawn on the hidden page and shown with a page
 * flip, so the bar never tears or flickers. With one page (open failed) the
 * same drawing goes straight to the visible frame and flickers. Escape
 * quits; the statistics are printed after the video mode is restored.
 */
#include <stdio.h>
#include "lt_dos_app.h"
#include "lt_event.h"
#include "lt_page_dos.h"

int main(void)
{
    lt_dos_app app = LT_DOS_APP_INITIALIZER;
    ltpg_pages pages;
    ltpg_status st;
    lte_event store[4], ev;
    lte_event_queue queue;
    lt_gfx_i16 x = 0, old[2] = { 0, 0 };
    unsigned which = 0, quit = 0;
    unsigned long frames = 0, flips, timeouts, reserved;
    unsigned two;

    if (lt_dos_app_init(&app) != LT_DOS_APP_OK) {
        puts("PAGEDEMO: no mode 9");
        return 1;
    }
    (void)lte_event_queue_init(&queue, store, 4U);
    st = ltpg_open(&pages);             /* before any other allocation */
    lt_gfx_clear(&pages.front, 1U);
    lt_gfx_clear(&pages.back, 1U);
    while (!quit) {
        /* erase this page's previous bar, draw the new one */
        lt_gfx_rect(&pages.back, old[which], 40, (lt_gfx_i16)(old[which] + 24),
                    160, 1U);
        lt_gfx_rect(&pages.back, x, 40, (lt_gfx_i16)(x + 24), 160, 14U);
        lt_gfx_rect(&pages.back, 0, 0, 320, 8, pages.two ? 10U : 12U);
        old[which] = x;
        ltpg_present(&pages, 0, 0);
        if (pages.two) which ^= 1U;
        x = (lt_gfx_i16)((x + 2) % 296);
        ++frames;
        (void)lti_dos_pump(&app.input, &queue, frames);
        while (lte_event_pop(&queue, &ev) == LTE_OK)
            if (ev.type == LTE_EVENT_KEY_PRESS && ev.code == 27U) quit = 1;
    }
    two = pages.two;
    flips = pages.flips;
    timeouts = pages.timeouts;
    reserved = ltpg_reserved_bytes(&pages);
    ltpg_close(&pages);
    lt_dos_app_shutdown(&app);
    printf("PAGEDEMO pages=%u open=%d frames=%lu flips=%lu "
           "sync_timeouts=%lu reserved=%lu bytes\n", two ? 2U : 1U,
           (int)st, frames, flips, timeouts, reserved);
    return 0;
}
