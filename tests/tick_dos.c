/* SPDX-License-Identifier: MIT; native timer ABI/ownership regression. */
#include <dos.h>
#include <stdio.h>
#include "lt_tick_dos.h"
/* Keep the address-taken callback-local pointer test a real ABI call. */
#pragma inline_depth(0)
typedef void (__interrupt __far *Vector)(void);
extern void lttk_test_seed(unsigned pending, unsigned depth);
extern unsigned lttk_test_pending(void);
extern unsigned lttk_test_stack_used(void);
extern unsigned lttk_test_stack_guard(void);
static volatile unsigned callbacks, bad, slow_once, stop_at;
static unsigned cookie = 0x5aa5U;
static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL line %u\n", (unsigned)__LINE__); } } while (0)
static void idle(void);
#pragma aux idle = "hlt" modify exact [];
static void alternate(unsigned segment, unsigned offset);
#pragma aux alternate = \
 "pushf" "cli" "mov bx,ss" "mov si,sp" \
 "mov ss,ax" "mov sp,dx" "push bx" "push si" "mov cx,8" \
 "sti" "again: hlt" "loop again" "cli" \
 "pop si" "pop bx" "mov ss,bx" "mov sp,si" "popf" \
 parm [ax] [dx] modify exact [bx cx si];
static unsigned local_check(volatile unsigned *p, unsigned value)
{ return *p == value; }
static void update(void *context)
{
    volatile unsigned local = 0x3c69U;
    unsigned long begin;
    if (*(unsigned *)context != 0x5aa5U || !local_check(&local,0x3c69U)) bad=1;
    ++callbacks;
    if (slow_once) {
        slow_once=0; begin=lttk_now();
        while (lttk_now()-begin < 3UL*23864UL) { }
    }
    if (stop_at && callbacks >= stop_at) lttk_run(0);
}
static void __interrupt __far foreign(void) { }
static void wait_ticks(unsigned count)
{
    unsigned long start=lttk_ticks();
    while (lttk_ticks()-start < count) idle();
}
int main(void)
{
    Vector before=_dos_getvect(8);
    unsigned stack_segment=0, observed, previous_calls;
    unsigned long previous_time, now, late;
    void *context=&cookie;
#if defined(__COMPACT__) || defined(__LARGE__)
    unsigned context_segment=0;
    setvbuf(stdout,0,_IONBF,0);
    puts("timer phase: allocation");
    if (_dos_allocmem(1,&context_segment)) { puts("allocation failed"); return 2; }
    puts("timer phase: allocated");
    context=(void *)MK_FP(context_segment,0);
    *(unsigned *)context=0x5aa5U;
#endif
    setvbuf(stdout,0,_IONBF,0);
    puts("timer phase: init");
    CHECK(lttk_close()==LTTK_NOT_OPEN);
    CHECK(lttk_open(999U,update,context)==LTTK_INVALID);
    CHECK(lttk_open(23864U,0,context)==LTTK_INVALID);
    CHECK(_dos_getvect(8)==before);
    CHECK(lttk_open(23864U,update,context)==LTTK_OK);
    CHECK(lttk_open(23864U,update,context)==LTTK_BUSY);
    CHECK(lttk_divisor()==23864U && !lttk_faults());
    wait_ticks(2); CHECK(callbacks==0);
    lttk_run(1); wait_ticks(4); CHECK(callbacks>=4 && !bad);
    lttk_defer(1);lttk_defer(1);previous_calls=callbacks;
    wait_ticks(3);CHECK(callbacks==previous_calls);
    lttk_defer(0);wait_ticks(2);CHECK(callbacks==previous_calls);
    lttk_defer(0);wait_ticks(2);CHECK(callbacks>=previous_calls+7);
    lttk_run(0);previous_calls=callbacks;wait_ticks(2);CHECK(callbacks==previous_calls);
    previous_time=lttk_now();
    { unsigned i; for(i=0;i<2000U;++i){now=lttk_now();CHECK(now>=previous_time);previous_time=now;} }
    puts("timer phase: foreign stack");
    CHECK(_dos_allocmem(256,&stack_segment)==0);
    if(stack_segment){
        lttk_run(1);previous_calls=callbacks;
        alternate(stack_segment,4094U);
        lttk_run(0);CHECK(callbacks>previous_calls && !bad);
        _dos_freemem(stack_segment);
    }
    puts("timer phase: nested IRQ");
    slow_once=1;lttk_run(1);wait_ticks(8);lttk_run(0);
    late=lttk_late();CHECK(late>=2 && lttk_backlog_max()>=2);
    observed=lttk_test_stack_used();
    CHECK(observed>0 && observed<LTTK_STACK_BYTES-16U && lttk_test_stack_guard());
    /* Seed a backlog under CLI, then let ONE IRQ drain. Callback stops itself
     * after a budget of calls so foreground can inspect remaining debt. */
    puts("timer phase: budget and faults");
    lttk_run(1);lttk_defer(1);previous_calls=callbacks;
    lttk_test_seed(20U,1U);stop_at=(unsigned)(callbacks+LTTK_DRAIN_BUDGET);
    lttk_defer(0);wait_ticks(1);
    CHECK(callbacks==previous_calls+LTTK_DRAIN_BUDGET);
    CHECK(lttk_test_pending()>=13U);stop_at=0;lttk_run(0);
    lttk_defer(1);lttk_test_seed(65535U,1U);wait_ticks(1);
    CHECK(lttk_faults()==LTTK_FAULT_PENDING);
    previous_calls=callbacks;lttk_run(1);lttk_defer(0);wait_ticks(2);CHECK(callbacks==previous_calls);
    CHECK(lttk_close()==LTTK_OK && _dos_getvect(8)==before);
    CHECK(lttk_divisor()==0 && lttk_now()==0);
    CHECK(lttk_open(23864U,update,context)==LTTK_OK && !lttk_faults());
    lttk_test_seed(0,65535U);lttk_defer(1);
    CHECK(lttk_faults()==LTTK_FAULT_DEFER);
    CHECK(lttk_close()==LTTK_OK && _dos_getvect(8)==before);
    CHECK(lttk_open(23864U,update,context)==LTTK_OK);
    _disable();_dos_setvect(8,foreign);
    CHECK(lttk_close()==LTTK_NOT_OURS && _dos_getvect(8)==foreign);
    _dos_setvect(8,before);_enable();
#if defined(__COMPACT__) || defined(__LARGE__)
    _dos_freemem(context_segment);
#endif
    printf("%s timer checks=%u failures=%u data_ptr=%u callback_ptr=%u observed_stack=%u/%u late=%lu faults/restoration/budget=checked\n",failures?"FAIL":"PASS",checks,failures,(unsigned)sizeof(void*),(unsigned)sizeof(lttk_work),observed,LTTK_STACK_BYTES,late);
    return failures?1:0;
}
