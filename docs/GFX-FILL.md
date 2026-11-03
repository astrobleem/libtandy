# Packed span and clear acceleration

On Open Watcom16-bit, `lt_gfx_clear()` and the byte-aligned middle of spans now use the compiler runtime's `_fmemset` far-byte fill. Odd left/right nibbles and clipping retain their original behavior. Short spans under8 complete bytes retain the inline C loop to avoid setup overhead. `-dLT_GFX_C_FILL` forces the portable baseline for comparisons. No new API, cache, heap, mutable global state, or hardware instruction is required by callers.

`tests/gfx_fill.c` compares the entire32KiB surface plus guards against an independent per-pixel rectangle oracle: all256 input clear colors,128 mixed-edge rectangles and9 extreme/clipped/empty cases. All393 cases pass strict hostC89, ASan/UBSan, and native8088 small/compact/large models. Existing graphics/line/clip tests remain part of `tools/build.py`.

`tests/gfx_fill_bench.c` is a RAM benchmark; compile once normally and once with `-dLT_GFX_C_FILL`. At DOSBox-X2025.02.01 fixed20,000cycles,100 operations:

| Operation | C baseline | Far fill |
|---|---:|---:|
| Full32KiB clear |1153ms|54ms|
| Clipped filled rectangle|1538ms|275ms|

Both produce framebuffer FNV`803b1dc5`. DOS `clock()` reports milliseconds but has roughly55ms granularity; the54ms measurement is near one clock tick. These figures show a large reduction in loop overhead, **not** calibrated8088 speed, VRAM-contention behavior or physical EX frame rate. Raw logs are under `build/fill/`.
