# DOS timer ABI and overload qualification

The IRQ0 adapter now supports Open Watcom's 16-bit **small, compact, medium
and large** memory models, including far-data builds with `-zt=0`. Match the
model and calling convention across the adapter, application, and callback
call graph. This change does not add a second timer or an audio driver.

## Correctness changes

* The ISR switches to a 3,072-byte explicitly near DGROUP stack before calling
  C work. Its indirect drain target is explicitly a **near function pointer**
  to a near function; ordinary application callbacks retain their model's
  near/far function-pointer ABI.
* Every assembly-visible state object is declared near separately. With
  Watcom, `unsigned __near a, b` does **not** place both objects near in a
  far-data model: `b` follows the default model. The initial multi-declarator
  implementation failed this test and was corrected before qualification.
* Compact/large callbacks can leave DS pointing to arbitrary far data. After
  the drain returns, the ISR reloads DS from the still-active private SS
  **before** reading the saved SS:SP. Without this restoration, compact-model
  execution failed after callbacks began. Real IRQ tests now cover it.
* Work is drained at most eight calls per outer interrupt. Remaining debt is
  retained for subsequent interrupts, rather than starving the foreground
  indefinitely in an unbounded drain loop. One slow callback can still stall
  the foreground; this is not a scheduler or a deadline guarantee.
* Pending-work and defer-depth counters never silently wrap. At 16-bit
  overflow the callback service stops, pending debt is cleared, and
  `lttk_faults()` exposes sticky `LTTK_FAULT_PENDING`/`LTTK_FAULT_DEFER` bits.
  BIOS/clock IRQ service continues. Close/open clears the fault; `lttk_run(1)`
  alone cannot restart a faulted timer.
* The 32-bit late diagnostic is read with interrupt-state preservation;
  IRQ-shared diagnostic state is volatile.

All interrupt-reachable C must have stack probes disabled (`-s`). The adapter
rejects a compilation with probes enabled. Watcom's `-ox` also implies `-s`,
but the qualified command spells out both. Callback code must avoid DOS,
blocking I/O and unbounded work, and its complete call graph plus nested
interrupts must fit the private stack. Open/close are foreground-only.

## Reproduce with installed tools

No downloads or application assets are required by the runner:

```
python3 tests/tick_matrix.py --watcom /path/to/watcom \
  --dosbox /path/to/dosbox-x
```

`--dosbox` may instead name an environment-setting executable wrapper when
an emulator needs a private shared-library path. The runner supplies a
headless Tandy/640-KiB/no-XMS configuration and an isolated build directory.
Use `--build`, `--cycles`, and `--timeout` to customize it. SDL dummy support
is required for this headless invocation. Install toolchains yourself; this
runner does not install software or alter physical hardware.

Compilation uses `-0 -ecc -bt=dos -ox -s -zt=0 -w4 -we -dLTTK_TEST -k8192`
with each of `-ms`, `-mc`, `-mm`, `-ml`. Only test builds expose the controlled
counter/canary hooks; they are absent without `LTTK_TEST`.

The test exercises real IRQs, a separately allocated interrupted SS stack,
address-taken callback locals through a non-inlined helper, a separately
allocated far context in compact/large, nested IRQ debt, nested foreground
deferral, stopped/start behavior, eight-call budget retention, seeded
16-bit overflow boundaries, fault recovery, vector restoration, preservation
of a replacement owner's vector, and 2,000 sampled monotonic clock reads.
Test hooks only seed impractically long counter-overflow boundaries; they
do not replace IRQ entry, callback dispatch, or stack switching.

## Recorded results (2026-11-02)

Official installed Open Watcom 2.0 beta dated 2026-10-01; DOSBox-X's
`8086_prefetch` CPU, fixed 20,000 emulated cycles, Tandy, 640 KiB:

| Model | Checks | Data/callback pointer bytes | Observed stack written-depth |
|---|---:|---:|---:|
| small | 2,028 PASS | 2 / 2 | 92 / 3,072 |
| compact | 2,028 PASS | 4 / 2 | 84 / 3,072 |
| medium | 2,028 PASS | 2 / 4 | 96 / 3,072 |
| large | 2,028 PASS | 4 / 4 | 98 / 3,072 |

Each deliberately slow callback produced three late ticks; all lower stack
canaries survived. `build/tick-matrix/` holds compile logs, native logs,
expected stack-probe rejection, emulator configuration and SHA-256 source/
executable manifest. These are native DOS ABI/correctness tests, **not** an
8088 instruction-cycle benchmark, real EX timing certification, or proof
that arbitrary callback stacks fit. Canary written-depth can undercount
untouched stack allocations; it is evidence for this test workload only.

The monotonic clock test is sampled, not an exhaustive proof of every PIT
latch/IRQ boundary. Physical hardware, arbitrary TSR/BIOS combinations,
real audio audibility, and other compiler/model options remain unqualified.

A separate conservative `--cycles 720 --build build/tick-matrix-720` repeat
also passed all four models (2,028 checks each, three induced late ticks).
Per-model written-depth observations are retained in the native logs.
Written-depth varies with interrupt phase; these measurements are not a
universal stack bound.
This is an emulator stress setting, not a calibrated 7.16-MHz EX equivalent.
