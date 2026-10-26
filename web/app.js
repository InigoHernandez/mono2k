// SPDX-License-Identifier: GPL-3.0-only
// MONO2K's emulator page: the dithered wordmark, the FM-1 panel in one bit, the screen, the controls (mouse, touch,
// computer keys, MIDI), the live readout, the machine sheet and the saves. The firmware itself runs in worklet.js.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
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
  const status = (s) => console.info("MONO2K:", s);   /* the page keeps no status line: messages go to the console */

  async function boot() {
    const r = await fetch("m2k.wasm?v=84");
    wasmBytes = await r.arrayBuffer();
    pre = new WebAssembly.Instance(new WebAssembly.Module(wasmBytes), {}).exports;
    if (pre._initialize) pre._initialize();
    pre.w_init(2000);
    pre.w_setting(0, isDark() ? 0 : 127);
    preT0 = performance.now();
    machines();
    params();
    namesFrom(pre);
    patterns();
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
          patterns();
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
          if (startPattern >= 0) { send({ t: "pattern", n: startPattern }); startPattern = -1; }
          status(ctx.sampleRate === 44100 ? "Running at 44.1 kHz, as on the FM-1." : `Running at ${ctx.sampleRate} Hz, resampled from 44.1 kHz.`);
        }
      };
      node.port.postMessage({ t: "wasm", bytes: wasmBytes, seed: (Math.random() * 4294967295) >>> 0, store: loadStore() });
      await ctx.resume();
      running = true;
      armed = true;
      svg.focus({ preventScroll: true });   /* the readout can start it too: stay where the reader is */
    } catch (e) {
      status("Could not start the audio: " + e.message);
      $("start").classList.remove("gone");
    }
  }
  const send = (m) => { if (node) node.port.postMessage(m); };

  // ---------------------------------------------------------------------------------------------------- MIDI
  /* Web MIDI: a device in (notes, controls, the clock and transport, into the core as the FM-1's USB and TRS take
   * them) and one out (what the core sends: GLO > MIDI's clock and notes), stamped ahead by the sound's latency */
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
    if (!navigator.requestMIDIAccess) { $("midi-note").textContent = "This browser has no Web MIDI: Chrome, Edge or Firefox."; return; }
    try { midiAcc = await navigator.requestMIDIAccess({ sysex: false }); }
    catch (e) { $("midi-note").textContent = "MIDI was not allowed: allow it in the site's settings and press Connect again."; return; }
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

  // ---------------------------------------------------------------------------------------------- readouts
  let machineNames = [], machineOrder = [];      /* names by number; the numbers in ALGORITHM's order */
  let startPattern = -1, patNames = [], projNames = [], curPat = 0, queuedPat = 0, playingNow = 0, curProj = 0;
  const label = (n) => "ABCD"[n >> 4] + String((n & 15) + 1).padStart(2, "0");
  /* the patterns: the web shows bank A, the demos; the other banks are the FM-1's. While playing, a click queues
   * the pattern for the end of the bar */
  function patterns() {
    const box = $("pats");
    if (!box.children.length) {
      for (let i = 0; i < 16; i++) {
        const b = document.createElement("button");
        b.type = "button";
        b.addEventListener("click", () => {
          if (!running) { startPattern = i; start(); return; }
          send({ t: "pattern", n: i });
        });
        box.appendChild(b);
      }
    }
    [...box.children].forEach((b, n) => {
      b.innerHTML = `<b>${label(n)}</b>${patNames[n] || ""}`;
      b.className = n === curPat ? "on" : n === queuedPat && playingNow ? "q" : "";
    });
  }
  function namesFrom(x) {
    const nP = x.w_patterns(), nJ = x.w_projects();
    patNames = Array.from({ length: nP }, (_, i) => cstr(x, x.w_pat_name(i)));
    projNames = Array.from({ length: nJ }, (_, i) => cstr(x, x.w_project_name(i)));
  }

  const PAGES = ["HOME", "SYN", "AMP", "FILT", "FX", "LFO", "ARP", "GLO"];
  let lastInfo = "", knobKey = "", curTrk = 0;
  function info(I) {
    const key = Array.prototype.join.call(I, ",");
    if (key === lastInfo) return;
    lastInfo = key;
    midiShow(I);
    const trk = I[0], pat = I[3];
    curTrk = trk;
    /* the four knobs are named for what they turn on this page, as the screen's footer names them */
    const kk = I[1] + "," + I[2] + "," + I[6];
    if (kk !== knobKey && pre && pre.w_knob && encEl.length === 8) {
      knobKey = kk;
      for (let k = 0; k < 4; k++) encEl[3 + k].role.textContent = cstr(pre, pre.w_knob(I[1], I[2], I[6], k)) || "—";
    }
    if (readBtn.step) {
      readBtn.step.innerHTML = I[4] ? ICON.stop : ICON.play;
      readBtn.step.setAttribute("aria-label", I[4] ? "Stop" : "Play");
      readBtn.grid.setAttribute("aria-pressed", String(!!I[7]));
      readBtn.rec.setAttribute("aria-pressed", String(!!I[8]));
    }
    $("r-trk").textContent = "T" + (trk + 1);
    $("r-mach").textContent = machineNames[I[6]] || "";
    $("r-pat").textContent = label(pat) + (I[12] !== pat && I[4] ? "›" + label(I[12]) : "");
    $("r-bpm").textContent = String(I[5] + 30);
    $("r-step").textContent = I[4] ? String(I[10] + 1).padStart(2, "0") + "/" + I[11] : "—";
    $("r-mode").textContent = (I[7] ? "GRID" : "PLAY") + (I[9] ? " FILL" : I[8] ? " REC" : "");
    const ol = $("tracks");
    if (ol.children.length !== 6) {
      ol.innerHTML = "";
      for (let t = 0; t < 6; t++) {
        const li = document.createElement("li");
        const sel = ctlButton("", "Track " + (t + 1), () => send({ t: "track", n: t }));
        const mute = ctlButton("M", "Mute track " + (t + 1), () => send({ t: "mute", n: t }));
        sel.className = "tsel"; mute.className = "tmute";
        li.append(sel, mute);
        ol.appendChild(li);
      }
    }
    for (let t = 0; t < 6; t++) {
      const li = ol.children[t], muted = (I[13] >> t) & 1, name = machineNames[I[17 + t]] || "";
      li.className = (t === trk ? "on" : "") + (muted ? " mute" : "");
      li.firstChild.innerHTML = `<b>${t + 1}</b>${name}`;
      li.firstChild.setAttribute("aria-label", `Track ${t + 1}, ${name}`);
      li.firstChild.setAttribute("aria-pressed", String(t === trk));
      li.lastChild.setAttribute("aria-pressed", String(!!muted));
    }
    curPat = pat; queuedPat = I[12]; playingNow = I[4]; curProj = I[23];
    patterns();
    void PAGES;
  }

  /* the readout plays too: arrows on track, machine, pattern and tempo (held, the tempo runs; Shift, by ten);
   * STEP starts and stops; KEYS turns the keys into the grid and arms recording. A track cell selects its track,
   * its M mutes it. Before the sound is on, the first touch turns it on. */
  const readBtn = {};
  /* the controls' icons, drawn on a 10-unit square so each sits in the middle of its button */
  const ICON = {
    left: '<svg viewBox="0 0 10 10" aria-hidden="true"><path d="M7.5 1.5v7L2 5z"/></svg>',
    right: '<svg viewBox="0 0 10 10" aria-hidden="true"><path d="M2.5 1.5v7L8 5z"/></svg>',
    play: '<svg viewBox="0 0 10 10" aria-hidden="true"><path d="M2.5 1v8L9 5z"/></svg>',
    stop: '<svg viewBox="0 0 10 10" aria-hidden="true"><path d="M2 2h6v6H2z"/></svg>',
  };
  const act = (fn) => { if (running) fn(); else start().then(() => { if (running) fn(); }); };
  const tap = (b) => { send({ t: "btn", b, d: 1 }); send({ t: "btn", b, d: 0 }); };
  function ctlButton(txt, label, fn, repeat) {
    const b = document.createElement("button");
    b.type = "button";
    b.textContent = txt;
    b.setAttribute("aria-label", label);
    if (!repeat) { b.addEventListener("click", (ev) => act(() => fn(ev))); return b; }
    let rt = 0, ri = 0;
    const stop = () => { clearTimeout(rt); clearInterval(ri); };
    b.addEventListener("click", (ev) => { if (ev.detail === 0) act(() => fn(ev)); });      /* from the keyboard */
    b.addEventListener("pointerdown", (ev) => {
      const sh = ev.shiftKey;
      act(() => fn({ shiftKey: sh }));
      rt = setTimeout(() => { ri = setInterval(() => fn({ shiftKey: sh }), 70); }, 380);
    });
    for (const e of ["pointerup", "pointerleave", "pointercancel"]) b.addEventListener(e, stop);
    return b;
  }
  function readControls() {
    const box = (id, ...btns) => { const s = document.createElement("span"); s.className = "ctls"; s.append(...btns); $(id).parentElement.appendChild(s); };
    const arrows = (id, name, fn, repeat) => {
      const dn = ctlButton("", "Previous " + name, (ev) => fn(-1, ev), repeat), up = ctlButton("", "Next " + name, (ev) => fn(1, ev), repeat);
      dn.className = up.className = "ar";
      dn.innerHTML = ICON.left; up.innerHTML = ICON.right;
      box(id, dn, up);
    };
    arrows("r-trk", "track", (d) => send({ t: "track", n: (curTrk + 6 + d) % 6 }));
    arrows("r-mach", "machine", (d) => turn(1, d));
    arrows("r-pat", "pattern", (d) => turn(2, d));
    arrows("r-bpm", "tempo", (d, ev) => send({ t: "tempo", d: d * (ev && ev.shiftKey ? 10 : 1) }), true);
    readBtn.step = ctlButton("", "Play", () => tap(10));
    readBtn.step.className = "ar";
    readBtn.step.innerHTML = ICON.play;
    box("r-step", readBtn.step);
    readBtn.grid = ctlButton("Grid", "Keys as steps (GRID)", () => tap(9));
    readBtn.rec = ctlButton("Rec", "Record", () => tap(11));
    box("r-mode", readBtn.grid, readBtn.rec);
  }

  // ---------------------------------------------------------------------------------------- machine sheet
  function machines() {
    const x = pre, n = x.w_machines(), grid = $("mgrid");
    machineNames = Array.from({ length: n }, (_, m) => cstr(x, x.w_mi(m, 0)));   /* by number, as patterns keep them */
    machineOrder = Array.from({ length: n }, (_, k) => (x.w_morder ? x.w_morder(k) : k));   /* (an older m2k.wasm: by number) */
    grid.innerHTML = "";
    for (let k = 0; k < n; k++) {                    /* the cards in ALGORITHM's order */
      const m = machineOrder[k];
      const name = machineNames[m], style = cstr(x, x.w_mi(m, 2)), cls = cstr(x, x.w_mi(m, 3));
      const card = document.createElement("article");
      card.className = "mcard";
      card.style.setProperty("--i", String(k % 3 + (k / 3 | 0)));   /* dealt out in diagonals when scrolled to */
      const st = ["PUNCH", "METAL", "PLASTIC"].map((s, i) => {          /* five segments, as the screen's card */
        const v = x.w_stat(m, i);
        return `<span class="stat"><b>${s}</b><i>${Array.from({ length: 5 }, (_, k) => `<u class="${k < v ? "on" : ""}"></u>`).join("")}</i></span>`;
      }).join("");
      const params = Array.from({ length: 8 }, (_, i) => {
        const s4 = cstr(x, x.w_syn(m, i, 0)), ln = cstr(x, x.w_syn(m, i, 1));
        return `<li><b>${"ABCDEFGH"[i]} ${s4}</b>${ln === "-" ? "" : ln}</li>`;
      }).join("");
      card.innerHTML = `<div><h4>${name}</h4><div class="meta"><span class="tags"><em class="inv">${cls}</em><em>${style}</em></span>${st}</div></div><canvas width="72" height="72"></canvas><ol>${params}</ol>`;
      grid.appendChild(card);
      const cv = card.querySelector("canvas"), g = cv.getContext("2d");
      const draw = (bits) => {
        const gs = getComputedStyle(grid);
        const img = g.createImageData(72, 72), I = hex(gs.getPropertyValue("--ink").trim()), P = hex(gs.getPropertyValue("--paper").trim());
        for (let p = 0; p < 72 * 72; p++) { const o = p * 4; img.data[o] = P[0]; img.data[o + 1] = P[1]; img.data[o + 2] = P[2]; img.data[o + 3] = 255; }
        for (let i = 0; i < 512; i++) for (let j = 0; j < 8; j++) {
          if (!((bits[i] >> (7 - j)) & 1)) continue;
          const x = (i & 7) * 8 + j + 4, y = (i >> 3) + 4, o = (y * 72 + x) * 4;
          img.data[o] = I[0]; img.data[o + 1] = I[1]; img.data[o + 2] = I[2];
        }
        g.putImageData(img, 0, 0);
      };
      const flat = () => draw(new Uint8Array(x.memory.buffer, x.w_mark(m), 512));
      flat();
      let anim = 0, f = 0;
      card.addEventListener("pointerenter", () => {
        clearInterval(anim);
        anim = setInterval(() => draw(new Uint8Array(x.memory.buffer, x.w_icon(m, f++ % 12), 512)), 80);
      });
      card.addEventListener("pointerleave", () => { clearInterval(anim); flat(); });
      card._redraw = flat;
    }
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
    if (kind === "k") send({ t: "key", k: i, d });
    else if (kind === "b") send({ t: "btn", b: i, d });
  }
  function turn(i, d) {
    if (!running || !d) return;
    send({ t: "enc", e: i, d });
    turnVisual(i, d);
  }
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
    if (ev.code === "Tab") { ev.preventDefault(); selKnob = selKnob >= 6 ? 3 : selKnob + 1; showSel(); return; }
    if (ev.code.startsWith("Arrow")) {
      ev.preventDefault();
      const d = (ev.code === "ArrowUp" || ev.code === "ArrowRight" ? 1 : -1) * (ev.shiftKey ? 8 : 1);
      turn(selKnob, d);
      return;
    }
    const m = KEYMAP[ev.code];
    if (!m) return;
    ev.preventDefault();
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
  function redrawTheme() { textures(); buildPanel(); wordmark(); document.querySelectorAll(".mcard").forEach((c) => c._redraw && c._redraw()); }
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => { if (theme === "auto") applyTheme(); });
  themeLabel();
  let rz = 0;
  window.addEventListener("resize", () => { clearTimeout(rz); rz = setTimeout(wordmark, 120); });
  /* the wordmark is drawn in Orbitron: wait for it, not just for the page's fonts */
  (document.fonts ? Promise.all([document.fonts.load('900 64px "Orbitron"'), document.fonts.ready]) : Promise.resolve())
    .then(() => { wordmark(); requestAnimationFrame(wmTick); });
  reveal();
  readControls();
  boot().catch((e) => status("Could not load the firmware: " + e.message));

  /* scroll: each part arrives once, as it comes into view, in steps as the screen moves; nothing moves for those
   * who ask for less motion, and without the observer everything is simply there */
  function reveal() {
    if (!("IntersectionObserver" in window) || matchMedia("(prefers-reduced-motion: reduce)").matches) return;
    document.documentElement.classList.add("reveal");
    const io = new IntersectionObserver((es) => {
      for (const e of es) if (e.isIntersecting) { e.target.classList.add("in"); io.unobserve(e.target); }
    }, { rootMargin: "0px 0px -10% 0px", threshold: 0 });
    const watch = (sel, cls) => document.querySelectorAll(sel).forEach((el) => {
      if (cls) el.classList.add(cls);
      const sib = el.parentElement ? [...el.parentElement.children].filter((c) => c.matches(sel)) : [];
      el.style.setProperty("--i", String(Math.max(0, sib.indexOf(el))));
      io.observe(el);
    });
    watch(".sec, .mgrid");
    watch(".sec__n, .hero__sub > *, .sec__body .lede, .cols2 > p, .keys", "rv-up");   /* not the foot: at the page's end it never scrolls far enough in */
    watch(".sec__body .display", "rv-wipe");
  }
  /* an accordion a person opens deals its contents in (style.css, gl-deal); one opened by the page does not */
  document.addEventListener("click", (e) => { const s = e.target.closest && e.target.closest("details > summary"); if (s) s.parentElement.classList.add("gl-dealt"); });
})();
