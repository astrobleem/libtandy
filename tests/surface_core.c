/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_surface.h"

static lts_u8 guarded[514];
static lts_storage storage;
static lts_surface a, b, c;
static int failures;

#define CHECK(test) do { if (!(test)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #test); ++failures; \
} } while (0)

int main(void)
{
    lts_storage old_storage;
    lts_storage saved_storage;
    lts_surface old_surface;
    lts_surface saved_surface;
    lts_rect r;
    lts_u8 before[514];
    unsigned int i;
    memset(guarded, 0xEE, sizeof(guarded));
    CHECK(lts_storage_init(&storage, guarded + 1, 512U) == LTS_OK);
    CHECK(lts_surface_init(&a, &storage, 8U, 8U, 6U, 12U, LTS_INDEX8) == LTS_OK);
    CHECK(lts_surface_init(&b, &storage, 40U, 8U, 6U, 12U, LTS_INDEX8) == LTS_OK);
    CHECK(lts_surface_init(&c, &storage, 50U, 8U, 6U, 10U, LTS_INDEX8) == LTS_OK);
    memset(&old_surface, 0x6B, sizeof(old_surface));
    memcpy(&saved_surface, &old_surface, sizeof(old_surface));
    CHECK(lts_surface_init(&old_surface, &storage, 500U, 8U, 6U, 12U,
                           LTS_INDEX8) == LTS_NOSPACE);
    CHECK(memcmp(&old_surface, &saved_surface, sizeof(old_surface)) == 0);
    CHECK(lts_surface_init(&old_surface, &storage, 0U, 8U, 6U, 8U, 9U) ==
          LTS_UNSUPPORTED);
    CHECK(memcmp(&old_surface, &saved_surface, sizeof(old_surface)) == 0);
    memcpy(&saved_surface, &a, sizeof(a));
    CHECK(lts_surface_init(&a, &storage, 500U, 8U, 6U, 12U,
                           LTS_INDEX8) == LTS_NOSPACE);
    CHECK(memcmp(&a, &saved_surface, sizeof(a)) == 0);
    memset(&old_storage, 0x5A, sizeof(old_storage));
    memcpy(&saved_storage, &old_storage, sizeof(old_storage));
    memcpy(before, guarded, sizeof(guarded));
    CHECK(lts_storage_init(&old_storage, guarded + 1, 65521U) == LTS_NOSPACE);
    CHECK(memcmp(&old_storage, &saved_storage, sizeof(old_storage)) == 0);
    CHECK(guarded[0] == before[0] && guarded[513] == before[513]);

    /* Public entry points reject mutated metadata before touching storage. */
    memcpy(before, guarded, sizeof(guarded));
    memcpy(&saved_surface, &a, sizeof(a));
    a.offset = 65530U;
    CHECK(lts_fill(&a, 0, 0x11U) == LTS_NOSPACE);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);
    a = saved_surface;
    a.pitch = 0U;
    CHECK(lts_key(&a, 1, 0U) == LTS_INVALID);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);
    a = saved_surface;
    a.width = 32768U; a.pitch = 32768U;
    CHECK(lts_clip(&a, 0) == LTS_INVALID);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);
    a = saved_surface;
    a.clip.x = -1;
    CHECK(lts_fill(&a, 0, 0x22U) == LTS_INVALID);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);
    a = saved_surface;
    a.clip.x = 7; a.clip.w = 2U;
    CHECK(lts_blit(&a, 0, &b, 0, 0) == LTS_INVALID);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);
    a = saved_surface;
    a.format = 2U;
    CHECK(lts_fill(&a, 0, 0x33U) == LTS_UNSUPPORTED);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);
    a = saved_surface;
    storage.capacity = 65521U;
    CHECK(lts_fill(&a, 0, 0x44U) == LTS_NOSPACE);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);
    storage.capacity = 512U;
    a = saved_surface;

    /* Clipped half-open fill touches visible pixels but leaves row padding. */
    CHECK(lts_fill(&a, 0, 3U) == LTS_OK);
    r.x = -2; r.y = 2; r.w = 6U; r.h = 9U;
    CHECK(lts_clip(&a, &r) == LTS_OK);
    r.x = 1; r.y = 1; r.w = 6U; r.h = 4U;
    CHECK(lts_fill(&a, &r, 7U) == LTS_OK);
    for (i = 0; i < 6U; ++i) {
        unsigned int x;
        for (x = 0; x < 12U; ++x) {
            lts_u8 expected = (i >= 2U && i < 5U && x >= 1U && x < 4U) ? 7U : 3U;
            if (x >= 8U) expected = 0xEEU;
            CHECK(storage.pixels[8U + i * 12U + x] == expected);
        }
    }

    /* Equal-pitch aliases copy as if from a snapshot in either direction. */
    CHECK(lts_clip(&a, 0) == LTS_OK);
    CHECK(lts_fill(&a, 0, 0U) == LTS_OK);
    for (i = 0; i < 6U; ++i) {
        unsigned int x;
        for (x = 0; x < 8U; ++x) storage.pixels[8U + i * 12U + x] = (lts_u8)(i * 8U + x);
    }
    CHECK(lts_blit(&a, 0, &a, 1, 1) == LTS_OK);
    CHECK(storage.pixels[8U + 12U + 1U] == 0U);
    CHECK(storage.pixels[8U + 12U + 7U] == 6U);
    CHECK(lts_blit(&a, 0, &a, 0, 0) == LTS_OK);

    /* A key skips raw source indices; destination padding remains untouched. */
    CHECK(lts_fill(&a, 0, 0xA5U) == LTS_OK);
    storage.pixels[8U] = 0U; storage.pixels[9U] = 1U;
    storage.pixels[48U] = 0xEEU;
    CHECK(lts_key(&a, 1, 0U) == LTS_OK);
    CHECK(lts_blit(&a, 0, &b, 0, 0) == LTS_OK);
    CHECK(storage.pixels[40U] == 0xEEU);
    CHECK(storage.pixels[41U] == 1U);
    CHECK(storage.pixels[48U] == 0xEEU);

    /* Different-pitch same-storage overlap is rejected before any write. */
    memcpy(before, guarded, sizeof(guarded));
    CHECK(lts_blit(&a, 0, &c, 0, 0) == LTS_OVERLAP);
    CHECK(memcmp(before, guarded, sizeof(guarded)) == 0);

    CHECK(guarded[0] == 0xEEU && guarded[513] == 0xEEU);
    if (failures != 0) return 1;
    puts("surface core tests passed");
    return 0;
}
