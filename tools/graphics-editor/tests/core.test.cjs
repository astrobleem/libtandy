/* SPDX-License-Identifier: MIT */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const Core = require("../core.js");
let checks = 0;
function check(condition, message) { checks += 1; assert.ok(condition, message); }
function equal(actual, expected, message) { checks += 1; assert.equal(actual, expected, message); }

/* Exact dimensions, four 8 KiB banks, and even/odd nibble order. */
equal(Core.WIDTH, 320, "mode-9 width");
equal(Core.HEIGHT, 200, "mode-9 height");
equal(Core.BYTES, 32768, "mode-9 byte count");
equal(Core.mode9Offset(0, 0), 0, "bank 0 start");
equal(Core.mode9Offset(319, 0), 159, "bank 0 row end");
equal(Core.mode9Offset(0, 1), 8192, "bank 1 start");
equal(Core.mode9Offset(319, 1), 8351, "bank 1 row end");
equal(Core.mode9Offset(0, 2), 16384, "bank 2 start");
equal(Core.mode9Offset(0, 3), 24576, "bank 3 start");
equal(Core.mode9Offset(0, 4), 160, "second group starts after 160 visible bytes");
equal(Core.mode9Offset(319, 199), 32575, "last visible byte");
equal(Core.mode9Offset(-1, 0), -1, "negative x is rejected");
equal(Core.mode9Offset(320, 0), -1, "right edge is rejected");
equal(Core.pixelOffset(-1, 0, 8, 8), -1, "sprite editor bounds");

/* Every palette entry is an exact self-match after sRGB linearization/OKLab. */
for (let index = 0; index < 16; index += 1) {
  equal(Core.nearestColor(Core.RGBI[index], Array.from({ length: 16 }, (_, i) => i)), index,
    "RGBI color " + index + " round-trips");
  const one = new Uint8ClampedArray([...Core.RGBI[index], 255]);
  const converted = Core.convertRgba(one, 1, 1, { mode: "screen", dither: "none" });
  equal(converted.indices[0], index, "palette pixel preserves index " + index);
  equal(converted.alpha[0], 1, "palette pixel " + index + " remains opaque");
}
check(Math.abs(Core.srgbToLinear(128) - 0.21586) < 0.0002, "sRGB midtone is linearized before processing");

/* Alpha stays separate from color: transparent index 0 and visible black index 0. */
const transparent = Core.convertRgba(new Uint8ClampedArray([0, 0, 0, 0]), 1, 1, { mode: "screen" });
const opaqueBlack = Core.convertRgba(new Uint8ClampedArray([0, 0, 0, 255]), 1, 1, { mode: "screen" });
equal(transparent.indices[0], 0, "transparent pixel has index zero");
equal(transparent.alpha[0], 0, "transparent pixel is marked transparent");
equal(opaqueBlack.indices[0], 0, "black pixel keeps color index zero");
equal(opaqueBlack.alpha[0], 1, "black pixel remains opaque");
const blackSprite = Core.convertRgba(new Uint8ClampedArray([0, 0, 0, 255, 0, 0, 0, 0]), 2, 1,
  { mode: "sprite", spriteColors: [0, 14, 15], dither: "none" });
equal(blackSprite.indices[0], 1, "opaque black maps to nonzero sprite index 1");
equal(blackSprite.alpha[0], 1, "opaque black sprite pixel remains visible");
equal(blackSprite.indices[1], 0, "transparent sprite pixel is index zero");
equal(blackSprite.alpha[1], 0, "transparent sprite pixel remains see-through");

const screen = Core.createFrame(320, 200, 0, true);
for (let x = 0; x < 16; x += 1) screen.indices[x] = x;
screen.indices[320] = 3; screen.indices[321] = 4; /* First pixel in bank 1. */
screen.indices[1280] = 12; /* First pixel of the next 4-row group. */
const packed = Core.packMode9(screen, 0);
equal(packed.length, 32768, "packed image is exactly 32 KiB");
for (let x = 0; x < 16; x += 2) equal(packed[x >> 1], (x << 4) | (x + 1), "pair is high-even/low-odd at x=" + x);
equal(packed[8192], 0x34, "first byte of bank 1");
equal(packed[160], 0xC0, "row 4 starts after 160 bytes in bank 0");
const alphaScreen = Core.createFrame(320, 200, 0, false);
alphaScreen.indices[0] = 0; alphaScreen.alpha[0] = 1; /* Opaque black. */
const blackAndTransparent = Core.packMode9(alphaScreen, 6);
equal(blackAndTransparent[0], 0x06, "opaque black is preserved; transparent pixel uses selected background");

/* Full screen round-trip, including every visible pixel and bounds. */
const pattern = Core.createFrame(320, 200, 0, true);
for (let y = 0; y < 200; y += 1) for (let x = 0; x < 320; x += 1) pattern.indices[y * 320 + x] = (x + 3 * y) & 15;
const roundtrip = Core.unpackMode9(Core.packMode9(pattern, 0));
for (let p = 0; p < 320 * 200; p += 1) equal(roundtrip.indices[p], pattern.indices[p], "screen round-trip pixel " + p);
equal(roundtrip.alpha[320 * 200 - 1], 1, "unpacked screen pixels are opaque");
assert.throws(() => Core.packMode9(Core.createFrame(8, 8, 0, false), 0), /320 x 200/);
assert.throws(() => Core.unpackMode9(new Uint8Array(32767)), /32,768/);

/* Sprite encoding is two MSB-first planes, with index zero skipped as transparency. */
const sprite = Core.createFrame(8, 8, 0, false);
sprite.indices[0] = 1; sprite.alpha[0] = 1; /* palette[1] can be opaque black. */
sprite.indices[7] = 2; sprite.alpha[7] = 1;
sprite.indices[63] = 3; sprite.alpha[63] = 1;
const planes = Core.encodeSpritePlanes(sprite);
equal(planes.plane0[0], 0x80, "plane 0 uses the leftmost bit for index 1");
equal(planes.plane1[0], 0x01, "plane 1 uses the rightmost bit for index 2");
equal(planes.plane0[7], 0x01, "last row retains index 3 low bit");
equal(planes.plane1[7], 0x01, "last row retains index 3 high bit");
const decodedSprite = Core.decodeSpritePlanes(planes.plane0, planes.plane1, [0, 0, 14, 15]);
for (let p = 0; p < 64; p += 1) {
  equal(decodedSprite.indices[p], sprite.indices[p], "sprite round-trip index " + p);
  equal(decodedSprite.alpha[p], sprite.alpha[p], "sprite round-trip alpha " + p);
}
assert.throws(() => Core.encodeSpritePlanes(Core.createFrame(16, 16, 0, false)), /8 x 8/);

/* The automatic selector distinguishes low-color pixel art, flat illustrations, and photos. */
const pixelSample = new Uint8ClampedArray(16 * 16 * 4);
for (let p = 0; p < 256; p += 1) { pixelSample[p * 4] = (p & 1) ? 255 : 0; pixelSample[p * 4 + 1] = 80; pixelSample[p * 4 + 3] = 255; }
equal(Core.detectImageKind(pixelSample, 16, 16).kind, "pixel-art", "flat few-color art avoids filtering");
const illustrationSample = new Uint8ClampedArray(128 * 128 * 4);
for (let y = 0; y < 128; y += 1) for (let x = 0; x < 128; x += 1) {
  const p = (y * 128 + x) * 4; illustrationSample[p] = x * 2; illustrationSample[p + 1] = y; illustrationSample[p + 2] = 115; illustrationSample[p + 3] = 255;
}
equal(Core.detectImageKind(illustrationSample, 128, 128).kind, "illustration", "smooth gradients are treated as illustration");
const photoSample = new Uint8ClampedArray(128 * 128 * 4);
for (let y = 0; y < 128; y += 1) for (let x = 0; x < 128; x += 1) {
  const p = (y * 128 + x) * 4; const noise = ((x * 37 + y * 73 + x * y * 11) & 63);
  photoSample[p] = Math.min(255, x * 2 + noise); photoSample[p + 1] = Math.min(255, y * 2 + noise); photoSample[p + 2] = 100 + noise; photoSample[p + 3] = 255;
}
equal(Core.detectImageKind(photoSample, 128, 128).kind, "photo", "dense detailed color is treated as photo");

/* Dither strengths/modes are responsive; none stays clean and both dither modes add texture. */
const gray = new Uint8ClampedArray(48 * 24 * 4);
for (let p = 0; p < 48 * 24; p += 1) { gray[p * 4] = 128; gray[p * 4 + 1] = 128; gray[p * 4 + 2] = 128; gray[p * 4 + 3] = 255; }
const noDither = Core.convertRgba(gray, 48, 24, { dither: "none" });
const floyd = Core.convertRgba(gray, 48, 24, { dither: "fs", ditherStrength: 75 });
const bayer = Core.convertRgba(gray, 48, 24, { dither: "ordered", ditherStrength: 75 });
equal(new Set(noDither.indices).size, 1, "no-dither keeps flat gray flat");
check(new Set(floyd.indices).size > 1, "Floyd–Steinberg creates local tone detail");
check(new Set(bayer.indices).size > 1, "Bayer creates ordered tone detail");
check(floyd.indices.every((color) => color < 16) && bayer.indices.every((color) => color < 16), "all dither choices remain within the RGBI palette");
const noStrength = Core.convertRgba(gray, 48, 24, { dither: "fs", ditherStrength: 0 });
check(noStrength.indices.every((color, p) => color === noDither.indices[p]), "zero dither strength disables the pattern");
const magenta = new Uint8ClampedArray([255, 0, 255, 255]);
equal(Core.convertRgba(magenta, 1, 1, { dither: "none" }).indices[0], 13, "saturated color maps to bright magenta");
check(Core.convertRgba(magenta, 1, 1, { dither: "none", saturation: 0 }).indices[0] !== 13, "zero saturation has an effect");
const alphaAwareInput = new Uint8ClampedArray([128,128,128,255, 0,0,0,0, 128,128,128,255]);
const alphaAware = Core.convertRgba(alphaAwareInput, 3, 1, { dither: "fs", ditherStrength: 100 });
const alphaBase = Core.convertRgba(new Uint8ClampedArray([128,128,128,255]), 1, 1, { dither: "none" });
equal(alphaAware.indices[2], alphaBase.indices[0], "transparent pixel blocks error bleed to the far side");

const exampleProject = JSON.parse(fs.readFileSync(path.join(__dirname, "../examples/little-fox.tandy.json"), "utf8"));
equal(exampleProject.format, "libtandy-graphics-workshop", "sample project format identifier");
equal(exampleProject.frames[0].indices.length, 64, "sample sprite pixel count");
equal(exampleProject.frames[0].alpha.length, 64, "sample sprite alpha count");
check(Core.encodeSpritePlanes({ ...exampleProject.frames[0], indices: Uint8Array.from(exampleProject.frames[0].indices), alpha: Uint8Array.from(exampleProject.frames[0].alpha) }).plane0.length === 8, "sample project encodes as a real sprite");

/* Header data and actual C call compile together with the repository graphics core. */
const headerSprite = Core.exportSpriteHeader([sprite], [0, 14, 15], "sample_sprite");
check(headerSprite.includes("static const lt_gfx_u8 sample_sprite_plane0[1][8]"), "sprite header uses exact C type and dimensions");
check(headerSprite.includes("sample_sprite_palette[4]"), "sprite header includes palette metadata");
check(headerSprite.includes("lt_gfx_sprite8_planes(&screen"), "sprite header documents correct API usage");
const headerScreen = Core.exportScreenHeader(pattern, "sample_screen", 0);
check(headerScreen.includes("sample_screen_mode9[LT_GFX_BYTES]"), "screen header uses the library's exact buffer length");
check(headerScreen.includes("320x200") && headerScreen.includes("Even x is the high nibble"), "screen header describes dimensions and nibble order");

const watcom = process.env.WCL || "C:\\WATCOM\\BINNT\\wcl386.exe";
if (fs.existsSync(watcom)) {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), "libtandy-editor-c-"));
  try {
    fs.writeFileSync(path.join(temp, "sample_sprite.h"), headerSprite);
    fs.writeFileSync(path.join(temp, "sample_screen.h"), headerScreen);
    fs.writeFileSync(path.join(temp, "roundtrip.c"), [
      "#include <string.h>",
      "#include <stdio.h>",
      "#include \"sample_sprite.h\"",
      "#include \"sample_screen.h\"",
      "int main(void) {",
      "  lt_gfx_surface screen;",
      "  lt_gfx_u8 framebuffer[LT_GFX_BYTES];",
      "  memset(framebuffer, 0x22, sizeof(framebuffer));",
      "  lt_gfx_init(&screen, framebuffer);",
      "  if (!lt_gfx_sprite8_planes(&screen, 0, 0, sample_sprite_plane0[0], sample_sprite_plane1[0], sample_sprite_palette)) return 1;",
      "  if (lt_gfx_get(&screen, 0, 0) != 0 || lt_gfx_get(&screen, 1, 0) != 2 || lt_gfx_get(&screen, 7, 0) != 14) return 2;",
      "  lt_gfx_init(&screen, sample_screen_mode9);",
      "  if (lt_gfx_get(&screen, 0, 0) != 0 || lt_gfx_get(&screen, 1, 0) != 1) return 3;",
      "  if (LT_GFX_BYTES != 32768U || sizeof(sample_screen_mode9) != 32768U) return 4;",
      "  puts(\"generated C headers work with libtandy graphics core\");",
      "  return 0;",
      "}"
    ].join("\n") + "\n");
    const executable = path.join(temp, "roundtrip.exe");
    const repoRoot = path.resolve(__dirname, "../../..");
    const compile = spawnSync(watcom, ["-bt=nt", "-ecc", "-w4", "-wx", "-os",
      "-i=" + path.join(repoRoot, "include"), "-i=" + temp, "-fe=" + executable,
      path.join(temp, "roundtrip.c"), path.join(repoRoot, "src/lt_gfx.c"),
      path.join(repoRoot, "src/lt_gfx_sprite8.c")], { encoding: "utf8", timeout: 30000 });
    if (compile.status !== 0) throw new Error("Open Watcom generated-C compile failed:\n" + compile.stdout + compile.stderr);
    const watcom16 = process.env.WCL16 || "C:\\WATCOM\\BINNT\\wcl.exe";
    if (fs.existsSync(watcom16)) {
      const compile16 = spawnSync(watcom16, ["-q", "-bt=dos", "-0", "-ms", "-ecc", "-os", "-s", "-w4", "-wx",
        "-i=" + path.join(repoRoot, "include"), "-i=" + temp, "-c",
        path.join(temp, "roundtrip.c"), path.join(repoRoot, "src/lt_gfx.c"),
        path.join(repoRoot, "src/lt_gfx_sprite8.c")], { encoding: "utf8", cwd: temp, timeout: 30000 });
      if (compile16.status !== 0) throw new Error("Open Watcom 16-bit generated-C compile failed:\n" + compile16.stdout + compile16.stderr);
      console.log("generated C headers compile in the repository's 16-bit DOS memory model");
      checks += 1;
    }
    const run = spawnSync(executable, [], { encoding: "utf8", timeout: 5000 });
    if (run.status !== 0) throw new Error("Generated-C integration executable failed (status=" + run.status + ", error=" + run.error + "):\n" + run.stdout + run.stderr);
    console.log(run.stdout.trim());
    checks += 1;
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
} else {
  throw new Error("Open Watcom wcl.exe was not found. Set WCL to the available C compiler to run the generated-header integration check.");
}

console.log("PASS graphics editor core: " + checks + " checks");
