# General-purpose 8088 improvements: qualification

Base: `94f4f4fb2e42c60363eb0737547c294cc5de053c`; tested 2026-11-02.
All additions are optional MIT library code, synthetic tests and documentation.
No new application assets, music, executable downloads or source-machine
conversion framework are included.

## Verified changes

- IRQ0 supports Watcom small, compact, medium and large models. Explicit
  near assembly operands/private drain ABI and DS restoration after far-data
  callbacks fix actual model failures. Eight-call drain budgeting retains
  debt; checked overflow stops work with sticky faults. All four models pass
  2,028 native checks at fixed 20,000 and 720 cycles.
- Optional INT23/24 guards preserve prior CRT policy and foreign vector
  owners. All four models pass 55 native checks, including foreign SS,
  saved registers, close/reopen and simulated pre-DOS3 rejection. These are
  **best effort**: DOS can turn disallowed FAIL into ABORT; no actual device
  failure was injected and foreground cleanup cannot always be guaranteed.
- Opaque packed tile8: 10,224 cases pass independent framebuffer/guard
  comparisons in small/compact/large, optimized and forced-C builds.
  The tested RAM-copy loop is about 74% faster than the generic packed blit
  in two emulator profiles, not a whole-scene or physical-hardware claim.
- Far fill: all 393 independent full-buffer clear/span/edge cases pass
  native small/compact/large and host sanitizers.
- Far-pointer limits: all four models and both optimized/portable kernels
  pass 64 cases each at source FFE0/FFDF and surface 8000/7FFF, with complete
  64 KiB comparisons. Surface and source window requirements are explicit.
- AY control-frame adapter: 622,802 checks, host/native hash `85731ebb`.
  Explicit clock/range/loss policies, caller-resolved envelopes, existing
  mute/attenuation/SFX ownership preserved. This is not waveform emulation.

The candidate-only checkout also passes strict C89 host checks, the existing
portable MIDI suite, and 20 noninteractive DOS programs in small and large
models. The near-pointer assertion in the event test is explicitly `__near`
rather than incorrectly assuming every model's implicit pointer is near.
Native timer, guard and far-boundary matrices use separate runners so their
special flags/hooks do not enter normal builds.

## Reproduce and interpret

Use installed Open Watcom and DOSBox-X; the scripts download nothing:

```
python tools/build.py --target host
python sound/tests/run.py --cc gcc
python tools/build.py --target dos --watcom /path/to/watcom
python tools/qualify_dos.py --dosbox /path/to/dosbox-x
python tools/build.py --target dos --model large --watcom /path/to/watcom
python tools/qualify_dos.py --model large --dosbox /path/to/dosbox-x
python tests/tick_matrix.py --watcom /path/to/watcom --dosbox /path/to/dosbox-x
python tools/qualify_guard.py --watcom /path/to/watcom --dosbox /path/to/dosbox-x
python tests/gfx_far_matrix.py --watcom /path/to/watcom --dosbox /path/to/dosbox-x
```

Observed with Open Watcom 2.0 beta 2026-10-01 and DOSBox-X 2025.02.01,
Tandy 640 KiB, normal `8086_prefetch`, no XMS/EMS/UMB. Fixed-cycle profiles
are **not** MHz equivalents or physical Tandy EX certification. No physical
sound/listening, VRAM contention, arbitrary callback stack or BIOS/TSR safety
claim is made. Native runners write command/hash manifests and logs under
`build/`; host execution alone does not qualify 16-bit ABI behavior.

Detailed contracts/results: [timer](TIMER-QUALIFICATION.md),
[guard](DOS-GUARD.md), [tile8](TILE8.md), [fill](GFX-FILL.md),
[far boundaries](GFX-FAR-QUALIFICATION.md), [AY frames](AY-FRAMES.md).

## Existing downloadable example provenance

No existing download is rebuilt or replaced by this library change. The SMS
package's existing manifest pins its qualified source to commit
`3184232cdcb05fd50c9d70eac29a7328d207f96f`. Its checker now verifies that
immutable commit's source hashes, plus the unchanged ZIP and native EXE
receipts, instead of misleadingly requiring the evolving library to remain
byte-identical to an old build. It reports current-source differences.
A full Git history (or the pinned commit fetched locally) is required for
this offline check; CI checks out history. No stored hash or native receipt
is relaxed, rewritten or relabeled as current. Existing engine package
checks remain unchanged and pass.
