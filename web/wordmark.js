// SPDX-License-Identifier: GPL-3.0-only
// The hero's wordmark, on both homes (the FM-1's and the Move's): MONO2K's logotype (panel.js's LOGO) filled across
// the grid at the cells' resolution and dithered, alive. M2K_WORDMARK(canvas) draws it, keeps it drawn (resize, the
// theme), and returns { draw, listen }: draw() again for a new theme, listen(analyser) to kick on the music's hits.
window.M2K_WORDMARK = (wm) => {
  "use strict";
  const css = () => getComputedStyle(document.documentElement);
  const ink = () => css().getPropertyValue("--ink").trim() || "#0b0b0b";
  const paper = () => css().getPropertyValue("--paper").trim() || "#f2f1ec";
  const hex = (c) => { const m = c.replace("#", ""); return [parseInt(m.slice(0, 2), 16), parseInt(m.slice(2, 4), 16), parseInt(m.slice(4, 6), 16)]; };
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
   * a break (rows shifted, slices slid: the letters broken) or a decay (worn down to the outlines); while music plays
   * (an analyser listened to), each hit crushes it a little. Nothing moves the letters themselves. */
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

  let rz = 0;
  window.addEventListener("resize", () => { clearTimeout(rz); rz = setTimeout(wordmark, 120); });
  /* the wordmark is drawn in Orbitron's grid: wait for it, not just for the page's fonts */
  (document.fonts ? Promise.all([document.fonts.load('900 64px "Orbitron"'), document.fonts.ready]) : Promise.resolve())
    .then(() => { wordmark(); requestAnimationFrame(wmTick); });
  /* it answers the pointer: passing over it crushes its dither a little, as a hit of the music does */
  wm.addEventListener("pointermove", () => { if (motion && performance.now() - kickT > 180) { kickT = performance.now(); kickAmp = 0.45; } });
  return { draw: wordmark, listen: (a) => { analyser = a; abuf = null; } };
};
