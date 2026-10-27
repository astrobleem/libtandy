# Engine validation — 2026-10-27

Candidate starts from public main `a034067` (merged compact-audio changes).
The engine is optional and no existing sound or graphics API was changed.
This record covers the C89 foreground engine, its explicit native presenters,
and the original graphics/text demonstration route; it is not a hardware
performance certification.

## Checks

- Managed UCRT64 GCC: all 16 host test executables passed, including the new
  lifecycle/physics and presentation tests and the existing audio/graphics
  suite. Strict C89, pedantic errors, warnings as errors, `-O2`.
- Open Watcom 1.9: normal complete DOS build passed with `-0 -ms -ecc -os -s
  -w4 -wx`, 4096-byte demo stacks. New modules/tests produced no diagnostics.
  The build still reports the preexisting always-false comparison in
  `lt_time.c` and unreachable code in `gfx_host.c`.
- Focused native execution: fixed 86Box build, Tandy 1000 HX, 8088 at
  4,772,728 Hz, 640 KiB, copied DOS disk. ENGINE and ENGVIEW passed with
  actual 16-bit types/far storage. ENGDOS passed mode selection and original
  mode/page/cursor restoration for all four modes, invalid-mode rejection,
  double-open/idempotent-close checks, and unchanged IRQ0/IRQ1 vectors.
- All five finite demonstration routes returned success to DOS. The BDA
  reported the expected BIOS modes during execution; early/late headless
  framebuffer captures were retained for each route.
- Independent reviewer replayed host engine/presentation binaries and reviewed
  code and docs before local commit. Findings led to fixed-tick-only lifecycle
  activation (including nested spawn chains/zero-step frames), correct far
  fixture initialization, native warning guards, leading-edge collision
  checks, and packed span/bank-address optimization. Final review found no
  remaining blocker.

| Run | Mode | Fixed ticks | Frames | Final debt | X/Y hits | Furthest camera X | Final body X,Y |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ENGDEMO /auto | 09h, 320x200x16 | 300 | 76 | 0 | 1 / 141 | 316 | 330,176 |
| ENGDEMO /auto /low | 08h, 160x200x16 | 300 | 76 | 0 | 1 / 141 | 396 | 330,176 |
| TXTDEMO /auto | 03h, 80x25 | 300 | 76 | 0 | 1 / 141 | 128 | 330,176 |
| TXTDEMO /auto /40 | 01h, 40x25 | 300 | 76 | 0 | 1 / 141 | 312 | 330,176 |
| ENGDEMO /auto /slow | 09h, three-tick foreground wait | 300 | 76 | 0 | 1 / 141 | 314 | 330,176 |

Awake/Start counts were 1/1 in every route; Update/LateUpdate counts matched.
The slow wait did not further reduce frame count on this emulator because
the viewport rendering already exceeded that wait. Determinism across
different frame partitions is directly tested in the portable/native core
test: 30 single-tick frames versus budgeted catch-up to the same 30 ticks;
also FixedUpdate -> Awake -> grandchild spawn chains with extra zero-step
frames versus one three-tick frame. Camera maxima may vary with LateUpdate
sampling; simulation results do not.

An initial 240-second exploratory run timed out during the unoptimized
graphics route. A second exploratory run was deliberately stopped after the
renderer bottleneck was identified. Both owned emulator children stopped
and released the shared byte-range lease. The final optimized suite reached
DONE, stopped its child and released that same lease. No unrelated emulator,
disk image or panning source was modified. The raw headless graphics captures
can include stale pixels outside the active 200-row scene; those pixels are
not evidence about the display adapter. Bank/padding/nibble correctness is
also checked directly by the native far-buffer presentation test.

## Native cost

Measured from Open Watcom OMF SEGDEF records, not object-file length:

| Optional module | 8088 CODE bytes | Module global data bytes |
| --- | --- | --- |
| `lt_engine.c` | 2089 | 0 |
| `lt_engine_view.c` | 622 | 0 |
| `lt_engine_dos.c` | 447 | 0 |
| Total | 3158 | 0 |

These figures exclude shared C runtime helpers (integer arithmetic,
`memset`, far fill and BIOS wrappers) and linker alignment. Application cost
depends on which helpers it already uses. Omitting the modules adds no
engine code or state to existing applications.

Actual native `sizeof`: world 18 bytes, slot 6, behavior table 10, body 8,
camera 8, tilemap descriptor 14, view 14, DOS video object 22. Eight slots and
one world therefore require 66 bytes, plus caller data/behavior tables.
The demo tile cells require 2400 bytes; no engine heap or interrupt stack.

ENGDEMO.EXE is 15,912 bytes and TXTDEMO.EXE 15,910 bytes. MZ loader minimum
image/allocation plus PSP is respectively 22,976 and 22,960 bytes (rounding
the load image up to paragraphs before adding minimum allocation), including
the linked 4096-byte stack and demo/static/CRT data, excluding the environment
and DOS bookkeeping. Both EXEs retain the compiler's default maximum-memory
request; these header minima are not a tested low-memory startup threshold.
The display apertures are 32 KiB for mode 9, 16 KiB for mode 8, and 4000/2000
bytes for text. The engine allocates no offscreen framebuffer.

## Reproduction and evidence

```powershell
python tools/build.py --target host --gcc-launcher C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py --runtime-bin C:/msys64/ucrt64/bin
python tools/build.py --target dos --watcom C:/WATCOM
```

Run ENGINE, ENGVIEW and ENGDOS in DOS on compatible Tandy hardware, then the
five commands above. Consumers do not need Python or converted assets.

Developer evidence on the validation host:
`C:/Users/chad/Documents/Codex/2026-10-27/task/native-engine-03/RESULT.json`
and adjacent CORE/VIEW/VIDEO/G320/G160/T80/T40/SLOW logs and PNGs.
Host/DOS compiler invocation manifests are under `build/host/BUILD.json` and
`build/dos/BUILD.json`; OMF segment totals are `build/dos/ENGINE-COST.json`.
The machine-local developer harness and measurement script are outside the
repository and are not consumer dependencies.

Final native binary SHA-256:

```text
ENGINE.EXE  e88b1480bcfa405499b20138e0fd59400a8714ea7a03e4b026e654a143efb703
ENGVIEW.EXE 48e67d3b46992d6e0c7ec9216094a6dcd3bd1fef6922cb70f777b6d2d75cd622
ENGDOS.EXE  dcfbfdc2c9f0f03c3209202165647846af6606795017c1277eb5dfcefac8d25b
ENGDEMO.EXE 5f3f80eaa62734c6949d61c798a917c6ae790b2ff544d388320a108f103660de
TXTDEMO.EXE 2c5be6da695efe692bbcebf2bce43082572a8971fc8c27915aec57b45c763c37
```
