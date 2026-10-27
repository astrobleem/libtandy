# Compact native PSG audio

The optional `lt_stream.c` player adds immutable compiled logical register
streams with backward phrase references. It borrows the existing audio
ownership/shadow/overlay path; it does not parse PSGlib files or add another
synthesizer. Existing scores, clip files and approved songs are unchanged.
The ideas are informed by [PSGlib's transport API](https://github.com/sverx/devkitSMS/blob/master/PSGlib/src/PSGlib.h)
and [phrase-reference format](https://github.com/sverx/PSGlib/blob/master/documents/PSG%20file%20format.txt).
Implementation is original C89, with no assembly copied from PSGlib.

## Attenuation

`lta_music_attenuation(audio, channel, steps)` independently adds 0..15 PSG
attenuation steps on each music channel. `lta_sfx_attenuation(audio, steps)`
does the same for the effect layer. Both saturate at 15 (silence), immediately
update the audible layer, and preserve original levels for later restoration.
Music changes beneath a borrowed channel only update its shadow. Effects
restore the latest music period, mode and adjusted level when stopped.
Init/acquire resets controls to zero; old code emits the same default traces.
The handle/player layouts grow: rebuild callers and modules together.
`LTA_API_VERSION` is now `0x0202`; clip descriptors and clip wire formats retain
their existing versions.
These are PSG-only controls: PC speaker callbacks retain fixed physical
amplitude. They do not implement speaker gain or boost.

## Looping clips and silent pause

`lt_sfx_loop(player, clip, total_plays)` accepts 1 for once, 2..65535 for a
finite total number of plays, and 0 for infinite. The ordinary trigger remains
one-shot. The next pass starts on the boundary tick, with no extra blank tick;
it first silences/reset its reserved effect voices, then applies tick zero.
Music keeps progressing below repeated effects. Priority replacement still
uses newest-wins for equal priority and rejects lower priority.

`lt_sfx_cancel` finishes the current pass. `lt_sfx_stop` immediately restores
music and discards the cursor. `lt_sfx_pause(player, 1)` silences the effect
and freezes event position, elapsed time and loop counter. It holds the
reservation, including the expanded tone2/noise pair, until resume or stop.
Music updates beneath it remain hidden. Resume (`paused=0`) restores its exact
current adjusted volumes; periods/noise state have remained reserved. A
boundary cancel while paused waits for resume; immediate stop also works
while paused. Resume after stop returns `LTA_STATE` and never restarts it.

`lt_stream_pause` applies the same silent freeze to a stream cursor, reference
counter and wait. Music pause silences the music layer on that handle, while
an overlay continues. Effect pause leaves unborrowed music audible. Neither
changes channel ownership. Disjoint handles are unaffected. At most one base
transport and one overlay transport may drive a handle. Global mute and
attenuation changes respect both pause flags. `lta_layer_pause` provides the
same volume policy for callers with another transport; those callers must
freeze their own event clocks. For static scores, pair this layer control with withholding score ticks;
this addition does not change the score speaker callback's existing policy.

## Stream descriptor and commands

Compile `lt_chip.c`, `lt_audio.c`, `lt_stream.c` and their headers. Streams
may update music under a clip overlay without requiring `lt_sfx.c` themselves.
The caller owns the player, descriptor and near-addressed bytes and keeps the
descriptor/data immutable through stop/end. Version 1 requires explicit byte
length (1..32767), chip `LTST_TANDY`, nonzero PSG clock Hz and rational tick
rate, a channel mask and priority. Metadata does not schedule, resample or
retune output: service at that cadence using the application's own clock.
Periods are already compiled for that clock. Sega's noise generator differs
from Tandy's; format support does not reproduce SMS percussion exactly.

All multi-byte operands below are little endian. No command is a raw PSG byte.

| Opcode | Operands | Meaning |
| --- | --- | --- |
| `00` | none | End; required as the last source byte |
| `01` | ticks:u16 | Wait 1..65535 caller ticks |
| `02` | offset:u16, bytes:u16, count:u8 | Play an earlier complete literal block 1..255 more times |
| `10..12` | period:u16, level:u8 | Complete tone operation, channel 0..2 |
| `13` | mode:u8, level:u8 | Complete noise operation |
| `20..23` | level:u8 | Level of an initialized voice, channel 0..3 |
| `30` | mask:u8 | Stop declared channels |

References must point backwards to earlier command boundaries, stay entirely
before the reference, and contain no references or END. Reference depth is
one by design. The block can contain waits. It is interpreted incrementally,
with one small return frame and no full-song decompression. There are no
callback/event hooks. Noise rate 3 requires tone 2 initialized first in the
stream and both channels declared. Tone2-dependent stop and restore retain
the existing paired-channel policy. Each tone operation emits a complete
latch/data pair, so other channel writes cannot leave a stale latch behind.

`lt_stream_start` validates in foreground before output. It checks record
widths, offsets/boundaries, masks, operand ranges, initialized voices, coupled
noise, mandatory END and nonzero total duration. Structural inspections and
expanded commands each have an 8192-work limit. Expanded duration is capped
at 2^31-1 ticks; arithmetic checks precede 16-bit additions. Over-limit assets
are rejected with `LTA_INVALID`; they are never partly borrowed/decompressed.
Validation uses bounded stack state, no song-sized scratch buffer or heap.

At most 64 commands (including REF, WAIT and END) are interpreted per caller
tick. A validated source exceeding this burst limit is rejected at start;
the tick path also enforces the cap and stops/restores its layer on failure.
There is no unbounded catch-up after a foreground stall. Start/validation is
foreground-only. Tick can be called in a caller-owned service context only
when the selected sink and every shared audio access obey its serialization
requirements; the module installs no ISR, clock or arbitrary callbacks.

A stream effect can replace a stream in its own player according to priority.
An overlay owned by another player returns `LTA_BUSY`: explicitly stop that
clip/player before switching transports. Infinite and cancellable effects use
the clip loop API; stream references represent finite compiled repetition.

## Building and running

`python tools/build.py --target dos --watcom C:/WATCOM` builds `STRMDEMO.EXE`,
`STRMBEN.EXE` and the portable checks including `STREAM.EXE`, using 8086/8088
small-model C89. Copy `STRMDEMO.EXE` to a DOS Tandy and run it directly; no
external asset, Python, installation or conversion is needed. `STRMDEMO /auto`
plays a finite six-second sequence: music, ducking and a looping effect,
effect pause/resume, music pause/resume, clean boundary cancellation and
restored volume. Interactive keys are printed at startup. The demo polls
retrace in foreground and cleans up on Escape, expiry or failure.

Generated original examples live under `examples/stream`. The optional
developer command `python tools/compile_stream.py input.json output.h`
creates a ready-to-compile header; `--plain` disables references for trace
comparison. It never writes unchecked PSG port streams. Runtime validation
remains authoritative; the compiler only encodes/compresses logical records.

See `COMPACT-AUDIO-VALIDATION.md` for measured sizes, native timings and checks.
