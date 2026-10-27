/* SPDX-License-Identifier: MIT */
#if !defined(__WATCOMC__) || !defined(__I86__)
#error lt_engine_dos requires 16-bit Open Watcom
#endif
#include <dos.h>
#include <string.h>
#include "lt_engine_dos.h"
static void bios(unsigned ax, unsigned bx, unsigned cx, unsigned dx)
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.x.ax = ax; r.x.bx = bx; r.x.cx = cx; r.x.dx = dx;
    int86(0x10, &r, &r);
}
void lte_dos_video_close(lte_dos_video *v)
{
    if (!v || !v->active) return;
    bios(v->mode, 0, 0, 0);
    bios(0x0500U | v->page, 0, 0, 0);
    bios(0x0100U, 0, ((unsigned)v->cursor_start << 8) | v->cursor_end, 0);
    bios(0x0200U, (unsigned)v->page << 8, 0,
         ((unsigned)v->row << 8) | v->col);
    v->active = 0; v->view.pixels = 0;
}
int lte_dos_video_open(lte_dos_video *v, unsigned mode)
{
    union REGS r;
    lte_view view;
    if (!v || v->active || !lte_view_bind(&view, mode,
        (unsigned char LTE_FAR *)MK_FP(0xb800, 0), 32768U)) return 0;
    memset(&r, 0, sizeof(r)); r.h.ah = 15;
    int86(0x10, &r, &r); v->mode = r.h.al; v->page = r.h.bh;
    memset(&r, 0, sizeof(r)); r.h.ah = 3; r.h.bh = v->page;
    int86(0x10, &r, &r);
    v->cursor_start = r.h.ch; v->cursor_end = r.h.cl;
    v->row = r.h.dh; v->col = r.h.dl; v->active = 1;
    bios(mode, 0, 0, 0);
    memset(&r, 0, sizeof(r)); r.h.ah = 15;
    int86(0x10, &r, &r);
    if (r.h.al != mode) { lte_dos_video_close(v); return 0; }
    bios(0x0500, 0, 0, 0); /* page zero matches the borrowed B800 aperture */
    bios(0x0100, 0, 0x2000, 0);
    v->view = view;
    return 1;
}
