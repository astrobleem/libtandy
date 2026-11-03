# Packed 8x8 tiles

`include/lt_gfx_tile8.h` / `src/lt_gfx_tile8.c` add a small optional primitive
for independently positioned opaque tiles. They do not introduce a tile engine,
emulated device, framebuffer cache, allocation, interrupt, or hardware owner.

```c
#include "lt_gfx_tile8.h"
/* 32 caller-owned FAR bytes: eight rows, four packed RGBI bytes each. */
lt_gfx_tile8(&surface, x, y, packed_tile);
```

The left pixel is the high nibble. Every color, including zero, is opaque.
Signed-16 coordinates clip safely, including extremes. Null arguments return
zero; valid arguments return one even for a wholly clipped tile. Source and
surface descriptor must not alias destination storage. Source must fit one
valid 32-byte far-pointer window; destination has the normal complete 32 KiB
mode-9 surface contract.

## Target optimization and choices

For an in-bounds tile at an even x and y divisible by four, Open Watcom's
8088 build uses sixteen `MOVSW` instructions across the four video banks.
Other positions use portable clipped pixel composition. Define
`LT_GFX_TILE8_C_ONLY` to disable the assembly path without changing behavior.
The kernel preserves DS/ES and follows the C ABI's clear direction flag.
Generated listings and native runs verify Watcom's nonuniform register-pair
allocation; the source pointer is segment DX / offset SI, but destination is
offset AX / segment DI. This is not guessed from the pragma's textual order.

Prefer the existing `lt_gfx_blit_packed` for larger contiguous rectangles or
batched rows. Tile8 is not a universal replacement for that API. A framebuffer
or artwork cache belongs to the caller. No automatic sprite transparency or
background restoration is supplied; `lt_gfx_sprite8_planes` remains the separate
2-bitplane transparent-sprite operation.

## Evidence (2026-11-02)

`tests/gfx_tile8.c` passes 10,224 draws on strict C89 host and ASan/UBSan builds
and in native DOS small, compact and large memory models. Both the target ASM
and forced-C builds pass. Tests cover every aligned fast-path start, all source
byte values, all destination parity/bank phases, signed extremes, clipping,
opaque-zero behavior, null arguments, and source/framebuffer guards. The oracle
uses independent long-coordinate pixel addressing rather than the copy kernel.

The target flags used for the focused matrix are
`-0 -ms|-mc|-ml -ecc -bt=dos -ox -s -zt=0 -w4 -we -k8192`.
The usual aggregate builds also run the test with their `-os` settings.

`tests/tile8_bench.c` compares fixed in-RAM aligned 8x8 copies against the
existing generic packed blit. It resets the buffer and verifies the same final
FNV hash (`dcf7e385`) for both methods. DOSBox-X 2025.02.01, Tandy/640 KiB,
normal core / 8086_prefetch, native large model:

| Fixed cycles | Copies | Packed blit ms | Tile8 ms | Reduction |
|---:|---:|---:|---:|---:|
| 20,000 | 200,000 | 5,163 | 1,318 | 74.5% |
| 720 | 20,000 | 14,610 | 3,845 | 73.7% |

These are elapsed DOS `clock()` measurements with roughly 55 ms granularity.
They are an instruction-profile-relative **RAM-buffer** comparison, not a
physical Tandy EX frame rate, VRAM-contention benchmark, or guarantee for
unaligned tiles. The source has no private data/table allocation; the measured
large-model `-ox` routine is 424 bytes. Compiler flags/models may change that.

## Reproduce

The normal `tools/build.py` host/DOS suites include `tile8`; DOS also builds
`TILEBEN.EXE`. Run `tools/qualify_dos.py --program tile8` against an installed
DOSBox-X. Use `--model large` consistently on both build and qualification to
check that model. For the forced-C comparison compile the same source/test
pair with `-dLT_GFX_TILE8_C_ONLY`; no other source changes are needed. Benchmark
`LT_TILE8_BENCH_FRAMES` defaults to 200; define it as 20 for the low-cycle run.

The new primitive is independently authored generic code. It includes no
application-specific effects, music, graphics or conversion tools.

High/odd far offsets and the framebuffer window contract are additionally
qualified in [GFX-FAR-QUALIFICATION.md](GFX-FAR-QUALIFICATION.md).
