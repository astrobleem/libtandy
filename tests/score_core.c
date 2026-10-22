/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_score.h"
#include "lt_sfx.h"
#include "../src/lt_score_clock.h"

static lt_chip chip;
static lta_handle audio;
static lt_score_player score_player;
static lt_sfx_player effects;
static lt_u8 trace[256];
static unsigned trace_count;
static unsigned failures;

#define CHECK(test) do { if (!(test)) { \
    printf("FAIL score_core.c:%d: %s\n", __LINE__, #test); ++failures; \
} } while (0)

typedef struct {
    lt_u16 divisor[16];
    lt_u8 gate[16];
    unsigned count;
} speaker_trace;

static speaker_trace speaker_log;

static const lt_score_note melody[] = {
    { 0U, 10U, 180U },
    { 16U, 30U, 170U }
};

static const lt_score_note bass[] = {
    { 0U, 10U, 220U },
    { 16U, 30U, 210U }
};

static const lt_score_drum drums[] = {
    { 0U, LTSCORE_DRUM_HAT },
    { 16U, LTSCORE_DRUM_HAT }
};

static const lt_score_kick kicks[] = {
    { 0U },
    { 16U }
};

static const lt_score source_score = {
    LTSCORE_VERSION, 4U, 192U, 16U, 32U, 422535UL,
    { melody, bass, 0 },
    { 2U, 2U, 0U },
    { 2U, 7U, 10U },
    { 6U, 10U, 8U },
    drums, 2U, kicks, 2U
};

static const lt_sfx_event overlay_event[] = {
    { 0U, LTSFX_OP_TONE, 0U, 44U, 1U, 3U }
};

static const lt_sfx_clip overlay_clip = {
    LTSFX_CLIP_VERSION, 1U, 1U, 1U, 8U,
    LTSFX_PROVENANCE_NONE,
    (lt_u16)sizeof(overlay_event), 60UL, 1UL, overlay_event
};

static void emit(void *context, lt_u8 value)
{
    (void)context;
    if (trace_count < sizeof(trace)) trace[trace_count++] = value;
}

static void speaker_emit(void *context, lt_u16 divisor, lt_u8 gate)
{
    speaker_trace *log = (speaker_trace *)context;
    if (log->count < 16U) {
        log->divisor[log->count] = divisor;
        log->gate[log->count] = gate;
        ++log->count;
    }
}

static int setup_audio(void)
{
    lta_request request;
    trace_count = 0U;
    memset(&speaker_log, 0, sizeof(speaker_log));
    lt_init(&chip, emit, 0);
    if (lta_init(&audio, &chip, 1U) != LTA_OK) return 0;
    lta_request_init(&request);
    request.op = LTA_OP_ACQUIRE;
    request.mask = 15U;
    if (lta_command(&audio, &request) != LTA_OK) return 0;
    trace_count = 0U;
    return lt_score_init(&score_player, &audio) == LTA_OK;
}

static void test_exact_register_timing(void)
{
    static const lt_u8 start_trace[] = {
        0x9fU, 0xbfU, 0xdfU, 0xffU, 0xe4U,
        0x84U, 0x0bU, 0x94U,
        0xacU, 0x0dU, 0xb9U,
        0xfaU
    };
    static const lt_u8 fifth_tick_trace[] = {
        0x9fU, 0xbfU, 0xdfU, 0xffU,
        0x8aU, 0x0aU, 0x94U,
        0xa2U, 0x0dU, 0xb9U,
        0xfaU
    };
    static const lt_u8 third_tick_trace[] = {
        0x8aU, 0x0aU, 0x94U, 0xa2U, 0x0dU, 0xb9U, 0xfdU, 0xfaU
    };
    static const lt_u8 fourth_tick_trace[] = { 0x9fU, 0xbfU };
    CHECK(setup_audio());
    CHECK(lt_score_start(&score_player, &source_score, 60U,
                         speaker_emit, &speaker_log) == LTA_OK);
    CHECK(trace_count == sizeof(start_trace));
    CHECK(memcmp(trace, start_trace, sizeof(start_trace)) == 0);
    CHECK(speaker_log.count == 1U &&
          speaker_log.divisor[0] == 9178U && speaker_log.gate[0] == 1U);
    CHECK(score_player.step == 495889UL);
    CHECK(lt_score_tick(&score_player) == LTA_OK);
    CHECK(score_player.phase == 495889UL &&
          trace_count == sizeof(start_trace) + 1U && trace[12] == 0x9fU);
    CHECK(lt_score_tick(&score_player) == LTA_OK);
    CHECK(trace_count == sizeof(start_trace) + 3U);
    CHECK(trace[13] == 0xbfU && trace[14] == 0xfdU);
    CHECK(lt_score_tick(&score_player) == LTA_OK);
    CHECK(trace_count == 15U + sizeof(third_tick_trace));
    CHECK(memcmp(trace + 15U, third_tick_trace,
                 sizeof(third_tick_trace)) == 0);
    CHECK(lt_score_tick(&score_player) == LTA_OK);
    CHECK(trace_count == 23U + sizeof(fourth_tick_trace));
    CHECK(memcmp(trace + 23U, fourth_tick_trace,
                 sizeof(fourth_tick_trace)) == 0);
    CHECK(lt_score_tick(&score_player) == LTA_OK);
    CHECK(score_player.phase == 1430869UL);
    CHECK(trace_count == 25U + sizeof(fifth_tick_trace));
    CHECK(memcmp(trace + 25U, fifth_tick_trace,
                 sizeof(fifth_tick_trace)) == 0);
    CHECK(chip.period[0] == 170U && chip.period[1] == 210U);
    CHECK(speaker_log.count == 8U);
    CHECK(speaker_log.divisor[1] == 11932U &&
          speaker_log.gate[1] == LTSCORE_SPEAKER_KEEP_GATE);
    CHECK(speaker_log.divisor[2] == 15297U &&
          speaker_log.gate[2] == LTSCORE_SPEAKER_KEEP_GATE);
    CHECK(speaker_log.divisor[3] == 0U &&
          speaker_log.gate[3] == LTSCORE_SPEAKER_GATE_OFF);
    CHECK(speaker_log.divisor[4] == 9178U &&
          speaker_log.gate[4] == LTSCORE_SPEAKER_GATE_ON);
    CHECK(speaker_log.divisor[5] == 11932U &&
          speaker_log.gate[5] == LTSCORE_SPEAKER_KEEP_GATE);
    CHECK(speaker_log.divisor[6] == 0U &&
          speaker_log.gate[6] == LTSCORE_SPEAKER_GATE_OFF);
    CHECK(speaker_log.divisor[7] == 9178U &&
          speaker_log.gate[7] == LTSCORE_SPEAKER_GATE_ON);
    CHECK(lt_score_stop(&score_player) == LTA_OK);
    CHECK(!score_player.active && trace[trace_count - 1U] == 0xffU);
    CHECK(lt_score_stop(&score_player) == LTA_OK);
}

static void test_music_shadow_during_effect(void)
{
    unsigned i;
    CHECK(setup_audio());
    CHECK(lt_score_start(&score_player, &source_score, 60U, 0, 0) == LTA_OK);
    CHECK(lt_sfx_init(&effects, &audio) == LTA_OK);
    CHECK(lt_sfx_trigger(&effects, &overlay_clip) == LTA_OK);
    CHECK(lt_sfx_tick(&effects) == LTA_OK);
    CHECK(chip.period[0] == 300U);
    for (i = 0U; i < 5U; ++i) {
        CHECK(lt_score_tick(&score_player) == LTA_OK);
        CHECK(lt_sfx_tick(&effects) == LTA_OK);
    }
    CHECK(audio.period[0] == 170U && audio.level[0] == 4U);
    CHECK(chip.period[0] == 300U);
    while (effects.active) CHECK(lt_sfx_tick(&effects) == LTA_OK);
    CHECK(!audio.effect_active && chip.period[0] == 170U);
    CHECK(chip.attenuation[0] == 4U);
    CHECK(lt_score_stop(&score_player) == LTA_OK);
}

static void test_validation_and_level_writes(void)
{
    lt_score invalid;
    lta_request request;
    unsigned before;
    CHECK(setup_audio());
    invalid = source_score;
    invalid.loop_end = 0U;
    before = trace_count;
    CHECK(lt_score_start(&score_player, &invalid, 60U, 0, 0) == LTA_INVALID);
    CHECK(trace_count == before && !score_player.active);

    lta_request_init(&request);
    request.op = LTA_OP_TONE_START;
    request.channel = 2U;
    request.period = 400U;
    request.level = 7U;
    CHECK(lta_command(&audio, &request) == LTA_OK);
    before = trace_count;
    lta_request_init(&request);
    request.op = LTA_OP_LEVEL;
    request.channel = 2U;
    request.level = 3U;
    CHECK(lta_command(&audio, &request) == LTA_OK);
    CHECK(trace_count == before + 1U && trace[trace_count - 1U] == 0xd3U);
    CHECK(audio.period[2] == 400U);

    before = trace_count;
    lta_request_init(&request);
    request.op = LTA_OP_NOISE_MODE;
    request.mode = 4U;
    CHECK(lta_command(&audio, &request) == LTA_OK);
    CHECK(trace_count == before + 1U && trace[trace_count - 1U] == 0xe4U);
}

static void test_32bit_tempo_rounding(void)
{
    lt_score boundary;
    lta_u32 legacy_sum, ticks_per_second;
    CHECK(setup_audio());

    /* These operands wrap at the target's 32-bit width. The old numerator +
     * tempo/2 expression became 51,032,704, then incorrectly rounded 0. */
    legacy_sum = (lta_u32)4096000000UL + (lta_u32)250000000UL;
    CHECK(legacy_sum == (lta_u32)51032704UL);
    CHECK(ltscore_ticks_per_second((lta_u32)4096U,
                                   (lta_u32)500000000UL,
                                   &ticks_per_second));
    CHECK(ticks_per_second == 8UL);

    boundary = source_score;
    boundary.ppq = 4096U;
    boundary.tempo_us = 500000000UL;
    CHECK(lt_score_start(&score_player, &boundary, 60U, 0, 0) == LTA_OK);
    CHECK(score_player.step == 8738UL);
    CHECK(lt_score_stop(&score_player) == LTA_OK);

    /* The ceil-half threshold must itself stay safe at UINT32_MAX. */
    CHECK(ltscore_ticks_per_second((lta_u32)4096U,
                                   (lta_u32)0xffffffffUL,
                                   &ticks_per_second));
    CHECK(ticks_per_second == 1UL);
}

int main(void)
{
    test_exact_register_timing();
    test_music_shadow_during_effect();
    test_validation_and_level_writes();
    test_32bit_tempo_rounding();
    if (failures) return 1;
    puts("PASS shared score scheduler and exact PSG trace");
    return 0;
}
