/* SPDX-License-Identifier: MIT */
#ifndef LT_DOS_APP_H
#define LT_DOS_APP_H

#include "lt_gfx.h"
#include "lt_input_dos.h"
#include "lt_sound.h"

/* Optional foreground convenience layer for a compatible Tandy DOS system.
 * The low-level graphics, input, and sound contracts remain available. */
typedef enum {
    LT_DOS_APP_OK = 0,
    LT_DOS_APP_INVALID = 1,
    LT_DOS_APP_VIDEO_FAILED = 2,
    LT_DOS_APP_AUDIO_FAILED = 3
} lt_dos_app_status;

#define LT_DOS_APP_SOUND_OWNER 1U
#define LT_DOS_APP_SOUND_MASK 15U
#define LT_DOS_APP_SOUND_PORT 0x00C0U

typedef struct {
    unsigned char mode;
    unsigned char page;
    unsigned char cursor_start;
    unsigned char cursor_end;
    unsigned char cursor_row;
    unsigned char cursor_column;
} lt_dos_video_state;

typedef struct {
    /* Borrowed direct mode-9 aperture. It is valid until shutdown. */
    lt_gfx_surface screen;
    /* This app owns the chip and all four channels while initialized. */
    lt_chip chip;
    /* Local BIOS-polling state only; no ISR, callback, or resident driver. */
    lti_dos_input input;
    unsigned short sound_port;
    lt_dos_video_state previous_video;
    unsigned char initialized;
} lt_dos_app;

/* Enters mode 9, hides the text cursor, binds the direct video aperture,
 * initializes BIOS input state, and claims the four sound channels.
 * No allocation or resident service is used. Call only on an inactive object. */
lt_dos_app_status lt_dos_app_init(lt_dos_app *app);

/* Mutes/unclaims sound, clears local input state, restores video mode/page and
 * cursor position/shape. Safe to call after a failed init or more than once. */
void lt_dos_app_shutdown(lt_dos_app *app);

#endif
