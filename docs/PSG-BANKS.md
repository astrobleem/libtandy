# Selectable PSG banks: source map and handoff

Audited against libtandy main `8a0313625382bf1b9fd16843336d9f4b2f577e44`,
libwintandy main `0e2a65eeedd2b49d5b5fce689eeac39e97fb9477`, and
OEMSound main `537ebc5363c3d6750f554bb955d96ec9f4ce833f` on 2026-10-25.
This section describes the pre-bank state; the canonical implementation is
documented at the end of this file.

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
or export exists in the Windows `MDAPI.H` (pre-port; canonical `MSYN`
now has `MSBank`, see the end of this file); player MCI `MC_SET` handles time
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

## Implementation: Default and Chamber banks (branch feat/psg-alt-bank)

API (`MIDISYN.H`): `MS_BANK_DEFAULT` 0, `MS_BANK_CHAMBER` 1, and
`LTP_U16 MSBank(MSYN *,LTP_U16)`, which returns 0 on success and nonzero with
state unchanged for any other ID. The bank is latched into `TVOICE.bank` at
melodic note-on, so held, pedal-released and already-sounding voices keep
their curve; the next note-on uses the new bank. Percussion (`voice[3]`) and
speaker routing ignore banks. `MSReset` zero-fills `MSYN`, so it restores
Default; callers re-apply their choice after every reset. CC0/CC32,
CC121/123, program change and `MDVoiceMode` do not touch the bank.

Data: `curves` grows from 11 to 19 rows; rows 11..18 are the Chamber
melodic presets (plucked keys, swelling organ, soft bass, slow pad, breathy
reed, fading lead, mallet, blip). Lookup is `preset+11` when the voice's bank
is nonzero. Vibrato still keys off presets 3..5, so Chamber pad, reed and
lead keep vibrato. Chamber peaks match Default; contrast is attack, decay
and held level.

Tests (`sound/tests/run.py`): `portable_midi.c`, `bank_test.c`, and
`trace_default.c`, whose embedded hash was taken from the pristine 5d7036b
tree, so Default equivalence is checked without that tree. Developer audition:
`tools/psg_audition.c <outdir>` renders host WAVs (an approximation, not Tandy
output) from the real synth; no normalization is applied.

Integration checklist for the Windows player:
1. Port by hunk, never wholesale: `TVOICE.bank` (INSTR.H); `curves[19]`,
   `InstStart` `bank=0`, and the row lookup (INSTR.C; these files match
   across all three trees); `MSYN.bank`, `MSBank` and the single line
   `s->voice[slot].bank=s->bank;` after the melodic `InstStart` (MIDISYN.*).
   Keep the Windows speaker-fallback and streamed-MIDI changes.
2. Call `MSBank` on both `synth` and `folded` in MIDICLK, and re-apply the
   selection after every `MSReset` (open, seek, reopen).
3. `LIVEMIDI.H` seeds programs from `voice[i].preset<<4`; it ignores `bank`,
   so a live seed would revert Chamber voices to Default. Decide whether to
   carry the bank in the seed.
4. Add an explicit setter/export and UI; do not reuse CC0/CC32 or
   `MDVoiceMode`. Keep the `SOURCE.json` hashes and the OEMSound mirror in step.
5. Qualify natively: Win16 code/DGROUP sizes, MIDICLK segment limits, and
   listening on real hardware. None of that has been done.

Host commands for the new checks (same wrapper and flags as above; add
`-DWITH_BANK` to `trace_default.c` to cover `MSBank(0)` as a no-op; `run.py`
builds all of these with an ordinary `--cc`):

```powershell
python C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py -- -x c -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-sign-compare -Wno-unused-function -O2 -DSYNTH_HOST -I sound sound/INSTR.C sound/MIDISYN.C sound/tests/bank_test.c -o build/host/bank_test.exe
python C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py -- ... -DWITH_BANK -I sound sound/INSTR.C sound/MIDISYN.C sound/tests/trace_default.c -o build/host/trace_default.exe
python C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py -- ... -I sound sound/INSTR.C sound/MIDISYN.C tools/psg_audition.c -o build/host/psg_audition.exe
```

Size notes (Open Watcom `-ms -0 -os`, not MSC 6.0 or the Win16 DLL model):
INSTR `_TEXT` +19, `_DATA` +128 (table 176 to 304 B), MIDISYN `_TEXT` +47.
`MSYN` stays 330 B at default 2-byte packing because `bank` and `TVOICE.bank`
use existing padding; under `-Zp1` it grows by 5 B (+10 B for `synth` and
`folded`). No `-Zp` appears in the libwintandy sources checked; the Windows
`MSYN` has the same odd `LTP_U8` run. Confirm against the native map.
