/* SPDX-License-Identifier: MIT */
#include "lt_time.h"
void ltt_init(ltt_clock *c)
{if(c){c->last=0;c->elapsed=0;c->initialized=0;c->resets=0;}}
ltt_status ltt_sample(ltt_clock *c,unsigned long raw,unsigned long *elapsed)
{
 unsigned long delta;
 if(!c || !elapsed || raw>=LTT_DAY_TICKS || c->initialized>1U ||
    c->last>=LTT_DAY_TICKS || c->elapsed>LTT_MAX_TICKS) return LTT_INVALID;
 if(!c->initialized){c->initialized=1;c->last=raw;*elapsed=c->elapsed;return LTT_OK;}
 delta=raw>=c->last?raw-c->last:LTT_DAY_TICKS-c->last+raw;
 c->last=raw;
 if(delta>LTT_DAY_TICKS/2UL) {
  if(c->resets!=65535U)c->resets++;
  *elapsed=c->elapsed;return LTT_RESET;
 }
 if(c->elapsed==LTT_MAX_TICKS || delta>LTT_MAX_TICKS-c->elapsed) {
  c->elapsed=LTT_MAX_TICKS;*elapsed=c->elapsed;return LTT_SATURATED;
 }
 c->elapsed+=delta;*elapsed=c->elapsed;return LTT_OK;
}
