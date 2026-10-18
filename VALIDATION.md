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
