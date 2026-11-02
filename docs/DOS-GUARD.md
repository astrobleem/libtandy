# Optional orderly DOS exit guards

`lt_guard_dos.h` and `adapters/lt_guard_dos.c` are opt-in Open Watcom 16-bit DOS3+ services. They do not run at startup and do not own the graphics, timer, PSG or atexit lifecycle.

```c
if (ltguard_open() != LTGUARD_OK) return 1;
/* Acquire application resources, then poll ltguard_quit() in foreground. */
/* On quit/error: stop IRQ work, mute/release audio, restore video and vectors. */
lost_vectors = ltguard_close();
```

Compile **only the adapter** with `-s -zu`, and use the same memory model/calling convention as its callers. The adapter rejects compilation without those flags. `-zu` is essential because DOS may invoke INT24 with SS different from DGROUP; `-s` prevents unsafe stack-check runtime calls from the interrupt. Ordinary callers need not use `-zu`.

INT23 latches a quit flag. INT24 latches quit/critical flags and changes only saved AL to 3 (**Fail**). **This is best effort:** if entry AH bit 3 says Fail is not permitted, DOS may convert that response to Abort and bypass foreground cleanup. The adapter does not fake a successful Ignore or repeatedly retry unsafe I/O. See the [DOS INT24 response contract](https://fd.lod.bz/rbil/interrup/dos_kernel/24.html). Neither handler calls DOS, allocates, prints, retries or exits. A failed I/O operation must still be handled normally by its caller. Flags persist after close; only successful open clears them. DOS before3 is rejected before vectors change.

Only one process-local owner is supported. Open while active returns BUSY. Close restores only vectors still pointing to this adapter; it reports a bitmask and leaves any later foreign handler installed. Repeated close is harmless. A later handler which chains to this module still requires this code to remain resident; the guard is not a TSR manager and cannot remove itself from a foreign chain. Restore application resources before closing it. This is not crash recovery, NMI handling, power-failure protection, or a guarantee every DOS break/error returns to the main loop.

The direct INT24 handler deliberately does **not** call `_harderr`: replacing the CRT callback and restoring only the interrupt vector would leave the process-global CRT policy changed.

## Qualification

Run with installed tools (nothing is downloaded):

```
python tools/qualify_guard.py --watcom /path/to/watcom --dosbox /path/to/dosbox-x
```

Open Watcom2026-10-01, `-0 -ecc -ox -s`, adapter additionally `-zu -zt=0`, passed55 native checks in **small, compact, medium and large** models. Tests exercised synthetic INT23/INT24, saved AX/BX/CX/DX/SI/DI, a separately allocated foreign SS, existing `_harderr` callback restoration, all foreign-vector combinations, repeated open/close, busy and simulated DOS2 rejection. Missing `-s` and missing `-zu` builds were rejected. The tests restore the original vectors.

DOSBox-X2025.02.01, Tandy640KB, `8086_prefetch`, normal core, fixed20,000cycles. These are ABI/behavior checks, not physical EX timing. No actual disk/device failure was injected; synthetic INT24 tests cannot validate DOS post-handler Fail-to-Abort behavior. Logs and command/hash manifest are written under `build/guard-matrix/`.
