// SPDX-License-Identifier: GPL-3.0-only
// The FM-1's own screen, away from the emulator: copies of the firmware's core (m2k.wasm, compiled once by app.js),
// each driven by a short script of panel actions and drawn as the emulator draws its screen (set against the page: a
// dark screen on a light page). The guide, the machines and the sections' figures use it; none of them touches the
// core that plays.
window.M2K_SCREENS = (() => {
  "use strict";
  const B = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTD: 12, OCTU: 13 };
  const H = window.M2K_HELP;
  const DARK = [0x0b, 0x0b, 0x0b], LIGHT = [0xf2, 0xf1, 0xec];      /* the emulator's "Paper" palette */
  const pageDark = () => { const t = document.documentElement.getAttribute("data-theme"); return t ? t === "dark" : matchMedia("(prefers-color-scheme: dark)").matches; };
  const motion = () => !matchMedia("(prefers-reduced-motion: reduce)").matches;
  async function instance() {
    const mod = await window.M2K_CORE.module;
    const x = (await WebAssembly.instantiate(mod, {})).exports;
    if (x._initialize) x._initialize();
    return x;
  }
  /* a 1-bit frame onto a 240 x 240 canvas, in the screen's two colours */
  function draw(cv, fb) {
    const g = cv.getContext("2d");
    if (!cv._img) cv._img = g.createImageData(240, 240);
    const d = cv._img.data;
    for (let i = 0, p = 0; i < 7200; i++) {
      const b = fb[i];
      for (let j = 7; j >= 0; j--, p += 4) {
        const c = (b >> j) & 1 ? DARK : LIGHT;
        d[p] = c[0]; d[p + 1] = c[1]; d[p + 2] = c[2]; d[p + 3] = 255;
      }
    }
    g.putImageData(cv._img, 0, 0);
  }
  /* a core of its own that plays a script from the factory's A01 and then runs on in time (its sound thrown away):
   * "b X" taps a button, "B X" holds it, "u X" lets go, "w ms" waits, "e knob d" turns a knob, "kd i" / "Kd i" holds
   * a white / black key, "k i" / "K i" taps one */
  class Player {
    constructor(x) { this.x = x; this.now = 0; this.acc = 0; }
    step(ms) {                                     /* time passes: the sound rendered, the screen drawn */
      const x = this.x;
      this.acc += ms * 44.1 / 128;
      while (this.acc >= 1) { x.w_render(128); this.acc -= 1; }
      this.now += ms;
      x.w_frame(this.now >>> 0);
    }
    wait(ms) { for (let t = 0; t < ms; t += 33) this.step(Math.min(33, ms - t)); }
    play(script, seed) {
      const x = this.x;
      x.w_init(seed || 4242);
      x.w_setting(0, pageDark() ? 0 : 127);
      this.now = 1; this.acc = 0;
      x.w_frame(1);
      this.now = 2100;                             /* past the boot's splash */
      x.w_frame(this.now);
      for (const line of (script || "").split(";").map((s) => s.trim()).filter(Boolean)) {
        const [op, a, c] = line.split(/\s+/);
        if (op === "b") { x.w_button(B[a], 1); this.wait(60); x.w_button(B[a], 0); }
        else if (op === "B") x.w_button(B[a], 1);
        else if (op === "u") x.w_button(B[a], 0);
        else if (op === "w") this.wait(+a);
        else if (op === "e") x.w_encoder(+a, +c);
        else if (op === "k" || op === "K") { const k = (op === "k" ? H.WHITE : H.BLACK)[+a]; x.w_key(k, 1); this.wait(60); x.w_key(k, 0); }
        else if (op === "kd") x.w_key(H.WHITE[+a], 1);
        else if (op === "Kd") x.w_key(H.BLACK[+a], 1);
      }
      return this.frame();
    }
    frame() { return new Uint8Array(this.x.memory.buffer, this.x.w_frame(this.now >>> 0), 7200); }
    theme() { this.x.w_setting(0, pageDark() ? 0 : 127); }
  }
  async function player() { return new Player(await instance()); }

  /* the sections' figures: <canvas data-screen="script">, played once in view and kept running while seen */
  async function figures() {
    const cvs = [...document.querySelectorAll("canvas[data-screen]")];
    if (!cvs.length) return;
    const live = new Set();
    for (const cv of cvs) {
      cv._p = await player();
      cv._ready = false;
    }
    const start = (cv) => { if (!cv._ready) { draw(cv, cv._p.play(cv.dataset.screen)); cv._ready = true; } };
    const io = "IntersectionObserver" in window ? new IntersectionObserver((es) => {
      for (const e of es) { if (e.isIntersecting) { start(e.target); live.add(e.target); } else live.delete(e.target); }
    }, { rootMargin: "100px" }) : null;
    cvs.forEach((cv) => (io ? io.observe(cv) : start(cv)));
    let last = performance.now();
    const tick = (t) => {
      const dt = Math.min(100, t - last);
      last = t;
      if (motion() && !document.hidden) for (const cv of live) { cv._p.step(dt); draw(cv, cv._p.frame()); }
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
    window.addEventListener("m2k-theme", () => { for (const cv of cvs) if (cv._ready) { cv._p.theme(); cv._p.step(1); draw(cv, cv._p.frame()); } });
  }
  figures().catch((e) => console.info("MONO2K: the figures' screens:", e.message));
  return { instance, player, draw, pageDark, motion, B };
})();
