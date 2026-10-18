/* SPDX-License-Identifier: MIT */
#include "lt_sound.h"
#include <assert.h>
#include <stdio.h>
static lt_u8 bytes[8192];
static unsigned used;
static void sink(void *context, lt_u8 b)
{
    (void)context;
    assert(used < sizeof(bytes));
    bytes[used++] = b;
}

static void chip_tests(void)
{
    lt_chip c;
    unsigned p,ch,n;
    lt_init(&c,sink,0);
    assert(!used && !lt_claim(&c,0,15));
    assert(!lt_claim(&c,1,16) && lt_claim(&c,1,15));
    assert(!lt_claim(&c,2,1));
    for(ch=0;ch<3;++ch) for(p=0;p<1024;++p) {
        used=0;
        assert(lt_tone(&c,1,(lt_u8)ch,(lt_u16)p,1));
        assert(used==2 && bytes[0]==(128|(ch<<5)|(p&15)));
        assert(bytes[1]==(p>>4));
        assert(lt_tone(&c,1,(lt_u8)ch,(lt_u16)p,0) && used==2);
    }
    for(ch=0;ch<4;++ch) for(p=0;p<16;++p) {
        used=0;
        assert(lt_attenuate(&c,1,(lt_u8)ch,(lt_u8)p,1));
        assert(used==1 && bytes[0]==(144|(ch<<5)|p));
    }
    used=0;
    assert(!lt_tone(&c,1,3,1,0) && !lt_tone(&c,1,0,1024,0));
    assert(!lt_attenuate(&c,1,4,0,0));
    assert(!lt_attenuate(&c,1,0,16,0) && !lt_noise(&c,1,8,0));
    assert(!used);
    for(p=0;p<8;++p) {
        used=0;
        assert(lt_noise(&c,1,(lt_u8)p,1));
        assert(used==1 && bytes[0]==(224|p));
        assert(lt_noise(&c,1,(lt_u8)p,0) && used==1);
        assert(lt_noise(&c,1,(lt_u8)p,1) && used==2);
    }
    used=0;
    assert(lt_stop(&c,1,4));
    assert(used==2 && bytes[0]==223 && bytes[1]==255);
    assert(!c.owner[2] && !c.owner[3]);
    assert(lt_claim(&c,2,8));
    assert(!lt_claim(&c,3,12));
    assert(!c.owner[2] && c.owner[3]==2);
    n=used;
    assert(!lt_noise(&c,2,7,1) && used==n);
    assert(lt_claim(&c,2,4) && lt_noise(&c,2,7,1));
    assert(!lt_stop(&c,3,12));
    assert(lt_stop(&c,2,8) && !c.owner[2] && !c.owner[3]);
    assert(lt_stop(&c,1,3));
}
int main(void)
{
    chip_tests();
    puts("PASS chip encoding/ownership/coupling");
    return 0;
}
