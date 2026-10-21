/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "lt_dos_app_platform.h"

#define LT_DOS_APP_VIDEO_MODE 9U

static void lt_dos_app_emit(void *context, lt_u8 byte)
{
    ltdp_audio_emit(context, byte);
}

static void lt_dos_app_restore(lt_dos_app *app, int video_saved,
                              int video_touched, int sound_claimed)
{
    if (sound_claimed)
        (void)lt_stop(&app->chip, LT_DOS_APP_SOUND_OWNER,
                      LT_DOS_APP_SOUND_MASK);
    if (video_saved && video_touched)
        ltdp_video_restore(&app->previous_video);
    memset(app, 0, sizeof(*app));
}

lt_dos_app_status lt_dos_app_init(lt_dos_app *app)
{
    int video_saved = 0;
    int video_touched = 0;
    int sound_claimed = 0;
    lt_gfx_u8 LT_GFX_FAR *aperture;

    if (app == 0) return LT_DOS_APP_INVALID;
    memset(app, 0, sizeof(*app));

    if (!ltdp_video_snapshot(&app->previous_video))
        return LT_DOS_APP_VIDEO_FAILED;
    video_saved = 1;

    /* Restore even when verification fails: BIOS mode changes can be partial. */
    video_touched = 1;
    if (!ltdp_video_set_mode(LT_DOS_APP_VIDEO_MODE)) {
        lt_dos_app_restore(app, video_saved, video_touched, sound_claimed);
        return LT_DOS_APP_VIDEO_FAILED;
    }

    ltdp_video_hide_cursor();
    aperture = ltdp_video_aperture();
    if (aperture == 0) {
        lt_dos_app_restore(app, video_saved, video_touched, sound_claimed);
        return LT_DOS_APP_VIDEO_FAILED;
    }
    lt_gfx_init(&app->screen, aperture);

    app->sound_port = LT_DOS_APP_SOUND_PORT;
    lt_init(&app->chip, lt_dos_app_emit, &app->sound_port);
    if (!lt_claim(&app->chip, LT_DOS_APP_SOUND_OWNER,
                  LT_DOS_APP_SOUND_MASK)) {
        lt_dos_app_restore(app, video_saved, video_touched, sound_claimed);
        return LT_DOS_APP_AUDIO_FAILED;
    }
    sound_claimed = 1;

    /* This state is caller-owned; input setup installs no system hook. */
    memset(&app->input, 0, sizeof(app->input));
    app->initialized = 1U;
    return LT_DOS_APP_OK;
}

void lt_dos_app_shutdown(lt_dos_app *app)
{
    if (app == 0 || !app->initialized) return;

    (void)lt_stop(&app->chip, LT_DOS_APP_SOUND_OWNER,
                  LT_DOS_APP_SOUND_MASK);
    memset(&app->input, 0, sizeof(app->input));
    ltdp_video_restore(&app->previous_video);
    memset(app, 0, sizeof(*app));
}
