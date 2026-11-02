/* SPDX-License-Identifier: MIT */
/* Synthetic software interrupts only: no disk errors are injected. */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lt_guard_dos.h"
typedef void (__interrupt __far *Vector)(void);
static unsigned checks,failures;
static volatile unsigned crt_calls;
static void __interrupt __far foreign(void) { }
static int __far crt_policy(unsigned deverr,unsigned errcode,unsigned __far *devhdr)
{ (void)deverr;(void)errcode;(void)devhdr;++crt_calls;_hardresume(_HARDERR_IGNORE);return 0; }
static unsigned critical_on_stack(unsigned segment,unsigned offset);
#pragma aux critical_on_stack = \
 "pushf" "cli" "mov bx,ss" "mov cx,sp" \
 "mov ss,ax" "mov sp,dx" "push bx" "push cx" "sti" \
 "mov ax,1234h" "int 24h" "cli" "pop cx" "pop bx" \
 "mov ss,bx" "mov sp,cx" "popf" \
 parm [ax] [dx] value [ax] modify exact [ax bx cx];
#define CHECK(x) do {++checks;if(!(x)){++failures;printf("FAIL line=%u\n",(unsigned)__LINE__);}}while(0)
static void call_int(unsigned vector,unsigned wantax)
{
    union REGS in,out;struct SREGS segments;
    memset(&in,0,sizeof(in));segread(&segments);
    in.x.ax=0x1234;in.x.bx=0x5678;in.x.cx=0x9abc;in.x.dx=0xdef0;
    in.x.si=0xa55a;in.x.di=0x5aa5;
    int86x(vector,&in,&out,&segments);
    if(out.x.ax!=wantax)printf("vector=%u AX=%04x expected=%04x\n",vector,out.x.ax,wantax);
    CHECK(out.x.ax==wantax);CHECK(out.x.bx==in.x.bx);
    CHECK(out.x.cx==in.x.cx);CHECK(out.x.dx==in.x.dx);
    CHECK(out.x.si==in.x.si);CHECK(out.x.di==in.x.di);
}
int main(void)
{
    Vector old23=_dos_getvect(0x23),old24=_dos_getvect(0x24),crt24;
    unsigned stack=0,mask;unsigned char version=_osmajor;
    setvbuf(stdout,0,_IONBF,0);
    CHECK(ltguard_close()==0);
    _osmajor=2;CHECK(ltguard_open()==LTGUARD_DOS_TOO_OLD);_osmajor=version;
    CHECK(_dos_getvect(0x23)==old23&&_dos_getvect(0x24)==old24);
    _harderr(crt_policy);crt24=_dos_getvect(0x24);
    CHECK(ltguard_open()==LTGUARD_OK);CHECK(ltguard_open()==LTGUARD_BUSY);
    CHECK(!ltguard_quit()&&!ltguard_critical());
    call_int(0x23,0x1234);CHECK(ltguard_quit()&&!ltguard_critical());
    CHECK(ltguard_close()==0);CHECK(ltguard_quit()&&!ltguard_critical());
    CHECK(_dos_getvect(0x23)==old23&&_dos_getvect(0x24)==crt24);
    CHECK(ltguard_close()==0);CHECK(ltguard_open()==LTGUARD_OK);
    CHECK(!ltguard_quit()&&!ltguard_critical());
    call_int(0x24,0x1203);CHECK(ltguard_quit()&&ltguard_critical());
    CHECK(crt_calls==0);CHECK(ltguard_close()==0);
    /* Closing must restore not just the vector but the untouched CRT policy. */
    /* Watcom CRT _hardresume clears AH, unlike our AL-only handler. */
    call_int(0x24,0x0000);CHECK(crt_calls==1);
    CHECK(ltguard_open()==LTGUARD_OK);
    CHECK(_dos_allocmem(128,&stack)==0);
    if(stack) {
        CHECK(critical_on_stack(stack,2046U)==0x1203U);
        CHECK(ltguard_quit()&&ltguard_critical());
        _dos_freemem(stack);
    }
    CHECK(ltguard_close()==0);
    for(mask=1;mask<=3;++mask) {
        CHECK(ltguard_open()==LTGUARD_OK);
        if(mask&1)_dos_setvect(0x23,foreign);
        if(mask&2)_dos_setvect(0x24,foreign);
        CHECK(ltguard_close()==mask);CHECK(ltguard_close()==0);
        CHECK(_dos_getvect(0x23)==((mask&1)?foreign:old23));
        CHECK(_dos_getvect(0x24)==((mask&2)?foreign:crt24));
        _dos_setvect(0x23,old23);_dos_setvect(0x24,crt24);
    }
    _dos_setvect(0x23,old23);_dos_setvect(0x24,old24);
    printf("%s guard checks=%u failures=%u int=%u ptr=%u; registers/foreignSS/CRT-policy/ownership restored; no disk fault injected\n",failures?"FAIL":"PASS",checks,failures,(unsigned)sizeof(int),(unsigned)sizeof(void*));
    return failures?1:0;
}
