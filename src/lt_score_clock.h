/* SPDX-License-Identifier: MIT */
#ifndef LT_SCORE_CLOCK_H
#define LT_SCORE_CLOCK_H

/* Internal helper shared with score_core so 32-bit DOS rounding is tested
 * even when the host's native unsigned long is wider than 32 bits. */
#include "lt_audio.h"

static int ltscore_ticks_per_second(lta_u32 ppq, lta_u32 tempo_us,
                                    lta_u32 *ticks_per_second)
{
    lta_u32 numerator, quotient, remainder, round_at;
    if (!tempo_us || !ticks_per_second) return 0;

    numerator = ppq * (lta_u32)1000000UL;
    quotient = numerator / tempo_us;
    remainder = numerator % tempo_us;

    /* Round half up without adding half the denominator to the numerator.
     * That addition can overflow 32-bit unsigned long on DOS. */
    round_at = tempo_us / (lta_u32)2U + (tempo_us & (lta_u32)1U);
    if (remainder >= round_at) ++quotient;
    *ticks_per_second = quotient;
    return 1;
}

#endif
