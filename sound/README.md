# Portable MIDI core

This is the first-party, MIT-licensed foreground MIDI parser and renderer from
the approved playback-opt9 engine. The 11 core files are preserved byte for
byte; `SOURCE.json` records their SHA-256 hashes. They use C89, fixed-size
state, integer arithmetic, and no OS, heap, port-I/O, timer, interrupt, or
third-party instrument-bank dependency. The local instrument curves in
`INSTR.C` are code tables, not imported patches or song assets.

`smf_open` validates a complete Standard MIDI File before playback (at most
60,000 bytes, 16 tracks and 32,768 channel events). `smf_next` yields channel
events in stable tick/track order; `smf_rewind` replays the validated file.
The caller retains the input buffer until `smf_close`. See `SMF.H` for return
values and lifetime rules.

`MSReset`, `MSSpeakerMode`, `MSMessageDeferred`, and `MSRender` maintain three
PSG tone voices, one PSG noise voice, and an optional PIT speaker voice. The
caller supplies MIDI event times and sends rendered periods, volumes, noise
state, and PIT divisor to its own hardware transport. The scheduler and
hardware drivers are deliberately outside this portable module. Callers that
coalesce same-time events should apply them with `MSMessageDeferred`, then
render the group once with `MSRender`; preserve the event order from `smf_next`.

Capability revision `portable-midi/1` accepts melodic MIDI note numbers
45–127 in PSG-only mode, and 0–127 when speaker mode is enabled. Speaker mode
adds 31 lower nominal labels, 14–44 (D0–G#2), via PIT divisors. Notes 0–13
are octave-folded into the PIT range. The PSG's 10-bit integer divider
quantizes pitches, especially high notes: labels 126 and 127 both use divider
8. Acceptance of 114 nominal labels 14–127 does **not** imply 114 distinct or
perfectly tuned pitches. Drum note-on admission is 35–81 on MIDI channel 10.
One local instrument bank is supported; nonzero bank selection is rejected.

On a host with a C89 compiler, run `python sound/tests/run.py --cc cc`.
`CORE-MIT.TXT` contains the required notice for this module.
