/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_audio.h"

typedef char lta_tick_size_must_be_4[(sizeof(lta_u32) == 4U) ? 1 : -1];
static lt_chip chip;
static lta_handle handle, other;
static lt_u8 trace[512];
static unsigned trace_count;
static int failures;

#define CHECK(x) do { if (!(x)) { \
    printf("FAIL audio_core.c:%d: %s\n", __LINE__, #x); ++failures; \
} } while (0)

static void emit(void *context, lt_u8 value)
{
    (void)context;
    if (trace_count < sizeof(trace)) trace[trace_count++] = value;
}

static lta_status command(lt_u8 op, lt_u8 mask, lt_u8 channel,
                          lt_u8 level, lt_u8 mode, lt_u8 enabled,
                          lt_u16 period, lta_u32 value)
{
    lta_request r;
    lta_request_init(&r);
    r.op = op; r.mask = mask; r.channel = channel; r.level = level;
    r.mode = mode; r.enabled = enabled; r.period = period; r.value = value;
    return lta_command(&handle, &r);
}

int main(void)
{
    lta_handle saved_handle;
    lt_chip saved_chip;
    unsigned before;

    lt_init(&chip, emit, 0);
    CHECK(lta_init(&handle, &chip, 1U) == LTA_OK);
    CHECK(lta_capabilities() == (LTA_CAP_TONE | LTA_CAP_NOISE |
          LTA_CAP_MUTE | LTA_CAP_AUTO_STOP));
    CHECK(lta_command(&handle, 0) == LTA_INVALID);
    CHECK(command(LTA_OP_PCM, 0, 0, 0, 0, 0, 0, 0) == LTA_UNSUPPORTED);
    CHECK(trace_count == 0U);

    CHECK(lta_init(&other, &chip, 1U) == LTA_OK);
    CHECK(command(LTA_OP_ACQUIRE, 13U, 0, 0, 0, 0, 0, 0) == LTA_OK);
    CHECK(handle.active && handle.mask == 13U && chip.owner[0] == 1U &&
          chip.owner[2] == 1U && chip.owner[3] == 1U);
    CHECK(trace_count == 3U && trace[0] == 0x9FU && trace[1] == 0xDFU &&
          trace[2] == 0xFFU);
    before = trace_count;
    {
        lta_request r;
        saved_handle = other;
        lta_request_init(&r); r.op = LTA_OP_ACQUIRE; r.mask = 3U;
        CHECK(lta_command(&other, &r) == LTA_BUSY);
        CHECK(!other.active && trace_count == before && chip.owner[1] == 0U);
        CHECK(memcmp(&other, &saved_handle, sizeof(other)) == 0);
    }
    CHECK(command(LTA_OP_ACQUIRE, 1U, 0, 0, 0, 0, 0, 0) == LTA_BUSY);
    CHECK(trace_count == before);
    saved_handle = handle; saved_chip = chip;
    CHECK(command(LTA_OP_TONE_START, 0, 2U, 4U, 0, 0, 1024U, 0) == LTA_INVALID);
    CHECK(memcmp(&handle, &saved_handle, sizeof(handle)) == 0);
    CHECK(memcmp(&chip, &saved_chip, sizeof(chip)) == 0);

    CHECK(command(LTA_OP_NOISE_START, 0, 0, 2U, 3U, 0, 0, 0) == LTA_STATE);
    CHECK(command(LTA_OP_TONE_START, 0, 2U, 4U, 0, 0, 0x155U, 0) == LTA_OK);
    CHECK(handle.playing == 4U && handle.period[2] == 0x155U);
    CHECK(trace[trace_count - 3U] == 0xC5U &&
          trace[trace_count - 2U] == 0x15U && trace[trace_count - 1U] == 0xD4U);
    CHECK(command(LTA_OP_NOISE_START, 0, 0, 2U, 7U, 0, 0, 0) == LTA_OK);
    CHECK(handle.playing == 12U && chip.noise_valid && chip.noise == 7U);
    before = trace_count;
    CHECK(command(LTA_OP_NOISE_UPDATE, 0, 0, 3U, 7U, 0, 0, 0) == LTA_OK);
    CHECK(trace_count == before + 1U && trace[before] == 0xF3U);
    CHECK(command(LTA_OP_TONE_UPDATE, 0, 2U, 5U, 0, 0, 0x166U, 0) == LTA_OK);

    CHECK(command(LTA_OP_MUTE, 0, 0, 0, 0, 1U, 0, 0) == LTA_OK);
    CHECK(handle.playing == 12U && handle.muted && handle.level[2] == 5U &&
          handle.level[3] == 3U && chip.attenuation[2] == 15U &&
          chip.attenuation[3] == 15U);
    CHECK(command(LTA_OP_TONE_START, 0, 2U, 6U, 0, 0, 0x177U, 0) == LTA_OK);
    CHECK(chip.attenuation[2] == 15U && handle.level[2] == 6U);
    CHECK(command(LTA_OP_MUTE, 0, 0, 0, 0, 0U, 0, 0) == LTA_OK);
    CHECK(chip.attenuation[2] == 6U && chip.attenuation[3] == 3U);
    before = trace_count; saved_handle = handle; saved_chip = chip;
    CHECK(command(LTA_OP_MUTE, 0, 0, 0, 0, 2U, 0, 0) == LTA_INVALID);
    CHECK(trace_count == before && memcmp(&handle, &saved_handle, sizeof(handle)) == 0 &&
          memcmp(&chip, &saved_chip, sizeof(chip)) == 0);

    CHECK(command(LTA_OP_SERVICE, 0, 0, 0, 0, 0, 0, 0xFFFFFFF0UL) == LTA_OK);
    CHECK(command(LTA_OP_ARM_STOP, 4U, 0, 0, 0, 0, 0, 20UL) == LTA_OK);
    CHECK(handle.deadline[2] == 4UL && (handle.deadline_mask & 4U));
    CHECK(command(LTA_OP_ARM_STOP, 4U, 0, 0, 0, 0, 0, 10UL) == LTA_OK);
    CHECK(handle.deadline[2] == 0xFFFFFFFAUL);
    CHECK(command(LTA_OP_SERVICE, 0, 0, 0, 0, 0, 0, 0xFFFFFFF8UL) == LTA_OK);
    CHECK(handle.playing == 12U);
    CHECK(command(LTA_OP_SERVICE, 0, 0, 0, 0, 0, 0, 0xFFFFFFF7UL) == LTA_TIME);
    CHECK(handle.last_now == 0xFFFFFFF8UL && handle.playing == 12U);
    CHECK(command(LTA_OP_SERVICE, 0, 0, 0, 0, 0, 0, 0xFFFFFFFAUL) == LTA_OK);
    CHECK(handle.playing == 0U && handle.deadline_mask == 0U);
    CHECK(chip.owner[2] == 1U && chip.owner[3] == 1U);
    CHECK(!chip.noise_valid && !(chip.tone_valid & 4U));
    CHECK(command(LTA_OP_TONE_UPDATE, 0, 2U, 4U, 0, 0, 1U, 0) == LTA_STATE);
    CHECK(command(LTA_OP_NOISE_UPDATE, 0, 0, 4U, 0, 0, 0, 0) == LTA_STATE);
    CHECK(command(LTA_OP_NOISE_START, 0, 0, 4U, 3U, 0, 0, 0) == LTA_STATE);

    CHECK(command(LTA_OP_TONE_START, 0, 0, 9U, 0, 0, 100U, 0) == LTA_OK);
    CHECK(command(LTA_OP_SERVICE, 0, 0, 0, 0, 0, 0, 0xFFFFFFF9UL) == LTA_TIME);
    CHECK(command(LTA_OP_ARM_STOP, 2U, 0, 0, 0, 0, 0, 10UL) == LTA_OWNER);
    CHECK(command(LTA_OP_ARM_STOP, 1U, 0, 0, 0, 0, 0, 0x80000000UL) == LTA_INVALID);
    CHECK(command(LTA_OP_ARM_STOP, 1U, 0, 0, 0, 0, 0, 0UL) == LTA_OK);
    CHECK(handle.playing == 0U && chip.owner[0] == 1U);
    before = trace_count;
    CHECK(command(LTA_OP_STOP, 16U, 0, 0, 0, 0, 0, 0) == LTA_INVALID);
    CHECK(trace_count == before);

    /* Lost ownership is detected before any active operation has effects. */
    chip.owner[2] = 9U; saved_handle = handle; saved_chip = chip;
    CHECK(command(LTA_OP_MUTE, 0, 0, 0, 0, 1U, 0, 0) == LTA_OWNER);
    CHECK(memcmp(&handle, &saved_handle, sizeof(handle)) == 0);
    CHECK(memcmp(&chip, &saved_chip, sizeof(chip)) == 0);
    chip.owner[2] = 1U;
    CHECK(command(LTA_OP_RELEASE, 0, 0, 0, 0, 0, 0, 0) == LTA_OK);
    CHECK(!handle.active && chip.owner[0] == 0U && chip.owner[2] == 0U &&
          chip.owner[3] == 0U && chip.attenuation[0] == 15U &&
          chip.attenuation[2] == 15U && chip.attenuation[3] == 15U);
    CHECK(command(LTA_OP_RELEASE, 0, 0, 0, 0, 0, 0, 0) == LTA_STATE);

    /* Cached coupled noise owned by another token must not expand our STOP. */
    {
        lt_chip split_chip;
        lta_handle split_handle;
        unsigned split_before;
        lt_init(&split_chip, emit, 0);
        CHECK(lt_claim(&split_chip, 2U, 8U));
        CHECK(lta_init(&split_handle, &split_chip, 1U) == LTA_OK);
        {
            lta_request r;
            lta_request_init(&r); r.op = LTA_OP_ACQUIRE; r.mask = 4U;
            CHECK(lta_command(&split_handle, &r) == LTA_OK);
        }
        split_chip.noise = 7U; split_chip.noise_valid = 1U;
        split_before = trace_count;
        {
            lta_request r;
            lta_request_init(&r); r.op = LTA_OP_STOP; r.mask = 4U;
            CHECK(lta_command(&split_handle, &r) == LTA_OK);
        }
        CHECK(trace_count == split_before + 1U && trace[split_before] == 0xDFU);
        CHECK(split_chip.owner[2] == 1U && split_chip.owner[3] == 2U &&
              split_chip.noise_valid == 1U && split_chip.noise == 7U);
    }

    /* Due-stop expansion is preflighted before SERVICE advances its clock. */
    {
        lt_chip preflight_chip, saved_preflight_chip;
        lta_handle preflight_handle, saved_preflight_handle;
        lta_request r;
        unsigned preflight_before;
        lt_init(&preflight_chip, emit, 0);
        preflight_chip.owner[2] = 1U;
        preflight_chip.owner[3] = 1U;
        preflight_chip.noise = 7U;
        preflight_chip.noise_valid = 1U;
        CHECK(lta_init(&preflight_handle, &preflight_chip, 1U) == LTA_OK);
        preflight_handle.mask = 4U;
        preflight_handle.active = 1U;
        preflight_handle.playing = 4U;
        preflight_handle.kind[2] = LTA_KIND_TONE;
        preflight_handle.clock_valid = 1U;
        preflight_handle.deadline_mask = 4U;
        preflight_handle.last_now = 100UL;
        preflight_handle.deadline[2] = 101UL;
        saved_preflight_chip = preflight_chip;
        saved_preflight_handle = preflight_handle;
        preflight_before = trace_count;
        lta_request_init(&r); r.op = LTA_OP_SERVICE; r.value = 101UL;
        CHECK(lta_command(&preflight_handle, &r) == LTA_OWNER);
        CHECK(trace_count == preflight_before &&
              memcmp(&preflight_chip, &saved_preflight_chip,
                     sizeof(preflight_chip)) == 0 &&
              memcmp(&preflight_handle, &saved_preflight_handle,
                     sizeof(preflight_handle)) == 0);
        preflight_handle.mask = 0U;
        saved_preflight_handle = preflight_handle;
        lta_request_init(&r); r.op = LTA_OP_RELEASE;
        CHECK(lta_command(&preflight_handle, &r) == LTA_OWNER);
        CHECK(trace_count == preflight_before &&
              memcmp(&preflight_handle, &saved_preflight_handle,
                     sizeof(preflight_handle)) == 0 &&
              memcmp(&preflight_chip, &saved_preflight_chip,
                     sizeof(preflight_chip)) == 0);
    }

    if (failures) return 1;
    puts("audio core tests passed");
    return 0;
}
