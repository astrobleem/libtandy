# Local validation, 2026-10-25

Base: libtandy main `5d7036b9cdd23be9a0a3be7e9bc53c81680b055d`.
Chad listened to and approved the host sound, then requested this optional
example. Chad approved publication of the tested example on 2026-10-25.

- The editable score, VGM and 26-row per-note audit are byte-identical to
  the approved assets. Header changes are comments only; all events,
  periods, tempo, levels and release gaps are unchanged.
- Re-rendering with this checkout's unchanged `lt_chip`, `lt_audio` and
  `lt_score` produces the identical WAV, register trace, audit and VGM.
  Approved WAV SHA-256:
  `9bd189015904bda25c5e75dff501696a7641934bb3ffeec87e3278f719d00b0d`.
- The normal host suite passes all 12 test programs, including the new
  regression comparing 173 actual PSG writes and their service frames to
  the frozen approved trace. The generator's read-only header check passes.
- The normal DOS build completed all 40 steps with Open Watcom's 8086
  small-model flags and warnings treated as errors. Native acceptance then
  exposed an output-name collision: the regression overwrote `DOEBEL.EXE`.
  The regression now builds as `DBSCORE.EXE`; its old 18,488-byte binary was
  preserved. Only the player was rebuilt with the same normal compiler
  recipe, without repeating host suites. The actual `DOEBEL.EXE` is 14,826
  bytes, SHA-256
  `a3857b85df56b2ff7d031916852f58ec9f64ac3e06bd1296d06b7f8d139026a9`.
- Both requested short native checks passed on an emulated Tandy 1000 HX,
  8088 at 7.159092 MHz, 640 KiB, MS-DOS 6.22. Normal completion returned
  successfully; Escape stopped active playback within eight BIOS ticks.
  Both paths reported PSG mute and restored the original IRQ0 vector
  `F000:FEA5`. Post-return PIT counters were in BIOS range, and the BIOS
  clock advanced six ticks in each approximate 0.31-second observation.
  [NATIVE.json](NATIVE.json) records the results and executable identity.
- The shared lease was acquired after the preceding owner finished. The
  private child was stopped before the lease was released. Physical hardware
  and native audio capture remain outside this acceptance check.

Commands run from the repository root:

```powershell
python examples/doebel/generate.py --check
python tools/build.py --target host --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py --runtime-bin C:/msys64/ucrt64/bin
python tools/build.py --target dos --watcom C:/WATCOM
git diff --check
```

The generated header is 1,739 bytes of source text; its 26 `lt_score_note`
events occupy 156 bytes of note data. The ready one-shot VGM is 612 bytes.
Build outputs and full compiler logs stay in ignored `build/` directories.
