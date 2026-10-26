/* SPDX-License-Identifier: MIT */
#include "lt_sms_boot.h"
#include "lt_sms_boot_data.h"

static int owns_all(const lt_sms_boot *p)
{
    unsigned i;
    for (i = 0; i < 4U; ++i)
        if (p->chip->owner[i] != p->token) return 0;
    return 1;
}

int lt_sms_boot_init(lt_sms_boot *p, lt_chip *chip, lt_u8 token)
{
    if (!p || !chip || !chip->emit || !token) return 0;
    p->chip = chip;
    p->token = token;
    p->tick = p->cursor = 0U;
    p->active = 0U;
    return 1;
}

int lt_sms_boot_start(lt_sms_boot *p)
{
    unsigned i;
    if (!p || !p->chip || !p->token || p->active) return 0;
    if (!lt_claim(p->chip, p->token, 15U)) return 0;
    for (i = 0; i < 4U; ++i)
        lt_attenuate(p->chip, p->token, (lt_u8)i, 15U, 1);
    p->tick = p->cursor = 0U;
    p->active = 1U;
    return 1;
}

int lt_sms_boot_stop(lt_sms_boot *p)
{
    if (!p || !p->chip) return 0;
    if (!p->active) return 1;
    if (!lt_stop(p->chip, p->token, 15U)) return 0;
    p->active = 0U;
    return 1;
}

int lt_sms_boot_tick(lt_sms_boot *p)
{
    lt_u16 at;
    unsigned count;
    if (!p || !p->chip) return 0;
    if (!p->active) return 1;
    if (!owns_all(p)) return 0;
    if (p->tick >= LTSB_END_TICK) return lt_sms_boot_stop(p);
    /* Fixed authored table: at most one group (12 writes) per update.
     * Each raw latch/data write is preserved, including noise resets.
     * Invalidate the chip cache so subsequent low-level users re-emit state.
     */
    if (p->cursor < sizeof(sms_boot_data)) {
        at = (lt_u16)(sms_boot_data[p->cursor] |
                      ((lt_u16)sms_boot_data[p->cursor + 1U] << 8));
        if (at == p->tick) {
            p->cursor += 2U;
            count = sms_boot_data[p->cursor++];
            while (count--)
                p->chip->emit(p->chip->context,
                              sms_boot_data[p->cursor++]);
            p->chip->tone_valid = 0U;
            p->chip->attenuation_valid = 0U;
            p->chip->noise_valid = 0U;
        }
    }
    ++p->tick;
    return 1;
}
