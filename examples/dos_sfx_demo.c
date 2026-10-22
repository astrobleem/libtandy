/* SPDX-License-Identifier: GPL-3.0-only */
/* Minimal foreground demo for the separately licensed JAWS sample clips. */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include "lt_audio.h"
#include "lt_sfx.h"
#include "jaws/jaws_clips.h"

extern void lt_dos_emit(void *, lt_u8);

static lt_u16 psg_port = 0x00C0U;
static lt_chip chip;
static lta_handle audio;
static lt_sfx_player effects;
static FILE *demo_log;

enum {
    DEMO_FAILURE_NONE = 0,
    DEMO_FAILURE_RETRACE = 1,
    DEMO_FAILURE_AUDIO = 2
};

static unsigned demo_failure_kind;
static unsigned demo_failure_frame;
static unsigned demo_retrace_phase;
static lta_status demo_failure_status;

/* Write only at startup, stage changes, and failure/cleanup boundaries. The
 * frame loops below stay free of disk I/O so retrace timing is not disturbed. */
static void demo_log_text(const char *text)
{
    if (demo_log) {
        (void)fputs(text, demo_log);
        (void)fputs("\n", demo_log);
        (void)fflush(demo_log);
    }
}

static void demo_log_status(const char *name, lta_status status)
{
    if (demo_log) {
        (void)fprintf(demo_log, "%s=%u\n", name, (unsigned)status);
        (void)fflush(demo_log);
    }
}

static void demo_log_entry(int argc, char *argv[])
{
    int i;
    if (!demo_log) return;
    (void)fprintf(demo_log, "ENTRY argc=%d\n", argc);
    for (i = 0; i < argc; ++i)
        (void)fprintf(demo_log, "ARG%d=%s\n", i, argv[i]);
    (void)fflush(demo_log);
}

static void demo_log_exit(int exit_code)
{
    if (!demo_log) return;
    (void)fprintf(demo_log, "EXIT_CODE=%d\n", exit_code);
    (void)fflush(demo_log);
    (void)fclose(demo_log);
    demo_log = NULL;
}

/* A CGA/Tandy-compatible status port exposes bit 3 during vertical retrace.
 * This demo waits in foreground and never takes over an interrupt or timer. */
#define SFX_RETRACE_PORT 0x03DA
#define SFX_RETRACE_BIT 0x08U
#define SFX_RETRACE_POLL_LIMIT 65535U

static int wait_retrace(void)
{
    unsigned poll;

    /* Leave an in-progress retrace, then wait for the next leading edge. */
    for (poll = 0U; poll < SFX_RETRACE_POLL_LIMIT; ++poll) {
        if ((inp(SFX_RETRACE_PORT) & SFX_RETRACE_BIT) == 0) break;
    }
    if (poll == SFX_RETRACE_POLL_LIMIT) {
        demo_retrace_phase = 1U; /* Could not leave a retrace already in progress. */
        return 0;
    }

    for (poll = 0U; poll < SFX_RETRACE_POLL_LIMIT; ++poll) {
        if (inp(SFX_RETRACE_PORT) & SFX_RETRACE_BIT) {
            demo_retrace_phase = 0U;
            return 1;
        }
    }
    demo_retrace_phase = 2U; /* No leading retrace edge appeared before timeout. */
    return 0;
}

/* One player tick follows each display frame, including the final expiry
 * tick that restores the music shadow after the clip's last event frame. */
static int clip_frames(const lt_sfx_clip *clip)
{
    unsigned frame;
    lta_status status;
    demo_failure_kind = DEMO_FAILURE_NONE;
    for (frame = 0U; frame <= clip->duration_ticks; ++frame) {
        if (!wait_retrace()) {
            demo_failure_kind = DEMO_FAILURE_RETRACE;
            demo_failure_frame = frame;
            return 0;
        }
        status = lt_sfx_tick(&effects);
        if (status != LTA_OK) {
            demo_failure_kind = DEMO_FAILURE_AUDIO;
            demo_failure_frame = frame;
            demo_failure_status = status;
            return 0;
        }
    }
    return 1;
}

static int idle_frames(unsigned count)
{
    unsigned frame;
    lta_status status;
    demo_failure_kind = DEMO_FAILURE_NONE;
    for (frame = 0U; frame < count; ++frame) {
        if (!wait_retrace()) {
            demo_failure_kind = DEMO_FAILURE_RETRACE;
            demo_failure_frame = frame;
            return 0;
        }
        status = lt_sfx_tick(&effects);
        if (status != LTA_OK) {
            demo_failure_kind = DEMO_FAILURE_AUDIO;
            demo_failure_frame = frame;
            demo_failure_status = status;
            return 0;
        }
    }
    return 1;
}

static int report_tick_failure(const char *stage, int retrace_exit,
                               int audio_exit)
{
    if (demo_failure_kind == DEMO_FAILURE_RETRACE) {
        if (demo_log) {
            (void)fprintf(demo_log,
                          "FAIL stage=%s kind=RETRACE_TIMEOUT phase=%u frame=%u limit=%u\n",
                          stage, demo_retrace_phase, demo_failure_frame,
                          (unsigned)SFX_RETRACE_POLL_LIMIT);
            (void)fflush(demo_log);
        }
        return retrace_exit;
    }
    if (demo_log) {
        (void)fprintf(demo_log,
                      "FAIL stage=%s kind=AUDIO_TICK status=%u frame=%u\n",
                      stage, (unsigned)demo_failure_status, demo_failure_frame);
        (void)fflush(demo_log);
    }
    return audio_exit;
}

static int run_finite_demo(void)
{
    lta_status status;
    puts("/demo: harpoon, 18 silent frames, conch, then exit.");

    demo_log_text("STAGE=HARPOON_TRIGGER");
    status = lt_sfx_trigger(&effects, &lt_sfx_jaws_harpoon);
    demo_log_status("HARPOON_TRIGGER_STATUS", status);
    if (status != LTA_OK) {
        demo_log_text("FAIL=HARPOON_TRIGGER");
        return 20;
    }
    puts("Harpoon: 11 source ticks; first PSG writes on the next retrace.");
    demo_log_text("STAGE=HARPOON_TICKS");
    if (!clip_frames(&lt_sfx_jaws_harpoon))
        return report_tick_failure("HARPOON", 21, 22);
    demo_log_text("HARPOON_TICKS=COMPLETE");

    demo_log_text("STAGE=SILENT_GAP");
    if (!idle_frames(18U)) return report_tick_failure("GAP", 23, 24);
    demo_log_text("SILENT_GAP=COMPLETE");

    demo_log_text("STAGE=CONCH_TRIGGER");
    status = lt_sfx_trigger(&effects, &lt_sfx_jaws_conch);
    demo_log_status("CONCH_TRIGGER_STATUS", status);
    if (status != LTA_OK) {
        demo_log_text("FAIL=CONCH_TRIGGER");
        return 25;
    }
    puts("Conch: 65 source ticks; first PSG writes on the next retrace.");
    demo_log_text("STAGE=CONCH_TICKS");
    if (!clip_frames(&lt_sfx_jaws_conch))
        return report_tick_failure("CONCH", 26, 27);
    demo_log_text("CONCH_TICKS=COMPLETE");
    puts("Demo complete; stopping all four owned PSG channels.");
    return 0;
}

static lta_status audio_tone(lt_u8 channel, lt_u16 period, lt_u8 level)
{
    lta_request request;
    lta_request_init(&request);
    request.op = LTA_OP_TONE_START;
    request.channel = channel;
    request.period = period;
    request.level = level;
    return lta_command(&audio, &request);
}

int main(int argc, char *argv[])
{
    lta_request request;
    lta_status audio_status;
    int demo_mode, demo_result, quit;

    demo_mode = 0;
    if (argc == 2 && strcmp(argv[1], "/demo") == 0) demo_mode = 1;
    if (argc > 1) {
        demo_log = fopen("SFXDEMO.LOG", "w");
        if (!demo_log) {
            puts("Could not open SFXDEMO.LOG; command not run.");
            return 10;
        }
        demo_log_entry(argc, argv);
    }
    if (demo_mode) {
        demo_log_text("MODE=FINITE_DEMO");
    } else if (argc != 1) {
        demo_log_text("ARG_REJECTED=expected no arguments or one /demo argument");
        demo_log_exit(2);
        puts("Usage: SFXDEMO [/demo]");
        return 2;
    }

    lt_init(&chip, lt_dos_emit, &psg_port);
    if (demo_mode) demo_log_text("PSG_SINK=PORT_C0");
    audio_status = lta_init(&audio, &chip, 1U);
    demo_log_status("AUDIO_INIT_STATUS", audio_status);
    if (audio_status != LTA_OK) {
        demo_log_text("FAIL=AUDIO_INIT");
        demo_log_exit(11);
        return 11;
    }
    lta_request_init(&request);
    request.op = LTA_OP_ACQUIRE;
    request.mask = 15U;
    audio_status = lta_command(&audio, &request);
    demo_log_status("CHANNEL_ACQUIRE_MASK_0F_STATUS", audio_status);
    if (audio_status != LTA_OK) {
        demo_log_text("FAIL=CHANNEL_ACQUIRE");
        demo_log_exit(12);
        return 12;
    }
    audio_status = lt_sfx_init(&effects, &audio);
    demo_log_status("SFX_INIT_STATUS", audio_status);
    if (audio_status != LTA_OK) {
        demo_log_text("FAIL=SFX_INIT");
        lta_request_init(&request);
        request.op = LTA_OP_RELEASE;
        audio_status = lta_command(&audio, &request);
        demo_log_status("INIT_FAILURE_RELEASE_STATUS", audio_status);
        demo_log_exit(13);
        return 13;
    }

    if (demo_mode) {
        demo_result = run_finite_demo();
        if (demo_result != 0) puts("Demo failed; see SFXDEMO.LOG for its stage.");
        audio_status = lt_sfx_stop(&effects);
        demo_log_status("CLEANUP_SFX_STOP_STATUS", audio_status);
        if (audio_status != LTA_OK && demo_result == 0) demo_result = 30;
        lta_request_init(&request);
        request.op = LTA_OP_STOP;
        request.mask = 15U;
        audio_status = lta_command(&audio, &request);
        demo_log_status("CLEANUP_STOP_MASK_0F_STATUS", audio_status);
        if (audio_status != LTA_OK && demo_result == 0) demo_result = 31;
        lta_request_init(&request);
        request.op = LTA_OP_RELEASE;
        audio_status = lta_command(&audio, &request);
        demo_log_status("CLEANUP_RELEASE_STATUS", audio_status);
        if (audio_status != LTA_OK && demo_result == 0) demo_result = 32;
        demo_log_text(demo_result == 0 ? "RESULT=PASS" : "RESULT=FAIL");
        demo_log_exit(demo_result);
        return demo_result;
    }

    if (audio_tone(0U, 360U, 12U) != LTA_OK ||
        audio_tone(1U, 240U, 9U) != LTA_OK) {
        lta_request_init(&request);
        request.op = LTA_OP_RELEASE;
        (void)lta_command(&audio, &request);
        return 1;
    }
    quit = 0;
    puts("Tandy PSG clips: H harpoon, C conch pickup, Q quit.");
    puts("One clip tick per CGA/Tandy vertical retrace (~60 Hz).");
    puts("No timer/IRQ takeover; waits time out after 65535 port samples.");
    while (!quit) {
        if (!wait_retrace()) {
            puts("Video retrace was not detected; stopping the demo.");
            quit = 1;
            continue;
        }
        audio_status = lt_sfx_tick(&effects);
        if (audio_status != LTA_OK) quit = 1;
        if (kbhit()) {
            int key = getch();
            if (key == 'q' || key == 'Q') {
                quit = 1;
            } else if (key == 'h' || key == 'H') {
                audio_status = lt_sfx_trigger(&effects,
                                              &lt_sfx_jaws_harpoon);
                if (audio_status != LTA_OK) puts("Harpoon cue rejected.");
            } else if (key == 'c' || key == 'C') {
                audio_status = lt_sfx_trigger(&effects,
                                              &lt_sfx_jaws_conch);
                if (audio_status != LTA_OK) puts("Conch cue rejected.");
            }
        }
    }

    (void)lt_sfx_stop(&effects);
    lta_request_init(&request);
    request.op = LTA_OP_STOP;
    request.mask = 15U;
    (void)lta_command(&audio, &request);
    lta_request_init(&request);
    request.op = LTA_OP_RELEASE;
    (void)lta_command(&audio, &request);
    return 0;
}
