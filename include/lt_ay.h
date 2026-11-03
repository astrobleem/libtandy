/* SPDX-License-Identifier: MIT */
#ifndef LT_AY_H
#define LT_AY_H
#include "lt_audio.h"

/* Optional, approximate AY control-frame adapter. Not an emulator, file
 * decoder, tracker, timer, PCM renderer, or MSX-specific interface. C89. */
#define LTAY_COUPLED_NOISE 1U
#define LTAY_PERIODIC_BASS 2U
#define LTAY_RANGE_FOLD 0U
#define LTAY_RANGE_CLAMP 1U
#define LTAY_RANGE_MUTE 2U

#define LTAY_LOSS_VOLUME 1U
#define LTAY_LOSS_ENVELOPE 2U
#define LTAY_LOSS_MIXER 4U
#define LTAY_LOSS_NOISE 8U
#define LTAY_LOSS_FIXED_NOISE 16U
#define LTAY_LOSS_RANGE 32U
#define LTAY_LOSS_PULSE_BASS 64U
#define LTAY_LOSS_ENVELOPE_ONLY 128U

typedef struct {
    /* Tandy period / AY period = target_clock / (2 * source_clock).
     * Supply a reduced positive rational; no clock detection is performed.
     * Both terms are 1..65535; all intermediate products fit 32 bits. */
    lt_u16 scale_num, scale_den;
    lt_u8 flags, range_policy;
} lt_ay_config;

typedef struct {
    lt_u8 reg[14];
    /* If any AY voice enables its hardware envelope, the caller must supply
     * the current resolved 4-bit AY DAC level. Missing level is UNSUPPORTED.
     * R11..R13 are not interpreted; no shape-8 or fixed-RMS assumption.
     * A low-rate sample cannot preserve fast envelope timbre. */
    lt_u8 envelope_valid, envelope_level;
} lt_ay_frame;

typedef struct {
    /* Every tone is initialized, including silent tone2 when clocking noise.
     * Attenuation: 0 loudest, 15 silent (same convention as lta_handle). */
    lt_u16 period[3];
    lt_u8 attenuation[4], noise_mode;
    lt_u16 losses;
} lt_ay_plan;

/* Unity ratio, coupled noise allowed, pulse bass disabled, octave folding.
 * Initialize/configure in foreground; state is entirely caller-owned. */
void lt_ay_config_init(lt_ay_config *config);
/* Pure bounded mapping: no allocation, I/O, ownership or mutable globals.
 * Invalid/unsupported input leaves plan unchanged. Objects must not overlap.
 * AY high register bits are masked; tone/noise period zero means one.
 * A successful result may have loss flags: callers must not label it lossless. */
lta_status lt_ay_map(const lt_ay_config *config, const lt_ay_frame *frame,
                     lt_ay_plan *plan);
/* Apply through an initialized, active lta_handle owning all four channels.
 * This is the only base-music writer; existing SFX overlays may share that
 * handle. Armed auto-stop deadlines are rejected with BUSY. No claims are
 * acquired/released and no timer/transport is installed. Serialize like all
 * lta calls. Plan and handle must not overlap. Existing mute, attenuation
 * and overlay restoration are honored.
 * Stop/mute/pause/release use the existing lta API, not a second lifecycle. */
lta_status lt_ay_apply(lta_handle *audio, const lt_ay_plan *plan);
#endif
