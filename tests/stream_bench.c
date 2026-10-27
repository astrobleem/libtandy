/* SPDX-License-Identifier: MIT */
/* Native 8088 foreground benchmark. PIT remains at BIOS mode/rate. */
#include <stdio.h>
#include "lt_stream.h"
#include "lt_sfx.h"
#include "../examples/stream/phrase.h"
#include "../examples/stream/chirp.h"
#define phrase phrase_plain
#define phrase_data phrase_plain_data
#include "../examples/stream/phrase_plain.h"
#undef phrase
#undef phrase_data
static lt_sfx_player effects;
static const lt_sfx_event events[] = {
    {0,LTSFX_OP_TONE,1,180,0,3},{3,LTSFX_OP_TONE,1,140,0,5}
};
static const lt_sfx_clip clip={2,2,0,2,6,0,12,60UL,1UL,events};

static lt_u16 pit(void);
#pragma aux pit = "pushf" "cli" "xor al,al" "out 43h,al" \
    "in al,40h" "mov ah,al" "in al,40h" "xchg al,ah" "popf" \
    value [ax] modify exact [ax]
static lt_chip chip;
static lta_handle audio;
static lt_stream_player player;
static unsigned emitted;
static void sink(void *ctx, lt_u8 byte)
{
    (void)ctx;(void)byte;++emitted;
}
static unsigned long bench(const lt_stream *s, unsigned *calls, lt_u8 effect)
{
    unsigned pass;
    unsigned long total=0;
    *calls=0;
    for(pass=0;pass<10U;++pass) {
        if(lt_stream_start(&player,s,effect)!=LTA_OK) return 0;
        while(player.active) {
            lt_u16 start=pit();
            if(lt_stream_tick(&player)!=LTA_OK) return 0;
            total+=(lt_u16)(start-pit());
            ++*calls;
        }
    }
    return total;
}
int main(void)
{
    unsigned i,calls;
    unsigned long empty=0,total;
    lta_request r;
    lt_u16 start;
    lt_init(&chip,sink,0);(void)lta_init(&audio,&chip,1);
    lta_request_init(&r);r.op=LTA_OP_ACQUIRE;r.mask=15;
    if(lta_command(&audio,&r)!=LTA_OK) return 1;
    (void)lt_stream_init(&player,&audio);
    for(i=0;i<1000U;++i) { start=pit();empty+=(lt_u16)(start-pit()); }
    printf("probe PIT clocks=%lu calls=1000\n",empty);
    total=bench(&chirp,&calls,1);
    printf("chirp PIT clocks=%lu calls=%u bytes=%u\n",total,calls,(unsigned)sizeof(chirp_data));
    (void)lt_sfx_init(&effects,&audio);
    total=0;calls=0;
    for(i=0;i<10U;++i) {
        if(lt_sfx_trigger(&effects,&clip)!=LTA_OK) return 1;
        while(effects.active) {
            start=pit();
            if(lt_sfx_tick(&effects)!=LTA_OK) return 1;
            total+=(lt_u16)(start-pit());++calls;
        }
    }
    printf("clip PIT clocks=%lu calls=%u event-bytes=%u\n",total,calls,(unsigned)sizeof(events));
    total=bench(&phrase_plain,&calls,0);
    printf("plain-phrase PIT clocks=%lu calls=%u bytes=%u\n",total,calls,(unsigned)sizeof(phrase_plain_data));
    total=bench(&phrase,&calls,0);
    printf("phrase PIT clocks=%lu calls=%u bytes=%u\n",total,calls,(unsigned)sizeof(phrase_data));
    printf("sizes handle=%u clip-player=%u descriptor=%u stream-player=%u cursor=%u\n",
           (unsigned)sizeof(audio),(unsigned)sizeof(lt_sfx_player),
           (unsigned)sizeof(lt_stream),(unsigned)sizeof(player),
           (unsigned)sizeof(lt_stream_cursor));
    r.op=LTA_OP_RELEASE;(void)lta_command(&audio,&r);
    return total ? 0 : 1;
}
