/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "FXMERGE.H"
static unsigned checks;
#define CK(x) do{++checks;if(!(x)){printf("FAIL %u %s\n",(unsigned)__LINE__,#x);return 1;}}while(0)
int main(void)
{
    FXMSTATE base,out,old;FXMLAYER fx[5];unsigned i,n;unsigned char bytes[16];
    memset(&base,0,sizeof(base));memset(fx,0,sizeof(fx));
    for(i=0;i<3;++i){base.period[i]=100+i;base.level[i]=i+2;}
    base.noise=4;base.level[3]=6;base.pit=21000;
    FXMMerge(&out,&base,fx);CK(!memcmp(&out,&base,sizeof(out)));
    fx[0].active=1;fx[0].period=400;fx[0].level=1;
    fx[2].active=1;fx[2].period=500;fx[2].level=3;
    fx[3].active=fx[3].borrow2=1;fx[3].period=600;fx[3].level=2;fx[3].noise=7;
    fx[4].active=1;fx[4].pit=1234;FXMMerge(&out,&base,fx);
    CK(out.period[0]==400&&out.level[0]==1&&out.period[1]==101&&out.level[1]==3);
    CK(out.period[2]==600&&out.level[2]==15&&out.level[3]==2&&out.noise==7&&out.pit==1234);
    fx[3].level=15;FXMMerge(&out,&base,fx);CK(out.level[2]==15);
    fx[3].active=fx[4].active=0;FXMMerge(&out,&base,fx);
    CK(out.period[2]==500&&out.level[2]==3&&out.noise==4&&out.pit==21000);
    old=out;CK(!FXMBytes(&out,&old,0,0,bytes));
    out.level[3]=4;n=FXMBytes(&out,&old,0,0,bytes);CK(n==1&&bytes[0]==0xf4);
    n=FXMBytes(&out,&old,0,1,bytes);CK(n==2&&bytes[0]==0xe4&&bytes[1]==0xf4);
    n=FXMBytes(&out,&old,1,0,bytes);CK(n==11);
    for(i=0;i<3;++i)CK(bytes[i*3+2]!=(0x9f|(i<<5)));
    printf("PASS shared FX merger checks=%u\n",checks);return 0;
}
