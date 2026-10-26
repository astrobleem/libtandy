# Validation and cost

The reusable sound module/API and extracted PSG data are unchanged. This
revision wires the accepted stroke-trace artwork into the optional Tandy
mode-9 example, with the requested quarter-second hold before the final hit.
It adds no CGA fallback and changes no graphics/sound driver or MIDI bank.

## Checks

- Managed GCC C89 host suite passes, including the original unit tests and
  every one of the 710 PSG bytes at its expected source-derived service tick.
- Open Watcom `-0 -ms -ecc -os -s -w4 -wx` DOS build passes. Both EXEs are
  self-contained; no consumer Python, PNG, VGM or conversion is required.
- `tools/check_tandy_trace.py` passes the actual runtime-header round trip,
  all six sparse patch passes and every individual byte's RGB invariance.
  The four masks need 14/15/15/16 distinct color histories; all 54 whole-
  sequence histories cannot fit in one fixed 4bpp mask.
- A native Tandy HX run configured at **4,772,728 Hz** passed the exact
  delivered executable. It completed all 52 trace steps and three handoffs
  with **zero missed animation deadlines**. TANDY settled at tick **421**;
  the tagline appeared at **481**, a **0.250021-second** hold.
- Handoffs measured **58.565, 63.274 and 68.125 ms**, against **95.841 ms**
  available each: at least **27.716 ms** spare. The initial C transfer loop
  exceeded this budget; the delivered loop uses a short, fully interruptible
  8088 `LODSW`/`LODSB`/`STOSB` routine with no CLI. Video transfers and
  DOS work stay out of the audio callback.
- Native diagnostics captured **714 exact byte/service-tick records**:
  all 710 source writes plus the four cleanup mutes. No trace overflow;
  two nested timer events were queued/drained by the existing adapter.
  Maximum recorded audio callback-entry offset was **0.680 ms**.
- Indexed native VRAM matches all **64,000 source-derived RGB pixels**.
  The 124 sampled native frames after white showed no regression in visible
  source-ink pixels; six frames sampled the full-blue/no-tagline hold.
  The live FRAME API occasionally adds a thin RGB(0,0,31) scanline marker,
  outside the example's palette. The raw recording preserves those capture
  artifacts; the VRAM image and independent patch checks do not contain them.
- Normal completion/hold/return restored IRQ0 and the BIOS clock (six BIOS
  ticks over about 0.31 s). The owned emulator stopped and the real shared
  lease was released. Core cancellation/restart/mute remain host-tested;
  earlier native cancellation checks are historical, before this logo change.

The receipt is [NATIVE.json](NATIVE.json). Physical hardware, analog output
and native audio recording are not qualified.

## Cost

| Component | Cost |
| --- | ---: |
| Sound stream: 119 groups / 710 PSG bytes | 1,067 constant bytes |
| Optional effect module, Watcom 8086 small-model text | 449 bytes |
| Caller-owned sound-effect player, DOS small model | 8 bytes |
| Mutable static state added by sound-effect module | 0 bytes |
| Initial logo RLE | 4,806 constant bytes |
| Sparse logo patches | 26,955 constant bytes |
| Logo deadlines/descriptors | 128 constant bytes |
| Total runtime logo tables | 31,889 constant bytes |
| `/stats` trace buffer, statically reserved in examples | 2,142 bytes |
| Sound-only EXE | 16,440 bytes |
| Logo EXE | 54,888 bytes |
| Logo minimum MZ startup allocation, including PSP | 64,840 bytes |

The MZ minimum excludes the DOS environment and optional CRT file buffers.
The example also uses the existing timer's 3 KiB private stack and requests
a 4 KiB program stack (included in that MZ minimum). Drawing uses the video
aperture directly, with no heap/full-frame RAM buffer during playback.
The foreground handoffs write **8,985 bytes total**, at most **1,690** in
one patch pass. Diagnostic file I/O happens only after timer/audio cleanup.
Applications compiling only the sound module do not pay for logo/diagnostics.

## Preview labels

The audible MP4 is an ideal host PSG approximation from the unchanged C
player trace: fixed 0.18 gain per voice, no normalization, analog filtering
or speaker model. Its decoded frames confirm white at frame 0, complete
blue lettering at 421, the same blue hold without red at 480, and the red
tagline at 481. A separate silent video contains real native captures with
4:3 correction; frame-RPC sampling is variable-rate and is not an audio or
precise elapsed-time recording. Guest tick/transfer receipts establish timing.
