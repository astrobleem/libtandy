/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "../adapters/lt_dos_app_platform.h"

static lt_dos_video_state fake_video_state;
static lt_gfx_u8 fake_aperture[LT_GFX_BYTES];
static lt_gfx_u8 fake_sound_bytes[32];
static unsigned fake_sound_count;
static unsigned fake_restore_count;
static int fake_mode_failure;
static int fake_aperture_failure;

void ltdp_fake_reset(void)
{
    fake_video_state.mode = 3U;
    fake_video_state.page = 1U;
    fake_video_state.cursor_start = 6U;
    fake_video_state.cursor_end = 7U;
    fake_video_state.cursor_row = 12U;
    fake_video_state.cursor_column = 24U;
    memset(fake_aperture, 0, sizeof(fake_aperture));
    memset(fake_sound_bytes, 0, sizeof(fake_sound_bytes));
    fake_sound_count = 0U;
    fake_restore_count = 0U;
    fake_mode_failure = 0;
    fake_aperture_failure = 0;
}

void ltdp_fake_fail_mode(int fail)
{
    fake_mode_failure = fail;
}

void ltdp_fake_fail_aperture(int fail)
{
    fake_aperture_failure = fail;
}

void ltdp_fake_video_state(lt_dos_video_state *state)
{
    if (state) *state = fake_video_state;
}

unsigned ltdp_fake_sound_count(void)
{
    return fake_sound_count;
}

unsigned ltdp_fake_restore_count(void)
{
    return fake_restore_count;
}

lt_gfx_u8 *ltdp_fake_aperture(void)
{
    return fake_aperture;
}

unsigned char ltdp_fake_sound_byte(unsigned index)
{
    return index < fake_sound_count && index < sizeof(fake_sound_bytes)
        ? fake_sound_bytes[index] : 0U;
}

int ltdp_video_snapshot(lt_dos_video_state *state)
{
    if (state == 0) return 0;
    *state = fake_video_state;
    return 1;
}

int ltdp_video_set_mode(unsigned char mode)
{
    fake_video_state.mode = mode;
    if (fake_mode_failure) return 0;
    return 1;
}

void ltdp_video_hide_cursor(void)
{
    fake_video_state.cursor_start = 0x20U;
    fake_video_state.cursor_end = 0U;
}

void ltdp_video_restore(const lt_dos_video_state *state)
{
    if (state) {
        fake_video_state = *state;
        ++fake_restore_count;
    }
}

lt_gfx_u8 LT_GFX_FAR *ltdp_video_aperture(void)
{
    return fake_aperture_failure ? 0 : fake_aperture;
}

void ltdp_audio_emit(void *context, lt_u8 byte)
{
    if (context == 0 || *(unsigned short *)context != LT_DOS_APP_SOUND_PORT)
        return;
    if (fake_sound_count < sizeof(fake_sound_bytes))
        fake_sound_bytes[fake_sound_count] = byte;
    ++fake_sound_count;
}
