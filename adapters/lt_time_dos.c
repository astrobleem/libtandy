/* SPDX-License-Identifier: MIT */
#include "lt_time_dos.h"
static unsigned flags(void);
#pragma aux flags = "pushf" "pop ax" value [ax] modify exact [ax]
static unsigned long read_raw(void);
#pragma aux read_raw = \
 "pushf" "cli" "push es" "mov ax,40h" "mov es,ax" \
 "mov ax,es:[6ch]" "mov dx,es:[6eh]" "pop es" "popf" \
 value [dx ax] modify exact [ax dx]
unsigned long ltd_raw_ticks(void){return read_raw();}
ltt_status ltd_clock_init(ltt_clock *c,int compatible)
{
 unsigned long elapsed;
 if(!c)return LTT_INVALID;
 if(!compatible)return LTT_UNSUPPORTED;
 ltt_init(c);return ltt_sample(c,ltd_raw_ticks(),&elapsed);
}
ltt_status ltd_now(ltt_clock *c,unsigned long *elapsed)
{return ltt_sample(c,ltd_raw_ticks(),elapsed);}
ltt_status ltd_delay_ms(ltt_clock *c,lts_u16 ms,
                       int (*cooperate)(void *),void *context)
{
 unsigned long start,now,previous,need;
 unsigned idle=0;
 ltt_status status;
 if(!(flags()&0x200U))return LTT_CONTEXT;
 status=ltd_now(c,&start);if(status!=LTT_OK)return status;
 if(!ms)return LTT_OK;
 need=((unsigned long)ms*1000UL+54924UL)/54925UL+1UL;previous=start;
 for(;;) {
  if(cooperate && cooperate(context))return LTT_CANCELLED;
  if(!(flags()&0x200U))return LTT_CONTEXT;
  status=ltd_now(c,&now);if(status!=LTT_OK)return status;
  if(now-start>=need)return LTT_OK;
  if(now==previous){if(++idle==65535U)return LTT_STALLED;}
  else{idle=0;previous=now;}
 }
}
