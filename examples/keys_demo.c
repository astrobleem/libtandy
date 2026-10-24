/* SPDX-License-Identifier: MIT */
/*
 * KEYDEMO: held keys (lt_keys_dos.h). A 16 x 8 grid shows all 128 key
 * codes: green while held, yellow for one poll when pressed, red for one
 * poll when released. Hold several arrow keys (keypad, gray or Tandy arrows)
 * at once and the square moves diagonally. Escape (released) quits; the
 * DOS keyboard works normally afterwards.
 */
#include <stdio.h>
#include "lt_dos_app.h"
#include "lt_keys_dos.h"

static int any(const ltkb_state *s, unsigned a, unsigned b, unsigned c)
{
    return ltkb_held(s, a) || ltkb_held(s, b) || ltkb_held(s, c);
}

int main(void)
{
    lt_dos_app app = LT_DOS_APP_INITIALIZER;
    ltkb_state keys;
    unsigned k, most = 0, n;
    lt_gfx_i16 x = 150, y = 150, cx, cy;
    lt_gfx_u8 c;

    if (lt_dos_app_init(&app) != LT_DOS_APP_OK) {
        puts("KEYDEMO: no mode 9");
        return 1;
    }
    if (ltkb_open(0) != LTKB_OK) {
        lt_dos_app_shutdown(&app);
        puts("KEYDEMO: keyboard busy");
        return 1;
    }
    lt_gfx_clear(&app.screen, 0U);
    for (;;) {
        ltkb_poll(&keys);
        if (ltkb_released(&keys, 1U)) break;           /* Escape */
        for (k = 0, n = 0; k < LTKB_KEYS; ++k) {
            c = 8U;
            if (ltkb_held(&keys, k)) { c = 10U; ++n; }
            if (ltkb_pressed(&keys, k)) c = 14U;
            if (ltkb_released(&keys, k)) c = 12U;
            cx = (lt_gfx_i16)(32 + (k & 15U) * 16U);
            cy = (lt_gfx_i16)(8 + (k >> 4) * 14U);
            lt_gfx_rect(&app.screen, cx, cy, (lt_gfx_i16)(cx + 12),
                        (lt_gfx_i16)(cy + 10), c);
        }
        if (n > most) most = n;
        lt_gfx_rect(&app.screen, x, y, (lt_gfx_i16)(x + 8),
                    (lt_gfx_i16)(y + 8), 0U);
        if (any(&keys, 0x4BU, 0x2BU, 0x1EU) && x > 0) --x;     /* left */
        if (any(&keys, 0x4DU, 0x4EU, 0x20U) && x < 311) ++x;   /* right */
        if (any(&keys, 0x48U, 0x29U, 0x11U) && y > 124) --y;   /* up */
        if (any(&keys, 0x50U, 0x4AU, 0x1FU) && y < 191) ++y;   /* down */
        lt_gfx_rect(&app.screen, x, y, (lt_gfx_i16)(x + 8),
                    (lt_gfx_i16)(y + 8), 15U);
    }
    k = ltkb_bytes();
    n = ltkb_close() == LTKB_OK;
    lt_dos_app_shutdown(&app);
    printf("KEYDEMO scan bytes=%u most keys held at once=%u restored=%u\n",
           k, most, n);
    return 0;
}
