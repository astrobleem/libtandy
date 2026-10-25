/* SPDX-License-Identifier: MIT */
/* Optional DOS example; the referenced musical data has its own notice.
 * Run DOEBEL.EXE explicitly in foreground Tandy DOS. Escape cancels.
 * No video switch, startup hook, bank changes or PC-speaker access.
 */
#include <conio.h>
#include <stdio.h>
#include "lt_audio.h"
#include "lt_score.h"
#include "lt_tick_dos.h"
#include "doebel/DOEBEL.H"

extern void lt_dos_emit(void *context, lt_u8 byte);
static lt_u16 psg_port = 0x00C0U;
static lt_chip chip;
static lta_handle audio;
static lt_score_player music;
static volatile unsigned finished, playback_error;

/* Existing timer adapter, nominal 240 Hz (1,193,182 / 4,972 Hz).
 * Keep the approved score's 240-Hz service contract and tempo unchanged. */
#define DOEBEL_PIT_DIVISOR 4972U

static void wait_for_interrupt(void);
#pragma aux wait_for_interrupt = "hlt" modify exact []

static void update(void *context)
{
    lta_status status;
    (void)context;
    if (finished) return;
    if (music.phase + music.step >= (lta_u32)DOEBEL_END_TICK << 16) {
        status = lt_score_stop(&music);
        finished = 1U;
    } else {
        status = lt_score_tick(&music);
    }
    if (status != LTA_OK) {
        playback_error = (unsigned)status;
        finished = 1U;
    }
}

int main(void)
{
    lta_request request;
    int timer_open = 0;
    int failed = 0;
    puts("Doebelinschmirtz Incorporated - optional PSG example");
    puts("One jingle; Escape cancels. Use exclusive foreground Tandy DOS.");
    lt_init(&chip, lt_dos_emit, &psg_port);
    if (lta_init(&audio, &chip, 1U) != LTA_OK) return 1;
    lta_request_init(&request);
    request.op = LTA_OP_ACQUIRE;
    request.mask = 15U;
    if (lta_command(&audio, &request) != LTA_OK) return 1;
    if (lt_score_init(&music, &audio) != LTA_OK) {
        failed = 1;
    } else if (lttk_open(DOEBEL_PIT_DIVISOR, update, 0) != LTTK_OK) {
        puts("DOEBEL: timer busy");
        failed = 1;
    } else {
        timer_open = 1;
        if (lt_score_start(&music, &doebel_score,
                           DOEBEL_SERVICE_HZ, 0, 0) != LTA_OK) {
            failed = 1;
        } else {
            lttk_run(1);
            while (!finished) {
                if (kbhit() && getch() == 27) break;
                wait_for_interrupt();
            }
            lttk_run(0);
        }
        if (lt_score_stop(&music) != LTA_OK) failed = 1;
    }
    /* Work is stopped before foreground audio cleanup. Release mutes all
     * owned channels; close restores the BIOS timer using the same adapter. */
    lta_request_init(&request);
    request.op = LTA_OP_RELEASE;
    if (lta_command(&audio, &request) != LTA_OK) failed = 1;
    if (timer_open && lttk_close() != LTTK_OK) failed = 1;
    if (playback_error) failed = 1;
    puts(failed ? "DOEBEL: playback/cleanup failed" : "DOEBEL: stopped, PSG muted");
    return failed;
}
