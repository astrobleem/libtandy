/* SPDX-License-Identifier: MIT */
/* Fixed-rate IRQ0 update clock; see lt_tick_dos.h. Explicit near state
 * keeps the private stack and assembly operands in DGROUP in every tested
 * memory model, including far-data builds with -zt=0. Compile with -s. */
#if !defined(__WATCOMC__) || !defined(__I86__) || !defined(__DOS__)
#error lt_tick_dos.c requires Open Watcom 16-bit DOS
#endif
#if !defined(__SW_S) || !__SW_S
#error Compile lt_tick_dos.c and all interrupt-reachable C with -s
#endif
#include <conio.h>
#include <dos.h>
#include "lt_tick_dos.h"

typedef void (__interrupt __far *lttk_vector)(void);

/* Keep one object per declaration: __near applies per declarator, not
 * to every name in a comma-separated declaration in far-data models. */
static lttk_vector __near previous;
static lttk_work __near work_fn;
static void * __near work_ctx;
static unsigned __near divisor_now;
static volatile unsigned long __near tick_count;
static unsigned long __near bios_acc;
static volatile unsigned __near waiting;
static volatile unsigned __near in_work;
static volatile unsigned __near defer_depth;
static volatile unsigned char __near running;
static unsigned char __near is_open;
static volatile unsigned __near backlog_peak;
static volatile unsigned __near fault_flags;
static volatile unsigned long __near late_count;
static unsigned __near saved_ss;
static unsigned __near saved_sp;
static unsigned __near private_sp;
static unsigned char __near private_stack[LTTK_STACK_BYTES];
static void (__near * __near drain_ptr)(void);

static unsigned lttk_cli(void)
{
    unsigned f = 0;
    _asm {
        pushf
        pop f
        cli
    }
    return f;
}

static void lttk_sti(unsigned f)
{
    _asm {
        push f
        popf
    }
}

/* Called with interrupts disabled. A fault is sticky until close/open;
 * never silently wrap pending work or nesting into an apparently idle state. */
static void __near lttk_fault(unsigned flag)
{
    fault_flags |= flag;
    running = 0;
    waiting = 0;
}

/* On the private stack with IF=1; always return with IF=0 before restoring
 * SS:SP. A bounded drain retains excess debt for the next timer interrupt. */
static void __near lttk_drain(void)
{
    unsigned serviced;
    for (serviced = 0; serviced < LTTK_DRAIN_BUDGET; ++serviced) {
        _disable();
        if (!waiting || defer_depth || fault_flags) return;
        --waiting;
        _enable();
        if (running) work_fn(work_ctx);
    }
    _disable();
}

static void __interrupt __far lttk_isr(void)
{
    ++tick_count;
    bios_acc += divisor_now;
    if (bios_acc >= 65536UL) {
        bios_acc -= 65536UL;
        (*previous)();              /* BIOS tick, INT 1Ch, EOI */
    } else {
        outp(0x20, 0x20);
    }
    if (fault_flags) return;
    if (waiting == 65535U) {
        lttk_fault(LTTK_FAULT_PENDING);
        return;
    }
    ++waiting;
    if (in_work) {                  /* the outer interrupt will run it */
        ++late_count;
        if (waiting > backlog_peak) backlog_peak = waiting;
        return;
    }
    if (defer_depth) return;
    in_work = 1;
    _asm {
        mov saved_ss, ss
        mov saved_sp, sp
        mov ax, ds
        mov ss, ax
        mov sp, private_sp
        cld
        sti
        call word ptr drain_ptr
        cli
        /* Far-data callbacks may return with DS addressing arbitrary data. */
        mov ax, ss
        mov ds, ax
        mov ss, saved_ss
        mov sp, saved_sp
    }
    in_work = 0;
}

lttk_status lttk_open(unsigned divisor, lttk_work work, void *context)
{
    unsigned f;
    if (is_open) return LTTK_BUSY;
    if (work == 0 || divisor < 1000U) return LTTK_INVALID;
    work_fn = work;
    work_ctx = context;
    divisor_now = divisor;
    tick_count = 0;
    bios_acc = 0;
    waiting = 0;
    in_work = 0;
    defer_depth = 0;
    running = 0;
    backlog_peak = 0;
    late_count = 0;
    fault_flags = 0;
#ifdef LTTK_TEST
    {
        unsigned i;
        for (i = 0; i < LTTK_STACK_BYTES; ++i) private_stack[i] = 0xa5U;
    }
#endif
    private_sp = (unsigned)(private_stack + LTTK_STACK_BYTES - 2U);
    drain_ptr = lttk_drain;
    f = lttk_cli();
    previous = _dos_getvect(8);
    _dos_setvect(8, lttk_isr);
    outp(0x43, 0x34);               /* channel 0, lo/hi byte, mode 2 */
    outp(0x40, divisor & 0xFFU);
    outp(0x40, divisor >> 8);
    is_open = 1;
    lttk_sti(f);
    return LTTK_OK;
}

lttk_status lttk_close(void)
{
    unsigned f;
    lttk_status st = LTTK_OK;
    if (!is_open) return LTTK_NOT_OPEN;
    f = lttk_cli();
    running = 0;
    outp(0x43, 0x36);               /* channel 0, lo/hi byte, mode 3 */
    outp(0x40, 0);
    outp(0x40, 0);                  /* divisor 65,536 */
    if (_dos_getvect(8) == lttk_isr) _dos_setvect(8, previous);
    else st = LTTK_NOT_OURS;
    is_open = 0;
    lttk_sti(f);
    return st;
}

void lttk_run(int on)
{
    unsigned f = lttk_cli();
    if (on && fault_flags) { lttk_sti(f); return; }
    if (on && !running) waiting = 0;
    running = (unsigned char)(on != 0);
    lttk_sti(f);
}

void lttk_defer(int on)
{
    unsigned f = lttk_cli();
    if (on) {
        if (defer_depth == 65535U) lttk_fault(LTTK_FAULT_DEFER);
        else ++defer_depth;
    }
    else if (defer_depth) --defer_depth;
    lttk_sti(f);
}

unsigned lttk_divisor(void)
{
    return is_open ? divisor_now : 0U;
}

unsigned long lttk_ticks(void)
{
    unsigned f = lttk_cli();
    unsigned long t = tick_count;
    lttk_sti(f);
    return t;
}

unsigned long lttk_now(void)
{
    unsigned f, lo, hi, pending, elapsed;
    unsigned long t;
    if (!is_open) return 0;
    f = lttk_cli();
    outp(0x43, 0x00);               /* latch channel 0 */
    lo = inp(0x40);
    hi = inp(0x40);
    t = tick_count;
    outp(0x20, 0x0A);               /* read the PIC request register */
    pending = inp(0x20) & 1U;
    lttk_sti(f);
    elapsed = divisor_now - (lo | (hi << 8));
    /* A reload not yet serviced starts the next period (small elapsed). */
    if (pending && elapsed < divisor_now / 2U) ++t;
    return t * divisor_now + elapsed;
}

unsigned lttk_backlog_max(void)
{
    return backlog_peak;
}

unsigned long lttk_late(void)
{
    unsigned f = lttk_cli();
    unsigned long result = late_count;
    lttk_sti(f);
    return result;
}

unsigned lttk_faults(void)
{
    return fault_flags;
}

#ifdef LTTK_TEST
/* Boundary injection and observed stack use, compiled only for native tests. */
void lttk_test_seed(unsigned pending, unsigned depth)
{
    unsigned f = lttk_cli();
    waiting = pending;
    defer_depth = depth;
    lttk_sti(f);
}
unsigned lttk_test_pending(void) { return waiting; }
unsigned lttk_test_stack_used(void)
{
    unsigned f = lttk_cli(), i = 0;
    while (i < LTTK_STACK_BYTES && private_stack[i] == 0xa5U) ++i;
    lttk_sti(f);
    return LTTK_STACK_BYTES - i;
}
unsigned lttk_test_stack_guard(void)
{
    unsigned f = lttk_cli(), i, ok = 1;
    for (i = 0; i < 16U; ++i) if (private_stack[i] != 0xa5U) ok = 0;
    lttk_sti(f);
    return ok;
}
#endif
