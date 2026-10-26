# libtandy

Source-only DOS and portable C89 components for Tandy software: SN76489-family
PSG register state, packed 320x200x16 graphics, two-plane 8x8 sprites, INDEX8
surfaces, bounded events, BIOS keyboard/mouse polling, cooperative coarse time
and a caller-driven audio facade. This is the DOS library, separate from the
Windows 3.0 USER/GDI project named libwintandy.



## Scope and contracts

| Modules | Contract |
| --- | --- |
| `lt_sound.h`, `lt_chip.c`, DOS sink | Caller owns hardware exclusion and scheduling. Owner tokens track channels; they do not arbitrate against other applications. Stop mutes before unclaim. |
| `lt_gfx.h`, `lt_gfx.c` | Caller supplies a valid 32 KiB packed four-bank mode-9 aperture. No mode switch, allocation or resident service in the raster core. |
| `lt_dos_app.h` (optional) | Foreground convenience setup for mode 9, the direct video aperture, BIOS input state and the default Tandy sound port. Saves/restores video page and cursor; no allocation or resident service. |
| `lt_gfx_sprite8.h` | Index 0 is transparent; nonzero indices mapped to black remain opaque. Ordered destination overlap is allowed; source/framebuffer aliasing is excluded. |
| `lt_surface.h`, `lt_media.h` | Bounded caller-owned INDEX8 storage, clipping, blits and palettes. No SDL code or compatibility claim. |
| `lt_event.h`, DOS input | Bounded foreground queue; BIOS press events and optional mouse snapshots. No key-up/held-state guarantee, installed ISR or mouse callback (see `lt_keys_dos.h` for held keys). |
| `lt_time.h`, DOS time | Cooperative caller-driven coarse time. No fine-frame-clock guarantee, callback thread or timer reprogramming. |
| `lt_page_dos.h` | Two-page mode-9 presentation by hardware page flip on Tandy 1000 EX/HX-class video. Reserves the top of the DOS arena (48 KiB on 640 KiB) while open; single-page fallback. See [`docs/PLATFORM-DOS.md`](docs/PLATFORM-DOS.md). |
| `lt_keys.h`, `lt_keys_dos.h` | Optional IRQ1 service: simultaneous held keys with press/release since the last poll. Restores the vector and BIOS keyboard state on close. |
| `lt_tick_dos.h` | Optional fixed-rate IRQ0 update clock (1,193,182/divisor Hz) that keeps the BIOS 18.2 Hz tick; the single timer owner, audio serviced from its callback. |
| `lt_mem_dos.h` | Launch memory figure and a DOS block pool released on every exit path. No built-in requirement. |
| `lt_audio.h` | Caller-driven channel ownership, explicit service/stop, and the shared PSG voice path used by native effects and static scores. No timer or IRQ takeover. |
| `lt_sfx.h`, `lt_score.h` | Bounded caller-ticked PSG clips and static Tandy scores, layered over `lt_audio.h`; no PCM, heap, or timer ownership. |
| [`sound/`](sound/README.md) | Standalone MIT C89 Standard MIDI File parser and PSG/PIT renderer. The caller supplies timing and hardware transport; see the module's capability revision and range limits. |

Include matching headers and compile the needed `.c` modules directly into
your application with matching calling convention, memory model and near/far
pointer contracts. There is no mandatory prebuilt library or runtime binary.
The hardware sink is optional. Pure tests capture writes in memory. The
standalone MIDI parser/renderer in `sound/` remains a separate optional module;
native clips and static score tables do not duplicate MIDI parsing/rendering.

## Build

Python 3 is used only for the build helper. Supply your own installed compiler;
no compiler, SDK, DOS, ROM or emulator is bundled.

```powershell
python tools/build.py --target dos --watcom C:/WATCOM
```

This compiles the modules for real-mode DOS with Open Watcom `-0 -ms -ecc`,
builds `build/dos/DEMO.EXE`, `build/dos/SFXDEMO.EXE` and the DOS unit-test
executables, and does not run them or start an emulator. The code targets
8086/8088, not 186/286 instructions.
In DOS on a compatible Tandy, `DEMO` enters mode 9, polls BIOS key presses,
moves an original square, plays a tone and restores the prior video mode, page
and cursor while muting/releasing its sound channels when Escape is pressed.
It requires exclusive foreground ownership of the hardware. Do not run this
DOS example as a normal Windows executable.

## Quickstart

`lt_dos_app_init()` is an optional convenience entry point for a small
foreground DOS program. Declare its object as `lt_dos_app app =
LT_DOS_APP_INITIALIZER;`. Init saves the current video mode/page and cursor,
enters mode 9, points `app.screen` at the direct video aperture, initializes
per-app input state, claims the four sound channels and mutes them to establish
a known quiet starting point. It allocates no memory and installs no interrupt
handler. It rejects a second init while the app is active without changing the
saved state. Call `lt_dos_app_shutdown()` after successful init; it mutes and
releases the channels, clears local input state and restores video/cursor. A
failed init rolls back and leaves the initialized app object inactive, so
shutdown is also safe after failure and repeated shutdown calls are harmless.

See [`examples/dos_demo.c`](examples/dos_demo.c) for a commented runnable
loop. It shows input polling, an update, direct drawing and an explicit coarse
delay. Escape quits. The delay uses DOS BIOS ticks and waits while idle; it
does not set a fixed frame rate. BIOS input reports press events only: it does
not provide key-up or held-key state. The example assumes the normal DOS BIOS
clock remains in place; programs that replace that clock need their own timing.

The app object owns the chip/input state and borrows the hardware video
aperture. It does not save arbitrary PSG register state; shutdown silences and
releases the channels. Keep the object at the same address until shutdown, do
not copy it while initialized, and keep the app in the foreground. Low-level
`lt_gfx_*`, `lti_dos_*` and `lt_*` sound calls remain available when an
application wants to manage each resource itself.

Portable host unit tests with a C89 compiler:

```sh
python tools/build.py --target host --cc gcc
python sound/tests/run.py --cc gcc
```

The host build uses `cc` from `PATH` by default. Pass `--cc <compiler>` to
select another compiler, or use `--gcc-launcher <launcher.py>` and
`--runtime-bin <runtime-directory>` when a Python-based compiler launcher is
needed. The launcher runs its DLL preflight before compiler startup. These
options do not change the DOS build.

On Chad's Windows host, use the established launcher; do not start raw
GCC/cc1. Both host runners accept the approved launcher. Selectable
Default/Chamber banks and the per-note audition regression are documented
in [the PSG bank source map](docs/PSG-BANKS.md).

```powershell
python tools/build.py --target host --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py --runtime-bin C:/msys64/ucrt64/bin
python sound/tests/run.py --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py
```

Build logs and a result manifest are local under `build/`. Host tests exercise
synthetic pixels/state and do not qualify DOS ABI, IRQ timing or hardware.
The original public-edition build record is historical. `VALIDATION.md` also
records the results for this local native-audio integration candidate.

## Native PSG effects and scores

Short effects are versioned `lt_sfx_clip` command sequences. A game initializes
one `lt_sfx_player` over its existing `lta_handle`, triggers a clip on a game
event, and calls `lt_sfx_tick()` from its existing update cadence:

```c
lt_sfx_init(&effects, &audio);
if (picked_up_coin)
    lt_sfx_trigger(&effects, &coin_clip);
/* Once per existing game update. */
lt_sfx_tick(&effects);
```

Effects borrow the declared PSG channels and expand the reservation to the
tone-2/noise pair when required by the hardware, respect clip priority, and
restore the latest music state when they end. The binary `.ltsfx` format is
canonical little-endian `LTSF/1`; the C descriptor is `LTSFX_CLIP/2` and
records a reduced rational source tick rate without owning a clock. See
[`docs/SFX-CLIPS.md`](docs/SFX-CLIPS.md) for the fields, editor import/export
contract, and validation limits.

Longer fixed arrangements use `lt_score` with static note/percussion tables and
the caller's existing timer cadence. It does not parse MIDI files. The
optional MIDI parser and renderer remain under [`sound/`](sound/README.md).
See [`docs/SCORE-PLAYER.md`](docs/SCORE-PLAYER.md) for timing and the safe
non-owning speaker callback contract. `examples/dos_sfx_demo.c` builds as
`SFXDEMO.EXE`: H/C/Q plays the JAWS cues interactively, and `/demo` runs a
finite harpoon, silent gap, conch and all-channel stop sequence from
foreground retrace polling only.

[`examples/doebel/`](examples/doebel/README.md) adds an optional three-tone
"Doebelinschmirtz Incorporated" jingle example using `lt_score`.
`DOEBEL.EXE` plays it once in foreground DOS; the checked-in score header and
PSG VGM need no Python preparation. The recognizable tune is a reduction of
the referenced *Evil Jingle*, with separate musical attribution in the example.


## Optional Sega Tune and animated Tandy logo

`SMSBOOT.EXE` plays the SMS BIOS startup tune extracted from Chad's OutRun SE.
`TANDYLOG.EXE` reveals the supplied TANDY logo on white and pops in
"clearly superior" on its final attack. Both are explicit, self-contained DOS
examples, with no automatic startup playback or consumer preparation.
See [usage, source and timing](examples/smsboot/README.md) and
[validation and cost](examples/smsboot/VALIDATION.md).
