/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "lt_dos_app.h"

int main(void)
{
    lt_dos_app app;
    unsigned i;
    int result = 0;

    if (lt_dos_app_init(&app) != LT_DOS_APP_OK) return 1;
    if (!app.initialized) result = 2;
    for (i = 0; i < 4U; ++i)
        if (app.chip.owner[i] != LT_DOS_APP_SOUND_OWNER) result = 3;
    lt_dos_app_shutdown(&app);
    if (app.initialized) result = 4;
    if (result) return result;
    puts("DOS app init/ownership/shutdown passed");
    return 0;
}
