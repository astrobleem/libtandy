# Initial public source edition validation

Checked 2026-10-18 against the source selection in `SOURCE.json`.

- Open Watcom C16 1.9 compiled all eight portable modules and three DOS
  adapters with `-bt=dos -0 -ms -ecc -os -s -w4 -wx`.
- The original synthetic DOS demonstration and six unit-test executables
  linked successfully. All 18 recorded compile/link commands returned zero.
- These DOS executables were not run for this publication. No new emulator
  or physical Tandy qualification is claimed.
- Warnings remain visible in the local build log. The original coarse-time
  module has a W124 comparison-to-maximum warning on 32-bit `unsigned long`;
  some constant-width unit assertions produce W201 unreachable-code warnings.
  This is a successful build with recorded warnings, not a warning-free claim.
- The installed Windows GCC host attempt timed out on the first chip compile.
  Its owned compiler child was terminated; no host test pass is claimed and
  that compiler failure was not repeatedly retried. The portable host recipe
  remains available for a working C89 compiler.

Private upstream experiments are not copied into this public repository and
do not qualify this edition's DOS ABI, IRQ timing, display rate, resident
services or physical EX behavior. Tests use synthetic data, not game assets.

## Local native-audio integration candidate

This review-only candidate is based on libtandy main commit
`151477effddb0e7d519a051f8d60e1ba7595496c`. It adds the caller-ticked
`LTSCORE/1` and `LTSFX_CLIP/2` modules, the canonical `LTSF/1` clip file API,
and two separately licensed JAWS-derived example cues. It does not alter the
standalone MIDI parser/renderer under `sound/`.

- Host command: `python tools/build.py --target host --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py --runtime-bin C:/msys64/ucrt64/bin`.
- Result: all host builds and tests passed. This includes the new SFX and
  score suites, the existing chip/audio/graphics/surface/time/event suites,
  and the DOS-app host lifecycle test. C compilation used strict C89,
  `-pedantic-errors -Wall -Wextra -Werror` through the checked GCC launcher.
- DOS command: `python tools/build.py --target dos --watcom C:/WATCOM`.
- Result: all modules, DOS examples (including `SFXDEMO.EXE`) and DOS unit-test
  executables compiled successfully with Open Watcom's 8088 small-model flags.
  These binaries were not run; no emulator or physical Tandy test is claimed.
- The host JAWS exporter rebuilt both canonical `.ltsfx` presets byte for
  byte: `harpoon.ltsfx` is 80 bytes with SHA-256
  `1c8b0abce3eaf6f19c78d9006027389bd9de9b6c89a894421f0a5cb5ce394a75`, and
  `conch.ltsfx` is 212 bytes with SHA-256
  `4872bd0c006ba39f1bf75a7a1453233b5dc54a35b1653aaa608259f41f583fe5`.
- JAWS host fixtures compare the emitted PSG command traces and tick windows
  with the source-derived expectations. The finite DOS demo's retrace-based
  audio was not launched or captured in this integration pass.
- The existing `sound/` MIDI code and fixtures are unchanged. This candidate
  keeps MIDI-file parsing/rendering separate from the static score and short
  native-effect APIs; it does not add another MIDI parser or renderer.

The branch is a local review candidate only. No commit, push, pull request,
emulator run, or physical hardware run was performed.
