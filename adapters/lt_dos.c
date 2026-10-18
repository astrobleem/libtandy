/* SPDX-License-Identifier: MIT; optional Open Watcom 8086 sink only.
 * Caller owns hardware exclusion, interrupt policy, pacing and timer.
 * context points to a caller-owned unsigned short port (usually 0xC0).
 * Keep the entire public core call serialized, not merely this byte sink.
 */
#include "lt_sound.h"
#if defined(__WATCOMC__) && defined(__I86__)
static void write_port(lt_u16 port, lt_u8 byte);
#pragma aux write_port = "out dx,al" parm [dx] [al] modify exact []
void lt_dos_emit(void *context, lt_u8 byte)
{
    write_port(*(const lt_u16 *)context, byte);
}
#else
#error This optional sink requires Open Watcom 16-bit x86
#endif
