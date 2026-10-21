/* SPDX-License-Identifier: MIT */
/* Pure format and conversion helpers shared by the browser editor and tests. */
(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.TandyGraphicsCore = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";

  const WIDTH = 320;
  const HEIGHT = 200;
  const BYTES = 32768;
  const RGBI = [
    [0, 0, 0], [0, 0, 170], [0, 170, 0], [0, 170, 170],
    [170, 0, 0], [170, 0, 170], [170, 85, 0], [170, 170, 170],
    [85, 85, 85], [85, 85, 255], [85, 255, 85], [85, 255, 255],
    [255, 85, 85], [255, 85, 255], [255, 255, 85], [255, 255, 255]
  ];
  const RGBI_NAMES = ["black", "blue", "green", "cyan", "red", "magenta", "brown", "light gray",
    "dark gray", "bright blue", "bright green", "bright cyan", "bright red", "bright magenta", "yellow", "white"];
  const BAYER_8 = [
    [0,32,8,40,2,34,10,42], [48,16,56,24,50,18,58,26],
    [12,44,4,36,14,46,6,38], [60,28,52,20,62,30,54,22],
    [3,35,11,43,1,33,9,41], [51,19,59,27,49,17,57,25],
    [15,47,7,39,13,45,5,37], [63,31,55,23,61,29,53,21]
  ];

  function createFrame(width, height, fillIndex, opaque) {
    const indices = new Uint8Array(width * height);
    const alpha = new Uint8Array(width * height);
    if (fillIndex) indices.fill(fillIndex & 15);
    if (opaque) alpha.fill(1);
    return { width, height, indices, alpha };
  }

  function pixelOffset(x, y, width, height) {
    if (!Number.isInteger(x) || !Number.isInteger(y) || x < 0 || y < 0 ||
        x >= width || y >= height) return -1;
    return y * width + x;
  }

  function mode9Offset(x, y) {
    if (!Number.isInteger(x) || !Number.isInteger(y) ||
        x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return -1;
    return (y & 3) * 8192 + (y >> 2) * 160 + (x >> 1);
  }

  function colorDistanceSquared(a, b) {
    const r = a[0] - b[0];
    const g = a[1] - b[1];
    const bl = a[2] - b[2];
    return r * r + g * g + bl * bl;
  }

  /* Decode the sRGB transfer curve before perceptual comparison or diffusion. */
  function srgbToLinear(value) {
    const channel = value / 255;
    return channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4);
  }

  function linearToSrgb(value) {
    const channel = Math.max(0, Math.min(1, value));
    const encoded = channel <= 0.0031308 ? 12.92 * channel : 1.055 * Math.pow(channel, 1 / 2.4) - 0.055;
    return Math.round(encoded * 255);
  }

  /* OKLab conversion, following Björn Ottosson's published sRGB reference. */
  function linearToOklab(rgb) {
    const l = Math.cbrt(0.4122214708 * rgb[0] + 0.5363325363 * rgb[1] + 0.0514459929 * rgb[2]);
    const m = Math.cbrt(0.2119034982 * rgb[0] + 0.6806995451 * rgb[1] + 0.1073969566 * rgb[2]);
    const s = Math.cbrt(0.0883024619 * rgb[0] + 0.2817188376 * rgb[1] + 0.6299787005 * rgb[2]);
    return [0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
      1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
      0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s];
  }

  function oklabToLinear(lab) {
    const l = Math.pow(lab[0] + 0.3963377774 * lab[1] + 0.2158037573 * lab[2], 3);
    const m = Math.pow(lab[0] - 0.1055613458 * lab[1] - 0.0638541728 * lab[2], 3);
    const s = Math.pow(lab[0] - 0.0894841775 * lab[1] - 1.2914855480 * lab[2], 3);
    return [
      Math.max(0, Math.min(1, 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s)),
      Math.max(0, Math.min(1, -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s)),
      Math.max(0, Math.min(1, -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s))
    ];
  }

  const PALETTE_LINEAR = RGBI.map((rgb) => rgb.map(srgbToLinear));
  const PALETTE_OKLAB = PALETTE_LINEAR.map(linearToOklab);
  const ALL_COLOR_INDICES = Array.from({ length: 16 }, (_, index) => index);

  function labDistanceSquared(a, b) {
    const l = a[0] - b[0]; const ca = a[1] - b[1]; const cb = a[2] - b[2];
    return l * l + ca * ca + cb * cb;
  }

  function nearestLab(lab, candidates) {
    let best = candidates[0];
    let distance = labDistanceSquared(lab, PALETTE_OKLAB[best]);
    for (let i = 1; i < candidates.length; i += 1) {
      const index = candidates[i];
      const next = labDistanceSquared(lab, PALETTE_OKLAB[index]);
      if (next < distance) { best = index; distance = next; }
    }
    return best;
  }

  function nearestColor(rgb, candidates) {
    return nearestLab(linearToOklab(rgb.map(srgbToLinear)), candidates);
  }

  function percentile(sorted, proportion) {
    if (!sorted.length) return 0;
    return sorted[Math.min(sorted.length - 1, Math.floor((sorted.length - 1) * proportion))];
  }

  function finiteOption(options, key, fallback) {
    if (options[key] == null || options[key] === "") return fallback;
    const value = Number(options[key]);
    return Number.isFinite(value) ? value : fallback;
  }

  /* Apply alpha/keying and all tone controls once, before any dither error is added. */
  function prepareColorBuffer(rgba, width, height, options) {
    const opts = options || {};
    if (!rgba || rgba.length !== width * height * 4) throw new RangeError("Pixel data length does not match its dimensions.");
    const pixelCount = width * height;
    const work = new Float32Array(pixelCount * 3);
    const opaque = new Uint8Array(pixelCount);
    const lightnessValues = [];
    const modeKey = opts.backgroundMode === "color";
    const keyRgb = RGBI[Math.max(0, Math.min(15, finiteOption(opts, "backgroundColor", 0) | 0))];
    const tolerance = Math.max(0, Math.min(255, finiteOption(opts, "backgroundTolerance", 0) | 0));
    const alphaThreshold = Math.max(1, Math.min(255, finiteOption(opts, "alphaThreshold", 128) | 0));

    for (let p = 0; p < pixelCount; p += 1) {
      const q = p * 4;
      const rgb8 = [rgba[q], rgba[q + 1], rgba[q + 2]];
      const keyed = modeKey && colorDistanceSquared(rgb8, keyRgb) <= tolerance * tolerance * 3;
      if (rgba[q + 3] < alphaThreshold || keyed) continue;
      const offset = p * 3;
      const linear = rgb8.map(srgbToLinear);
      work[offset] = linear[0];
      work[offset + 1] = linear[1];
      work[offset + 2] = linear[2];
      opaque[p] = 1;
      lightnessValues.push(linearToOklab(linear)[0]);
    }

    const exposure = Math.max(-2, Math.min(2, finiteOption(opts, "exposure", 0)));
    const contrast = Math.max(0, Math.min(2, finiteOption(opts, "contrast", 100) / 100));
    const saturation = Math.max(0, Math.min(2, finiteOption(opts, "saturation", 100) / 100));
    const gamma = Math.max(0.5, Math.min(2, finiteOption(opts, "gamma", 100) / 100));
    let autoStrength = opts.autoCorrect && !opts.pixelArt
      ? Math.max(0, Math.min(1, finiteOption(opts, "autoStrength", 0) / 100)) : 0;
    let low = 0;
    let high = 1;
    if (autoStrength > 0 && lightnessValues.length > 1) {
      lightnessValues.sort((a, b) => a - b);
      low = percentile(lightnessValues, 0.01);
      high = percentile(lightnessValues, 0.99);
      if (high - low < 0.08) autoStrength = 0;
    }

    for (let p = 0; p < pixelCount; p += 1) {
      if (!opaque[p]) continue;
      const offset = p * 3;
      const lab = linearToOklab([work[offset], work[offset + 1], work[offset + 2]]);
      let light = lab[0];
      if (autoStrength > 0 && high > low) {
        const corrected = Math.max(0, Math.min(1, (light - low) / (high - low) * 0.92 + 0.04));
        light += (corrected - light) * autoStrength;
      }
      light = Math.max(0, Math.min(1, light * Math.pow(2, exposure)));
      light = Math.max(0, Math.min(1, (light - 0.5) * contrast + 0.5));
      light = Math.max(0, Math.min(1, Math.pow(light, 1 / gamma)));
      const corrected = oklabToLinear([light, lab[1] * saturation, lab[2] * saturation]);
      work[offset] = corrected[0];
      work[offset + 1] = corrected[1];
      work[offset + 2] = corrected[2];
    }
    return { work, opaque };
  }

  function bayerThreshold(x, y) {
    /* Recursive 8×8 Bayer threshold matrix, normalized to a zero-mean spread. */
    return ((BAYER_8[y & 7][x & 7] + 0.5) / 64) - 0.5;
  }

  function detectImageKind(rgba, width, height) {
    const total = width * height;
    const stride = Math.max(1, Math.floor(total / 16000));
    const colors = new Set();
    let samples = 0; let edges = 0;
    for (let p = 0; p < total; p += stride) {
      const q = p * 4;
      if (rgba[q + 3] < 128) continue;
      colors.add(((rgba[q] >> 4) << 8) | ((rgba[q + 1] >> 4) << 4) | (rgba[q + 2] >> 4));
      samples += 1;
      const x = p % width;
      if (x + stride < width) {
        const n = q + stride * 4;
        if (Math.abs(rgba[q] - rgba[n]) + Math.abs(rgba[q + 1] - rgba[n + 1]) + Math.abs(rgba[q + 2] - rgba[n + 2]) > 72) edges += 1;
      }
      if (colors.size > 512 && samples > 100) break;
    }
    const edgeRatio = samples ? edges / samples : 0;
    if (colors.size <= 64) return { kind: "pixel-art", colors: colors.size, edgeRatio };
    if (colors.size > 280 && edgeRatio > 0.11) return { kind: "photo", colors: colors.size, edgeRatio };
    return { kind: "illustration", colors: colors.size, edgeRatio };
  }

  function convertRgba(rgba, width, height, options) {
    const opts = options || {};
    const mode = opts.mode === "sprite" ? "sprite" : "screen";
    const spriteColors = (opts.spriteColors || [15, 12, 11]).map((c) => Math.max(0, Math.min(15, c | 0)));
    const frame = createFrame(width, height, 0, false);
    const candidates = mode === "sprite" ? spriteColors : RGBI.map((_, i) => i);
    const pixelCount = width * height;
    const prepared = prepareColorBuffer(rgba, width, height, opts);
    const work = prepared.work;
    const opaque = prepared.opaque;
    const automatic = opts.dither === "auto";
    const ditherMode = automatic ? (opts.pixelArt ? "none" : (opts.recommendedDither || "fs"))
      : (opts.dither === true ? "fs" : (opts.dither || "none"));
    const ditherStrength = Math.max(0, Math.min(1, finiteOption(opts, "ditherStrength", 0) / 100));
    const mapErrorStrength = ditherStrength;
    function spreadError(nx, ny, weight, error) {
      if (nx < 0 || nx >= width || ny >= height) return;
      const next = ny * width + nx;
      if (!opaque[next]) return;
      const offset = next * 3;
      for (let channel = 0; channel < 3; channel += 1) {
        work[offset + channel] = Math.max(0, Math.min(1,
          work[offset + channel] + error[channel] * weight * mapErrorStrength));
      }
    }
    for (let y = 0; y < height; y += 1) {
      const reverse = ditherMode === "fs" && (y & 1) === 1;
      for (let scan = 0; scan < width; scan += 1) {
        const x = reverse ? width - 1 - scan : scan;
        const p = y * width + x;
        if (!opaque[p]) { frame.indices[p] = 0; frame.alpha[p] = 0; continue; }
        let linear = [work[p * 3], work[p * 3 + 1], work[p * 3 + 2]];
        let lab = linearToOklab(linear);
        if (ditherMode === "ordered" && ditherStrength > 0) {
          const offset = bayerThreshold(x, y) * 0.26 * ditherStrength;
          linear = linear.map((channel) => Math.max(0, Math.min(1, channel + offset)));
          lab = linearToOklab(linear);
        }
        const color = nearestLab(lab, candidates);
        frame.alpha[p] = 1;
        frame.indices[p] = mode === "sprite" ? spriteColors.indexOf(color) + 1 : color;
        if (ditherMode !== "fs" || mapErrorStrength <= 0) continue;
        const quantized = PALETTE_LINEAR[color];
        const error = [linear[0] - quantized[0], linear[1] - quantized[1], linear[2] - quantized[2]];
        const direction = reverse ? -1 : 1;
        spreadError(x + direction, y, 7 / 16, error);
        spreadError(x - direction, y + 1, 3 / 16, error);
        spreadError(x, y + 1, 5 / 16, error);
        spreadError(x + direction, y + 1, 1 / 16, error);
      }
    }
    return frame;
  }

  /* Choose a three-ink palette by minimizing total OKLab error over the 16 RGBI colors. */
  function recommendSpriteColors(rgba, width, height, options) {
    const opts = options || {};
    const prepared = prepareColorBuffer(rgba, width, height, opts);
    const labs = [];
    const counts = new Uint32Array(16);
    for (let p = 0; p < prepared.opaque.length; p += 1) {
      if (!prepared.opaque[p]) continue;
      const offset = p * 3;
      const lab = linearToOklab([prepared.work[offset], prepared.work[offset + 1], prepared.work[offset + 2]]);
      labs.push(lab);
      const closest = nearestLab(lab, ALL_COLOR_INDICES);
      counts[closest] += 1;
    }
    const fallback = (opts.spriteColors || [14, 0, 15]).map((c) => Math.max(0, Math.min(15, c | 0)));
    if (!labs.length) return fallback.slice(0, 3);

    const individuallyBest = Array.from(counts.keys()).filter((index) => counts[index] > 0)
      .sort((a, b) => counts[b] - counts[a] || a - b);
    if (individuallyBest.length <= 3) {
      const selected = individuallyBest.slice();
      for (const index of fallback) if (selected.length < 3 && !selected.includes(index)) selected.push(index);
      for (let index = 0; selected.length < 3 && index < 16; index += 1) if (!selected.includes(index)) selected.push(index);
      return selected;
    }

    const distances = new Float32Array(labs.length * 16);
    for (let p = 0; p < labs.length; p += 1) {
      for (let color = 0; color < 16; color += 1) {
        distances[p * 16 + color] = labDistanceSquared(labs[p], PALETTE_OKLAB[color]);
      }
    }
    let best = [0, 1, 2];
    let bestScore = Infinity;
    for (let a = 0; a < 14; a += 1) for (let b = a + 1; b < 15; b += 1) for (let c = b + 1; c < 16; c += 1) {
      let score = 0;
      for (let p = 0; p < labs.length; p += 1) {
        const offset = p * 16;
        score += Math.min(distances[offset + a], distances[offset + b], distances[offset + c]);
      }
      if (score < bestScore) { bestScore = score; best = [a, b, c]; }
    }
    const useCounts = new Uint32Array(16);
    for (let p = 0; p < labs.length; p += 1) {
      const offset = p * 16;
      let chosen = best[0];
      let distance = distances[offset + chosen];
      for (let i = 1; i < best.length; i += 1) {
        const next = best[i];
        if (distances[offset + next] < distance) { chosen = next; distance = distances[offset + next]; }
      }
      useCounts[chosen] += 1;
    }
    return best.sort((a, b) => useCounts[b] - useCounts[a] || a - b);
  }

  function packMode9(frame, transparentColor) {
    if (!frame || frame.width !== WIDTH || frame.height !== HEIGHT ||
        frame.indices.length !== WIDTH * HEIGHT || frame.alpha.length !== WIDTH * HEIGHT) {
      throw new RangeError("Mode-9 export requires a 320 x 200 image.");
    }
    const packed = new Uint8Array(BYTES);
    const background = (transparentColor == null ? 0 : transparentColor) & 15;
    for (let y = 0; y < HEIGHT; y += 1) {
      const row = (y & 3) * 8192 + (y >> 2) * 160;
      for (let x = 0; x < WIDTH; x += 1) {
        const p = y * WIDTH + x;
        const color = (frame.alpha[p] ? frame.indices[p] : background) & 15;
        const address = row + (x >> 1);
        packed[address] = (x & 1)
          ? ((packed[address] & 0xf0) | color)
          : ((packed[address] & 0x0f) | (color << 4));
      }
    }
    return packed;
  }

  function unpackMode9(packed) {
    if (!packed || packed.length !== BYTES) throw new RangeError("A mode-9 image is exactly 32,768 bytes.");
    const frame = createFrame(WIDTH, HEIGHT, 0, true);
    for (let y = 0; y < HEIGHT; y += 1) {
      const row = (y & 3) * 8192 + (y >> 2) * 160;
      for (let x = 0; x < WIDTH; x += 1) {
        const byte = packed[row + (x >> 1)];
        frame.indices[y * WIDTH + x] = (x & 1) ? byte & 15 : (byte >> 4) & 15;
      }
    }
    return frame;
  }

  function encodeSpritePlanes(frame) {
    if (!frame || frame.width !== 8 || frame.height !== 8 ||
        frame.indices.length !== 64 || frame.alpha.length !== 64) {
      throw new RangeError("libtandy sprites are exactly 8 x 8 pixels.");
    }
    const plane0 = new Uint8Array(8);
    const plane1 = new Uint8Array(8);
    for (let y = 0; y < 8; y += 1) {
      for (let x = 0; x < 8; x += 1) {
        const p = y * 8 + x;
        const sourceIndex = frame.indices[p];
        if (!Number.isInteger(sourceIndex) || sourceIndex < 0 || sourceIndex > 3) {
          throw new RangeError("Sprite color indices must be from 0 to 3.");
        }
        if (frame.alpha[p] && sourceIndex === 0) {
          throw new RangeError("Sprite index 0 is transparent; use ink slot 1 for opaque black.");
        }
        const index = frame.alpha[p] ? sourceIndex : 0;
        const bit = 0x80 >> x;
        if (index & 1) plane0[y] |= bit;
        if (index & 2) plane1[y] |= bit;
      }
    }
    return { plane0, plane1 };
  }

  function decodeSpritePlanes(plane0, plane1, palette) {
    if (plane0.length !== 8 || plane1.length !== 8 || palette.length !== 4) {
      throw new RangeError("A libtandy sprite needs two 8-byte planes and four palette entries.");
    }
    const frame = createFrame(8, 8, 0, false);
    for (let y = 0; y < 8; y += 1) {
      for (let x = 0; x < 8; x += 1) {
        const bit = 0x80 >> x;
        const index = ((plane0[y] & bit) ? 1 : 0) | ((plane1[y] & bit) ? 2 : 0);
        const p = y * 8 + x;
        if (index !== 0) {
          frame.indices[p] = index;
          frame.alpha[p] = 1;
        }
      }
    }
    return frame;
  }

  function cleanName(name) {
    const cleaned = String(name || "tandy_art").toLowerCase()
      .replace(/[^a-z0-9_]+/g, "_").replace(/^_+|_+$/g, "");
    return /^[0-9]/.test(cleaned) ? "asset_" + cleaned : (cleaned || "tandy_art");
  }

  function hex(value) { return "0x" + (value & 255).toString(16).toUpperCase().padStart(2, "0") + "U"; }
  function paletteDescription(index) { return index + "=" + RGBI_NAMES[index] + " RGB(" + RGBI[index].join(",") + ")"; }

  function exportSpriteHeader(frames, colors, name) {
    const id = cleanName(name);
    const planes = frames.map(encodeSpritePlanes);
    const palette = [0, colors[0] & 15, colors[1] & 15, colors[2] & 15];
    const rows = (key, field) => planes.map((pair) => "    { " + Array.from(pair[key]).map(hex).join(", ") + " }").join(",\n");
    return [
      "/* Generated locally by libtandy Graphics Workshop. MIT-licensed editor. */",
      "/* Sprite format: 8x8 pixels, two MSB-first bitplanes (8 bytes each). */",
      "/* Index 0 is transparent. palette[0] is ignored; entries 1..3 map to RGBI indices " +
        palette.slice(1).map((value, index) => (index + 1) + "=" + paletteDescription(value)).join(", ") + ". */",
      "#ifndef " + id.toUpperCase() + "_SPRITE_H",
      "#define " + id.toUpperCase() + "_SPRITE_H",
      "#include \"lt_gfx_sprite8.h\"",
      "#define " + id.toUpperCase() + "_WIDTH 8U",
      "#define " + id.toUpperCase() + "_HEIGHT 8U",
      "#define " + id.toUpperCase() + "_FRAME_COUNT " + frames.length + "U",
      "static const lt_gfx_u8 " + id + "_plane0[" + frames.length + "][8] = {",
      rows("plane0"),
      "};",
      "static const lt_gfx_u8 " + id + "_plane1[" + frames.length + "][8] = {",
      rows("plane1"),
      "};",
      "static const lt_gfx_u8 " + id + "_palette[4] = { " + palette.map(hex).join(", ") + " };",
      "/* Draw frame 0: lt_gfx_sprite8_planes(&screen, x, y, " + id + "_plane0[0], " + id + "_plane1[0], " + id + "_palette); */",
      "#endif",
      ""
    ].join("\n");
  }

  function exportScreenHeader(frame, name, transparentColor) {
    const id = cleanName(name);
    const packed = packMode9(frame, transparentColor);
    const lines = [];
    for (let i = 0; i < packed.length; i += 16) {
      lines.push("    " + Array.from(packed.subarray(i, i + 16)).map(hex).join(", "));
    }
    return [
      "/* Generated locally by libtandy Graphics Workshop. MIT-licensed editor. */",
      "/* 320x200, packed mode-9, 32 KiB, four 8 KiB scanline banks. */",
      "/* Even x is the high nibble; odd x is the low nibble. */",
      "/* RGBI colors: " + RGBI.map((rgb, index) => paletteDescription(index)).join("; ") + ". */",
      "#ifndef " + id.toUpperCase() + "_MODE9_H",
      "#define " + id.toUpperCase() + "_MODE9_H",
      "#include \"lt_gfx.h\"",
      "#define " + id.toUpperCase() + "_WIDTH LT_GFX_WIDTH",
      "#define " + id.toUpperCase() + "_HEIGHT LT_GFX_HEIGHT",
      "#define " + id.toUpperCase() + "_BYTES LT_GFX_BYTES",
      "static lt_gfx_u8 " + id + "_mode9[LT_GFX_BYTES] = {",
      lines.join(",\n"),
      "};",
      "/* Draw: lt_gfx_surface background; lt_gfx_init(&background, " + id + "_mode9); */",
      "#endif",
      ""
    ].join("\n");
  }

  return {
    WIDTH, HEIGHT, BYTES, RGBI, RGBI_NAMES, createFrame, pixelOffset, mode9Offset,
    nearestColor, detectImageKind, srgbToLinear, linearToOklab, convertRgba, recommendSpriteColors, packMode9, unpackMode9,
    encodeSpritePlanes, decodeSpritePlanes, cleanName,
    exportSpriteHeader, exportScreenHeader
  };
});
