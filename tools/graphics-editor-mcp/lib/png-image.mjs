import { PNG } from "pngjs";
import jpeg from "jpeg-js";

export const MAX_PNG_DIMENSION = 4096;
export const MAX_PNG_PIXELS = 1_048_576;

export function decodePng(buffer) {
  if (!Buffer.isBuffer(buffer) || buffer.length < 33) throw new Error("Input is not a complete PNG file.");
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  if (!buffer.subarray(0, 8).equals(signature) || buffer.toString("ascii", 12, 16) !== "IHDR") {
    throw new Error("Only PNG pictures are supported by the local MCP importer.");
  }
  const width = buffer.readUInt32BE(16);
  const height = buffer.readUInt32BE(20);
  if (!width || !height || width > MAX_PNG_DIMENSION || height > MAX_PNG_DIMENSION || width * height > MAX_PNG_PIXELS) {
    throw new Error(`PNG dimensions exceed the ${MAX_PNG_DIMENSION}-pixel side or ${MAX_PNG_PIXELS}-pixel total limit.`);
  }
  let decoded;
  try { decoded = PNG.sync.read(buffer); }
  catch (error) { throw new Error(`Could not decode PNG: ${error.message}`); }
  if (decoded.width !== width || decoded.height !== height || decoded.data.length !== width * height * 4) {
    throw new Error("PNG decoder returned inconsistent image dimensions or pixel data.");
  }
  return { format: "png", width, height, data: new Uint8Array(decoded.data) };
}

function exifOrientation(buffer, segmentOffset, segmentEnd) {
  const start = segmentOffset + 2;
  if (segmentEnd - start < 14 || buffer.toString("ascii", start, start + 6) !== "Exif\0\0") return 1;
  const tiff = start + 6;
  const byteOrder = buffer.toString("ascii", tiff, tiff + 2);
  const little = byteOrder === "II";
  if (!little && byteOrder !== "MM") return 1;
  const read16 = (offset) => little ? buffer.readUInt16LE(offset) : buffer.readUInt16BE(offset);
  const read32 = (offset) => little ? buffer.readUInt32LE(offset) : buffer.readUInt32BE(offset);
  if (read16(tiff + 2) !== 42) return 1;
  const ifd = tiff + read32(tiff + 4);
  if (ifd + 2 > segmentEnd) return 1;
  const count = read16(ifd);
  for (let i = 0; i < count; i += 1) {
    const entry = ifd + 2 + i * 12;
    if (entry + 12 > segmentEnd) return 1;
    if (read16(entry) === 0x0112 && read16(entry + 2) === 3 && read32(entry + 4) >= 1) {
      const orientation = read16(entry + 8);
      return orientation >= 1 && orientation <= 8 ? orientation : 1;
    }
  }
  return 1;
}

function jpegDimensions(buffer) {
  if (buffer.length < 4 || buffer[0] !== 0xff || buffer[1] !== 0xd8) throw new Error("Input is not a complete JPEG file.");
  const startOfFrame = new Set([0xc0, 0xc1, 0xc2, 0xc3, 0xc5, 0xc6, 0xc7, 0xc9, 0xca, 0xcb, 0xcd, 0xce, 0xcf]);
  let offset = 2;
  let width = 0; let height = 0; let orientation = 1;
  while (offset < buffer.length) {
    if (buffer[offset] !== 0xff) throw new Error("JPEG marker stream is malformed.");
    while (offset < buffer.length && buffer[offset] === 0xff) offset += 1;
    if (offset >= buffer.length) break;
    const marker = buffer[offset++];
    if (marker === 0xd9 || marker === 0xda) break;
    if (marker === 0x01 || (marker >= 0xd0 && marker <= 0xd7)) continue;
    if (offset + 2 > buffer.length) throw new Error("JPEG segment is truncated.");
    const segmentLength = buffer.readUInt16BE(offset);
    if (segmentLength < 2 || offset + segmentLength > buffer.length) throw new Error("JPEG segment length is invalid.");
    if (marker === 0xe1) orientation = exifOrientation(buffer, offset, offset + segmentLength);
    if (startOfFrame.has(marker)) {
      if (segmentLength < 8) throw new Error("JPEG frame header is incomplete.");
      height = buffer.readUInt16BE(offset + 3);
      width = buffer.readUInt16BE(offset + 5);
    }
    offset += segmentLength;
  }
  if (!width || !height) throw new Error("JPEG has no supported frame header.");
  return { width, height, orientation };
}

function orientJpeg(decoded, orientation) {
  if (orientation === 1) return { width: decoded.width, height: decoded.height, data: new Uint8Array(decoded.data) };
  const swap = orientation >= 5;
  const width = swap ? decoded.height : decoded.width;
  const height = swap ? decoded.width : decoded.height;
  const data = new Uint8Array(width * height * 4);
  for (let y = 0; y < decoded.height; y += 1) for (let x = 0; x < decoded.width; x += 1) {
    let dx = x; let dy = y;
    if (orientation === 2) dx = decoded.width - 1 - x;
    else if (orientation === 3) { dx = decoded.width - 1 - x; dy = decoded.height - 1 - y; }
    else if (orientation === 4) dy = decoded.height - 1 - y;
    else if (orientation === 5) { dx = y; dy = x; }
    else if (orientation === 6) { dx = decoded.height - 1 - y; dy = x; }
    else if (orientation === 7) { dx = decoded.height - 1 - y; dy = decoded.width - 1 - x; }
    else if (orientation === 8) { dx = y; dy = decoded.width - 1 - x; }
    const from = (y * decoded.width + x) * 4;
    const to = (dy * width + dx) * 4;
    data.set(decoded.data.subarray(from, from + 4), to);
  }
  return { width, height, data };
}

export function decodeImage(buffer) {
  if (Buffer.isBuffer(buffer) && buffer.length >= 8 && buffer[0] === 137 && buffer.toString("ascii", 1, 4) === "PNG") {
    return decodePng(buffer);
  }
  const { width, height, orientation } = jpegDimensions(buffer);
  if (!width || !height || width > MAX_PNG_DIMENSION || height > MAX_PNG_DIMENSION || width * height > MAX_PNG_PIXELS) {
    throw new Error(`JPEG dimensions exceed the ${MAX_PNG_DIMENSION}-pixel side or ${MAX_PNG_PIXELS}-pixel total limit.`);
  }
  let decoded;
  try { decoded = jpeg.decode(buffer, { useTArray: true, formatAsRGBA: true }); }
  catch (error) { throw new Error(`Could not decode JPEG: ${error.message}`); }
  if (decoded.width !== width || decoded.height !== height || decoded.data.length !== width * height * 4) {
    throw new Error("JPEG decoder returned inconsistent image dimensions or pixel data.");
  }
  return { format: "jpeg", ...orientJpeg(decoded, orientation) };
}

function bilinearPixel(source, sourceWidth, sourceHeight, x, y, output, offset) {
  x = Math.max(0, Math.min(sourceWidth - 1, x));
  y = Math.max(0, Math.min(sourceHeight - 1, y));
  const x0 = Math.floor(x); const y0 = Math.floor(y);
  const fx = x - x0; const fy = y - y0;
  let alpha = 0; let red = 0; let green = 0; let blue = 0;
  for (let dy = 0; dy <= 1; dy += 1) {
    const sy = y0 + dy;
    if (sy < 0 || sy >= sourceHeight) continue;
    const wy = dy ? fy : 1 - fy;
    for (let dx = 0; dx <= 1; dx += 1) {
      const sx = x0 + dx;
      if (sx < 0 || sx >= sourceWidth) continue;
      const weight = wy * (dx ? fx : 1 - fx);
      const from = (sy * sourceWidth + sx) * 4;
      const opacity = source[from + 3] / 255 * weight;
      alpha += opacity;
      red += source[from] * opacity;
      green += source[from + 1] * opacity;
      blue += source[from + 2] * opacity;
    }
  }
  output[offset] = alpha ? Math.round(red / alpha) : 0;
  output[offset + 1] = alpha ? Math.round(green / alpha) : 0;
  output[offset + 2] = alpha ? Math.round(blue / alpha) : 0;
  output[offset + 3] = Math.max(0, Math.min(255, Math.round(alpha * 255)));
}

export function resizeRgba(source, sourceWidth, sourceHeight, targetWidth, targetHeight, fit, filter) {
  if (source.length !== sourceWidth * sourceHeight * 4) throw new Error("RGBA pixel data length does not match image dimensions.");
  if (!Number.isInteger(targetWidth) || !Number.isInteger(targetHeight) || targetWidth < 1 || targetHeight < 1) {
    throw new Error("Target image dimensions must be positive integers.");
  }
  if (sourceWidth === targetWidth && sourceHeight === targetHeight) return new Uint8Array(source);
  if (!["fit", "crop", "stretch"].includes(fit)) throw new Error("Resize mode must be fit, crop, or stretch.");
  if (!["nearest", "bilinear"].includes(filter)) throw new Error("Resampling must be nearest or bilinear.");

  let scaleX; let scaleY;
  let left = 0; let top = 0;
  if (fit === "stretch") {
    scaleX = targetWidth / sourceWidth;
    scaleY = targetHeight / sourceHeight;
  } else {
    const scale = fit === "fit"
      ? Math.min(targetWidth / sourceWidth, targetHeight / sourceHeight)
      : Math.max(targetWidth / sourceWidth, targetHeight / sourceHeight);
    scaleX = scale; scaleY = scale;
    left = (targetWidth - sourceWidth * scale) / 2;
    top = (targetHeight - sourceHeight * scale) / 2;
  }

  const output = new Uint8Array(targetWidth * targetHeight * 4);
  for (let y = 0; y < targetHeight; y += 1) {
    const sy = (y + 0.5 - top) / scaleY - 0.5;
    for (let x = 0; x < targetWidth; x += 1) {
      const sx = (x + 0.5 - left) / scaleX - 0.5;
      const offset = (y * targetWidth + x) * 4;
      if (fit === "fit" && (sx < -0.5 || sy < -0.5 || sx > sourceWidth - 0.5 || sy > sourceHeight - 0.5)) continue;
      if (filter === "nearest") {
        const px = Math.max(0, Math.min(sourceWidth - 1, Math.round(sx)));
        const py = Math.max(0, Math.min(sourceHeight - 1, Math.round(sy)));
        const from = (py * sourceWidth + px) * 4;
        output.set(source.subarray(from, from + 4), offset);
      } else {
        bilinearPixel(source, sourceWidth, sourceHeight, sx, sy, output, offset);
      }
    }
  }
  return output;
}

export function horizontalFrames(image, frameWidth, frameHeight, count) {
  if (!Number.isInteger(count) || count < 1 || image.width !== frameWidth * count || image.height !== frameHeight) {
    throw new Error(`A ${count}-frame horizontal sheet must be exactly ${frameWidth * count} x ${frameHeight} pixels.`);
  }
  const frames = [];
  for (let frame = 0; frame < count; frame += 1) {
    const rgba = new Uint8Array(frameWidth * frameHeight * 4);
    for (let y = 0; y < frameHeight; y += 1) {
      const sourceOffset = (y * image.width + frame * frameWidth) * 4;
      const targetOffset = y * frameWidth * 4;
      rgba.set(image.data.subarray(sourceOffset, sourceOffset + frameWidth * 4), targetOffset);
    }
    frames.push(rgba);
  }
  return frames;
}
