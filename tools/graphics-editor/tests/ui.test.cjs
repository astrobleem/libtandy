/* SPDX-License-Identifier: MIT */
/* Browser smoke test using an installed Chromium browser and its local CDP endpoint. */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const http = require("node:http");
const os = require("node:os");
const path = require("node:path");
const { spawn } = require("node:child_process");
const zlib = require("node:zlib");

const APP_ROOT = path.resolve(__dirname, "..");
const BROWSER_CANDIDATES = [
  process.env.CHROME_PATH,
  "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
  "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"
].filter(Boolean);
const BROWSER = BROWSER_CANDIDATES.find((candidate) => fs.existsSync(candidate));
  if (!BROWSER) throw new Error("Set CHROME_PATH to an already-installed Chrome or Edge executable.");

const MIME = { ".html": "text/html", ".css": "text/css", ".js": "text/javascript", ".json": "application/json", ".svg": "image/svg+xml" };
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const clamp = (value) => Math.max(0, Math.min(255, Math.round(value)));
function requestJson(url) {
  return new Promise((resolve, reject) => {
    let settled = false;
    const timer = setTimeout(() => {
      settled = true;
      request.destroy(new Error("Browser debugging endpoint timed out."));
    }, 3000);
    const request = http.get(url, { agent: false }, (response) => {
      const chunks = [];
      response.on("data", (chunk) => chunks.push(chunk));
      response.on("end", () => {
        if (settled) return;
        clearTimeout(timer);
        if (response.statusCode < 200 || response.statusCode >= 300) return reject(new Error("Browser debugging endpoint returned HTTP " + response.statusCode));
        try { resolve(JSON.parse(Buffer.concat(chunks).toString("utf8"))); }
        catch (error) { reject(error); }
      });
    });
    request.on("error", (error) => { clearTimeout(timer); reject(error); });
  });
}

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) crc = (crc >>> 1) ^ (crc & 1 ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function pngChunk(name, data) {
  const type = Buffer.from(name, "ascii");
  const length = Buffer.alloc(4); length.writeUInt32BE(data.length);
  const checksum = Buffer.alloc(4); checksum.writeUInt32BE(crc32(Buffer.concat([type, data])));
  return Buffer.concat([length, type, data, checksum]);
}
function encodePng(width, height, rgba) {
  const rows = Buffer.alloc(height * (width * 4 + 1));
  for (let y = 0; y < height; y += 1) {
    const start = y * (width * 4 + 1);
    rows[start] = 0;
    Buffer.from(rgba.buffer, rgba.byteOffset + y * width * 4, width * 4).copy(rows, start + 1);
  }
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0); header.writeUInt32BE(height, 4);
  header[8] = 8; header[9] = 6;
  return Buffer.concat([
    Buffer.from([137,80,78,71,13,10,26,10]), pngChunk("IHDR", header),
    pngChunk("IDAT", zlib.deflateSync(rows)), pngChunk("IEND", Buffer.alloc(0))
  ]);
}
function decodePng(buffer) {
  let offset = 8; let width = 0; let height = 0; let bitDepth = 0; let colorType = 0;
  const compressed = [];
  while (offset < buffer.length) {
    const length = buffer.readUInt32BE(offset); offset += 4;
    const name = buffer.toString("ascii", offset, offset + 4); offset += 4;
    const data = buffer.subarray(offset, offset + length); offset += length + 4;
    if (name === "IHDR") { width = data.readUInt32BE(0); height = data.readUInt32BE(4); bitDepth = data[8]; colorType = data[9]; }
    if (name === "IDAT") compressed.push(data);
    if (name === "IEND") break;
  }
  assert.equal(bitDepth, 8, "PNG output uses 8-bit samples");
  assert.equal(colorType, 6, "transparent PNG output uses RGBA samples");
  const raw = zlib.inflateSync(Buffer.concat(compressed));
  const pixels = new Uint8Array(width * height * 4);
  for (let y = 0; y < height; y += 1) {
    const rowStart = y * (width * 4 + 1);
    const filter = raw[rowStart];
    for (let x = 0; x < width * 4; x += 1) {
      const source = raw[rowStart + x + 1];
      const left = x >= 4 ? pixels[y * width * 4 + x - 4] : 0;
      const up = y > 0 ? pixels[(y - 1) * width * 4 + x] : 0;
      const upperLeft = y > 0 && x >= 4 ? pixels[(y - 1) * width * 4 + x - 4] : 0;
      let predictor = 0;
      if (filter === 1) predictor = left;
      else if (filter === 2) predictor = up;
      else if (filter === 3) predictor = Math.floor((left + up) / 2);
      else if (filter === 4) {
        const p = left + up - upperLeft;
        const da = Math.abs(p - left), db = Math.abs(p - up), dc = Math.abs(p - upperLeft);
        predictor = da <= db && da <= dc ? left : db <= dc ? up : upperLeft;
      } else assert.equal(filter, 0, "PNG row filter is supported");
      pixels[y * width * 4 + x] = (source + predictor) & 255;
    }
  }
  return { width, height, pixels };
}
function makePhotoLikeImage(width, height) {
  const rgba = new Uint8Array(width * height * 4);
  for (let y = 0; y < height; y += 1) for (let x = 0; x < width; x += 1) {
    const p = (y * width + x) * 4;
    const grain = (x * 37 + y * 71 + x * y * 13) & 63;
    rgba[p] = clamp(x * 2 + grain);
    rgba[p + 1] = clamp(y * 3 + grain * 2);
    rgba[p + 2] = clamp(70 + x + grain);
    rgba[p + 3] = 255;
  }
  return rgba;
}
function makeSpriteImage() {
  const rgba = new Uint8Array(8 * 8 * 4);
  for (let y = 0; y < 8; y += 1) for (let x = 0; x < 8; x += 1) {
    const p = (y * 8 + x) * 4;
    if (x === 1 && y === 0) { rgba[p + 3] = 0; continue; }
    const color = x < 3 ? [0,0,0] : x < 5 ? [170,0,0] : [0,0,170];
    rgba.set([...color, 255], p);
  }
  return rgba;
}
function makeSolidScreen() {
  const rgba = new Uint8Array(320 * 200 * 4);
  for (let p = 0; p < rgba.length; p += 4) rgba.set([255, 255, 255, 255], p);
  return rgba;
}
function makeSheetImage() {
  const rgba = new Uint8Array(16 * 8 * 4);
  for (let y = 0; y < 8; y += 1) for (let x = 0; x < 16; x += 1) {
    const p = (y * 16 + x) * 4;
    if ((x % 8) === 1 && y === 0) { rgba[p + 3] = 0; continue; }
    const color = x < 8 ? (x % 8 < 4 ? [170,0,0] : [0,0,0]) : (x % 8 < 4 ? [0,0,170] : [0,0,0]);
    rgba.set([...color, 255], p);
  }
  return rgba;
}

class CDP {
  constructor(url) {
    this.socket = new WebSocket(url);
    this.nextId = 1;
    this.pending = new Map();
    this.eventListeners = new Map();
    this.ready = new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error("Timed out connecting to the isolated browser debugging socket.")), 5000);
      this.socket.addEventListener("open", () => { clearTimeout(timer); resolve(); }, { once: true });
      this.socket.addEventListener("error", () => { clearTimeout(timer); reject(new Error("Could not connect to the isolated browser debugging socket.")); }, { once: true });
      this.socket.addEventListener("close", () => { clearTimeout(timer); reject(new Error("Browser closed the debugging socket.")); }, { once: true });
    });
    this.socket.addEventListener("message", (event) => {
      const message = JSON.parse(event.data);
      if (!message.id) {
        for (const listener of this.eventListeners.get(message.method) || []) listener(message.params || {});
        return;
      }
      const pending = this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(message.id);
      clearTimeout(pending.timer);
      if (message.error) pending.reject(new Error(message.error.message));
      else pending.resolve(message.result || {});
    });
    this.socket.addEventListener("close", () => {
      for (const pending of this.pending.values()) {
        clearTimeout(pending.timer);
        pending.reject(new Error("Browser closed the debugging socket."));
      }
      this.pending.clear();
    });
  }
  async send(method, params = {}) {
    await this.ready;
    const id = this.nextId++;
    const result = new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error("Browser command timed out: " + method));
      }, 5000);
      this.pending.set(id, { resolve, reject, timer });
    });
    this.socket.send(JSON.stringify({ id, method, params }));
    return result;
  }
  on(method, listener) {
    const listeners = this.eventListeners.get(method) || [];
    listeners.push(listener);
    this.eventListeners.set(method, listeners);
  }
  close() { this.socket.close(); }
}

async function main() {
  const timestamp = new Date().toISOString().replace(/[:.]/g, "-");
  const output = process.env.LIBTANDY_QA_OUTPUT || path.join(os.tmpdir(), "libtandy-graphics-editor-ui-" + timestamp);
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), "libtandy-ui-profile-"));
  fs.mkdirSync(output, { recursive: true });
  const fixtures = path.join(profile, "fixtures"); fs.mkdirSync(fixtures, { recursive: true });
  const photo = path.join(fixtures, "photo-like.png");
  const solid = path.join(fixtures, "solid-screen.png");
  const sprite = path.join(fixtures, "black-and-color-sprite.png");
  const sheet = path.join(fixtures, "two-frame-sheet.png");
  const malformed = path.join(fixtures, "invalid-sprite-project.tandy.json");
  const opaqueZero = path.join(fixtures, "opaque-transparent-index.tandy.json");
  fs.writeFileSync(photo, encodePng(80, 50, makePhotoLikeImage(80, 50)));
  fs.writeFileSync(solid, encodePng(320, 200, makeSolidScreen()));
  fs.writeFileSync(sprite, encodePng(8, 8, makeSpriteImage()));
  fs.writeFileSync(sheet, encodePng(16, 8, makeSheetImage()));
  fs.writeFileSync(malformed, JSON.stringify({ format: "libtandy-graphics-workshop", version: 1, name: "invalid",
    mode: "sprite", spriteColors: [0, 4, 1], screenBackground: 0,
    frames: [{ width: 8, height: 8, indices: [4, ...Array(63).fill(0)], alpha: [1, ...Array(63).fill(0)] }] }));
  fs.writeFileSync(opaqueZero, JSON.stringify({ format: "libtandy-graphics-workshop", version: 1, name: "invalid-black",
    mode: "sprite", spriteColors: [0, 4, 1], screenBackground: 0,
    frames: [{ width: 8, height: 8, indices: Array(64).fill(0), alpha: [1, ...Array(63).fill(0)] }] }));

  const server = http.createServer((request, response) => {
    const requested = decodeURIComponent(new URL(request.url, "http://127.0.0.1").pathname);
    const pathname = requested === "/" ? "/index.html" : requested;
    if (pathname === "/favicon.ico") { response.writeHead(204).end(); return; }
    const target = path.resolve(APP_ROOT, "." + pathname);
    if (!target.startsWith(APP_ROOT + path.sep) && target !== path.join(APP_ROOT, "index.html")) {
      response.writeHead(403).end("Forbidden"); return;
    }
    fs.readFile(target, (error, content) => {
      if (error) { response.writeHead(404).end("Not found"); return; }
      response.writeHead(200, { "Content-Type": MIME[path.extname(target)] || "application/octet-stream" }).end(content);
    });
  });
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const appUrl = "http://127.0.0.1:" + server.address().port + "/";
  const args = ["--headless=new", "--disable-gpu", "--disable-gpu-compositing", "--use-gl=angle", "--use-angle=swiftshader",
    "--remote-debugging-address=127.0.0.1", "--remote-debugging-port=0", "--remote-allow-origins=*",
    "--enable-logging=stderr", "--v=1", "--no-first-run", "--no-default-browser-check",
    "--user-data-dir=" + profile, "--window-size=1440,1200", appUrl];
  let browserLog = "";
  const browser = spawn(BROWSER, args, { stdio: ["ignore", "pipe", "pipe"] });
  browser.stdout.on("data", (chunk) => { browserLog += chunk.toString(); });
  browser.stderr.on("data", (chunk) => { browserLog += chunk.toString(); });
  let cdpClient = null;
  console.log("Launching isolated browser test: " + path.basename(BROWSER));
  const activePortFile = path.join(profile, "DevToolsActivePort");
  let port = null;
  try {
    for (let attempt = 0; attempt < 100; attempt += 1) {
      if (browser.exitCode != null) throw new Error("Browser exited before the page opened.\n" + browserLog.slice(-8000));
      if (fs.existsSync(activePortFile)) { port = Number(fs.readFileSync(activePortFile, "utf8").split(/\r?\n/)[0]); break; }
      await sleep(100);
    }
    if (!port) throw new Error("Browser did not publish a debugging port.\n" + browserLog.slice(-8000));
    console.log("Browser debugging port is ready.");
    const targets = await requestJson("http://127.0.0.1:" + port + "/json/list");
    const pageTarget = targets.find((target) => target.type === "page");
    if (!pageTarget) throw new Error("Browser opened without a page target.");
    console.log("Browser page target is ready.");
    const cdp = cdpClient = new CDP(pageTarget.webSocketDebuggerUrl);
    const pageDiagnostics = { console: [], exceptions: [], log: [] };
    cdp.on("Runtime.consoleAPICalled", (event) => {
      pageDiagnostics.console.push({ type: event.type, args: (event.args || []).map((arg) => arg.value ?? arg.description ?? "") });
    });
    cdp.on("Runtime.exceptionThrown", (event) => {
      pageDiagnostics.exceptions.push(event.exceptionDetails && (event.exceptionDetails.text || event.exceptionDetails.exception && event.exceptionDetails.exception.description));
    });
    cdp.on("Log.entryAdded", (event) => {
      if (event.entry && event.entry.level === "error") pageDiagnostics.log.push({ text: event.entry.text, url: event.entry.url || "" });
    });
    await cdp.send("Runtime.enable");
    await cdp.send("Page.enable");
    await cdp.send("DOM.enable");
    await cdp.send("Log.enable");
    await cdp.send("Page.setDownloadBehavior", { behavior: "allow", downloadPath: output });

    async function evaluate(expression) {
      const reply = await cdp.send("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true });
      if (reply.exceptionDetails) throw new Error(reply.exceptionDetails.text || "Page script failed.");
      return reply.result.value;
    }
    async function waitFor(expression, description, timeout = 15000) {
      const end = Date.now() + timeout;
      while (Date.now() < end) {
        if (await evaluate(expression)) return;
        await sleep(100);
      }
      let pageState = null;
      try {
        pageState = await evaluate("(() => {const ids=['asset-mode','pending-image','pending-preview-wrap','apply-image','canvas-dimensions','canvas-status','toast','frame-count'];return {url:location.href,body:document.body.innerText.slice(0,1800),controls:Object.fromEntries(ids.map(id=>{const e=document.getElementById(id);return [id,e?{text:e.textContent,value:e.value,hidden:e.hidden,disabled:e.disabled}:null]}))};})()");
      } catch (error) { pageState = { evaluateError: error.message }; }
      const screenshotName = "failure-" + description.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") + ".png";
      try { await capture(screenshotName); } catch (error) { pageState.captureError = error.message; }
      console.error("Timed-out page diagnostics: " + JSON.stringify({ description, pageState, pageDiagnostics }));
      throw new Error("Timed out waiting for " + description + ".");
    }
    async function setFile(file) {
      const { root } = await cdp.send("DOM.getDocument", { depth: -1 });
      const { nodeId } = await cdp.send("DOM.querySelector", { nodeId: root.nodeId, selector: "#file-picker" });
      if (!nodeId) throw new Error("The image/project file control was not found.");
      await cdp.send("DOM.setFileInputFiles", { nodeId, files: [file] });
    }
    async function click(selector) {
      const bounds = await evaluate("(() => {const e=document.querySelector(" + JSON.stringify(selector) + ");e.scrollIntoView({block:'center',inline:'center'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,visible:r.width>0&&r.height>0};})()");
      if (!bounds || !bounds.visible) throw new Error("Control is not visible: " + selector);
      await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", x: bounds.x, y: bounds.y });
      await cdp.send("Input.dispatchMouseEvent", { type: "mousePressed", x: bounds.x, y: bounds.y, button: "left", clickCount: 1 });
      await cdp.send("Input.dispatchMouseEvent", { type: "mouseReleased", x: bounds.x, y: bounds.y, button: "left", clickCount: 1 });
    }
    async function setControl(selector, value, eventName = "change") {
      await evaluate("(() => {const e=document.querySelector(" + JSON.stringify(selector) + ");e.value=" + JSON.stringify(String(value)) + ";e.dispatchEvent(new Event(" + JSON.stringify(eventName) + ",{bubbles:true}));})()");
    }
    async function setCheckbox(selector, checked) {
      await evaluate("(() => {const e=document.querySelector(" + JSON.stringify(selector) + ");e.checked=" + Boolean(checked) + ";e.dispatchEvent(new Event('change',{bubbles:true}));})()");
    }
    async function pixel(selector, x, y) {
      return evaluate("(() => {const c=document.querySelector(" + JSON.stringify(selector) + ");return Array.from(c.getContext('2d').getImageData(" + x + "," + y + ",1,1).data);})()");
    }
    async function isVisible(selector) {
      return evaluate("(() => {const e=document.querySelector(" + JSON.stringify(selector) + ");return !!e&&!e.hidden&&getComputedStyle(e).display!=='none';})()");
    }
    async function clickCanvasPixel(x, y) {
      const rect = await evaluate("(() => {const c=document.querySelector('#art-canvas');c.scrollIntoView({block:'center',inline:'center'});const r=c.getBoundingClientRect();return {x:r.x+(" + x + "+.5)*r.width/c.width,y:r.y+(" + y + "+.5)*r.height/c.height};})()");
      await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", x: rect.x, y: rect.y });
      await cdp.send("Input.dispatchMouseEvent", { type: "mousePressed", x: rect.x, y: rect.y, button: "left", clickCount: 1 });
      await cdp.send("Input.dispatchMouseEvent", { type: "mouseReleased", x: rect.x, y: rect.y, button: "left", clickCount: 1 });
    }
    async function capture(name) {
      const screenshot = await cdp.send("Page.captureScreenshot", { format: "png", captureBeyondViewport: false });
      fs.writeFileSync(path.join(output, name), Buffer.from(screenshot.data, "base64"));
    }
    async function waitDownload(filename, timeout = 10000) {
      const target = path.join(output, filename);
      const end = Date.now() + timeout;
      while (Date.now() < end) {
        if (fs.existsSync(target) && !fs.existsSync(target + ".crdownload")) return target;
        await sleep(100);
      }
      throw new Error("Download did not finish: " + filename);
    }

    await waitFor("document.querySelector('#asset-mode') !== null", "editor startup");
    await setControl("#asset-mode", "screen");
    assert.equal(await isVisible("#screen-background-wrap"), true, "screen fill control is visible in screen mode");
    assert.equal(await isVisible("#screen-background"), true, "screen background chooser is visible in screen mode");
    assert.equal(await isVisible("#sheet-wrap"), false, "sprite-sheet option is hidden in screen mode");
    assert.equal(await isVisible("#export-binary"), true, "mode-9 export is visible in screen mode");
    await setFile(photo);
    await waitFor("!document.querySelector('#apply-image').disabled && !document.querySelector('#pending-preview-wrap').hidden", "screen import preview");
    assert.match(await evaluate("document.querySelector('#dither-recommendation').textContent"), /photo/i);
    await setControl("#dither-strength", 82, "input");
    await setControl("#auto-strength", 48, "input");
    await capture("01-screen-import-preview.png");
    await click("#apply-image");
    await sleep(500);
    const afterConvert = await evaluate("(() => {const d=document.querySelector('#canvas-dimensions');const p=document.querySelector('#pending-image');const c=document.querySelector('#art-canvas');return {dimensions:d&&d.textContent,canvas:c&&[c.width,c.height],mode:document.querySelector('#asset-mode')&&document.querySelector('#asset-mode').value,pending:p&&p.textContent,applyDisabled:document.querySelector('#apply-image')&&document.querySelector('#apply-image').disabled,toast:document.querySelector('#toast')&&document.querySelector('#toast').textContent,frames:document.querySelector('#frame-count')&&document.querySelector('#frame-count').textContent};})() ");
    console.log("Page state after Convert: " + JSON.stringify(afterConvert));
    await capture("02-screen-after-convert.png");
    if (pageDiagnostics.exceptions.length || pageDiagnostics.log.length) {
      console.log("Page errors: " + JSON.stringify(pageDiagnostics));
    }
    await waitFor("document.querySelector('#art-canvas').width===320 && document.querySelector('#art-canvas').height===200 && document.querySelector('#toast').textContent.includes('converted')", "screen conversion");
    const beforePencil = await pixel("#art-canvas", 10, 10);
    await click("#palette [data-color='12']");
    await click("[data-tool='pencil']");
    await clickCanvasPixel(10, 10);
    const afterPencil = await pixel("#art-canvas", 10, 10);
    assert.deepEqual(afterPencil, [255, 85, 85, 255], "pencil draws the selected RGBI red");
    await click("#undo");
    assert.deepEqual(await pixel("#art-canvas", 10, 10), beforePencil, "undo restores the original pixel");
    await click("#redo");
    assert.deepEqual(await pixel("#art-canvas", 10, 10), afterPencil, "redo reapplies the pencil edit");

    await click("#save-project");
    const savedScreenProject = await waitDownload("photo_like.tandy.json");
    await setFile(savedScreenProject);
    await waitFor("document.querySelector('#project-name').value === 'photo-like'", "project reopen");
    await click("#export-png");
    const screenPng = await waitDownload("photo_like_preview.png");
    const screenPngBytes = fs.readFileSync(screenPng);
    assert.equal(screenPngBytes.readUInt32BE(16), 320, "PNG export keeps screen width");
    assert.equal(screenPngBytes.readUInt32BE(20), 200, "PNG export keeps screen height");
    await click("#export-c");
    const screenHeader = await waitDownload("photo_like_mode9.h");
    assert.match(fs.readFileSync(screenHeader, "utf8"), /LT_GFX_BYTES/);
    await click("#export-binary");
    const mode9Bin = await waitDownload("photo_like_mode9.bin");
    assert.equal(fs.statSync(mode9Bin).size, 32768, "raw export is exactly 32 KiB");

    await setFile(solid);
    await waitFor("document.querySelector('#pending-image').textContent.includes('solid-screen.png') && !document.querySelector('#apply-image').disabled && !document.querySelector('#pending-preview-wrap').hidden", "solid screen import preview");
    await click("#apply-image");
    await click("#palette [data-color='2']");
    await click("[data-tool='fill']");
    await clickCanvasPixel(10, 10);
    assert.deepEqual(await pixel("#art-canvas", 319, 199), [0, 170, 0, 255], "fill recolors the connected screen area");
    await click("#undo");
    assert.deepEqual(await pixel("#art-canvas", 319, 199), [255, 255, 255, 255], "undo restores the fill area");
    await click("#redo");
    assert.deepEqual(await pixel("#art-canvas", 319, 199), [0, 170, 0, 255], "redo reapplies the fill area");

    await setControl("#asset-mode", "sprite");
    assert.equal(await isVisible("#screen-background-wrap"), false, "screen fill control is hidden in sprite mode");
    assert.equal(await isVisible("#screen-background"), false, "screen background chooser is hidden in sprite mode");
    assert.equal(await isVisible("#sheet-wrap"), true, "sprite-sheet option is visible in sprite mode");
    assert.equal(await isVisible("#export-binary"), false, "mode-9 export is hidden in sprite mode");
    await setControl("#dither-mode", "none");
    await setCheckbox("#auto-correct", false);
    await setFile(sprite);
    await waitFor("document.querySelector('#pending-image').textContent.includes('black-and-color-sprite.png') && !document.querySelector('#apply-image').disabled && !document.querySelector('#pending-preview-wrap').hidden", "sprite import preview");
    const previewBlack = await pixel("#pending-preview", 0, 0);
    const previewTransparent = await pixel("#pending-preview", 1, 0);
    assert.deepEqual(previewBlack, [0, 0, 0, 255], "opaque black stays visible in the converted preview");
    assert.equal(previewTransparent[3], 0, "transparent index zero stays see-through in the preview");
    await capture("02-sprite-black-and-transparent.png");
    await click("#apply-image");
    await capture("03-sprite-black-and-transparent-on-canvas.png");
    assert.deepEqual(await pixel("#art-canvas", 0, 0), [0, 0, 0, 255], "opaque black stays visible on the editor canvas");
    assert.equal((await pixel("#art-canvas", 1, 0))[3], 0, "transparent pixel stays see-through on the editor canvas");
    const suggestedInks = await evaluate("Array.from(document.querySelectorAll('.ink-slot i')).map(e=>e.style.backgroundColor)");
    assert.deepEqual(suggestedInks.slice(0, 3).sort(), ["rgb(0, 0, 0)", "rgb(0, 0, 170)", "rgb(170, 0, 0)"].sort(),
      "sprite import suggests the visible colors present in the image");
    await click("[data-slot='2']");
    await click("#palette [data-color='2']");
    assert.equal(await evaluate("document.querySelector('#lock-sprite-palette').checked"), true, "manually choosing an ink locks the sprite palette");
    await click("#undo");
    assert.deepEqual(await evaluate("Array.from(document.querySelectorAll('.ink-slot i')).map(e=>e.style.backgroundColor)"), suggestedInks,
      "undo restores the previous sprite palette");

    await click("[data-tool='eraser']");
    await clickCanvasPixel(0, 0);
    assert.equal((await pixel("#art-canvas", 0, 0))[3], 0, "eraser makes a pixel transparent");
    await click("#undo");
    assert.deepEqual(await pixel("#art-canvas", 0, 0), [0, 0, 0, 255], "undo restores opaque black");
    await click("#redo");
    assert.equal((await pixel("#art-canvas", 0, 0))[3], 0, "redo reapplies transparency");
    await click("#add-frame");
    assert.equal(await evaluate("document.querySelector('#frame-count').textContent"), "2", "sprite frame can be copied");
    await click("#remove-frame");
    assert.equal(await evaluate("document.querySelector('#frame-count').textContent"), "1", "sprite frame can be removed");
    await click("#export-c");
    const spriteHeader = await waitDownload("black_and_color_sprite_sprite.h");
    assert.match(fs.readFileSync(spriteHeader, "utf8"), /plane0\[1\]\[8\]/);
    await click("#export-png");
    const spritePng = await waitDownload("black_and_color_sprite_preview.png");
    const spritePngData = decodePng(fs.readFileSync(spritePng));
    assert.deepEqual(Array.from(spritePngData.pixels.slice(2 * 4, 3 * 4)), [0, 0, 0, 255], "PNG export keeps an unerased opaque black pixel");
    assert.equal(spritePngData.pixels[7], 0, "PNG export keeps transparent index zero");

    await setCheckbox("#as-sprite-sheet", true);
    await setFile(sheet);
    await waitFor("document.querySelector('#pending-image').textContent.includes('2 frames')", "two-frame sprite sheet preview");
    await click("#apply-image");
    assert.equal(await evaluate("document.querySelector('#frame-count').textContent"), "2", "sheet imports as two editable frames");
    await click("#export-sheet");
    const spriteSheetPng = await waitDownload("two_frame_sheet_sheet.png");
    assert.equal(fs.readFileSync(spriteSheetPng).readUInt32BE(16), 16, "sprite sheet PNG places two 8-pixel frames side by side");

    await setFile(malformed);
    await waitFor("document.querySelector('#toast').textContent.includes('Sprite color index must be from 0 to 3')", "malformed project rejection");
    assert.equal(await evaluate("document.querySelector('#frame-count').textContent"), "2", "invalid project does not replace the valid artwork");
    await setFile(opaqueZero);
    await waitFor("document.querySelector('#toast').textContent.includes('index 0 is transparent')", "opaque sprite index-zero rejection");
    assert.equal(await evaluate("document.querySelector('#frame-count').textContent"), "2", "invalid opaque black project does not replace the valid artwork");
    await capture("04-sprite-sheet-frames.png");

    console.log("PASS browser UI: image preview, correction controls, conversion, pencil, eraser, undo/redo, project save/reopen, PNG/C/mode-9 export, black/transparency, palette suggestion, frames, sheet import, malformed project.");
    console.log("Screenshots: " + output);
  } catch (error) {
    throw new Error((error.stack || error.message || String(error)) + "\nBrowser output:\n" + browserLog.slice(-8000));
  } finally {
    if (cdpClient) cdpClient.close();
    server.close();
    if (browser.exitCode == null) browser.kill();
    if (typeof server.closeAllConnections === "function") server.closeAllConnections();
    if (browser.stdout) browser.stdout.destroy();
    if (browser.stderr) browser.stderr.destroy();
  }
}

main().catch((error) => {
  console.error(error.stack || error.message || String(error));
  process.exit(1);
});
