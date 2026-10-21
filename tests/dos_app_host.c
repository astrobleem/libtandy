/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "lt_dos_app.h"

void ltdp_fake_reset(void);
void ltdp_fake_fail_mode(int fail);
void ltdp_fake_fail_aperture(int fail);
void ltdp_fake_video_state(lt_dos_video_state *state);
unsigned ltdp_fake_sound_count(void);
unsigned ltdp_fake_restore_count(void);
lt_gfx_u8 *ltdp_fake_aperture(void);
unsigned char ltdp_fake_sound_byte(unsigned index);

#define CHECK(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++failures; \
} } while (0)

static int same_video(const lt_dos_video_state *a,
                      const lt_dos_video_state *b)
{
    return a->mode == b->mode && a->page == b->page &&
           a->cursor_start == b->cursor_start &&
           a->cursor_end == b->cursor_end &&
           a->cursor_row == b->cursor_row &&
           a->cursor_column == b->cursor_column;
}

int main(void)
{
    lt_dos_app app;
    lt_dos_video_state before, after;
    unsigned i, sound_before, frame;
    int failures = 0;

    /* Successful init owns mode 9, the direct aperture, and all sound channels. */
    ltdp_fake_reset();
    ltdp_fake_video_state(&before);
    CHECK(lt_dos_app_init(0) == LT_DOS_APP_INVALID);
    CHECK(lt_dos_app_init(&app) == LT_DOS_APP_OK);
    CHECK(app.initialized == 1U);
    CHECK(app.screen.pixels == ltdp_fake_aperture());
    CHECK(app.sound_port == LT_DOS_APP_SOUND_PORT);
    CHECK(app.input.mouse_enabled == 0U && app.input.mouse_seen == 0U);
    for (i = 0; i < 4U; ++i) CHECK(app.chip.owner[i] == LT_DOS_APP_SOUND_OWNER);
    ltdp_fake_video_state(&after);
    CHECK(after.mode == 9U);
    CHECK(after.cursor_start == 0x20U);

    /* A normal loop exit can always mute audio and give BIOS video state back. */
    CHECK(lt_tone(&app.chip, LT_DOS_APP_SOUND_OWNER, 0U, 256U, 1));
    CHECK(lt_attenuate(&app.chip, LT_DOS_APP_SOUND_OWNER, 0U, 8U, 1));
    for (frame = 0; frame < 3U; ++frame)
        lt_gfx_put(&app.screen, (lt_gfx_i16)frame, 0, (lt_gfx_u8)frame);
    app.input.mouse_enabled = 1U;
    app.input.mouse_seen = 1U;
    app.input.buttons = 3U;
    sound_before = ltdp_fake_sound_count();
    lt_dos_app_shutdown(&app);
    CHECK(ltdp_fake_sound_count() == sound_before + 4U);
    for (i = 0; i < 4U; ++i)
        CHECK(ltdp_fake_sound_byte(sound_before + i) ==
              (unsigned char)(144U | (i << 5) | 15U));
    CHECK(ltdp_fake_restore_count() == 1U);
    ltdp_fake_video_state(&after);
    CHECK(same_video(&after, &before));
    CHECK(app.initialized == 0U);
    CHECK(app.input.mouse_enabled == 0U && app.input.mouse_seen == 0U &&
          app.input.buttons == 0U);
    for (i = 0; i < 4U; ++i) CHECK(app.chip.owner[i] == 0U);
    lt_dos_app_shutdown(&app);
    CHECK(ltdp_fake_restore_count() == 1U);

    /* Mode-set failure may have changed hardware; rollback restores it. */
    ltdp_fake_reset();
    ltdp_fake_video_state(&before);
    ltdp_fake_fail_mode(1);
    CHECK(lt_dos_app_init(&app) == LT_DOS_APP_VIDEO_FAILED);
    ltdp_fake_video_state(&after);
    CHECK(same_video(&after, &before));
    CHECK(ltdp_fake_restore_count() == 1U);
    CHECK(ltdp_fake_sound_count() == 0U);
    CHECK(app.initialized == 0U);
    CHECK(app.input.mouse_enabled == 0U && app.input.mouse_seen == 0U);
    lt_dos_app_shutdown(&app);
    CHECK(ltdp_fake_restore_count() == 1U);

    /* A missing aperture after mode entry also restores the prior video state. */
    ltdp_fake_reset();
    ltdp_fake_video_state(&before);
    ltdp_fake_fail_aperture(1);
    CHECK(lt_dos_app_init(&app) == LT_DOS_APP_VIDEO_FAILED);
    ltdp_fake_video_state(&after);
    CHECK(same_video(&after, &before));
    CHECK(ltdp_fake_restore_count() == 1U);
    CHECK(ltdp_fake_sound_count() == 0U);
    CHECK(app.initialized == 0U);
    CHECK(app.input.mouse_enabled == 0U && app.input.mouse_seen == 0U);
    lt_dos_app_shutdown(&app);
    CHECK(ltdp_fake_restore_count() == 1U);

    if (failures) return 1;
    puts("DOS app lifecycle passed init, rollback, loop exit, ownership");
    return 0;
}
