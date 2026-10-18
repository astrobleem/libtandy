/* SPDX-License-Identifier: MIT */
#ifndef LT_TIME_DOS_H
#define LT_TIME_DOS_H
#include "lt_time.h"
/* Caller asserts IRQ0 owner maintains standard BIOS18.2Hz day ticks. Unknown
 * or incompatible timing returns UNSUPPORTED. Foreground real DOS only. */
ltt_status ltd_clock_init(ltt_clock *clock,int bios_tick_compatible);
unsigned long ltd_raw_ticks(void);
ltt_status ltd_now(ltt_clock *clock,unsigned long *elapsed);
/* Coarse minimum delay: rounds up plus one phase tick;0 returns immediately.
 * Callback runs foreground, may service existing sound/input, nonzero cancels.
 * Requires IF=1 and compatible BIOS cadence; never programs PIT or calls1Ah.
 * Consecutive65535 unchanged polls return STALLED (target8088 profile). */
ltt_status ltd_delay_ms(ltt_clock *clock,lts_u16 ms,
                       int (*cooperate)(void *),void *context);
#endif
