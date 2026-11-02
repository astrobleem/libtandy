/* SPDX-License-Identifier: MIT */
#include "lt_ay.h"
#include "lt_sfx.h"
#include <stdio.h>
#include <string.h>

static lt_chip chip;
static lta_handle audio;
static lt_ay_config config;
static lt_ay_frame frame;
static lt_ay_plan plan, saved;
static lt_sfx_player effect;
static lta_u32 checks, hash = 2166136261UL, writes;
static lta_u32 failures;
static unsigned frame_writes, maximum;
static lt_u16 decoded_period[3];
static lt_u8 decoded_attenuation[4], decoded_noise, latch;
#define CHECK(x) do { ++checks; if (!(x)) { \
    if (failures < 8) printf("FAIL ay_core.c:%u: %s\n", (unsigned)__LINE__, #x); \
    ++failures; } } while (0)

static void emit(void *context, lt_u8 value)
{
    unsigned channel;
    (void)context;
    ++writes; ++frame_writes;
    hash = (hash ^ value) * 16777619UL;
    if (value & 128) latch = (lt_u8)((value >> 4) & 7);
    channel = latch >> 1;
    if (latch & 1) decoded_attenuation[channel] = (lt_u8)(value & 15);
    else if (latch == 6) decoded_noise = (lt_u8)(value & 7);
    else if (value & 128)
        decoded_period[channel] = (lt_u16)((decoded_period[channel] & 1008) | (value & 15));
    else decoded_period[channel] = (lt_u16)((decoded_period[channel] & 15) | ((unsigned)(value & 63) << 4));
}
static lta_status command(lt_u8 op, lt_u8 channel, lt_u8 enabled, lta_u32 value)
{
    lta_request r;
    lta_request_init(&r); r.op = op; r.channel = channel; r.mask = 15;
    r.enabled = enabled; r.value = value;
    return lta_command(&audio, &r);
}
static void tones(unsigned a, unsigned b, unsigned c)
{
    memset(&frame, 0, sizeof frame);
    frame.reg[0] = (lt_u8)a; frame.reg[1] = (lt_u8)(a >> 8);
    frame.reg[2] = (lt_u8)b; frame.reg[3] = (lt_u8)(b >> 8);
    frame.reg[4] = (lt_u8)c; frame.reg[5] = (lt_u8)(c >> 8);
    frame.reg[7] = 56; frame.reg[8] = frame.reg[9] = frame.reg[10] = 15;
}
static void apply(void)
{
    unsigned i;
    frame_writes = 0;
    CHECK(lt_ay_apply(&audio, &plan) == LTA_OK);
    if (frame_writes > maximum) maximum = frame_writes;
    CHECK(frame_writes <= 13);
    for (i = 0; i < 3; ++i) {
        CHECK(plan.period[i] >= 1 && plan.period[i] <= 1023);
        CHECK(decoded_period[i] == chip.period[i]);
        CHECK(audio.period[i] == plan.period[i]);
    }
    for (i = 0; i < 4; ++i) {
        CHECK(plan.attenuation[i] <= 15);
        CHECK(decoded_attenuation[i] == chip.attenuation[i]);
        CHECK(audio.level[i] == plan.attenuation[i]);
    }
    CHECK(decoded_noise == chip.noise && audio.noise_mode == plan.noise_mode);
    if ((plan.noise_mode & 3) == 3) CHECK(plan.attenuation[2] == 15);
}
static void map(void)
{
    CHECK(lt_ay_map(&config, &frame, &plan) == LTA_OK);
    apply();
}
static void basics(void)
{
    lta_u32 before;
    unsigned i;
    lt_ay_config_init(&config);
    tones(100,200,300); map();
    CHECK(plan.period[0] == 100 && plan.period[2] == 300 && !plan.losses);
    apply(); CHECK(frame_writes == 0);
    config.scale_num = 3; config.scale_den = 2;
    tones(101,0,300); map(); CHECK(plan.period[0] == 152 && plan.period[1] == 2);
    lt_ay_config_init(&config); tones(4095,200,300);
    map(); CHECK(plan.period[0] == 512 && (plan.losses & LTAY_LOSS_RANGE));
    config.range_policy = LTAY_RANGE_CLAMP; map(); CHECK(plan.period[0] == 1023);
    config.range_policy = LTAY_RANGE_MUTE; map(); CHECK(plan.attenuation[0] == 15);
    lt_ay_config_init(&config); config.flags |= LTAY_PERIODIC_BASS;
    tones(100,200,3000); map();
    CHECK(plan.period[2] == 200 && plan.noise_mode == 3 && plan.attenuation[2] == 15);
    CHECK(plan.losses & LTAY_LOSS_PULSE_BASS);
    tones(100,200,300); frame.reg[7] = 52; frame.reg[6] = 9; map();
    CHECK(plan.noise_mode == 7 && plan.period[2] == 9 && plan.attenuation[2] == 15);
    tones(100,200,300); frame.reg[7] = 48; frame.reg[6] = 22; map(); CHECK(plan.noise_mode == 4);
    frame.reg[6] = 23; map(); CHECK(plan.noise_mode == 5);
    config.scale_num = 2; frame.reg[6] = 31; map(); CHECK(plan.noise_mode == 6);
    lt_ay_config_init(&config); tones(100,200,300); frame.reg[8] = 16;
    saved = plan; before = writes;
    CHECK(lt_ay_map(&config, &frame, &plan) == LTA_UNSUPPORTED);
    CHECK(memcmp(&saved, &plan, sizeof plan) == 0 && writes == before);
    frame.envelope_valid = 1; frame.envelope_level = 15;
    for (i = 0; i < 16; ++i) {
        frame.reg[13] = (lt_u8)i; map();
        CHECK(plan.attenuation[0] == 0 && (plan.losses & LTAY_LOSS_ENVELOPE));
    }
    frame.reg[7] |= 9; map();
    CHECK(plan.attenuation[0] == 15 && (plan.losses & LTAY_LOSS_ENVELOPE_ONLY));
    saved = plan; config.scale_den = 0;
    CHECK(lt_ay_map(&config, &frame, &plan) == LTA_INVALID);
    CHECK(memcmp(&saved, &plan, sizeof plan) == 0);
    lt_ay_config_init(&config); frame.envelope_valid = 2;
    CHECK(lt_ay_map(&config, &frame, &plan) == LTA_INVALID);
    frame.envelope_valid = 1; frame.envelope_level = 16;
    CHECK(lt_ay_map(&config, &frame, &plan) == LTA_INVALID);
    saved = plan; plan.period[0] = 1024; before = writes;
    CHECK(lt_ay_apply(&audio, &plan) == LTA_INVALID && writes == before); plan = saved;
    CHECK(command(LTA_OP_SERVICE,0,0,1) == LTA_OK);
    CHECK(command(LTA_OP_ARM_STOP,0,0,100) == LTA_OK); before = writes;
    CHECK(lt_ay_apply(&audio, &plan) == LTA_BUSY && writes == before);
    CHECK(command(LTA_OP_STOP,0,0,0) == LTA_OK);
    CHECK(lt_ay_apply(&audio, &plan) == LTA_OK);
    chip.owner[0] = 2; before = writes;
    CHECK(lt_ay_apply(&audio, &plan) == LTA_OWNER && writes == before);
    chip.owner[0] = 1;
}
static void exhaustive(void)
{
    unsigned period, policy, mixer, level, flags;
    lt_ay_config_init(&config);
    for (policy = 0; policy < 3; ++policy) {
        config.range_policy = (lt_u8)policy;
        for (period = 0; period < 4096; ++period) {
            tones(period,4095-period,1023); map();
        }
    }
    for (flags = 0; flags < 4; ++flags) {
        config.flags = (lt_u8)flags; config.range_policy = LTAY_RANGE_FOLD;
        for (mixer = 0; mixer < 64; ++mixer) for (level = 0; level < 32; ++level) {
            tones(100,2000,4095); frame.reg[7] = (lt_u8)mixer;
            frame.reg[6] = (lt_u8)level; frame.reg[8] = (lt_u8)level;
            frame.reg[9] = (lt_u8)(31-level); frame.reg[10] = (lt_u8)level;
            frame.envelope_valid = 1; frame.envelope_level = (lt_u8)(level & 15);
            map();
        }
    }
    config.scale_num = 65535; config.scale_den = 1;
    for (period = 0; period < 4096; ++period) { tones(period,4095-period,0); map(); }
    config.scale_num = 1; config.scale_den = 65535;
    tones(4095,1,0); map(); CHECK(plan.period[0] == 1 && plan.period[1] == 1);
}
static void rejected_inputs(void)
{
    lt_ay_config c;
    lt_ay_plan p;
    lta_handle h;
    lta_u32 before = writes;
    lt_ay_config_init(0);
    lt_ay_config_init(&c); tones(100,200,300);
    CHECK(lt_ay_map(&c,&frame,&p) == LTA_OK); saved = p;
    CHECK(lt_ay_map(0,&frame,&p) == LTA_INVALID);
    CHECK(lt_ay_map(&c,0,&p) == LTA_INVALID);
    CHECK(lt_ay_map(&c,&frame,0) == LTA_INVALID);
    c.scale_num = 0; CHECK(lt_ay_map(&c,&frame,&p) == LTA_INVALID);
    c.scale_num = 1; c.flags = 4; CHECK(lt_ay_map(&c,&frame,&p) == LTA_INVALID);
    c.flags = 0; c.range_policy = 3; CHECK(lt_ay_map(&c,&frame,&p) == LTA_INVALID);
    CHECK(memcmp(&p,&saved,sizeof p) == 0);
    CHECK(lt_ay_apply(0,&p) == LTA_INVALID);
    CHECK(lt_ay_apply(&audio,0) == LTA_INVALID);
    p.period[0] = 0; CHECK(lt_ay_apply(&audio,&p) == LTA_INVALID); p = saved;
    p.attenuation[3] = 16; CHECK(lt_ay_apply(&audio,&p) == LTA_INVALID); p = saved;
    p.noise_mode = 8; CHECK(lt_ay_apply(&audio,&p) == LTA_INVALID); p = saved;
    p.noise_mode = 3; CHECK(lt_ay_apply(&audio,&p) == LTA_INVALID); p = saved;
    h = audio; h.active = 0; CHECK(lt_ay_apply(&h,&p) == LTA_STATE);
    h = audio; h.mask = 7; CHECK(lt_ay_apply(&h,&p) == LTA_OWNER);
    h = audio; h.chip = 0; CHECK(lt_ay_apply(&h,&p) == LTA_INVALID);
    CHECK(writes == before);
}
static void layers(void)
{
    static const lt_sfx_event events[] = {{0,LTSFX_OP_TONE,2,188,1,2}};
    static const lt_sfx_clip clip = {LTSFX_CLIP_VERSION,4,1,1,4,0,6,60UL,1UL,events};
    lta_u32 before;
    unsigned i;
    lt_ay_config_init(&config); config.flags |= LTAY_PERIODIC_BASS;
    tones(100,200,3000); map();
    CHECK(lt_sfx_init(&effect, &audio) == LTA_OK);
    CHECK(lt_sfx_trigger(&effect, &clip) == LTA_OK);
    CHECK(lt_sfx_tick(&effect) == LTA_OK);
    CHECK(audio.effect_mask == 12 && chip.period[2] == 444);
    tones(110,210,3450); CHECK(lt_ay_map(&config, &frame, &plan) == LTA_OK);
    CHECK(lt_ay_apply(&audio, &plan) == LTA_OK);
    CHECK(chip.period[2] == 444 && audio.period[2] == 230);
    CHECK(lt_sfx_stop(&effect) == LTA_OK);
    CHECK(chip.period[2] == 230 && chip.attenuation[2] == 15 && chip.noise == 3);
    before = writes; CHECK(lt_ay_apply(&audio, &plan) == LTA_OK); CHECK(writes == before);
    CHECK(command(LTA_OP_MUTE,0,1,0) == LTA_OK);
    tones(150,250,400); CHECK(lt_ay_map(&config, &frame, &plan) == LTA_OK);
    CHECK(lt_ay_apply(&audio, &plan) == LTA_OK);
    for (i = 0; i < 4; ++i) CHECK(chip.attenuation[i] == 15);
    CHECK(command(LTA_OP_MUTE,0,0,0) == LTA_OK);
    CHECK(chip.period[0] == 150 && chip.attenuation[0] == 0);
    CHECK(lta_music_attenuation(&audio,0,4) == LTA_OK);
    tones(151,250,400); map(); CHECK(chip.attenuation[0] == 4);
    CHECK(lta_layer_pause(&audio,0,1) == LTA_OK);
    tones(152,251,401); map();
    for (i = 0; i < 4; ++i) CHECK(chip.attenuation[i] == 15);
    CHECK(lta_layer_pause(&audio,0,0) == LTA_OK);
    CHECK(chip.period[0] == 152 && chip.attenuation[0] == 4);
}
int main(void)
{
    lt_init(&chip, emit, 0); CHECK(lta_init(&audio, &chip, 1) == LTA_OK);
    CHECK(command(LTA_OP_ACQUIRE,0,0,0) == LTA_OK);
    basics(); exhaustive(); layers(); rejected_inputs();
    CHECK(command(LTA_OP_RELEASE,0,0,0) == LTA_OK);
    CHECK(hash == 0x85731ebbUL && writes == 48309UL && maximum == 12U);
    printf("AY %s checks=%lu writes=%lu hash=%08lx max=%u config=%u frame=%u plan=%u int=%u ptr=%u\n",
           failures ? "FAIL" : "PASS", (unsigned long)checks, (unsigned long)writes,
           (unsigned long)hash, maximum, (unsigned)sizeof config, (unsigned)sizeof frame,
           (unsigned)sizeof plan, (unsigned)sizeof(int), (unsigned)sizeof(void *));
    return failures != 0;
}
