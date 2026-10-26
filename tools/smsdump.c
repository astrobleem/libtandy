/* SPDX-License-Identifier: MIT */
/* Developer host register capture; compile with lt_chip.c/lt_sms_boot.c. */
#include <stdio.h>
#include "lt_sms_boot.h"
static lt_chip chip;
static lt_sms_boot effect;
static unsigned frame, capturing;
static void emit(void *context, lt_u8 byte)
{
    (void)context;
    if (capturing) printf("%u,%u\n", frame, (unsigned)byte);
}
int main(void)
{
    lt_init(&chip, emit, 0);
    if (!lt_sms_boot_init(&effect, &chip, 1U) ||
        !lt_sms_boot_start(&effect)) return 1;
    capturing = 1U;
    puts("tick,byte");
    while (effect.active) {
        if (!lt_sms_boot_tick(&effect)) return 1;
        ++frame;
    }
    return 0;
}
