/* SPDX-License-Identifier: MIT */
#ifndef LT_TIME_H
#define LT_TIME_H
#include "lt_surface.h"
#define LTT_DAY_TICKS 0x1800b0UL
#define LTT_MAX_TICKS 0xffffffffUL
typedef struct {
 unsigned long last,elapsed;
 lts_u16 initialized,resets;
} ltt_clock;
typedef enum {LTT_OK=0,LTT_INVALID=1,LTT_RESET=2,LTT_SATURATED=3,
 LTT_UNSUPPORTED=4,LTT_CONTEXT=5,LTT_STALLED=6,LTT_CANCELLED=7} ltt_status;
void ltt_init(ltt_clock *clock);
/* BIOS ticks, not milliseconds. Sample at least once per12h. Midnight is
 * modular; backwards/large discontinuities rebase without decreasing elapsed.
 * Saturation at32-bit maximum is sticky. No BDA reads/writes in this core. */
ltt_status ltt_sample(ltt_clock *clock,unsigned long raw,unsigned long *elapsed);
#endif
