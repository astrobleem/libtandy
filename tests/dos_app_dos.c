/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "lt_dos_app.h"
#include "../adapters/lt_dos_app_platform.h"

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
    lt_dos_app app = LT_DOS_APP_INITIALIZER;
    lt_dos_video_state before, after;
    unsigned i;
    int result = 0;

    if (!ltdp_video_snapshot(&before)) return 1;
    if (lt_dos_app_init(&app) != LT_DOS_APP_OK) return 1;
    if (!app.initialized) result = 2;
    for (i = 0; i < 4U; ++i)
        if (app.chip.owner[i] != LT_DOS_APP_SOUND_OWNER) result = 3;
    lt_dos_app_shutdown(&app);
    if (app.initialized) result = 4;
    if (!ltdp_video_snapshot(&after) || !same_video(&before, &after))
        result = 5;
    if (result) return result;
    puts("DOS app init/ownership/shutdown/video restore passed");
    return 0;
}
