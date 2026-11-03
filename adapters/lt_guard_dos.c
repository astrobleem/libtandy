/* SPDX-License-Identifier: MIT */
/* Optional DOS guard. Direct handlers do not replace the CRT _harderr policy. */
#if !defined(__WATCOMC__) || !defined(__I86__) || !defined(__DOS__)
#error lt_guard_dos requires Open Watcom 16-bit DOS
#endif
#if !defined(__SW_ZU) || !__SW_ZU || !defined(__SW_S) || !__SW_S
#error Compile lt_guard_dos.c separately with -s -zu (foreign interrupt stack)
#endif
#include "lt_guard_dos.h"
#include <dos.h>
#include <i86.h>
#include <stdlib.h>

typedef void (__interrupt __far *ltguard_vector)(void);
static ltguard_vector previous23, previous24;
static unsigned opened;
static volatile unsigned quit_flag, critical_flag;

static void __interrupt __far break_handler(void)
{
    quit_flag = 1;
}
static void __interrupt __far critical_handler(union INTPACK regs)
{
    quit_flag = 1;
    critical_flag = 1;
    /* INTPACK names the registers saved by Watcom's interrupt prologue.
     * Change only saved AL; IRET restores every other caller register.
     * Best effort: DOS may convert FAIL to ABORT if entry AH bit3 is clear.
     * Never invent an IGNORE result or retry potentially destructive I/O. */
    regs.h.al = 3;
}
int ltguard_open(void)
{
    if (opened) return LTGUARD_BUSY;
    if (_osmajor < 3) return LTGUARD_DOS_TOO_OLD;
    previous23 = _dos_getvect(0x23);
    previous24 = _dos_getvect(0x24);
    quit_flag = 0;
    critical_flag = 0;
    _dos_setvect(0x23, break_handler);
    _dos_setvect(0x24, (ltguard_vector)critical_handler);
    opened = 1;
    return LTGUARD_OK;
}
unsigned ltguard_close(void)
{
    unsigned result = 0;
    if (!opened) return 0;
    if (_dos_getvect(0x23) == break_handler)
        _dos_setvect(0x23, previous23);
    else result |= LTGUARD_LOST_BREAK;
    if (_dos_getvect(0x24) == (ltguard_vector)critical_handler)
        _dos_setvect(0x24, previous24);
    else result |= LTGUARD_LOST_CRITICAL;
    opened = 0;
    return result;
}
unsigned ltguard_quit(void) { return quit_flag; }
unsigned ltguard_critical(void) { return critical_flag; }
