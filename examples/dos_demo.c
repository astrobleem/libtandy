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
#include "dos_demo_motion.h"

static const lt_gfx_u8 square[8] = {
    0xFFU, 0x81U, 0xBDU, 0xA5U, 0xA5U, 0xBDU, 0x81U, 0xFFU
};

/* The 8086 HLT instruction sleeps until a timer or keyboard event wakes it. */
static void wait_for_interrupt(void);
#pragma aux wait_for_interrupt = "hlt" modify exact []

static int wait_until_next_tick(void *context)
{
    (void)context;
    wait_for_interrupt();
    return 0;
}

/* Keep setup details above main so the game loop stays easy to find. */
static const char *setup_demo(lt_dos_app *app, ltt_clock *clock,
                              lte_event_queue *input_queue,
                              lte_event *input_storage)
{
    lt_dos_app_status app_status;

    /* Initialize video, local input and sound. Sound starts muted. */
    app_status = lt_dos_app_init(app);
    if (app_status != LT_DOS_APP_OK)
        return "Could not start the Tandy video/sound session.";
    if (lte_event_queue_init(input_queue, input_storage, 8U) != LTE_OK)
        return "Could not create the input queue.";
    /* This example leaves DOS's standard BIOS clock alone. */
    if (ltd_clock_init(clock, 1) != LTT_OK)
        return "The standard DOS clock is not available.";
    if (!lt_tone(&app->chip, LT_DOS_APP_SOUND_OWNER, 0U, 256U, 1) ||
        !lt_attenuate(&app->chip, LT_DOS_APP_SOUND_OWNER, 0U, 8U, 1))
        return "Could not start the demo tone.";
    return 0;
}

int main(void)
{
    lt_dos_app app = LT_DOS_APP_INITIALIZER;
    ltt_clock clock;
    lte_event input_storage[8];
    lte_event_queue input_queue;
    lte_event event;
    lti_status input_status;
    ltt_status time_status;
    unsigned short x;
    const char *error_message;
    int quit;
    int result;

    error_message = setup_demo(&app, &clock, &input_queue, input_storage);
    result = error_message == 0 ? 0 : 1;
    if (error_message != 0) goto cleanup;
    x = 8U;
    quit = 0;

    while (!quit) {
        /* INPUT: BIOS press events only; no key-up or held-key state. */
        input_status = lti_dos_pump(&app.input, &input_queue,
                                    ltd_raw_ticks());
        if (input_status != LTI_OK && input_status != LTI_EMPTY) {
            error_message = "Input polling failed.";
            result = 1;
            break;
        }
        while (lte_event_pop(&input_queue, &event) == LTE_OK)
            if (event.type == LTE_EVENT_KEY_PRESS && event.code == 27U)
                quit = 1;
        if (quit) break;

        /* UPDATE: put your game rules here. This demo moves one square. */
        x = dos_demo_next_x(x);

        /* DRAW: mode 9 writes straight to video memory; no present call. */
        lt_gfx_clear(&app.screen, 1U);
        lt_gfx_glyph8(&app.screen, (lt_gfx_i16)x, 96, square, 15U, 0U, 1);

        /* TIMING: coarse BIOS ticks; idle wait, with no fixed frame rate. */
        time_status = ltd_delay_ms(&clock, 55U, wait_until_next_tick, 0);
        if (time_status != LTT_OK) {
            error_message = "The DOS clock stopped or became incompatible.";
            result = 1;
            break;
        }
    }

cleanup:
    /* CLEANUP: mute/release sound and restore the saved video and cursor. */
    lt_dos_app_shutdown(&app);
    if (error_message != 0) puts(error_message);
    return result;
}
