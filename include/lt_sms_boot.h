/* SPDX-License-Identifier: MIT */
#ifndef LT_SMS_BOOT_H
#define LT_SMS_BOOT_H
#include "lt_sound.h"

/* Optional US/European SMS BIOS v1.3 Sega Tune, from Chad's OutRun SE.
 * Raw PSG effect, not an LTSFX short clip. No heap, OS, timer or IRQ.
 * Use a unique nonzero owner token. All four channels must be exclusive; do not share with an lta_handle.
 * Serialize calls. Keep chip/player alive and do not copy an active player.
 * Call tick once per 1193182/4972 Hz update (nominal 240 Hz). Tick zero
 * is emitted on the first call. Pausing service pauses the effect.
 */
#define LTSB_PIT_DIVISOR 4972U
#define LTSB_RATE_NUM 596591UL
#define LTSB_RATE_DEN 2486UL
#define LTSB_FINAL_TICK 481U
#define LTSB_END_TICK 805U
#define LTSB_PSG_WRITES 710U

typedef struct {
    lt_chip *chip;
    lt_u16 tick, cursor;
    lt_u8 token, active;
} lt_sms_boot;

/* All functions return 1 on success, 0 on invalid arguments/ownership.
 * Init is silent. Start claims all voices atomically, then mutes them.
 * Stop/normal completion mute before releasing; stop is idle-safe.
 * Start on an active player fails without retriggering; stop first.
 * No music restoration: reacquire/restart your music after this effect.
 */
int lt_sms_boot_init(lt_sms_boot *player, lt_chip *chip, lt_u8 token);
int lt_sms_boot_start(lt_sms_boot *player);
int lt_sms_boot_tick(lt_sms_boot *player);
int lt_sms_boot_stop(lt_sms_boot *player);
#endif
