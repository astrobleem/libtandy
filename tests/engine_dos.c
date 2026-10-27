/* SPDX-License-Identifier: MIT */
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include "lt_engine_dos.h"
static void snapshot(unsigned char state[6])
{
    union REGS r;
    memset(&r, 0, sizeof(r)); r.h.ah = 15;
    int86(0x10, &r, &r); state[0] = r.h.al; state[1] = r.h.bh;
    memset(&r, 0, sizeof(r)); r.h.ah = 3; r.h.bh = state[1];
    int86(0x10, &r, &r);
    state[2] = r.h.ch; state[3] = r.h.cl; state[4] = r.h.dh; state[5] = r.h.dl;
}
int main(void)
{
    lte_dos_video v = LTE_DOS_VIDEO_INITIALIZER;
    unsigned char before[6], after[6];
    unsigned modes[4] = {1, 3, 8, 9}, i;
    unsigned short vector[4];
    const volatile unsigned short __far *ivt =
        (const volatile unsigned short __far *)MK_FP(0, 0x20);
    int pass = 1;
    for (i = 0; i < 4; ++i) vector[i] = ivt[i]; /* IRQ0 and IRQ1 */
    snapshot(before);
    if (lte_dos_video_open(&v, 4) || v.active) pass = 0;
    for (i = 0; i < 4 && pass; ++i) {
        if (!lte_dos_video_open(&v, modes[i])) { pass = 0; break; }
        snapshot(after);
        if (after[0] != modes[i] || after[1] != 0) pass = 0;
        if (lte_dos_video_open(&v, modes[i])) pass = 0;
        lte_view_rect(&v.view, -2, -2, 20, 20, 15, '@');
        lte_dos_video_close(&v);
        lte_dos_video_close(&v);
        snapshot(after);
        if (memcmp(before, after, sizeof(before)) || v.active || v.view.pixels) pass = 0;
    }
    for (i = 0; i < 4; ++i) if (vector[i] != ivt[i]) pass = 0;
    lte_dos_video_close(&v);
    printf("engine DOS mode/page/cursor restore, double-open/close, invalid mode, "
           "IRQ0/1 unchanged: %s; video=%u bytes\n", pass ? "PASS" : "FAIL",
           (unsigned)sizeof(v));
    return pass ? 0 : 1;
}
