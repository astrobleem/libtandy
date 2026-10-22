/* SPDX-License-Identifier: MIT */
#include "lt_sfx.h"

typedef char ltsfx_event_must_be_six_bytes[
    (sizeof(lt_sfx_event) == 6U) ? 1 : -1];

static lt_u16 event_value(const lt_sfx_event *event)
{
    return (lt_u16)((lt_u16)event->value_lo |
                    ((lt_u16)event->value_hi << 8));
}

static lta_u32 rate_gcd(lta_u32 left, lta_u32 right)
{
    while (right) {
        lta_u32 remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static lta_status validate_clip(const lt_sfx_clip *clip)
{
    unsigned i;
    lt_u8 last_tick = 0U;
    lt_u8 have_tick = 0U;
    lt_u8 tone2_ready = 0U;
    lt_u8 noise3_active = 0U;
    lt_u16 expected_bytes;

    /* Reject a short or malformed export before it can borrow any voice. */
    if (!clip || clip->version != LTSFX_CLIP_VERSION ||
        clip->channel_mask == 0U || (clip->channel_mask & 240U) != 0U ||
        clip->provenance_id > 1U ||
        clip->event_count == 0U || clip->event_count > LTSFX_MAX_EVENTS ||
        clip->duration_ticks == 0U || !clip->tick_rate_num ||
        !clip->tick_rate_den ||
        rate_gcd(clip->tick_rate_num, clip->tick_rate_den) != 1UL ||
        !clip->events)
        return LTA_INVALID;
    expected_bytes = (lt_u16)((lt_u16)clip->event_count *
                              (lt_u16)sizeof(lt_sfx_event));
    if (clip->event_bytes != expected_bytes) return LTA_INVALID;

    for (i = 0U; i < clip->event_count; ++i) {
        const lt_sfx_event *event = &clip->events[i];
        lt_u16 value = event_value(event);
        lt_u8 bit;
        if (event->tick >= clip->duration_ticks ||
            (have_tick && event->tick < last_tick)) return LTA_INVALID;
        have_tick = 1U;
        last_tick = event->tick;

        if (event->channel > 3U) return LTA_INVALID;
        bit = (lt_u8)(1U << event->channel);
        if (!(clip->channel_mask & bit)) return LTA_INVALID;

        if (event->op == LTSFX_OP_TONE) {
            if (event->channel >= 3U || value > LTA_MAX_PERIOD ||
                event->level > 15U) return LTA_INVALID;
            if (event->channel == 2U) tone2_ready = 1U;
        } else if (event->op == LTSFX_OP_NOISE) {
            if (event->channel != LTA_NOISE_CHANNEL || value > 7U ||
                event->level > 15U) return LTA_INVALID;
            if ((value & 3U) == 3U) {
                if ((clip->channel_mask & LTA_TONE2_NOISE) !=
                    LTA_TONE2_NOISE || !tone2_ready) return LTA_INVALID;
                noise3_active = 1U;
            } else {
                noise3_active = 0U;
            }
        } else if (event->op == LTSFX_OP_STOP) {
            if (value != 0U || event->level != 0U) return LTA_INVALID;
            if (event->channel == 2U) {
                tone2_ready = 0U;
                if (noise3_active) noise3_active = 0U;
            } else if (event->channel == 3U && noise3_active) {
                tone2_ready = 0U;
                noise3_active = 0U;
            }
        } else {
            return LTA_INVALID;
        }
    }
    return LTA_OK;
}

static lta_u32 read_u32_le(const lt_u8 *bytes)
{
    return (lta_u32)bytes[0] | ((lta_u32)bytes[1] << 8) |
           ((lta_u32)bytes[2] << 16) | ((lta_u32)bytes[3] << 24);
}

static void write_u32_le(lt_u8 *bytes, lta_u32 value)
{
    bytes[0] = (lt_u8)value;
    bytes[1] = (lt_u8)(value >> 8);
    bytes[2] = (lt_u8)(value >> 16);
    bytes[3] = (lt_u8)(value >> 24);
}

lta_status lt_sfx_file_size_v1(const lt_sfx_clip *clip, lt_u16 *byte_count)
{
    lta_status status;
    if (!byte_count) return LTA_INVALID;
    status = validate_clip(clip);
    if (status != LTA_OK) return status;
    *byte_count = (lt_u16)(LTSFX_FILE_HEADER_BYTES + clip->event_bytes);
    return LTA_OK;
}

lta_status lt_sfx_file_decode_v1(const lt_u8 *bytes, lt_u16 byte_count,
                                 lt_sfx_clip *clip,
                                 lt_sfx_event *event_storage,
                                 lt_u8 event_capacity)
{
    lt_sfx_clip candidate;
    lt_u8 event_count, duration_ticks;
    lt_u16 expected_bytes, i;
    lta_status status;

    if (!bytes || !clip || !event_storage ||
        byte_count < LTSFX_FILE_HEADER_BYTES ||
        bytes[0] != (lt_u8)'L' || bytes[1] != (lt_u8)'T' ||
        bytes[2] != (lt_u8)'S' || bytes[3] != (lt_u8)'F' ||
        bytes[4] != LTSFX_FILE_VERSION ||
        bytes[5] != LTSFX_FILE_HEADER_BYTES || bytes[19] != 0U ||
        bytes[18] > 1U)
        return LTA_INVALID;

    event_count = bytes[8];
    duration_ticks = bytes[9];
    if (event_count == 0U || event_count > LTSFX_MAX_EVENTS ||
        duration_ticks == 0U || event_capacity < event_count)
        return LTA_INVALID;
    expected_bytes = (lt_u16)(LTSFX_FILE_HEADER_BYTES +
                              (lt_u16)event_count * 6U);
    if (byte_count != expected_bytes) return LTA_INVALID;

    candidate.version = LTSFX_CLIP_VERSION;
    candidate.channel_mask = bytes[6];
    candidate.priority = bytes[7];
    candidate.event_count = event_count;
    candidate.duration_ticks = duration_ticks;
    candidate.provenance_id = bytes[18];
    candidate.event_bytes = (lt_u16)((lt_u16)event_count * 6U);
    candidate.tick_rate_num = read_u32_le(bytes + 10);
    candidate.tick_rate_den = read_u32_le(bytes + 14);
    candidate.events = event_storage;

    /* The file length and header are already bounded, so copy exactly the
     * declared records into caller storage and reuse the runtime validator. */
    for (i = 0U; i < event_count; ++i) {
        const lt_u8 *record = bytes + LTSFX_FILE_HEADER_BYTES + i * 6U;
        event_storage[i].tick = record[0];
        event_storage[i].op = record[1];
        event_storage[i].channel = record[2];
        event_storage[i].value_lo = record[3];
        event_storage[i].value_hi = record[4];
        event_storage[i].level = record[5];
    }
    status = validate_clip(&candidate);
    if (status != LTA_OK) return status;
    *clip = candidate;
    return LTA_OK;
}

lta_status lt_sfx_file_encode_v1(const lt_sfx_clip *clip,
                                 lt_u8 *destination,
                                 lt_u16 destination_capacity,
                                 lt_u16 *written_bytes)
{
    lt_u16 required, i;
    lta_status status;
    if (written_bytes) *written_bytes = 0U;
    if (!destination || !written_bytes) return LTA_INVALID;
    status = lt_sfx_file_size_v1(clip, &required);
    if (status != LTA_OK || destination_capacity < required)
        return LTA_INVALID;

    destination[0] = (lt_u8)'L';
    destination[1] = (lt_u8)'T';
    destination[2] = (lt_u8)'S';
    destination[3] = (lt_u8)'F';
    destination[4] = LTSFX_FILE_VERSION;
    destination[5] = LTSFX_FILE_HEADER_BYTES;
    destination[6] = clip->channel_mask;
    destination[7] = clip->priority;
    destination[8] = clip->event_count;
    destination[9] = clip->duration_ticks;
    write_u32_le(destination + 10, clip->tick_rate_num);
    write_u32_le(destination + 14, clip->tick_rate_den);
    destination[18] = clip->provenance_id;
    destination[19] = 0U;
    for (i = 0U; i < clip->event_count; ++i) {
        const lt_sfx_event *event = &clip->events[i];
        lt_u8 *record = destination + LTSFX_FILE_HEADER_BYTES + i * 6U;
        record[0] = event->tick;
        record[1] = event->op;
        record[2] = event->channel;
        record[3] = event->value_lo;
        record[4] = event->value_hi;
        record[5] = event->level;
    }
    *written_bytes = required;
    return LTA_OK;
}

static void clear_player(lt_sfx_player *player)
{
    player->clip = 0;
    player->event_index = 0U;
    player->elapsed = 0U;
    player->active = 0U;
    player->borrowed_mask = 0U;
}

lta_status lt_sfx_init(lt_sfx_player *player, lta_handle *audio)
{
    lt_sfx_player next;
    if (!player || !audio) return LTA_INVALID;
    next.audio = audio;
    next.clip = 0;
    next.event_index = 0U;
    next.elapsed = 0U;
    next.active = 0U;
    next.borrowed_mask = 0U;
    *player = next;
    return LTA_OK;
}

lta_status lt_sfx_trigger(lt_sfx_player *player, const lt_sfx_clip *clip)
{
    lta_status status;
    lt_u8 borrowed;
    if (!player || !player->audio) return LTA_INVALID;
    status = validate_clip(clip);
    if (status != LTA_OK) return status;
    if (!player->audio->active) return LTA_STATE;
    if ((clip->channel_mask & (lt_u8)~player->audio->mask) != 0U)
        return LTA_OWNER;

    if (player->active) {
        if (!player->clip || !player->audio->effect_active)
            return LTA_STATE;
        if (clip->priority < player->clip->priority) return LTA_BUSY;
        /* Stop first so the old clip cannot leak into the replacement. */
        status = lt_sfx_stop(player);
        if (status != LTA_OK) return status;
    } else if (player->audio->effect_active) {
        return LTA_BUSY;
    }

    status = lta_overlay_begin(player->audio, clip->channel_mask, &borrowed);
    if (status != LTA_OK) return status;
    player->clip = clip;
    player->event_index = 0U;
    player->elapsed = 0U;
    player->active = 1U;
    player->borrowed_mask = borrowed;
    return LTA_OK;
}

static lta_status apply_event(lt_sfx_player *player,
                              const lt_sfx_event *event)
{
    lt_u16 value = event_value(event);
    if (event->op == LTSFX_OP_TONE)
        return lta_overlay_tone(player->audio, event->channel, value,
                                event->level);
    if (event->op == LTSFX_OP_NOISE)
        return lta_overlay_noise(player->audio, (lt_u8)value, event->level);
    if (event->op == LTSFX_OP_STOP)
        return lta_overlay_stop(player->audio,
                                (lt_u8)(1U << event->channel));
    return LTA_INVALID;
}

lta_status lt_sfx_tick(lt_sfx_player *player)
{
    lta_status status;
    const lt_sfx_clip *clip;
    if (!player || !player->audio) return LTA_INVALID;
    if (player->active > 1U) return LTA_INVALID;
    if (!player->active) return LTA_OK;
    if (!player->audio->active || !player->audio->effect_active) {
        clear_player(player);
        return LTA_STATE;
    }
    clip = player->clip;
    if (!clip || player->elapsed > clip->duration_ticks ||
        player->event_index > clip->event_count) return LTA_INVALID;
    if (player->elapsed == clip->duration_ticks) {
        status = lta_overlay_end(player->audio);
        if (status == LTA_OK) clear_player(player);
        return status;
    }
    /* One caller tick may contain several ordered register changes. */
    while (player->event_index < clip->event_count &&
           clip->events[player->event_index].tick == player->elapsed) {
        status = apply_event(player,
                             &clip->events[player->event_index]);
        if (status != LTA_OK) return status;
        ++player->event_index;
    }
    ++player->elapsed;
    return LTA_OK;
}

lta_status lt_sfx_stop(lt_sfx_player *player)
{
    lta_status status;
    if (!player || !player->audio) return LTA_INVALID;
    if (!player->active) {
        clear_player(player);
        return LTA_OK;
    }
    if (!player->audio->active || !player->audio->effect_active) {
        clear_player(player);
        return LTA_STATE;
    }
    status = lta_overlay_end(player->audio);
    if (status == LTA_OK) clear_player(player);
    return status;
}

lta_status lt_sfx_reset(lt_sfx_player *player)
{
    return lt_sfx_stop(player);
}
