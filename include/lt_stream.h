/* SPDX-License-Identifier: MIT */
#ifndef LT_STREAM_H
#define LT_STREAM_H
#include "lt_audio.h"

#define LTST_VERSION 1U
#define LTST_TANDY 1U
#define LTST_BUDGET 64U
#define LTST_MAX_BYTES 32767U
#define LTST_MAX_STEPS 8192UL

/* Immutable caller-owned descriptor and near data, no allocation. Version 1:
 * 00 END; 01 ticks:u16le WAIT (nonzero); 02 offset:u16le bytes:u16le
 * count:u8 REF (backward, whole commands, no nested REF/END);
 * 10..12 period:u16le level:u8 TONE channel 0..2;
 * 13 mode:u8 level:u8 NOISE; 20..23 level:u8 LEVEL; 30 mask:u8 STOP.
 * REF plays the earlier block count additional times. END is compulsory.
 * Metadata describes already compiled Tandy periods, not an SMS converter.
 * Each tick emits at most 64 logical operations. No raw PSG writes. */
typedef struct {
    const lt_u8 *data;
    lt_u16 bytes;
    lt_u8 version, chip, channel_mask, priority;
    lta_u32 clock_hz, rate_num, rate_den;
} lt_stream;

typedef struct {
    lt_u16 pc, return_pc, ref_start, ref_end;
    lt_u8 repeats;
} lt_stream_cursor;

typedef struct {
    lta_handle *audio;
    const lt_stream *stream;
    lt_stream_cursor cursor;
    lt_u16 wait;
    lt_u8 active, effect, paused, borrowed_mask;
} lt_stream_player;

/* Foreground validation expands no song buffer. At most 8192 virtual
 * commands, 32767 source bytes, one reference frame and 2^31-1 total ticks.
 * Rejects truncation, unaligned references, nesting, oversized bursts,
 * invalid channels and tone2-dependent noise without an initialized clock.
 * Init/start/validate belong in foreground, never in an IRQ. */
lta_status lt_stream_validate(const lt_stream *stream);
lta_status lt_stream_init(lt_stream_player *player, lta_handle *audio);
/* effect=1 borrows channels through the existing overlay and priority path;
 * effect=0 updates base music, including under an effect. One base player
 * and one effect player/clip per handle; caller serializes all operations.
 * An effect stream may replace an equal/lower priority stream in this player.
 * A different overlay player is busy: stop it explicitly before switching. */
lta_status lt_stream_start(lt_stream_player *player, const lt_stream *stream,
                            lt_u8 effect);
lta_status lt_stream_tick(lt_stream_player *player);
/* Pause silences its layer, holds reservations and freezes time. Stop
 * discards the cursor, restoring current music for effects or stopping
 * declared music channels. */
lta_status lt_stream_pause(lt_stream_player *player, lt_u8 paused);
lta_status lt_stream_stop(lt_stream_player *player);
#endif
