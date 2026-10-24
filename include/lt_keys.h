/* SPDX-License-Identifier: MIT */
#ifndef LT_KEYS_H
#define LT_KEYS_H

/* Held-key state from raw keyboard scan bytes (XT set 1, as the Tandy 1000
 * and PC keyboards deliver at port 60h). Portable C89: the DOS adapter
 * (lt_keys_dos.h) feeds bytes from its IRQ1 handler; tests feed them
 * directly.
 *
 * Keys are numbered by their 7-bit make code (1 = Escape, 0x48 = Up on the
 * keypad, ...). Enhanced-keyboard E0-prefixed keys report the code of the
 * keypad key they duplicate (gray Up = 0x48); the E0 2A/E0 36 "fake shift"
 * bytes those keyboards add are ignored, and the Pause sequence (E1 ...) is
 * skipped. Tandy 1000 keyboards send their own codes for keys such as the
 * arrow keys (Up 29h, Left 2Bh, Down 4Ah, Right 4Eh); map them in the game. */
#define LTKB_KEYS 128U
#define LTKB_BYTES 16U

typedef struct {
    /* Down now. */
    unsigned char held[LTKB_BYTES];
    /* Went down / up at least once since the previous poll. A key tapped
     * between two polls is in both and not in held. */
    unsigned char pressed[LTKB_BYTES];
    unsigned char released[LTKB_BYTES];
} ltkb_state;

/* Decoder state: the live bitmaps plus prefix tracking. */
typedef struct {
    ltkb_state live;
    unsigned char prefix;       /* last byte was E0 */
    unsigned char skip;         /* bytes left of an E1 sequence */
    unsigned bytes;             /* scan bytes seen (wraps) */
} ltkb_decoder;

void ltkb_decoder_init(ltkb_decoder *d);
/* One scan byte. Interrupt-safe: no calls, bounded work. */
void ltkb_feed(ltkb_decoder *d, unsigned char scan);
/* Copy the live state to out and clear pressed/released in d. The caller
 * provides any needed exclusion (the DOS adapter disables IRQs). */
void ltkb_take(ltkb_decoder *d, ltkb_state *out);

/* Queries on a snapshot; scan 0..127. */
int ltkb_held(const ltkb_state *s, unsigned scan);
int ltkb_pressed(const ltkb_state *s, unsigned scan);
int ltkb_released(const ltkb_state *s, unsigned scan);
/* Held now, or pressed since the last poll: a tap shorter than one poll
 * interval still counts for one poll. */
int ltkb_active(const ltkb_state *s, unsigned scan);

#endif
