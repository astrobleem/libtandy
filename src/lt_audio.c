/* SPDX-License-Identifier: MIT */
#include "lt_audio.h"

#define LTA_VALID_MASK 15U
#define LTA_TONE_BITS 7U
#define LTA_U32_HALF 0x80000000UL

static lt_u8 output_level(const lta_handle *h, unsigned channel,
                           lt_u8 level, int effect)
{
    unsigned sum = (unsigned)level + (effect ? h->sfx_attenuation :
                                             h->music_attenuation[channel]);
    return (lt_u8)((h->muted || (effect ? h->effect_paused : h->music_paused) || sum > 15U) ? 15U : sum);
}

static int lta_mask_valid(lt_u8 mask)
{
    return mask != 0U && (mask & 240U) == 0U;
}

static int lta_owns_mask(const lta_handle *h, lt_u8 mask)
{
    unsigned i;
    if (!h->active || !h->chip || !h->chip->emit ||
        !lta_mask_valid(mask) || (mask & (lt_u8)~h->mask) != 0U)
        return 0;
    for (i = 0; i < 4U; ++i)
        if ((h->mask & (1U << i)) && h->chip->owner[i] != h->token)
            return 0;
    return 1;
}

static lta_status lta_preflight_active(lta_handle *h)
{
    if (!h || !h->chip || !h->token || h->active > 1U ||
        h->muted > 1U || h->music_paused > 1U || h->effect_paused > 1U || (h->mask & 240U) != 0U ||
        (h->playing & (lt_u8)~h->mask) != 0U ||
        (h->deadline_mask & (lt_u8)~h->mask) != 0U ||
        h->effect_active > 1U || (h->effect_mask & 240U) != 0U ||
        (h->effect_playing & (lt_u8)~h->effect_mask) != 0U ||
        (h->effect_mask & (lt_u8)~h->mask) != 0U ||
        (!h->effect_active && (h->effect_mask || h->effect_playing)))
        return LTA_INVALID;
    if (!h->active) return LTA_STATE;
    if (!lta_owns_mask(h, h->mask)) return LTA_OWNER;
    return LTA_OK;
}

static void lta_cancel_deadlines(lta_handle *h, lt_u8 mask)
{
    unsigned i;
    h->deadline_mask &= (lt_u8)~mask;
    for (i = 0; i < 4U; ++i)
        if (mask & (1U << i)) h->deadline[i] = 0UL;
}

static lt_u8 lta_stop_expand(const lta_handle *h, lt_u8 mask)
{
    if (h->effect_active) {
        if ((h->playing & 8U) && h->kind[3] == LTA_KIND_NOISE &&
            (h->noise_mode & 3U) == 3U && (mask & LTA_TONE2_NOISE))
            mask |= LTA_TONE2_NOISE;
    } else if (h->chip->noise_valid && (h->chip->noise & 3U) == 3U &&
               h->chip->owner[3] == h->token &&
               (mask & LTA_TONE2_NOISE)) {
        mask |= LTA_TONE2_NOISE;
    }
    return mask;
}

static lta_status lta_stop_preflight(const lta_handle *h, lt_u8 mask,
                                     lt_u8 *expanded_out)
{
    unsigned i;
    lt_u8 expanded;
    expanded = lta_stop_expand(h, mask);
    if (!lta_mask_valid(expanded) || (expanded & (lt_u8)~h->mask) != 0U)
        return LTA_OWNER;
    for (i = 0; i < 4U; ++i) {
        lt_u8 bit = (lt_u8)(1U << i);
        if ((expanded & bit) && h->chip->owner[i] != h->token)
            return LTA_OWNER;
    }
    if (expanded_out) *expanded_out = expanded;
    return LTA_OK;
}

static void lta_stop_apply(lta_handle *h, lt_u8 expanded)
{
    unsigned i;
    /* The complete mask was checked before any output. Keep ownership. */
    for (i = 0; i < 4U; ++i) {
        lt_u8 bit = (lt_u8)(1U << i);
        /* A music stop updates its saved state but leaves the effect alone. */
        if ((expanded & bit) &&
            !(h->effect_active && (h->effect_mask & bit)))
            (void)lt_attenuate(h->chip, h->token, (lt_u8)i, 15U, 1);
    }
    h->playing &= (lt_u8)~expanded;
    for (i = 0; i < 3U; ++i)
        if (expanded & (1U << i)) {
            h->kind[i] = LTA_KIND_NONE;
            if (!(h->effect_active && (h->effect_mask & (1U << i))))
                h->chip->tone_valid &= (lt_u8)~(1U << i);
        }
    if (expanded & 8U) {
        h->kind[3] = LTA_KIND_NONE;
        if (!(h->effect_active && (h->effect_mask & 8U)))
            h->chip->noise_valid = 0U;
    }
    lta_cancel_deadlines(h, expanded);
}

static lta_status lta_stop_internal(lta_handle *h, lt_u8 mask)
{
    lt_u8 expanded;
    lta_status status = lta_stop_preflight(h, mask, &expanded);
    if (status != LTA_OK) return status;
    lta_stop_apply(h, expanded);
    return LTA_OK;
}

static lta_status lta_acquire(lta_handle *h, const lta_request *r)
{
    unsigned i;
    lta_handle next;
    if (!lta_mask_valid(r->mask)) return LTA_INVALID;
    if (h->active) return LTA_BUSY;
    if (!h->chip->emit) return LTA_INVALID;
    for (i = 0; i < 4U; ++i)
        if ((r->mask & (1U << i)) && h->chip->owner[i] != 0U)
            return LTA_BUSY;
    if (!lt_claim(h->chip, h->token, r->mask)) return LTA_BUSY;
    for (i = 0; i < 4U; ++i)
        if ((r->mask & (1U << i)) &&
            !lt_attenuate(h->chip, h->token, (lt_u8)i, 15U, 1)) {
            /* With exclusive foreground ownership, this is unreachable. */
            return LTA_OWNER;
        }
    next = *h;
    next.mask = r->mask;
    next.active = 1U;
    next.muted = 0U;
    next.playing = 0U;
    next.clock_valid = 0U;
    next.deadline_mask = 0U;
    next.noise_mode = 0U;
    next.effect_mask = 0U;
    next.effect_active = 0U;
    next.effect_playing = 0U;
    next.effect_noise_mode = 0U;
    next.sfx_attenuation = 0U;
    next.music_paused = next.effect_paused = 0U;
    for (i = 0; i < 4U; ++i) {
        next.kind[i] = LTA_KIND_NONE;
        next.level[i] = 15U;
        next.music_attenuation[i] = 0U;
        next.effect_kind[i] = LTA_KIND_NONE;
        next.effect_level[i] = 15U;
        next.deadline[i] = 0UL;
    }
    for (i = 0; i < 3U; ++i) next.period[i] = 0U;
    *h = next;
    return LTA_OK;
}

static lta_status lta_release(lta_handle *h)
{
    unsigned i;
    lt_u8 mask = h->mask;
    if (!lta_owns_mask(h, mask)) return LTA_OWNER;
    /* lt_stop preflights its complete (possibly coupled) expansion. */
    if (!lt_stop(h->chip, h->token, mask)) return LTA_OWNER;
    h->active = 0U;
    h->music_paused = 0U;
    h->mask = 0U;
    h->playing = 0U;
    h->muted = 0U;
    h->deadline_mask = 0U;
    h->clock_valid = 0U;
    h->noise_mode = 0U;
    h->effect_mask = 0U;
    h->effect_active = 0U;
    h->effect_playing = 0U;
    h->effect_noise_mode = 0U;
    h->effect_paused = 0U;
    for (i = 0; i < 4U; ++i) {
        h->kind[i] = LTA_KIND_NONE;
        h->level[i] = 15U;
        h->effect_kind[i] = LTA_KIND_NONE;
        h->effect_level[i] = 15U;
        h->deadline[i] = 0UL;
    }
    return LTA_OK;
}

static lta_status lta_tone(lta_handle *h, const lta_request *r, int start)
{
    lt_u8 bit, attenuation;
    if (r->channel >= 3U || r->period > LTA_MAX_PERIOD || r->level > 15U)
        return LTA_INVALID;
    bit = (lt_u8)(1U << r->channel);
    if (!(h->mask & bit)) return LTA_OWNER;
    if (!start && (!(h->playing & bit) || h->kind[r->channel] != LTA_KIND_TONE))
        return LTA_STATE;
    attenuation = output_level(h, r->channel, r->level, 0);
    /* Store music updates in h; the effect owns hardware until it ends. */
    if (!(h->effect_active && (h->effect_mask & bit))) {
        if (!lt_tone(h->chip, h->token, r->channel, r->period, start))
            return LTA_OWNER;
        if (!lt_attenuate(h->chip, h->token, r->channel, attenuation, start))
            return LTA_OWNER;
    }
    h->period[r->channel] = r->period;
    h->level[r->channel] = r->level;
    h->kind[r->channel] = LTA_KIND_TONE;
    h->playing |= bit;
    if (start) lta_cancel_deadlines(h, bit);
    return LTA_OK;
}

static lta_status lta_noise(lta_handle *h, const lta_request *r, int start)
{
    lt_u8 attenuation;
    if (r->mode > 7U || r->level > 15U) return LTA_INVALID;
    if (!(h->mask & 8U)) return LTA_OWNER;
    if (!start && (!(h->playing & 8U) || h->kind[3] != LTA_KIND_NOISE))
        return LTA_STATE;
    attenuation = output_level(h, 3U, r->level, 0);
    if ((r->mode & 3U) == 3U) {
        if (!(h->mask & 4U)) return LTA_STATE;
        if (h->effect_active && (h->effect_mask & 4U)) {
            if (!(h->playing & 4U) || h->kind[2] != LTA_KIND_TONE)
                return LTA_STATE;
        } else if (!(h->chip->tone_valid & 4U)) {
            return LTA_STATE;
        }
    }
    /* Noise is saved the same way while its channel is borrowed. */
    if (!(h->effect_active && (h->effect_mask & 8U))) {
        if (!lt_noise(h->chip, h->token, r->mode, start)) return LTA_OWNER;
        if (!lt_attenuate(h->chip, h->token, 3U, attenuation, start))
            return LTA_OWNER;
    }
    h->noise_mode = r->mode;
    h->level[3] = r->level;
    h->kind[3] = LTA_KIND_NOISE;
    h->playing |= 8U;
    if (start) lta_cancel_deadlines(h, 8U);
    return LTA_OK;
}

/* Change a voice level without rewriting its period or noise mode. Music
 * envelope steps use this path, and they still update the saved music level
 * when an effect currently owns that channel. */
static lta_status lta_level(lta_handle *h, const lta_request *r)
{
    lt_u8 bit, attenuation;
    if (r->channel >= 4U || r->level > 15U) return LTA_INVALID;
    bit = (lt_u8)(1U << r->channel);
    if (!(h->mask & bit)) return LTA_OWNER;
    attenuation = output_level(h, r->channel, r->level, 0);
    if (!(h->effect_active && (h->effect_mask & bit))) {
        if (!lt_attenuate(h->chip, h->token, r->channel,
                          attenuation, 1)) return LTA_OWNER;
    }
    h->level[r->channel] = r->level;
    return LTA_OK;
}

/* Configure noise separately from its volume. Fixed-white-noise scores use
 * this to match the PSG's one-byte control write. Rate 3 still requires a
 * live tone-2 clock, including while a foreground effect borrows the pair. */
static lta_status lta_noise_mode(lta_handle *h, const lta_request *r)
{
    if (r->mode > 7U) return LTA_INVALID;
    if (!(h->mask & 8U)) return LTA_OWNER;
    if ((r->mode & 3U) == 3U) {
        if (!(h->mask & 4U) || !(h->playing & 4U) ||
            h->kind[2] != LTA_KIND_TONE) return LTA_STATE;
    }
    if (!(h->effect_active && (h->effect_mask & 8U))) {
        if (!lt_noise(h->chip, h->token, r->mode, 1)) return LTA_OWNER;
    }
    h->noise_mode = r->mode;
    h->kind[3] = LTA_KIND_NOISE;
    h->playing |= 8U;
    lta_cancel_deadlines(h, 8U);
    return LTA_OK;
}

static lta_status refresh_volumes(lta_handle *h)
{
    unsigned i;
    for (i = 0; i < 4U; ++i) {
        lt_u8 bit = (lt_u8)(1U << i), attenuation = 15U;
        if (!(h->mask & bit)) continue;
        if (h->effect_mask & bit) {
            if (h->effect_playing & bit)
                attenuation = output_level(h, i, h->effect_level[i], 1);
        } else if (h->playing & bit) {
            attenuation = output_level(h, i, h->level[i], 0);
        }
        if (!lt_attenuate(h->chip, h->token, (lt_u8)i, attenuation, 1))
            return LTA_OWNER;
    }
    return LTA_OK;
}

static lta_status lta_mute(lta_handle *h, lt_u8 enabled)
{
    if (enabled > 1U) return LTA_INVALID;
    if (h->muted == enabled) return LTA_OK;
    h->muted = enabled;
    return refresh_volumes(h);
}

/* Reserve channels for one short foreground effect. Tone 3 and noise share
 * generator state, so borrowing either one also reserves the pair when the
 * music handle owns both voices. */
lta_status lta_overlay_begin(lta_handle *h, lt_u8 mask,
                             lt_u8 *borrowed_mask)
{
    lta_status status;
    unsigned i;
    if (!h || !borrowed_mask || !lta_mask_valid(mask)) return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    if (h->effect_active) return LTA_BUSY;
    if ((mask & (lt_u8)~h->mask) != 0U) return LTA_OWNER;
    if ((mask & LTA_TONE2_NOISE) &&
        (h->mask & LTA_TONE2_NOISE) == LTA_TONE2_NOISE)
        mask |= LTA_TONE2_NOISE;
    if (h->chip->noise_valid && (h->chip->noise & 3U) == 3U &&
        h->chip->owner[3] == h->token &&
        (mask & LTA_TONE2_NOISE))
        mask |= LTA_TONE2_NOISE;
    if ((mask & (lt_u8)~h->mask) != 0U) return LTA_OWNER;
    if (!lta_owns_mask(h, mask)) return LTA_OWNER;

    h->effect_mask = mask;
    h->effect_active = 1U;
    h->effect_playing = 0U;
    h->effect_noise_mode = 0U;
    h->effect_paused = 0U;
    for (i = 0; i < 4U; ++i) {
        h->effect_kind[i] = LTA_KIND_NONE;
        h->effect_level[i] = 15U;
        if (mask & (1U << i))
            (void)lt_attenuate(h->chip, h->token, (lt_u8)i, 15U, 1);
    }
    *borrowed_mask = mask;
    return LTA_OK;
}

lta_status lta_overlay_tone(lta_handle *h, lt_u8 channel,
                            lt_u16 period, lt_u8 level)
{
    lta_status status;
    lt_u8 bit, start;
    if (!h || channel >= 3U || period > LTA_MAX_PERIOD || level > 15U)
        return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    if (!h->effect_active) return LTA_STATE;
    bit = (lt_u8)(1U << channel);
    if (!(h->effect_mask & bit)) return LTA_OWNER;
    start = (lt_u8)(!(h->effect_playing & bit) ||
                    h->effect_kind[channel] != LTA_KIND_TONE);
    if (!lt_tone(h->chip, h->token, channel, period, start))
        return LTA_OWNER;
    if (!lt_attenuate(h->chip, h->token, channel,
                      output_level(h, channel, level, 1),
                      start || h->effect_level[channel] != level))
        return LTA_OWNER;
    h->effect_kind[channel] = LTA_KIND_TONE;
    h->effect_level[channel] = level;
    h->effect_playing |= bit;
    return LTA_OK;
}

lta_status lta_overlay_noise(lta_handle *h, lt_u8 mode, lt_u8 level)
{
    lta_status status;
    lt_u8 start;
    if (!h || mode > 7U || level > 15U) return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    if (!h->effect_active) return LTA_STATE;
    if (!(h->effect_mask & 8U)) return LTA_OWNER;
    if ((mode & 3U) == 3U &&
        ((h->effect_mask & LTA_TONE2_NOISE) != LTA_TONE2_NOISE ||
         !(h->effect_playing & 4U) ||
         h->effect_kind[2] != LTA_KIND_TONE ||
         !(h->chip->tone_valid & 4U))) return LTA_STATE;
    start = (lt_u8)(!(h->effect_playing & 8U) ||
                    h->effect_kind[3] != LTA_KIND_NOISE);
    if (!lt_noise(h->chip, h->token, mode, start)) return LTA_OWNER;
    if (!lt_attenuate(h->chip, h->token, 3U,
                      output_level(h, 3U, level, 1),
                      start || h->effect_level[3] != level))
        return LTA_OWNER;
    h->effect_kind[3] = LTA_KIND_NOISE;
    h->effect_level[3] = level;
    h->effect_noise_mode = mode;
    h->effect_playing |= 8U;
    return LTA_OK;
}

lta_status lta_overlay_stop(lta_handle *h, lt_u8 mask)
{
    lta_status status;
    unsigned i;
    if (!h || !lta_mask_valid(mask)) return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    if (!h->effect_active) return LTA_STATE;
    if ((mask & (lt_u8)~h->effect_mask) != 0U) return LTA_OWNER;
    if ((h->effect_noise_mode & 3U) == 3U &&
        (mask & LTA_TONE2_NOISE)) mask |= LTA_TONE2_NOISE;
    if ((mask & (lt_u8)~h->effect_mask) != 0U) return LTA_OWNER;
    for (i = 0; i < 4U; ++i) {
        lt_u8 bit = (lt_u8)(1U << i);
        if (mask & bit) {
            (void)lt_attenuate(h->chip, h->token, (lt_u8)i, 15U, 1);
            h->effect_playing &= (lt_u8)~bit;
            h->effect_kind[i] = LTA_KIND_NONE;
            if (i < 3U) h->chip->tone_valid &= (lt_u8)~bit;
            else h->chip->noise_valid = 0U;
        }
    }
    if (mask & 8U) h->effect_noise_mode = 0U;
    return LTA_OK;
}

lta_status lta_overlay_end(lta_handle *h)
{
    lta_status status;
    unsigned i;
    lt_u8 mask;
    if (!h) return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    if (!h->effect_active) return LTA_STATE;
    mask = h->effect_mask;

    /* Silence the borrowed output before rebuilding the latest music state. */
    for (i = 0; i < 4U; ++i)
        if (mask & (1U << i))
            (void)lt_attenuate(h->chip, h->token, (lt_u8)i, 15U, 1);
    for (i = 0; i < 3U; ++i)
        if (mask & (1U << i)) h->chip->tone_valid &= (lt_u8)~(1U << i);
    if (mask & 8U) h->chip->noise_valid = 0U;

    h->effect_active = 0U;
    h->effect_mask = 0U;
    h->effect_playing = 0U;
    h->effect_noise_mode = 0U;
    h->effect_paused = 0U;
    for (i = 0; i < 4U; ++i) {
        h->effect_kind[i] = LTA_KIND_NONE;
        h->effect_level[i] = 15U;
    }

    /* Tone 2 is restored before noise so mode 3 has a valid clock source. */
    for (i = 0; i < 3U; ++i) {
        lt_u8 bit = (lt_u8)(1U << i);
        if (!(mask & bit)) continue;
        if ((h->playing & bit) && h->kind[i] == LTA_KIND_TONE) {
            (void)lt_tone(h->chip, h->token, (lt_u8)i, h->period[i], 1);
            (void)lt_attenuate(h->chip, h->token, (lt_u8)i,
                               output_level(h, i, h->level[i], 0), 1);
        } else {
            h->chip->tone_valid &= (lt_u8)~bit;
        }
    }
    if (mask & 8U) {
        if ((h->playing & 8U) && h->kind[3] == LTA_KIND_NOISE) {
            (void)lt_noise(h->chip, h->token, h->noise_mode, 1);
            (void)lt_attenuate(h->chip, h->token, 3U,
                               output_level(h, 3U, h->level[3], 0), 1);
        } else {
            h->chip->noise_valid = 0U;
        }
    }
    return LTA_OK;
}

static lta_status lta_arm_stop(lta_handle *h, const lta_request *r)
{
    unsigned i;
    if (!lta_mask_valid(r->mask) || r->value >= LTA_U32_HALF)
        return LTA_INVALID;
    if ((r->mask & (lt_u8)~h->mask) != 0U) return LTA_OWNER;
    if (!h->clock_valid || (r->mask & (lt_u8)~h->playing) != 0U)
        return LTA_STATE;
    if (r->value == 0UL) return lta_stop_internal(h, r->mask);
    for (i = 0; i < 4U; ++i)
        if (r->mask & (1U << i)) h->deadline[i] = h->last_now + r->value;
    h->deadline_mask |= r->mask;
    return LTA_OK;
}

static lta_status lta_service(lta_handle *h, const lta_request *r)
{
    unsigned i;
    lta_u32 now = r->value;
    lt_u8 due = 0U;
    lt_u8 expanded = 0U;
    if (h->clock_valid) {
        lta_u32 delta = now - h->last_now;
        if (delta >= LTA_U32_HALF) return LTA_TIME;
    }
    for (i = 0; i < 4U; ++i)
        if ((h->deadline_mask & (1U << i)) &&
            (lta_u32)(now - h->deadline[i]) < LTA_U32_HALF)
            due |= (lt_u8)(1U << i);
    if (due) {
        lta_status status = lta_stop_preflight(h, due, &expanded);
        if (status != LTA_OK) return status;
    }
    h->clock_valid = 1U;
    h->last_now = now;
    if (due) lta_stop_apply(h, expanded);
    return LTA_OK;
}

lta_status lta_init(lta_handle *handle, lt_chip *chip, lt_u8 token)
{
    lta_handle next;
    if (!handle || !chip || !chip->emit || token == 0U) return LTA_INVALID;
    next.chip = chip;
    next.token = token;
    next.mask = 0U;
    next.active = 0U;
    next.muted = 0U;
    next.playing = 0U;
    next.clock_valid = 0U;
    next.deadline_mask = 0U;
    next.noise_mode = 0U;
    next.effect_mask = 0U;
    next.effect_active = 0U;
    next.effect_playing = 0U;
    next.effect_noise_mode = 0U;
    next.sfx_attenuation = 0U;
    next.music_paused = next.effect_paused = 0U;
    next.last_now = 0UL;
    {
        unsigned i;
        for (i = 0; i < 4U; ++i) {
            next.kind[i] = LTA_KIND_NONE;
            next.level[i] = 15U;
            next.music_attenuation[i] = 0U;
            next.effect_kind[i] = LTA_KIND_NONE;
            next.effect_level[i] = 15U;
            next.deadline[i] = 0UL;
        }
        for (i = 0; i < 3U; ++i) next.period[i] = 0U;
    }
    *handle = next;
    return LTA_OK;
}

void lta_request_init(lta_request *request)
{
    if (!request) return;
    request->op = 0U;
    request->mask = 0U;
    request->channel = 0U;
    request->level = 0U;
    request->mode = 0U;
    request->enabled = 0U;
    request->period = 0U;
    request->value = 0UL;
}

lt_u16 lta_capabilities(void)
{
    return LTA_CAP_TONE | LTA_CAP_NOISE | LTA_CAP_MUTE | LTA_CAP_AUTO_STOP;
}

lta_status lta_command(lta_handle *h, const lta_request *r)
{
    lta_status status;
    if (!h || !r) return LTA_INVALID;
    if (r->op == LTA_OP_PCM || r->op < LTA_OP_ACQUIRE ||
        r->op > LTA_OP_NOISE_MODE)
        return LTA_UNSUPPORTED;
    if (!h->chip || !h->token || h->active > 1U) return LTA_INVALID;
    if (r->op == LTA_OP_ACQUIRE) return lta_acquire(h, r);
    if (r->op == LTA_OP_RELEASE) {
        if (!h->active) return LTA_STATE;
        return lta_release(h);
    }
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    switch (r->op) {
    case LTA_OP_TONE_START: return lta_tone(h, r, 1);
    case LTA_OP_TONE_UPDATE: return lta_tone(h, r, 0);
    case LTA_OP_NOISE_START: return lta_noise(h, r, 1);
    case LTA_OP_NOISE_UPDATE: return lta_noise(h, r, 0);
    case LTA_OP_STOP:
        if (!lta_mask_valid(r->mask)) return LTA_INVALID;
        if ((r->mask & (lt_u8)~h->mask) != 0U) return LTA_OWNER;
        return lta_stop_internal(h, r->mask);
    case LTA_OP_MUTE: return lta_mute(h, r->enabled);
    case LTA_OP_ARM_STOP: return lta_arm_stop(h, r);
    case LTA_OP_SERVICE: return lta_service(h, r);
    case LTA_OP_LEVEL: return lta_level(h, r);
    case LTA_OP_NOISE_MODE: return lta_noise_mode(h, r);
    default: return LTA_UNSUPPORTED;
    }
}

lta_status lta_music_attenuation(lta_handle *h, lt_u8 channel, lt_u8 value)
{
    lta_status status;
    lt_u8 bit;
    if (!h || channel > 3U || value > 15U) return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    bit = (lt_u8)(1U << channel);
    if (!(h->mask & bit)) return LTA_OWNER;
    h->music_attenuation[channel] = value;
    if ((h->playing & bit) && !(h->effect_mask & bit))
        (void)lt_attenuate(h->chip, h->token, channel,
                           output_level(h, channel, h->level[channel], 0), 1);
    return LTA_OK;
}

lta_status lta_sfx_attenuation(lta_handle *h, lt_u8 value)
{
    lta_status status;
    unsigned i;
    if (!h || value > 15U) return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    h->sfx_attenuation = value;
    for (i = 0; i < 4U; ++i)
        if (h->effect_playing & (1U << i))
            (void)lt_attenuate(h->chip, h->token, (lt_u8)i,
                               output_level(h, i, h->effect_level[i], 1), 1);
    return LTA_OK;
}

lta_status lta_layer_pause(lta_handle *h, lt_u8 effect, lt_u8 paused)
{
    lta_status status;
    unsigned i;
    if (!h || effect > 1U || paused > 1U) return LTA_INVALID;
    status = lta_preflight_active(h);
    if (status != LTA_OK) return status;
    if (effect && !h->effect_active) return LTA_STATE;
    if (effect) h->effect_paused = paused;
    else h->music_paused = paused;
    for (i = 0; i < 4U; ++i) {
        lt_u8 bit = (lt_u8)(1U << i);
        if (effect) {
            if (!(h->effect_playing & bit)) continue;
        } else if (!(h->playing & bit) || (h->effect_mask & bit)) continue;
        (void)lt_attenuate(h->chip, h->token, (lt_u8)i,
                           output_level(h, i, effect ? h->effect_level[i] :
                                        h->level[i], effect), 1);
    }
    return LTA_OK;
}
