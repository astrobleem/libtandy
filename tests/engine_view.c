/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_engine_view.h"
#define CHECK(v) do { if (!(v)) { printf("FAIL engine view line %d\n", __LINE__); return 1; } } while (0)
/* Far: the 32 KiB fixture must not consume DOS small-model DGROUP. */
static unsigned char LTE_FAR pixels[32770U];
int main(void)
{
    lte_view view, saved;
    unsigned i;
    CHECK(lte_view_bind(&view, 8, pixels + 1, 16384));
    saved = view;
    CHECK(!lte_view_bind(&view, 8, pixels + 1, 16383));
    CHECK(!memcmp(&saved, &view, sizeof(view)));
    CHECK(!lte_view_bind(&view, 4, pixels + 1, 32768U));
    for (i = 0; i < 32770U; ++i) pixels[i] = 0x5a;
    lte_view_rect(&view, 0, 0, 160, 200, 3, 0);
    CHECK(pixels[1] == 0x33 && pixels[8193] == 0x33);
    CHECK(pixels[1 + 99 * 80] == 0x33);
    CHECK(pixels[1 + 8192 + 99 * 80 + 79] == 0x33);
    CHECK(pixels[0] == 0x5a && pixels[16385] == 0x5a);
    CHECK(pixels[1 + 8000] == 0x5a); /* bank padding untouched */
    lte_view_rect(&view, 1, 0, 2, 1, 14, 0);
    CHECK(pixels[1] == 0x3e && pixels[2] == 0xe3);
    lte_view_rect(&view, -2, -2, 3, 3, 7, 0);
    CHECK(pixels[1] == 0x7e);
    CHECK(lte_view_bind(&view, 9, pixels + 1, 32768U));
    lte_view_rect(&view, 319, 199, 30000, 30000, 15, 0);
    CHECK(pixels[1 + 3 * 8192 + 49 * 160 + 159] == 0x5f);
    CHECK(pixels[32769U] == 0x5a);
    CHECK(lte_view_bind(&view, 3, pixels + 1, 4000));
    lte_view_rect(&view, 0, 0, 80, 25, 0x1e, 219);
    CHECK(pixels[1] == 219 && pixels[2] == 0x1e);
    CHECK(pixels[3999] == 219 && pixels[4000] == 0x1e);
    CHECK(lte_view_bind(&view, 1, pixels + 1, 2000));
    lte_view_rect(&view, 39, 24, 2, 2, 0x2f, '@');
    CHECK(pixels[1999] == '@' && pixels[2000] == 0x2f);
    CHECK(pixels[2001] == 219); /* clip at 40x25 */
    printf("engine view passed native 8/9 banks, nibble edges, text 1/3, clipping; view=%u bytes\n", (unsigned)sizeof(view));
    return 0;
}
