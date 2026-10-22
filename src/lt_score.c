/* SPDX-License-Identifier: MIT */
#include "lt_score.h"
#include "lt_score_clock.h"

#define LTSCORE_U32_MAX 0xffffffffUL
#define LTSCORE_TIMER_DEFAULT 60U
#define LTSCORE_KICK_STAGE1 9178U
#define LTSCORE_KICK_STAGE2 11932U
#define LTSCORE_KICK_STAGE3 15297U
#define LTSCORE_MAX_EVENTS_PER_TICK 32U

static lta_status score_level(lt_score_player *player, lt_u8 channel,
                              lt_u8 level)
{
    lta_request request;
    lta_request_init(&request);
    request.op = LTA_OP_LEVEL;
    request.channel = channel;
    request.level = level;
    return lta_command(player->audio, &request);
}

static lta_status score_noise_mode(lt_score_player *player, lt_u8 mode)
{
    lta_request request;
    lta_request_init(&request);
    request.op = LTA_OP_NOISE_MODE;
    request.mode = mode;
    return lta_command(player->audio, &request);
}

static lta_status score_tone(lt_score_player *player, lt_u8 channel,
                             lt_u16 period, lt_u8 level)
{
    lta_request request;
    lta_request_init(&request);
    request.op = LTA_OP_TONE_START;
    request.channel = channel;
    request.period = period;
    request.level = level;
    return lta_command(player->audio, &request);
}

static lta_status validate_score(const lt_score *score)
{
    unsigned voice, i;
    if (!score || score->version != LTSCORE_VERSION ||
        score->ppq == 0U || score->ppq > 4096U || score->tempo_us == 0UL ||
        score->loop_start >= score->loop_end || score->loop_end > 32767U ||
        score->noise_mode > 7U || (score->noise_mode & 3U) == 3U ||
        score->drum_count > LTSCORE_MAX_DRUMS ||
        score->kick_count > LTSCORE_MAX_KICKS ||
        (score->drum_count && !score->drums) ||
        (score->kick_count && !score->kicks)) return LTA_INVALID;

    for (voice = 0U; voice < LTSCORE_VOICES; ++voice) {
        if (score->note_count[voice] > LTSCORE_MAX_NOTES ||
            (score->note_count[voice] && !score->notes[voice]) ||
            score->level[voice] > 15U) return LTA_INVALID;
        for (i = 0U; i < score->note_count[voice]; ++i) {
            const lt_score_note *note = &score->notes[voice][i];
            if (note->start >= note->end ||
                note->start >= score->loop_end ||
                note->end > score->loop_end ||
                note->period > LTA_MAX_PERIOD ||
                (i && note->start < score->notes[voice][i - 1U].start))
                return LTA_INVALID;
        }
    }
    for (i = 0U; i < score->drum_count; ++i) {
        const lt_score_drum *drum = &score->drums[i];
        if (drum->tick >= score->loop_end ||
            drum->kind < LTSCORE_DRUM_HAT ||
            drum->kind > LTSCORE_DRUM_ADDED_HAT ||
            (i && drum->tick < score->drums[i - 1U].tick))
            return LTA_INVALID;
    }
    for (i = 0U; i < score->kick_count; ++i) {
        if (score->kicks[i].tick >= score->loop_end ||
            (i && score->kicks[i].tick < score->kicks[i - 1U].tick))
            return LTA_INVALID;
    }
    return LTA_OK;
}

static lta_status score_clock(const lt_score *score, lta_u32 timer_rate,
                              lta_u32 *step)
{
    lta_u32 ticks_per_second, end_phase;
    if (!timer_rate) timer_rate = LTSCORE_TIMER_DEFAULT;
    if (!ltscore_ticks_per_second((lta_u32)score->ppq,
                                  score->tempo_us, &ticks_per_second))
        return LTA_INVALID;
    if (!ticks_per_second || ticks_per_second > 65535UL)
        return LTA_INVALID;
    *step = (ticks_per_second << 16) / timer_rate;
    end_phase = (lta_u32)score->loop_end << 16;
    if (!*step || *step > LTSCORE_U32_MAX - end_phase)
        return LTA_INVALID;
    return LTA_OK;
}

static void score_voice_init(lt_score_player *player, unsigned voice,
                             lt_u16 first_index)
{
    lt_score_voice *state = &player->voice[voice];
    state->index = first_index;
    state->gate_end = 0U;
    state->attack_end = 0U;
    state->decay_start = 0U;
    state->level = player->score->level[voice];
    state->playing = 0U;
    state->attack_pending = 0U;
    state->decay_pending = 0U;
}

static lt_u16 find_first_note(const lt_score *score, unsigned voice,
                              lt_u16 first_tick)
{
    lt_u16 index = 0U;
    while (index < score->note_count[voice] &&
           score->notes[voice][index].start < first_tick) ++index;
    return index;
}

static lt_u16 find_first_drum(const lt_score *score, lt_u16 first_tick)
{
    lt_u16 index = 0U;
    while (index < score->drum_count &&
           score->drums[index].tick < first_tick) ++index;
    return index;
}

static lt_u16 find_first_kick(const lt_score *score, lt_u16 first_tick)
{
    lt_u16 index = 0U;
    while (index < score->kick_count &&
           score->kicks[index].tick < first_tick) ++index;
    return index;
}

static lta_status score_voice_tick(lt_score_player *player, unsigned voice,
                                   lt_u16 now)
{
    const lt_score *score = player->score;
    lt_score_voice *state = &player->voice[voice];
    lta_status status;

    if (state->playing) {
        if (now >= state->gate_end) {
            status = score_level(player, (lt_u8)voice, 15U);
            if (status != LTA_OK) return status;
            state->playing = 0U;
        } else if (state->decay_pending && now >= state->decay_start) {
            unsigned level = (unsigned)state->level + 3U;
            if (level > 15U) level = 15U;
            status = score_level(player, (lt_u8)voice, (lt_u8)level);
            if (status != LTA_OK) return status;
            state->decay_pending = 0U;
        } else if (state->attack_pending && now >= state->attack_end) {
            status = score_level(player, (lt_u8)voice, state->level);
            if (status != LTA_OK) return status;
            state->attack_pending = 0U;
        }
    }

    {
        unsigned done = 0U;
        while (state->index < score->note_count[voice] &&
               score->notes[voice][state->index].start <= now &&
               done < LTSCORE_MAX_EVENTS_PER_TICK) {
            const lt_score_note *note = &score->notes[voice][state->index];
            if (note->end > now) {
                lt_u16 duration = (lt_u16)(note->end - note->start);
                lt_u16 gate_end = (lt_u16)(note->end -
                    ((duration > (lt_u16)(score->release_gap[voice] + 2U)) ?
                     score->release_gap[voice] : 1U));
                lt_u8 attack_level;
                if (gate_end <= now) gate_end = note->end;
                attack_level = (lt_u8)(state->level + 2U > 15U ?
                                       15U : state->level + 2U);
                status = score_tone(player, (lt_u8)voice, note->period,
                                    attack_level);
                if (status != LTA_OK) return status;
                state->gate_end = gate_end;
                state->attack_end = (lt_u16)(now + 8U);
                state->decay_start = (lt_u16)(gate_end > 16U ?
                                              gate_end - 16U : gate_end);
                state->attack_pending = 1U;
                state->decay_pending = 1U;
                state->playing = 1U;
            }
            ++state->index;
            ++done;
        }
    }
    return LTA_OK;
}

static unsigned drum_priority(lt_u8 kind)
{
    if (kind == LTSCORE_DRUM_CRASH) return 3U;
    if (kind == LTSCORE_DRUM_SNARE) return 2U;
    return 1U;
}

static lta_status drum_start(lt_score_player *player, lt_u8 kind,
                             lt_u16 now)
{
    unsigned priority = drum_priority(kind);
    lt_u16 duration;
    lta_status status;
    if (player->drum_playing &&
        priority < drum_priority(player->drum_kind)) return LTA_OK;
    player->drum_kind = kind;
    player->drum_playing = 1U;
    if (kind == LTSCORE_DRUM_CRASH) duration = 192U;
    else if (kind == LTSCORE_DRUM_SNARE) duration = 72U;
    else duration = 24U;
    player->drum_end = (lt_u16)(now + duration);
    if (kind == LTSCORE_DRUM_CRASH) status = score_level(player, 3U, 7U);
    else if (kind == LTSCORE_DRUM_SNARE) status = score_level(player, 3U, 6U);
    else status = score_level(player, 3U, 10U);
    return status;
}

static lta_status drum_tick(lt_score_player *player, lt_u16 now)
{
    const lt_score *score = player->score;
    lta_status status;
    if (player->drum_playing) {
        if (now >= player->drum_end) {
            status = score_level(player, 3U, 15U);
            if (status != LTA_OK) return status;
            player->drum_playing = 0U;
        } else {
            lt_u16 elapsed = (lt_u16)(player->drum_end - now);
            if (player->drum_kind == LTSCORE_DRUM_CRASH) {
                if (elapsed <= 48U) status = score_level(player, 3U, 14U);
                else if (elapsed <= 96U) status = score_level(player, 3U, 12U);
                else if (elapsed <= 144U) status = score_level(player, 3U, 10U);
                else if (elapsed <= 180U) status = score_level(player, 3U, 9U);
                else status = LTA_OK;
            } else if (player->drum_kind == LTSCORE_DRUM_SNARE &&
                       elapsed <= 24U) {
                status = score_level(player, 3U, 11U);
            } else if ((player->drum_kind == LTSCORE_DRUM_HAT ||
                        player->drum_kind == LTSCORE_DRUM_ADDED_HAT) &&
                       elapsed <= 12U) {
                status = score_level(player, 3U, 13U);
            } else status = LTA_OK;
            if (status != LTA_OK) return status;
        }
    }
    {
        unsigned done = 0U;
        while (player->drum_index < score->drum_count &&
               score->drums[player->drum_index].tick <= now &&
               done < LTSCORE_MAX_EVENTS_PER_TICK) {
            const lt_score_drum *event = &score->drums[player->drum_index];
            if ((unsigned long)event->tick + 24UL >= now) {
                status = drum_start(player, event->kind, now);
                if (status != LTA_OK) return status;
            }
            ++player->drum_index;
            ++done;
        }
    }
    return LTA_OK;
}

static void speaker_write(lt_score_player *player, lt_u16 divisor,
                          lt_u8 gate)
{
    if (player->speaker) player->speaker(player->speaker_context, divisor, gate);
}

static void speaker_kick(lt_score_player *player)
{
    speaker_write(player, LTSCORE_KICK_STAGE1, LTSCORE_SPEAKER_GATE_ON);
    player->speaker_stage = 1U;
}

static lta_status kick_tick(lt_score_player *player, lt_u16 now)
{
    const lt_score *score = player->score;
    unsigned done = 0U;
    if (player->speaker_stage == 1U) {
        speaker_write(player, LTSCORE_KICK_STAGE2,
                      LTSCORE_SPEAKER_KEEP_GATE);
        player->speaker_stage = 2U;
    } else if (player->speaker_stage == 2U) {
        speaker_write(player, LTSCORE_KICK_STAGE3,
                      LTSCORE_SPEAKER_KEEP_GATE);
        player->speaker_stage = 3U;
    } else if (player->speaker_stage == 3U) {
        speaker_write(player, 0U, LTSCORE_SPEAKER_GATE_OFF);
        player->speaker_stage = 0U;
    }
    while (player->kick_index < score->kick_count &&
           score->kicks[player->kick_index].tick <= now &&
           done < LTSCORE_MAX_EVENTS_PER_TICK) {
        if ((unsigned long)score->kicks[player->kick_index].tick + 24UL >= now)
            speaker_kick(player);
        ++player->kick_index;
        ++done;
    }
    return LTA_OK;
}

static lta_status score_mute_all(lt_score_player *player)
{
    unsigned channel;
    lta_status status;
    for (channel = 0U; channel < 4U; ++channel) {
        status = score_level(player, (lt_u8)channel, 15U);
        if (status != LTA_OK) return status;
    }
    return LTA_OK;
}

static lta_status reset_main(lt_score_player *player)
{
    unsigned voice;
    lta_status status;
    for (voice = 0U; voice < LTSCORE_VOICES; ++voice)
        score_voice_init(player, voice,
                         find_first_note(player->score, voice,
                                         player->score->loop_start));
    player->drum_index = find_first_drum(player->score,
                                        player->score->loop_start);
    player->kick_index = find_first_kick(player->score,
                                        player->score->loop_start);
    player->drum_playing = 0U;
    player->speaker_stage = 0U;
    status = score_mute_all(player);
    if (status != LTA_OK) return status;
    speaker_write(player, 0U, LTSCORE_SPEAKER_GATE_OFF);
    return LTA_OK;
}

lta_status lt_score_init(lt_score_player *player, lta_handle *audio)
{
    lt_score_player next;
    if (!player || !audio) return LTA_INVALID;
    next.audio = audio;
    next.score = 0;
    next.speaker = 0;
    next.speaker_context = 0;
    next.phase = 0UL;
    next.step = 0UL;
    next.drum_end = 0U;
    next.drum_index = 0U;
    next.kick_index = 0U;
    next.active = 0U;
    next.drum_kind = 0U;
    next.drum_playing = 0U;
    next.speaker_stage = 0U;
    {
        unsigned voice;
        for (voice = 0U; voice < LTSCORE_VOICES; ++voice) {
            next.voice[voice].index = 0U;
            next.voice[voice].gate_end = 0U;
            next.voice[voice].attack_end = 0U;
            next.voice[voice].decay_start = 0U;
            next.voice[voice].level = 15U;
            next.voice[voice].playing = 0U;
            next.voice[voice].attack_pending = 0U;
            next.voice[voice].decay_pending = 0U;
        }
    }
    *player = next;
    return LTA_OK;
}

lta_status lt_score_start(lt_score_player *player, const lt_score *score,
                          lta_u32 timer_rate,
                          lt_score_speaker_sink speaker, void *context)
{
    lta_status status;
    lta_u32 step;
    unsigned voice;
    if (!player || !player->audio || player->active > 1U)
        return LTA_INVALID;
    status = validate_score(score);
    if (status != LTA_OK) return status;
    if (!player->audio->active ||
        (player->audio->mask & 15U) != 15U) return LTA_OWNER;
    status = score_clock(score, timer_rate, &step);
    if (status != LTA_OK) return status;
    if (player->active) {
        status = lt_score_stop(player);
        if (status != LTA_OK) return status;
    }

    player->score = score;
    player->speaker = speaker;
    player->speaker_context = context;
    player->phase = 0UL;
    player->step = step;
    player->drum_end = 0U;
    player->drum_index = 0U;
    player->kick_index = 0U;
    player->drum_kind = 0U;
    player->drum_playing = 0U;
    player->speaker_stage = 0U;
    for (voice = 0U; voice < LTSCORE_VOICES; ++voice) {
        score_voice_init(player, voice, 0U);
    }

    status = score_mute_all(player);
    if (status != LTA_OK) return status;
    status = score_noise_mode(player, score->noise_mode);
    if (status != LTA_OK) return status;
    player->active = 1U;
    for (voice = 0U; voice < LTSCORE_VOICES; ++voice) {
        status = score_voice_tick(player, voice, 0U);
        if (status != LTA_OK) return status;
    }
    status = drum_tick(player, 0U);
    if (status != LTA_OK) return status;
    return kick_tick(player, 0U);
}

lta_status lt_score_tick(lt_score_player *player)
{
    lta_status status;
    lta_u32 end_phase, loop_phase, span_phase, over;
    lt_u16 now;
    unsigned voice;
    if (!player || !player->audio || player->active > 1U)
        return LTA_INVALID;
    if (!player->active) return LTA_OK;
    if (!player->score || !player->audio->active) return LTA_STATE;

    end_phase = (lta_u32)player->score->loop_end << 16;
    player->phase += player->step;
    if (player->phase >= end_phase) {
        over = player->phase - end_phase;
        loop_phase = (lta_u32)player->score->loop_start << 16;
        span_phase = (lta_u32)(player->score->loop_end -
                               player->score->loop_start) << 16;
        player->phase = loop_phase + over % span_phase;
        status = reset_main(player);
        if (status != LTA_OK) return status;
    }
    now = (lt_u16)(player->phase >> 16);
    for (voice = 0U; voice < LTSCORE_VOICES; ++voice) {
        status = score_voice_tick(player, voice, now);
        if (status != LTA_OK) return status;
    }
    status = drum_tick(player, now);
    if (status != LTA_OK) return status;
    return kick_tick(player, now);
}

lta_status lt_score_stop(lt_score_player *player)
{
    lta_status status;
    if (!player || !player->audio || player->active > 1U)
        return LTA_INVALID;
    if (!player->active) return LTA_OK;
    if (!player->audio->active) return LTA_STATE;
    status = score_mute_all(player);
    if (status != LTA_OK) return status;
    speaker_write(player, 0U, LTSCORE_SPEAKER_GATE_OFF);
    player->active = 0U;
    player->score = 0;
    player->speaker = 0;
    player->speaker_context = 0;
    player->phase = 0UL;
    player->step = 0UL;
    player->drum_playing = 0U;
    player->speaker_stage = 0U;
    return LTA_OK;
}
