# Static Tandy score player

`lt_score` serves long, fixed Tandy PSG arrangements that need their own note
tables. It does not parse or store MIDI files. The application keeps sorted
static arrays and describes them with `lt_score`; the player supplies the
three tone voices, envelopes, fixed-noise percussion, tempo accumulator, loop
and optional kick stages.

```c
lt_score_init(&music, &audio);
lt_score_start(&music, &score, timer_rate, speaker_sink, speaker_context);
/* Call from the application's existing timer/game callback. */
lt_score_tick(&music);
lt_score_stop(&music);
```

A Nyan-style descriptor can stay entirely in the app:

```c
static const lt_score score = {
    LTSCORE_VERSION, 4U, 192U, 1536U, 13824U, 422535UL,
    { melody, bass, harmony }, { MELODY_COUNT, BASS_COUNT, HARMONY_COUNT },
    { 2U, 7U, 10U }, { 6U, 10U, 8U },
    drums, DRUM_COUNT, kicks, KICK_COUNT
};
```

`timer_rate` is `lta_u32`, matching the source adapter's 32-bit
`TandyMusicStart(unsigned long timerRate)` on DOS.

The player requires one `lta_handle` that already owns PSG channels 0 through
3. `timer_rate` is the caller's tick cadence; zero selects 60 Hz. Tempo is
expressed as integer microseconds per quarter note plus PPQ. Each call advances
the same 16.16 phase used by the extracted Nyan scheduler. It starts at score
tick zero, plays the intro once, then repeats `[loop_start, loop_end)`. Call
`lt_score_tick` at the same cadence passed to `lt_score_start`; it does not
install a timer or IRQ.

The descriptor limits each tone table to 2,048 notes and percussion/kick
tables to 4,096 events. Events are sorted by start tick, valid through the
loop end and caller-owned for the player's lifetime. Up to 32 due notes per
voice and 32 due events in each percussion table are applied per timer call.
This keeps foreground/IRQ work bounded; score authors must keep event density
within that budget at the selected timer rate.

Tempo conversion rounds `PPQ * 1,000,000 / tempo_us` to the nearest whole
score tick per second using quotient and remainder. It does not add half the
tempo to the numerator, which would overflow on 32-bit DOS for some otherwise
valid descriptors.

Tone envelope changes use `LTA_OP_LEVEL`, which emits only the volume latch and
updates the base music shadow if an effect currently borrows that voice. The
fixed-noise setup uses `LTA_OP_NOISE_MODE`, which emits only the noise control
byte. These operations preserve the Nyan trace shape, unlike a tone update
that would rewrite a period on every envelope step. Tone-2-coupled noise
modes are rejected by the score descriptor; short clips continue to use the
`lt_sfx` path, which expands ownership and restores the coupled pair safely.

The optional speaker callback receives `(divisor, gate_command)` requests for
the 130, 100 and 78 Hz kick stages. `KEEP_GATE` changes only PIT2's divisor;
`GATE_ON` changes the divisor then opens the speaker gate; `GATE_OFF` closes it.
The callback is a transport hook, not an owner: on every callback, including
the gate-off request made during stop or reset, the caller must check its
existing cooperative speaker lease and ignore the request if that lease is
not held. `lt_score` does not probe port 61, claim PIT2, or promise restoration
of arbitrary prior PIT2 state.

## NyanCat source provenance

The read-only source used to shape the scheduler is the accepted local
NyanCat `SRC/TANDY.C` (SHA-256
`3AF27D4D211C1D69A2FDDF83012EDF53E0F530AE2974E0E200CE2AA12AF32EA8`) and
`SRC/TANDY.H` (`90FEC106D0282061908E5AAE79F1D99956303F5028A1378A77215F590289CB7C`).
The header exposes `TandyMusicStart(timerRate)`, `TandyMusicTick()` and
`TandyMusicStop()`; Nyan calls these from its existing timer path.

The app's static tables remain app data. Their source comments credit Tyler's
Nyan Cat MIDI entry at VGMusic (2019-07-25). The supplied source MIDI has
SHA-256 `DF8624CF4E9DA63A40FE18616CB63F04FD354E27FA80E3A655C5875FDC0F40B9`;
the accepted score audit has SHA-256
`EC27376A42E2D21CFDDE5372B45211B4625498DD718FFDBF7BA821D331F4AC0A`.
The accepted `MUSIC-FINAL.LOG` is 123 bytes, SHA-256
`287A6B084AD68FFEA0BD3CE61CFECE99D563351EB41E3AF9F6994E00548C20AC`; it
records `loop_count=1`, `speaker_acquired=1`, and the accepted melody, bass,
harmony, kick and percussion counts.
The score uses 192 PPQ and 422,535 microseconds per quarter. The scheduler
rounds `192,000,000 / 422,535` to 454 ticks per second, then advances
`(454 << 16) / timerRate` each timer call. At 60 Hz this is 495,889 phase units.
The one-time intro is 1,536 ticks; the repeating section is
`[1,536, 13,824)`, a 12,288-tick A/B pair.

The source routines are `TandyMusicVolume` and `TandyMusicTone` (lines
558-572), `TandyVoiceTick` (623-673), speaker ownership and kick stages
(`TandySpeakerAcquire`, `TandySpeakerKick`, `TandySpeakerTick`, lines 697-786),
noise percussion priority/envelopes (`TandyDrumStart`/`TandyDrumTick`,
797-860), and score lifecycle (`TandyMusicStart`/`TandyMusicTick`/
`TandyMusicStop`, 887-958). The source tables contain 238 melody notes,
72 bass notes, 32 harmony notes, 107 noise events and 36 speaker-kick ticks.

`lt_score` contains the reusable scheduling and write behavior only. It does
not contain Nyan's note/percussion arrays, MIDI file, graphics, application
adapter or PIT ownership logic. No Nyan files were edited for this extraction.

## Public vendored subset check

The accepted showcase is `astrobleem/NyanCat-Tandy`, main commit
`2445730356dd900295640648f28aeef929bdfe7f`. Its
`third_party/libtandy/` subset contains `lt_audio`, `lt_chip`, `lt_score`, the
score clock helper, `lt_sound.h`, and the DOS audio adapter. The corresponding
code/header/adapter files in this candidate have the same Git blob IDs as that
commit's tree. The score documentation had the same source text, then this
candidate appended the public-subset check above; that documentation-only
difference records the comparison. There is no drift in the shared score,
chip, or audio implementation.

That vendored subset does not include `lt_sfx.h`, `lt_sfx.c`, the clip file
serializer, JAWS cue tables, or their tests. Those are the additional native
effect work in this candidate. The standalone `sound/` MIDI parser and
renderer remain separate and are not copied into Nyan's subset.
