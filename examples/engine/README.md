# Optional engine DOS demonstrations

[Download ENGINE.ZIP](https://github.com/astrobleem/libtandy/raw/refs/heads/main/examples/engine/ENGINE.ZIP)
for the qualified, self-contained DOS executables. Extract to a directory on
compatible Tandy DOS hardware and run explicitly. No Python, assets, setup
program or conversion step is needed on the DOS machine.

| Command | Display |
| --- | --- |
| `ENGDEMO` | Tandy 320x200x16 (mode 09h) |
| `ENGDEMO /low` | Tandy 160x200x16 (mode 08h) |
| `TXTDEMO` | Color text 80x25 (mode 03h) |
| `TXTDEMO /40` | Color text 40x25 (mode 01h) |

A/D or arrows set horizontal velocity; Space jumps; Escape quits and restores
video. Each example uses the same world, tile collisions and gravity, with a
camera following the moving body through a scrolling map. Add `/auto` for a
finite 300-fixed-tick route, or `/auto /slow` for a three-BIOS-tick foreground
wait. The engine runs fixed callbacks in the foreground; it owns no timer,
ISR, sound channel or heap. These are small convenience examples, not a
high-frame-rate platformer benchmark.

`RUNCHECK.BAT` runs ENGINE/ENGVIEW/ENGDOS, the native lifecycle/physics,
presentation and mode-restoration checks. The ZIP also contains the guide,
validation record and MIT license. It bundles no emulator, ROM, DOS or compiler.

The [engine guide](../../docs/ENGINE.md) documents optional source integration,
supported display layouts and bounds. The [validation record](../../docs/ENGINE-VALIDATION.md)
records the 4.77 MHz Tandy HX emulator results and code/state costs.
[DOS-BUILD.json](DOS-BUILD.json) pins the qualified source and ZIP members;
[NATIVE.json](NATIVE.json) records exact tested binary hashes and runtime results.
CI checks the committed download without requiring an emulator or compiler
for its verification.
