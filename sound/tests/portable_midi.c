/* SPDX-License-Identifier: MIT; portable checks for the frozen MIDI core. */
#include <stdio.h>
#include <string.h>
#include "SMF.H"
#include "MIDISYN.H"
#include "MIDIMSG.H"

static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { \
    printf("FAIL %u: %s\n", (unsigned)__LINE__, #x); ++failures; } } while (0)

static LTP_U32 message(unsigned status, unsigned a, unsigned b)
{
    return (LTP_U32)status | ((LTP_U32)a << 8) | ((LTP_U32)b << 16);
}

int main(void)
{
    /* One bounded format-0 track: low and highest melodic notes, two releases,
       then EOT. Division 120; one tick at default tempo floors to 4 ms. */
    static const unsigned char song[] = {
        'M','T','h','d',0,0,0,6,0,0,0,1,0,120,
        'M','T','r','k',0,0,0,20,
        0,0x90,14,100, 1,0x90,127,100,
        0,0x80,14,0, 0,0x80,127,0, 0,0xff,0x2f,0
    };
    unsigned char damaged[sizeof(song)];
    SMF_EVENT e, first = {0};
    MSYN synth;
    unsigned i, count;

    CHECK(sizeof(LTP_U8) == 1 && sizeof(LTP_U16) == 2 && sizeof(LTP_U32) == 4);
    CHECK(MSValidMessage(message(0x90,44,100),0) == 0);
    CHECK(MSValidMessage(message(0x90,44,100),1) == 1);
    CHECK(MSValidMessage(message(0x90,45,100),0) == 1);
    CHECK(MSValidMessage(message(0x90,127,100),0) == 1);
    CHECK(MSValidMessage(message(0x90,128,100),1) == 0);
    CHECK(MSValidMessage(message(0x99,34,100),1) == 0);
    CHECK(MSValidMessage(message(0xb0,0,1),1) == 0); /* no other bank */
    CHECK(MSValidMessage(message(0xb0,0,0),1) == 1);

    MSReset(&synth);
    synth.flags = 0;
    CHECK(MSMessage(&synth,0,message(0x90,14,100)) == 1);
    MSSpeakerMode(&synth,1);
    CHECK(MSMessage(&synth,0,message(0x90,14,100)) == 0);
    CHECK(synth.speakerDivisor == 65009);
    CHECK(MSMessage(&synth,0,message(0x80,14,0)) == 0);
    CHECK(synth.speakerDivisor == 0);
    CHECK(MSMessage(&synth,0,message(0x90,0,100)) == 0);
    CHECK(synth.speakerDivisor != 0 && synth.speakerNote == 1);
    CHECK(MSMessage(&synth,0,message(0x80,0,0)) == 0);
    CHECK(MSMessage(&synth,0,message(0x90,127,100)) == 0);
    CHECK(synth.speakerDivisor == 0 && synth.note[0] == 127);
    CHECK(synth.p[0] == 8); /* quantized high PSG divider */

    CHECK(smf_open(song,sizeof(song)) == 1);
    CHECK(smf_track_count() == 1 && smf_division() == 120);
    CHECK(smf_event_count() == 4 && smf_duration_ms() == 4);
    count = 0;
    while (smf_next(&e) == 1) {
        if (count == 0) first = e;
        CHECK(e.ms == (count == 0 ? 0 : 4));
        ++count;
    }
    CHECK(count == 4);
    CHECK(first.status == 0x90 && first.data1 == 14 && first.data2 == 100);
    CHECK(smf_rewind() == 1);
    CHECK(smf_next(&e) == 1 && e.status == first.status &&
          e.data1 == first.data1 && e.ms == first.ms);
    smf_close();
    for (i = 0; i < sizeof(song); ++i)
        CHECK(smf_open(song,i) == 0);
    memcpy(damaged,song,sizeof(song));
    damaged[21] = 21; /* declared track exceeds available bytes */
    CHECK(smf_open(damaged,sizeof(damaged)) == 0);
    memcpy(damaged,song,sizeof(song));
    damaged[23] = 0xf0; /* truncated SysEx length */
    CHECK(smf_open(damaged,sizeof(damaged)) == 0);
    CHECK(smf_open(song,60001U) == 0);
    printf("portable MIDI: %u checks, %u failures\n",checks,failures);
    return failures != 0;
}
