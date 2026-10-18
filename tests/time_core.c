/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_time.h"
#define CHECK(v) do{if(!(v)){printf("FAIL time line%d\n",__LINE__);return 1;}}while(0)
int main(void)
{
 ltt_clock c,before;unsigned long out;
 ltt_init(&c);CHECK(ltt_sample(&c,LTT_DAY_TICKS-2UL,&out)==LTT_OK && out==0);
 CHECK(ltt_sample(&c,LTT_DAY_TICKS-1UL,&out)==LTT_OK && out==1);
 CHECK(ltt_sample(&c,0,&out)==LTT_OK && out==2);
 CHECK(ltt_sample(&c,5,&out)==LTT_OK && out==7);
 CHECK(ltt_sample(&c,4,&out)==LTT_RESET && out==7 && c.resets==1);
 CHECK(ltt_sample(&c,5,&out)==LTT_OK && out==8);
 c.elapsed=LTT_MAX_TICKS-1UL;
 CHECK(ltt_sample(&c,7,&out)==LTT_SATURATED && out==LTT_MAX_TICKS);
 CHECK(ltt_sample(&c,8,&out)==LTT_SATURATED && out==LTT_MAX_TICKS);
 before=c;out=123;
 CHECK(ltt_sample(&c,LTT_DAY_TICKS,&out)==LTT_INVALID && out==123 && !memcmp(&c,&before,sizeof(c)));
 ltt_init(&c);CHECK(ltt_sample(&c,65534UL,&out)==LTT_OK);
 CHECK(ltt_sample(&c,65536UL,&out)==LTT_OK && out==2);
 puts("time core passed midnight,wordcarry,reset,saturation,transactionality");return 0;
}
