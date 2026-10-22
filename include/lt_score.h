/* SPDX-License-Identifier: MIT */
#ifndef LT_SCORE_H
#define LT_SCORE_H

/* Static, caller-ticked three-tone Tandy scores. Tables stay in the app. */
#include "lt_audio.h"

#define LTSCORE_VERSION 1U
#define LTSCORE_VOICES 3U
#define LTSCORE_MAX_NOTES 2048U
#define LTSCORE_MAX_DRUMS 4096U
#define LTSCORE_MAX_KICKS 2048U

#define LTSCORE_DRUM_HAT 1U
#define LTSCORE_DRUM_SNARE 2U
#define LTSCORE_DRUM_CRASH 3U
#define LTSCORE_DRUM_ADDED_HAT 4U

#define LTSCORE_SPEAKER_KEEP_GATE 0U
#define LTSCORE_SPEAKER_GATE_ON 1U
#define LTSCORE_SPEAKER_GATE_OFF 2U

typedef struct {
    lt_u16 start, end, period;
} lt_score_note;

typedef struct {
    lt_u16 tick;
    lt_u8 kind;
} lt_score_drum;

/* One optional PIT2 kick at this score tick. The app's speaker sink decides
 * whether its existing cooperative lease permits each requested write. */
typedef struct {
    lt_u16 tick;
} lt_score_kick;

typedef struct {
    lt_u8 version, noise_mode;
    lt_u16 ppq, loop_start, loop_end;
    lta_u32 tempo_us;
    const lt_score_note *notes[LTSCORE_VOICES];
    lt_u16 note_count[LTSCORE_VOICES];
    lt_u8 level[LTSCORE_VOICES], release_gap[LTSCORE_VOICES];
    const lt_score_drum *drums;
    lt_u16 drum_count;
    const lt_score_kick *kicks;
    lt_u16 kick_count;
} lt_score;

/* The sink only receives requests. It must check its app-owned PIT lease and
 * perform no write when the caller does not hold that lease. KEEP changes
 * only the divisor; ON changes divisor then opens the gate; OFF closes it. */
typedef void (*lt_score_speaker_sink)(void *context, lt_u16 divisor,
                                      lt_u8 gate);

typedef struct {
    lt_u16 index, gate_end, attack_end, decay_start;
    lt_u8 level, playing, attack_pending, decay_pending;
} lt_score_voice;

/* Keep one player with each lta_handle. Score tables must remain static and
 * unchanged until stop. This state uses fixed storage and no heap. */
typedef struct {
    lta_handle *audio;
    const lt_score *score;
    lt_score_speaker_sink speaker;
    void *speaker_context;
    lta_u32 phase, step;
    lt_u16 drum_index, kick_index;
    lt_u16 drum_end;
    lt_u8 active, drum_kind, drum_playing, speaker_stage;
    lt_score_voice voice[LTSCORE_VOICES];
} lt_score_player;

/* Init is silent. Start uses the supplied timer rate (zero means 60 Hz),
 * writes the initial mute/noise state, and emits events at score tick zero.
 * Tick advances the score's 16.16 tempo phase once; call it from the app's
 * existing timer/game cadence. Intro events play once before loop_start. */
lta_status lt_score_init(lt_score_player *player, lta_handle *audio);
lta_status lt_score_start(lt_score_player *player, const lt_score *score,
                          lta_u32 timer_rate,
                          lt_score_speaker_sink speaker, void *context);
lta_status lt_score_tick(lt_score_player *player);
lta_status lt_score_stop(lt_score_player *player);

#endif
