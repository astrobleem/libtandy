# Graphics Workshop for libtandy

**[Open the web app](https://astrobleem.github.io/libtandy/)** — no installation required.

A small, dependency-free browser editor for Tandy mode-9 screens and libtandy's 8×8 sprites. It runs in a current browser and is also available for local use. Imported pictures and work stay in the browser; the editor makes no network requests and does not upload files.

## Start here

1. Open the web app above, or open `index.html` in a current Chrome, Edge, Firefox, or Safari browser to run it locally. If your browser blocks local files, start the optional local server below.
2. Choose **Sprite · 8 × 8** or **Full screen · 320 × 200**.
3. Click **Import picture**. PNG, JPEG, WebP, SVG, saved `.tandy.json` projects, and the editor's raw 32,768-byte mode-9 `.bin` export are supported. You can also choose **Try the example** to edit the built-in fox.
4. Choose **Fit**, **Fill & crop**, or **Stretch** and adjust the live color preview. The before and after samples are shown together.
5. Pick a tool, then click or drag on the large canvas. Press `P`, `E`, `F`, or `I` for pencil, eraser, fill, or eyedropper. `Ctrl+Z` and `Ctrl+Y` undo and redo.
6. Click **Save project** to download an editable project. Use the PNG and C-header buttons when you are ready to use your art.

### Optional local server

From the repository folder, run:

```powershell
python -m http.server 8000 --directory tools/graphics-editor
```

Then open <http://127.0.0.1:8000>. The server is only on your computer; no account, build step, package install, or internet connection is needed after the files are present.

## Friendly image conversion

**Automatic** is selected by default. The editor samples the imported image locally and makes a small, explainable choice:

| Detected picture | Automatic resizing and palette conversion |
| --- | --- |
| Low-color pixel art | Nearest-neighbor resizing, no dithering, and no automatic tone correction. |
| Photo-like image | High-quality browser resizing, gentle auto tone correction, and serpentine Floyd–Steinberg error diffusion. |
| Illustration or gradients | High-quality browser resizing, gentle auto tone correction, and ordered 8×8 Bayer dithering. |

This is a heuristic, not a claim that one method is best for every picture. Choose **No dithering**, **Floyd–Steinberg**, or **Bayer 8 × 8** whenever you prefer. The dither and correction strengths are adjustable. Open **Adjust color by hand** for exposure, contrast, saturation, and gamma controls. Manual settings remain available for pixel art; Automatic mode only skips its own smoothing, dithering, and tone correction when it detects low-color pixel art.

Color distance is measured in OKLab after decoding sRGB into linear light. Dither error is spread in linear RGB. Transparent input pixels are excluded from tone correction and error diffusion so their colors cannot leak into nearby visible pixels. The RGBI values and RGBI color order come directly from libtandy's `src/lt_media.c`.

The palette has a separate **T · Transparent** choice. RGBI color 0 is ordinary black. A visible black pixel in screen art remains an opaque color-index-0 pixel. A visible black sprite pixel uses sprite index 1, 2, or 3 mapped to RGBI black; only sprite index 0 is transparent.

You can change the preview background between a checkerboard, warm paper, dark, and blue. The screen exporter fills transparent screen pixels with the selected solid background color because the mode-9 framebuffer itself has no alpha channel.

For sprite imports, the editor evaluates the visible pixels across all frames and suggests three RGBI inks that minimize total OKLab color error. Turn on **Lock these ink colors** to keep your own palette; picking a palette color turns the lock on for you. Transparency remains separate from the three inks.

### Supported sample pictures

Open these files from `examples/` with **Import picture**:

- `photo-like-landscape.svg` — an original, locally created tonal and textured sample for photo-style conversion. It is procedural artwork, not a camera photograph.
- `sunset-town.svg` — an original vector illustration with gradients and flat edges.
- `pixel-sprites-4x8.svg` — four hard-edged 8×8 frames in a horizontal strip. Turn on **Split a horizontal sprite sheet** before converting.
- `little-fox.tandy.json` — the built-in fox example as an editable project file.

## Files you can save

- **PNG picture** saves the current frame with transparency.
- **PNG frame sheet** places the project's frames side by side.
- **Editable project JSON** stores all frame pixel indices, alpha, artwork type, and palette choices. Open it later with **Open project**.
- **C header for libtandy** emits code for the selected artwork type, including dimensions, exact array lengths, and palette information. A screen header contains one writable `lt_gfx_u8[LT_GFX_BYTES]` buffer. A sprite header contains one two-plane 8-row set per frame and a shared four-byte palette.
- **Mode-9 screen bytes** emits a raw 32,768-byte file only in Full screen mode. It is exactly the 32 KiB packed four-bank layout consumed by libtandy's graphics core.

The generated C is intended to be included next to the matching libtandy headers and compiled with the corresponding source. It uses `lt_gfx_init` for a screen and `lt_gfx_sprite8_planes` for sprites. The in-repository conversion test compiles sample generated headers against `include/lt_gfx.h`, `include/lt_gfx_sprite8.h`, `src/lt_gfx.c`, and `src/lt_gfx_sprite8.c` with Open Watcom C32.

### Mode-9 details (Advanced)

The screen is 320×200. Each byte stores two 4-bit colors: even `x` goes in the high nibble and odd `x` in the low nibble. Scanlines are divided into four 8,192-byte banks. The visible address formula used by `src/lt_gfx.c` is:

```text
(y & 3) * 8192 + (y >> 2) * 160 + (x >> 1)
```

The 320 visible pixels in each row use 160 bytes; the rest of each bank is padding. The C header's array length is exactly `LT_GFX_BYTES` (32,768). The raw binary has the same layout.

### Sprite details (Advanced)

The library accepts exactly one 8×8 sprite per API call. `plane0[8]` carries index bit 0; `plane1[8]` carries index bit 1. In each byte, the most-significant bit is the leftmost pixel. Index 0 leaves the destination unchanged. The four-byte palette's entry 0 is ignored; entries 1–3 map nontransparent sprite indices to RGBI colors 0–15. Mapping one of those entries to RGBI black is valid and still draws an opaque pixel.

For a generated sprite named `little-fox`, the exported header supplies `little_fox_plane0`, `little_fox_plane1`, and `little_fox_palette`. A typical call is:

```c
#include "little_fox_sprite.h"

/* 'screen' is an lt_gfx_surface initialized with a full 32 KiB mode-9 buffer. */
lt_gfx_sprite8_planes(&screen, 100, 80,
                      little_fox_plane0[0], little_fox_plane1[0],
                      little_fox_palette);
```

For animation, select the matching row in both plane arrays. The editor does not start a game loop or program video hardware.

## Checks

Run the conversion and export checks from this folder:

```powershell
node tests/core.test.cjs
```

The checks cover all 16 RGBI colors, transparent versus opaque black, sprite planes, nibble order, bank edges, full-screen round-trip, corrected-ramp dithering, sprite palette search, and per-pixel plus 8x8 tone-error comparisons on photo-like, gradient, and pixel-art samples. The generated-C check compiles and runs headers against the actual libtandy core with Open Watcom C32 and, when available, compiles the same output for the 16-bit DOS memory model.

`node tests/ui.test.cjs` is a dependency-free browser smoke test. It uses an already-installed Chrome or Edge, exercises the import-to-export workflow, checks transparency and palette behavior, tests the frame strip and malformed-project handling, and saves page screenshots. Set `CHROME_PATH` only when the browser is installed in a nonstandard location; `LIBTANDY_QA_OUTPUT` can select an output folder. The test keeps the browser's normal sandbox enabled and requests software rendering for headless capture.

## Notes

- Only indexed RGBI pixels and transparency are saved in the project. The original source image is not embedded.
- The sprite API is 8×8 and supports three visible ink indices plus transparency. Other sprite sizes can still be edited as screens, but the sprite C export uses 8×8 frames.
- Multiple screen frames are retained in JSON and PNG sheets. Mode-9 binary and C data exports the currently selected screen frame.
- Photos contain more source detail than the fixed 16-color palette can preserve. Compare the before/after sample and try the two dither styles; image content and preference determine which looks better.

## Conversion references

- [OKLab reference conversion](https://bottosson.github.io/posts/oklab/) by Björn Ottosson, including linear-sRGB conversion equations.
- [W3C CSS Color Module Level 4](https://www.w3.org/TR/css-color-4/) documents the web color spaces and sRGB handling.
- The Floyd–Steinberg 7/16, 3/16, 5/16, 1/16 error weights originate in R. W. Floyd and L. Steinberg's 1976 spatial gray-scale work; see this [University of Wisconsin graphics course note](https://research.cs.wisc.edu/graphics/Courses/559-f2004/docs/floyd-steinberg.pdf) for the standard kernel.
- The ordered 8×8 threshold matrix is the standard Bayer matrix described in Bryce Bayer's 1973 work on ordered dither; a [Bayer matrix reference](https://en.wikipedia.org/wiki/Ordered_dithering) shows the normalized threshold map and its operation.
