# Far-pointer boundary qualification

`tests/gfx_far_dos.c` tests graphics in allocated DOS RAM, without switching
video modes or touching video memory. It allocates three separate 64-KiB
blocks (source, destination, independent expected image). The test deliberately
uses non-normalized pointers at the highest valid offsets:

* Packed 32-byte tile at `segment:FFE0` and odd `segment:FFDF`.
* Full 32,768-byte surface at `segment:8000` and odd `segment:7FFF`.
* Fast aligned tiles, unaligned rows/columns, clipped edges, wholly rejected
  extreme coordinates, bottom-right pixels, full clears, and odd/even spans.

Every destination comparison covers all 65,536 bytes, so unused bank padding,
pixels outside a primitive, and all available before/after guards are checked.
The entire source block is checked for mutation. At `FFE0`/`8000` there is no
post-buffer guard inside the segment; the window ends exactly at `FFFF`.
The odd-offset cases leave one post-buffer guard byte. This does not authorize
pointers whose buffers cross a 64-KiB offset wrap; those remain out of contract.

Reproduce using installed Linux-host Open Watcom and DOSBox-X (no downloads):

```
python3 tests/gfx_far_matrix.py --watcom /path/to/watcom \
  --dosbox /path/to/dosbox-x
```

An environment-setting executable wrapper may replace the DOSBox-X path.
The runner compiles small, compact, medium and large model executables, with
`-0 -ecc -ox -s -zt=0 -w4 -we`. Each model is tested twice: the native tile
assembly/CRT fill implementation and forced portable C using
`LT_GFX_TILE8_C_ONLY` plus `LT_GFX_C_FILL`. Each executable uses the independent
per-pixel expected-image model rather than comparing the two implementations
only to one another.

Results and source/executable hashes are written under `build/gfx-far-matrix`.
The emulator configuration is Tandy, 640 KiB, no XMS/EMS, `8086_prefetch`, fixed
20,000 cycles. These checks qualify pointer-window arithmetic and byte results,
not physical EX performance, actual video-memory timing, or arbitrary aliasing.

Recorded 2026-11-02 with Open Watcom 2.0 beta 2026-10-01 and DOSBox-X:
**all eight executables passed 64 cases each, zero failures** (512 total
case executions). Data pointers were 2 bytes in small/medium and 4 bytes in
compact/large; framebuffer/source pointers are explicitly far in every model.
