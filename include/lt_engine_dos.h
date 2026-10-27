/* SPDX-License-Identifier: MIT */
#ifndef LT_ENGINE_DOS_H
#define LT_ENGINE_DOS_H
#include "lt_engine_view.h"
typedef struct {
    lte_view view;
    unsigned char active, mode, page, cursor_start, cursor_end, row, col;
} lte_dos_video;
#define LTE_DOS_VIDEO_INITIALIZER { {0,0,0,0,0,0}, 0,0,0,0,0,0,0 }
/* Foreground Open Watcom DOS only. Caller asserts compatible Tandy hardware
 * for modes 8/9 (BIOS mode query is not hardware detection). Saves/restores
 * mode/page/cursor, not old screen contents or custom palettes/registers.
 * No sound or timer ownership. Exclusive video owner; object must not move. */
int lte_dos_video_open(lte_dos_video *, unsigned mode);
void lte_dos_video_close(lte_dos_video *);
#endif
