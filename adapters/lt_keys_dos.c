/* SPDX-License-Identifier: MIT */
/* IRQ1 held-key service; see lt_keys_dos.h. */
#if !defined(__WATCOMC__) || !defined(__I86__)
#error lt_keys_dos.c requires Open Watcom 16-bit x86
#endif
#include <conio.h>
#include <dos.h>
#include "lt_keys_dos.h"

typedef void (__interrupt __far *ltkb_vector)(void);

static ltkb_decoder keys;
static ltkb_vector previous;
static unsigned char open_flags, is_open;

static unsigned ltkb_cli(void)
{
    unsigned f = 0;
    _asm {
        pushf
        pop f
        cli
    }
    return f;
}

static void ltkb_sti(unsigned f)
{
    _asm {
        push f
        popf
    }
}

static void __interrupt __far ltkb_isr(void)
{
    unsigned char scan = (unsigned char)inp(0x60);
    unsigned char ctl;
    ltkb_feed(&keys, scan);
    if (open_flags & LTKB_CHAIN_BIOS) {
        _chain_intr(previous);          /* BIOS reads, acks and EOIs */
    }
    ctl = (unsigned char)inp(0x61);     /* keyboard acknowledge */
    outp(0x61, ctl | 0x80);
    outp(0x61, ctl);
    outp(0x20, 0x20);
}

ltkb_status ltkb_open(unsigned flags)
{
    unsigned f;
    if (is_open) return LTKB_BUSY;
    f = ltkb_cli();
    ltkb_decoder_init(&keys);
    open_flags = (unsigned char)flags;
    previous = _dos_getvect(9);
    _dos_setvect(9, ltkb_isr);
    is_open = 1;
    ltkb_sti(f);
    return LTKB_OK;
}

ltkb_status ltkb_close(void)
{
    unsigned f;
    ltkb_status st = LTKB_OK;
    if (!is_open) return LTKB_NOT_OPEN;
    f = ltkb_cli();
    if (_dos_getvect(9) == ltkb_isr) _dos_setvect(9, previous);
    else st = LTKB_NOT_OURS;
    if (!(open_flags & LTKB_CHAIN_BIOS)) {
        /* BIOS data area: 0417h shift/Ctrl/Alt (keep the lock toggles),
         * 0418h held-key bits, 041Ah/041Ch buffer head/tail. */
        unsigned char __far *bda = (unsigned char __far *)MK_FP(0x40, 0);
        bda[0x17] &= 0xF0U;
        bda[0x18] = 0;
        *(unsigned __far *)(bda + 0x1A) = *(unsigned __far *)(bda + 0x1C);
    }
    ltkb_decoder_init(&keys);
    is_open = 0;
    ltkb_sti(f);
    return st;
}

int ltkb_is_open(void)
{
    return is_open;
}

void ltkb_poll(ltkb_state *out)
{
    unsigned f = ltkb_cli();
    ltkb_take(&keys, out);
    ltkb_sti(f);
}

int ltkb_down_now(unsigned scan)
{
    if (scan >= LTKB_KEYS) return 0;
    return LTKB_BIT(keys.live.held, scan);
}

unsigned ltkb_bytes(void)
{
    unsigned f = ltkb_cli(), n = keys.bytes;
    ltkb_sti(f);
    return n;
}
