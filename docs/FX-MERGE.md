# Prepared effects and music

`sound/FXMERGE.H` and `FXMERGE.C` provide a pure C89 state merger and delta
encoder. They own no timer, hardware, memory allocation, or sound claim.
The application supplies a validated base PSG/PIT state and five effect
slots: tone0, tone1, tone2, noise, and speaker. Periods are 1..1023,
attenuation is 0..15, noise mode is 0..7, and PIT is 0 (off) or 151..65535.
Inactive slots restore the current base state. A new trigger replaces its
lane's prior effect; cursor, retrigger and clock policy belong to the producer.

Pitched noise sets `borrow2` in slot3. It wins over both music and an effect
on tone2, sets that divider, and mutes tone2's audible output. The reservation
continues during gated/silent rows. When the noise slot becomes inactive,
tone2's current state returns, including a tone effect that continued advancing
while hidden. Fixed-rate noise leaves tone2 alone. An active speaker slot
replaces the current base PIT divisor, including zero for a gate-off row.

`FXMMerge(out,base,layers)` produces a complete state. `FXMBytes` compares it
with the last emitted state and writes at most 11 bytes into a 16-byte buffer.
Set `initial` only at whole-owner startup. Set `noiseAttack` for an actual
noise retrigger, including restoration of a sounding base noise voice.
Other updates write the noise level without resetting the LFSR. Buffer and
input objects must not overlap; out may alias base. Inputs must be validated.

An owning application should merge first, publish the resulting timestamped
PSG/PIT writes to its existing transport, then cache the complete state.
Never insert Effects Lab `FXBytes`' initial whole-PSG mute sequence into an
individual song lane. The compiler's trajectory rows can feed the slots
directly. These are synthesized PSG/PIT effects; there is no PCM layer.

OEMSound Beats Lab supplies the application producer, TFX1 patch import,
per-step transposition and DWA2 song persistence. libwintandy's
`libtandywin/AUDIO/SOURCE/FXMERGE.*` is the matching Windows source mirror.
Existing MIDI banks, driver APIs, and transport ordinals are unchanged.

Run the standalone check on Windows with the managed GCC launcher:

```
python sound/tests/fxmerge.py --out <new-private-output>
```

The merger uses 78 bytes of state/layers on a 16-bit target, plus the
caller's previous state and output buffer. It contains no tables. Host checks
do not establish native code size, IRQ timing, or audible quality.
