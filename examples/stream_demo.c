/* SPDX-License-Identifier: MIT */
/* Original Tandy PSG ducking/loop/pause example. No install or data files. */
#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include "lt_stream.h"
#include "lt_sfx.h"
#include "stream/phrase.h"

extern void lt_dos_emit(void *, lt_u8);
static lt_u16 port = 0xC0;
static lt_chip chip;
static lta_handle audio;
static lt_stream_player music;
static lt_sfx_player effects;
static const lt_sfx_event engine_events[] = {
    {0,LTSFX_OP_TONE,0,210,0,3},
    {6,LTSFX_OP_TONE,0,220,0,5},
    {12,LTSFX_OP_TONE,0,200,0,4}
};
static const lt_sfx_clip engine = {2,1,2,3,18,0,18,60UL,1UL,engine_events};

static int retrace(void)
{
    unsigned i;
    for(i=0;i<65535U;++i) if (!(inp(0x3DA)&8)) break;
    if(i==65535U) return 0;
    for(i=0;i<65535U;++i) if (inp(0x3DA)&8) return 1;
    return 0;
}
static lta_status duck(lt_u8 value)
{
    lta_status s = lta_music_attenuation(&audio,0,value);
    if(s==LTA_OK) s=lta_music_attenuation(&audio,1,value);
    return s;
}
int main(int argc, char **argv)
{
    unsigned frame;
    unsigned long irq0 = *(unsigned long __far *)MK_FP(0,0x20);
    int key=0, automatic=(argc>1 && !strcmp(argv[1],"/auto"));
    lta_request r;
    lta_status status;
    puts("Original PSG stream / duck / looping effect demo");
    puts("L loop, C cancel at boundary, S stop effect, E pause effect,");
    puts("P pause music, Escape quit. /auto runs six seconds.");
    lt_init(&chip,lt_dos_emit,&port);
    status=lta_init(&audio,&chip,1);
    lta_request_init(&r);r.op=LTA_OP_ACQUIRE;r.mask=15;
    if(status==LTA_OK) status=lta_command(&audio,&r);
    if(status!=LTA_OK) return 1;
    (void)lt_stream_init(&music,&audio);
    (void)lt_sfx_init(&effects,&audio);
    status=lt_stream_start(&music,&phrase,0);
    /* Quiet second voice continues while the effect borrows the melody. */
    r.op=LTA_OP_TONE_START;r.channel=1;r.period=856;r.level=10;
    if(status==LTA_OK) status=lta_command(&audio,&r);
    for(frame=0;status==LTA_OK && frame<360U;++frame) {
        if(!retrace()) { status=LTA_TIME; break; }
        key=kbhit()?getch():0;
        if(key==27) break;
        if(automatic) {
            if(frame==60U) key='l';
            if(frame==120U || frame==150U) key='e';
            if(frame==180U || frame==210U) key='p';
            if(frame==240U) key='c';
        }
        if(key=='l' || key=='L') {
            status=duck(5);
            if(status==LTA_OK) status=lt_sfx_loop(&effects,&engine,0);
        }
        if(key=='c' || key=='C') status=lt_sfx_cancel(&effects);
        if(key=='s' || key=='S') status=lt_sfx_stop(&effects);
        if((key=='e' || key=='E') && effects.active)
            status=lt_sfx_pause(&effects,(lt_u8)!effects.paused);
        if(key=='p' || key=='P')
            status=lt_stream_pause(&music,(lt_u8)!music.paused);
        if(status==LTA_OK) status=lt_stream_tick(&music);
        if(status==LTA_OK) status=lt_sfx_tick(&effects);
        if(status==LTA_OK && !effects.active && audio.music_attenuation[0])
            status=duck(0);
        if(!automatic && frame==359U) frame=0; /* interactive until Escape */
    }
    (void)lt_sfx_stop(&effects);
    (void)lt_stream_stop(&music);
    r.op=LTA_OP_RELEASE;(void)lta_command(&audio,&r);
    printf("STREAMDEMO status=%u frames=%u muted/released=%u irq0_unchanged=%u\n",
           (unsigned)status,frame,(unsigned)(!audio.active && !chip.owner[0]),
           (unsigned)(irq0 == *(unsigned long __far *)MK_FP(0,0x20)));
    return status==LTA_OK ? 0 : 1;
}
