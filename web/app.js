// SPDX-License-Identifier: GPL-3.0-only
// MONO2K's emulator page: the dithered wordmark, the FM-1 panel in one bit, the screen, the controls (mouse, touch,
// computer keys, MIDI), the status line under the panel (what the control under the pointer does, help.js), the bar's
// tools and the saves. The firmware itself runs in worklet.js; the page's other copies of it (the guide, the machines)
// in screens.js, guide.js and engines.js, from the module compiled here.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  /* the firmware, fetched and compiled once for every copy the page runs */
  const wasmP = fetch("m2k.wasm?v=87").then((r) => { if (!r.ok) throw new Error(r.status); return r.arrayBuffer(); });
  window.M2K_CORE = { bytes: wasmP, module: wasmP.then((b) => WebAssembly.compile(b)) };
  const NS = "http://www.w3.org/2000/svg";
  const css = () => getComputedStyle(document.documentElement);
  const ink = () => css().getPropertyValue("--ink").trim() || "#0b0b0b";
  const paper = () => css().getPropertyValue("--paper").trim() || "#f2f1ec";
  const BAYER = (() => {
    const b = (n) => {
      if (n === 1) return [[0]];
      const s = b(n / 2), m = n / 2, r = [];
      for (let y = 0; y < n; y++) {
        r.push([]);
        for (let x = 0; x < n; x++) r[y].push(4 * s[y % m][x % m] + [[0, 2], [3, 1]][(y / m) | 0][(x / m) | 0]);
      }
      return r;
    };
    return b(8);
  })();
  const hex = (c) => { const m = c.replace("#", ""); return [parseInt(m.slice(0, 2), 16), parseInt(m.slice(2, 4), 16), parseInt(m.slice(4, 6), 16)]; };

  // ------------------------------------------------------------------------------------- dither textures (CSS)
  function textures() {
    const make = (w, h, f, col = ink()) => {
      const c = document.createElement("canvas");
      c.width = w; c.height = h;
      const g = c.getContext("2d");
      g.fillStyle = col;
      for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) if (f(x, y)) g.fillRect(x, y, 1, 1);
      return `url(${c.toDataURL()})`;
    };
    const r = document.documentElement.style;
    r.setProperty("--d-sparse", make(4, 4, (x, y) => x === 0 && y === 0));
    r.setProperty("--d-dots", make(2, 2, (x, y) => x === 0 && y === 0));
    r.setProperty("--d-check", make(2, 2, (x, y) => (x + y) % 2 === 0));
    /* dithered gradients, ink to nothing: across (256 x 8) and down (8 x 64), drawn big and pixelated */
    r.setProperty("--g-across", make(256, 8, (x, y) => BAYER[y & 7][x & 7] < 64 - x / 4));
    r.setProperty("--g-down", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < 64 - y));
    r.setProperty("--g-down-inv", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < 64 - y, paper()));   /* for a block in the other theme */
    r.setProperty("--g-up", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < y));
    r.setProperty("--d-check-p", make(2, 2, (x, y) => (x + y) % 2 === 0, paper()));   /* the checker in paper, on ink */
    /* the page's one motion: masks that are solid, then the screen's dither falling away, then nothing. Slid across a
     * block (wipe) or down a screen (scan), stepped, they bring it in as the FM-1's transitions do (style.css) */
    const ramp = (i, b) => i < 32 || (i < 64 && b < (64 - i) * 2);
    r.setProperty("--m-wipe", make(96, 8, (x, y) => ramp(x, BAYER[y & 7][x & 7])));
    r.setProperty("--m-scan", make(8, 96, (x, y) => ramp(y, BAYER[y & 7][x & 7])));
  }

  // --------------------------------------------------------------------------- the wordmark: the logotype, dithered
  const wm = $("wordmark");
  let wmBase = null;
  function wordmark() {
    const cssW = wm.parentElement.clientWidth - 32;
    const cell = cssW > 900 ? 4 : 3;
    const W = Math.max(60, Math.floor(cssW / cell));
    /* the logotype (mono2k_handoff/brand), filled across the grid at the cells' resolution, then dithered per frame */
    const off = document.createElement("canvas");
    const g = off.getContext("2d");
    const L = window.M2K_PANEL.LOGO;
    const H = Math.max(12, Math.round(W * L.h / L.w));
    off.width = W; off.height = H;
    g.setTransform(W / L.w, 0, 0, H / L.h, 0, 0);
    g.fillStyle = "#000";
    for (const [d, rule] of L.paths) g.fill(new Path2D(d), rule);
    g.setTransform(1, 0, 0, 1, 0, 0);
    const a = g.getImageData(0, 0, W, H).data;
    const in_ = (x, y) => x >= 0 && y >= 0 && x < W && y < H && a[(y * W + x) * 4 + 3] > 110;
    const on = new Uint8Array(W * H), edge = new Uint8Array(W * H);
    for (let y = 0; y < H; y++)
      for (let x = 0; x < W; x++) {
        if (!in_(x, y)) continue;
        on[y * W + x] = 1;
        edge[y * W + x] = !in_(x - 1, y) || !in_(x + 1, y) || !in_(x, y - 1) || !in_(x, y + 1) ? 1 : 0;
      }
    wmBase = { W, H, on, edge, cell };
    wm.width = W; wm.height = H;
    wm.style.aspectRatio = `${W} / ${H}`;
    drawWordmark(performance.now());
  }

  /* its life, one bit-crusher: the dither drifting all the time, slowly and smoothly at random across the letters; now
   * and then (every 6 to 12 s, never the same twice running) a crush (the resolution and the depth falling, then back),
   * a break (rows shifted, slices slid: the letters broken) or a decay (worn down to the outlines); while the emulator
   * plays, each hit of the music crushes it a little. Nothing moves the letters themselves. */
  const EV = { crush: [900, 0.45], break: [420, 0.35], decay: [1400, 0.2] };   /* length (ms), how often */
  const motion = !matchMedia("(prefers-reduced-motion: reduce)").matches;
  let ev = null, evT0 = 0, evNext = 3500, evLast = "", evSeed = 1, evPeak = 1, kickT = -1e9, kickAmp = 0, lvAvg = 0, lastDraw = 0;
  let analyser = null, abuf = null;
  function drawWordmark(t) {
    if (!wmBase) return;
    const { W, H, on, edge } = wmBase;
    const g = wm.getContext("2d"), img = g.createImageData(W, H);
    const I = hex(ink()), P = hex(paper());
    const p = ev ? Math.min(1, (t - evT0) / EV[ev][0]) : 0;
    const env = ev ? Math.sin(Math.PI * Math.min(1, p * 1.15)) * evPeak : 0;           /* in, held a moment, out */
    const k = motion ? Math.max(0, 1 - (t - kickT) / 240) * kickAmp : 0;
    const crush = Math.min(1, (ev === "crush" ? env : 0) + 0.55 * k);
    const brk = ev === "break" ? env : 0, decay = ev === "decay" ? env : 0;
    let seed = brk ? ((t / 45) | 0) * 7919 + evSeed : evSeed;
    const rnd = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff; };
    /* the drift: a few slow sines out of step, smooth and never repeating soon */
    const tt = motion ? t : 0;
    const dens = new Float32Array(W);
    const levels = crush > 0.05 ? Math.max(2, Math.round(9 - crush * 7)) : 0;          /* the depth, crushed */
    for (let x = 0; x < W; x++) {
      const u = x / W;
      const drift = 0.11 * Math.sin(x * 0.061 + tt * 0.00047) * Math.sin(x * 0.017 - tt * 0.00031) + 0.05 * Math.sin(x * 0.13 + tt * 0.0011);
      let d = 0.14 + 0.86 * Math.pow(u, 1.3) + drift;                                  /* sparse to solid, as on the bitmap poster */
      d *= 1 - 0.85 * decay;
      if (levels) d = Math.round(d * levels) / levels;
      dens[x] = d * 64;
    }
    const bs = 1 + Math.floor(crush * 3.2);                                            /* the resolution, crushed: 1 .. 4 cells */
    const sw = Math.max(2, (W / 28) | 0), slice = new Int16Array(Math.ceil(W / sw) + 1);
    if (brk) for (let i = 0; i < slice.length; i++) slice[i] = rnd() < 0.35 ? Math.round((rnd() - 0.5) * H * 0.24 * brk) : 0;
    let band = 0, shift = 0, inv = false;
    for (let y = 0; y < H; y++) {
      if (brk && band-- <= 0) { band = 2 + ((rnd() * 9) | 0); const act = rnd() < 0.4 * brk + 0.1; shift = act ? Math.round((rnd() - 0.5) * 34 * brk) : 0; inv = act && rnd() < 0.15; }
      for (let x = 0; x < W; x++) {
        let sx = x - shift, sy = y + (brk ? slice[(x / sw) | 0] : 0), bx = sx, by = sy;
        if (bs > 1) { bx = Math.floor(sx / bs); by = Math.floor(sy / bs); sx = bx * bs; sy = by * bs; }   /* held over the block, */
        let v = 0;                                                                                    /* dithered on the blocks' grid */
        if (sx >= 0 && sx < W && sy >= 0 && sy < H) {
          const i = sy * W + sx;
          if (on[i]) v = edge[i] || BAYER[by & 7][bx & 7] < dens[sx] ? 1 : 0;
        }
        if (inv) v ^= 1;
        const c = v ? I : P, o = (y * W + x) * 4;
        img.data[o] = c[0]; img.data[o + 1] = c[1]; img.data[o + 2] = c[2]; img.data[o + 3] = 255;
      }
    }
    g.putImageData(img, 0, 0);
  }
  /* the music's level, from the output: a hit is a jump over its recent average */
  function level() {
    if (!analyser) return 0;
    if (!abuf) abuf = new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(abuf);
    let s = 0;
    for (let i = 0; i < abuf.length; i++) s += abuf[i] * abuf[i];
    return Math.sqrt(s / abuf.length);
  }
  function wmTick(t) {
    let draw = false;
    if (motion) {
      if (!ev && t > evNext) {                                   /* the next event, by its weight, not the last again */
        const names = Object.keys(EV).filter((n) => n !== evLast);
        let r = Math.random() * names.reduce((a, n) => a + EV[n][1], 0);
        ev = names.find((n) => (r -= EV[n][1]) <= 0) || names[0];
        evLast = ev; evT0 = t; evSeed = (Math.random() * 1e9) | 0; evPeak = 0.6 + Math.random() * 0.4;
      }
      if (ev && t > evT0 + EV[ev][0]) { ev = null; evNext = t + 6000 + Math.random() * 6000; draw = true; }
      const lv = level();
      if (lv > 0.015 && lv > lvAvg * 1.5 && t - kickT > 110) { kickT = t; kickAmp = Math.min(1, 0.3 + lv * 2.2); }
      lvAvg += (lv - lvAvg) * 0.08;
      if (ev || t - kickT < 260 || t - lastDraw > 90) draw = true;   /* at rest the drift alone, ~11 frames a second */
    }
    if (draw) { lastDraw = t; drawWordmark(t); }
    requestAnimationFrame(wmTick);
  }

  // ------------------------------------------------------------------------------------------------ the panel
  // the FM-1's geometry and MONO2K's names for its controls: panel.js, shared with the manual
  const { isBlack, KEYR, BTNR, ENC_CX, ENC_CY, BTN, ROLE, ENC, EROLE, NOTE, BLACK_ROLE } = window.M2K_PANEL;

  // computer keys: the 27 keys as a tracker's piano; letters for the buttons (F-keys too)
  const KEYMAP = {
    KeyZ: ["k", 0], KeyS: ["k", 1], KeyX: ["k", 2], KeyD: ["k", 3], KeyC: ["k", 4], KeyF: ["k", 5], KeyV: ["k", 6],
    KeyB: ["k", 7], KeyH: ["k", 8], KeyN: ["k", 9], KeyJ: ["k", 10], KeyM: ["k", 11], KeyQ: ["k", 12], Digit2: ["k", 13],
    KeyW: ["k", 14], Digit3: ["k", 15], KeyE: ["k", 16], Digit4: ["k", 17], KeyR: ["k", 18], KeyT: ["k", 19],
    Digit6: ["k", 20], KeyY: ["k", 21], Digit7: ["k", 22], KeyU: ["k", 23], KeyI: ["k", 24], Digit9: ["k", 25], KeyO: ["k", 26],
    KeyP: ["b", 0], F1: ["b", 0], Digit0: ["b", 1], F2: ["b", 1], Digit8: ["b", 2], F3: ["b", 2], KeyL: ["b", 3], F4: ["b", 3],
    Digit5: ["b", 4], F5: ["b", 4], KeyG: ["b", 5], F6: ["b", 5], Digit1: ["b", 6], F7: ["b", 6], Home: ["b", 6],
    Backslash: ["b", 7], F8: ["b", 7], KeyK: ["b", 8], F9: ["b", 8], KeyA: ["b", 9], F10: ["b", 9],
    Space: ["b", 10], Enter: ["b", 11], Minus: ["b", 12], Equal: ["b", 13],
    Semicolon: ["e", 0, -1], Quote: ["e", 0, 1], Comma: ["e", 1, -1], Period: ["e", 1, 1],
    BracketLeft: ["e", 2, -1], BracketRight: ["e", 2, 1], Slash: ["e", 7, 1],
  };
  const hintOf = (kind, idx) => {
    const order = { k: [], b: [] };
    for (const [code, m] of Object.entries(KEYMAP)) if (m[0] === kind && m[1] === idx && !code.startsWith("F") && code !== "Home") return code.replace("Key", "").replace("Digit", "").replace("Backslash", "\\").replace("Space", "SPACE").replace("Enter", "ENTER").replace("Minus", "-").replace("Equal", "=");
    void order;
    return "";
  };

  const svg = $("panel");
  const el = (tag, attrs, parent = svg) => { const e = document.createElementNS(NS, tag); for (const k in attrs) e.setAttribute(k, attrs[k]); parent.appendChild(e); return e; };
  const rect = (r, rad, attrs, p) => el("rect", { x: r[0], y: r[1], width: r[2], height: r[3], rx: rad, ...attrs }, p);
  const text = (x, y, s, size, attrs = {}, p = svg) => Object.assign(el("text", { x, y, "font-size": size, "text-anchor": "middle", "font-weight": 700, "letter-spacing": "0.04em", fill: "currentColor", ...attrs }, p), { textContent: s });
  const keyEl = [], btnEl = [], encEl = [];
  let selKnob = 3;
  const encAngle = new Array(8).fill(0);

  function buildPanel() {
    svg.innerHTML = "";
    keyEl.length = 0; btnEl.length = 0; encEl.length = 0;
    svg.style.color = "var(--ink)";
    const defs = el("defs", {});
    const pat = (id, w, h, cells, fill = "currentColor") => { const p = el("pattern", { id, width: w, height: h, patternUnits: "userSpaceOnUse" }, defs); for (const [x, y] of cells) el("rect", { x, y, width: 1, height: 1, fill }, p); };
    pat("pSparse", 4, 4, [[0, 0]]);
    pat("pDots", 2, 2, [[0, 0]]);
    pat("pCheck", 2, 2, [[0, 0], [1, 1]]);
    pat("pLines", 1, 2, [[0, 0]]);
    const S = { fill: "none", stroke: "currentColor", "stroke-width": 1.5 };
    rect([22, 22, 860, 522], 38, { fill: "var(--paper)", stroke: "currentColor", "stroke-width": 2.5 });
    /* the keys: an island in the other theme's colours, as the screen is; patterns are drawn in the defs' colours,
     * so the island's ink is the page's paper */
    const kb = el("g", { class: "inv" });
    pat("pDotsI", 2, 2, [[0, 0]], "var(--paper)");
    pat("pCheckI", 2, 2, [[0, 0], [1, 1]], "var(--paper)");
    rect([52, 306, 800, 198], 20, { fill: "var(--paper)" }, kb);
    {
      const c = document.createElement("canvas");
      c.width = 400; c.height = 99;
      const g = c.getContext("2d");
      g.fillStyle = paper();
      for (let y = 0; y < 99; y++) for (let x = 0; x < 400; x++) if (BAYER[y & 7][x & 7] < (y / 99) * 26) g.fillRect(x, y, 1, 1);
      const p = el("pattern", { id: "pBed", x: 52, y: 306, width: 800, height: 198, patternUnits: "userSpaceOnUse" }, defs);
      el("image", { href: c.toDataURL(), width: 800, height: 198, preserveAspectRatio: "none", style: "image-rendering: pixelated" }, p);
    }
    rect([52, 306, 800, 198], 20, { fill: "url(#pBed)", stroke: "currentColor", "stroke-width": 1, "stroke-dasharray": "2 3" }, kb);
    /* the buttons: in the page's colours */
    const bg = el("g", {});
    for (const [r, rad] of [[[484, 162, 338, 124], 8], [[77, 243, 127, 36], 6]]) {
      rect(r, rad, { fill: "var(--paper)" }, bg);
      rect(r, rad, { fill: "url(#pSparse)", stroke: "currentColor", "stroke-width": 1, "stroke-dasharray": "2 3" }, bg);
    }
    /* the screen: the LCD (247, 59, 201 square) in a bezel of its own colour, 10 of padding, rounded, and a line */
    rect([233, 45, 229, 229], 18, { fill: "none", stroke: "currentColor", "stroke-width": 1.5 });
    bezel = rect([237, 49, 221, 221], 14, { fill: "currentColor" });
    bezelDark = -1;
    /* the isotype, top left, where the FM-1 has its maker's name */
    const iso = window.M2K_PANEL.ISO, isoG = el("g", { transform: `translate(58 34) scale(${15 / iso.h})`, fill: "currentColor", "aria-label": "MONO2K" });
    for (const [d, rule] of iso.paths) el("path", { d, "fill-rule": rule }, isoG);
    text(846, 47, "SIX MONO MACHINES · M-VAVE FM-1", 6.5, { "text-anchor": "end" });
    text(822, 296, "M-VAVE FM-1 · 27 KEYS · F3–G5", 6.5, { "text-anchor": "end" });
    for (let k = 0; k < 27; k++) {
      const r = KEYR[k], cx = r[0] + 21, g = el("g", { "data-c": "k" + k, class: "ctl" }, kb);
      const cap = rect(r, 6, { fill: "var(--paper)", stroke: "currentColor", "stroke-width": 1.5 }, g);
      const led = rect([cx - 3, r[1] + 10, 6, 26], 1, { fill: "none", stroke: "currentColor", "stroke-width": 1 }, g);
      const m = 53 + k;
      const lab = text(cx, r[1] + 52, NOTE[m % 12] + (Math.floor(m / 12) - 1), 8, {}, g);
      const role = isBlack(k) ? text(cx, r[1] + 63, BLACK_ROLE[KEYR.slice(0, k).filter((_, j) => isBlack(j)).length], 7, { "font-weight": 900 }, g) : null;
      const h = hintOf("k", k);
      const hint = h ? text(cx, r[1] + 72, h, 7, { "font-family": "ui-monospace, Menlo, monospace", "font-weight": 700 }, g) : null;
      keyEl.push({ cap, led, lab, role, hint, lvl: -1 });
    }
    for (let i = 0; i < 14; i++) {
      const r = BTNR[i], oct = i >= 12, g = el("g", { "data-c": "b" + i, class: "ctl" }, bg);
      const cap = rect(r, 3, { fill: "var(--paper)", stroke: "currentColor", "stroke-width": 1.5 }, g);
      const t = text(r[0] + r[2] / 2, r[1] + (oct ? 15 : 15), BTN[i], oct ? 9 : 8.5, { "font-weight": 800 }, g);
      if (!oct) text(r[0] + r[2] / 2, r[1] - 5, ROLE[i], 7, { "font-weight": 900 }, bg);
      const h = hintOf("b", i);
      const hint = h ? text(r[0] + r[2] / 2, oct ? r[1] + r[3] + 10 : r[1] + 29, h, 7, { "font-family": "ui-monospace, Menlo, monospace" }, g) : null;
      btnEl.push({ cap, t, hint, lvl: -1 });
    }
    for (let i = 0; i < 8; i++) {
      const cx = ENC_CX[i], cy = ENC_CY[i], g = el("g", { "data-c": "e" + i, class: "ctl" });
      const sel = el("circle", { cx, cy, r: 26, ...S, "stroke-width": 2.5, opacity: 0 }, g);
      el("circle", { cx, cy, r: 21, fill: "var(--paper)", stroke: "currentColor", "stroke-width": 1.5 }, g);
      el("circle", { cx, cy, r: 17, fill: "none", stroke: "currentColor", "stroke-width": 1, "stroke-dasharray": "1.5 2.5" }, g);
      el("circle", { cx, cy, r: 9, fill: "url(#pCheck)", stroke: "currentColor", "stroke-width": 1 }, g);
      const ptr = el("line", { x1: cx, y1: cy - 10, x2: cx, y2: cy - 19, stroke: "currentColor", "stroke-width": 3, "stroke-linecap": "square" }, g);
      text(cx, cy - 37, ENC[i], 6.5, { "font-weight": 600 });
      const role = text(cx, cy - 28, EROLE[i], 8, { "font-weight": 900 });
      encEl.push({ sel, ptr, cx, cy, role });
    }
    showSel();
    lastInfo = ""; knobKey = "";
  }
  function showSel() { encEl.forEach((e, i) => e.sel.setAttribute("opacity", i === selKnob ? 1 : 0)); }
  function turnVisual(i, d) {
    encAngle[i] += d * 0.26;
    const e = encEl[i], s = Math.sin(encAngle[i]), c = Math.cos(encAngle[i]);
    e.ptr.setAttribute("x1", e.cx + 10 * s); e.ptr.setAttribute("y1", e.cy - 10 * c);
    e.ptr.setAttribute("x2", e.cx + 19 * s); e.ptr.setAttribute("y2", e.cy - 19 * c);
  }
  const lvlFill = (v) => (v > 200 ? "currentColor" : v > 90 ? "url(#pCheckI)" : v > 20 ? "url(#pDotsI)" : "var(--paper)");
  function leds(L) {
    for (let k = 0; k < 27; k++) {
      const v = L[k], e = keyEl[k], q = v > 200 ? 3 : v > 90 ? 2 : v > 20 ? 1 : 0;
      if (q === e.lvl) continue;
      e.lvl = q;
      e.led.setAttribute("fill", lvlFill(v));
      e.cap.setAttribute("stroke-width", q === 3 ? 3 : 1.5);
    }
    for (let i = 0; i < 14; i++) {
      const v = L[27 + i], e = btnEl[i], q = v > 200 ? 3 : v > 60 ? 2 : 0;
      if (q === e.lvl) continue;
      e.lvl = q;
      e.cap.setAttribute("fill", q === 3 ? "currentColor" : q === 2 ? "url(#pCheck)" : "var(--paper)");
      e.t.setAttribute("fill", q === 3 ? "var(--paper)" : "currentColor");
      if (e.hint) e.hint.setAttribute("fill", q === 3 ? "var(--paper)" : "currentColor");
    }
  }

  // ------------------------------------------------------------------------------------------------- the LCD
  const lcd = $("lcd"), lg = lcd.getContext("2d"), limg = lg.createImageData(240, 240);
  /* the screen's two colours: the firmware draws light (ink on paper) or dark (its negative, GLO > SET), and
   * the palette gives the dark and the light colour for either */
  const PALETTES = [
    { name: "Paper", dark: "#0b0b0b", light: "#f2f1ec" },
    { name: "LCD", dark: "#16211b", light: "#a9b79c" },
    { name: "UV", dark: "#06050b", light: "#38e4ff" },
    { name: "Hot", dark: "#06050b", light: "#ff2f8e" },
  ];
  let pal = 0;
  const isDark = () => { const t = document.documentElement.getAttribute("data-theme"); return t ? t === "dark" : matchMedia("(prefers-color-scheme: dark)").matches; };
  try { pal = +(localStorage.getItem("mono2k.palette2") || 0) % PALETTES.length; } catch (e) { pal = 0; }
  let lastFb = null, bezel = null, bezelDark = -1;
  function drawLcd(fb) {
    lastFb = fb;
    const I = hex(PALETTES[pal].dark), P = hex(PALETTES[pal].light), d = limg.data;
    let inked = 0;
    for (let i = 0, p = 0; i < 7200; i++) {
      const b = fb[i];
      for (let j = 7; j >= 0; j--, p += 4) {
        const on = (b >> j) & 1, c = on ? I : P;
        inked += on;
        d[p] = c[0]; d[p + 1] = c[1]; d[p + 2] = c[2]; d[p + 3] = 255;
      }
    }
    lg.putImageData(limg, 0, 0);
    /* the bezel takes the screen's background: mostly ink is a dark screen (with some slack, so a wipe or a
     * gradient doesn't flicker it) */
    const dark = inked > 57600 * 0.62 ? 1 : inked < 57600 * 0.38 ? 0 : bezelDark < 0 ? 0 : bezelDark;
    if (bezel && dark !== bezelDark) { bezelDark = dark; bezel.setAttribute("fill", dark ? PALETTES[pal].dark : PALETTES[pal].light); }
  }
  function setPalette(i) {
    pal = i % PALETTES.length;
    try { localStorage.setItem("mono2k.palette2", String(pal)); } catch (e) { /* private mode */ }
    bezelDark = -1;
    if (lastFb) drawLcd(lastFb);
  }

  // -------------------------------------------------------------------------------------- the firmware core
  let wasmBytes = null, pre = null, preT0 = 0, node = null, ctx = null, running = false;
  const cstr = (x, p) => { const m = new Uint8Array(x.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
  const STORE_KEY = "mono2k.store.v2";
  const loadStore = () => {
    try {
      const s = localStorage.getItem(STORE_KEY);
      if (!s) return null;
      const b = atob(s), u = new Uint8Array(b.length);
      for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i);
      return u.buffer;
    } catch (e) { return null; }
  };
  const saveStore = (bytes) => {
    try {
      let s = "";
      for (let i = 0; i < bytes.length; i += 4096) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 4096));
      localStorage.setItem(STORE_KEY, btoa(s));
      status("Saved in this browser.");
    } catch (e) { status("Could not save here (private window?)."); }
  };
  const status = (s) => console.info("MONO2K:", s);   /* the saves' messages, to the console (the status line is say()) */

  async function boot() {
    wasmBytes = await wasmP;
    pre = (await WebAssembly.instantiate(await window.M2K_CORE.module, {})).exports;
    if (pre._initialize) pre._initialize();
    pre.w_init(2000);
    pre.w_setting(0, isDark() ? 0 : 127);
    preT0 = performance.now();
    machines();
    params();
    namesFrom(pre);
    requestAnimationFrame(preTick);
  }
  function preTick(t) {
    if (running) return;
    const ms = (t - preT0) >>> 0;
    drawLcd(new Uint8Array(pre.memory.buffer, pre.w_frame(ms), 7200));
    leds(new Uint8Array(pre.memory.buffer, pre.w_leds(), 41));
    info(new Uint8Array(pre.memory.buffer, pre.w_info(), 32));
    requestAnimationFrame(preTick);
  }

  async function start() {
    if (running) return;
    $("start").classList.add("gone");
    try {
      try { ctx = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" }); }
      catch (e) { ctx = new AudioContext({ latencyHint: "interactive" }); }
      ctx.onstatechange = () => sndShow();
      await ctx.audioWorklet.addModule("worklet.js?v=35");
      node = new AudioWorkletNode(ctx, "mono2k", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
      node.connect(ctx.destination);
      analyser = ctx.createAnalyser();                /* the hero listens: its letters kick on the beat */
      analyser.fftSize = 512;
      node.connect(analyser);
      node.port.onmessage = (ev) => {
        const m = ev.data;
        if (m.t === "frame") {
          drawLcd(m.fb);
          leds(m.leds);
          info(m.info);
          if (m.store) saveStore(m.store);
          if (m.names) send({ t: "names" });
        } else if (m.t === "names") {
          patNames = m.pats; projNames = m.projs;
        } else if (m.t === "store") {
          saveStore(m.bytes);
        } else if (m.t === "export") {
          const a = document.createElement("a");
          const nm = (projNames[m.slot] || "PROJECT").replace(/[^A-Z0-9]+/gi, "-");
          a.href = URL.createObjectURL(new Blob([m.bytes], { type: "application/octet-stream" }));
          a.download = `mono2k-P${m.slot + 1}-${nm}.m2kp`;
          a.click();
          setTimeout(() => URL.revokeObjectURL(a.href), 2000);
          status(`Exported P${m.slot + 1}.`);
        } else if (m.t === "imported") {
          status(m.ok ? `Imported into P${m.slot + 1}. Load it to play it.` : "That file is not a MONO2K project.");
        } else if (m.t === "mout") {                 /* what the core sends, as the sound plays it */
          midiSend(m.list);
        } else if (m.t === "ready") {
          displayMode();
          status(ctx.sampleRate === 44100 ? "Running at 44.1 kHz, as on the FM-1." : `Running at ${ctx.sampleRate} Hz, resampled from 44.1 kHz.`);
          say(ctx.sampleRate === 44100 ? "Sound on, at 44.1 kHz as on the FM-1." : `Sound on, resampled to ${ctx.sampleRate} Hz.`);
        }
      };
      node.port.postMessage({ t: "wasm", bytes: wasmBytes, seed: (Math.random() * 4294967295) >>> 0, store: loadStore() });
      await ctx.resume();
      running = true;
      armed = true;
      sndShow();
      svg.focus({ preventScroll: true });   /* the bar can start it too: stay where the reader is */
    } catch (e) {
      status("Could not start the audio: " + e.message);
      say("The sound could not start here: " + e.message);
      $("start").classList.remove("gone");
    }
  }
  const send = (m) => { if (node) node.port.postMessage(m); };

  // ---------------------------------------------------------------------------------------------------- MIDI
  /* Web MIDI: a device in (notes, controls, the clock and transport, into the core as the FM-1's USB takes them: GLO >
   * MIDI's SYIN and IN's NOTE pick USB or TRS there) and one out (what the core sends: GLO > MIDI's clock and notes),
   * stamped ahead by the sound's latency */
  const S_SYNI = 2, S_SYNO = 3, S_NOTO = 4, MIDI_KEY = "mono2k.midi";
  let midiAcc = null, midiIn = null, midiOut = null;
  const midiPrefs = () => { try { return JSON.parse(localStorage.getItem(MIDI_KEY) || "{}"); } catch (e) { return {}; } };
  const midiKeep = (p) => { try { localStorage.setItem(MIDI_KEY, JSON.stringify({ ...midiPrefs(), ...p })); } catch (e) {} };
  function midiSend(list) {
    if (!midiOut) return;
    const at = performance.now() + ((ctx && (ctx.baseLatency || 0) + (ctx.outputLatency || 0)) || 0) * 1000;
    for (const b of list) { try { midiOut.send(b, at); } catch (e) { /* a port gone */ } }
  }
  function midiBind() {
    if (midiIn) midiIn.onmidimessage = null;
    midiIn = midiAcc ? midiAcc.inputs.get($("midi-in").value) || null : null;
    midiOut = midiAcc ? midiAcc.outputs.get($("midi-out").value) || null : null;
    if (midiIn) midiIn.onmidimessage = (ev) => {
      const d = ev.data;
      if (d.length && d[0] !== 0xF0) send({ t: "midi", s: d[0], a: d[1] || 0, b: d[2] || 0 });
    };
    midiKeep({ in: $("midi-in").value, out: $("midi-out").value });
  }
  function midiFill() {
    const pr = midiPrefs(), opt = (p) => { const o = document.createElement("option"); o.value = p.id; o.textContent = p.name; return o; };
    for (const [id, ports, cur] of [["midi-in", midiAcc.inputs, pr.in], ["midi-out", midiAcc.outputs, pr.out]]) {
      const sel = $(id), none = document.createElement("option");
      none.value = ""; none.textContent = "None";
      sel.replaceChildren(none, ...[...ports.values()].map(opt));
      sel.value = cur && ports.has(cur) ? cur : "";
      sel.disabled = false;
    }
    midiBind();
  }
  function midiSync(id, i, on) {
    $(id).setAttribute("aria-pressed", on ? "true" : "false");
    send({ t: "set", i, v: on ? 127 : 0 });
  }
  $("midi-on").addEventListener("click", async () => {
    if (!navigator.requestMIDIAccess) { $("midi-note").textContent = "No Web MIDI here: try Chrome, Edge or Firefox."; return; }
    try { midiAcc = await navigator.requestMIDIAccess({ sysex: false }); }
    catch (e) { $("midi-note").textContent = "MIDI not allowed: allow it, then Connect again."; return; }
    midiAcc.onstatechange = midiFill;
    midiFill();
    $("midi-on").textContent = "On";
    $("midi-on").setAttribute("aria-pressed", "true");
  });
  $("midi-in").addEventListener("change", midiBind);
  $("midi-out").addEventListener("change", midiBind);
  for (const [id, i] of [["midi-syni", S_SYNI], ["midi-syno", S_SYNO], ["midi-noto", S_NOTO]])   /* kept with the store */
    $(id).addEventListener("click", () => midiSync(id, i, $(id).getAttribute("aria-pressed") !== "true"));
  /* the toggles as the core has them (GLO > MIDI on the panel moves them too) */
  function midiShow(I) {
    $("midi-syni").setAttribute("aria-pressed", I[24] ? "true" : "false");
    $("midi-syno").setAttribute("aria-pressed", I[25] ? "true" : "false");
    $("midi-noto").setAttribute("aria-pressed", I[26] ? "true" : "false");
  }
  /* the display is set against the page: a light page, a dark screen, and the other way round */
  function displayMode() {
    const v = isDark() ? 0 : 127;
    if (pre) pre.w_setting(0, v);
    send({ t: "set", i: 0, v });
  }

  // ---------------------------------------------------------------------------------------- the status line
  let machineNames = [], machineOrder = [];      /* names by number; the numbers in ALGORITHM's order */
  let patNames = [], projNames = [];
  const label = (n) => "ABCD"[n >> 4] + String((n & 15) + 1).padStart(2, "0");
  function namesFrom(x) {
    const nP = x.w_patterns(), nJ = x.w_projects();
    patNames = Array.from({ length: nP }, (_, i) => cstr(x, x.w_pat_name(i)));
    projNames = Array.from({ length: nJ }, (_, i) => cstr(x, x.w_project_name(i)));
  }
  /* the line under the panel names what the pointer (or the last computer key) is over and what it does there, from the
   * core's state: the page and its knobs, the keys' mode, the track, the transport (help.js has the words) */
  const statusEl = $("status");
  let statusOn = "", statusText = "", I0 = null;
  const say = (s) => {                             /* each new line scans in (style.css: .status.flip) */
    if (s === statusText) return;
    statusText = s;
    statusEl.textContent = s;
    statusEl.classList.remove("flip"); void statusEl.offsetWidth; statusEl.classList.add("flip");
  };
  function ctxOf(I) {
    const c = { page: 0, sub: 0, trk: 0, mach: "", grid: 0, pads: 0, rec: 0, playing: 0, ready: 0, spage: 0, oct: 0, pat: "A01", label: "HOME", knobs: [] };
    if (!I) return c;
    Object.assign(c, { page: I[1], sub: I[2], trk: I[0], mach: machineNames[I[6]] || "", grid: I[7], rec: I[8], playing: I[4],
      pads: I[27], ready: I[29], spage: I[30], oct: (I[14] << 24) >> 24, pat: label(I[3]) });
    if (pre && pre.w_page_label) {
      c.label = cstr(pre, pre.w_page_label(I[1], I[2]));
      c.knobs = [0, 1, 2, 3].map((k) => ({ s4: cstr(pre, pre.w_knob(I[1], I[2], I[6], k)), name: cstr(pre, pre.w_kname(I[1], I[2], I[6], k)) }));
    }
    return c;
  }
  function statusShow() {
    if (statusOn === "panel") { say("The panel has the keys: Z… play, Space PLAY; Esc lets go."); return; }
    if (!statusOn) {
      say(running ? "Point at a control: this line says what it does." : "Press Start, then point at a control: this line says what it does.");
      return;
    }
    say(window.M2K_HELP.line(statusOn, ctxOf(I0)));
  }
  const statusAt = (id) => { if (id !== statusOn) { statusOn = id; statusShow(); } };

  let lastInfo = "", knobKey = "";
  function info(I) {
    const key = Array.prototype.join.call(I, ",");
    if (key === lastInfo) return;
    lastInfo = key;
    I0 = I.slice ? I.slice() : Array.from(I);
    midiShow(I);
    /* the four knobs are named for what they turn on this page, as the screen's knob row names them */
    const kk = I[1] + "," + I[2] + "," + I[6];
    if (kk !== knobKey && pre && pre.w_knob && encEl.length === 8) {
      knobKey = kk;
      for (let k = 0; k < 4; k++) encEl[3 + k].role.textContent = cstr(pre, pre.w_knob(I[1], I[2], I[6], k)) || "—";
    }
    if (statusOn) statusShow();
  }

  // ------------------------------------------------------------------------------------------------- the bar
  /* the sound on and off (the context suspended: the instrument pauses, its screen holds), MIDI, the computer's keys,
   * and the demos back: each a drawer under the bar, one open at a time */
  function sndShow() {
    const on = running && ctx && ctx.state === "running";
    $("snd").setAttribute("aria-pressed", String(!!on));
    $("snd").textContent = on ? "Sound on" : "Sound off";
  }
  $("snd").addEventListener("click", async () => {
    if (!running) { await start(); return; }
    if (ctx.state === "running") await ctx.suspend(); else await ctx.resume();
    sndShow();
  });
  const drawers = [["midi-tog", "midi"], ["keys-tog", "keymap"], ["reset", "reset-ask"]];
  function drawer(id) {
    for (const [b, d] of drawers) {
      const open = d === id && $(d).hidden;
      $(d).hidden = !open;
      $(b).setAttribute("aria-expanded", String(open));
    }
  }
  for (const [b, d] of drawers) $(b).addEventListener("click", () => drawer(d));
  $("reset-no").addEventListener("click", () => { drawer(""); $("reset").focus(); });
  $("reset-yes").addEventListener("click", () => {
    try { localStorage.removeItem(STORE_KEY); } catch (e) { /* private mode: nothing kept anyway */ }
    if (node) send({ t: "reset", seed: (Math.random() * 4294967295) >>> 0 });
    if (pre) { pre.w_init(2000); pre.w_setting(0, isDark() ? 0 : 127); namesFrom(pre); }
    displayMode();
    drawer("");
    say("The factory's demos are back; this browser's saves are gone.");
    $("reset").focus();
  });
  document.addEventListener("keydown", (ev) => {           /* Esc closes an open drawer (when the panel has no keys) */
    if (ev.code !== "Escape" || armed) return;
    const open = drawers.find(([, d]) => !$(d).hidden);
    if (open) { drawer(""); $(open[0]).focus(); }
  });

  // ---------------------------------------------------------------------------------------- machine names
  function machines() {
    const x = pre, n = x.w_machines();
    machineNames = Array.from({ length: n }, (_, m) => cstr(x, x.w_mi(m, 0)));   /* by number, as patterns keep them */
    machineOrder = Array.from({ length: n }, (_, k) => (x.w_morder ? x.w_morder(k) : k));   /* (an older m2k.wasm: by number) */
  }
  // ---------------------------------------------------------------------------- every parameter, page by page
  /* read off the firmware's own pages, so the list is the panel's: a block for each page (or subpage where it has a
   * name of its own), each knob's short name and its long one; SYN's for the machine chosen */
  function params() {
    const x = pre, grid = $("pgrid");
    if (!x.w_pages) return;
    const P = { HOME: 0, SYN: 1, AMP: 2, FILT: 3, FX: 4, LFO: 5, ARP: 6, GLO: 7 };
    const knobs = (page, subs, m) => subs.flatMap((sub) => [0, 1, 2, 3].map((k) => ({ s4: cstr(x, x.w_knob(page, sub, m, k)), name: cstr(x, x.w_kname(page, sub, m, k)), code: x.w_kcode(page, sub, k) })))
      .filter((r) => r.name);
    const blocks = [
      ["SYN", P.SYN, [0, 1]], ["AMP", P.AMP, [0, 1]], ["FILTER", P.FILT, [0, 1]],
      ...[0, 1, 2, 3].map((sub) => [cstr(x, x.w_page_label(P.FX, sub)), P.FX, [sub]]),
      ...[0, 1, 2].map((l) => [`LFO ${l + 1}`, P.LFO, [2 * l, 2 * l + 1]]),
      ["ARP", P.ARP, [0]], ["HOME", P.HOME, [0]],
      ...[0, 1, 2, 3].map((sub) => [cstr(x, x.w_page_label(P.GLO, sub)), P.GLO, [sub]]),
    ];
    const li = (r) => `<li><b>${r.s4}</b>${r.name}</li>`;
    grid.innerHTML = blocks.map(([t, page, subs]) => page === P.SYN
      ? `<section class="pblk"><h5>${t}<select id="psyn" aria-label="Machine">${machineOrder.map((m) => `<option value="${m}">${machineNames[m]}</option>`).join("")}</select></h5><ol id="psynl">${knobs(page, subs, 0).map(li).join("")}</ol></section>`
      : `<section class="pblk"><h5>${t}</h5><ol>${knobs(page, subs, 0).map(li).join("")}</ol></section>`).join("");
    $("psyn").addEventListener("change", (e) => { $("psynl").innerHTML = knobs(P.SYN, [0, 1], +e.target.value).map(li).join(""); });
    const codes = new Set(blocks.flatMap(([, page, subs]) => knobs(page, subs, 0).map((r) => r.code)));
    $("pcount").textContent = `Eight pages, ${codes.size} controls`;
  }

  // ------------------------------------------------------------------------------------------------- input
  const held = new Set();
  function ctlFrom(target) {
    const g = target.closest && target.closest("[data-c]");
    return g ? g.getAttribute("data-c") : null;
  }
  function press(c, d) {
    if (!running) return;
    const kind = c[0], i = +c.slice(1);
    const g = svg.querySelector(`[data-c="${c}"]`);   /* the key or button goes down, as on the FM-1 (style.css) */
    if (g) g.classList.toggle("down", !!d);
    if (kind === "k") send({ t: "key", k: i, d });
    else if (kind === "b") send({ t: "btn", b: i, d });
  }
  function turn(i, d) {
    if (!running || !d) return;
    send({ t: "enc", e: i, d });
    turnVisual(i, d);
  }
  /* the status line follows the pointer over the controls and the screen, and the computer's keys */
  svg.addEventListener("pointerover", (ev) => statusAt(ctlFrom(ev.target) || ""));
  svg.addEventListener("pointerleave", () => statusAt(""));
  lcd.addEventListener("pointerenter", () => statusAt("lcd"));
  lcd.addEventListener("pointerleave", () => statusAt(""));
  svg.addEventListener("focus", () => { if (running) armed = true; statusAt(running ? "panel" : ""); });   /* (Tab to it: the keys too) */
  svg.addEventListener("blur", () => { if (statusOn === "panel") statusAt(""); });
  const pointers = new Map();
  svg.addEventListener("pointerdown", (ev) => {
    if (!running) { start(); return; }
    const c = ctlFrom(ev.target);
    if (!c) return;
    ev.preventDefault();
    svg.setPointerCapture(ev.pointerId);
    if (c[0] === "e") {
      const i = +c.slice(1);
      if (i >= 3 && i <= 6) { selKnob = i; showSel(); }
      pointers.set(ev.pointerId, { c, y: ev.clientY, acc: 0 });
      return;
    }
    pointers.set(ev.pointerId, { c });
    press(c, 1);
  });
  const up = (ev) => {
    const p = pointers.get(ev.pointerId);
    if (!p) return;
    pointers.delete(ev.pointerId);
    if (p.c[0] !== "e") press(p.c, 0);
  };
  svg.addEventListener("pointerup", up);
  svg.addEventListener("pointercancel", up);
  svg.addEventListener("pointermove", (ev) => {
    const p = pointers.get(ev.pointerId);
    if (!p || p.c[0] !== "e") return;
    p.acc += p.y - ev.clientY;
    p.y = ev.clientY;
    const k = svg.getBoundingClientRect().width / 876;
    const step = 7 * k;
    while (Math.abs(p.acc) >= step) { const d = p.acc > 0 ? 1 : -1; p.acc -= d * step; turn(+p.c.slice(1), d); }
  });
  let wheelT = 0;
  svg.addEventListener("wheel", (ev) => {
    const c = ctlFrom(ev.target);
    if (!c || c[0] !== "e" || !running) return;
    ev.preventDefault();
    const now = performance.now();
    if (now - wheelT < 28) return;
    wheelT = now;
    turn(+c.slice(1), ev.deltaY < 0 ? 1 : -1);
  }, { passive: false });

  /* the computer keys play the panel from Start, or from a click on it, until Esc or a click elsewhere */
  let armed = false;
  document.addEventListener("pointerdown", (ev) => { armed = running && !!(ev.target.closest && ev.target.closest("#panelwrap")); }, true);
  document.addEventListener("keydown", (ev) => {
    if (!running || !armed || ev.metaKey || ev.ctrlKey) return;
    if (ev.code === "Escape") { armed = false; svg.blur(); return; }
    if (ev.code === "Tab") { ev.preventDefault(); selKnob = selKnob >= 6 ? 3 : selKnob + 1; showSel(); statusAt("e" + selKnob); return; }
    if (ev.code.startsWith("Arrow")) {
      ev.preventDefault();
      statusAt("e" + selKnob);
      const d = (ev.code === "ArrowUp" || ev.code === "ArrowRight" ? 1 : -1) * (ev.shiftKey ? 8 : 1);
      turn(selKnob, d);
      return;
    }
    const m = KEYMAP[ev.code];
    if (!m) return;
    ev.preventDefault();
    statusAt(m[0] + m[1]);
    if (m[0] === "e") { turn(m[1], m[2] * (ev.shiftKey ? 4 : 1)); return; }
    if (ev.repeat || held.has(ev.code)) return;
    held.add(ev.code);
    press(m[0] + m[1], 1);
  });
  document.addEventListener("keyup", (ev) => {
    const m = KEYMAP[ev.code];
    if (!m || !held.has(ev.code)) return;
    held.delete(ev.code);
    if (m[0] !== "e") press(m[0] + m[1], 0);
  });
  window.addEventListener("blur", () => { for (const c of held) { const m = KEYMAP[c]; if (m[0] !== "e") press(m[0] + m[1], 0); } held.clear(); });


  /* the theme: Auto (the system's), Light, Dark; remembered */
  const THEMES = ["auto", "light", "dark"];
  let theme = "auto";
  try { theme = localStorage.getItem("mono2k.theme") || "auto"; } catch (e) { theme = "auto"; }
  const themeLabel = () => { $("theme").textContent = theme === "auto" ? "Auto" : theme === "light" ? "Light" : "Dark"; };
  function applyTheme() {
    if (theme === "auto") document.documentElement.removeAttribute("data-theme");
    else document.documentElement.setAttribute("data-theme", theme);
    try { if (theme === "auto") localStorage.removeItem("mono2k.theme"); else localStorage.setItem("mono2k.theme", theme); } catch (e) { /* private mode */ }
    themeLabel();
    displayMode();
    redrawTheme();
  }
  $("theme").addEventListener("click", () => { theme = THEMES[(THEMES.indexOf(theme) + 1) % THEMES.length]; applyTheme(); });
  $("start").addEventListener("click", start);

  // ----------------------------------------------------------------------------------------------- go
  textures();
  buildPanel();
  setPalette(pal);
  /* the page's other drawings (the guide, the machines, the figures) redraw on "m2k-theme", as on the subpages */
  function redrawTheme() { textures(); buildPanel(); wordmark(); window.dispatchEvent(new Event("m2k-theme")); }
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => { if (theme === "auto") applyTheme(); });
  themeLabel();
  let rz = 0;
  window.addEventListener("resize", () => { clearTimeout(rz); rz = setTimeout(wordmark, 120); });
  /* the wordmark is drawn in Orbitron: wait for it, not just for the page's fonts */
  (document.fonts ? Promise.all([document.fonts.load('900 64px "Orbitron"'), document.fonts.ready]) : Promise.resolve())
    .then(() => { wordmark(); requestAnimationFrame(wmTick); });
  reveal();
  boot().catch((e) => status("Could not load the firmware: " + e.message));

  /* scroll: each part arrives once, as it comes into view, in steps as the screen moves; nothing moves for those
   * who ask for less motion, and without the observer everything is simply there */
  function reveal() {
    if (!("IntersectionObserver" in window) || matchMedia("(prefers-reduced-motion: reduce)").matches) return;
    document.documentElement.classList.add("reveal");
    const io = new IntersectionObserver((es) => {
      for (const e of es) if (e.isIntersecting) {
        const t = e.target;
        t.classList.add("in");
        io.unobserve(t);
        if (t.matches(".wipe, .scan")) setTimeout(() => t.classList.add("done"), 1600);   /* its mask off once it has arrived */
      }
    }, { rootMargin: "0px 0px -10% 0px", threshold: 0 });
    const watch = (sel, cls) => document.querySelectorAll(sel).forEach((el) => {
      if (cls) el.classList.add(cls);
      const sib = el.parentElement ? [...el.parentElement.children].filter((c) => c.matches(sel)) : [];
      el.style.setProperty("--i", String(Math.max(0, sib.indexOf(el))));
      io.observe(el);
    });
    watch(".sec");
    watch(".sec__n, .hero__sub > *, .sec__body .lede, .pts > li, .todo__list > li, .made__steps > li, .made__facts > li, .gaps dt", "rv-up");   /* not the foot: at the page's end it never scrolls far enough in */
    watch(".sec__body .display", "wipe");
    watch(".wipe, .scan");                       /* blocks wipe in through the dither, screens scan in */
  }

  /* the FM-1 lands as it is scrolled to: tilted back on the page's plane, it rights itself by the time it is in full
   * view (a transform only; nothing for those who ask for less motion) */
  const landEl = $("land");
  if (landEl && motion) {
    let pend = false;
    const land = () => {
      pend = false;
      const r = landEl.getBoundingClientRect(), vh = window.innerHeight || 800;
      const p = Math.min(1, Math.max(0, (vh - r.top) / (vh * 0.75)));
      const t = 1 - p * p * (3 - 2 * p);           /* smooth, flat at both ends */
      landEl.style.setProperty("--tilt", t < 0.002 ? "0" : t.toFixed(3));
      landEl.classList.toggle("flat", t < 0.002);
    };
    window.addEventListener("scroll", () => { if (!pend) { pend = true; requestAnimationFrame(land); } }, { passive: true });
    window.addEventListener("resize", land);
    land();
  }
  /* the wordmark answers the pointer: passing over it crushes its dither a little, as a hit of the music does */
  wm.addEventListener("pointermove", () => { if (motion && performance.now() - kickT > 180) { kickT = performance.now(); kickAmp = 0.45; } });
  /* an accordion a person opens deals its contents in (style.css, gl-deal); one opened by the page does not */
  document.addEventListener("click", (e) => { const s = e.target.closest && e.target.closest("details > summary"); if (s) s.parentElement.classList.add("gl-dealt"); });
})();
