#!/usr/bin/env node
import path from "node:path";
import { createRequire } from "node:module";
import { McpServer } from "@modelcontextprotocol/server";
import { StdioServerTransport } from "@modelcontextprotocol/server/stdio";
import * as z from "zod/v4";
import { decodeImage, horizontalFrames, resizeRgba } from "./lib/png-image.mjs";
import { createFileSandbox, MAX_FILE_BYTES, MAX_PROJECT_BYTES, MAX_SCREEN_FRAMES,
  MAX_SPRITE_FRAMES, projectSummary, projectToJson, validateProject } from "./lib/project.mjs";

const require = createRequire(import.meta.url);
const Core = require("../graphics-editor/core.js");
const MAX_IMAGE_BYTES = MAX_FILE_BYTES;

function requestedWorkdir() {
  const index = process.argv.indexOf("--workdir");
  if (index < 0 || !process.argv[index + 1]) {
    throw new Error("Start this optional server with --workdir <existing-folder>; all tool paths stay inside that folder.");
  }
  return path.resolve(process.argv[index + 1]);
}

function textResult(value) {
  return { structuredContent: value, content: [{ type: "text", text: JSON.stringify(value, null, 2) }] };
}

function errorResult(error) {
  return { isError: true, content: [{ type: "text", text: `Error: ${error?.message || String(error)}` }] };
}

function tool(handler) {
  return async (input) => {
    try { return textResult(await handler(input)); }
    catch (error) { return errorResult(error); }
  };
}

const projectSummarySchema = z.object({
  name: z.string(),
  mode: z.enum(["screen", "sprite"]),
  width: z.number().int(),
  height: z.number().int(),
  frameCount: z.number().int(),
  spriteColors: z.array(z.number().int()).length(3),
  screenBackground: z.number().int(),
  transparentPixels: z.number().int(),
  opaqueBlackPixels: z.number().int()
});
const conversionResultSchema = z.object({
  written: z.string(),
  bytes: z.number().int(),
  source: z.object({ format: z.enum(["png", "jpeg"]), width: z.number().int(), height: z.number().int() }),
  output: projectSummarySchema,
  detectedImageKind: z.enum(["pixel-art", "photo", "illustration"]),
  resampling: z.enum(["nearest", "bilinear"]),
  settings: z.object({ dither: z.enum(["auto", "none", "fs", "ordered"]), ditherStrength: z.number(),
    pixelArt: z.boolean(), autoCorrect: z.boolean(), spriteColors: z.array(z.number().int()).length(3) })
});
const projectInfoSchema = projectSummarySchema.extend({ path: z.string() });
const importedProjectSchema = z.object({ written: z.string(), bytes: z.number().int() }).extend(projectSummarySchema.shape);
const exportResultSchema = z.object({ written: z.string(), bytes: z.number().int(), format: z.string(),
  selectedFrame: z.number().int().optional() }).extend(projectSummarySchema.shape);

function extensionMustMatch(relativePath, endings, label) {
  const lower = relativePath.toLowerCase();
  if (!endings.some((ending) => lower.endsWith(ending))) {
    throw new Error(`${label} output path must end in ${endings.join(" or ")}.`);
  }
}

async function readProject(sandbox, filePath) {
  const bytes = await sandbox.read(filePath, MAX_PROJECT_BYTES);
  let source;
  try { source = JSON.parse(bytes.toString("utf8")); }
  catch (error) { throw new Error(`Could not read project JSON: ${error.message}`); }
  return validateProject(source);
}

function normalizeConversionSettings(input, imageKind, spriteColors) {
  const pixelArt = input.pixel_art ?? imageKind.kind === "pixel-art";
  const recommendedDither = input.recommended_dither ?? (pixelArt ? "none" : imageKind.kind === "photo" ? "fs" : imageKind.kind === "illustration" ? "ordered" : "none");
  return {
    mode: input.mode,
    spriteColors,
    dither: input.dither,
    ditherStrength: input.dither_strength,
    recommendedDither,
    pixelArt,
    autoCorrect: input.auto_correct,
    autoStrength: input.auto_strength,
    exposure: input.exposure,
    contrast: input.contrast,
    saturation: input.saturation,
    gamma: input.gamma,
    backgroundMode: input.background_mode,
    backgroundColor: input.background_color,
    backgroundTolerance: input.background_tolerance,
    alphaThreshold: input.alpha_threshold
  };
}

function makeFrames(rgbaFrames, width, height, conversionOptions) {
  return rgbaFrames.map((rgba) => Core.convertRgba(rgba, width, height, conversionOptions));
}

async function createServer(workdir) {
  const sandbox = await createFileSandbox(workdir);
  const server = new McpServer({ name: "libtandy-graphics-workshop", version: "0.1.0" });

  server.registerTool("convert_image", {
    description: "Convert a local PNG or JPEG picture or horizontal frame sheet into an editable libtandy Graphics Workshop project. Sprite inks are suggested automatically unless you provide spriteColors. Transparent pixels stay transparent; visible black remains opaque.",
    outputSchema: conversionResultSchema,
    inputSchema: z.object({
      input_path: z.string().min(1).max(1024).describe("PNG or JPEG path relative to the configured working folder."),
      output_path: z.string().min(1).max(1024).describe("New .tandy.json path relative to the configured working folder; existing files are never replaced."),
      mode: z.enum(["screen", "sprite"]).default("screen").describe("Choose a 320x200 screen or an 8x8 sprite."),
      name: z.string().max(48).optional(),
      frame_count: z.number().int().min(1).max(64).default(1).describe("Use 1 for one picture or the number of equal-width frames in a horizontal sheet. Sheet cells must already be 320x200 for screens or 8x8 for sprites; limits are 16 screen frames or 64 sprite frames."),
      resize_mode: z.enum(["fit", "crop", "stretch"]).default("fit").describe("Fit preserves the whole picture with transparent padding; crop fills the target and trims the edges; stretch fills the target exactly."),
      resampling: z.enum(["auto", "nearest", "bilinear"]).default("auto").describe("Auto uses nearest-neighbor for detected pixel art and alpha-aware bilinear sampling for other images."),
      sprite_colors: z.array(z.number().int().min(0).max(15)).length(3).optional().describe("Optional locked RGBI colors for sprite ink slots 1..3. Omit to select the best three visible colors."),
      screen_background: z.number().int().min(0).max(15).default(0).describe("RGBI color used for transparent pixels when packing a screen, which has no alpha channel."),
      dither: z.enum(["auto", "none", "fs", "ordered"]).default("auto").describe("Auto selects serpentine error diffusion for photo-like images, ordered Bayer for illustrations, and none for pixel art."),
      dither_strength: z.number().min(0).max(100).default(65).describe("Dither amount from 0 (off) to 100 (full)."),
      recommended_dither: z.enum(["none", "fs", "ordered"]).optional().describe("Override the pattern used when dither is auto: none, serpentine Floyd-Steinberg, or ordered Bayer."),
      pixel_art: z.boolean().optional().describe("Override image-kind detection; true preserves hard edges and skips automatic tone correction."),
      auto_correct: z.boolean().default(true).describe("Apply gentle automatic tone correction to non-pixel-art images."),
      auto_strength: z.number().min(0).max(100).default(35).describe("Automatic tone-correction strength from 0 to 100."),
      exposure: z.number().min(-2).max(2).default(0).describe("Manual exposure adjustment in stops, from -2 to +2."),
      contrast: z.number().min(0).max(200).default(100).describe("Manual contrast percentage; 100 leaves contrast unchanged."),
      saturation: z.number().min(0).max(200).default(100).describe("Manual saturation percentage; 100 leaves color saturation unchanged."),
      gamma: z.number().min(50).max(200).default(100).describe("Manual gamma percentage; 100 leaves gamma unchanged."),
      background_mode: z.enum(["alpha", "color"]).default("alpha").describe("Keep image alpha, or make pixels near background_color transparent."),
      background_color: z.number().int().min(0).max(15).default(0).describe("RGBI color used by color-key transparency."),
      background_tolerance: z.number().int().min(0).max(255).default(0).describe("RGB color-key tolerance from 0 to 255."),
      alpha_threshold: z.number().int().min(1).max(255).default(128).describe("Source alpha below this threshold becomes transparent.")
    }).strict()
  }, tool(async (input) => {
    extensionMustMatch(input.output_path, [".tandy.json"], "Editable project");
    const imageBytes = await sandbox.read(input.input_path, MAX_IMAGE_BYTES);
    const image = decodeImage(imageBytes);
    const targetWidth = input.mode === "sprite" ? 8 : Core.WIDTH;
    const targetHeight = input.mode === "sprite" ? 8 : Core.HEIGHT;
    const maxFrames = input.mode === "sprite" ? MAX_SPRITE_FRAMES : MAX_SCREEN_FRAMES;
    if (input.frame_count > maxFrames) throw new Error(`${input.mode} import is limited to ${maxFrames} frames.`);
    const imageKind = Core.detectImageKind(image.data, image.width, image.height);
    const paletteAnalysis = { spriteColors: input.sprite_colors || [14, 0, 15] };
    const conversionOptions = normalizeConversionSettings(input, imageKind, paletteAnalysis.spriteColors);
    const pixelArt = conversionOptions.pixelArt;
    const filter = input.resampling === "auto" ? (pixelArt ? "nearest" : "bilinear") : input.resampling;
    const rgbaFrames = input.frame_count > 1
      ? horizontalFrames(image, targetWidth, targetHeight, input.frame_count)
      : [resizeRgba(image.data, image.width, image.height, targetWidth, targetHeight, input.resize_mode, filter)];
    if (input.mode === "sprite" && !input.sprite_colors) {
      const combined = new Uint8Array(rgbaFrames.reduce((total, frame) => total + frame.length, 0));
      let offset = 0;
      for (const frame of rgbaFrames) { combined.set(frame, offset); offset += frame.length; }
      paletteAnalysis.spriteColors = Core.recommendSpriteColors(combined, targetWidth, targetHeight * rgbaFrames.length,
        conversionOptions);
    }
    conversionOptions.spriteColors = paletteAnalysis.spriteColors;
    const project = {
      version: 1,
      name: (input.name || path.basename(input.input_path).replace(/\.[^.]+$/, "")).slice(0, 48) || "imported-art",
      mode: input.mode,
      frames: makeFrames(rgbaFrames, targetWidth, targetHeight, conversionOptions),
      spriteColors: paletteAnalysis.spriteColors.slice(),
      screenBackground: input.screen_background
    };
    const output = JSON.stringify(projectToJson(project), null, 2) + "\n";
    await sandbox.writeNew(input.output_path, output);
    const validated = validateProject(projectToJson(project));
    return { written: input.output_path, bytes: Buffer.byteLength(output), source: { format: image.format, width: image.width, height: image.height },
      output: projectSummary(validated), detectedImageKind: imageKind.kind, resampling: filter,
      settings: { dither: conversionOptions.dither, ditherStrength: conversionOptions.ditherStrength,
        pixelArt: conversionOptions.pixelArt, autoCorrect: conversionOptions.autoCorrect, spriteColors: project.spriteColors } };
  }));

  server.registerTool("inspect_project", {
    description: "Read and validate an editable Graphics Workshop project and summarize its dimensions, frames, transparency, and opaque black pixels.",
    outputSchema: projectInfoSchema,
    inputSchema: z.object({ project_path: z.string().min(1).max(1024).describe("Project JSON path relative to the configured working folder.") }).strict()
  }, tool(async ({ project_path }) => {
    const project = await readProject(sandbox, project_path);
    return { path: project_path, ...projectSummary(project) };
  }));

  server.registerTool("import_mode9", {
    description: "Open a raw 32,768-byte libtandy mode-9 screen file into an editable screen project.",
    outputSchema: importedProjectSchema,
    inputSchema: z.object({
      input_path: z.string().min(1).max(1024),
      output_path: z.string().min(1).max(1024),
      name: z.string().max(48).optional(),
      screen_background: z.number().int().min(0).max(15).default(0)
    }).strict()
  }, tool(async (input) => {
    extensionMustMatch(input.output_path, [".tandy.json"], "Editable project");
    const bytes = await sandbox.read(input.input_path, 32768);
    if (bytes.length !== Core.BYTES) throw new Error("A raw mode-9 image must be exactly 32,768 bytes.");
    const project = {
      version: 1,
      name: (input.name || path.basename(input.input_path).replace(/\.[^.]+$/, "")).slice(0, 48) || "mode9-screen",
      mode: "screen",
      frames: [Core.unpackMode9(bytes)],
      spriteColors: [14, 0, 15],
      screenBackground: input.screen_background
    };
    const normalized = validateProject(projectToJson(project));
    const output = JSON.stringify(projectToJson(normalized), null, 2) + "\n";
    await sandbox.writeNew(input.output_path, output);
    return { written: input.output_path, bytes: Buffer.byteLength(output), ...projectSummary(normalized) };
  }));

  server.registerTool("import_sprite_planes", {
    description: "Turn one libtandy 8x8 sprite's two 8-byte bitplanes and three visible RGBI palette entries into an editable project. Plane bits are MSB-first; index 0 remains transparent, including when a visible palette entry is black.",
    outputSchema: importedProjectSchema,
    inputSchema: z.object({
      plane0: z.array(z.number().int().min(0).max(255)).length(8),
      plane1: z.array(z.number().int().min(0).max(255)).length(8),
      palette: z.array(z.number().int().min(0).max(15)).length(3).describe("RGBI colors mapped by sprite indices 1, 2, and 3."),
      output_path: z.string().min(1).max(1024),
      name: z.string().max(48).default("imported-sprite")
    }).strict()
  }, tool(async (input) => {
    extensionMustMatch(input.output_path, [".tandy.json"], "Editable project");
    const frame = Core.decodeSpritePlanes(new Uint8Array(input.plane0), new Uint8Array(input.plane1), [0, ...input.palette]);
    const project = validateProject(projectToJson({ version: 1, name: input.name, mode: "sprite", frames: [frame],
      spriteColors: input.palette, screenBackground: 0 }));
    const output = JSON.stringify(projectToJson(project), null, 2) + "\n";
    await sandbox.writeNew(input.output_path, output);
    return { written: input.output_path, bytes: Buffer.byteLength(output), ...projectSummary(project) };
  }));

  server.registerTool("export_asset", {
    description: "Export a validated editable project as another project JSON, a libtandy C header, or a raw packed mode-9 screen. Sprite headers include all frames and the shared three-ink palette; screen exports use one selected frame.",
    outputSchema: exportResultSchema,
    inputSchema: z.object({
      project_path: z.string().min(1).max(1024),
      output_path: z.string().min(1).max(1024),
      format: z.enum(["project-json", "sprite-c-header", "screen-c-header", "mode9-bin"]).describe("Choose an editable project copy, sprite C header, screen C header, or exact packed mode-9 binary."),
      frame_index: z.number().int().min(0).max(MAX_SCREEN_FRAMES - 1).default(0).describe("Zero-based screen frame to export as C or mode-9 bytes."),
      symbol_name: z.string().max(48).optional()
    }).strict()
  }, tool(async (input) => {
    const project = await readProject(sandbox, input.project_path);
    let data;
    let kind;
    if (input.format === "project-json") {
      extensionMustMatch(input.output_path, [".tandy.json"], "Editable project");
      data = JSON.stringify(projectToJson(project), null, 2) + "\n";
      kind = "editable project JSON";
    } else if (input.format === "sprite-c-header") {
      if (project.mode !== "sprite") throw new Error("Sprite C header export needs a sprite project.");
      extensionMustMatch(input.output_path, [".h"], "C header");
      data = Core.exportSpriteHeader(project.frames, project.spriteColors, input.symbol_name || project.name);
      kind = "libtandy sprite C header";
    } else {
      if (project.mode !== "screen") throw new Error("Mode-9 screen export needs a full-screen project.");
      if (input.frame_index >= project.frames.length) throw new Error("Selected frame is outside the project frame range.");
      if (input.format === "screen-c-header") {
        extensionMustMatch(input.output_path, [".h"], "C header");
        data = Core.exportScreenHeader(project.frames[input.frame_index], input.symbol_name || project.name, project.screenBackground);
        kind = "libtandy mode-9 C header";
      } else {
        extensionMustMatch(input.output_path, [".bin"], "Mode-9 binary");
        data = Buffer.from(Core.packMode9(project.frames[input.frame_index], project.screenBackground));
        kind = "32,768-byte packed mode-9 screen";
      }
    }
    const bytes = Buffer.isBuffer(data) ? data.length : Buffer.byteLength(data);
    await sandbox.writeNew(input.output_path, data);
    return { written: input.output_path, bytes, format: kind, ...projectSummary(project),
      selectedFrame: project.mode === "screen" ? input.frame_index : undefined };
  }));

  return server;
}

async function main() {
  const server = await createServer(requestedWorkdir());
  await server.connect(new StdioServerTransport());
}

main().catch((error) => {
  process.stderr.write(`libtandy graphics MCP server: ${error.message}\n`);
  process.exitCode = 1;
});
