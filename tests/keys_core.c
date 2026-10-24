/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "lt_keys.h"

static int failures;
#define CHECK(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++failures; \
} } while (0)

static void feed(ltkb_decoder *d, const unsigned char *b, unsigned n)
{
    while (n--) ltkb_feed(d, *b++);
}

int main(void)
{
    ltkb_decoder d;
    ltkb_state s;
    static const unsigned char three[] = { 0x48, 0x4B, 0x39 };
    static const unsigned char gray_up[] = { 0xE0, 0x2A, 0xE0, 0x48 };
    static const unsigned char gray_up_off[] = { 0xE0, 0xC8, 0xE0, 0xAA };
    static const unsigned char pause[] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
    static const unsigned char tap[] = { 0x1E, 0x9E };

    ltkb_decoder_init(&d);
    /* Several keys held at once; repeats are not new presses. */
    feed(&d, three, 3U);
    ltkb_take(&d, &s);
    CHECK(ltkb_held(&s, 0x48) && ltkb_held(&s, 0x4B) && ltkb_held(&s, 0x39));
    CHECK(ltkb_pressed(&s, 0x48) && ltkb_pressed(&s, 0x39));
    ltkb_feed(&d, 0x48);                         /* typematic repeat */
    ltkb_take(&d, &s);
    CHECK(ltkb_held(&s, 0x48) && !ltkb_pressed(&s, 0x48));
    ltkb_feed(&d, 0xCB);                         /* Left up */
    ltkb_take(&d, &s);
    CHECK(!ltkb_held(&s, 0x4B) && ltkb_released(&s, 0x4B));
    CHECK(ltkb_held(&s, 0x48) && ltkb_held(&s, 0x39));
    ltkb_take(&d, &s);
    CHECK(!ltkb_released(&s, 0x4B));
    /* Gray keys map to the keypad code; fake shifts are ignored. */
    ltkb_decoder_init(&d);
    feed(&d, gray_up, 4U);
    ltkb_take(&d, &s);
    CHECK(ltkb_held(&s, 0x48) && !ltkb_held(&s, 0x2A));
    feed(&d, gray_up_off, 4U);
    ltkb_take(&d, &s);
    CHECK(!ltkb_held(&s, 0x48) && ltkb_released(&s, 0x48));
    /* A real left shift is a key. */
    ltkb_feed(&d, 0x2A);
    ltkb_take(&d, &s);
    CHECK(ltkb_held(&s, 0x2A));
    /* Pause is skipped entirely. */
    ltkb_decoder_init(&d);
    feed(&d, pause, 6U);
    ltkb_take(&d, &s);
    CHECK(!ltkb_held(&s, 0x1D) && !ltkb_held(&s, 0x45) &&
          !ltkb_pressed(&s, 0x45));
    ltkb_feed(&d, 0x1C);
    ltkb_take(&d, &s);
    CHECK(ltkb_held(&s, 0x1C));
    /* A tap between polls: pressed and released, not held, but active. */
    ltkb_decoder_init(&d);
    feed(&d, tap, 2U);
    ltkb_take(&d, &s);
    CHECK(!ltkb_held(&s, 0x1E) && ltkb_pressed(&s, 0x1E) &&
          ltkb_released(&s, 0x1E) && ltkb_active(&s, 0x1E));
    ltkb_take(&d, &s);
    CHECK(!ltkb_active(&s, 0x1E));
    CHECK(!ltkb_held(&s, 200U) && !ltkb_held(0, 1U));
    CHECK(d.bytes == 2U);
    if (failures) return 1;
    puts("keys core tests passed");
    return 0;
}
