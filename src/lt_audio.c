/* SPDX-License-Identifier: MIT */
#include "lt_audio.h"

#define LTA_VALID_MASK 15U
#define LTA_TONE_BITS 7U
#define LTA_U32_HALF 0x80000000UL

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
        h->muted > 1U || (h->mask & 240U) != 0U ||
        (h->playing & (lt_u8)~h->mask) != 0U ||
        (h->deadline_mask & (lt_u8)~h->mask) != 0U)
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
    if (h->chip->noise_valid && (h->chip->noise & 3U) == 3U &&
        h->chip->owner[3] == h->token &&
        (mask & LTA_TONE2_NOISE)) mask |= LTA_TONE2_NOISE;
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
        if (expanded & bit)
            (void)lt_attenuate(h->chip, h->token, (lt_u8)i, 15U, 1);
    }
    h->playing &= (lt_u8)~expanded;
    for (i = 0; i < 3U; ++i)
        if (expanded & (1U << i)) {
            h->kind[i] = LTA_KIND_NONE;
            h->chip->tone_valid &= (lt_u8)~(1U << i);
        }
    if (expanded & 8U) {
        h->kind[3] = LTA_KIND_NONE;
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
    for (i = 0; i < 4U; ++i) {
        next.kind[i] = LTA_KIND_NONE;
        next.level[i] = 15U;
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
    h->mask = 0U;
    h->playing = 0U;
    h->muted = 0U;
    h->deadline_mask = 0U;
    h->clock_valid = 0U;
    h->noise_mode = 0U;
    for (i = 0; i < 4U; ++i) {
        h->kind[i] = LTA_KIND_NONE;
        h->level[i] = 15U;
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
    if (!lt_tone(h->chip, h->token, r->channel, r->period, start))
        return LTA_OWNER;
    attenuation = h->muted ? 15U : r->level;
    if (!lt_attenuate(h->chip, h->token, r->channel, attenuation, start))
        return LTA_OWNER;
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
    if ((r->mode & 3U) == 3U &&
        (!(h->mask & 4U) || !(h->chip->tone_valid & 4U))) return LTA_STATE;
    if (!lt_noise(h->chip, h->token, r->mode, start)) return LTA_OWNER;
    attenuation = h->muted ? 15U : r->level;
    if (!lt_attenuate(h->chip, h->token, 3U, attenuation, start))
        return LTA_OWNER;
    h->noise_mode = r->mode;
    h->level[3] = r->level;
    h->kind[3] = LTA_KIND_NOISE;
    h->playing |= 8U;
    if (start) lta_cancel_deadlines(h, 8U);
    return LTA_OK;
}

static lta_status lta_mute(lta_handle *h, lt_u8 enabled)
{
    unsigned i;
    if (enabled > 1U) return LTA_INVALID;
    if (h->muted == enabled) return LTA_OK;
    for (i = 0; i < 4U; ++i) {
        lt_u8 bit = (lt_u8)(1U << i);
        if (h->mask & bit) {
            lt_u8 attenuation = 15U;
            if (!enabled && (h->playing & bit)) attenuation = h->level[i];
            if (!lt_attenuate(h->chip, h->token, (lt_u8)i,
                              attenuation, 1)) return LTA_OWNER;
        }
    }
    h->muted = enabled;
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
    next.last_now = 0UL;
    {
        unsigned i;
        for (i = 0; i < 4U; ++i) {
            next.kind[i] = LTA_KIND_NONE;
            next.level[i] = 15U;
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
        r->op > LTA_OP_SERVICE)
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
    default: return LTA_UNSUPPORTED;
    }
}
