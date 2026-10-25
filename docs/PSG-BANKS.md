# Selectable PSG banks: source map and handoff

Audited against libtandy main `8a0313625382bf1b9fd16843336d9f4b2f577e44`,
libwintandy main `0e2a65eeedd2b49d5b5fce689eeac39e97fb9477`, and
OEMSound main `537ebc5363c3d6750f554bb955d96ec9f4ce833f` on 2026-10-25.
This guides future implementation; this documentation change adds no bank
or selection API.

## Source ownership

| Owner / path | Current role |
| --- | --- |
| libtandy `sound/INSTR.C`, `INSTR.H` | Canonical compact curves, `TVOICE`, `InstStart`, `InstRead`. |
| libtandy `sound/MIDISYN.C`, `MIDISYN.H` | `MSYN`, program-to-preset mapping, allocation, controllers, note-off, render cache. |
| libtandy `sound/MIDIMSG.H`, `LTPBASE.H` | MIDI admission, integer widths, envelope/vibrato flags. |
| libtandy `sound/tests/portable_midi.c`, `tests/run.py` | Standalone parser/renderer checks. |
| libwintandy `libtandywin/AUDIO/SOURCE/` | Vendored `INSTR.*` / `MIDISYN.*`; `MIDICLK.C` owns foreground rendering and paired speaker/PSG fallback. `MDAPI.H` / `MIDICLK.DEF` define its API. |
| Same Windows directory: `MCITNDY.C`, `MCIPSG.C`, `SMF.*`, `MIDISKIP.H` | Streamed validation, admission and event production into MIDICLK. |
| OEMSound `experimental/WINLIB30/SOURCE/` | Matching Windows source mirror consumed by `tools/WINAMP/PACK.PY` / `VERIFY.PY`. Despite the directory name, it supplies the current player engine. |
| OEMSound `src/win30/DRIVER/` | Canonical unified `SOUND.DRV`; retain existing ownership and speaker transport. Bank tables do not belong here. |

`libtandy/src/lt_audio.c`, `lt_score.c`, and `lt_sfx.c` are separate native
PSG APIs, not MIDI banks. `libwintandy/sound/` is a separate pinned producer
compatibility module, not the player's synth. OEMSound's Winamp UI lives in
`experimental/WINAMP30/`; its player, format and decompression work is active
in another task and outside this handoff.

At these revisions `INSTR.C` / `INSTR.H` match across the canonical core,
Windows vendor and OEMSound mirror. Windows `MIDISYN.*` / `SMF.*` differ
from the standalone core and match the OEMSound mirror. Preserve the newer
Windows speaker-fallback and streamed-file changes when porting focused
instrument changes. Coordinate mirror updates explicitly; do not overwrite
the published Windows engine with the older portable parser/renderer.

## Actual instrument controls

- `INSTR.C` has `curves[11][16]`: eight melodic presets (keys, organ,
  bass, pad, reed, lead, bell, hit) and three percussion presets (kick,
  snare, hat). The table occupies 176 bytes before linker overhead.
- Entries are PSG attenuation 0..15 at 55 ms steps. Index 15 holds unless
  its value is 15, which ends the voice. Updates jump to the current index
  after a stall, with no catch-up loop. Attack/decay/held-level shapes are
  table values, not ADSR parameters.
- Melodic program 0..127 selects `program[channel] >> 4`: eight groups of
  sixteen programs. Program changes affect subsequent note-ons. Channel 10
  percussion chooses presets 8..10 and noise settings in `MIDISYN.C`,
  independently of the melodic program mapping.
- Velocity adds attenuation; CC7/CC11 add quantized channel gain. CC64
  defers note-off until pedal lift. Other note-off mutes immediately:
  there is no separate release-envelope stage or release-time knob.
- Presets 3..5 support divider-based vibrato after eight steps, with an
  eight-step motion cycle. CC1 affects depth; divider quantization limits
  high notes. Bend is fixed at +/-2 semitones. Pan is stored but output
  remains mono; CC91/93 add no reverb/chorus effect.
- The PSG has three square-wave tones and one noise voice. The optional
  PIT voice has fixed amplitude with a zero/nonzero gain gate and does not
  use PSG curves. No samples, filters, PWM, arbitrary waveform timbres or
  SoundFont loading are provided.

## Existing hooks and missing selector

`MSReset` initializes flags to 3 (envelopes plus optional vibrato).
`MSSpeakerMode` controls legacy/prepared-speaker/low-note-fold routing, not
banks. `MSMessage[Deferred]` / `MSRender` apply events and render state;
`MSInvalidate` marks direct changes for the render cache.

Windows `MDVoiceMode` (export ordinal 16) accepts flags 0, 1 or 3 in
`MD_OPEN`, before the first successful `MDPush` or `MDCommit` (`haveTime`
and `committed` must both be zero); `MDOpen` defaults voice flags to 1.
This controls envelopes/vibrato, not banks. The older direct-driver
`MODM_SYNTHCONFIG` in `MIDIAPI.H` also controls flags. No bank field, setter
or export exists in `MSYN` / `MDAPI.H`; player MCI `MC_SET` handles time
format only.

Low-level `MIDIMSG.H` rejects nonzero CC0/CC32. The Windows file producer
uses `MIDISKIP.H` to skip unsupported nonzero bank selects, keeping the one
bank and honoring program changes. Zero bank select is an admitted no-op.
These are different policies, neither an alternate-bank selector. A future
explicit API must define invalid IDs, reset/default behavior, active-note
behavior and cache invalidation, keeping Windows `synth` / `folded` in sync.

## Scope, size and acceptance

Preserve Default output exactly, add one contrasting local bank, and provide
a developer host A/B audition of the same event sequence. Keep notes,
programs, controllers, velocity and timing identical between takes. Use
original synthetic fixtures; do not edit accepted songs or require consumer
Python/modern-PC preparation. Traces establish output equivalence; an audible
host preview must actually render audio and label its limits.

Keep C89, integer math, fixed storage, O(1) lookup and existing foreground
scheduling. No heap, new driver, timer/IRQ owner or large per-voice bank copy.
Account for tables once and per-`MSYN` additions twice in MIDICLK (`synth`
and `folded`). Preserve the DS near-pointer scratch storage noted in
`MSRender`; Win16 DLL SS and DS need not match.

The current Windows receipt `libtandywin/AUDIO/TESTS/NATIVE/MIDI8.MD`
records MIDICLK CODE/DGROUP minima 14352/12018 bytes, MCITNDY 10948/3398,
and RAWCLK 13800/27168. MIDICLK code is movable/discardable/preloaded;
data remains fixed/shared. The DLLs have zero local heap/private stack.
Report added code/data and native map/segment evidence; host `sizeof` does
not prove 16-bit layout. Respect segment limits and the 640 KB real-mode
budget; host checks alone cannot establish that a new bank fits.

Parser versions differ: standalone `sound/SMF.H` is buffered, limited to
60000 bytes / 32768 channel events / 16 tracks. Windows `SMF.H` streams
up to 1 MiB / 65000 events / 16 tracks with 64-byte track windows; its
memory-buffer convenience API retains a single-segment limit.

Implementation checks: Default traces before/after across all presets and
flat/envelope/vibrato modes; alternate audible difference; invalid IDs and
reset/switch/active-note behavior; note-off/sustain, CC7/11/1, pitch bend,
steal, percussion, long stall and render cache; paired speaker/folded
consistency. Retain parser/controller regressions. Native qualification is
a separate explicitly scheduled step; no emulator launch in this handoff.

## Windows host checks

From the libtandy root (developer Python only):

```powershell
python tools/build.py --target host --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py --runtime-bin C:/msys64/ucrt64/bin
python C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py -- -x c -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-sign-compare -Wno-unused-function -O2 -DSYNTH_HOST -I sound sound/INSTR.C sound/MIDISYN.C sound/SMF.C sound/tests/portable_midi.c -o build/host/portable_midi.exe
& ./build/host/portable_midi.exe
```

The first command creates `build/host`. `sound/tests/run.py --cc gcc` is
the POSIX/ordinary compiler recipe; it accepts one executable and cannot
invoke the Python launcher via multiword `--cc`. Use the explicit Windows
command above. Integration MIDI checks are in libwintandy `docs/BUILD.TXT`.
Host checks do not qualify native timing, audio or code size.
