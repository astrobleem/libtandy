# Stroke-trace visual prototype — unpublished

The previous left-to-right wipe was rejected. This candidate follows pen
routes through T, A, N, D and Y together, with cyan heads, bright-blue
afterglow and settled dark-blue trails. Every pixel is clipped to the
original supplied artwork. The final image is pixel-identical to the prior
native framebuffer; only the path and timing of its appearance change.
The Sega Tune extraction and reusable sound module are unchanged.

This is the requested visual-review step. `tools/trace_tandy_logo.py` builds
the encoded masks and checks them in software. `TRACE.H` is the candidate
asset, **not yet wired into `sms_demo.c`**. The current DOS executable still
has the rejected wipe; do not present it as the new motion. No new native
test cycle or repository publication has been performed for this prototype.

## Palette technique and its actual limit

One fixed 4bpp image can give at most 16 distinct per-pixel color histories
across palette-only animation. It cannot hold 16 arbitrary binary frames;
arbitrary F-frame membership can require 2^F histories. The complete proposed
trace actually needs **54 distinct histories**, so it cannot fit in one
unchanging indexed bitmap.

This candidate uses **four masks**, each with 13 palette-driven progress
steps, for **52 trace positions**. Index 0 holds completed blue, 1..13 are
the active stroke groups, 14 is the tagline, and 15 is white background.
The four masks require **14, 15, 15 and 16 distinct histories**, respectively.
The generator verifies every reconstructed subframe pixel against its
independent arrival map, and verifies all three bitmap handoffs preserve
the visible RGB image. It does not claim unlimited independent states.

At a handoff, settle active colors to blue, rewrite their pixel indices to
permanent blue 0, reset the now-unused active palette entries to white, then
install the next encoded mask while its new strokes are still white. These
are three invisible handoffs, rather than full bitmap redraws on every
animation frame. The static RLE data occupies **28,462 bytes**, plus eight
bytes of phase offsets. A straightforward implementation scans the 19,584
byte logo rectangle twice per handoff (117,504 bytes across all three).
The eventual DOS foreground cost and phase-readiness deadlines still need
measurement; no timing qualification is claimed by this software preview.

The 52 steps use cumulative ceil deadlines on the existing sound service
clock. The last step completes the lettering, settles its highlights and
reveals the red tagline at **tick 481 / 2.004331 seconds**, the same final
musical attack as before. The MP4 is rendered at that service rate with the
unchanged audio from the actual C player's PSG trace. Decoded-frame checks
confirm white at frame 0, no red tagline at 480, and the complete logo plus
tagline at 481. It is a host approximation, not captured native audio.

## Reference inspected read-only

Chad's `C:\Users\chad\Downloads\sonic3dblast.bin` is 4,194,304 bytes,
SHA256 `2de28ad6e4b084fb41e4a6ba2a19fceba675ea154c770630b0d5ffea4ce31aa0`.
Its Genesis header has serial `GM MK-1844 -00`, `(C)SEGA 1996.SEP`, and a
matching stored/computed checksum `A934`. There are 167 plausible RNC1
headers. The logo-animation offsets were not identified, and no Sonic
artwork or ROM bytes were copied into this example.

The supplied [GameHut video](https://www.youtube.com/watch?v=c-aQvP7CUAI)
could not be retrieved: web requests failed/throttled and the browser
inventory was empty. The candidate adapts the user-described idea; it does
not claim a verified recreation of that video's exact implementation.
