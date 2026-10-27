/* SPDX-License-Identifier: MIT */
#include "lt_stream.h"
#include <string.h>

static lt_u16 word(const lt_u8 *p)
{
    return (lt_u16)((lt_u16)p[0] | ((lt_u16)p[1] << 8));
}

static unsigned width(lt_u8 op)
{
    if (op == 0U) return 1U;
    if (op == 1U || op == 0x13U) return 3U;
    if (op == 2U) return 6U;
    if (op >= 0x10U && op <= 0x12U) return 4U;
    if ((op >= 0x20U && op <= 0x23U) || op == 0x30U) return 2U;
    return 0U;
}

/* All additions follow subtraction-based bounds checks (16-bit unsigned). */
static int record(const lt_stream *s, lt_u16 pc, unsigned *n)
{
    if (pc >= s->bytes) return 0;
    *n = width(s->data[pc]);
    return *n != 0U && *n <= (unsigned)(s->bytes - pc);
}

static void advance_reference(lt_stream_cursor *c)
{
    if (c->repeats && c->pc == c->ref_end) {
        --c->repeats;
        c->pc = c->repeats ? c->ref_start : c->return_pc;
    }
}

static void enter_reference(lt_stream_cursor *c, const lt_u8 *p)
{
    c->return_pc = c->pc;
    c->ref_start = word(p + 1);
    c->ref_end = (lt_u16)(c->ref_start + word(p + 3));
    c->repeats = p[5];
    c->pc = c->ref_start;
}

lta_status lt_stream_validate(const lt_stream *s)
{
    lt_u16 pc = 0U;
    unsigned n, burst = 0U;
    lta_u32 steps = 0UL, ticks = 0UL, inspections = 0UL;
    lt_u8 tone2 = 0U, noise3 = 0U, playing = 0U;
    lt_stream_cursor c;
    if (!s || !s->data || !s->bytes || s->bytes > LTST_MAX_BYTES ||
        s->version != LTST_VERSION || s->chip != LTST_TANDY ||
        !s->clock_hz || !s->rate_num || !s->rate_den ||
        !s->channel_mask || (s->channel_mask & 240U)) return LTA_INVALID;
    /* Structural pass: references must start/end on earlier boundaries and
     * contain no control transfers. Depth is exactly one, never recursive. */
    while (pc < s->bytes) {
        const lt_u8 *p = s->data + pc;
        if (++inspections > LTST_MAX_STEPS || !record(s, pc, &n)) return LTA_INVALID;
        if (*p == 0U && pc != s->bytes - 1U) return LTA_INVALID;
        if (*p == 2U) {
            lt_u16 start = word(p + 1), len = word(p + 3), scan = 0U;
            if (!len || !p[5] || start >= pc || len > pc - start)
                return LTA_INVALID;
            while (scan < start) {
                unsigned k;
                if (++inspections > LTST_MAX_STEPS || !record(s, scan, &k)) return LTA_INVALID;
                scan = (lt_u16)(scan + k);
            }
            if (scan != start) return LTA_INVALID;
            while (scan < start + len) {
                unsigned k;
                if (++inspections > LTST_MAX_STEPS || !record(s, scan, &k) || s->data[scan] == 0U ||
                    s->data[scan] == 2U || k > (unsigned)(start + len - scan))
                    return LTA_INVALID;
                scan = (lt_u16)(scan + k);
            }
        }
        pc = (lt_u16)(pc + n);
    }
    if (s->data[s->bytes - 1U] != 0U) return LTA_INVALID;
    memset(&c, 0, sizeof(c));
    /* Dry run validates expanded semantics and worst per-tick work. */
    for (;;) {
        const lt_u8 *p;
        lt_u8 op, channel, mask;
        advance_reference(&c);
        if (!record(s, c.pc, &n) || ++steps > LTST_MAX_STEPS ||
            ++burst > LTST_BUDGET) return LTA_INVALID;
        p = s->data + c.pc;
        op = *p;
        c.pc = (lt_u16)(c.pc + n);
        if (op == 0U) return ticks ? LTA_OK : LTA_INVALID;
        if (op == 2U) { enter_reference(&c, p); continue; }
        if (op == 1U) {
            lt_u16 wait = word(p + 1);
            if (!wait || ticks > LTA_MAX_DURATION - wait) return LTA_INVALID;
            ticks += wait;
            burst = 0U;
            continue;
        }
        if (op == 0x30U) {
            mask = p[1];
            if (!mask || (mask & (lt_u8)~s->channel_mask)) return LTA_INVALID;
            if ((mask & 12U) && noise3) { tone2 = noise3 = 0U; mask |= 12U; }
            playing &= (lt_u8)~mask;
            if (mask & 4U) tone2 = 0U;
            continue;
        }
        channel = (lt_u8)(op & 3U);
        if (!(s->channel_mask & (1U << channel))) return LTA_INVALID;
        if (op >= 0x20U) {
            if (p[1] > 15U || !(playing & (1U << channel))) return LTA_INVALID;
        } else if (op == 0x13U) {
            if (p[1] > 7U || p[2] > 15U) return LTA_INVALID;
            playing |= 8U;
            noise3 = (lt_u8)((p[1] & 3U) == 3U);
            if (noise3 && (!tone2 || (s->channel_mask & 12U) != 12U))
                return LTA_INVALID;
        } else {
            if (word(p + 1) > 1023U || p[3] > 15U) return LTA_INVALID;
            playing |= (lt_u8)(1U << channel);
            if (channel == 2U) tone2 = 1U;
        }
    }
}

lta_status lt_stream_init(lt_stream_player *p, lta_handle *h)
{
    if (!p || !h) return LTA_INVALID;
    memset(p, 0, sizeof(*p));
    p->audio = h;
    return LTA_OK;
}

lta_status lt_stream_stop(lt_stream_player *p)
{
    lta_status status;
    lta_request r;
    if (!p || !p->audio) return LTA_INVALID;
    if (!p->active) return LTA_OK;
    if (p->effect) status = lta_overlay_end(p->audio);
    else {
        lta_request_init(&r);
        r.op = LTA_OP_STOP;
        r.mask = p->stream->channel_mask;
        status = lta_command(p->audio, &r);
        if (status == LTA_OK) (void)lta_layer_pause(p->audio, 0U, 0U);
    }
    if (status == LTA_OK || status == LTA_STATE) p->active = 0U;
    return status;
}

lta_status lt_stream_start(lt_stream_player *p, const lt_stream *s,
                            lt_u8 effect)
{
    lta_status status;
    if (!p || !p->audio || effect > 1U) return LTA_INVALID;
    status = lt_stream_validate(s);
    if (status != LTA_OK) return status;
    if (!p->audio->active) return LTA_STATE;
    if (s->channel_mask & (lt_u8)~p->audio->mask) return LTA_OWNER;
    if (effect && p->active && p->effect &&
        s->priority < p->stream->priority) return LTA_BUSY;
    if (effect && p->audio->effect_active && !(p->active && p->effect))
        return LTA_BUSY;
    status = lt_stream_stop(p);
    if (status != LTA_OK) return status;
    p->borrowed_mask = 0U;
    if (effect) {
        status = lta_overlay_begin(p->audio, s->channel_mask, &p->borrowed_mask);
        if (status != LTA_OK) return status;
    }
    p->stream = s;
    memset(&p->cursor, 0, sizeof(p->cursor));
    p->wait = 0U;
    p->paused = 0U;
    p->effect = effect;
    p->active = 1U;
    return LTA_OK;
}

static lta_status apply(lt_stream_player *p, const lt_u8 *data)
{
    lt_u8 op = data[0], ch = (lt_u8)(op & 3U);
    lta_request r;
    /* Validate again at the output boundary; metadata/data must remain
     * immutable during playback. Ownership is checked by the backend too. */
    if (op == 0x30U) {
        if (!data[1] || (data[1] & (lt_u8)~p->stream->channel_mask))
            return LTA_INVALID;
    } else if (!(p->stream->channel_mask & (1U << ch))) return LTA_OWNER;
    if (p->effect) {
        if (op <= 0x12U) return lta_overlay_tone(p->audio, ch,
                                                word(data + 1), data[3]);
        if (op == 0x13U) return lta_overlay_noise(p->audio, data[1], data[2]);
        if (op == 0x30U) return lta_overlay_stop(p->audio, data[1]);
        /* LEVEL needs an existing logical effect voice. Reissue its complete
         * operation, never an unchecked latch-dependent byte. */
        if (!(p->audio->effect_playing & (1U << ch))) return LTA_STATE;
        if (ch == 3U) return lta_overlay_noise(p->audio,
                                       p->audio->effect_noise_mode, data[1]);
        return lta_overlay_tone(p->audio, ch, p->audio->chip->period[ch], data[1]);
    }
    lta_request_init(&r);
    r.channel = ch;
    if (op <= 0x12U) {
        r.op = LTA_OP_TONE_START; r.period = word(data + 1); r.level = data[3];
    } else if (op == 0x13U) {
        r.op = LTA_OP_NOISE_START; r.mode = data[1]; r.level = data[2];
    } else if (op == 0x30U) {
        r.op = LTA_OP_STOP; r.mask = data[1];
    } else { r.op = LTA_OP_LEVEL; r.level = data[1]; }
    return lta_command(p->audio, &r);
}

lta_status lt_stream_tick(lt_stream_player *p)
{
    unsigned work, n;
    lta_status status = LTA_INVALID;
    if (!p || !p->audio) return LTA_INVALID;
    if (!p->active || p->paused) return LTA_OK;
    if (p->wait && --p->wait) return LTA_OK;
    for (work = 0U; work < LTST_BUDGET; ++work) {
        const lt_u8 *data;
        lt_u8 op;
        advance_reference(&p->cursor);
        if (!record(p->stream, p->cursor.pc, &n)) break;
        data = p->stream->data + p->cursor.pc;
        op = *data;
        p->cursor.pc = (lt_u16)(p->cursor.pc + n);
        if (op == 0U) return lt_stream_stop(p);
        if (op == 1U) {
            p->wait = word(data + 1);
            if (!p->wait) break;
            return LTA_OK;
        }
        if (op == 2U) {
            if (p->cursor.repeats) break;
            enter_reference(&p->cursor, data);
            continue;
        }
        status = apply(p, data);
        if (status != LTA_OK) break;
        status = LTA_INVALID;
    }
    (void)lt_stream_stop(p);
    return status;
}

lta_status lt_stream_pause(lt_stream_player *p, lt_u8 paused)
{
    if (!p || !p->audio || paused > 1U) return LTA_INVALID;
    if (!p->active) return LTA_STATE;
    {
        lta_status status = lta_layer_pause(p->audio, p->effect, paused);
        if (status != LTA_OK) return status;
    }
    p->paused = paused;
    return LTA_OK;
}
