/* SPDX-License-Identifier: MIT */
#ifndef LT_GUARD_DOS_H
#define LT_GUARD_DOS_H

/* Optional foreground DOS 3+ orderly-exit service, Open Watcom 16-bit.
 * Compile adapters/lt_guard_dos.c separately with -s -zu. No compiler flags
 * are required on callers beyond a consistent memory model/calling convention.
 * Install before owning video, timers or sound. Handlers only latch flags;
 * foreground must poll quit, restore its resources, then call close. Neither
 * handler calls DOS, retries an operation, allocates, prints or exits.
 * One active owner per process; do not nest or call from an IRQ. */
#define LTGUARD_OK 0
#define LTGUARD_BUSY 1
#define LTGUARD_DOS_TOO_OLD 2
#define LTGUARD_LOST_BREAK 1
#define LTGUARD_LOST_CRITICAL 2

/* Open clears flags and installs INT23/INT24. BUSY leaves existing state alone.
 * DOS <3 is rejected before changing vectors: INT24 FAIL requires DOS 3+. */
int ltguard_open(void);
/* Restore only vectors still owned by this service. Return a bitmask of lost
 * ownership; foreign replacements remain installed. Repeated close returns 0.
 * This does not restore the caller's video/PIT/PSG; do that BEFORE close. */
unsigned ltguard_close(void);
/* Flags remain latched after close for diagnostics; a successful open clears.
 * INT23 latches quit; INT24 latches both and requests AL=3 (Fail).
 * If DOS disallows Fail (entry AH bit3 clear), DOS may instead Abort and
 * bypass foreground cleanup. This guard cannot guarantee orderly recovery.
 * A failed I/O call still requires normal caller error handling and cleanup. */
unsigned ltguard_quit(void);
unsigned ltguard_critical(void);
#endif
