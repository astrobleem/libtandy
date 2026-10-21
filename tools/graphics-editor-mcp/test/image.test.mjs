import assert from "node:assert/strict";
import jpeg from "jpeg-js";
import test from "node:test";
import { decodeImage, decodePng, horizontalFrames, resizeRgba } from "../lib/png-image.mjs";

test("image decoding bounds are checked before raster allocation", () => {
  const png = Buffer.alloc(33);
  Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]).copy(png, 0);
  png.writeUInt32BE(13, 8);
  png.write("IHDR", 12, "ascii");
  png.writeUInt32BE(4097, 16);
  png.writeUInt32BE(1, 20);
  assert.throws(() => decodePng(png), /dimensions exceed/);
});

test("alpha-aware resize avoids color bleed and frame split enforces cell bounds", () => {
  const source = new Uint8Array([255, 0, 0, 255, 0, 0, 255, 0]);
  const resized = resizeRgba(source, 2, 1, 4, 2, "stretch", "bilinear");
  const edge = (0 * 4 + 2) * 4;
  assert.ok(resized[edge + 3] > 0 && resized[edge + 3] < 255);
  assert.equal(resized[edge], 255);
  assert.equal(resized[edge + 2], 0);

  const image = { width: 4, height: 1, data: Uint8Array.from([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]) };
  const frames = horizontalFrames(image, 2, 1, 2);
  assert.deepEqual(Array.from(frames[0]), [1,2,3,4,5,6,7,8]);
  assert.deepEqual(Array.from(frames[1]), [9,10,11,12,13,14,15,16]);
  assert.throws(() => horizontalFrames(image, 2, 1, 3), /sheet must be exactly/);
});

test("JPEG EXIF orientation is applied before conversion", () => {
  const rgba = Buffer.alloc(4 * 2 * 4, 255);
  const encoded = jpeg.encode({ width: 4, height: 2, data: rgba }, 100).data;
  const app1 = Buffer.alloc(36);
  app1[0] = 0xff; app1[1] = 0xe1;
  app1.writeUInt16BE(34, 2);
  app1.write("Exif\0\0", 4, "ascii");
  app1.write("II", 10, "ascii");
  app1.writeUInt16LE(42, 12);
  app1.writeUInt32LE(8, 14);
  app1.writeUInt16LE(1, 18);
  app1.writeUInt16LE(0x0112, 20);
  app1.writeUInt16LE(3, 22);
  app1.writeUInt32LE(1, 24);
  app1.writeUInt16LE(6, 28);
  app1.writeUInt32LE(0, 32);
  const image = decodeImage(Buffer.concat([encoded.subarray(0, 2), app1, encoded.subarray(2)]));
  assert.equal(image.format, "jpeg");
  assert.equal(image.width, 2);
  assert.equal(image.height, 4);
  assert.equal(image.data.length, 2 * 4 * 4);
});
