/* SPDX-License-Identifier: MIT */
/* Preserve the actual PSG writes/timing that generated Chad's approved WAV.
 * The literal fixture is frozen from that audition, not regenerated from
 * the candidate score. Musical data attribution: examples/doebel/README.md.
 */
#include <stdio.h>
#include "lt_audio.h"
#include "lt_score.h"
#include "../examples/doebel/DOEBEL.H"
#include "../examples/doebel/APPROVED.H"

static lt_chip chip;
static lta_handle audio;
static lt_score_player music;
static unsigned frame, index, failures;

static void emit(void *context, lt_u8 byte)
{
    (void)context;
    if (index >= DOEBEL_APPROVED_WRITES ||
        doebel_approved[index].frame != frame ||
        doebel_approved[index].byte != byte) {
        ++failures;
    }
    ++index;
}

int main(void)
{
    lta_request request;
    unsigned voice;
    lt_init(&chip, emit, 0);
    if (lta_init(&audio, &chip, 1U) != LTA_OK) return 1;
    lta_request_init(&request);
    request.op = LTA_OP_ACQUIRE;
    request.mask = 15U;
    if (lta_command(&audio, &request) != LTA_OK) return 2;
    if (lt_score_init(&music, &audio) != LTA_OK) return 3;
    if (lt_score_start(&music, &doebel_score,
                       DOEBEL_SERVICE_HZ, 0, 0) != LTA_OK) return 4;
    while (music.phase + music.step < (lta_u32)DOEBEL_END_TICK << 16) {
        ++frame;
        if (lt_score_tick(&music) != LTA_OK) return 5;
    }
    ++frame;
    if (lt_score_stop(&music) != LTA_OK) return 6;
    lta_request_init(&request);
    request.op = LTA_OP_RELEASE;
    if (lta_command(&audio, &request) != LTA_OK) return 7;
    if (frame != 1023U || index != DOEBEL_APPROVED_WRITES) ++failures;
    for (voice = 0U; voice < 4U; ++voice)
        if (chip.attenuation[voice] != 15U || chip.owner[voice]) ++failures;
    if (failures) {
        printf("FAIL Doebel approved trace: %u differences\n", failures);
        return 8;
    }
    puts("PASS Doebel approved PSG bytes, timing, levels and ending mute");
    return 0;
}
