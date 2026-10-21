# libtandy

Source-only DOS and portable C89 components for Tandy software: SN76489-family
PSG register state, packed 320x200x16 graphics, two-plane 8x8 sprites, INDEX8
surfaces, bounded events, BIOS keyboard/mouse polling, cooperative coarse time
and a caller-driven audio facade. This is the DOS library, separate from the
Windows 3.0 USER/GDI project named libwintandy.

This first public edition contains the original MIT modules selected from
existing local libtandy revision `d633d25b6bfd9f56105acdd2e86f960c8db3efb5`.
`SOURCE.json` records the source file hashes and the small chip-test extraction.
The existing MIT copyright and permission notice are retained verbatim in
`LICENSE`; no license is assigned to excluded third-party work.

## Scope and contracts

| Modules | Contract |
| --- | --- |
| `lt_sound.h`, `lt_chip.c`, DOS sink | Caller owns hardware exclusion and scheduling. Owner tokens track channels; they do not arbitrate against other applications. Stop mutes before unclaim. |
| `lt_gfx.h`, `lt_gfx.c` | Caller supplies a valid 32 KiB packed four-bank mode-9 aperture. No mode switch, allocation or resident service in the raster core. |
| `lt_dos_app.h` (optional) | Foreground convenience setup for mode 9, the direct video aperture, BIOS input state and the default Tandy sound port. Saves/restores video page and cursor; no allocation or resident service. |
| `lt_gfx_sprite8.h` | Index 0 is transparent; nonzero indices mapped to black remain opaque. Ordered destination overlap is allowed; source/framebuffer aliasing is excluded. |
| `lt_surface.h`, `lt_media.h` | Bounded caller-owned INDEX8 storage, clipping, blits and palettes. No SDL code or compatibility claim. |
| `lt_event.h`, DOS input | Bounded foreground queue; BIOS press events and optional mouse snapshots. No key-up/held-state guarantee, installed ISR or mouse callback. |
| `lt_time.h`, DOS time | Cooperative caller-driven coarse time. No fine-frame-clock guarantee, callback thread or timer reprogramming. |
| `lt_audio.h` | Caller-driven channel/event ownership and explicit service/stop. No song decoder or replacement scheduler. |
| [`sound/`](sound/README.md) | Standalone MIT C89 Standard MIDI File parser and PSG/PIT renderer. The caller supplies timing and hardware transport; see the module's capability revision and range limits. |

Include matching headers and compile the needed `.c` modules directly into
your application with matching calling convention, memory model and near/far
pointer contracts. There is no mandatory prebuilt library or runtime binary.
The hardware sink is optional. Pure tests capture writes in memory.

## Build

Python 3 is used only for the build helper. Supply your own installed compiler;
no compiler, SDK, DOS, ROM or emulator is bundled.

```powershell
python tools/build.py --target dos --watcom C:/WATCOM
```

This compiles the modules for real-mode DOS with Open Watcom `-0 -ms -ecc`,
builds `build/dos/DEMO.EXE` and the DOS unit-test executables, and does not run
them or start an emulator. The code targets 8086/8088, not 186/286 instructions.
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

On Windows pass the full path of an installed GCC if it is not on PATH.
Build logs and a result manifest are local under `build/`. Host tests exercise
synthetic pixels/state and do not qualify DOS ABI, IRQ timing or hardware.
The public initial build status and recorded warnings are documented in
`VALIDATION.md`; no fresh DOS execution or host test pass is claimed there.

## Deliberate release boundaries

This edition excludes the separate GPL RAVE RPI1 evaluator/reference data,
Win16 adapter, process-scoped overlay loader, resident graphics extension,
private qualification traces and prebuilt runtime binaries. Those components
are not relicensed by this repository's MIT license. No game ROMs, extracted
commercial graphics, songs, proprietary toolchains, private workspace paths
or credentials are included. Public source availability does not authorize
publication of an application's copyrighted assets.

Keep game-specific fine timing and held-input adapters when these library
contracts do not provide an equivalent. Measure actual 8088 rendering costs
before replacing optimized code. This repository does not claim a complete
game engine, NES emulation, 60 FPS rendering or physical EX qualification.
