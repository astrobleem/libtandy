# Validation and cost

Built on libtandy main `a5dd8b815742991e7629b8cde269ee8de6ab5f9b`.
No existing sound/graphics driver or MIDI-bank behavior is changed.

- Managed GCC C89 host suite passes, including all previous tests and a new
  reference check of every one of the 710 PSG bytes at its scheduled tick.
  It also tests silent initialization, atomic ownership conflict, bounded
  work, restart, early cancellation, idle stop, final mute/release and cache
  invalidation before subsequent low-level use.
- Open Watcom `-0 -ms -ecc -os -s -w4 -wx` DOS build passes, including
  `SMSBOOT.EXE`, `TANDYLOG.EXE`, and `smscheck.EXE`. Test/demo output names
  are checked for collisions on the case-insensitive DOS filesystem.
- Original Library archive, project VGM and both v0.13 assembly tables
  agree on all 710 bytes. The optional extraction checker passes.
- A short emulated Tandy HX session passed sound completion, progressive
  logo/hold, Escape, mute/release and restoration of IRQ0 and BIOS clock.
  All post-return samples advanced six BIOS ticks in about 0.31 seconds.
  The real shared emulator lease was acquired and released, and only the
  owned child was controlled. Early/middle/final native frames were captured.
  That run used light-red palette 12; the final example changes only the
  tagline to palette 4 for a closer saturated red. Its DOS rebuild passes;
  the final-color native retry could not acquire the busy shared lease and
  launched no emulator. No physical-hardware or captured-audio qualification.

| Added component | Cost |
| --- | ---: |
| Sound stream: 119 deadline groups / 710 PSG bytes | 1,067 constant bytes |
| Optional effect module, Watcom 8086 small-model text | 449 bytes |
| Caller-owned effect player, DOS small model | 8 bytes |
| Mutable static state added by effect module | 0 bytes |
| Source-derived logo RLE | 7,828 constant bytes |
| Sound-only DOS executable | 8,956 bytes |
| Logo DOS executable | 22,710 bytes |

The effect also uses the caller's existing `lt_chip`. The DOS example uses
the existing timer adapter's 3 KiB private stack and requests a 4 KiB program
stack. Logo conversion expands into the existing video aperture before
playback, with no heap or full-frame RAM buffer. There is no cost to an
application that does not compile/call this optional module.

The review package includes a WAV synthesized from the actual C player's
PSG output, a synchronized HTML preview, GIF, and PNG. These are host
approximations with ideal square waves and Tandy-style 15-bit noise, fixed
0.18 gain per voice, no normalization, analog filtering or speaker model.
The HTML follows audio playback time. GIF timing rounds to 10 ms and has
no audio. Preview output is not human listening acceptance or native audio.
