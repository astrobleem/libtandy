/* SPDX-License-Identifier: MIT */
/*
 * Build from the repository root:
 *   python tools/build.py --target dos --watcom C:/WATCOM
 * Run DEMO.EXE on compatible Tandy DOS hardware.
 * Controls: press Escape to quit; the square moves across the screen.
 */
#include <stdio.h>
#include "lt_dos_app.h"
#include "lt_event.h"
#include "lt_time_dos.h"

static const lt_gfx_u8 square[8] = {
    0xFFU, 0x81U, 0xBDU, 0xA5U, 0xA5U, 0xBDU, 0x81U, 0xFFU
};

int main(void)
{
    lt_dos_app app;
    ltt_clock clock;
    lte_event input_storage[8];
    lte_event_queue input_queue;
    lte_event event;
    lt_dos_app_status app_status;
    lti_status input_status;
    ltt_status time_status;
    unsigned char x;
    const char *error_message;
    int quit;
    int result;

    /* Initialization saves the current video/cursor state, enters mode 9,
     * binds app.screen to video memory, and claims sound for this app. */
    app_status = lt_dos_app_init(&app);
    if (app_status != LT_DOS_APP_OK) {
        puts("Could not start the Tandy video/sound session.");
        return 1;
    }

    if (lte_event_queue_init(&input_queue, input_storage, 8U) != LTE_OK) {
        lt_dos_app_shutdown(&app);
        puts("Could not create the input queue.");
        return 1;
    }

    /* This example leaves DOS's standard BIOS clock alone. The delay below
     * is coarse and tick-based; it does not promise a particular frame rate. */
    if (ltd_clock_init(&clock, 1) != LTT_OK) {
        lt_dos_app_shutdown(&app);
        puts("The standard DOS clock is not available.");
        return 1;
    }

    if (!lt_tone(&app.chip, LT_DOS_APP_SOUND_OWNER, 0U, 256U, 1) ||
        !lt_attenuate(&app.chip, LT_DOS_APP_SOUND_OWNER, 0U, 8U, 1)) {
        lt_dos_app_shutdown(&app);
        puts("Could not start the demo tone.");
        return 1;
    }

    x = 8U;
    error_message = 0;
    quit = 0;
    result = 0;

    while (!quit) {
        /* INPUT: this reads BIOS press events only. There is no key-up or
         * held-key state; Escape is the only control in this example. */
        input_status = lti_dos_pump(&app.input, &input_queue,
                                    ltd_raw_ticks());
        if (input_status != LTI_OK && input_status != LTI_EMPTY) {
            error_message = "Input polling failed.";
            result = 1;
            break;
        }
        while (lte_event_pop(&input_queue, &event) == LTE_OK) {
            if (event.type == LTE_EVENT_KEY_PRESS && event.code == 27U)
                quit = 1;
        }
        if (quit) break;

        /* UPDATE: put your game rules here. This tiny demo moves one square. */
        ++x;
        if (x > 304U) x = 8U;

        /* DRAW: lt_gfx writes straight to mode-9 video memory, so a separate
         * present call is not needed. Replace this with your scene drawing. */
        lt_gfx_clear(&app.screen, 1U);
        lt_gfx_glyph8(&app.screen, (lt_gfx_i16)x, 96, square, 15U, 0U, 1);

        /* TIMING: this coarse minimum delay rounds to BIOS ticks and includes
         * an extra phase tick. Measure elapsed time for real game movement. */
        time_status = ltd_delay_ms(&clock, 55U, 0, 0);
        if (time_status != LTT_OK) {
            error_message = "The DOS clock stopped or became incompatible.";
            result = 1;
            break;
        }
    }

    /* CLEANUP: always mute/unclaim sound and restore the saved video/cursor. */
    lt_dos_app_shutdown(&app);
    if (error_message != 0) puts(error_message);
    return result;
}
