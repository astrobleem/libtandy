/* SPDX-License-Identifier: MIT */
#ifndef LT_KEYS_DOS_H
#define LT_KEYS_DOS_H
#include "lt_keys.h"

/* IRQ1 keyboard service for one foreground DOS program (Open Watcom,
 * 16-bit). While open, an interrupt handler reads each scan byte from port
 * 60h into an lt_keys.h decoder, so any number of keys can be held at once
 * and every press/release between two polls is seen.
 *
 * Default mode: the handler acknowledges the keyboard (port 61h bit 7
 * pulse, as the PC/XT and Tandy 1000 BIOS do) and the PIC itself; the BIOS
 * keyboard handler does not run, so the BIOS buffer does not fill, there are
 * no buffer-full beeps, and Ctrl-C, Ctrl-Break and Ctrl-Alt-Del are not
 * acted on while the game runs. LTKB_CHAIN_BIOS instead passes every byte on
 * to the previous handler as well (BIOS buffer, shift states and
 * Ctrl-Break work; the program must then drain the buffer and handle
 * INT 23h/1Bh itself).
 *
 * Close puts the previous vector back if ours is still installed. In the
 * default mode it also clears the BIOS shift/Ctrl/Alt state bits and
 * empties the BIOS key buffer, because releases the BIOS never saw would
 * otherwise leave a modifier stuck for DOS. Toggle states (Caps/Num/Scroll
 * Lock) are kept. */
#define LTKB_CHAIN_BIOS 1U

typedef enum {
    LTKB_OK = 0,
    LTKB_BUSY = 1,          /* already open */
    LTKB_NOT_OPEN = 2,
    LTKB_NOT_OURS = 3       /* another program replaced the vector; it is
                               left in place and the service is closed */
} ltkb_status;

ltkb_status ltkb_open(unsigned flags);
ltkb_status ltkb_close(void);
int ltkb_is_open(void);
/* Snapshot of the keys since the previous poll (see lt_keys.h). Valid also
 * when closed (all clear after close). */
void ltkb_poll(ltkb_state *out);
/* Is the key down right now? A single-byte read of the live state, no
 * snapshot: cheap enough for a per-frame check such as a quit key. Does
 * not consume presses/releases. */
int ltkb_down_now(unsigned scan);
/* Scan bytes received since open (diagnostics). */
unsigned ltkb_bytes(void);

#endif
