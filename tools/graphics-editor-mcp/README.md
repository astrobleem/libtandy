# Optional local MCP interface

The browser editor in [`../graphics-editor`](../graphics-editor/README.md) remains standalone: open `index.html` and no Node install or MCP connection is needed. This separate, optional package lets an MCP-compatible assistant convert local pictures and export libtandy assets through the same `core.js` conversion and encoding functions used by the editor.

The server uses [MCP's official TypeScript SDK v2](https://github.com/modelcontextprotocol/typescript-sdk#readme) over **stdio**. It does not open a network port, call a hosted service, upload image files, invoke a shell, or fetch URLs. It supports PNG and JPEG input, including JPEG EXIF orientation; use the browser editor for WebP, SVG, previews, and hands-on pixel editing.

## Install and connect

Requirements: Node.js 20 or later and an MCP host that can launch a local stdio server.

Install only this optional package from a checkout of the repository:

```powershell
cd tools/graphics-editor-mcp
npm ci
```

In your MCP host's server configuration, point `command` to your Node executable and pass the server script and an **existing folder** that contains the files you want to use. For example:

```json
{
  "mcpServers": {
    "libtandy-graphics": {
      "command": "node",
      "args": [
        "C:/src/libtandy/tools/graphics-editor-mcp/server.mjs",
        "--workdir",
        "C:/games/my-project/assets"
      ]
    }
  }
}
```

Use your own absolute paths in this host configuration. Tool calls then name files **relative to that folder**. The server rejects absolute paths, `..` path segments, and paths that resolve outside the folder. It reads only the input files named in a call. Output files must be new; the server refuses to overwrite an existing file. Output parent folders must already exist.

For a direct local launch while diagnosing the connection:

```powershell
node server.mjs --workdir "C:\games\my-project\assets"
```

It waits for MCP protocol messages on standard input and sends protocol messages on standard output. Diagnostics go to standard error.

## Available tools

### `convert_image`

Read a local PNG or JPEG, resize or split a horizontal sheet, convert to the actual 16-color Tandy palette, and write an editable `.tandy.json` project. Single images can use `fit`, `crop`, or `stretch`; automatic resampling uses nearest-neighbor for detected pixel art and alpha-aware bilinear sampling for other images. Sprite imports automatically choose three visible Tandy colors unless `sprite_colors` is provided to lock them.

The conversion options include `dither` (`auto`, `none`, `fs`, or `ordered`), `dither_strength`, `recommended_dither`, `pixel_art`, automatic tone correction, exposure, contrast, saturation, gamma, alpha threshold, and optional background color keying. `auto` uses the same editor core and its photo/illustration/pixel-art recommendations. Pass `frame_count` for an equal-width horizontal sheet; each cell must already be 8x8 for sprites or 320x200 for screens.

Example for a four-frame 8x8 sprite strip; the server suggests the palette:

```json
{
  "input_path": "player-strip.png",
  "output_path": "player.tandy.json",
  "mode": "sprite",
  "frame_count": 4,
  "dither": "auto"
}
```

### `inspect_project`

Validate a project and report its mode, dimensions, frame count, transparent-pixel count, and opaque-black-pixel count. This is useful for checking transparency and sprite palettes before export.

### `export_asset`

Read an editable project and write one of:

- `project-json`: another editable `.tandy.json` copy.
- `sprite-c-header`: a libtandy sprite C header containing every frame and the shared palette.
- `screen-c-header`: a libtandy mode-9 C header for one selected screen frame.
- `mode9-bin`: the exact 32,768-byte packed mode-9 screen for one selected frame.

Example:

```json
{
  "project_path": "player.tandy.json",
  "output_path": "player_sprite.h",
  "format": "sprite-c-header"
}
```

For screen export, choose `screen-c-header` or `mode9-bin`; the optional `frame_index` starts at zero. A screen has no transparency in the packed hardware buffer, so transparent pixels use the project's `screenBackground` color when exported.

### `import_mode9`

Read an existing raw mode-9 `.bin` file that is exactly 32,768 bytes and write an editable screen project.

### `import_sprite_planes`

Import one libtandy 8x8 sprite from two arrays of eight MSB-first plane bytes plus three RGBI palette colors. This can reopen an existing sprite as an editable project. Index 0 remains transparent; a visible sprite index mapped to RGBI black stays opaque.

Example:

```json
{
  "plane0": [128, 0, 0, 0, 0, 0, 0, 0],
  "plane1": [0, 0, 0, 0, 0, 0, 0, 0],
  "palette": [0, 14, 15],
  "output_path": "imported-sprite.tandy.json"
}
```

## File and size limits

- PNG or JPEG, at most 10 MiB, 4,096 pixels per side, and 1,048,576 decoded pixels total.
- Projects at most 8 MiB and 1,048,576 pixels total: up to 16 screen frames or 64 sprite frames.
- Tool file paths are relative to the configured working folder. Parent folders for new outputs must already exist.
- Existing output files are never replaced. Choose a new output path for each export.
- The project stores palette indices and alpha separately. Screen index 0 can be opaque black; sprite index 0 is transparent, while nonzero sprite indices mapped to black remain opaque.

## Developer checks

From this folder, run the protocol, path-boundary, image-decoding, frame-sheet, alpha, and core-export parity checks with:

```powershell
node --test test/*.test.mjs
```

The protocol test starts the actual stdio server and talks to it through the official MCP client. It compares generated C headers and packed mode-9 bytes directly with `tools/graphics-editor/core.js`. The existing editor core and generated-C checks can also be run from `../graphics-editor` with `node tests/core.test.cjs`.

The dependency install applies only to this optional MCP package. The browser editor has no server/account requirement and does not load this package.
