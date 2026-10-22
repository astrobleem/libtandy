# Caller-ticked PSG effect clips

`lt_sfx` plays short SN76496-family command sequences through the existing
`lta_handle` and `lt_chip` ownership path. It allocates no memory, installs no
timer or interrupt, and emits no PCM. The game calls `lt_sfx_tick` from its
existing foreground update at the clip's intended tick rate.

## C descriptor `LTSFX_CLIP_VERSION` 2

The C descriptor is runtime-facing. Its `tick_rate_num/tick_rate_den` fields
express the source timeline as a reduced positive rational number of caller
ticks per second. For example, `150247/2500` is exactly 60.0988 Hz; 60 Hz is
`60/1`. The player never consults these fields to schedule itself: the game
continues to call `lt_sfx_tick` from its existing update cadence.

The event record is exactly six bytes:

| Field | Meaning |
| --- | --- |
| `tick` | Absolute caller tick from clip start; events are sorted, and ties keep array order. |
| `op` | `LTSFX_OP_TONE`, `LTSFX_OP_NOISE`, or `LTSFX_OP_STOP`. |
| `channel` | Tone channel `0..2`; noise channel `3`. |
| `value_lo`, `value_hi` | Little-endian 10-bit tone period; noise mode `0..7`; zero for stop. |
| `level` | PSG attenuation `0..15`; zero for stop. |

The clip descriptor contains version `LTSFX_CLIP_VERSION`, channel mask,
priority, event count, duration, provenance ID, exact event byte length,
source tick rate and an event pointer. A clip has 1..64 events and 1..255
ticks; each event time is less than its duration. `event_bytes` must equal
`event_count * 6`, so a short or mismatched clip is rejected before any
channel is borrowed. Provenance ID zero means no source notice; ID one marks
the JAWS Milestone 2-derived samples. Keep the descriptor and events unchanged
while playing.

## Binary `.ltsfx` file version 1

The binary file is canonical little-endian data, not a C struct dump. The
fixed 20-byte header is followed immediately by exactly `event_count` records:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `LTSF` |
| 4 | 1 | File version, `1` |
| 5 | 1 | Header size, `20` |
| 6 | 1 | Channel mask |
| 7 | 1 | Priority |
| 8 | 1 | Event count, `1..64` |
| 9 | 1 | Duration in caller ticks, `1..255` |
| 10 | 4 | Tick-rate numerator, unsigned LE32 |
| 14 | 4 | Tick-rate denominator, unsigned LE32 |
| 18 | 1 | Provenance ID: `0` none, `1` JAWS M2-derived |
| 19 | 1 | Reserved, must be zero |

The record fields remain `(tick, op, channel, value_lo, value_hi, level)`.
The exact file size is `20 + 6 * event_count` bytes (maximum 404). Rates must
be positive and reduced. The parser rejects unknown versions or header sizes,
nonzero reserved/unknown provenance values, malformed events, truncation,
trailing bytes and undersized caller storage. Noise coupling rules match the
runtime trigger validator.

No-heap API:

```c
lt_sfx_file_size_v1(&clip, &needed_bytes);
lt_sfx_file_encode_v1(&clip, file_bytes, capacity, &written_bytes);
lt_sfx_file_decode_v1(file_bytes, file_size, &clip, event_storage,
                      event_storage_capacity);
```

The decoder copies records into caller-owned storage and points the returned
descriptor at that storage. It never executes code or allocates memory. The
Beats editor can save/import `.ltsfx` files and emit C-array headers directly
from its clip model; it does not need to parse arbitrary C. Generated JAWS
derivatives retain provenance ID one, which the C exporter can emit as the
comment `/* Derived from JAWS M2; see examples/jaws/README.md. */`.
These APIs are not in-place: keep the clip descriptor, its event array, input
file bytes, output file bytes and `written_bytes` in distinct non-overlapping
storage. On decode failure, the output descriptor is unchanged, but event
scratch storage may already contain copied records. Beginner examples should
use separate static arrays for the file buffer and event storage.

The C-array export shape is the same as the checked-in JAWS presets:

```c
static const lt_sfx_event pickup_events[] = {
    { 0U, LTSFX_OP_TONE, 0U, 80U, 0U, 3U },
    { 3U, LTSFX_OP_TONE, 0U, 40U, 0U, 6U }
};
static const lt_sfx_clip pickup_clip = {
    LTSFX_CLIP_VERSION, 1U, 2U, 2U, 6U,
    LTSFX_PROVENANCE_NONE, (lt_u16)sizeof(pickup_events),
    60UL, 1UL, pickup_events
};
```

The channel mask bits 0..2 name tone voices and bit 3 names noise. A noise
mode that selects tone 2 (`mode & 3 == 3`) requires both tone 2 and noise in
the clip mask, with a tone-2 period event before the noise event. When the
audio handle owns both channels, borrowing either tone 2 or noise temporarily
reserves and mutes the pair, preventing an effect from changing the background
noise clock.

## Playback and ownership

Initialize one player for an existing `lta_handle`. Triggering validates the
whole clip, borrows its channels and silences the music there. Tick-zero
commands begin on the next call to `lt_sfx_tick`. A lower-priority clip returns
`LTA_BUSY`; equal or higher priority replaces the current effect. The newest
equal-priority trigger wins. Playback is bounded by 64 event applications in
one caller tick.

Music commands continue to update `lta_handle` while the effect owns a voice,
but those writes stay suppressed on borrowed channels. At expiry, `stop`, or
`reset`, the player replays the latest music state for those channels. A music
stop during an effect is remembered as silence. Other channels remain under
the existing music handle throughout.

```c
lt_sfx_init(&effects, &music);
if (pickup_collected)
    lt_sfx_trigger(&effects, &lt_sfx_jaws_conch);
/* Once per existing game update: */
lt_sfx_tick(&effects);
```

## Source examples

`examples/jaws/jaws_clips.h` contains the harpoon cue and conch-pickup cue
derived from JAWS Milestone 2 observations. Their ownership, 60.0988 Hz source
tick, period mapping, capture references, and separate GPL-3-only status are
documented in that directory. The player and core library remain MIT licensed.

`examples/dos_sfx_demo.c` advances one clip tick on each detected vertical
retrace edge from status port `0x3DA`, bit 3. Each wait is bounded to 65,535
port samples; a missing edge stops the demo. This is foreground polling only:
it does not install an IRQ or reprogram PIT. The active display mode sets the
demo rate, nominally about 60 Hz on CGA/Tandy video. The JAWS source rate is
60.0988 Hz, so nominal demo lengths are about 183.3 ms for the 11-frame
harpoon and 1.083 s for the 65-frame conch; exact duration follows retrace.
This keeps each PSG transition on its own display frame instead of batching
several transitions at an 18.2 Hz BIOS sample.

Build `SFXDEMO.EXE` with the DOS build command in the root README, then run
`SFXDEMO /demo` from DOS for a finite, keyboard-free pass. It triggers the
harpoon before waiting for a retrace; that next edge applies tick zero. It
then calls `lt_sfx_tick` for all 11 clip ticks plus one expiry tick, waits 18
silent retrace ticks, and plays the 65-tick conch plus its expiry tick. The
pass therefore spans 96 retrace edges (about 1.6 seconds at nominal 60 Hz),
then sends stop to all four owned PSG channels and releases them. A retrace or
audio error exits through the same cleanup. Running without arguments keeps
the interactive H/C/Q demo.
