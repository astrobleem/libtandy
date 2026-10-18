/* SPDX-License-Identifier: MIT */
/* Original synthetic demonstration; no game assets. */
#include <conio.h>
#include <dos.h>
#include "lt_gfx.h"
#include "lt_sound.h"

extern void lt_dos_emit(void *, lt_u8);

static unsigned short port = 0x00C0;
static lt_chip chip;
static lt_gfx_surface screen;
static const unsigned char glyph[8] = {
    0xFF, 0x81, 0xA5, 0x81, 0xBD, 0x81, 0x81, 0xFF
};

int main(void)
{
    union REGS regs;
    unsigned old_mode, i;
    regs.h.ah = 15;
    int86(0x10, &regs, &regs);
    old_mode = regs.h.al;
    regs.x.ax = 9;
    int86(0x10, &regs, &regs);
    lt_gfx_init(&screen, MK_FP(0xB800, 0));
    lt_gfx_clear(&screen, 1);
    lt_gfx_glyph8(&screen, 156, 96, glyph, 15, 0, 1);
    lt_init(&chip, lt_dos_emit, &port);
    if (!lt_claim(&chip, 1, 15)) {
        regs.x.ax = old_mode;
        int86(0x10, &regs, &regs);
        return 1;
    }
    for (i = 0; i < 4; ++i) lt_attenuate(&chip, 1, (lt_u8)i, 15, 1);
    lt_tone(&chip, 1, 0, 256, 1);
    lt_attenuate(&chip, 1, 0, 8, 1);
    getch();
    lt_stop(&chip, 1, 15);
    regs.x.ax = old_mode;
    int86(0x10, &regs, &regs);
    return 0;
}
