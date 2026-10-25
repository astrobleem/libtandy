# Default and Chamber PSG banks

The portable MIDI renderer offers Default (0) and Chamber (1). Default keeps
the original output. Chamber adds eight melodic attenuation curves. Notes,
programs, controllers, percussion and speaker routing keep their meaning.

## Selection and lifetime

`MSBank(MSYN *, id)` returns zero on success. An invalid ID returns nonzero
without changing state. A melodic note latches selection in `TVOICE.bank`
at note-on. Held and pedal-released voices retain their existing curves.
`TVOICE.preset` keeps its original 0..10 meaning; never encode bank in it.
`MSReset` restores Default; callers reapply their explicit selection after
each reset and preserve per-voice bank state during live rebuilding.

Program changes, CC121/123 and zero CC0/32 do not select a bank. Nonzero
CC0/32 remain unsupported by low-level admission; the Windows file producer
skips them separately. `MSSpeakerMode` controls speaker/folding behavior;
synth flags control envelopes/vibrato. Neither selects instruments. Direct
voice/controller changes require `MSInvalidate` before `MSRender`.

## Curves and source ownership

`sound/INSTR.C` owns `curves[19][16]`. Original rows0..10 are unchanged:
eight melodic presets and kick/snare/hat. Chamber rows11..18 provide
plucked keys, swelling organ, soft bass, slow pad, breathy reed, fading
lead, mallet and blip. Values are PSG attenuation0..15 at55ms steps.
Step15 holds unless it is15, which ends the voice. There is no separate
note-off release envelope. Chamber attacks take up to165ms to contribute
PCM in the audition; short bell/hit curves can decay before key release.

Melodic programs select `program[channel] >> 4`. Presets3..5 retain
divider vibrato after eight steps. CC1 changes its depth; CC7/11 affect
attenuation; CC64 defers note-off; bend is fixed at +/-2 semitones. The
optional PIT speaker ignores PSG curves. No samples or SoundFonts are added.
Production rendering remains C89, integer, fixed storage and bounded lookup,
with no heap, OS, ports, timer or IRQ ownership.

| Repository/path | Role |
| --- | --- |
| libtandy `sound/INSTR.*`, `MIDISYN.*` | Canonical bank tables, allocation and rendering. |
| libtandy `sound/SMF.*` | Buffered parser:60000bytes,32768events,16tracks. |
| libtandy `tools/psgaudit.c` | Developer host A/B mixer, an approximation of PSG output. |
| libwintandy `libtandywin/AUDIO/SOURCE/` | Windows vendor and MIDI/speaker integration. |
| OEMSound `experimental/WINLIB30/SOURCE/` | Matching current player engine mirror. |
| OEMSound `src/win30/PLAYER/` | MINIPLAY UI and persistent Default/Chamber choice. |
| OEMSound `src/win30/DRIVER/` | The one shared SOUND.DRV. |

Windows preserves newer speaker fallback and streaming SMF (1MiB,
65000events,16tracks,64-byte windows). Port focused hunks, never overwrite
that parser/renderer with this portable version. libtandy `src/lt_audio.c`,
`lt_score.c`, `lt_sfx.c` and libwintandy `sound/` are separate APIs.

The qualified Windows integration uses `MDInstrumentBank` (ordinal19),
`MDSeedBank` (20), and `MciBank` (905). Both MIDICLK renderers (`synth` and
`folded`) reapply selection after resets, and live seeding preserves each
voice's bank. MINIPLAY stores selection in app-local WINAMP.INI. Windows
evidence is in libwintandy `docs/BANKS.TXT` and the source-pinned
`libtandywin/AUDIO/TESTS/NATIVE/BANKS.JSN`.

## Host checks and unchanged auditions

On POSIX use `python3 sound/tests/run.py --cc gcc`. On Chad's Windows host:

```powershell
python sound/tests/run.py --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py
python tools/build.py --target host --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py --runtime-bin C:/msys64/ucrt64/bin
```

The Windows runner refuses raw GCC and uses the launcher's DLL preflight.
The suite includes76 MIDI checks,111 bank checks and two Default traces
(including repeated `MSBank(0)`): `70D11B5D`,6634events,9798renders. Cases
cover presets, controllers, flat/envelope/vibrato, speaker, cache and stalls.

The actual audition mixer is privately instrumented without changing its
source or writing WAVs. Every expected nonzero-velocity note-on must allocate
and contribute to quantized final PCM, allowing attack delay. All154 notes
contribute, with zero missing/clipped/rejected/stolen notes in these scenes.
A deliberately muted phrase note must be detected as exactly one missing
voice despite nonzero mixed output. Velocity-zero note-ons are note-offs.
This is stronger than whole-scene RMS/peak alone.

For a read-only comparison against all eight existing approved takes:

```powershell
python sound/tests/audition/run.py --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py --audio C:/Users/chad/work/out/audition
```

The separate developer tool `tools/psgaudit.c <existing-output-directory>`
renders A/B WAVs with identical events and gain, without normalization.
These host previews do not claim physical Tandy output. No audition or
accepted song is changed by these tests.

## Size and native evidence

Tables add128bytes. Open Watcom `-ms -0 -os` measured INSTR code+19 and
MIDISYN+47bytes. Two-byte packing uses existing padding; packed-one-byte
layouts add five bytes per MSYN. Windows MSC6 measured TVOICE10/MSYN332,
MIDICLK CODE14844/DGROUP12148 (+492/+130), MCITNDY11192/3404 (+244/+6),
and MINIPLAY48202/16016 (+688/+416). Corrected RAWCLK paired-live handling
adds67code bytes; DGROUP27168 and SOUND.DRV remain unchanged.

Host checks alone do not establish Win16 ABI or hardware timing. Windows
evidence keeps85 file-player checks and28 corrected live-bank checks distinct
from the17-check exact-final-ZIP install/play/Stop/Close/DOS-cleanup smoke.
The older file suite is not relabelled as a rerun. No physical hardware
test is claimed. The merged Doebel example remains unchanged.
