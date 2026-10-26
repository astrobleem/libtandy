/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "lt_sms_boot.h"
#include "sms_boot_ref.h"

static lt_chip chip;
static lt_sms_boot player;
static unsigned frame, index, writes, bad, checking;
static void emit(void *context, lt_u8 byte)
{
    (void)context;
    ++writes;
    if (checking == 1U) {
        if (index >= LTSB_PSG_WRITES ||
            reference[index].tick != frame || reference[index].byte != byte)
            ++bad;
        ++index;
    } else if (checking == 2U) {
        if (byte != (lt_u8)(0x9FU + 0x20U * index)) ++bad;
        ++index;
    }
}

#define REQUIRE(c) do { if (!(c)) { \
    printf("SMS boot failure at line %u\n", (unsigned)__LINE__); \
    return 1; } } while (0)

int main(void)
{
    unsigned i, before;
    lt_init(&chip, emit, 0);
    REQUIRE(!lt_sms_boot_init(0, &chip, 1U));
    REQUIRE(!lt_sms_boot_init(&player, &chip, 0U));
    REQUIRE(lt_sms_boot_init(&player, &chip, 1U));
    REQUIRE(writes == 0U && lt_sms_boot_stop(&player));
    REQUIRE(lt_claim(&chip, 2U, 4U));
    REQUIRE(!lt_sms_boot_start(&player));
    REQUIRE(writes == 0U && chip.owner[0] == 0U && chip.owner[2] == 2U);
    REQUIRE(lt_stop(&chip, 2U, 4U));
    writes = index = 0U; checking = 2U;
    REQUIRE(lt_sms_boot_start(&player));
    REQUIRE(writes == 4U && index == 4U);
    REQUIRE(!lt_sms_boot_start(&player));
    writes = index = 0U; checking = 1U;
    for (frame = 0; frame < LTSB_END_TICK; ++frame) {
        REQUIRE(player.tick == frame && player.active);
        before = writes;
        REQUIRE(lt_sms_boot_tick(&player));
        REQUIRE(writes - before <= 12U);
    }
    REQUIRE(index == LTSB_PSG_WRITES && writes == LTSB_PSG_WRITES);
    checking = 2U; index = 0U;
    REQUIRE(lt_sms_boot_tick(&player));
    REQUIRE(index == 4U && !player.active && !bad);
    for (i = 0; i < 4U; ++i)
        REQUIRE(chip.owner[i] == 0U && chip.attenuation[i] == 15U);
    before = writes;
    REQUIRE(lt_sms_boot_tick(&player) && lt_sms_boot_stop(&player));
    REQUIRE(before == writes);
    checking = 0U;
    REQUIRE(lt_sms_boot_start(&player));
    for (i = 0; i < 10U; ++i) REQUIRE(lt_sms_boot_tick(&player));
    REQUIRE(lt_sms_boot_stop(&player));
    before = writes;
    REQUIRE(lt_sms_boot_tick(&player) && writes == before);
    REQUIRE(lt_claim(&chip, 3U, 15U));
    REQUIRE(lt_tone(&chip, 3U, 0U, 613U, 0));
    REQUIRE(writes == before + 2U);
    REQUIRE(lt_stop(&chip, 3U, 15U));
    printf("PASS SMS boot: 710 source bytes/deadlines, bounded writes, "
           "conflict, restart, cancel, mute/release; player %u bytes\n",
           (unsigned)sizeof(player));
    return 0;
}
