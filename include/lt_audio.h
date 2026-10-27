/* SPDX-License-Identifier: MIT */
#ifndef LT_AUDIO_H
#define LT_AUDIO_H

/* Original caller-driven tone/noise façade over lt_chip. No PCM, heap, OS,
 * timer, or independent audio engine is used. C89. */
#include <limits.h>
#include "lt_sound.h"

#define LTA_API_VERSION 0x0202U

#if ULONG_MAX == 0xFFFFFFFFUL
typedef unsigned long lta_u32;
#elif UINT_MAX == 0xFFFFFFFFU
typedef unsigned int lta_u32;
#else
#error libtandy audio requires a 32-bit unsigned integer type
#endif

typedef enum {
    LTA_OK = 0,
    LTA_INVALID = 1,
    LTA_BUSY = 2,
    LTA_OWNER = 3,
    LTA_STATE = 4,
    LTA_TIME = 5,
    LTA_UNSUPPORTED = 6
} lta_status;

#define LTA_OP_ACQUIRE 1U
#define LTA_OP_RELEASE 2U
#define LTA_OP_TONE_START 3U
#define LTA_OP_TONE_UPDATE 4U
#define LTA_OP_NOISE_START 5U
#define LTA_OP_NOISE_UPDATE 6U
#define LTA_OP_STOP 7U
#define LTA_OP_MUTE 8U
#define LTA_OP_ARM_STOP 9U
#define LTA_OP_SERVICE 10U
#define LTA_OP_LEVEL 11U
#define LTA_OP_NOISE_MODE 12U
#define LTA_OP_PCM 128U

#define LTA_CAP_TONE 0x0001U
#define LTA_CAP_NOISE 0x0002U
#define LTA_CAP_MUTE 0x0004U
#define LTA_CAP_AUTO_STOP 0x0008U

#define LTA_NOISE_CHANNEL 3U
#define LTA_TONE2_NOISE LT_TONE2_NOISE
#define LTA_MAX_PERIOD 1023U
#define LTA_MAX_DURATION 0x7FFFFFFFUL

/* Unused request fields are ignored. ACQUIRE/STOP/ARM_STOP use mask;
 * tone operations use channel (0..2), period (0..1023), and level (0..15);
 * noise operations use mode (0..7) and level; MUTE uses enabled (0/1).
 * ARM_STOP uses value as a nonzero duration or immediate-stop zero;
 * SERVICE uses value as a modulo-32-bit sample in caller-defined clock units.
 * LEVEL writes only volume for acquired channel 0..3; NOISE_MODE writes only
 * the noise-control latch and updates the saved mode. The first SERVICE
 * establishes time; later gaps must be below 2^31 ticks. */
typedef struct {
    lt_u8 op, mask, channel, level, mode, enabled;
    lt_u16 period;
    lta_u32 value;
} lta_request;

typedef struct {
    lt_chip *chip;
    lt_u8 token, mask, active, muted, playing, kind[4], level[4];
    lt_u16 period[3];
    lt_u8 noise_mode, clock_valid, deadline_mask;
    lta_u32 last_now, deadline[4];
    /* Private overlay state used by the caller-driven clip player. */
    lt_u8 effect_mask, effect_active, effect_playing, effect_kind[4];
    lt_u8 effect_level[4], effect_noise_mode;
    lt_u8 music_attenuation[4], sfx_attenuation;
    lt_u8 music_paused, effect_paused;
} lta_handle;

#define LTA_KIND_NONE 0U
#define LTA_KIND_TONE 1U
#define LTA_KIND_NOISE 2U

/* token must be nonzero and unique among live handles. chip must already be
 * initialized with lt_init. Init is silent; do not reinitialize a live chip
 * or a live handle. The handle, chip, and request must not overlap. */
lta_status lta_init(lta_handle *handle, lt_chip *chip, lt_u8 token);
void lta_request_init(lta_request *request);
lt_u16 lta_capabilities(void);
/* Serialized access only: commands sharing a chip must not run concurrently
 * or reentrantly. A handle exclusively manages its claimed
 * channels; disjoint handles may share the chip with unique tokens. Callers
 * must honor tone2/coupled-noise semantics and exclude all external writers.
 * TONE/NOISE_UPDATE require a playing voice; START restarts it and cancels its
 * deadline. STOP mutes/cancels deadlines but keeps claims. Request and handle
 * must not overlap. Capabilities are static; no hardware/frequency detection. */
lta_status lta_command(lta_handle *handle, const lta_request *request);

/* Add 0..15 PSG attenuation steps, saturating at silence. Original levels
 * remain intact. Applies immediately to audible voices, including overlays.
 * Init/acquire resets both controls to zero. Serialized like commands. These
 * PSG controls do not change the PC speaker's fixed output amplitude. */
lta_status lta_music_attenuation(lta_handle *handle, lt_u8 channel,
                                  lt_u8 attenuation);
lta_status lta_sfx_attenuation(lta_handle *handle, lt_u8 attenuation);
/* Silence one layer, preserving logical levels and ownership. Transport
 * players freeze their own cursors separately. Effect pause retains claims;
 * music underneath keeps progressing. Resume restores adjusted volumes. */
lta_status lta_layer_pause(lta_handle *handle, lt_u8 effect, lt_u8 paused);

/* Internal interface for the reusable clip player in lt_sfx.h. Applications
 * should use lt_sfx_* instead of calling these overlay helpers directly. */
lta_status lta_overlay_begin(lta_handle *handle, lt_u8 mask,
                             lt_u8 *borrowed_mask);
lta_status lta_overlay_tone(lta_handle *handle, lt_u8 channel,
                            lt_u16 period, lt_u8 level);
lta_status lta_overlay_noise(lta_handle *handle, lt_u8 mode, lt_u8 level);
lta_status lta_overlay_stop(lta_handle *handle, lt_u8 mask);
lta_status lta_overlay_end(lta_handle *handle);

#endif
