# Doebelinschmirtz Incorporated

An optional three-tone PSG rendition of the recognizable *Evil Jingle* from
*Phineas and Ferb*, for Chad's “Doebelinschmirtz Incorporated” joke. It uses
the existing `lt_score` API. Noise and PC speaker are unused.

The melody follows the opening of the [licensed piano score preview](https://www.musicnotes.com/sheetmusic/sheet-music-boss/evil-jingle/MN0202127),
arranged by Andrew Wrangell / Sheet Music Boss, edited by Samuel Dickenson.
The preview credits Danny Jacob, Jeff Marsh, Jon Barry, Zac Moncrief,
Dan Povenmire and J.G. Orrantia. This is a reduction of their recognizable
tune, not an original composition. Musical rights remain with their owners;
the library and playback code keep their existing MIT licenses.

## Play or use the score

Build from the repository root with the normal developer toolchain:

```powershell
python tools/build.py --target dos --watcom C:/WATCOM
```

Run `build/dos/DOEBEL.EXE` explicitly in exclusive foreground Tandy DOS.
It plays once, mutes/releases the PSG and restores the BIOS timer. Escape
cancels. It uses the existing `lt_tick_dos` adapter at nominal 240 Hz
(PIT divisor 4,972, about 239.9803 Hz). No startup installation or speaker
access is involved. Normal completion and Escape/timer cleanup passed on an
emulated Tandy HX; see [the native receipt](NATIVE.json).

`DOEBEL.H` is the ready-to-compile static score; `DOEBEL.VGM` is a ready
one-shot PSG stream for an existing compatible VGM player. Neither needs a
generator or Python on the playback machine. In an application that already
owns the audio handle and update cadence:

```c
#include "DOEBEL.H"

lt_score_init(&music, &audio);
lt_score_start(&music, &doebel_score, DOEBEL_SERVICE_HZ, 0, 0);
/* Once per existing 240-Hz update, before the silent loop boundary: */
if (music.phase + music.step >= ((lta_u32)DOEBEL_END_TICK << 16))
    lt_score_stop(&music);
else
    lt_score_tick(&music);
```

The caller owns all four PSG channels, serialization and timing. See
[`../doebel_demo.c`](../doebel_demo.c) for setup, errors and cleanup. Do not
install another timer in an app that already has one.

## Edit and verify

`DOEBEL.json` is the unchanged approved editable score: 10 melody, 8 harmony
and 8 bass events. It retains 192 PPQ, 355,030 microseconds/quarter, 2:1 swing,
the tied ending and attenuation levels 2/9/7. The bass is raised an octave to
fit PSG range. The checked-in header preserves those notes and envelope gaps.

`generate.py --write` rebuilds only the header after deliberate edits;
`generate.py --check` checks it without writing. This is a developer tool.
The frozen `APPROVED.H` register fixture and `NOTES.csv` per-note audit are
from the host audition Chad heard and approved on 2026-10-25. The host suite
compares every actual PSG byte and service frame against that fixture,
including repeated attacks and the ending mute/release. `APPROVED.json`
retains the reference WAV and audit hashes; `HOSTCHECK.json` is the original
numerical receipt.

The approved WAV is a 4.512517-second host approximation, with fixed master
gain 0.18, identical-gain stems, no normalization and a peak of -15.28 dBFS.
The master gain is a host-rendering factor; native PSG attenuation remains
2/9/7 with the existing score envelopes. The approximation uses ideal square
waves and does not model analog filtering or speaker response. No gain,
notes, timing or balance changes are introduced by this example.

Listening approval covers the host sound. The short native normal-completion,
Escape and timer-restoration checks passed using the shared emulator lease.
Physical hardware and native audio capture are not qualified. Chad approved
publication on 2026-10-25. Status text in the unchanged
score JSON is historical audition metadata; this README and the validation
receipts describe current acceptance.
