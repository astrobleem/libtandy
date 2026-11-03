/* SPDX-License-Identifier: MIT */
#include "lt_ay.h"
#include <string.h>

/* Rounded SN 2 dB steps from AY DAC reference levels. Very quiet nonzero
 * values saturate at the SN floor, not silence. This is AY, not YM's DAC. */
static const lt_u8 ay_attenuation[16] = {
    15,14,14,14,14,13,12,10,9,7,5,4,3,2,1,0
};
void lt_ay_config_init(lt_ay_config *c)
{
    if (!c) return;
    c->scale_num = c->scale_den = 1;
    c->flags = LTAY_COUPLED_NOISE;
    c->range_policy = LTAY_RANGE_FOLD;
}
static lta_u32 scale(const lt_ay_config *c, unsigned period)
{
    lta_u32 p;
    if (!period) period = 1;
    if (c->scale_num == c->scale_den) return (lta_u32)period;
    p = ((lta_u32)period * c->scale_num + c->scale_den / 2U) / c->scale_den;
    return p ? p : 1UL;
}
static lt_u16 fit(const lt_ay_config *c, lta_u32 p, lt_u8 *level,
                  lt_u16 *losses)
{
    if (p > 1023UL) {
        *losses |= LTAY_LOSS_RANGE;
        if (c->range_policy == LTAY_RANGE_MUTE) { *level = 15; return 1; }
        if (c->range_policy == LTAY_RANGE_CLAMP) return 1023;
        /* Maximum scaled AY period is 4095*65535: at most 19 halvings. */
        while (p > 1023UL) p = (p >> 1) + (p & 1UL);
    }
    return (lt_u16)p;
}
lta_status lt_ay_map(const lt_ay_config *c, const lt_ay_frame *f,
                     lt_ay_plan *out)
{
    lt_ay_plan p;
    lta_u32 period[3], low, noise_period;
    lt_u8 level[3], audible[3], noise_level, bass, coupled;
    unsigned i, count, noise_count, slot;
    if (!c || !f || !out || !c->scale_num || !c->scale_den ||
        c->flags > (LTAY_COUPLED_NOISE | LTAY_PERIODIC_BASS) ||
        c->range_policy > LTAY_RANGE_MUTE || f->envelope_valid > 1 ||
        (f->envelope_valid && f->envelope_level > 15)) return LTA_INVALID;
    if (((f->reg[8] | f->reg[9] | f->reg[10]) & 16) && !f->envelope_valid)
        return LTA_UNSUPPORTED;
    memset(&p, 0, sizeof p);
    for (i = 0; i < 3; ++i) p.period[i] = 1;
    for (i = 0; i < 4; ++i) p.attenuation[i] = 15;
    p.noise_mode = 4;
    count = noise_count = 0; low = 0; bass = 255; noise_level = 15;
    for (i = 0; i < 3; ++i) {
        unsigned tone = !(f->reg[7] & (1U << i));
        unsigned noise = !(f->reg[7] & (8U << i));
        unsigned env = f->reg[8+i] & 16;
        unsigned value = env ? f->envelope_level : f->reg[8+i] & 15;
        period[i] = scale(c, f->reg[2*i] | ((unsigned)(f->reg[2*i+1] & 15) << 8));
        level[i] = ay_attenuation[value];
        if (value && value != 15) p.losses |= LTAY_LOSS_VOLUME;
        if (env) {
            p.losses |= LTAY_LOSS_ENVELOPE;
            if (!tone && !noise) p.losses |= LTAY_LOSS_ENVELOPE_ONLY;
        }
        if (tone && noise && level[i] < 15) {
            p.losses |= LTAY_LOSS_MIXER;
            level[i] = (lt_u8)(level[i] >= 12 ? 15 : level[i] + 3);
        }
        audible[i] = (lt_u8)(tone && level[i] < 15);
        if (noise && level[i] < 15) {
            ++noise_count;
            if (level[i] < noise_level) noise_level = level[i];
        }
        if (audible[i]) {
            ++count;
            if (period[i] > 1023UL && period[i] > low) {
                low = period[i]; bass = (lt_u8)i;
            }
        }
    }
    noise_period = scale(c, f->reg[6] & 31);
    if (noise_count > 1) p.losses |= LTAY_LOSS_MIXER;
    coupled = 0;
    if (noise_level < 15) {
        p.losses |= LTAY_LOSS_NOISE;
        bass = 255; p.attenuation[3] = noise_level;
        if ((c->flags & LTAY_COUPLED_NOISE) && count <= 2 && noise_period <= 1023UL) {
            coupled = 1; p.noise_mode = 7; p.period[2] = (lt_u16)noise_period;
        } else {
            p.losses |= LTAY_LOSS_FIXED_NOISE;
            p.noise_mode = (lt_u8)(noise_period < 23UL ? 4 : noise_period < 46UL ? 5 : 6);
        }
    } else if ((c->flags & LTAY_PERIODIC_BASS) && bass != 255) {
        /* Tandy NCR periodic noise has a 15-step ring. This is an optional
         * pulse waveform, not an AY square or a portable SN-chip assumption. */
        p.losses |= LTAY_LOSS_PULSE_BASS;
        coupled = 1; p.noise_mode = 3;
        p.attenuation[3] = level[bass] >= 3 ? (lt_u8)(level[bass]-3) : 0;
        p.period[2] = fit(c, (low + 7UL) / 15UL, &p.attenuation[3], &p.losses);
    } else bass = 255;
    slot = 0;
    for (i = 0; i < 3; ++i) if (audible[i] && i != bass) {
        unsigned dest = coupled ? slot++ : i;
        p.attenuation[dest] = level[i];
        p.period[dest] = fit(c, period[i], &p.attenuation[dest], &p.losses);
    }
    *out = p;
    return LTA_OK;
}
static lta_status set_level(lta_handle *h, unsigned channel, lt_u8 level)
{
    lta_request r;
    lta_request_init(&r); r.op = LTA_OP_LEVEL;
    r.channel = (lt_u8)channel; r.level = level;
    return lta_command(h, &r);
}
lta_status lt_ay_apply(lta_handle *h, const lt_ay_plan *p)
{
    unsigned i;
    lta_request r;
    lta_status s;
    if (!h || !p || !h->chip || !h->chip->emit || !h->token ||
        p->noise_mode > 7) return LTA_INVALID;
    for (i = 0; i < 3; ++i)
        if (!p->period[i] || p->period[i] > 1023) return LTA_INVALID;
    for (i = 0; i < 4; ++i) if (p->attenuation[i] > 15) return LTA_INVALID;
    if ((p->noise_mode & 3) == 3 && p->attenuation[2] != 15) return LTA_INVALID;
    if (h->active != 1) return LTA_STATE;
    if (h->mask != 15) return LTA_OWNER;
    for (i = 0; i < 4; ++i) if (h->chip->owner[i] != h->token) return LTA_OWNER;
    if (h->deadline_mask) return LTA_BUSY;
    if ((p->noise_mode & 3) == 3 && (h->playing & 4) && h->level[2] != 15) {
        s = set_level(h, 2, 15); if (s != LTA_OK) return s;
    }
    if ((h->playing & 8) && h->noise_mode != p->noise_mode && h->level[3] != 15) {
        s = set_level(h, 3, 15); if (s != LTA_OK) return s;
    }
    for (i = 0; i < 3; ++i) {
        unsigned start = !(h->playing & (1U << i)) || h->kind[i] != LTA_KIND_TONE;
        if (!start && h->period[i] == p->period[i]) {
            if (h->level[i] != p->attenuation[i]) {
                s = set_level(h, i, p->attenuation[i]); if (s != LTA_OK) return s;
            }
            continue;
        }
        lta_request_init(&r); r.op = start ? LTA_OP_TONE_START : LTA_OP_TONE_UPDATE;
        r.channel = (lt_u8)i; r.period = p->period[i]; r.level = p->attenuation[i];
        s = lta_command(h, &r); if (s != LTA_OK) return s;
    }
    if (!(h->playing & 8) || h->kind[3] != LTA_KIND_NOISE || h->noise_mode != p->noise_mode) {
        lta_request_init(&r);
        r.op = !(h->playing & 8) || h->kind[3] != LTA_KIND_NOISE ? LTA_OP_NOISE_START : LTA_OP_NOISE_UPDATE;
        r.mode = p->noise_mode; r.level = p->attenuation[3];
        return lta_command(h, &r);
    }
    if (h->level[3] != p->attenuation[3]) return set_level(h, 3, p->attenuation[3]);
    return LTA_OK;
}
