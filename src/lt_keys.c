/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "lt_keys.h"

static void ltkb_clear(unsigned char *bits)
{
    unsigned i;
    for (i = 0; i < LTKB_BYTES; ++i) bits[i] = 0;
}

void ltkb_decoder_init(ltkb_decoder *d)
{
    if (d == 0) return;
    ltkb_clear(d->live.held);
    ltkb_clear(d->live.pressed);
    ltkb_clear(d->live.released);
    d->prefix = 0;
    d->skip = 0;
    d->bytes = 0;
}

void ltkb_feed(ltkb_decoder *d, unsigned char scan)
{
    unsigned char code, bit, *byte;
    if (d == 0) return;
    ++d->bytes;
    if (d->skip) {                      /* rest of the Pause sequence */
        --d->skip;
        return;
    }
    if (scan == 0xE0U) {
        d->prefix = 1;
        return;
    }
    if (scan == 0xE1U) {
        d->skip = 5;
        d->prefix = 0;
        return;
    }
    code = (unsigned char)(scan & 0x7FU);
    if (d->prefix) {
        d->prefix = 0;
        if (code == 0x2AU || code == 0x36U) return;     /* fake shifts */
    }
    if (code == 0U) return;             /* overrun/error byte */
    bit = (unsigned char)(1U << (code & 7U));
    byte = &d->live.held[code >> 3];
    if (scan & 0x80U) {
        if (*byte & bit) d->live.released[code >> 3] |= bit;
        *byte &= (unsigned char)~bit;
    } else {
        if (!(*byte & bit)) d->live.pressed[code >> 3] |= bit;
        *byte |= bit;                   /* typematic repeats are not presses */
    }
}

void ltkb_take(ltkb_decoder *d, ltkb_state *out)
{
    if (d == 0 || out == 0) return;
    memcpy(out, &d->live, sizeof(*out));
    memset(d->live.pressed, 0, LTKB_BYTES);
    memset(d->live.released, 0, LTKB_BYTES);
}

static int ltkb_bit(const unsigned char *bits, unsigned scan)
{
    if (scan >= LTKB_KEYS) return 0;
    return (bits[scan >> 3] >> (scan & 7U)) & 1U;
}

int ltkb_held(const ltkb_state *s, unsigned scan)
{
    return s != 0 && ltkb_bit(s->held, scan);
}

int ltkb_pressed(const ltkb_state *s, unsigned scan)
{
    return s != 0 && ltkb_bit(s->pressed, scan);
}

int ltkb_released(const ltkb_state *s, unsigned scan)
{
    return s != 0 && ltkb_bit(s->released, scan);
}

int ltkb_active(const ltkb_state *s, unsigned scan)
{
    return ltkb_held(s, scan) || ltkb_pressed(s, scan);
}
