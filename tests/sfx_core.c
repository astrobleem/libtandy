/* SPDX-License-Identifier: GPL-3.0-only */
/* Source-derived JAWS clip trace checks; see examples/jaws/COPYING. */
#include <stdio.h>
#include <string.h>
#include "lt_sfx.h"
#include "../examples/jaws/jaws_clips.h"

typedef char ltsfx_tick_must_be_one_byte[(sizeof(lt_u8) == 1U) ? 1 : -1];
typedef char ltsfx_word_must_be_two_bytes[(sizeof(lt_u16) == 2U) ? 1 : -1];

static lt_chip chip;
static lta_handle audio;
static lt_sfx_player player;
static lt_u8 trace[512];
static unsigned trace_count;
static int failures;

#define CHECK(test) do { if (!(test)) { \
    printf("FAIL sfx_core.c:%d: %s\n", __LINE__, #test); ++failures; \
} } while (0)

static void emit(void *context, lt_u8 value)
{
    (void)context;
    if (trace_count < sizeof(trace)) trace[trace_count++] = value;
}

static int setup_audio(lt_u8 mask)
{
    lta_request request;
    trace_count = 0U;
    lt_init(&chip, emit, 0);
    if (lta_init(&audio, &chip, 1U) != LTA_OK) return 0;
    lta_request_init(&request);
    request.op = LTA_OP_ACQUIRE;
    request.mask = mask;
    if (lta_command(&audio, &request) != LTA_OK) return 0;
    trace_count = 0U;
    return 1;
}

static lta_status tone_command(lt_u8 op, lt_u8 channel,
                               lt_u16 period, lt_u8 level)
{
    lta_request request;
    lta_request_init(&request);
    request.op = op;
    request.channel = channel;
    request.period = period;
    request.level = level;
    return lta_command(&audio, &request);
}

static lta_status noise_command(lt_u8 op, lt_u8 mode, lt_u8 level)
{
    lta_request request;
    lta_request_init(&request);
    request.op = op;
    request.mode = mode;
    request.level = level;
    return lta_command(&audio, &request);
}

static void save_expected(unsigned *counts, unsigned frame)
{
    counts[frame] = trace_count;
}

static void source_shot_reference(unsigned *counts)
{
    static const lt_u16 periods[11] = {
        96U, 48U, 24U, 51U, 101U, 96U, 191U, 180U, 360U, 340U, 340U
    };
    unsigned i;
    for (i = 0U; i < 11U; ++i) {
        (void)lt_tone(&chip, 1U, 1U, periods[i], i == 0U);
        (void)lt_attenuate(&chip, 1U, 1U, 4U, i == 0U);
        save_expected(counts, i);
    }
    /* The next source tick is the voice stop callback's mute policy. */
    (void)lt_attenuate(&chip, 1U, 1U, 15U, 1);
    save_expected(counts, 11U);
}

static void source_conch_reference(unsigned *counts)
{
    static const lt_u16 periods[8] = {
        161U, 81U, 143U, 72U, 128U, 64U, 114U, 57U
    };
    static const lt_u8 attenuation[4] = { 1U, 5U, 9U, 13U };
    unsigned frame;
    for (frame = 0U; frame < 65U; ++frame) {
        unsigned phrase = frame / 16U;
        unsigned phase = frame < 64U ? (frame % 16U) / 2U : 7U;
        if (phrase > 3U) phrase = 3U;
        (void)lt_tone(&chip, 1U, 1U, periods[phase], frame == 0U);
        (void)lt_attenuate(&chip, 1U, 1U, attenuation[phrase],
                           frame == 0U);
        save_expected(counts, frame);
    }
    (void)lt_attenuate(&chip, 1U, 1U, 15U, 1);
    save_expected(counts, 65U);
}

static void check_source_clip(const lt_sfx_clip *clip, int conch)
{
    static lt_u8 expected[512];
    static unsigned expected_count[256];
    unsigned expected_length, t;
    lt_sfx_player local_player;
    lta_status status;

    trace_count = 0U;
    lt_init(&chip, emit, 0);
    CHECK(lt_claim(&chip, 1U, 2U));
    if (conch) source_conch_reference(expected_count);
    else source_shot_reference(expected_count);
    expected_length = trace_count;
    memcpy(expected, trace, expected_length);

    CHECK(setup_audio(2U));
    CHECK(lt_sfx_init(&local_player, &audio) == LTA_OK);
    CHECK(lt_sfx_trigger(&local_player, clip) == LTA_OK);
    trace_count = 0U;
    for (t = 0U; t < clip->duration_ticks; ++t) {
        status = lt_sfx_tick(&local_player);
        CHECK(status == LTA_OK);
        CHECK(trace_count == expected_count[t]);
    }
    CHECK(local_player.active && local_player.elapsed == clip->duration_ticks);
    CHECK(lt_sfx_tick(&local_player) == LTA_OK);
    CHECK(!local_player.active && !audio.effect_active);
    CHECK(trace_count == expected_count[clip->duration_ticks]);
    CHECK(trace_count == expected_length);
    CHECK(memcmp(trace, expected, expected_length) == 0);
}

static void test_source_traces(void)
{
    CHECK(lt_sfx_jaws_harpoon.event_count == 10U);
    CHECK(lt_sfx_jaws_harpoon.duration_ticks == 11U);
    CHECK(lt_sfx_jaws_conch.event_count == 32U);
    CHECK(lt_sfx_jaws_conch.duration_ticks == 65U);
    check_source_clip(&lt_sfx_jaws_harpoon, 0);
    check_source_clip(&lt_sfx_jaws_conch, 1);
}

static void test_invalid_clips(void)
{
    static lt_sfx_event valid_event[1] = {
        { 0U, LTSFX_OP_TONE, 1U, 100U, 0U, 3U }
    };
    static const lt_sfx_event unsorted[2] = {
        { 1U, LTSFX_OP_TONE, 1U, 100U, 0U, 3U },
        { 0U, LTSFX_OP_TONE, 1U, 110U, 0U, 3U }
    };
    static const lt_sfx_event noise3[1] = {
        { 0U, LTSFX_OP_NOISE, 3U, 3U, 0U, 3U }
    };
    lt_sfx_clip clip;
    lta_handle handle_before;
    lt_chip chip_before;
    unsigned trace_before;
    lt_sfx_event too_many[1];

    CHECK(setup_audio(15U));
    CHECK(lt_sfx_init(&player, &audio) == LTA_OK);
    clip.version = LTSFX_CLIP_VERSION;
    clip.channel_mask = 2U;
    clip.priority = 1U;
    clip.event_count = 1U;
    clip.duration_ticks = 1U;
    clip.provenance_id = LTSFX_PROVENANCE_NONE;
    clip.event_bytes = (lt_u16)sizeof(valid_event);
    clip.tick_rate_num = 60UL;
    clip.tick_rate_den = 1UL;
    clip.events = valid_event;

    trace_before = trace_count;
    handle_before = audio;
    chip_before = chip;
    CHECK(lt_sfx_trigger(&player, 0) == LTA_INVALID);
    CHECK(trace_count == trace_before && !audio.effect_active);
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_OK);
    CHECK(lt_sfx_stop(&player) == LTA_OK);
    trace_count = trace_before;
    handle_before = audio;
    chip_before = chip;

    clip.version = 1U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.version = LTSFX_CLIP_VERSION;
    clip.event_bytes = 5U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.event_bytes = (lt_u16)sizeof(valid_event);
    clip.duration_ticks = 0U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.duration_ticks = 1U;
    clip.channel_mask = 1U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.channel_mask = 2U;
    clip.event_count = 2U;
    clip.event_bytes = (lt_u16)(2U * sizeof(lt_sfx_event));
    clip.events = unsorted;
    clip.duration_ticks = 2U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.event_count = 65U;
    clip.event_bytes = 390U;
    clip.events = too_many;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);

    clip.event_count = 1U;
    clip.event_bytes = (lt_u16)sizeof(noise3);
    clip.events = noise3;
    clip.channel_mask = 8U;
    clip.duration_ticks = 1U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.channel_mask = 12U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);

    clip.events = valid_event;
    clip.channel_mask = 2U;
    clip.duration_ticks = 1U;
    clip.tick_rate_num = 0UL;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.tick_rate_num = 120UL;
    clip.tick_rate_den = 2UL;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.tick_rate_num = 60UL;
    clip.tick_rate_den = 1UL;
    clip.provenance_id = 2U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    clip.provenance_id = LTSFX_PROVENANCE_NONE;
    ((lt_sfx_event *)valid_event)[0].value_hi = 4U;
    CHECK(lt_sfx_trigger(&player, &clip) == LTA_INVALID);
    ((lt_sfx_event *)valid_event)[0].value_hi = 0U;
    CHECK(trace_count == trace_before);
    CHECK(memcmp(&audio, &handle_before, sizeof(audio)) == 0);
    CHECK(memcmp(&chip, &chip_before, sizeof(chip)) == 0);
}

static void test_file_round_trip_and_validation(void)
{
    static const lt_u8 expected_prefix[26] = {
        'L', 'T', 'S', 'F', 1U, 20U, 2U, 3U, 10U, 11U,
        0xe7U, 0x4aU, 0x02U, 0x00U,
        0xc4U, 0x09U, 0x00U, 0x00U, 1U, 0U,
        0U, LTSFX_OP_TONE, 1U, 96U, 0U, 4U
    };
    lt_u8 bytes[LTSFX_FILE_MAX_BYTES + 1U];
    lt_u8 second[LTSFX_FILE_MAX_BYTES];
    lt_sfx_event storage[LTSFX_MAX_EVENTS];
    lt_sfx_clip imported;
    lt_u16 byte_count, written;
    unsigned index;

    CHECK(lt_sfx_file_size_v1(&lt_sfx_jaws_harpoon, &byte_count) == LTA_OK);
    CHECK(byte_count == 80U);
    CHECK(lt_sfx_jaws_harpoon.tick_rate_num == 150247UL &&
          lt_sfx_jaws_harpoon.tick_rate_den == 2500UL);
    CHECK(lt_sfx_file_encode_v1(&lt_sfx_jaws_harpoon, bytes,
                                LTSFX_FILE_MAX_BYTES, &written) == LTA_OK);
    CHECK(written == byte_count);
    CHECK(memcmp(bytes, expected_prefix, sizeof(expected_prefix)) == 0);
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_OK);
    CHECK(imported.version == LTSFX_CLIP_VERSION &&
          imported.provenance_id == LTSFX_PROVENANCE_JAWS_M2 &&
          imported.tick_rate_num == 150247UL &&
          imported.tick_rate_den == 2500UL &&
          imported.event_count == 10U && imported.duration_ticks == 11U);
    CHECK(imported.events == storage && storage[9].tick == 9U &&
          storage[9].value_lo == 84U);
    CHECK(lt_sfx_file_encode_v1(&imported, second,
                                LTSFX_FILE_MAX_BYTES, &written) == LTA_OK);
    CHECK(written == byte_count && memcmp(bytes, second, byte_count) == 0);

    CHECK(lt_sfx_file_decode_v1(bytes, (lt_u16)(byte_count - 1U),
                                &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);
    bytes[byte_count] = 0U;
    CHECK(lt_sfx_file_decode_v1(bytes, (lt_u16)(byte_count + 1U),
                                &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                9U) == LTA_INVALID);
    CHECK(lt_sfx_file_encode_v1(&lt_sfx_jaws_harpoon, second,
                                (lt_u16)(byte_count - 1U), &written) ==
          LTA_INVALID);
    CHECK(written == 0U);

    for (index = 4U; index <= 5U; ++index) {
        lt_u8 saved = bytes[index];
        bytes[index] = 0xffU;
        CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                    LTSFX_MAX_EVENTS) == LTA_INVALID);
        bytes[index] = saved;
    }
    bytes[0] = (lt_u8)'X';
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);
    bytes[0] = (lt_u8)'L';
    bytes[18] = 2U;
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);
    bytes[18] = 1U;
    bytes[19] = 1U;
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);
    bytes[19] = 0U;
    bytes[14] = 0U;
    bytes[15] = 0U;
    bytes[16] = 0U;
    bytes[17] = 0U;
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);

    CHECK(lt_sfx_file_encode_v1(&lt_sfx_jaws_harpoon, bytes,
                                LTSFX_FILE_MAX_BYTES, &written) == LTA_OK);
    bytes[21] = 0U;
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);

    CHECK(lt_sfx_file_encode_v1(&lt_sfx_jaws_harpoon, bytes,
                                LTSFX_FILE_MAX_BYTES, &written) == LTA_OK);
    bytes[10] = 120U;
    bytes[11] = 0U;
    bytes[12] = 0U;
    bytes[13] = 0U;
    bytes[14] = 2U;
    bytes[15] = 0U;
    bytes[16] = 0U;
    bytes[17] = 0U;
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);

    CHECK(lt_sfx_file_encode_v1(&lt_sfx_jaws_harpoon, bytes,
                                LTSFX_FILE_MAX_BYTES, &written) == LTA_OK);
    bytes[8] = 65U;
    CHECK(lt_sfx_file_decode_v1(bytes, byte_count, &imported, storage,
                                LTSFX_MAX_EVENTS) == LTA_INVALID);
}

static void test_preemption_and_stop(void)
{
    lt_sfx_clip active_clip;
    lt_sfx_clip low_clip;
    lt_sfx_clip equal_clip;
    lt_sfx_clip high_clip;
    lt_sfx_player second_player;
    unsigned before;

    CHECK(setup_audio(3U));
    CHECK(lt_sfx_init(&player, &audio) == LTA_OK);
    CHECK(lt_sfx_init(&second_player, &audio) == LTA_OK);
    active_clip = lt_sfx_jaws_conch;
    CHECK(lt_sfx_trigger(&player, &active_clip) == LTA_OK);
    before = trace_count;
    CHECK(lt_sfx_trigger(&second_player, &lt_sfx_jaws_harpoon) == LTA_BUSY);
    CHECK(trace_count == before && !second_player.active);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    before = trace_count;
    low_clip = lt_sfx_jaws_harpoon;
    CHECK(lt_sfx_trigger(&player, &low_clip) == LTA_BUSY);
    CHECK(trace_count == before && player.clip == &active_clip &&
          player.elapsed == 1U);

    equal_clip = lt_sfx_jaws_conch;
    CHECK(lt_sfx_trigger(&player, &equal_clip) == LTA_OK);
    CHECK(player.clip == &equal_clip && player.elapsed == 0U &&
          player.active && audio.effect_active);
    high_clip = lt_sfx_jaws_harpoon;
    high_clip.priority = (lt_u8)(equal_clip.priority + 1U);
    CHECK(lt_sfx_trigger(&player, &high_clip) == LTA_OK);
    CHECK(player.clip == &high_clip && player.elapsed == 0U);
    CHECK(lt_sfx_stop(&player) == LTA_OK);
    CHECK(!player.active && !audio.effect_active &&
          player.event_index == 0U && player.elapsed == 0U);
    CHECK(lt_sfx_stop(&player) == LTA_OK);
    CHECK(lt_sfx_reset(&player) == LTA_OK);
}

static void test_music_restore(void)
{
    lta_request request;
    unsigned before;
    unsigned i;

    CHECK(setup_audio(3U));
    CHECK(tone_command(LTA_OP_TONE_START, 0U, 400U, 10U) == LTA_OK);
    CHECK(tone_command(LTA_OP_TONE_START, 1U, 240U, 7U) == LTA_OK);
    trace_count = 0U;
    CHECK(lt_sfx_init(&player, &audio) == LTA_OK);
    CHECK(lt_sfx_trigger(&player, &lt_sfx_jaws_harpoon) == LTA_OK);
    trace_count = 0U;
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    before = trace_count;
    CHECK(tone_command(LTA_OP_TONE_UPDATE, 1U, 333U, 2U) == LTA_OK);
    CHECK(trace_count == before);
    CHECK(audio.period[1] == 333U && audio.level[1] == 2U);
    for (i = 1U; i < 11U; ++i) CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(player.active);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(!player.active && !audio.effect_active);
    CHECK(chip.period[1] == 333U && chip.attenuation[1] == 2U);
    CHECK(chip.period[0] == 400U && chip.attenuation[0] == 10U);
    CHECK(trace_count >= 4U && trace[trace_count - 4U] == 0xBFU &&
          trace[trace_count - 3U] == 0xADU &&
          trace[trace_count - 2U] == 0x14U &&
          trace[trace_count - 1U] == 0xB2U);

    CHECK(lt_sfx_trigger(&player, &lt_sfx_jaws_conch) == LTA_OK);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(lt_sfx_stop(&player) == LTA_OK);
    CHECK(chip.period[1] == 333U && chip.attenuation[1] == 2U);
    CHECK(lt_sfx_reset(&player) == LTA_OK);

    CHECK(lt_sfx_trigger(&player, &lt_sfx_jaws_harpoon) == LTA_OK);
    trace_count = 0U;
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    before = trace_count;
    lta_request_init(&request);
    request.op = LTA_OP_STOP;
    request.mask = 2U;
    CHECK(lta_command(&audio, &request) == LTA_OK);
    CHECK(trace_count == before && audio.playing == 1U);
    for (i = 1U; i < 11U; ++i) CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(!player.active && chip.attenuation[1] == 15U &&
          !(chip.tone_valid & 2U));
}

static void test_noise_coupling(void)
{
    static const lt_sfx_event tone2_event[1] = {
        { 0U, LTSFX_OP_TONE, 2U, 100U, 0U, 15U }
    };
    static const lt_sfx_event noise3_events[3] = {
        { 0U, LTSFX_OP_TONE, 2U, 100U, 0U, 15U },
        { 0U, LTSFX_OP_NOISE, 3U, 3U, 0U, 3U },
        { 1U, LTSFX_OP_STOP, 2U, 0U, 0U, 0U }
    };
    lt_sfx_clip tone2_clip;
    lt_sfx_clip noise3_clip;

    CHECK(setup_audio(12U));
    CHECK(tone_command(LTA_OP_TONE_START, 2U, 300U, 5U) == LTA_OK);
    CHECK(noise_command(LTA_OP_NOISE_START, 3U, 4U) == LTA_OK);
    CHECK(lt_sfx_init(&player, &audio) == LTA_OK);

    tone2_clip.version = LTSFX_CLIP_VERSION;
    tone2_clip.channel_mask = 4U;
    tone2_clip.priority = 1U;
    tone2_clip.event_count = 1U;
    tone2_clip.duration_ticks = 1U;
    tone2_clip.provenance_id = LTSFX_PROVENANCE_NONE;
    tone2_clip.event_bytes = (lt_u16)sizeof(tone2_event);
    tone2_clip.tick_rate_num = 60UL;
    tone2_clip.tick_rate_den = 1UL;
    tone2_clip.events = tone2_event;
    CHECK(lt_sfx_trigger(&player, &tone2_clip) == LTA_OK);
    CHECK(player.borrowed_mask == 12U);
    CHECK(chip.attenuation[2] == 15U && chip.attenuation[3] == 15U);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(tone_command(LTA_OP_TONE_UPDATE, 2U, 301U, 6U) == LTA_OK);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(chip.period[2] == 301U && chip.attenuation[2] == 6U);
    CHECK(chip.noise_valid && chip.noise == 3U &&
          chip.attenuation[3] == 4U);

    noise3_clip.version = LTSFX_CLIP_VERSION;
    noise3_clip.channel_mask = 12U;
    noise3_clip.priority = 1U;
    noise3_clip.event_count = 3U;
    noise3_clip.duration_ticks = 3U;
    noise3_clip.provenance_id = LTSFX_PROVENANCE_NONE;
    noise3_clip.event_bytes = (lt_u16)sizeof(noise3_events);
    noise3_clip.tick_rate_num = 60UL;
    noise3_clip.tick_rate_den = 1UL;
    noise3_clip.events = noise3_events;
    CHECK(lt_sfx_trigger(&player, &noise3_clip) == LTA_OK);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(chip.noise_valid && chip.noise == 3U &&
          chip.attenuation[3] == 3U);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(!chip.noise_valid && chip.attenuation[2] == 15U &&
          chip.attenuation[3] == 15U);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(lt_sfx_tick(&player) == LTA_OK);
    CHECK(chip.period[2] == 301U && chip.attenuation[2] == 6U);
    CHECK(chip.noise_valid && chip.noise == 3U &&
          chip.attenuation[3] == 4U);
}

int main(void)
{
    test_source_traces();
    test_invalid_clips();
    test_file_round_trip_and_validation();
    test_preemption_and_stop();
    test_music_restore();
    test_noise_coupling();
    if (failures) return 1;
    puts("sfx core tests passed");
    return 0;
}
