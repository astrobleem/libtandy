/* SPDX-License-Identifier: MIT */
#ifndef LT_SFX_H
#define LT_SFX_H

/* Versioned, caller-ticked PSG clips. No allocation, timer or PCM path. */
#include "lt_audio.h"

#define LTSFX_CLIP_VERSION 2U
#define LTSFX_FILE_VERSION 1U
#define LTSFX_FILE_HEADER_BYTES 20U
#define LTSFX_MAX_EVENTS 64U
#define LTSFX_MAX_TICKS 255U
#define LTSFX_FILE_MAX_BYTES (LTSFX_FILE_HEADER_BYTES + 6U * LTSFX_MAX_EVENTS)

#define LTSFX_OP_TONE 1U
#define LTSFX_OP_NOISE 2U
#define LTSFX_OP_STOP 3U

#define LTSFX_PROVENANCE_NONE 0U
#define LTSFX_PROVENANCE_JAWS_M2 1U

/* Six-byte source-export record: absolute tick, operation, channel, a
 * little-endian 16-bit value, then level. TONE value is a 10-bit period;
 * NOISE value is a 3-bit mode; STOP has a zero value and level. */
typedef struct {
    lt_u8 tick, op, channel, value_lo, value_hi, level;
} lt_sfx_event;

typedef struct {
    lt_u8 version, channel_mask, priority, event_count, duration_ticks;
    lt_u8 provenance_id;
    lt_u16 event_bytes;
    /* Source/caller tick rate, ticks per second as a reduced rational.
     * Metadata only: lt_sfx_tick still advances only when the caller calls it. */
    lta_u32 tick_rate_num, tick_rate_den;
    const lt_sfx_event *events;
} lt_sfx_clip;

/* Canonical .ltsfx file APIs. The wire header is 20 bytes and events follow
 * as six-byte records. Check size before encode; insufficient output capacity
 * returns LTA_INVALID and leaves written_bytes zero. Decode copies into
 * caller-owned event storage; clip is set only on success, though scratch may
 * be overwritten on failure. These are not in-place APIs: clip metadata,
 * event storage, input bytes, output bytes, and written_bytes must not overlap. */
lta_status lt_sfx_file_size_v1(const lt_sfx_clip *clip, lt_u16 *byte_count);
lta_status lt_sfx_file_decode_v1(const lt_u8 *bytes, lt_u16 byte_count,
                                 lt_sfx_clip *clip,
                                 lt_sfx_event *event_storage,
                                 lt_u8 event_capacity);
lta_status lt_sfx_file_encode_v1(const lt_sfx_clip *clip,
                                 lt_u8 *destination,
                                 lt_u16 destination_capacity,
                                 lt_u16 *written_bytes);

/* One foreground clip player per lta_handle. Clip and event data must remain
 * readable and unchanged until the player finishes or is stopped. */
typedef struct {
    lta_handle *audio;
    const lt_sfx_clip *clip;
    lt_u8 event_index, elapsed, active, borrowed_mask;
} lt_sfx_player;

/* Init is silent and may run before audio ownership is acquired. */
lta_status lt_sfx_init(lt_sfx_player *player, lta_handle *audio);

/* A trigger is bounded and never waits. It borrows and mutes the clip's voices;
 * tick zero is emitted by the next lt_sfx_tick call. Higher priority replaces
 * a running clip; equal priority replaces it too, so the newest cue wins.
 * Lower priority is rejected with LTA_BUSY. */
lta_status lt_sfx_trigger(lt_sfx_player *player, const lt_sfx_clip *clip);

/* Call once per caller-defined game tick. Multiple same-tick records are
 * applied in order. Expiry restores the latest base music voice state. */
lta_status lt_sfx_tick(lt_sfx_player *player);

/* Stop restores the latest base music state immediately. Reset also clears
 * the elapsed tick and event cursor; both are safe when already idle. */
lta_status lt_sfx_stop(lt_sfx_player *player);
lta_status lt_sfx_reset(lt_sfx_player *player);

#endif
