/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LT_SFX_JAWS_CLIPS_H
#define LT_SFX_JAWS_CLIPS_H

/* Source-derived JAWS M2 cues. See COPYING and README.md in this directory. */
#include "lt_sfx.h"

#define JAWS_SFX_TONE(t, p, a) \
    { (t), LTSFX_OP_TONE, 1U, (lt_u8)(p), (lt_u8)((p) >> 8), (a) }

static const lt_sfx_event lt_sfx_jaws_harpoon_events[] = {
    JAWS_SFX_TONE(0U,  96U, 4U),
    JAWS_SFX_TONE(1U,  48U, 4U),
    JAWS_SFX_TONE(2U,  24U, 4U),
    JAWS_SFX_TONE(3U,  51U, 4U),
    JAWS_SFX_TONE(4U, 101U, 4U),
    JAWS_SFX_TONE(5U,  96U, 4U),
    JAWS_SFX_TONE(6U, 191U, 4U),
    JAWS_SFX_TONE(7U, 180U, 4U),
    JAWS_SFX_TONE(8U, 360U, 4U),
    JAWS_SFX_TONE(9U, 340U, 4U)
};

static const lt_sfx_clip lt_sfx_jaws_harpoon = {
    LTSFX_CLIP_VERSION, 2U, 3U, 10U, 11U,
    LTSFX_PROVENANCE_JAWS_M2,
    (lt_u16)sizeof(lt_sfx_jaws_harpoon_events),
    150247UL, 2500UL,
    lt_sfx_jaws_harpoon_events
};

static const lt_sfx_event lt_sfx_jaws_conch_events[] = {
    JAWS_SFX_TONE( 0U, 161U,  1U),
    JAWS_SFX_TONE( 2U,  81U,  1U),
    JAWS_SFX_TONE( 4U, 143U,  1U),
    JAWS_SFX_TONE( 6U,  72U,  1U),
    JAWS_SFX_TONE( 8U, 128U,  1U),
    JAWS_SFX_TONE(10U,  64U,  1U),
    JAWS_SFX_TONE(12U, 114U,  1U),
    JAWS_SFX_TONE(14U,  57U,  1U),
    JAWS_SFX_TONE(16U, 161U,  5U),
    JAWS_SFX_TONE(18U,  81U,  5U),
    JAWS_SFX_TONE(20U, 143U,  5U),
    JAWS_SFX_TONE(22U,  72U,  5U),
    JAWS_SFX_TONE(24U, 128U,  5U),
    JAWS_SFX_TONE(26U,  64U,  5U),
    JAWS_SFX_TONE(28U, 114U,  5U),
    JAWS_SFX_TONE(30U,  57U,  5U),
    JAWS_SFX_TONE(32U, 161U,  9U),
    JAWS_SFX_TONE(34U,  81U,  9U),
    JAWS_SFX_TONE(36U, 143U,  9U),
    JAWS_SFX_TONE(38U,  72U,  9U),
    JAWS_SFX_TONE(40U, 128U,  9U),
    JAWS_SFX_TONE(42U,  64U,  9U),
    JAWS_SFX_TONE(44U, 114U,  9U),
    JAWS_SFX_TONE(46U,  57U,  9U),
    JAWS_SFX_TONE(48U, 161U, 13U),
    JAWS_SFX_TONE(50U,  81U, 13U),
    JAWS_SFX_TONE(52U, 143U, 13U),
    JAWS_SFX_TONE(54U,  72U, 13U),
    JAWS_SFX_TONE(56U, 128U, 13U),
    JAWS_SFX_TONE(58U,  64U, 13U),
    JAWS_SFX_TONE(60U, 114U, 13U),
    JAWS_SFX_TONE(62U,  57U, 13U)
};

static const lt_sfx_clip lt_sfx_jaws_conch = {
    LTSFX_CLIP_VERSION, 2U, 4U, 32U, 65U,
    LTSFX_PROVENANCE_JAWS_M2,
    (lt_u16)sizeof(lt_sfx_jaws_conch_events),
    150247UL, 2500UL,
    lt_sfx_jaws_conch_events
};

#undef JAWS_SFX_TONE

#endif
