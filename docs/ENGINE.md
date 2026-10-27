# Optional foreground game engine

`lt_engine.h` adds small C89 game-loop conveniences without changing any
existing API. Compile `src/lt_engine.c` only when wanted. It owns no heap,
clock, interrupt, sound channel or display. Caller-owned object slots hold a
behavior table and user pointer; the user pointer can refer to any game data,
including a physics body. Simulation units need not be pixels.

## Lifecycle and time

```c
static lte_object slots[8];
static lte_world game;
static const lte_behavior actor = { awake, start, fixed, update, late };
/* Each callback: void callback(lte_world *w, lte_object *o). */
lte_init(&game, slots, 8, 10, 4);
lte_add(&game, &actor, &my_actor_data);
/* Foreground, once per drawn frame; time units are chosen by the caller. */
lte_frame(&game, elapsed_since_previous_frame);
```

The period here is 10 caller time units. Each frame adds elapsed time to
`game.debt`, then runs at most four complete fixed passes. For each pass:

1. Reclaim removed slots and snapshot pending objects.
2. Call all pending **Awake**, in slot order.
3. Call all surviving pending **Start**, in slot order.
4. Call all active **FixedUpdate**, in slot order, then increment `game.ticks`.

After the fixed passes, call all active **Update**, then all active
**LateUpdate**, once each in slot order. Awake/Start run exactly once per
surviving object, before its first FixedUpdate. Removing an object during
Awake suppresses its Start. Zero-step frames run Update/LateUpdate
on existing active objects but do not activate or reclaim objects. Keeping
lifecycle transitions at fixed-tick boundaries makes simulation spawn chains
independent of rendering cadence, including extra zero-elapsed frames.

Creation inside any callback becomes eligible at the next fixed-tick boundary,
never halfway through the current pass. This includes creation inside
Awake/Start. Removal immediately suppresses all later callbacks; the dead
slot cannot be reused until the next fixed-tick boundary. `lte_add` returns
NULL when storage is full. Never retain an object pointer after removal:
slots are reusable and pointers are not generation handles. Behavior tables,
user data and the slot array must outlive their objects. Treat world/object
internals as read-only except your own `user` data. Do not initialize a world
again inside a callback. Reentrant `lte_frame` returns -1.

Fixed callbacks run in the foreground, never from the engine's own ISR.
Slow drawing retains debt and catches up within the budget on later frames;
it never silently changes the fixed period. A 32-bit time accumulator
saturates on overflow and sets sticky `game.saturated`, making lost time
observable. Ticks wrap modulo 2^32. For a pause, stop advancing caller time
or deliberately rebase the caller's clock. The engine does not infer pauses.
Deterministic simulation belongs in FixedUpdate with tick-indexed inputs;
Update/LateUpdate run per rendered frame and are not a deterministic clock.

The callback budget limits the number of passes, each visiting at most the
slot capacity. It cannot bound arbitrary user callback code. A long stall can
leave a visible backlog: choose a period/budget appropriate to the 8088 and
monitor debt. There are no threads, global state or unbounded catch-up loops.
Do not call the engine from an interrupt or concurrently. The existing
`lttk_work` API is unchanged; do not install game callbacks there to drive
this engine. A foreground caller may sample `lttk_ticks()` if it separately
owns that optional clock, or simply use the unchanged BIOS clock as the demos do.

## Collision and camera

`lte_box` is an integer half-open AABB. `lte_overlap` tests object/object
overlap: a shared edge is not a collision. It does not automatically resolve
object pairs or provide rigid-body dynamics, mass, rotation, impulses or a
broad phase. A game chooses its pair list and response.

`lte_move_tiles` resolves X then Y, one world unit at a time, against tile
cells whose masked bits are nonzero. It validates the original body, then
tests each new leading edge, preventing tunneling through a one-cell wall.
Outside the map is solid. It returns `LTE_HIT_X`/`LTE_HIT_Y`; the caller can
stop or reverse velocity, apply gravity, or jump on a downward Y hit. Bodies
already inside solids are rejected unchanged rather than guessed out of them.
Invalid map/body/displacement returns `LTE_MOVE_INVALID` without mutation.

Coordinates are -16384..16383, body sizes 1..1024, displacement -256..256 per
axis per call. Maps have at most 65535 cells and at most 16384 world units
per axis. Cells and video buffers may be far pointers on Watcom, but each
array must remain within one 64 KiB segment; no huge-pointer traversal.
Object storage and behavior/user near pointers must fit the chosen memory
model/DGROUP. No per-object body or sprite is imposed.

Collision work is bounded by the initial overlapped tile rectangle plus
`abs(dx)` leading columns and `abs(dy)` leading rows. A conservative probe
bound is `C*R + abs(dx)*R + abs(dy)*C`, where
`C=ceil((body.w-1)/tile_w)+1`, `R=ceil((body.h-1)/tile_h)+1`.
Typical demo bodies/tiles are 8x8 with displacement 2 horizontally and at
most 6 vertically. Huge bodies on 1-unit tiles remain expensive on an 8088.

`lte_camera_set` clamps a viewport to world bounds, pinning oversized
viewports to zero. `lte_visible_tiles` visits only the visible map cells,
row-major, with camera-relative rectangles. Partially visible edge tiles may
have negative coordinates; the presenter clips them. The map is bounded,
not wrapping. Drawing the visible viewport each frame is software tilemap
scrolling; there is no new hardware panning/page-flip or smooth-frame-rate claim.

## Actual display coverage

`lt_engine_view.h` / `src/lt_engine_view.c` are separate optional presenters.
They bind caller-owned memory and clip colored rectangles; they never set a
mode themselves. Glyphs/attributes are supplied for text rectangles.

| BIOS mode | Native display | Memory layout | Bytes to bind |
| --- | --- | --- | --- |
| 09h | Tandy 320x200, 16 colors | four 8 KiB banks, 160 bytes/row | 32768 |
| 08h | Tandy 160x200, 16 colors | two 8 KiB banks, 80 bytes/row | 16384 |
| 03h | color text 80x25 | character/attribute pairs, 160 bytes/row | 4000 |
| 01h | color text 40x25 | character/attribute pairs, 80 bytes/row | 2000 |

In graphics, even pixels use the high nibble; row bank is `y % banks`,
row within bank is `y / banks`. Padding is untouched. Text presentation
uses character cells, so the demo scales the same 8-unit world tile to one
cell and aligns its camera to 8 units. Physics and gravity remain identical.
Text's coarser display naturally hides fractional-cell movement.

`lt_engine_dos.h` / `adapters/lt_engine_dos.c` optionally save the BIOS
mode/page/cursor, enter one of these modes at page zero, bind B800:0000 and
restore the saved state on close or failed mode verification. Initialize
with `LTE_DOS_VIDEO_INITIALIZER`, keep the object at its original address,
and pair each successful open with close. Repeated close is safe. The caller
asserts compatible Tandy hardware for 08h/09h; the BIOS mode query verifies
selection, not adapter identity. This resets the BIOS palette; it does not
save prior pixels, custom palette or arbitrary registers. Exclusive video
ownership is required. No audio or timer resources are acquired.

The preexisting `lt_gfx` raster remains specific to 320x200 mode 9. The new
160x200 support is an explicit two-bank packed presenter, not a claim that
INDEX8 RAM surfaces select a native mode. CGA 04h/06h, 160x100 text tricks,
Tandy 0Ah, monochrome text and other resolutions have no new adapter here.
The simulation and camera can still be reused with a caller's renderer.

## Ready-to-run DOS demonstrations

Build with the normal Open Watcom helper:

```powershell
python tools/build.py --target dos --watcom C:/WATCOM
```

Python is only a developer build helper. A consumer runs the EXEs in DOS
on compatible hardware without Python, an asset converter or PC preparation.
The sources can also be compiled directly with matching 8088 small-model
flags; see the source list in `tools/build.py`.

```text
wcl -q -bt=dos -0 -ms -ecc -os -s -i=include -k4096 -fe=ENGDEMO.EXE examples/engine_demo.c src/lt_engine.c src/lt_engine_view.c adapters/lt_engine_dos.c src/lt_time.c adapters/lt_time_dos.c
```

```text
ENGDEMO                 Tandy mode 9, scrolling/collision/gravity
ENGDEMO /low            Tandy native mode 8, 160x200
TXTDEMO                 80x25 text, same simulation
TXTDEMO /40             40x25 text, same simulation
```

A/D or arrows set horizontal velocity; Space jumps while grounded; Escape
restores video and quits. This small example uses BIOS press events, not
held keys. No sound is played. The map contains repeating colored landmarks,
a floor and a tall blocking wall. The camera follows the moving body through
the 768-unit world. There is no sprite asset or loading step.

Add `/auto` for an automatic 300-tick route that jumps, hits the wall,
reverses, and prints its collision counts, furthest camera position and
lifecycle counts after returning to DOS. Add `/slow` to wait three BIOS
ticks between foreground frames. Physics still uses one BIOS tick per fixed
step (about 18.2 Hz), with up to four steps per frame. The demo caps its
finite route at exactly 300 ticks even if the caller samples a larger stall.
Direct viewport redraw can tear and will be slow on an 8088; this is a small
correctness/convenience example, not a high-frame-rate platformer benchmark.

Host tests cover ordering, callback removal/addition, nested-frame rejection,
slow-frame debt, deterministic spawn chains with zero-step frames, half-open
collision edges, tunneling, invalid movement, camera bounds and partial
tiles. Presentation tests check native bank addresses, nibble preservation,
clipping and text attributes in both column counts. DOS builds compile those
same tests with native 16-bit types and far pointers.

See [the validation and cost record](ENGINE-VALIDATION.md) for the measured
8088 sizes, native mode runs and their practical limits.
