/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "lt_stream.h"
#include "lt_sfx.h"
#include "../examples/stream/phrase.h"
#include "../examples/stream/chirp.h"
#define phrase phrase_plain
#define phrase_data phrase_plain_data
#include "../examples/stream/phrase_plain.h"
#undef phrase
#undef phrase_data

static lt_chip chip;
static lta_handle audio;
static lt_stream_player music, effect;
static lt_sfx_player sfx;
static unsigned failures, count;
static lt_u8 trace[4096], saved[4096];
#define CHECK(x) do { if (!(x)) { printf("FAIL stream:%u %s\n", \
    (unsigned)__LINE__, #x); ++failures; } } while (0)
static void emit(void *ctx, lt_u8 byte)
{
    (void)ctx;
    if (count < sizeof(trace)) trace[count++] = byte;
    else ++failures;
}
static lta_status command(lt_u8 op, lt_u8 ch, lt_u16 period, lt_u8 level)
{
    lta_request r;
    lta_request_init(&r);
    r.op = op; r.channel = ch; r.period = period; r.level = level;
    r.mask = 15U;
    return lta_command(&audio, &r);
}
static void setup(void)
{
    lt_init(&chip, emit, 0);
    CHECK(lta_init(&audio, &chip, 1U) == LTA_OK);
    CHECK(command(LTA_OP_ACQUIRE, 0, 0, 0) == LTA_OK);
    CHECK(lt_stream_init(&music, &audio) == LTA_OK);
    CHECK(lt_stream_init(&effect, &audio) == LTA_OK);
    CHECK(lt_sfx_init(&sfx, &audio) == LTA_OK);
    count = 0U;
}
static const lt_u8 packed[] = {
    0x10, 100,0,2, 1,2,0, 0x10,120,0,4, 1,2,0,
    2,0,0,14,0,7, 0
};
static const lt_u8 plain[] = {
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,
    0x10,100,0,2,1,2,0,0x10,120,0,4,1,2,0,0
};
static const lt_sfx_event events[] = {
    {0, LTSFX_OP_TONE, 0, 80,0,3},
    {1, LTSFX_OP_TONE, 0, 90,0,5}
};
static const lt_sfx_clip clip = {2,1,2,2,2,0,12,60UL,1UL,events};
static lt_stream descriptor(const lt_u8 *data, lt_u16 bytes)
{
    lt_stream s;
    s.data = data; s.bytes = bytes; s.version = 1U; s.chip = 1U;
    s.channel_mask = 1U; s.priority = 2U;
    s.clock_hz = 3579545UL; s.rate_num = 60UL; s.rate_den = 1UL;
    return s;
}
static void traces(void)
{
    lt_stream s = descriptor(packed, sizeof(packed));
    unsigned length, i;
    setup();
    CHECK(lt_stream_start(&music, &s, 0) == LTA_OK);
    for (i = 0; i < 33U; ++i) CHECK(lt_stream_tick(&music) == LTA_OK);
    CHECK(!music.active);
    length = count; memcpy(saved, trace, count);
    setup(); s.data = plain; s.bytes = sizeof(plain);
    CHECK(lt_stream_start(&music, &s, 0) == LTA_OK);
    for (i = 0; i < 33U; ++i) CHECK(lt_stream_tick(&music) == LTA_OK);
    CHECK(count == length && memcmp(saved, trace, count) == 0);
    CHECK(lt_stream_start(&effect, &s, 1) == LTA_OK);
    CHECK(lt_stream_tick(&effect) == LTA_OK);
    CHECK(lt_stream_pause(&effect, 1) == LTA_OK);
    length = count;
    CHECK(lt_stream_tick(&effect) == LTA_OK && count == length);
    CHECK(lt_stream_pause(&effect, 0) == LTA_OK);
    CHECK(lt_stream_stop(&effect) == LTA_OK);
    CHECK(lt_stream_pause(&effect, 0) == LTA_STATE);
}
static void generated_assets(void)
{
    unsigned i, length;
    setup();
    CHECK(lt_stream_start(&music,&phrase,0)==LTA_OK);
    for(i=0;i<481U;++i) CHECK(lt_stream_tick(&music)==LTA_OK);
    CHECK(!music.active);
    length=count;memcpy(saved,trace,count);
    setup();
    CHECK(lt_stream_start(&music,&phrase_plain,0)==LTA_OK);
    for(i=0;i<481U;++i) CHECK(lt_stream_tick(&music)==LTA_OK);
    CHECK(count==length && memcmp(saved,trace,count)==0);
    CHECK(lt_stream_start(&effect,&chirp,1)==LTA_OK);
    CHECK(lt_stream_tick(&effect)==LTA_OK);
    CHECK(lt_stream_pause(&effect,1)==LTA_OK && chip.attenuation[1]==15);
    CHECK(lt_stream_pause(&effect,0)==LTA_OK && chip.attenuation[1]==3);
    CHECK(lt_stream_stop(&effect)==LTA_OK);
}
static void attenuation_and_loops(void)
{
    unsigned i, before;
    setup();
    CHECK(command(LTA_OP_TONE_START,0,100,2) == LTA_OK);
    CHECK(lta_music_attenuation(&audio,0,4) == LTA_OK);
    CHECK(chip.attenuation[0] == 6 && audio.level[0] == 2);
    CHECK(lta_music_attenuation(&audio,0,15) == LTA_OK);
    CHECK(chip.attenuation[0] == 15 && audio.level[0] == 2);
    CHECK(lta_music_attenuation(&audio,0,0) == LTA_OK);
    CHECK(chip.attenuation[0] == 2);
    CHECK(lt_sfx_loop(&sfx,&clip,0) == LTA_OK);
    CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(lta_sfx_attenuation(&audio,4) == LTA_OK);
    CHECK(chip.attenuation[0] == 7 && audio.effect_level[0] == 3);
    before = count;
    CHECK(lta_music_attenuation(&audio,0,5) == LTA_OK && count == before);
    CHECK(command(LTA_OP_TONE_UPDATE,0,333,6) == LTA_OK);
    CHECK(audio.period[0] == 333 && chip.period[0] == 80);
    CHECK(lt_sfx_pause(&sfx,1) == LTA_OK);
    for (i=0;i<10U;++i) CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(sfx.elapsed == 1U);
    CHECK(lt_sfx_pause(&sfx,0) == LTA_OK);
    for (i=0;i<10U;++i) CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(sfx.active);
    CHECK(lta_sfx_attenuation(&audio,15) == LTA_OK);
    CHECK(chip.attenuation[0] == 15);
    CHECK(lt_sfx_cancel(&sfx) == LTA_OK);
    for (i=0;i<3U;++i) CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(!sfx.active && chip.period[0] == 333 && chip.attenuation[0] == 11);
    CHECK(trace[count-3] == (lt_u8)(0x80U | (333U & 15U)));
    CHECK(trace[count-2] == (333U >> 4) && trace[count-1] == 0x9BU);
    CHECK(lt_sfx_loop(&sfx,&clip,3) == LTA_OK);
    for (i=0;i<6U;++i) CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(sfx.active && sfx.plays_left == 1);
    CHECK(lt_sfx_tick(&sfx) == LTA_OK && !sfx.active);
    CHECK(lt_sfx_loop(&sfx,&clip,65535U) == LTA_OK);
    for (i=0;i<3U;++i) CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(sfx.plays_left == 65534U);
    CHECK(lt_sfx_stop(&sfx) == LTA_OK);
    CHECK(lt_sfx_pause(&sfx,0) == LTA_STATE);
    CHECK(lta_music_attenuation(&audio,0,16) == LTA_INVALID);
    CHECK(lta_sfx_attenuation(&audio,16) == LTA_INVALID);
}
static void malformed(void)
{
    lt_u8 data[600];
    lt_stream s = descriptor(data, sizeof(packed));
    unsigned i;
    memcpy(data,packed,sizeof(packed));
    for (i=0;i<sizeof(packed);++i) {
        s.bytes=(lt_u16)i;
        CHECK(lt_stream_validate(&s) == LTA_INVALID);
    }
    s.bytes=sizeof(packed);
    data[15]=1; CHECK(lt_stream_validate(&s) == LTA_INVALID); /* alignment */
    data[15]=0; data[17]=15; CHECK(lt_stream_validate(&s) == LTA_INVALID);
    memcpy(data,packed,sizeof(packed)); data[19]=0;
    CHECK(lt_stream_validate(&s) == LTA_INVALID); /* zero count */
    memcpy(data,packed,sizeof(packed)); data[15]=255; data[16]=255;
    CHECK(lt_stream_validate(&s) == LTA_INVALID); /* offset overflow */
    memcpy(data,packed,sizeof(packed)); data[5]=0;
    CHECK(lt_stream_validate(&s) == LTA_INVALID); /* zero wait */
    memcpy(data,packed,sizeof(packed)); data[0]=0x11;
    CHECK(lt_stream_validate(&s) == LTA_INVALID); /* undeclared voice */
    /* 64 LEVELs plus WAIT exceed one tick's command budget. */
    data[0]=0x10; data[1]=100; data[2]=0; data[3]=2;
    for(i=0;i<64U;++i) { data[4+i*2]=0x20; data[5+i*2]=2; }
    data[132]=1; data[133]=1; data[134]=0; data[135]=0; s.bytes=136;
    CHECK(lt_stream_validate(&s) == LTA_INVALID);
    /* Repeated waits exceed expanded-step cap with bounded source storage. */
    for(i=0;i<256U;++i) { /* use 100 waits, repeat 255 times three times */
        if(i<100U) { data[i*3]=1; data[i*3+1]=255; data[i*3+2]=255; }
    }
    for(i=0;i<3U;++i) {
        unsigned at=300+i*6;
        data[at]=2; data[at+1]=0; data[at+2]=0;
        data[at+3]=44; data[at+4]=1; data[at+5]=255;
    }
    data[318]=0; s.bytes=319;
    CHECK(lt_stream_validate(&s) == LTA_INVALID);
    s.bytes=32768U; CHECK(lt_stream_validate(&s) == LTA_INVALID);
    s.bytes=65535U; CHECK(lt_stream_validate(&s) == LTA_INVALID);
}
static void progressing_music(void)
{
    lt_stream s=descriptor(packed,sizeof(packed));
    unsigned i;
    setup();
    CHECK(lt_stream_start(&music,&s,0)==LTA_OK);
    CHECK(lt_sfx_loop(&sfx,&clip,0)==LTA_OK);
    for(i=0;i<11U;++i) {
        CHECK(lt_stream_tick(&music)==LTA_OK);
        CHECK(lt_sfx_tick(&sfx)==LTA_OK);
    }
    CHECK(audio.period[0]==120 && chip.period[0]==80);
    CHECK(lt_sfx_stop(&sfx)==LTA_OK);
    CHECK(chip.period[0]==120 && chip.attenuation[0]==4);
    CHECK(lt_stream_stop(&music)==LTA_OK);
}
static void silent_pause(void)
{
    lt_stream s = descriptor(packed, sizeof(packed));
    unsigned i;
    lt_u16 pc, wait;
    setup();
    CHECK(lt_stream_start(&music,&s,0) == LTA_OK);
    CHECK(lt_stream_tick(&music) == LTA_OK);
    CHECK(lta_music_attenuation(&audio,0,3) == LTA_OK);
    pc=music.cursor.pc; wait=music.wait;
    CHECK(lt_stream_pause(&music,1) == LTA_OK);
    CHECK(chip.attenuation[0] == 15);
    for(i=0;i<5U;++i) CHECK(lt_stream_tick(&music) == LTA_OK);
    CHECK(music.cursor.pc == pc && music.wait == wait);
    CHECK(lt_stream_pause(&music,1) == LTA_OK);
    CHECK(lta_sfx_attenuation(&audio,2) == LTA_OK);
    CHECK(lt_sfx_loop(&sfx,&clip,0) == LTA_OK);
    CHECK(lt_sfx_tick(&sfx) == LTA_OK && chip.attenuation[0] == 5);
    CHECK(lt_stream_pause(&music,0) == LTA_OK && chip.attenuation[0] == 5);
    CHECK(command(LTA_OP_TONE_START,1,200,4) == LTA_OK);
    CHECK(lt_sfx_pause(&sfx,1) == LTA_OK);
    CHECK(chip.attenuation[0] == 15 && chip.attenuation[1] == 4);
    CHECK(lt_sfx_tick(&sfx) == LTA_OK && sfx.elapsed == 1);
    CHECK(lta_sfx_attenuation(&audio,5) == LTA_OK && chip.attenuation[0] == 15);
    CHECK(lta_layer_pause(&audio,0,1) == LTA_OK);
    CHECK(chip.attenuation[1] == 15);
    {
        lta_request r;
        lta_request_init(&r);r.op=LTA_OP_MUTE;r.enabled=1;
        CHECK(lta_command(&audio,&r) == LTA_OK);
        r.enabled=0;CHECK(lta_command(&audio,&r) == LTA_OK);
        CHECK(chip.attenuation[0] == 15 && chip.attenuation[1] == 15);
    }
    CHECK(lt_sfx_pause(&sfx,0) == LTA_OK && chip.attenuation[0] == 8);
    CHECK(lt_sfx_pause(&sfx,1) == LTA_OK);
    CHECK(lt_sfx_cancel(&sfx) == LTA_OK);
    CHECK(lt_sfx_tick(&sfx) == LTA_OK && sfx.active);
    CHECK(lt_sfx_pause(&sfx,0) == LTA_OK);
    for(i=0;i<3U;++i) CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(!sfx.active && chip.attenuation[0] == 15);
    CHECK(lta_layer_pause(&audio,0,0) == LTA_OK);
    CHECK(chip.attenuation[0] == 5 && chip.attenuation[1] == 4);
    CHECK(lt_stream_pause(&music,1) == LTA_OK);
    CHECK(lt_stream_stop(&music) == LTA_OK && !audio.music_paused);
    CHECK(chip.attenuation[0] == 15 && chip.attenuation[1] == 4);
    CHECK(lt_sfx_loop(&sfx,&clip,0) == LTA_OK);
    CHECK(lt_sfx_tick(&sfx) == LTA_OK);
    CHECK(lt_sfx_pause(&sfx,1) == LTA_OK);
    CHECK(lt_sfx_stop(&sfx) == LTA_OK && !audio.effect_paused);
    CHECK(chip.attenuation[0] == 15);
}
static void limits_and_priority(void)
{
    lt_u8 data[200];
    lt_stream d=descriptor(data,0);
    unsigned i;
    setup();
    data[0]=0x10;data[1]=100;data[2]=0;data[3]=2;
    for(i=0;i<62U;++i) {data[4+i*2]=0x20;data[5+i*2]=2;}
    data[128]=1;data[129]=1;data[130]=0;data[131]=0;d.bytes=132;
    CHECK(lt_stream_validate(&d)==LTA_OK); /* exactly 64 ops incl WAIT */
    CHECK(lt_stream_start(&music,&d,0)==LTA_OK);
    CHECK(lt_stream_tick(&music)==LTA_OK);
    CHECK(lt_stream_tick(&music)==LTA_OK && !music.active);
    /* Corrupt an immutable asset deliberately: hard tick budget still stops. */
    CHECK(lt_stream_start(&music,&d,0)==LTA_OK);
    data[128]=0x20;data[129]=2;data[130]=0x20;data[131]=2;
    data[132]=1;data[133]=1;data[134]=0;data[135]=0;d.bytes=136;
    CHECK(lt_stream_tick(&music)==LTA_INVALID && !music.active);
    memcpy(data,packed,sizeof(packed));
    data[20]=2;data[21]=14;data[22]=0;data[23]=6;data[24]=0;data[25]=1;data[26]=0;
    d.bytes=27;CHECK(lt_stream_validate(&d)==LTA_INVALID); /* nested REF */
    CHECK(lt_sfx_loop(&sfx,&clip,0)==LTA_OK);
    CHECK(lt_sfx_tick(&sfx)==LTA_OK);
    CHECK(lt_sfx_pause(&sfx,1)==LTA_OK);
    {
        lt_sfx_clip low=clip;
        low.priority=1;
        CHECK(lt_sfx_loop(&sfx,&low,1)==LTA_BUSY && sfx.paused);
        CHECK(lt_sfx_loop(&sfx,&clip,2)==LTA_OK && !sfx.paused);
        CHECK(!audio.effect_paused && sfx.plays_left==2);
    }
    CHECK(lt_sfx_stop(&sfx)==LTA_OK);
}
static void coupling(void)
{
    static const lt_u8 data[]={0x12,77,0,15,0x13,7,2,1,2,0,0};
    lt_stream s=descriptor(data,sizeof(data));
    lta_request r;
    setup(); s.channel_mask=12;
    CHECK(command(LTA_OP_TONE_START,2,222,15) == LTA_OK);
    lta_request_init(&r); r.op=LTA_OP_NOISE_START; r.mode=7; r.level=4;
    CHECK(lta_command(&audio,&r) == LTA_OK);
    CHECK(lt_stream_start(&effect,&s,1) == LTA_OK);
    CHECK(lt_stream_tick(&effect) == LTA_OK);
    CHECK(command(LTA_OP_TONE_UPDATE,2,444,15) == LTA_OK);
    CHECK(lta_music_attenuation(&audio,3,3) == LTA_OK);
    CHECK(lt_stream_tick(&effect) == LTA_OK);
    CHECK(lt_stream_tick(&effect) == LTA_OK);
    CHECK(chip.period[2] == 444 && chip.noise == 7 && chip.attenuation[3] == 7);
    CHECK(trace[count-2] == 0xE7 && trace[count-1] == 0xF7);
    s.data=data+4; s.bytes=7;
    CHECK(lt_stream_validate(&s) == LTA_INVALID);
}
int main(void)
{
    traces(); puts("PASS packed/plain trace"); fflush(stdout);
    generated_assets(); puts("PASS generated assets"); fflush(stdout);
    attenuation_and_loops(); puts("PASS attenuation/loops"); fflush(stdout);
    malformed(); puts("PASS malformed/bounds"); fflush(stdout);
    coupling(); progressing_music(); silent_pause(); limits_and_priority();
    printf("stream checks failures=%u; handle=%u clip-player=%u stream=%u player=%u cursor=%u\n",
           failures,(unsigned)sizeof(lta_handle),(unsigned)sizeof(lt_sfx_player),
           (unsigned)sizeof(lt_stream),(unsigned)sizeof(lt_stream_player),
           (unsigned)sizeof(lt_stream_cursor));
    return failures ? 1 : 0;
}
