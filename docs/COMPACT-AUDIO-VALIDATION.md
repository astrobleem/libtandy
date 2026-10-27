# Compact audio validation

Measured against main `06a1e4b`, using Open Watcom 1.9 with the repository's
`-0 -ms -ecc -os -s -w4 -wx` DOS build, and the managed GCC UCRT64 wrapper
with `-std=c89 -pedantic-errors -Wall -Wextra -Werror -O2` on the host.

## Resource costs

OMF segment lengths, measured by `tools/measure_audio_omf.py`:

| Module | Main code bytes | New code bytes | Added code |
| --- | ---: | ---: | ---: |
| `lt_audio` | 4505 | 5135 | 630 |
| `lt_sfx` | 1778 | 2014 | 236 |
| `lt_stream` (optional) | 0 | 2083 | 2083 |

All three have zero static data/constant segment bytes. Added module code
totals 2949 bytes when all three are linked; stream code is omitted by simply
not compiling it. The attenuation/pause changes cost 630 code bytes in the
existing audio module; they are not behind a preprocessor flag. These are
actual module segment lengths, excluding shared C runtime/linker padding.
They are not `.obj` container sizes or a universal application EXE delta.

| Caller-owned object | Main DOS bytes | New DOS bytes |
| --- | ---: | ---: |
| `lta_handle` | 58 | 66 |
| `lt_sfx_player` | 8 | 12 |
| `lt_sfx_clip` | 18 | 18 |
| `lt_stream` descriptor | absent | 20 |
| `lt_stream_player` | absent | 20 |
| Reference cursor (included in player) | absent | 10 |

The host ABI sizes are 72, 24, 32 and 32 bytes for the handle, clip player,
stream descriptor and stream player respectively. DOS state sizes were
executed on the emulated 8088; baseline sizes were emitted by the same Watcom
compiler using main's headers. No heap, whole-song buffer or module globals
were added. Per-call stack costs are not separately measured; the DOS test
and demonstration use the repository's 4096-byte stack setting.

## Original asset sizes

| Asset | Literal stream | Referenced stream | With 20-byte DOS descriptor |
| --- | ---: | ---: | ---: |
| Two-note chirp (six ticks) | 15 | 15 | 35 |
| Four-note phrase repeated eight times (480 ticks) | 225 | 35 | 245 literal / 55 referenced |

The chirp also fits the existing clip representation: 12 event bytes plus
its 18-byte DOS descriptor = 30 bytes. The stream is five bytes larger for
this short cue; compression is not universally beneficial. The repeated
phrase saves 190 data bytes (84.4% of the stream alone, 77.6% including its
descriptor). The two players each have their own separate state cost above.
Generated C headers, source JSON, plain comparison header and measured OMF
records are in `examples/stream`.

## Native playback cost

A private Tandy 1000 HX 8088 emulator configured at 4,772,728 Hz executed the
DOS tests and `STRMBEN.EXE`. The shared emulator lease was acquired normally;
only copied private disks and the run's own child process were used. The
benchmark latches the existing BIOS PIT counter without changing its mode,
rate or interrupt handler. It measures tick calls with a counting sink,
including ownership/shadow work; disk output and start/validation are outside
the timed intervals. Real PSG port transport and acoustics are not represented
by this sink. BIOS interrupts remain enabled and may affect samples.

| Case | Calls | Total PIT clocks | Mean microseconds/call after probe subtraction |
| --- | ---: | ---: | ---: |
| Short chirp, stream effect | 70 | 159956 | 1870.1 |
| Same chirp, existing clip effect | 70 | 142046 | 1655.6 |
| Literal phrase, music | 4810 | 3439130 | 554.2 |
| Referenced phrase, music | 4810 | 3466592 | 559.0 |

The empty measurement probe averaged 53.738 PIT clocks across 1000 calls.
Conversion uses 1,193,182 PIT clocks/second. Each asset was played ten times.
The referenced phrase adds about 4.8 microseconds per tick relative to the
literal stream in this run. The short effect stream costs about 214.4 extra
microseconds per tick relative to the existing clip. These are averages over
the named mixes of event, wait and ending ticks, not worst-case IRQ deadlines
or hardware timing guarantees. The hard playback bound is 64 commands/tick.
Foreground validation is deliberately limited to 8192 structural inspections
and 8192 expanded commands; an earlier 65535-step prototype made the deliberate
malformed-bound test unnecessarily slow on the 8088.

## Checks completed

- All repository host tests pass, preserving existing audio, SFX, score,
  approved Doebel and SMS boot traces.
- All repository modules, DOS examples and DOS tests compile with the actual
  8086/8088 Watcom configuration and warnings as errors.
- Native `STREAM`, `AUDIO`, `SFX` and `SCORE` pass. New checks cover generated
  compressed/literal byte-trace equivalence, immediate attenuation and
  saturation, original level preservation, looping effects over progressing
  music, finite/infinite loop policy, priority replacement, boundary cancel,
  immediate stop, paused stop/cancel, frozen waits/reference state, repeated
  silent pause/resume, independent audible layers, global mute/unmute while
  paused, and tone2/noise restoration including complete latch writes.
- Malformed tests cover truncation, alignment, offsets and 16-bit overflow
  boundaries, nested references, zero counts/waits, undeclared channels,
  excessive expanded work, the exact 64-command accepted boundary and runtime
  budget failure. Invalid starts validate before borrowing/output.
- The standalone `STRMDEMO /auto` runs 360 retraces and reports status zero,
  all channels muted/released. A final demo build compares IRQ0 at program
  entry and exit directly; see the final native demo record. The full-suite
  harness's original vector sample was taken before BIOS initialization and
  is not a valid entry/exit comparison.
- Independent review identified an uncapped structural reference scan; it was
  capped before these final checks. A subsequent review found no additional
  concrete reference/arithmetic/playback defect.

Native logs are in `examples/stream/NATIVE.json`; the final demo entry/exit
check is in `examples/stream/DEMO-NATIVE.json`. This is emulated DOS validation,
not a physical Tandy audibility claim. No publishing operation was performed.
