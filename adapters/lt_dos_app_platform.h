/* SPDX-License-Identifier: MIT */
#ifndef LT_DOS_APP_PLATFORM_H
#define LT_DOS_APP_PLATFORM_H

#include "lt_dos_app.h"

int ltdp_video_snapshot(lt_dos_video_state *state);
int ltdp_video_set_mode(unsigned char mode);
void ltdp_video_hide_cursor(void);
void ltdp_video_restore(const lt_dos_video_state *state);
lt_gfx_u8 LT_GFX_FAR *ltdp_video_aperture(void);
void ltdp_audio_emit(void *context, lt_u8 byte);

#endif
