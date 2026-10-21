import assert from "node:assert/strict";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import { Client } from "@modelcontextprotocol/client";
import { StdioClientTransport } from "@modelcontextprotocol/client/stdio";
import jpeg from "jpeg-js";
import { PNG } from "pngjs";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const Core = require("../../graphics-editor/core.js");
const packageRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const serverPath = path.join(packageRoot, "server.mjs");

function pngBytes(width, height, pixelAt) {
  const data = Buffer.alloc(width * height * 4);
  for (let y = 0; y < height; y += 1) for (let x = 0; x < width; x += 1) {
    const color = pixelAt(x, y);
    data.set(color, (y * width + x) * 4);
  }
  return PNG.sync.write({ width, height, data });
}

function projectFrame(projectPath, frameIndex = 0) {
  const project = JSON.parse(require("node:fs").readFileSync(projectPath, "utf8"));
  return { project, frame: project.frames[frameIndex] };
}

test("local MCP workflows share exact browser-core conversions and exports", async (t) => {
  const workspace = await fs.mkdtemp(path.join(os.tmpdir(), "libtandy-mcp-test-"));
  t.after(async () => fs.rm(workspace, { recursive: true, force: true }));

  const screenPng = pngBytes(320, 200, (x, y) => {
    const pixel = y * 320 + x;
    const rgb = Core.RGBI[pixel % 16];
    return [rgb[0], rgb[1], rgb[2], pixel === 1 ? 0 : 255];
  });
  await fs.writeFile(path.join(workspace, "screen.png"), screenPng);

  const spritePng = pngBytes(16, 8, (x, y) => {
    const localX = x % 8;
    const localPixel = y * 8 + localX;
    if (x === 1 && y === 0) return [0, 0, 0, 0];
    const colors = [0, 14, 4, 1, 15];
    const rgb = Core.RGBI[colors[(localPixel + Math.floor(x / 8) * 2) % colors.length]];
    return [rgb[0], rgb[1], rgb[2], 255];
  });
  await fs.writeFile(path.join(workspace, "sprites.png"), spritePng);
  const jpegSource = Buffer.alloc(320 * 200 * 4);
  for (let p = 0; p < 320 * 200; p += 1) {
    const color = Core.RGBI[(p * 7 + Math.floor(p / 320)) % 16];
    jpegSource.set([color[0], color[1], color[2], 255], p * 4);
  }
  const jpegBytes = jpeg.encode({ width: 320, height: 200, data: jpegSource }, 100).data;
  await fs.writeFile(path.join(workspace, "photo.jpg"), jpegBytes);
  const conversionPng = pngBytes(320, 200, (x, y) => {
    const noise = (x * 37 + y * 73 + x * y * 11) & 63;
    return [Math.min(255, x * 2 + noise), Math.min(255, y * 2 + noise), 100 + noise, 255];
  });
  await fs.writeFile(path.join(workspace, "conversion-settings.png"), conversionPng);
  const keyingPng = pngBytes(320, 200, (x, y) => {
    const pixel = y * 320 + x;
    if (pixel === 0) return [0, 0, 0, 255];
    if (pixel === 1) return [255, 255, 255, 150];
    if (pixel === 2) return [255, 85, 85, 255];
    const gray = Core.RGBI[7];
    return [gray[0], gray[1], gray[2], 255];
  });
  await fs.writeFile(path.join(workspace, "keying.png"), keyingPng);
  const paletteSpritePng = pngBytes(16, 8, (x, y) => {
    const localX = x % 8;
    const color = y === 0 && localX === 0 ? 0 : y === 1 && localX === 1 ? 4 : y === 1 && localX === 2 ? 1 : 2;
    const rgb = Core.RGBI[color];
    return [rgb[0], rgb[1], rgb[2], 255];
  });
  await fs.writeFile(path.join(workspace, "palette-sprite-sheet.png"), paletteSpritePng);
  await fs.writeFile(path.join(workspace, "broken.tandy.json"), JSON.stringify({
    format: "libtandy-graphics-workshop", version: 1, mode: "sprite", name: "broken",
    spriteColors: [0, 14, 15], screenBackground: 0,
    frames: [{ width: 8, height: 8, indices: [0], alpha: [1] }]
  }));

  const transport = new StdioClientTransport({
    command: process.execPath,
    args: [serverPath, "--workdir", workspace],
    cwd: packageRoot,
    stderr: "pipe"
  });
  const client = new Client({ name: "libtandy-graphics-workshop-test", version: "1.0.0" });
  t.after(async () => { await client.close().catch(() => {}); await transport.close().catch(() => {}); });
  await client.connect(transport);

  const toolList = await client.listTools();
  assert.deepEqual(toolList.tools.map((item) => item.name).sort(), [
    "convert_image", "export_asset", "import_mode9", "import_sprite_planes", "inspect_project"
  ]);
  assert.ok(toolList.tools.find((item) => item.name === "convert_image").inputSchema.properties.output_path);

  async function invoke(name, args) {
    const result = await client.callTool({ name, arguments: args });
    const text = result.content?.find((item) => item.type === "text")?.text || "";
    return { result, data: text.startsWith("{") ? JSON.parse(text) : text };
  }

  const convertedScreen = await invoke("convert_image", {
    input_path: "screen.png", output_path: "screen.tandy.json", mode: "screen", name: "screen-test",
    dither: "none", auto_correct: false, pixel_art: true, resize_mode: "stretch", screen_background: 6
  });
  assert.equal(convertedScreen.result.isError, undefined);
  assert.equal(convertedScreen.data.output.opaqueBlackPixels > 0, true);
  assert.equal(convertedScreen.data.output.transparentPixels, 1);
  const { project: screenProject, frame: screenFrame } = projectFrame(path.join(workspace, "screen.tandy.json"));
  const decodedScreen = PNG.sync.read(screenPng);
  const expectedScreen = Core.convertRgba(decodedScreen.data, 320, 200, {
    mode: "screen", spriteColors: [14, 0, 15], dither: "none", ditherStrength: 65,
    recommendedDither: "none", pixelArt: true, autoCorrect: false, autoStrength: 35,
    exposure: 0, contrast: 100, saturation: 100, gamma: 100, backgroundMode: "alpha",
    backgroundColor: 0, backgroundTolerance: 0, alphaThreshold: 128
  });
  assert.deepEqual(screenFrame.indices, Array.from(expectedScreen.indices));
  assert.deepEqual(screenFrame.alpha, Array.from(expectedScreen.alpha));

  const convertedJpeg = await invoke("convert_image", {
    input_path: "photo.jpg", output_path: "photo.tandy.json", mode: "screen", name: "jpeg-test",
    dither: "none", auto_correct: false, pixel_art: true
  });
  assert.equal(convertedJpeg.result.isError, undefined);
  assert.equal(convertedJpeg.data.source.format, "jpeg");
  const jpegDecoded = jpeg.decode(jpegBytes, { useTArray: true, formatAsRGBA: true });
  const expectedJpeg = Core.convertRgba(jpegDecoded.data, 320, 200, {
    mode: "screen", spriteColors: [14, 0, 15], dither: "none", ditherStrength: 65,
    recommendedDither: "none", pixelArt: true, autoCorrect: false, autoStrength: 35,
    exposure: 0, contrast: 100, saturation: 100, gamma: 100, backgroundMode: "alpha",
    backgroundColor: 0, backgroundTolerance: 0, alphaThreshold: 128
  });
  const { frame: jpegFrame } = projectFrame(path.join(workspace, "photo.tandy.json"));
  assert.deepEqual(jpegFrame.indices, Array.from(expectedJpeg.indices));
  assert.ok(jpegFrame.alpha.every((value) => value === 1), "JPEG input stays fully opaque");

  const decodedConversion = PNG.sync.read(conversionPng);
  assert.equal(Core.detectImageKind(decodedConversion.data, 320, 200).kind, "photo");
  const coreConversionBase = {
    mode: "screen", spriteColors: [14, 0, 15], dither: "auto", ditherStrength: 100,
    recommendedDither: "none", pixelArt: false, autoCorrect: false, autoStrength: 100,
    exposure: 0, contrast: 100, saturation: 100, gamma: 100,
    backgroundMode: "alpha", backgroundColor: 0, backgroundTolerance: 0, alphaThreshold: 128
  };
  const noDither = await invoke("convert_image", {
    input_path: "conversion-settings.png", output_path: "settings-none.tandy.json", mode: "screen",
    dither: "auto", dither_strength: 100, recommended_dither: "none", pixel_art: false,
    auto_correct: false, auto_strength: 100
  });
  assert.equal(noDither.result.isError, undefined);
  const expectedNoDither = Core.convertRgba(decodedConversion.data, 320, 200, { ...coreConversionBase });
  const { frame: noDitherFrame } = projectFrame(path.join(workspace, "settings-none.tandy.json"));
  assert.deepEqual(noDitherFrame.indices, Array.from(expectedNoDither.indices),
    "MCP maps the explicit ordered/error-diffusion recommendation through the core options");

  const withFloyd = await invoke("convert_image", {
    input_path: "conversion-settings.png", output_path: "settings-fs.tandy.json", mode: "screen",
    dither: "auto", dither_strength: 100, recommended_dither: "fs", pixel_art: false,
    auto_correct: false, auto_strength: 100
  });
  assert.equal(withFloyd.result.isError, undefined);
  const expectedFloyd = Core.convertRgba(decodedConversion.data, 320, 200, {
    ...coreConversionBase, recommendedDither: "fs"
  });
  const { frame: floydFrame } = projectFrame(path.join(workspace, "settings-fs.tandy.json"));
  assert.deepEqual(floydFrame.indices, Array.from(expectedFloyd.indices),
    "MCP applies the selected serpentine dither from snake_case input");
  assert.notDeepEqual(floydFrame.indices, noDitherFrame.indices,
    "the dither recommendation visibly changes this photo-like conversion");

  const zeroStrength = await invoke("convert_image", {
    input_path: "conversion-settings.png", output_path: "settings-zero.tandy.json", mode: "screen",
    dither: "auto", dither_strength: 0, recommended_dither: "fs", pixel_art: false,
    auto_correct: false, auto_strength: 100
  });
  assert.equal(zeroStrength.result.isError, undefined);
  const expectedZero = Core.convertRgba(decodedConversion.data, 320, 200, {
    ...coreConversionBase, recommendedDither: "fs", ditherStrength: 0
  });
  const { frame: zeroFrame } = projectFrame(path.join(workspace, "settings-zero.tandy.json"));
  assert.deepEqual(zeroFrame.indices, Array.from(expectedZero.indices), "zero dither strength remains zero");
  assert.deepEqual(zeroFrame.indices, noDitherFrame.indices, "zero-strength error diffusion matches no dithering");

  const corrected = await invoke("convert_image", {
    input_path: "conversion-settings.png", output_path: "settings-corrected.tandy.json", mode: "screen",
    dither: "none", pixel_art: false, auto_correct: true, auto_strength: 100
  });
  assert.equal(corrected.result.isError, undefined);
  const expectedCorrected = Core.convertRgba(decodedConversion.data, 320, 200, {
    ...coreConversionBase, dither: "none", recommendedDither: "none", autoCorrect: true, autoStrength: 100
  });
  const { frame: correctedFrame } = projectFrame(path.join(workspace, "settings-corrected.tandy.json"));
  assert.deepEqual(correctedFrame.indices, Array.from(expectedCorrected.indices),
    "MCP maps automatic tone correction and strength to core options");
  assert.notDeepEqual(correctedFrame.indices, noDitherFrame.indices,
    "automatic tone correction changes this low-contrast source");

  const pixelArtOverride = await invoke("convert_image", {
    input_path: "conversion-settings.png", output_path: "settings-pixel-art.tandy.json", mode: "screen",
    dither: "auto", dither_strength: 100, recommended_dither: "fs", pixel_art: true,
    auto_correct: true, auto_strength: 100
  });
  assert.equal(pixelArtOverride.result.isError, undefined);
  const expectedPixelArtOverride = Core.convertRgba(decodedConversion.data, 320, 200, {
    ...coreConversionBase, recommendedDither: "fs", pixelArt: true, autoCorrect: true, autoStrength: 100
  });
  const { frame: pixelArtOverrideFrame } = projectFrame(path.join(workspace, "settings-pixel-art.tandy.json"));
  assert.deepEqual(pixelArtOverrideFrame.indices, Array.from(expectedPixelArtOverride.indices),
    "explicit pixel-art choice overrides detected photo-like content in the shared core");
  assert.notDeepEqual(pixelArtOverrideFrame.indices, floydFrame.indices,
    "pixel-art override disables automatic correction and dithering");

  const decodedKeying = PNG.sync.read(keyingPng);
  const keyed = await invoke("convert_image", {
    input_path: "keying.png", output_path: "keyed.tandy.json", mode: "screen",
    dither: "none", pixel_art: true, auto_correct: false, background_mode: "color",
    background_color: 0, background_tolerance: 0, alpha_threshold: 200, screen_background: 5
  });
  assert.equal(keyed.result.isError, undefined);
  const expectedKeyed = Core.convertRgba(decodedKeying.data, 320, 200, {
    mode: "screen", spriteColors: [14, 0, 15], dither: "none", ditherStrength: 65,
    recommendedDither: "none", pixelArt: true, autoCorrect: false, autoStrength: 35,
    exposure: 0, contrast: 100, saturation: 100, gamma: 100,
    backgroundMode: "color", backgroundColor: 0, backgroundTolerance: 0, alphaThreshold: 200
  });
  const { project: keyProject, frame: keyFrame } = projectFrame(path.join(workspace, "keyed.tandy.json"));
  assert.deepEqual(keyFrame.indices, Array.from(expectedKeyed.indices));
  assert.deepEqual(keyFrame.alpha, Array.from(expectedKeyed.alpha));
  assert.equal(keyFrame.alpha[0], 0, "opaque black selected as the color key becomes transparent");
  assert.equal(keyFrame.alpha[1], 0, "alpha below the explicit threshold becomes transparent");
  assert.equal(keyFrame.alpha[2], 1, "visible red remains opaque");
  assert.equal(keyProject.screenBackground, 5, "screen background is stored separately from conversion settings");

  const decodedPaletteSheet = PNG.sync.read(paletteSpritePng);
  const keyedPalette = await invoke("convert_image", {
    input_path: "palette-sprite-sheet.png", output_path: "palette-keyed.tandy.json", mode: "sprite",
    frame_count: 2, dither: "none", pixel_art: true, auto_correct: false,
    background_mode: "color", background_color: 2, background_tolerance: 0
  });
  assert.equal(keyedPalette.result.isError, undefined);
  const expectedPalette = Core.recommendSpriteColors(decodedPaletteSheet.data, 8, 16, {
    mode: "sprite", spriteColors: [14, 0, 15], dither: "none", ditherStrength: 65,
    recommendedDither: "none", pixelArt: true, autoCorrect: false, autoStrength: 35,
    exposure: 0, contrast: 100, saturation: 100, gamma: 100,
    backgroundMode: "color", backgroundColor: 2, backgroundTolerance: 0, alphaThreshold: 128
  });
  const { project: keyedPaletteProject, frame: keyedPaletteFrame } = projectFrame(path.join(workspace, "palette-keyed.tandy.json"));
  assert.deepEqual(keyedPaletteProject.spriteColors, expectedPalette,
    "sprite palette suggestion uses the same normalized color-key and tone settings as conversion");
  assert.ok(!expectedPalette.includes(2), "the keyed green background is excluded from the sprite ink search");
  assert.equal(keyedPaletteFrame.alpha[1], 0, "color-keyed sprite background stays transparent");

  const packedScreen = Buffer.from(Core.packMode9(expectedScreen, screenProject.screenBackground));
  const screenBinResult = await invoke("export_asset", {
    project_path: "screen.tandy.json", output_path: "screen.bin", format: "mode9-bin"
  });
  assert.equal(screenBinResult.result.isError, undefined);
  assert.equal(screenBinResult.data.bytes, 32768);
  assert.deepEqual(await fs.readFile(path.join(workspace, "screen.bin")), packedScreen);
  assert.equal(packedScreen[0], 0x06, "screen background color fills transparency without changing opaque black");

  const screenHeader = await invoke("export_asset", {
    project_path: "screen.tandy.json", output_path: "screen.h", format: "screen-c-header", symbol_name: "screen-test"
  });
  assert.equal(screenHeader.result.isError, undefined);
  assert.equal(await fs.readFile(path.join(workspace, "screen.h"), "utf8"),
    Core.exportScreenHeader(expectedScreen, "screen-test", 6));

  const importedScreen = await invoke("import_mode9", {
    input_path: "screen.bin", output_path: "reimported.tandy.json", name: "reimported"
  });
  assert.equal(importedScreen.result.isError, undefined);
  const { frame: reimportedFrame } = projectFrame(path.join(workspace, "reimported.tandy.json"));
  const unpacked = Core.unpackMode9(packedScreen);
  assert.deepEqual(reimportedFrame.indices, Array.from(unpacked.indices));
  assert.deepEqual(reimportedFrame.alpha, Array.from(unpacked.alpha));

  const convertedSprites = await invoke("convert_image", {
    input_path: "sprites.png", output_path: "sprites.tandy.json", mode: "sprite", name: "sprite-test",
    frame_count: 2, sprite_colors: [0, 14, 4], dither: "none", auto_correct: false, pixel_art: true
  });
  assert.equal(convertedSprites.result.isError, undefined);
  assert.equal(convertedSprites.data.output.frameCount, 2);
  assert.ok(convertedSprites.data.output.opaqueBlackPixels > 0);
  const { project: spriteProject, frame: spriteFrame0 } = projectFrame(path.join(workspace, "sprites.tandy.json"), 0);
  const decodedSheet = PNG.sync.read(spritePng);
  for (let frameIndex = 0; frameIndex < 2; frameIndex += 1) {
    const rgba = new Uint8Array(8 * 8 * 4);
    for (let y = 0; y < 8; y += 1) {
      const from = (y * 16 + frameIndex * 8) * 4;
      rgba.set(decodedSheet.data.subarray(from, from + 8 * 4), y * 8 * 4);
    }
    const expected = Core.convertRgba(rgba, 8, 8, {
      mode: "sprite", spriteColors: [0, 14, 4], dither: "none", ditherStrength: 65,
      recommendedDither: "none", pixelArt: true, autoCorrect: false, autoStrength: 35,
      exposure: 0, contrast: 100, saturation: 100, gamma: 100, backgroundMode: "alpha",
      backgroundColor: 0, backgroundTolerance: 0, alphaThreshold: 128
    });
    assert.deepEqual(spriteProject.frames[frameIndex].indices, Array.from(expected.indices));
    assert.deepEqual(spriteProject.frames[frameIndex].alpha, Array.from(expected.alpha));
  }
  assert.equal(spriteFrame0.alpha[1], 0, "sheet alpha remains transparent");
  assert.equal(spriteFrame0.indices[0], 1, "opaque black maps to a nonzero sprite ink");
  assert.equal(spriteProject.spriteColors[0], 0, "sprite ink slot 1 can map to RGBI black");
  const spriteHeader = await invoke("export_asset", {
    project_path: "sprites.tandy.json", output_path: "sprites.h", format: "sprite-c-header"
  });
  assert.equal(spriteHeader.result.isError, undefined);
  const expectedSpriteFrames = spriteProject.frames.map((frame) => ({ width: 8, height: 8,
    indices: Uint8Array.from(frame.indices), alpha: Uint8Array.from(frame.alpha) }));
  assert.equal(await fs.readFile(path.join(workspace, "sprites.h"), "utf8"),
    Core.exportSpriteHeader(expectedSpriteFrames, spriteProject.spriteColors, "sprite-test"));

  const autoPalette = await invoke("convert_image", {
    input_path: "sprites.png", output_path: "sprites-auto.tandy.json", mode: "sprite", name: "auto-palette",
    frame_count: 2, dither: "none", auto_correct: false, pixel_art: true
  });
  assert.equal(autoPalette.result.isError, undefined);
  assert.equal(autoPalette.data.output.spriteColors.length, 3);
  assert.ok(autoPalette.data.output.spriteColors.every((color) => color >= 0 && color < 16));

  const planeImport = await invoke("import_sprite_planes", {
    plane0: [128, 0, 0, 0, 0, 0, 0, 0], plane1: [0, 0, 0, 0, 0, 0, 0, 0],
    palette: [0, 14, 15], output_path: "plane-import.tandy.json", name: "plane-import"
  });
  assert.equal(planeImport.result.isError, undefined);
  assert.equal(planeImport.data.opaqueBlackPixels, 1);
  assert.equal(planeImport.data.transparentPixels, 63);
  const { frame: importedSprite } = projectFrame(path.join(workspace, "plane-import.tandy.json"));
  assert.equal(importedSprite.indices[0], 1);
  assert.equal(importedSprite.alpha[0], 1);
  assert.equal(importedSprite.indices[1], 0);
  assert.equal(importedSprite.alpha[1], 0);

  const inspected = await invoke("inspect_project", { project_path: "sprites.tandy.json" });
  assert.equal(inspected.result.isError, undefined);
  assert.equal(inspected.data.frameCount, 2);
  const malformed = await invoke("inspect_project", { project_path: "broken.tandy.json" });
  assert.equal(malformed.result.isError, true);
  assert.match(malformed.result.content[0].text, /index or transparency data length/);

  const traversal = await invoke("convert_image", {
    input_path: "../screen.png", output_path: "traversal.tandy.json", mode: "screen"
  });
  assert.equal(traversal.result.isError, true);
  assert.match(traversal.result.content[0].text, /relative file path|cannot contain/);
  const beforeNoOverwrite = await fs.readFile(path.join(workspace, "screen.bin"));
  const noOverwrite = await invoke("export_asset", {
    project_path: "screen.tandy.json", output_path: "screen.bin", format: "mode9-bin"
  });
  assert.equal(noOverwrite.result.isError, true);
  assert.match(noOverwrite.result.content[0].text, /already exists/);
  assert.deepEqual(await fs.readFile(path.join(workspace, "screen.bin")), beforeNoOverwrite);

  const wrongSheet = await invoke("convert_image", {
    input_path: "sprites.png", output_path: "wrong-sheet.tandy.json", mode: "screen", frame_count: 2
  });
  assert.equal(wrongSheet.result.isError, true);
  assert.match(wrongSheet.result.content[0].text, /horizontal sheet must be exactly/);
});
