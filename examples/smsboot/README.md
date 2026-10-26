# Sega Tune and Tandy logo

Two optional foreground DOS examples, with no startup hook or installation:

[Download SMSBOOT.ZIP](https://github.com/astrobleem/libtandy/raw/refs/heads/main/examples/smsboot/SMSBOOT.ZIP)
for the exact qualified EXEs, or build the source below. The small download
contains only these optional DOS applications and their instructions/notices;
it bundles no compiler, library runtime, game or ROM. [DOS-BUILD.json](DOS-BUILD.json)
records the qualified source revision and hashes checked by CI.

- `SMSBOOT.EXE`: plays the Sega Master System BIOS tune once.
- `TANDYLOG.EXE`: starts white and traces the supplied lettering with cyan
  heads and blue trails. TANDY settles fully blue about a quarter-second
  before the hit; the red "clearly superior" tagline pops in on the final
  attack. The final logo stays until
  a key. `/auto` returns when the sound ends. Escape cancels either example.

Build with the normal developer command from the repository root:

```powershell
python tools/build.py --target dos --watcom C:/WATCOM
```

Copy `build/dos/SMSBOOT.EXE` and/or `build/dos/TANDYLOG.EXE` to compatible
Tandy DOS and run explicitly. Each EXE is self-contained: no PNG, VGM,
Python, conversion, or other preparation is needed on the playback machine.
They require exclusive foreground ownership of the PSG. The logo uses Tandy
mode 9 and its gate-array palette. Exit restores video mode/page/cursor and
the previous IRQ0 vector/BIOS timer; the PSG is muted and released.

## Reuse the sound

Compile `src/lt_sms_boot.c` and `src/lt_chip.c` with your application. The
named effect uses the existing low-level `lt_chip` ownership/sink contract.
It preserves the original latch/data writes, including noise resets. It is
separate from `lt_sfx`: that short format permits only 64 commands/255 ticks;
this tune has 710 PSG writes and runs for 805 service ticks. No existing
short-clip, score, MIDI bank, driver, or graphics implementation was changed.

```c
#include "lt_sms_boot.h"

lt_sms_boot boot;

/* chip is already initialized with your existing PSG sink. Token 1 must
 * be unique on this chip. Check each return value in your application. */
lt_sms_boot_init(&boot, &chip, 1U);  /* silent */
lt_sms_boot_start(&boot);          /* claims all four voices, initially mute */
lt_sms_boot_tick(&boot);           /* tick zero at the chosen start instant */
/* Then once per existing 1193182/4972 Hz update (about 239.9803 Hz): */
lt_sms_boot_tick(&boot);
/* On cancel, or harmlessly after automatic completion: */
lt_sms_boot_stop(&boot);
```

The caller supplies storage, timing, and serialization. The effect owns no
timer, IRQ, heap, or OS resource. Do not run it concurrently with an
`lta_handle`/music player on the same channels: stop/release music first and
reacquire it afterwards. Stop mutes before releasing; it does not restore
previous music. An ownership conflict fails atomically. Starting an active
effect fails; stop first to restart. Do not reinitialize a live player/chip.

`examples/sms_demo.c` shows setup and cleanup using the existing optional
`lt_tick_dos` adapter. An application with its own clock should service the
effect from that clock instead of installing another timer.

## Timeline and supplied artwork

The source's last attack starts at sample **88,291 / 44,100 seconds**.
The retained PSG frame sets the three tone dividers, restarts the volume
attack (`97 B7 D7`) and sets noise (`F0`). This is the final new chord/attack;
the subsequent divider changes are its authored modulation and decay.
`LTSB_FINAL_TICK` is **481**, at **2.004331 seconds** on the DOS clock.
TANDY finishes and settles fully blue at **tick 421 / 1.754311 seconds**.
It holds for 60 ticks (about **0.250 seconds**). Only the tagline palette
changes at tick 481, immediately before the final attack's PSG writes.
There is no independent animation sleep clock or framebuffer redraw on that
beat. Sound completion is tick **805**, at **3.354442 seconds**.

`logo.png` is Chad's supplied image, Library ID
`libfile_932ad524a9f881919e212eefa0964f77`, SHA256
`1455baca00398742cdd18c20452a4941dc6f510d30f1447bfa416159827a022c`.
The lettering and tagline are reduced from those actual pixels, not recreated
with fonts. `TRACE_RT.H` contains the ready-to-use initial mask and sparse
byte patches. Background index 15 stays white; index 0 holds completed blue;
indices 1..13 animate strokes; index 14 reveals the tagline. The
mode-9 conversion uses 288x136 pixels centered at (16,32), accounting for
the 5:6 pixel width/height ratio on a 4:3 display. Four masks provide 52
stroke positions, with three invisible foreground handoffs. A short 8088
patch loop keeps interrupts enabled; no video transfer happens in the audio
callback. Each handoff has a 23-tick (95.84 ms) window. See the
[stroke implementation](STROKE-PREVIEW.md) and [native measurements](NATIVE.json).
The available 16-color palette reduces the original colors and edge detail.

`tools/trace_tandy_logo.py --out <directory>` is an optional developer
regeneration/preview tool (Pillow/NumPy). `tools/check_tandy_trace.py` verifies
the actual sparse runtime encoding. Neither is needed on the DOS machine.
For diagnostics only, run `TANDYLOG /auto /stats`: it reports handoff times
and cue ticks, and saves `SMSPSG.BIN` after stopping/restoring the timer.
Normal playback does not create diagnostic files.

## Source

Extracted from Chad's existing **Tandy OutRun SE v0.13**, at:

```text
C:\Users\chad\Documents\Codex\2026-10-15\task-4\tandy-outrun-se\
  splash-tone-input\Sega-Tune.vgm
  hybrid-v013-320\SPLTUNE.INC
  hybrid-v013-160\SPLTUNE.INC
```

Both resolution tables contain exactly the same 710 source PSG bytes. The
original supplied archive, `BIOS (EMU).zophar.zip` (Library ID
`libfile_b41780fb82488191b9ffca7a2a162a9b`), was independently materialized
and its gzip member `BIOS - 01 - Sega Tune.vgm` matches the project's decoded
VGM SHA256 `8f4961245386c32595b2288238ca7567d144757abe151d28b35a9968727ea2b1`.
Its GD3 identifies **US/European BIOS v1.3**, Sega Master System, 1986,
logged by **Maxim**. The archive has one Sega Tune; its remaining tracks are
Snail Maze cues and Space Harrier music. Musical material remains Sega's;
the existing library/playback code keeps its MIT license. The supplied logo
retains its existing rights. No OutRun file, ROM, or asset was modified.

The VGM is 1.01, 147,826 samples (3.352063 seconds), PSG clock 3,579,540 Hz,
no loop, and one all-channel stereo-enable command (irrelevant to mono).
The Tandy clock assumption is 3,579,545 Hz: converting every ten-bit divider
by rounding `period * 3579545 / 3579540` leaves every divider unchanged.
The 1.4 ppm clock difference needs no changed notes. Sega and Tandy noise
feedback differ, so the noise grain can differ even with identical controls.

`REFERENCE.csv` retains source sample timestamps and bytes. The production
table uses cumulative ceil deadlines at 1,193,182/4,972 Hz, rather than the
OutRun example's coarser 64 Hz schedule. The worst lateness at this tune's
timestamps is **2.378 ms**. At most one group/12 PSG bytes is emitted per
service call. `tools/extract_sms_boot.py <source.vgm>` checks the generated
table/reference without changing the input; `--write` deliberately regenerates.

See [VALIDATION.md](VALIDATION.md) for tests, limits and memory/code cost.
