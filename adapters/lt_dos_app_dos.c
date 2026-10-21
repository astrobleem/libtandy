/* SPDX-License-Identifier: MIT */
#if !defined(__WATCOMC__) || !defined(__I86__)
#error This optional application layer requires Open Watcom 16-bit x86
#endif

#include <dos.h>
#include <string.h>
#include "lt_dos_app_platform.h"

extern void lt_dos_emit(void *context, lt_u8 byte);

int ltdp_video_snapshot(lt_dos_video_state *state)
{
    union REGS regs;

    if (state == 0) return 0;
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 15U;
    int86(0x10, &regs, &regs);
    state->mode = regs.h.al;
    state->page = regs.h.bh;

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 3U;
    regs.h.bh = state->page;
    int86(0x10, &regs, &regs);
    state->cursor_start = regs.h.ch;
    state->cursor_end = regs.h.cl;
    state->cursor_row = regs.h.dh;
    state->cursor_column = regs.h.dl;
    return 1;
}

int ltdp_video_set_mode(unsigned char mode)
{
    union REGS regs;

    memset(&regs, 0, sizeof(regs));
    regs.x.ax = mode;
    int86(0x10, &regs, &regs);

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 15U;
    int86(0x10, &regs, &regs);
    return regs.h.al == mode;
}

void ltdp_video_hide_cursor(void)
{
    union REGS regs;

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 1U;
    regs.x.cx = 0x2000U;
    int86(0x10, &regs, &regs);
}

void ltdp_video_restore(const lt_dos_video_state *state)
{
    union REGS regs;

    if (state == 0) return;
    memset(&regs, 0, sizeof(regs));
    regs.x.ax = state->mode;
    int86(0x10, &regs, &regs);

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 5U;
    regs.h.al = state->page;
    int86(0x10, &regs, &regs);

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 1U;
    regs.h.ch = state->cursor_start;
    regs.h.cl = state->cursor_end;
    int86(0x10, &regs, &regs);

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 2U;
    regs.h.bh = state->page;
    regs.h.dh = state->cursor_row;
    regs.h.dl = state->cursor_column;
    int86(0x10, &regs, &regs);
}

lt_gfx_u8 LT_GFX_FAR *ltdp_video_aperture(void)
{
    return (lt_gfx_u8 LT_GFX_FAR *)MK_FP(0xB800, 0);
}

void ltdp_audio_emit(void *context, lt_u8 byte)
{
    lt_dos_emit(context, byte);
}
