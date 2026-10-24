# DOS platform services: pages, held keys, update clock, memory

Four small Open Watcom 16-bit DOS modules for a foreground Tandy 1000 game,
plus portable pieces with host tests. They came out of the native JAWS port
(first-party code, relicensed MIT for this library); no game data or logic
is included. Each module owns one resource, opens it explicitly, and gives it
back on close, also after a failed open.

| Header | Resource | Code (8088, `-os`) | Data |
| --- | --- | --- | --- |
| `lt_page_dos.h` | second video page, page register | 1,157 bytes | 1 byte + the `ltpg_pages` object (about 40 bytes) |
| `lt_keys_dos.h` + `lt_keys.h` | IRQ1 | 406 + 464 bytes | 58 bytes |
| `lt_tick_dos.h` | IRQ0 / PIT channel 0 | 839 bytes | 3,112 bytes (3 KiB private stack) |
| `lt_mem_dos.h` | DOS memory blocks | 413 bytes | the `ltmem_pool` object (108 bytes) |
| `lt_gfx.h` additions (`lt_gfx_rows.c`) | (none) | 551 bytes | none |

## Two pages (`lt_page_dos.h`)

Presentation is a **hardware page flip**, not a copy: the CRT field of the
Tandy page register (port 3DFh) switches between two 32 KiB frames during
vertical blank. Drawing goes to the hidden frame through its physical
segment; the CPU window at B800h stays on page 6.

Assumptions: Tandy 1000 EX/HX/SX/TX-class video (page register 3DFh with
bits 6-7 = 11 for the 32 KiB modes 9/10; the BIOS copy at 0040:008Ah; status
3DAh bit 3 = vertical sync; video memory in the top 128 KiB of RAM). The
location of page 4 is found by a signature written through B800h, so the
amount of installed RAM is not assumed. Not supported: CGA/EGA/VGA, the
original 1000/1000A page layout if it differs, or a display in another
mode.

Memory: page 4, page 5 and the part of page 6 below the end of the DOS arena
are ordinary DOS memory on a 640 KiB machine. `ltpg_open` takes it with a
last-fit allocation so DOS cannot give it to anyone else; with the arena
ending at 624 KiB that is **48 KiB** (`ltpg_reserved_bytes`). Open the pages
right after setting mode 9 and before other allocations, while the top of the
arena is still free. On failure the object describes single-buffered drawing
at B800h, so a program can continue with one page.

`ltpg_present` waits for the next refresh first. With a fine clock (for
example `lttk_now` and a period of 19,912 PIT cycles for 59.92 Hz) it
predicts the sync pulse, which is short and often hidden while an IRQ0
update runs; without one it polls status bit 3 a bounded number of times.

## Held keys (`lt_keys.h`, `lt_keys_dos.h`)

`ltkb_open` installs an IRQ1 handler that feeds every scan byte into a
decoder, so any number of keys can be held and every press and release
between two polls is reported (`held`, `pressed`, `released`;
`ltkb_active` = held or tapped since the last poll). Enhanced-keyboard gray
keys report the keypad code; fake shifts and Pause are dropped. Tandy 1000
keyboards send their own codes for some keys (arrows 29h/2Bh/4Ah/4Eh), so
games map both sets.

By default the BIOS handler does not run while the service is open (no
buffer fill, beeps, Ctrl-Break or Ctrl-Alt-Del). Close restores the previous
vector (only if ours is still installed), clears the BIOS shift/Ctrl/Alt
state the BIOS could not track, and empties its buffer, so the DOS keyboard
works normally afterwards. `LTKB_CHAIN_BIOS` keeps the BIOS informed instead.

## Update clock (`lt_tick_dos.h`)

`lttk_open(divisor, work, context)` programs PIT channel 0 (mode 2) and
installs the one IRQ0 handler. The rate is 1,193,182 / divisor Hz:
`LTTK_DIVISOR_60HZ` (19,886) gives 60.0001 Hz, `LTTK_DIVISOR_NES` (19,854)
60.0988 Hz. The handler chains to the previous IRQ0 handler each time
65,536 PIT cycles have accumulated, so the BIOS day counter and INT 1Ch keep
their 18.2065 Hz average (each BIOS tick at most one update late);
`lt_time_dos.h` keeps working.

`work` runs once per tick on a private 3 KiB stack with interrupts enabled.
A tick that arrives while it runs is counted and run immediately afterwards
in order (`lttk_late`, `lttk_backlog_max`). Put updates (input sampling, game
logic, `lt_sfx_tick`, score service) in the callback and draw in the
foreground: logic speed is independent of drawing speed. libtandy's audio
installs no timer, so this is the only IRQ0 handler; service audio from the
callback, or bracket foreground audio calls with `lttk_defer(1)` /
`lttk_defer(0)` (held ticks run at the next tick). `lttk_now` returns PIT
cycles since open for fine timing. Close restores PIT mode 3 / 65,536 and the
previous vector.

## Memory (`lt_mem_dos.h`)

`ltmem_launch_paras` reports the memory the program found at start (its own
block plus the largest free block). Compare it with the program's own
measured requirement before changing the video mode; the library holds no
requirement. `ltmem_alloc` records DOS blocks in a pool and
`ltmem_release_all` frees them on every exit path; `peak_paras` gives the
memory cost to report.

Measuring a requirement: `tools/eatlow.asm` (nasm) builds `EATLOW.COM`,
which leaves only N KiB as the largest free block and runs a program. The
program receives less than N KiB: DOS first takes the child's copy of the
environment (rounded up to paragraphs) and one 16-byte block header for each
of the environment and the program out of that block. Read the program's own
launch figure, not N.

## Graphics additions (`lt_gfx.h`)

`lt_gfx_row` (address of a row in the four-bank layout), `lt_gfx_copy_rows`
(restore a dirty box from another page or a background frame),
`lt_gfx_blit_packed` (linear packed image into the frame) and
`lt_gfx_copy_wrapped` (wrapped horizontal scroll of a row, 2-pixel steps).
Portable C89 in `src/lt_gfx_rows.c`, host tested; `_fmemcpy` on 16-bit
Watcom.

## Startup and shutdown order

```c
lt_dos_app_init(&app);          /* mode 9, sound muted */
ltpg_open(&pages);              /* before any other allocation */
ltmem_alloc(&pool, ...);        /* assets */
ltkb_open(0);
lttk_open(LTTK_DIVISOR_60HZ, update, &game);
lttk_run(1);
/* ... foreground draws into pages.back, ltpg_present ... */
lttk_close();                   /* reverse order; each is safe if its */
ltkb_close();                   /* open failed or never happened      */
ltmem_release_all(&pool);
ltpg_close(&pages);
lt_dos_app_shutdown(&app);
```

`examples/page_demo.c`, `keys_demo.c` and `tick_demo.c` (PAGEDEMO, KEYDEMO,
TICKDEMO) show each service on its own with original graphics and sound.
