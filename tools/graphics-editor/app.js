/* SPDX-License-Identifier: MIT */
(function () {
  "use strict";
  const Core = window.TandyGraphicsCore;
  const $ = (id) => document.getElementById(id);
  const canvas = $("art-canvas");
  const ctx = canvas.getContext("2d", { alpha: true });
  const paletteHost = $("palette");
  const modeSelect = $("asset-mode");
  let project;
  let frameIndex = 0;
  let selectedColor = 14;
  let selectedSlot = 1;
  let transparentSelected = false;
  let tool = "pencil";
  let zoom = 24;
  let stroke = false;
  let lastPoint = null;
  let undoStack = [];
  let redoStack = [];
  let pendingImage = null;
  let pendingSourceName = "";
  let pendingFrameImages = null;
  let pendingSpriteColors = null;
  let pendingKind = null;
  let pendingOriginalKind = null;
  let pendingRawPreview = null;
  let toastTimer = null;

  function mode() { return project.mode; }
  function dimensions() { return mode() === "sprite" ? [8, 8] : [Core.WIDTH, Core.HEIGHT]; }
  function currentFrame() { return project.frames[frameIndex]; }
  function activeInk() { return mode() === "sprite" ? selectedSlot : selectedColor; }
  function effectiveSpriteColors() { return pendingSpriteColors || project.spriteColors; }
  function activePaletteColor() { return mode() === "sprite" ? effectiveSpriteColors()[selectedSlot - 1] : selectedColor; }
  function escapeHtml(text) {
    return String(text).replace(/[&<>"']/g, (character) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[character]);
  }
  function flash(message) {
    const toast = $("toast");
    toast.textContent = message;
    toast.classList.add("show");
    if (toastTimer) clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.remove("show"), 2400);
  }
  function clearPendingImport() {
    pendingImage = null;
    pendingFrameImages = null;
    pendingSpriteColors = null;
    $("pending-preview-wrap").hidden = true;
    $("pending-image").textContent = "Import a picture to preview it here.";
    $("pending-image").className = "pending-image";
    $("apply-image").disabled = true;
  }
  function syncSaveLabel(message) {
    $("save-state").innerHTML = '<span class="status-dot"></span>' + escapeHtml(message || "Only on this device");
  }
  function selectedExportName() { return Core.cleanName(project.name || "tandy_art"); }

  function frameToJson(frame) {
    return { width: frame.width, height: frame.height,
      indices: Array.from(frame.indices), alpha: Array.from(frame.alpha) };
  }
  function makeProjectJson() {
    return { format: "libtandy-graphics-workshop", version: 1, name: project.name,
      mode: mode(), spriteColors: project.spriteColors.slice(),
      screenBackground: project.screenBackground,
      frames: project.frames.map(frameToJson) };
  }
  function snapshot() { return JSON.stringify(makeProjectJson()); }
  function restoreSnapshot(serialized) {
    const data = JSON.parse(serialized);
    project = normalizeProject(data);
    pendingSpriteColors = mode() === "sprite" ? project.spriteColors.slice() : null;
    frameIndex = Math.min(frameIndex, project.frames.length - 1);
    renderAll();
    if (pendingImage) drawPendingPreview();
  }
  function pushUndo() {
    undoStack.push(snapshot());
    if (undoStack.length > 60) undoStack.shift();
    redoStack = [];
    updateHistoryButtons();
    syncSaveLabel("Unsaved changes");
  }
  function undo() {
    if (!undoStack.length) return;
    redoStack.push(snapshot());
    restoreSnapshot(undoStack.pop());
    syncSaveLabel("Undone · save a project to keep it");
    updateHistoryButtons();
  }
  function redo() {
    if (!redoStack.length) return;
    undoStack.push(snapshot());
    restoreSnapshot(redoStack.pop());
    syncSaveLabel("Redone · save a project to keep it");
    updateHistoryButtons();
  }
  function updateHistoryButtons() {
    $("undo").disabled = undoStack.length === 0;
    $("redo").disabled = redoStack.length === 0;
  }
  function cloneFrame(frame) {
    return { width: frame.width, height: frame.height,
      indices: new Uint8Array(frame.indices), alpha: new Uint8Array(frame.alpha) };
  }
  function resizeFrame(frame, width, height) {
    const output = Core.createFrame(width, height, 0, false);
    for (let y = 0; y < height; y += 1) {
      const sy = Math.min(frame.height - 1, Math.floor((y + 0.5) * frame.height / height));
      for (let x = 0; x < width; x += 1) {
        const sx = Math.min(frame.width - 1, Math.floor((x + 0.5) * frame.width / width));
        const from = sy * frame.width + sx;
        const to = y * width + x;
        output.indices[to] = frame.indices[from];
        output.alpha[to] = frame.alpha[from];
      }
    }
    return output;
  }
  function normalizeProject(data) {
    if (!data || !Array.isArray(data.frames) || data.frames.length < 1 || data.frames.length > 200) {
      throw new Error("That project needs between 1 and 200 frames.");
    }
    if (data.mode != null && data.mode !== "sprite" && data.mode !== "screen") throw new Error("Choose either Sprite or Full screen mode.");
    const chosenMode = data.mode === "sprite" ? "sprite" : "screen";
    const [width, height] = chosenMode === "sprite" ? [8, 8] : [Core.WIDTH, Core.HEIGHT];
    const frames = data.frames.map((source, index) => {
      if (!source || !Array.isArray(source.indices) || !Array.isArray(source.alpha)) {
        throw new Error("Frame " + (index + 1) + " is missing pixel data.");
      }
      const sourceWidth = Number(source.width);
      const sourceHeight = Number(source.height);
      if (sourceWidth * sourceHeight !== source.indices.length || sourceWidth * sourceHeight !== source.alpha.length ||
          !Number.isInteger(sourceWidth) || !Number.isInteger(sourceHeight) || sourceWidth < 1 || sourceHeight < 1 || sourceWidth > 320 || sourceHeight > 200) {
        throw new Error("Frame " + (index + 1) + " has invalid dimensions or pixel counts.");
      }
      const input = Core.createFrame(sourceWidth, sourceHeight, 0, false);
      for (let p = 0; p < input.indices.length; p += 1) {
        const indexValue = Number(source.indices[p]);
        const maximumIndex = chosenMode === "sprite" ? 3 : 15;
        if (!Number.isInteger(indexValue) || indexValue < 0 || indexValue > maximumIndex) {
          throw new Error(chosenMode === "sprite" ? "Sprite color index must be from 0 to 3." : "Color index must be from 0 to 15.");
        }
        const alphaValue = source.alpha[p];
        if (alphaValue !== 0 && alphaValue !== 1 && alphaValue !== false && alphaValue !== true) {
          throw new Error("Pixel transparency must be either 0 or 1.");
        }
        if (chosenMode === "sprite" && (alphaValue === 1 || alphaValue === true) && indexValue === 0) {
          throw new Error("Sprite index 0 is transparent. Use ink slot 1 for opaque black.");
        }
        input.indices[p] = indexValue;
        input.alpha[p] = alphaValue ? 1 : 0;
      }
      return sourceWidth === width && sourceHeight === height ? input : resizeFrame(input, width, height);
    });
    const name = String(data.name || "untitled").slice(0, 48);
    const sourceSpriteColors = data.spriteColors == null ? [14, 0, 15] : data.spriteColors;
    if (!Array.isArray(sourceSpriteColors) || sourceSpriteColors.length !== 3 ||
        sourceSpriteColors.some((color) => !Number.isInteger(color) || color < 0 || color > 15)) {
      throw new Error("A sprite palette needs exactly three Tandy colors from 0 to 15.");
    }
    const spriteColors = sourceSpriteColors.slice();
    const background = data.screenBackground == null ? 0 : Number(data.screenBackground);
    if (!Number.isInteger(background) || background < 0 || background > 15) throw new Error("Screen background color must be from 0 to 15.");
    return { version: 1, name, mode: chosenMode, frames, spriteColors,
      screenBackground: background };
  }

  function makeExampleProject() {
    const fox = Core.createFrame(8, 8, 0, false);
    const rows = ["..1111..", ".122221.", "12222221", "12232221", "12222221", "12322321", ".122221.", "..1111.."];
    for (let y = 0; y < 8; y += 1) {
      for (let x = 0; x < 8; x += 1) {
        const character = rows[y][x];
        const p = y * 8 + x;
        if (character !== ".") {
          fox.indices[p] = Number(character);
          fox.alpha[p] = 1;
        }
      }
    }
    return { version: 1, name: "little-fox", mode: "sprite", frames: [fox],
      spriteColors: [14, 0, 15], screenBackground: 0 };
  }

  function updatePalette() {
    paletteHost.innerHTML = "";
    Core.RGBI.forEach((rgb, index) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "swatch";
      button.style.backgroundColor = "rgb(" + rgb.join(",") + ")";
      button.title = index + " · " + colorName(index) + " (RGB " + rgb.join(", ") + ")";
      button.setAttribute("aria-label", colorName(index) + ", palette index " + index);
      button.dataset.color = String(index);
      button.innerHTML = '<span class="swatch-number">' + index.toString(16).toUpperCase() + "</span>";
      button.addEventListener("click", () => {
        transparentSelected = false;
        if (mode() === "sprite") {
          $("lock-sprite-palette").checked = true;
          if (project.spriteColors[selectedSlot - 1] !== index) pushUndo();
          project.spriteColors[selectedSlot - 1] = index;
          pendingSpriteColors = project.spriteColors.slice();
          document.querySelectorAll(".ink-slot").forEach((slot) => slot.classList.toggle("selected", Number(slot.dataset.slot) === selectedSlot));
        } else selectedColor = index;
        updatePalette();
        drawCanvas();
        drawFrames();
        updateUsage();
        drawPendingPreview();
      });
      const selected = mode() === "sprite"
        ? !transparentSelected && effectiveSpriteColors()[selectedSlot - 1] === index
        : !transparentSelected && selectedColor === index;
      button.classList.toggle("selected", selected);
      paletteHost.appendChild(button);
    });
    $("transparent-swatch").classList.toggle("selected", transparentSelected);
    $("sprite-ink-picker").hidden = mode() !== "sprite";
    $("screen-background-wrap").hidden = mode() !== "screen";
    $("screen-background").hidden = mode() !== "screen";
    $("sheet-wrap").hidden = mode() !== "sprite";
    $("export-binary").hidden = mode() !== "screen";
    $("c-export-caption").textContent = mode() === "sprite"
      ? "Two bitplanes + three Tandy ink colors"
      : "Packed 32 KiB screen data";
    $("mode-help").textContent = mode() === "sprite"
      ? "A tiny 8 × 8 picture with see-through pixels."
      : "A full Tandy screen, ready to pack.";
    document.querySelectorAll(".ink-slot").forEach((slot) => {
      const i = Number(slot.dataset.slot) - 1;
      slot.querySelector("i").style.backgroundColor = "rgb(" + Core.RGBI[effectiveSpriteColors()[i]].join(",") + ")";
      slot.classList.toggle("selected", Number(slot.dataset.slot) === selectedSlot);
    });
    const backgroundSelect = $("screen-background");
    backgroundSelect.innerHTML = Core.RGBI.map((rgb, index) => '<option value="' + index + '">' + index + " · " + colorName(index) + "</option>").join("");
    backgroundSelect.value = String(project.screenBackground);
  }

  function colorName(index) {
    return ["Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Brown", "Light gray",
      "Dark gray", "Bright blue", "Bright green", "Bright cyan", "Bright red", "Bright magenta", "Yellow", "White"][index];
  }

  function renderFrameInto(targetCtx, frame, paletteMap) {
    const image = targetCtx.createImageData(frame.width, frame.height);
    for (let p = 0; p < frame.width * frame.height; p += 1) {
      const colorIndex = mode() === "sprite" ? (paletteMap || project.spriteColors)[Math.max(0, Math.min(2, frame.indices[p] - 1))] : frame.indices[p];
      const color = Core.RGBI[colorIndex] || Core.RGBI[0];
      const q = p * 4;
      image.data[q] = color[0];
      image.data[q + 1] = color[1];
      image.data[q + 2] = color[2];
      image.data[q + 3] = frame.alpha[p] ? 255 : 0;
      if (mode() === "sprite" && frame.indices[p] === 0) image.data[q + 3] = 0;
    }
    targetCtx.putImageData(image, 0, 0);
  }
  function drawCanvas() {
    const frame = currentFrame();
    if (!frame) return;
    canvas.width = frame.width;
    canvas.height = frame.height;
    renderFrameInto(ctx, frame);
    const [width, height] = dimensions();
    canvas.style.width = Math.round(width * zoom) + "px";
    canvas.style.height = Math.round(height * zoom) + "px";
    $("zoom-label").textContent = (Math.round(zoom * 10) / 10) + "×";
    $("canvas-dimensions").textContent = width + " × " + height + " px";
    $("canvas-title").innerHTML = (mode() === "sprite" ? "Sprite" : "Full screen") + ' <span>' + width + " × " + height + "</span>";
    $("empty-state").hidden = true;
    updateUsage();
  }

  function drawFrames() {
    const host = $("frame-list");
    host.innerHTML = "";
    project.frames.forEach((frame, index) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "frame-thumb" + (index === frameIndex ? " selected" : "");
      button.title = "Select frame " + (index + 1);
      const thumb = document.createElement("canvas");
      thumb.width = frame.width;
      thumb.height = frame.height;
      renderFrameInto(thumb.getContext("2d"), frame);
      button.appendChild(thumb);
      const number = document.createElement("span");
      number.textContent = index + 1;
      button.appendChild(number);
      button.addEventListener("click", () => {
        frameIndex = index;
        drawCanvas();
        drawFrames();
      });
      host.appendChild(button);
    });
    $("frame-count").textContent = String(project.frames.length);
    $("remove-frame").disabled = project.frames.length <= 1;
  }

  function updateUsage() {
    const name = selectedExportName();
    if (mode() === "sprite") {
      $("usage-snippet").textContent = '#include "' + name + '_sprite.h"\nlt_gfx_sprite8_planes(&screen, x, y,\n  ' + name + '_plane0[0], ' + name + '_plane1[0], ' + name + '_palette);';
      $("advanced-details").textContent = "8×8 pixels · 2 bitplanes × 8 rows · index 0 transparent · palette[1..3] = " +
        project.spriteColors.map((index) => index + " (" + colorName(index) + ")").join(", ") +
        ". Use lt_gfx_sprite8_planes from lt_gfx_sprite8.h. Each exported frame has two 8-byte row arrays.";
    } else {
      $("usage-snippet").textContent = '#include "' + name + '_mode9.h"\nlt_gfx_surface screen;\nlt_gfx_init(&screen, ' + name + '_mode9);';
      $("advanced-details").textContent = "320×200 pixels · 32,768 bytes · 4 banks × 8,192 bytes. Address = (y & 3) × 8,192 + (y >> 2) × 160 + (x >> 1). Even x: high nibble; odd x: low nibble. Transparent pixels are filled with color " + project.screenBackground + " (" + colorName(project.screenBackground) + ").";
    }
  }
  function renderAll() {
    modeSelect.value = mode();
    $("project-name").value = project.name;
    updatePalette();
    drawCanvas();
    drawFrames();
    updateHistoryButtons();
    scaleZoom();
  }
  function scaleZoom() {
    const [width, height] = dimensions();
    canvas.style.width = Math.round(width * zoom) + "px";
    canvas.style.height = Math.round(height * zoom) + "px";
    $("zoom-label").textContent = (Math.round(zoom * 10) / 10) + "×";
  }

  function setTool(next) {
    tool = next;
    document.querySelectorAll(".tool-button").forEach((button) => button.classList.toggle("active", button.dataset.tool === tool));
    canvas.style.cursor = tool === "eyedropper" ? "copy" : tool === "fill" ? "cell" : "crosshair";
    $("canvas-status").textContent = ({ pencil: "Pencil ready", eraser: "Eraser ready", fill: "Fill one connected area", eyedropper: "Pick a pixel color" })[tool];
  }
  function setPixel(x, y, ink, isTransparent) {
    const frame = currentFrame();
    const p = Core.pixelOffset(x, y, frame.width, frame.height);
    if (p < 0) return;
    if (isTransparent) {
      frame.indices[p] = 0;
      frame.alpha[p] = 0;
    } else {
      frame.indices[p] = ink;
      frame.alpha[p] = 1;
    }
  }
  function drawLine(x0, y0, x1, y1, ink, isTransparent) {
    let dx = Math.abs(x1 - x0); let sx = x0 < x1 ? 1 : -1;
    let dy = -Math.abs(y1 - y0); let sy = y0 < y1 ? 1 : -1;
    let error = dx + dy;
    for (;;) {
      setPixel(x0, y0, ink, isTransparent);
      if (x0 === x1 && y0 === y1) break;
      const doubled = 2 * error;
      if (doubled >= dy) { error += dy; x0 += sx; }
      if (doubled <= dx) { error += dx; y0 += sy; }
    }
  }
  function bucketFill(x, y) {
    const frame = currentFrame();
    const start = y * frame.width + x;
    const targetIndex = frame.indices[start];
    const targetAlpha = frame.alpha[start];
    const fillTransparent = transparentSelected || tool === "eraser";
    const fillIndex = fillTransparent ? 0 : activeInk();
    const fillAlpha = fillTransparent ? 0 : 1;
    if (targetIndex === fillIndex && targetAlpha === fillAlpha) return false;
    const stack = [start];
    const seen = new Uint8Array(frame.width * frame.height);
    seen[start] = 1;
    while (stack.length) {
      const p = stack.pop();
      if (frame.indices[p] !== targetIndex || frame.alpha[p] !== targetAlpha) continue;
      frame.indices[p] = fillIndex;
      frame.alpha[p] = fillAlpha;
      const xPos = p % frame.width;
      const yPos = (p / frame.width) | 0;
      const neighbors = [];
      if (xPos > 0) neighbors.push(p - 1);
      if (xPos + 1 < frame.width) neighbors.push(p + 1);
      if (yPos > 0) neighbors.push(p - frame.width);
      if (yPos + 1 < frame.height) neighbors.push(p + frame.width);
      for (const next of neighbors) if (!seen[next]) { seen[next] = 1; stack.push(next); }
    }
    return true;
  }
  function pointFromEvent(event) {
    const rect = canvas.getBoundingClientRect();
    const x = Math.floor((event.clientX - rect.left) * canvas.width / rect.width);
    const y = Math.floor((event.clientY - rect.top) * canvas.height / rect.height);
    return { x: Math.max(0, Math.min(canvas.width - 1, x)), y: Math.max(0, Math.min(canvas.height - 1, y)) };
  }
  canvas.addEventListener("pointerdown", (event) => {
    if (event.button !== 0) return;
    event.preventDefault();
    const point = pointFromEvent(event);
    const frame = currentFrame();
    const p = point.y * frame.width + point.x;
    if (tool === "eyedropper") {
      transparentSelected = frame.alpha[p] === 0;
      if (mode() === "sprite") selectedSlot = Math.max(1, Math.min(3, frame.indices[p] || selectedSlot));
      else selectedColor = frame.indices[p];
      updatePalette();
      $("canvas-status").textContent = frame.alpha[p] ? (mode() === "sprite" ? "Picked ink " + frame.indices[p] : "Picked " + colorName(frame.indices[p])) : "Picked transparency";
      return;
    }
    if (tool === "fill") {
      const prior = snapshot();
      const changed = bucketFill(point.x, point.y);
      if (changed) {
        undoStack.push(prior);
        redoStack = [];
        updateHistoryButtons();
        syncSaveLabel("Unsaved changes");
        drawCanvas(); drawFrames();
      }
      return;
    }
    pushUndo();
    stroke = true;
    lastPoint = point;
    canvas.setPointerCapture(event.pointerId);
    const erase = tool === "eraser" || transparentSelected;
    setPixel(point.x, point.y, activeInk(), erase);
    drawCanvas();
  });
  canvas.addEventListener("pointermove", (event) => {
    const p = pointFromEvent(event);
    $("cursor-position").textContent = "x " + p.x + " · y " + p.y;
    if (!stroke || (tool !== "pencil" && tool !== "eraser")) return;
    const erase = tool === "eraser" || transparentSelected;
    drawLine(lastPoint.x, lastPoint.y, p.x, p.y, activeInk(), erase);
    lastPoint = p;
    drawCanvas();
  });
  canvas.addEventListener("pointerup", () => { if (stroke) { stroke = false; lastPoint = null; drawFrames(); } });
  canvas.addEventListener("pointercancel", () => { stroke = false; lastPoint = null; drawFrames(); });
  canvas.addEventListener("pointerleave", () => { $("cursor-position").textContent = "x — · y —"; });
  canvas.addEventListener("wheel", (event) => {
    if (!event.ctrlKey && !event.metaKey) return;
    event.preventDefault();
    changeZoom(event.deltaY < 0 ? 1 : -1);
  }, { passive: false });

  function changeZoom(direction) {
    const step = mode() === "sprite" ? 2 : 0.5;
    zoom = Math.max(mode() === "sprite" ? 6 : 0.5, Math.min(mode() === "sprite" ? 80 : 5, zoom + direction * step));
    scaleZoom();
  }
  function fitZoom() {
    const [width, height] = dimensions();
    const frame = $("canvas-frame");
    zoom = Math.max(mode() === "sprite" ? 6 : 0.5, Math.floor(Math.min((frame.clientWidth - 64) / width, (frame.clientHeight - 72) / height) * 10) / 10);
    scaleZoom();
  }

  document.querySelectorAll(".tool-button").forEach((button) => button.addEventListener("click", () => setTool(button.dataset.tool)));
  $("transparent-swatch").addEventListener("click", () => {
    transparentSelected = true;
    updatePalette();
    setTool("pencil");
    $("canvas-status").textContent = "Pencil will draw transparent pixels";
  });
  document.querySelectorAll(".ink-slot").forEach((button) => button.addEventListener("click", () => {
    selectedSlot = Number(button.dataset.slot);
    transparentSelected = false;
    updatePalette();
    drawPendingPreview();
  }));
  $("screen-background").addEventListener("change", (event) => {
    if (project.screenBackground !== Number(event.target.value)) pushUndo();
    project.screenBackground = Number(event.target.value);
    updateUsage();
  });
  modeSelect.addEventListener("change", (event) => {
    const next = event.target.value;
    if (next === mode()) return;
    pushUndo();
    const [width, height] = next === "sprite" ? [8, 8] : [Core.WIDTH, Core.HEIGHT];
    project.mode = next;
    project.frames = project.frames.map((frame) => resizeFrame(frame, width, height));
    zoom = next === "sprite" ? 24 : 1.5;
    transparentSelected = false;
    updatePalette(); drawCanvas(); drawFrames(); updateUsage(); scaleZoom(); drawPendingPreview();
  });
  $("preview-background").addEventListener("change", (event) => {
    $("canvas-stage").className = "canvas-stage " + (event.target.value === "checker" ? "checkerboard" : event.target.value);
  });
  $("zoom-in").addEventListener("click", () => changeZoom(1));
  $("zoom-out").addEventListener("click", () => changeZoom(-1));
  $("fit-canvas").addEventListener("click", fitZoom);
  $("undo").addEventListener("click", undo);
  $("redo").addEventListener("click", redo);
  $("project-name").addEventListener("change", (event) => {
    project.name = event.target.value.trim().slice(0, 48) || "untitled";
    event.target.value = project.name;
    updateUsage();
  });

  function addFrame(copy) {
    pushUndo();
    project.frames.splice(frameIndex + 1, 0, copy ? cloneFrame(currentFrame()) : Core.createFrame(...dimensions(), 0, false));
    frameIndex += 1;
    drawFrames(); drawCanvas();
    flash(copy ? "Frame copied" : "Blank frame added");
  }
  $("add-frame").addEventListener("click", () => addFrame(true));
  $("remove-frame").addEventListener("click", () => {
    if (project.frames.length <= 1) return;
    pushUndo();
    project.frames.splice(frameIndex, 1);
    frameIndex = Math.min(frameIndex, project.frames.length - 1);
    drawFrames(); drawCanvas();
  });

  function fileDownload(filename, blob) {
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = filename;
    anchor.style.display = "none";
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1500);
  }
  function downloadText(filename, text) {
    fileDownload(filename, new Blob([text], { type: "text/plain;charset=utf-8" }));
  }
  function downloadProject() {
    downloadText(selectedExportName() + ".tandy.json", JSON.stringify(makeProjectJson(), null, 2) + "\n");
    syncSaveLabel("Project downloaded");
    flash("Editable project saved to Downloads");
  }
  $("save-project").addEventListener("click", downloadProject);
  $("open-project").addEventListener("click", () => { $("file-picker").click(); });
  $("import-image").addEventListener("click", () => { $("file-picker").click(); });
  $("empty-import").addEventListener("click", () => { $("file-picker").click(); });
  $("file-picker").addEventListener("change", async (event) => {
    const file = event.target.files && event.target.files[0];
    event.target.value = "";
    if (!file) return;
    if (/\.json$/i.test(file.name) || file.type === "application/json") {
      try {
        const parsed = JSON.parse(await file.text());
        project = normalizeProject(parsed);
        clearPendingImport();
        frameIndex = 0; undoStack = []; redoStack = [];
        renderAll(); syncSaveLabel("Project opened"); flash("Project opened in this browser");
      } catch (error) { flash(error.message || "This project could not be opened."); }
      return;
    }
    if (/\.bin$/i.test(file.name) || file.type === "application/octet-stream") {
      try {
        const bytes = new Uint8Array(await file.arrayBuffer());
        if (bytes.length !== Core.BYTES) throw new Error("A mode-9 screen file must be exactly 32,768 bytes.");
        project = { version: 1, name: file.name.replace(/\.bin$/i, "").slice(0, 48), mode: "screen",
          frames: [Core.unpackMode9(bytes)], spriteColors: project.spriteColors.slice(), screenBackground: 0 };
        clearPendingImport();
        frameIndex = 0; undoStack = []; redoStack = []; zoom = 1.5;
        renderAll(); syncSaveLabel("Mode-9 screen opened"); flash("32 KiB mode-9 screen opened");
      } catch (error) { flash(error.message || "This mode-9 screen could not be opened."); }
      return;
    }
    if (!file.type.startsWith("image/")) { flash("Choose a PNG, JPG, WebP, or project JSON file."); return; }
    const image = new Image();
    image.onload = () => {
      pendingImage = image;
      pendingSpriteColors = null;
      pendingSourceName = file.name;
      pendingFrameImages = null;
      pendingOriginalKind = analyzeOriginalImage(image);
      pendingKind = pendingOriginalKind;
      pendingRawPreview = null;
      $("dither-recommendation").textContent = autoChoiceLabel(pendingKind.kind);
      $("pending-image").textContent = file.name + " · " + image.naturalWidth + " × " + image.naturalHeight + " px";
      $("pending-image").className = "pending-image ready";
      $("apply-image").disabled = false;
      $("apply-image").textContent = "Convert picture to colors";
      drawPendingPreview();
    };
    image.onerror = () => { flash("This picture could not be opened."); };
    image.src = URL.createObjectURL(file);
  });

  function analyzeOriginalImage(image) {
    const sample = document.createElement("canvas");
    sample.width = 128; sample.height = 128;
    const sampleCtx = sample.getContext("2d", { willReadFrequently: true });
    sampleCtx.imageSmoothingEnabled = false;
    sampleCtx.clearRect(0, 0, 128, 128);
    sampleCtx.drawImage(image, 0, 0, 128, 128);
    const sampleData = sampleCtx.getImageData(0, 0, 128, 128);
    return Core.detectImageKind(sampleData.data, 128, 128);
  }
  function autoChoiceLabel(kind) {
    if (kind === "pixel-art") return "Auto: pixel art detected · keeps hard edges, no dither.";
    if (kind === "photo") return "Auto: photo detected · Floyd–Steinberg keeps fine detail.";
    return "Auto: illustration detected · Bayer keeps edges clean.";
  }
  function drawResizedSource(image, width, height, ctx2d, modeName, pixelArt) {
    ctx2d.clearRect(0, 0, width, height);
    ctx2d.imageSmoothingEnabled = modeName !== "pixel" && !pixelArt;
    ctx2d.imageSmoothingQuality = "high";
    if (modeName === "stretch") {
      ctx2d.drawImage(image, 0, 0, width, height);
      return;
    }
    const scale = modeName === "fill"
      ? Math.max(width / image.naturalWidth, height / image.naturalHeight)
      : Math.min(width / image.naturalWidth, height / image.naturalHeight);
    const drawWidth = image.naturalWidth * scale;
    const drawHeight = image.naturalHeight * scale;
    ctx2d.drawImage(image, (width - drawWidth) / 2, (height - drawHeight) / 2, drawWidth, drawHeight);
  }

  function transparencyOptions() {
    const setting = $("import-transparency").value;
    if (setting === "opaque") return { backgroundMode: "opaque" };
    if (setting === "color") return { backgroundMode: "color", backgroundColor: activePaletteColor(), backgroundTolerance: Number($("key-tolerance").value) };
    return { backgroundMode: "preserve" };
  }
  function importRgba(imageData, opts) {
    const rgba = new Uint8ClampedArray(imageData.data);
    if (opts.backgroundMode === "opaque") for (let p = 3; p < rgba.length; p += 4) rgba[p] = 255;
    return rgba;
  }
  function conversionSettings(opts, spriteColors) {
    return {
      mode: mode(), spriteColors: spriteColors || project.spriteColors,
      dither: $("dither-mode").value, ditherStrength: Number($("dither-strength").value),
      recommendedDither: pendingKind && pendingKind.kind === "photo" ? "fs" : pendingKind && pendingKind.kind === "illustration" ? "ordered" : "none",
      pixelArt: !!(pendingKind && pendingKind.kind === "pixel-art"),
      autoCorrect: $("auto-correct").checked, autoStrength: Number($("auto-strength").value),
      exposure: Number($("exposure").value) / 50,
      contrast: Number($("contrast").value), saturation: Number($("saturation").value),
      gamma: Number($("gamma").value),
      backgroundMode: opts.backgroundMode, backgroundColor: opts.backgroundColor,
      backgroundTolerance: opts.backgroundTolerance
    };
  }
  function convertImageData(imageData, opts, spriteColors) {
    return Core.convertRgba(importRgba(imageData, opts), imageData.width, imageData.height,
      conversionSettings(opts, spriteColors));
  }
  function drawFramePreview(target, frame) {
    const local = target.getContext("2d");
    target.width = frame.width;
    target.height = frame.height;
    renderFrameInto(local, frame);
  }
  function makePendingFrames() {
    const [width, height] = dimensions();
    const opts = transparencyOptions();
    const doSheet = mode() === "sprite" && $("as-sprite-sheet").checked;
    if (doSheet) {
      if (pendingImage.naturalHeight !== 8 || pendingImage.naturalWidth < 8 || pendingImage.naturalWidth % 8 !== 0) {
        throw new Error("A horizontal sheet must be 8 pixels tall and contain complete 8 × 8 frames.");
      }
      const frames = [];
      pendingKind = { kind: "pixel-art", colors: 16, edgeRatio: 0 };
      $("dither-recommendation").textContent = autoChoiceLabel("pixel-art");
      const raw = document.createElement("canvas"); raw.width = 8; raw.height = 8;
      const rawCtx = raw.getContext("2d", { willReadFrequently: true });
      let firstPixels = null;
      for (let x = 0; x < pendingImage.naturalWidth; x += 8) {
        rawCtx.clearRect(0, 0, 8, 8);
        rawCtx.drawImage(pendingImage, x, 0, 8, 8, 0, 0, 8, 8);
        const pixels = rawCtx.getImageData(0, 0, 8, 8);
        if (firstPixels === null) firstPixels = pixels;
        frames.push(pixels);
      }
      const sources = frames.map((imageData) => importRgba(imageData, opts));
      if ($("lock-sprite-palette").checked) pendingSpriteColors = project.spriteColors.slice();
      else {
        const combined = new Uint8ClampedArray(sources.reduce((total, source) => total + source.length, 0));
        let offset = 0;
        sources.forEach((source) => { combined.set(source, offset); offset += source.length; });
        pendingSpriteColors = Core.recommendSpriteColors(combined, 8, 8 * frames.length,
          conversionSettings(opts, project.spriteColors));
      }
      updatePalette();
      const settings = conversionSettings(opts, pendingSpriteColors);
      return { frames: sources.map((source) => Core.convertRgba(source, 8, 8, settings)), source: firstPixels };
    }
    pendingKind = pendingOriginalKind || { kind: "illustration", colors: 0, edgeRatio: 0 };
    $("dither-recommendation").textContent = autoChoiceLabel(pendingKind.kind);
    const raw = document.createElement("canvas"); raw.width = width; raw.height = height;
    const rawCtx = raw.getContext("2d", { willReadFrequently: true });
    const pixelArt = !!(pendingKind && pendingKind.kind === "pixel-art");
    drawResizedSource(pendingImage, width, height, rawCtx, $("resize-mode").value, pixelArt);
    const source = rawCtx.getImageData(0, 0, width, height);
    if (mode() === "sprite") {
      pendingSpriteColors = $("lock-sprite-palette").checked
        ? project.spriteColors.slice()
        : Core.recommendSpriteColors(importRgba(source, opts), width, height,
          conversionSettings(opts, project.spriteColors));
      updatePalette();
    } else pendingSpriteColors = null;
    return { frames: [convertImageData(source, opts, pendingSpriteColors || project.spriteColors)], source };
  }
  function drawPendingPreview() {
    if (!pendingImage) return;
    try {
      const pending = makePendingFrames();
      pendingFrameImages = pending.frames;
      pendingRawPreview = pending.source;
      const first = pendingFrameImages[0];
      const preview = $("pending-preview");
      preview.width = first.width;
      preview.height = first.height;
      const previewScale = Math.min(230 / first.width, 100 / first.height);
      preview.style.width = Math.max(1, Math.round(first.width * previewScale)) + "px";
      preview.style.height = Math.max(1, Math.round(first.height * previewScale)) + "px";
      renderFrameInto(preview.getContext("2d"), first, mode() === "sprite" ? pendingSpriteColors : null);
      const before = $("pending-source-preview");
      before.width = pendingRawPreview.width;
      before.height = pendingRawPreview.height;
      before.style.width = Math.max(1, Math.round(pendingRawPreview.width * previewScale)) + "px";
      before.style.height = Math.max(1, Math.round(pendingRawPreview.height * previewScale)) + "px";
      before.getContext("2d").putImageData(pendingRawPreview, 0, 0);
      $("pending-preview-wrap").hidden = false;
      $("pending-image").textContent = pendingSourceName + " · " + pendingImage.naturalWidth + " × " + pendingImage.naturalHeight + " → " + first.width + " × " + first.height + (pendingFrameImages.length > 1 ? " · " + pendingFrameImages.length + " frames" : "");
      $("pending-image").className = "pending-image ready";
      $("apply-image").disabled = false;
    } catch (error) {
      pendingFrameImages = null;
      $("pending-preview-wrap").hidden = true;
      $("pending-image").textContent = error.message;
      $("pending-image").className = "pending-image error";
      $("apply-image").disabled = true;
    }
  }
  $("resize-mode").addEventListener("change", drawPendingPreview);
  $("dither-mode").addEventListener("change", drawPendingPreview);
  $("dither-strength").addEventListener("input", (event) => {
    $("dither-strength-value").value = event.target.value + "%";
    $("dither-strength-value").textContent = event.target.value + "%";
    drawPendingPreview();
  });
  $("auto-correct").addEventListener("change", drawPendingPreview);
  $("auto-strength").addEventListener("input", (event) => {
    $("auto-strength-value").value = event.target.value + "%";
    $("auto-strength-value").textContent = event.target.value + "%";
    drawPendingPreview();
  });
  ["exposure", "contrast", "saturation", "gamma"].forEach((controlId) => {
    $(controlId).addEventListener("input", (event) => {
      const value = Number(event.target.value);
      const label = controlId === "exposure" ? ((value > 0 ? "+" : "") + (value / 50).toFixed(1) + " EV") : value + "%";
      $(controlId + "-value").value = label;
      $(controlId + "-value").textContent = label;
      drawPendingPreview();
    });
  });
  $("import-transparency").addEventListener("change", () => {
    $("key-tolerance").disabled = $("import-transparency").value !== "color";
    drawPendingPreview();
  });
  $("key-tolerance").addEventListener("input", (event) => {
    $("tolerance-value").value = event.target.value;
    $("tolerance-value").textContent = event.target.value;
    drawPendingPreview();
  });
  $("as-sprite-sheet").addEventListener("change", () => {
    $("resize-mode").disabled = $("as-sprite-sheet").checked;
    drawPendingPreview();
  });
  $("lock-sprite-palette").addEventListener("change", drawPendingPreview);
  $("apply-image").addEventListener("click", () => {
    if (!pendingFrameImages || !pendingFrameImages.length) return;
    pushUndo();
    if (mode() === "sprite" && pendingSpriteColors) project.spriteColors = pendingSpriteColors.slice();
    project.frames = pendingFrameImages.map(cloneFrame);
    frameIndex = 0;
    project.name = pendingSourceName.replace(/\.[^.]+$/, "").slice(0, 48) || "imported-art";
    $("project-name").value = project.name;
    updatePalette(); drawFrames(); drawCanvas(); updateUsage();
    flash("Picture converted to the Tandy palette");
  });

  function frameToRgbaCanvas(frame) {
    const output = document.createElement("canvas");
    output.width = frame.width; output.height = frame.height;
    renderFrameInto(output.getContext("2d"), frame);
    return output;
  }
  function exportPng(sheet) {
    const frames = sheet ? project.frames : [currentFrame()];
    const [width, height] = dimensions();
    const output = document.createElement("canvas");
    output.width = width * frames.length;
    output.height = height;
    const outCtx = output.getContext("2d");
    frames.forEach((frame, index) => outCtx.drawImage(frameToRgbaCanvas(frame), index * width, 0));
    output.toBlob((blob) => {
      if (!blob) { flash("PNG export failed."); return; }
      fileDownload(selectedExportName() + (sheet ? "_sheet" : "_preview") + ".png", blob);
      flash(sheet ? "PNG frame sheet downloaded" : "PNG picture downloaded");
    }, "image/png");
  }
  $("export-png").addEventListener("click", () => exportPng(false));
  $("export-sheet").addEventListener("click", () => exportPng(true));
  $("export-c").addEventListener("click", () => {
    const data = mode() === "sprite"
      ? Core.exportSpriteHeader(project.frames, project.spriteColors, selectedExportName())
      : Core.exportScreenHeader(currentFrame(), selectedExportName(), project.screenBackground);
    downloadText(selectedExportName() + (mode() === "sprite" ? "_sprite.h" : "_mode9.h"), data);
    flash("libtandy C header downloaded");
  });
  $("export-binary").addEventListener("click", () => {
    if (mode() !== "screen") {
      flash("Choose Full screen to export the 32 KiB mode-9 bytes.");
      return;
    }
    const data = Core.packMode9(currentFrame(), project.screenBackground);
    fileDownload(selectedExportName() + "_mode9.bin", new Blob([data], { type: "application/octet-stream" }));
    flash("32,768 mode-9 bytes downloaded");
  });
  $("show-advanced").addEventListener("change", (event) => { $("advanced-details").hidden = !event.target.checked; });
  $("copy-snippet").addEventListener("click", async () => {
    try { await navigator.clipboard.writeText($("usage-snippet").textContent); flash("Usage example copied"); }
    catch (_) { flash("Copy is unavailable here; select the example text to copy it."); }
  });

  $("load-example").addEventListener("click", () => {
    project = makeExampleProject();
    frameIndex = 0; selectedSlot = 1; selectedColor = 14;
    undoStack = []; redoStack = []; zoom = 24;
    renderAll(); syncSaveLabel("Example loaded"); flash("Example sprite ready to edit");
  });

  document.addEventListener("keydown", (event) => {
    if (event.ctrlKey || event.metaKey) {
      if (event.key.toLowerCase() === "z" && !event.shiftKey) { event.preventDefault(); undo(); return; }
      if (event.key.toLowerCase() === "y" || (event.key.toLowerCase() === "z" && event.shiftKey)) { event.preventDefault(); redo(); return; }
      return;
    }
    if (/^(INPUT|SELECT|TEXTAREA)$/.test(document.activeElement.tagName)) return;
    const key = event.key.toLowerCase();
    if (key === "p") setTool("pencil");
    if (key === "e") setTool("eraser");
    if (key === "f") setTool("fill");
    if (key === "i") setTool("eyedropper");
  });
  window.addEventListener("resize", () => { if (zoom > 0) scaleZoom(); });

  project = makeExampleProject();
  zoom = 28;
  $("key-tolerance").disabled = true;
  $("tolerance-value").value = "0";
  $("tolerance-value").textContent = "0";
  renderAll();
  setTool("pencil");
})();
