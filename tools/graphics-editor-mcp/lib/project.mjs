import fs from "node:fs/promises";
import path from "node:path";

export const MAX_FILE_BYTES = 10 * 1024 * 1024;
export const MAX_PROJECT_BYTES = 8 * 1024 * 1024;
export const MAX_PIXELS_TOTAL = 1_048_576;
export const MAX_SCREEN_FRAMES = 16;
export const MAX_SPRITE_FRAMES = 64;

const within = (root, candidate) => {
  const relative = path.relative(root, candidate);
  return relative === "" || (!relative.startsWith(".." + path.sep) && relative !== ".." && !path.isAbsolute(relative));
};

function relativeParts(value) {
  if (typeof value !== "string" || value.length < 1 || value.length > 1024 || value.includes("\0")) {
    throw new Error("Use a non-empty relative file path of at most 1,024 characters.");
  }
  if (path.posix.isAbsolute(value) || path.win32.isAbsolute(value) || /^[A-Za-z]:/.test(value) || value.includes(":")) {
    throw new Error("File paths must be relative to the configured working folder.");
  }
  const parts = value.split(/[\\/]+/).filter(Boolean);
  if (!parts.length || parts.some((part) => part === ".." || part === ".")) {
    throw new Error("File paths cannot contain '.' or '..' path segments.");
  }
  return parts;
}

export async function createFileSandbox(workdir) {
  const root = await fs.realpath(path.resolve(workdir));
  const stat = await fs.stat(root);
  if (!stat.isDirectory()) throw new Error("The configured working folder is not a directory.");

  function candidateFor(relativePath) {
    const candidate = path.resolve(root, ...relativeParts(relativePath));
    if (!within(root, candidate)) throw new Error("File path escapes the configured working folder.");
    return candidate;
  }

  return {
    root,
    async read(relativePath, maxBytes = MAX_FILE_BYTES) {
      const candidate = candidateFor(relativePath);
      const resolved = await fs.realpath(candidate);
      if (!within(root, resolved)) throw new Error("Input file resolves outside the configured working folder.");
      const handle = await fs.open(resolved, "r");
      try {
        const stat = await handle.stat();
        if (!stat.isFile()) throw new Error("The selected input path is not a regular file.");
        if (stat.size > maxBytes) throw new Error(`Input file exceeds the ${maxBytes}-byte limit.`);
        const bounded = Buffer.allocUnsafe(maxBytes + 1);
        let offset = 0;
        while (offset < bounded.length) {
          const { bytesRead } = await handle.read(bounded, offset, bounded.length - offset, offset);
          if (!bytesRead) break;
          offset += bytesRead;
        }
        if (offset > maxBytes) throw new Error(`Input file exceeds the ${maxBytes}-byte limit.`);
        return Buffer.from(bounded.subarray(0, offset));
      } finally {
        await handle.close();
      }
    },
    async writeNew(relativePath, data) {
      const candidate = candidateFor(relativePath);
      const parent = await fs.realpath(path.dirname(candidate));
      if (!within(root, parent)) throw new Error("Output folder resolves outside the configured working folder.");
      const output = path.join(parent, path.basename(candidate));
      let handle;
      try {
        handle = await fs.open(output, "wx", 0o600);
        await handle.writeFile(data);
      } catch (error) {
        if (handle) {
          await handle.close().catch(() => {});
          await fs.unlink(output).catch(() => {});
        }
        if (error?.code === "EEXIST") throw new Error("Output already exists; choose a new path to avoid overwriting it.");
        if (error?.code === "ENOENT") throw new Error("Output folder must already exist inside the configured working folder.");
        throw error;
      }
      await handle.close();
      return output;
    }
  };
}

function isIntegerIn(value, low, high) {
  return Number.isInteger(value) && value >= low && value <= high;
}

export function validateProject(source) {
  if (!source || typeof source !== "object" || Array.isArray(source)) throw new Error("Project JSON must contain an object.");
  if (source.format != null && source.format !== "libtandy-graphics-workshop") throw new Error("Unsupported project format.");
  if (source.version !== 1) throw new Error("Only graphics project version 1 is supported.");
  if (source.mode !== "screen" && source.mode !== "sprite") throw new Error("Project mode must be 'screen' or 'sprite'.");
  if (!Array.isArray(source.frames) || source.frames.length < 1) throw new Error("Project must contain at least one frame.");
  const maxFrames = source.mode === "screen" ? MAX_SCREEN_FRAMES : MAX_SPRITE_FRAMES;
  if (source.frames.length > maxFrames) throw new Error(`Project exceeds the ${maxFrames}-frame limit for ${source.mode} art.`);
  const width = source.mode === "screen" ? 320 : 8;
  const height = source.mode === "screen" ? 200 : 8;
  const pixelsPerFrame = width * height;
  if (source.frames.length * pixelsPerFrame > MAX_PIXELS_TOTAL) throw new Error("Project contains too many pixels.");

  const frames = source.frames.map((frame, index) => {
    if (!frame || typeof frame !== "object" || frame.width !== width || frame.height !== height) {
      throw new Error(`Frame ${index + 1} must be exactly ${width} x ${height} pixels for ${source.mode} mode.`);
    }
    if (!Array.isArray(frame.indices) || !Array.isArray(frame.alpha) ||
        frame.indices.length !== pixelsPerFrame || frame.alpha.length !== pixelsPerFrame) {
      throw new Error(`Frame ${index + 1} has invalid index or transparency data length.`);
    }
    const indices = new Uint8Array(pixelsPerFrame);
    const alpha = new Uint8Array(pixelsPerFrame);
    const maxIndex = source.mode === "sprite" ? 3 : 15;
    for (let p = 0; p < pixelsPerFrame; p += 1) {
      const color = frame.indices[p];
      const opaque = frame.alpha[p] === true ? 1 : frame.alpha[p] === false ? 0 : frame.alpha[p];
      if (!isIntegerIn(color, 0, maxIndex)) {
        throw new Error(source.mode === "sprite" ? `Frame ${index + 1} has a sprite index outside 0..3.` : `Frame ${index + 1} has a color index outside 0..15.`);
      }
      if (opaque !== 0 && opaque !== 1) throw new Error(`Frame ${index + 1} has a transparency value other than 0 or 1.`);
      if (source.mode === "sprite" && opaque === 1 && color === 0) {
        throw new Error("Sprite index 0 is transparent; use ink slot 1 for opaque black.");
      }
      indices[p] = color;
      alpha[p] = opaque;
    }
    return { width, height, indices, alpha };
  });

  const sourceColors = source.spriteColors == null ? [14, 0, 15] : source.spriteColors;
  if (!Array.isArray(sourceColors) || sourceColors.length !== 3 || sourceColors.some((color) => !isIntegerIn(color, 0, 15))) {
    throw new Error("A sprite palette must contain exactly three Tandy colors from 0 to 15.");
  }
  const background = source.screenBackground == null ? 0 : source.screenBackground;
  if (!isIntegerIn(background, 0, 15)) throw new Error("Screen background color must be from 0 to 15.");
  const name = typeof source.name === "string" ? source.name.slice(0, 48) : "untitled";
  return { version: 1, name: name || "untitled", mode: source.mode, frames,
    spriteColors: sourceColors.slice(), screenBackground: background };
}

export function projectToJson(project) {
  return {
    format: "libtandy-graphics-workshop",
    version: 1,
    name: project.name,
    mode: project.mode,
    spriteColors: project.spriteColors.slice(),
    screenBackground: project.screenBackground,
    frames: project.frames.map((frame) => ({
      width: frame.width,
      height: frame.height,
      indices: Array.from(frame.indices),
      alpha: Array.from(frame.alpha)
    }))
  };
}

export function projectSummary(project) {
  return {
    name: project.name,
    mode: project.mode,
    width: project.frames[0].width,
    height: project.frames[0].height,
    frameCount: project.frames.length,
    spriteColors: project.spriteColors,
    screenBackground: project.screenBackground,
    transparentPixels: project.frames.reduce((sum, frame) => sum + frame.alpha.reduce((count, value) => count + (value ? 0 : 1), 0), 0),
    opaqueBlackPixels: project.mode === "screen"
      ? project.frames.reduce((sum, frame) => sum + frame.indices.reduce((count, value, p) => count + (value === 0 && frame.alpha[p] ? 1 : 0), 0), 0)
      : project.frames.reduce((sum, frame) => sum + frame.indices.reduce((count, value, p) => count + (value !== 0 && frame.alpha[p] && project.spriteColors[value - 1] === 0 ? 1 : 0), 0), 0)
  };
}
