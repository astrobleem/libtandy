# Optional AY control-frame adapter

`lt_ay.h` / `lt_ay.c` adapt an already decoded AY-3-8910-style control frame to
libtandy. This is useful to a register-based music/sound producer without
requiring a particular source machine, file format, tracker, or transport.
It is **not an AY emulator** or a promise of equivalent sound.

The adapter is optional. Link `lt_ay.c`, `lt_audio.c`, and `lt_chip.c`; add the
existing SFX module only if the application uses clips. It does not change
instrument banks, install a clock, perform I/O, allocate memory, parse files,
or contain any song assets. Call frequency belongs to the application.

## Two layers, no new lifecycle

1. `lt_ay_map(config, frame, plan)` is a pure, bounded transformation. It can
   be used without a live chip. Invalid or unsupported inputs leave the plan
   unchanged. Keep the three objects non-overlapping.
2. `lt_ay_apply(audio, plan)` applies that plan through an existing
   `lta_handle`. The handle must own all four channels and this must be its
   only base-music writer. Existing SFX overlays, master mute, per-channel
   attenuation and music-layer pause continue to work. Changes while an
   effect borrows channels update the base state restored when it ends.
   The plan must not overlap the handle. Attenuation controls address Tandy
   physical channels; resource remapping can change which AY voice uses one.

The adapter acquires/releases nothing. Use the existing `lta_init`, ACQUIRE,
MUTE, STOP, RELEASE and layer APIs. All calls sharing a chip remain serialized;
foreground work must be deferred/stopped around an interrupt-owned renderer.
Armed auto-stop deadlines conflict with frame-driven music and return BUSY.
Repeated identical plans cause no writes; level-only changes do not rewrite
noise control and therefore do not spuriously restart its generator.
Rejected mapping leaves the previous sound untouched; an application may
explicitly mute/stop on error rather than continuing its previous plan.

An `lt_dos_app` initially owns low-level chip claims. Hand them off explicitly
before acquiring an `lta_handle`: `lt_stop(&app.chip,
LT_DOS_APP_SOUND_OWNER, LT_DOS_APP_SOUND_MASK)`, then initialize/acquire the
handle. **`lta_acquire` rejects already-owned channels even with the same
token**, unlike the low-level `lt_claim` function.

```c
lt_ay_config config;
lt_ay_frame frame;
lt_ay_plan plan;
/* audio is already initialized and owns mask 15. */
lt_ay_config_init(&config);
/* Set period_scale = target PSG clock / (2 * source AY clock).
 * The default 1/1 is only correct for a matching clock relationship. */
config.scale_num = 1;
config.scale_den = 1;
/* Fill frame.reg[0..13], envelope_valid and envelope_level. */
if (lt_ay_map(&config, &frame, &plan) == LTA_OK) {
    /* Inspect plan.losses as appropriate for the application. */
    if (lt_ay_apply(&audio, &plan) != LTA_OK) { /* handle error */ }
}
```

Both ratio terms accept 1..65535. Supply a reduced rational or a suitable
approximation if exact clock integers do not fit. Conversion rounds to the
nearest nonzero period. Arithmetic uses at most 32 bits; the maximum product
is `4095 * 65535`. Unity scaling has a fast path. No actual clock detection,
rate conversion, resampling or schedule interpretation occurs.

## Deliberate approximation policies

- **Envelopes:** if any volume register enables its envelope, the caller must
  provide `envelope_valid=1` and the current resolved AY DAC level 0..15.
  Otherwise mapping returns `LTA_UNSUPPORTED`, with no output mutation.
  R11–R13 are not interpreted. All shapes therefore use the same explicit
  caller-level contract; there is no tune-specific shape-8/RMS shortcut.
  Sampling levels at 50/60 Hz cannot reproduce fast hardware-envelope timbre.
  A voice with both tone and noise disabled is silent in the plan, including
  envelope-only waveforms; the latter sets `LTAY_LOSS_ENVELOPE_ONLY`.
- **Levels:** fixed AY DAC levels are rounded to SN 2 dB attenuation steps.
  Quiet nonzero levels saturate at the SN floor; only AY level zero maps
  directly to silence. This is an AY DAC approximation, not a YM2149 profile.
- **Range:** select octave folding, clamping to 1023, or muting for periods
  outside the Tandy tone range. Folding is the default and preserves pitch
  class, not octave. The maximum input requires at most 19 halvings.
- **Noise:** optional coupled noise uses a silent tone 2 as its clock when
  no more than two tones are audible and the scaled noise divider fits.
  Otherwise the nearest logarithmic fixed divider 16/32/64 is selected.
  AY and Tandy noise feedback circuits differ even when rates match.
- **Mixing:** AY gates tone/noise per voice; Tandy has one additive noise
  output. A mixed voice is split into tone/noise at 6 dB attenuation each,
  and shared noise takes the loudest requested level. This changes timbre
  and relative levels; it is not a reconstruction of the AY gate waveform.
- **Pulse bass:** disabled by default. `LTAY_PERIODIC_BASS` assigns one low
  voice to Tandy NCR's 15-stage periodic noise when noise is otherwise unused.
  Tone 2 becomes a silent clock and the other voices use tone 0/1. The narrow
  pulse waveform, nearest-multiple-of-15 tuning, and up-to-6 dB compensation
  differ from AY square waves. Do not use this profile for an incompatible
  PSG variant with a different periodic-noise ring length.

`plan.losses` reports volume, envelope, mixer, noise, fixed-noise-rate, range,
pulse-bass and envelope-only compromises. These flags help diagnostics; a
zero mask is **not** a certification of analog or phase equivalence. Integer
period rounding and the chosen clock-ratio approximation also affect tuning.
Tone slots may change when a clock is reserved. Tone 2 is muted before
becoming a noise clock, and noise is muted before a mode transition.

Reference basis: [TI SN76489AN data sheet](https://map.grauw.nl/resources/sound/texas_instruments_sn76489an.pdf),
[AY-3-8910/8912 original manual](https://pub.intvprime.com/ba4ef/ie/Programming/AY-3-8910-8912-Programmable-Sound-Generator-Data-Manual.pdf),
[Ayumi's AY DAC reference values](https://github.com/true-grue/ayumi/blob/master/ayumi.c),
and the [DOSBox-X NCR/SN implementation](https://dosbox-x.com/doxygen/html/sn76496_8cpp_source.html).
The attenuation table is a small derived integer lookup, not a bundled
third-party synthesizer or recording.

## Verification

`tests/ay_core.c` uses synthetic register frames only. It covers all 4,096 AY
periods and all three range policies; mixer, envelope/volume, noise and policy
combinations; extreme clock ratios; unchanged output on rejected mapping;
ownership/deadline rejection; byte-stream round trips; repeat-write caching;
mute, attenuation, layer pause and tone2/noise SFX restoration.

Strict C89 host, ASan/UBSan and native Open Watcom 8088 runs agree on the
synthetic emitted-byte hash `85731ebb` (48,309 writes; maximum observed 12 per
apply). Configuration/frame/plan sizes are 6/16/14 bytes on the validated host
and 16-bit models. No persistent player state is added beyond the existing
handle. With Open Watcom `-0 -ml -ecc -ox -s -zt=8192`, the adapter adds
2,089 code bytes and a 16-byte constant lookup; existing audio/chip code is
excluded. Tests use no original demo assets and execute no foreign binaries.

These checks establish functional behavior, not real-hardware audibility,
chip-write spacing, analog level matching, full interrupt-stack high-water,
or a guaranteed frame-time budget. The native test's accelerated emulator
setting is not a calibrated 8088 performance measurement.
