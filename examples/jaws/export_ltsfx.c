/* SPDX-License-Identifier: GPL-3.0-only */
/* Write the canonical importable files from the source-derived C presets. */
#include <stdio.h>
#include "jaws_clips.h"

static int write_clip(const char *path, const lt_sfx_clip *clip)
{
    lt_u8 bytes[LTSFX_FILE_MAX_BYTES];
    lt_u16 size, written;
    FILE *file;

    if (lt_sfx_file_size_v1(clip, &size) != LTA_OK ||
        lt_sfx_file_encode_v1(clip, bytes, (lt_u16)sizeof(bytes),
                              &written) != LTA_OK || written != size)
        return 0;

    file = fopen(path, "wb");
    if (!file) return 0;
    if (fwrite(bytes, 1U, written, file) != written) {
        (void)fclose(file);
        return 0;
    }
    if (fclose(file) != 0) return 0;
    return 1;
}

int main(void)
{
    if (!write_clip("harpoon.ltsfx", &lt_sfx_jaws_harpoon) ||
        !write_clip("conch.ltsfx", &lt_sfx_jaws_conch)) {
        puts("Could not write JAWS .ltsfx presets.");
        return 1;
    }
    puts("Wrote harpoon.ltsfx and conch.ltsfx.");
    return 0;
}
