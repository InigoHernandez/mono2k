// SPDX-License-Identifier: GPL-3.0-only
/* MONO2K on the Move, playable in the browser: an original flat drawing of the Move's layout (its controls where the
 * device has them) running MONO2K's Move build, the same controller, screen, LEDs and samples as on the Move.
 *
 * Embed (the Move home is web/move/index.html, beside this file):
 *   <div id="move-gear"></div>
 *   <script src="gear.js?v=6"></script>
 *   <script>MoveGear.init(document.getElementById("move-gear"));</script>
 * MoveGear.init(el, opts) builds the drawing into el and returns { start, mute }. opts (all optional):
 *   bar: false       no line under the drawing (the control under the pointer, the keys, the sound toggle)
 *   keys: false      no computer keyboard
 * It loads, from this script's folder and with their own ?v=: gear.css, gear-worklet.js (the AudioWorklet) and
 * m2k_move.wasm (web/build.sh: web/move/m2k_move_web.c on the Move's glue, move/m2k_move_glue.c).
 * The sound starts on the first press (a browser's rule); before it the screen shows the power-on frame. Nothing is
 * saved: each visit opens bank A. MONO2K plays on its own clock here (INT): Play just plays.
 *
 * Mouse and touch: pads and steps press and hold (a pad's velocity from where it is pressed, higher louder); knobs
 * drag up and down or scroll, a hover is a touch; the jog turns by drag or scroll, a click presses it; Shift, Copy and
 * Delete latch on a click (click again to let go), so one pointer can combine. Keys: A..K the bottom row of pads (the
 * piano's white keys), W..O the row over it, Z / X Down / Up, Space Play, Shift Shift. */
window.MoveGear = (() => {
  "use strict";
  const V = { css: 4, worklet: 1, wasm: 4 };      /* bump with each changed file (the site's ?v= rule) */
  const BASE = (() => {
    const s = document.currentScript && document.currentScript.src;
    return s ? s.replace(/[^/]*$/, "") : "";
  })();
  const NS = "http://www.w3.org/2000/svg";
  const motion = !matchMedia("(prefers-reduced-motion: reduce)").matches;

  /* ------------------------------------------------------------------------------------------- the layout */
  /* one drawing in units (the viewBox): the left column (the screen over it and the track buttons, the jog, Back and
   * Menu, Play and Rec), the track buttons, the 4 x 8 pads, the 2 x 4 function buttons, the knobs and the volume over
   * them, the 16 steps under the pads, the arrows beside the steps */
  const W = 1128, H = 508;                          /* wide and low (the owner's) */
  const LX = 28, LW = 172, TX = 214, TW = 30, PX = 258, PW = 80, PH = 58, PG = 8, FX = 972, FW = 128;   /* pads wider than tall */
  const TY = 24, TH = 124, PY = 166, SY = 458;
  const OLED = { x: 32, y: 34, w: 208, h: 104 };   /* the screen's glass, inside its black bezel */
  const padX = (c) => PX + c * (PW + PG);
  const rowY = (r) => PY + r * (PH + PG);           /* r 0 the top row */
  const rowC = (r) => rowY(r) + PH / 2;
  const KNOB_Y = TY + TH / 2, KNOB_R = 25;
  const JOG = { x: LX + LW / 2, y: (rowC(0) + rowC(1)) / 2, r: 54 };
  const VOL = { x: FX + FW / 2 + 18, y: KNOB_Y, r: 29 };

  /* the Move's buttons: CC, where, its picture; lit: a white LED's brightness from MONO2K (MV_BTN_CC's slot), "rec"
   * and Sample the palette, "play" MONO2K's transport, "shift" this page's own, 0 none (the host's) */
  const BTN_SLOT = { 86: 0, 88: 1, 56: 2, 60: 3, 119: 4, 62: 5, 63: 6, 55: 7, 54: 8, 50: 9, 58: 10, 118: 11, 52: 12 };
  const BUTTONS = [
    { cc: 51, x: JOG.x - 32, y: rowC(2), r: 21, ic: "back", name: "Back" },
    { cc: 50, x: JOG.x + 32, y: rowC(2), r: 21, ic: "menu", name: "Menu · closes a list" },
    { cc: 85, x: JOG.x - 32, y: SY, r: 21, ic: "play", name: "Play · Space" },
    { cc: 86, x: JOG.x + 32, y: SY, r: 21, ic: "rec", name: "Rec · records live" },
    { cc: 52, x: FX + FW * 0.25, y: rowC(0), r: 22, ic: "capture", name: "Capture · what you just played into the steps" },
    { cc: 118, x: FX + FW * 0.75, y: rowC(0), r: 22, ic: "sample", name: "Sample · PADS: the 8 tracks, their mutes above" },
    { cc: 58, x: FX + FW * 0.25, y: rowC(1), r: 22, ic: "loop", name: "Loop · the arp's hold" },
    { cc: 88, x: FX + FW * 0.75, y: rowC(1), r: 22, ic: "mute", name: "Mute · tap: the track · hold: steps mute tracks" },
    { cc: 119, x: FX + FW * 0.25, y: rowC(2), r: 22, ic: "delete", name: "Delete · click to latch, then a step or knob", latch: 1 },
    { cc: 60, x: FX + FW * 0.75, y: rowC(2), r: 22, ic: "copy", name: "Copy · click to latch, then source and target", latch: 1 },
    { cc: 56, x: FX + FW * 0.25, y: rowC(3), r: 22, ic: "undo", name: "Undo · Shift: redo" },
    { cc: 49, x: FX + FW * 0.75, y: rowC(3), r: 22, ic: "shift", name: "Shift · click to latch", latch: 1 },
    { cc: 55, x: FX + FW / 2, y: SY - 17, r: 11.5, ic: "up", name: "Up · octave up · X" },
    { cc: 54, x: FX + FW / 2, y: SY + 17, r: 11.5, ic: "down", name: "Down · octave down · Z" },
    { cc: 62, x: FX + FW / 2 - 31, y: SY, r: 11.5, ic: "left", name: "Left · the bars · Shift: the bank" },
    { cc: 63, x: FX + FW / 2 + 31, y: SY, r: 11.5, ic: "right", name: "Right · the bars · Shift: the bank" },
  ];

  /* MONO2K's pad map (move/screen/m2k_screen.h MS_PAD_PAGE, README's table) and PERFORM's keys (m2k_move_ui.c) */
  const PAGE_PADS = ["HOME", "TRK", "PAT", "GRID", "MIX", "GLO", "SOUNDS", "PERFORM",
                     "SYN", "AMP", "FILT", "EFX", "DSTR", "LFO1", "LFO2", "LFO3"];
  const KEY_NOTE = ["C", "D", "E", "F", "G", "A", "B", "C", "C#", "D#", "", "F#", "G#", "A#", "", ""];
  const PERF_TOP = ["LPF", "DUB", "HPF", "GATE", "CRUSH", "DRIVE", "WOW", "SHIMMER"];
  const PERF_LOW = [["RPT 1/4", "RPT 1/8", "RPT 1/16", "RPT 1/32", "ROLL", "STUTTER", "REVERSE", "STOP"],
                    ["FREEZE", "SCATTER", "OCT DN", "OCT UP", "SPIN", "KILL", "", ""]];

  /* the Move's LED palette (Schwung's constants.mjs, indices 0..127), shown lifted: an LED's low levels glow where a
   * screen's would be black */
  const PAL = ("000000,FF4032,800400,C93C00,AC1F00,8C5018,491804,FADC3B,FFC516,B6FF0E,79FF18,34C216,4F8A04,62FF55,297D53," +
    "269E72,31ADFF,3663FC,1A34FF,1C0CE6,153999,3937FF,5722FF,972BFF,852178,FF1032,FF2BD4,A63421,995628,876700,90821F," +
    "4A8700,007F12,1853B2,624BAD,733A67,F8BCAF,FF9B76,FFBF5F,D9AF71,FFF480,BFBA69,BCCC88,AEFF99,7CDD9F,89B47D,80F3FF," +
    "7ACEFC,68A1D3,858FC2,BBAAF2,CDBBE4,EF8BB0,859D8C,6B756E,84909B,6A7075,88859D,6C6A75,9D859C,746A74,9C9D85,74756A," +
    "9D8484,756A6A,661914,210806,460300,280000,5D1700,200D00,470C00,1C0800,3B2B14,1C130A,250E05,0D0602,645817,201C07," +
    "664E08,211902,486605,172101,306609,0F2103,144D08,061902,1F3701,0A1100,276622,0C210B,143E29,081910,004D36,00180E," +
    "134566,061621,152764,070C20,0A1466,030621,0B045C,03011D,0A1C4C,040B1E,161666,070721,220D66,0B0421,3C1166,130521," +
    "350D30,11040F,660614,210206,661154,21051B,000000,595959,1A1A1A,FFFFFF,595959,CCCCCC,404040,141414,0000FF,00FF00," +
    "FF0000").split(",");
  const LIFT = 0.68;
  const lit = PAL.map((h) => {
    const c = [0, 2, 4].map((i) => parseInt(h.slice(i, i + 2), 16) / 255);
    if (c[0] + c[1] + c[2] === 0) return null;
    return "rgb(" + c.map((v) => Math.round(255 * Math.pow(v, LIFT))).join(",") + ")";
  });
  const white = (b) => (b > 0 ? "rgb(" + Array(3).fill(Math.round(90 + 165 * Math.pow(Math.min(1, b / 124), 0.7))).join(",") + ")" : null);
  const FAM = [lit[3], lit[16], lit[26], "#9a9a9a"];   /* the page's family: the sound orange, the rest azure, functions magenta */

  /* the buttons' pictures, MONO2K's own, on a 16 x 16 grid in 1.5 px strokes */
  const ICON = {
    back: "M7.5 4 3.5 8l4 4M3.5 8H12.5",
    menu: "M3.5 5h9M3.5 8h9M3.5 11h9",
    play: "M5.5 3.8 12.2 8 5.5 12.2Z",
    rec: "M8 3.6a4.4 4.4 0 1 1 0 8.8a4.4 4.4 0 1 1 0-8.8Z",
    capture: "M3.5 6V3.5H6M10 3.5h2.5V6M12.5 10v2.5H10M6 12.5H3.5V10",
    sample: "M2.5 8c1.2-5 2.6-5 3.7 0s2.5 5 3.6 0 2.5-5 3.7 0",
    loop: "M12.4 8.6A4.5 4.5 0 1 1 10.7 4.4M11.2 2.4l-.4 2.4-2.4-.2",
    mute: "M3 6.4h2.4L8.4 4v8L5.4 9.6H3ZM10.6 6.2l3 3.6M13.6 6.2l-3 3.6",
    delete: "M4.5 4.5l7 7M11.5 4.5l-7 7",
    copy: "M3.5 6.5h6v6h-6ZM6.5 6.5v-3h6v6h-3",
    undo: "M6 4.2 3.2 7 6 9.8M3.2 7h6.6a3 3 0 0 1 0 6H7.2",
    shift: "M8 3.2l5 5.2h-2.8v4.4H5.8V8.4H3Z",
    up: "M8 4.2v7.6M4.2 8h7.6",
    down: "M4.2 8h7.6",
    left: "M9.6 4.4 6 8l3.6 3.6",
    right: "M6.4 4.4 10 8l-3.6 3.6",
  };
  const FILLED = { play: 1, rec: 1 };

  const el = (tag, attrs, parent) => {
    const e = document.createElementNS(NS, tag);
    for (const k in attrs) e.setAttribute(k, attrs[k]);
    if (parent) parent.appendChild(e);
    return e;
  };
  const arcPath = (cx, cy, r, a0, a1) => {           /* degrees, 0 up, clockwise */
    const p = (a) => [cx + r * Math.sin(a * Math.PI / 180), cy - r * Math.cos(a * Math.PI / 180)];
    if (Math.abs(a1 - a0) < 0.5) return "";
    const [x0, y0] = p(a0), [x1, y1] = p(a1);
    return `M${x0.toFixed(2)} ${y0.toFixed(2)}A${r} ${r} 0 ${Math.abs(a1 - a0) > 180 ? 1 : 0} ${a1 > a0 ? 1 : 0} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
  };

  /* gear.css, once; resolves when it applies (the drawing waits for it, so nothing is seen, or eased, unstyled) */
  let cssP = null;
  function loadCss() {
    if (cssP) return cssP;
    cssP = new Promise((ok) => {
      const l = document.createElement("link");
      l.rel = "stylesheet";
      l.href = BASE + "gear.css?v=" + V.css;
      l.onload = l.onerror = () => ok();
      setTimeout(ok, 4000);
      document.head.appendChild(l);
    });
    return cssP;
  }

  /* ----------------------------------------------------------------------------------------------- the build */
  function build(root, opts) {
    root.classList.add("mg");
    root.tabIndex = 0;
    root.setAttribute("role", "application");
    root.setAttribute("aria-label", "MONO2K on the Move, playable: pads, steps, knobs and buttons");
    const zoom = document.createElement("canvas");
    zoom.className = "mg__zoom";
    zoom.setAttribute("aria-hidden", "true");
    root.appendChild(zoom);
    const dev = document.createElement("div");
    dev.className = "mg__dev";
    root.appendChild(dev);
    const svg = el("svg", { class: "mg__svg", viewBox: `0 0 ${W} ${H}`, "aria-hidden": "true" }, dev);
    const ctl = {};                                      /* data-c -> its parts */
    const add = (key, g, parts) => { g.setAttribute("data-c", key); ctl[key] = Object.assign({ g }, parts); return g; };

    el("rect", { class: "mg-body", x: 1, y: 1, width: W - 2, height: H - 2, rx: 4 }, svg);
    el("line", { class: "mg-rule", x1: LX, y1: (TY + TH + PY) / 2, x2: W - LX, y2: (TY + TH + PY) / 2 }, svg);

    /* the screen: a black bezel; the glass is a canvas laid over it */
    el("rect", { class: "mg-bezel", x: OLED.x - 6, y: OLED.y - 6, width: OLED.w + 12, height: OLED.h + 12, rx: 1 }, svg);

    /* the knobs, the volume */
    const knob = (key, cx, cy, r) => {
      const g = add(key, el("g", { class: "mg-knob" }, svg), {});
      el("circle", { class: "mg-hit", cx, cy, r: r + 9 }, g);
      el("path", { class: "mg-knob__track", d: arcPath(cx, cy, r + 6, -135, 135) }, g);
      const val = el("path", { class: "mg-knob__val", d: "" }, g);
      el("circle", { class: "mg-knob__ring", cx, cy, r: r + 2.5 }, g);
      const cap = el("g", { class: "mg-knob__cap", style: `transform-origin:${cx}px ${cy}px` }, g);
      el("circle", { class: "mg-knob__body", cx, cy, r }, cap);
      el("line", { class: "mg-knob__mark", x1: cx, y1: cy - r * 0.42, x2: cx, y2: cy - r * 0.86 }, cap);
      ctl[key].val = val; ctl[key].cap = cap; ctl[key].geo = { cx, cy, r };
    };
    for (let k = 0; k < 8; k++) knob("knob:" + k, padX(k) + PW / 2, KNOB_Y, KNOB_R);
    knob("vol", VOL.x, VOL.y, VOL.r);

    /* the jog: one plain circle (the owner's); a turn lights its edge a moment, a press darkens it */
    {
      const g = add("jog", el("g", { class: "mg-jog", style: `transform-origin:${JOG.x}px ${JOG.y}px` }, svg), {});
      ctl.jog.body = el("circle", { class: "mg-jog__body", cx: JOG.x, cy: JOG.y, r: JOG.r }, g);
      ctl.jog.rot = el("g", {}, g);
    }

    /* the buttons */
    for (const b of BUTTONS) {
      const g = add("btn:" + b.cc, el("g", { class: "mg-btn" + (b.inert ? " mg-btn--inert" : "") + (b.latch ? " mg-btn--latch" : ""),
        style: `transform-origin:${b.x}px ${b.y}px` }, svg), { b });
      el("circle", { class: "mg-hit", cx: b.x, cy: b.y, r: b.r + 4 }, g);
      el("circle", { class: "mg-btn__body", cx: b.x, cy: b.y, r: b.r }, g);
      const s = b.r * (b.r > 15 ? 0.95 : 1.15) / 16;
      const ic = el("path", { class: "mg-btn__icon" + (FILLED[b.ic] ? " mg-btn__icon--fill" : ""), d: ICON[b.ic],
        transform: `translate(${b.x - 8 * s} ${b.y - 8 * s}) scale(${s})`, "stroke-width": 1.5 / s * (b.r > 15 ? 1 : 0.85) }, g);
      ctl["btn:" + b.cc].icon = ic;
      if (b.latch) ctl["btn:" + b.cc].dot = el("circle", { class: "mg-btn__latch", cx: b.x + b.r * 0.72, cy: b.y - b.r * 0.72, r: 2.6 }, g);
    }

    /* the track buttons, CC 43 (track 1) on top */
    for (let b = 0; b < 4; b++) {
      const cy = rowC(b);
      const g = add("trk:" + b, el("g", { class: "mg-trk", style: `transform-origin:${TX + TW / 2}px ${cy}px` }, svg), {});
      el("rect", { class: "mg-hit", x: TX - 2, y: cy - 30, width: TW + 4, height: 60 }, g);
      el("rect", { class: "mg-trk__body", x: TX + 4, y: cy - 23, width: TW - 8, height: 46, rx: 11 }, g);
      ctl["trk:" + b].led = el("rect", { class: "mg-trk__led", x: TX + 11, y: cy - 14, width: TW - 22, height: 28, rx: 4 }, g);
    }

    /* the pads, 0 bottom left to 31 top right */
    for (let i = 0; i < 32; i++) {
      const c = i % 8, r = 3 - (i >> 3), x = padX(c), y = rowY(r);
      const g = add("pad:" + i, el("g", { class: "mg-pad", style: `transform-origin:${x + PW / 2}px ${y + PH / 2}px` }, svg), {});
      ctl["pad:" + i].led = el("rect", { class: "mg-pad__face", x, y, width: PW, height: PH, rx: 2 }, g);
      el("rect", { class: "mg-pad__flash", x, y, width: PW, height: PH, rx: 2 }, g);
    }

    /* the steps, round, a beat's tick under every fourth */
    const pitch = (8 * PW + 7 * PG) / 16;
    for (let i = 0; i < 16; i++) {
      const cx = PX + pitch * (i + 0.5);
      const g = add("step:" + i, el("g", { class: "mg-step", style: `transform-origin:${cx}px ${SY}px` }, svg), {});
      el("rect", { class: "mg-hit", x: cx - pitch / 2, y: SY - 16, width: pitch, height: 32 }, g);
      ctl["step:" + i].led = el("rect", { class: "mg-step__face", x: cx - 11, y: SY - 11, width: 22, height: 22, rx: 11 }, g);   /* round (the owner's) */
      if (i % 4 === 0) el("rect", { class: "mg-step__tick", x: cx - 6, y: SY + 15, width: 12, height: 2, rx: 1 }, svg);
    }

    const oled = document.createElement("canvas");
    oled.className = "mg__oled";
    dev.appendChild(oled);

    let bar = null;
    if (opts.bar !== false) {
      bar = document.createElement("div");
      bar.className = "mg__bar";
      bar.innerHTML =
        '<p class="mg__say" aria-live="polite"></p>' +
        '<button type="button" class="mg__snd" aria-pressed="false"><svg viewBox="0 0 16 16" aria-hidden="true">' +
        '<path class="mg__spk" d="M2.5 6h2.6L8.5 3.2v9.6L5.1 10H2.5Z"/><path class="mg__wave" d="M10.6 5.4a3.6 3.6 0 0 1 0 5.2M12.4 3.6a6.1 6.1 0 0 1 0 8.8"/>' +
        '<path class="mg__x" d="M10.5 6.2l3.6 3.6M14.1 6.2l-3.6 3.6"/></svg><span>Sound</span></button>';
      root.appendChild(bar);
    }
    return { svg, ctl, oled, zoom, dev, bar };
  }

  /* -------------------------------------------------------------------------------------------- the engine */
  /* the controls as messages; the sound in gear-worklet.js. Before the first press, and where no worklet runs, a copy
   * of the same wasm on this thread draws the screen (silent) */
  function dispatch(x, c) {
    switch (c.t) {
      case "pad": x.w_pad(c.i, c.v); break;
      case "step": x.w_step(c.i, c.v); break;
      case "knob": x.w_knob(c.i, c.v); break;
      case "touch": x.w_touch(c.i, c.v); break;
      case "jog": x.w_jog(c.v); break;
      case "btn": x.w_button(c.i, c.v); break;
      default: break;
    }
  }
  function localCore(module, seed) {
    const x = new WebAssembly.Instance(module, {}).exports;
    if (x._initialize) x._initialize();
    x.w_init(seed);
    const n = x.w_nleds();
    return {
      send(c) { dispatch(x, c); },
      blocks(k) { for (let i = 0; i < k; i++) x.w_render(128); },
      frame() {
        const m = x.memory.buffer;
        return { fb: new Uint8Array(m, x.w_frame(), 1024).slice(), leds: new Uint8Array(m, x.w_leds(), n).slice(),
          st: new Uint8Array(m, x.w_state(), 48).slice() };
      },
    };
  }

  /* --------------------------------------------------------------------------------------------- the gear */
  function init(root, opts = {}) {
    if (!root) throw new Error("MoveGear.init: no element");
    if (root._moveGear) return root._moveGear;
    root.style.display = "none";                        /* (until gear.css applies: no styles computed before it) */
    loadCss().then(() => { root.style.display = ""; });
    const ui = build(root, opts);
    const { svg, ctl, oled, zoom, dev, bar } = ui;
    const say = bar && bar.querySelector(".mg__say"), sndBtn = bar && bar.querySelector(".mg__snd");
    const seed = (Math.random() * 4294967295) >>> 0;

    let bytes = null, module = null, local = null, ctx = null, node = null, gain = null;
    let state = "idle";                               /* idle, starting, running (worklet), local (silent), failed */
    let pending = [];
    let volume = 0.8, muted = false;
    let last = null, shown = { leds: [], st: null }, visible = true, drawQueued = false;

    const wasmP = fetch(BASE + "m2k_move.wasm?v=" + V.wasm)
      .then((r) => { if (!r.ok) throw new Error("m2k_move.wasm " + r.status); return r.arrayBuffer(); })
      .then((b) => { bytes = b; return WebAssembly.compile(b); })
      .then((m) => { module = m; preview(); return m; })
      .catch((e) => { state = "failed"; status("The playable Move could not load: " + e.message); });

    /* the power-on frame, silent, until the first press */
    function preview() {
      if (state !== "idle" && state !== "starting") return;
      const p = localCore(module, seed);
      p.blocks(4);
      take(p.frame());
    }

    /* a control to the core. Before the sound has started only a press (a gesture: a browser lets audio start in one)
     * starts it, queued until the worklet is ready; a hover or a scroll then is dropped */
    let gesture = false;
    function send(c) {
      if (state === "running") node.port.postMessage(c);
      else if (state === "local") local.send(c);
      else if (state === "starting") pending.push(c);
      else if (state === "idle" && gesture) { pending.push(c); start(); }
    }

    async function start() {
      if (state !== "idle") { if (ctx && ctx.state === "suspended") ctx.resume().catch(() => {}); return; }
      state = "starting";
      root.classList.add("mg--on");
      try {
        const AC = window.AudioContext || window.webkitAudioContext;
        if (!AC) throw new Error("no Web Audio");
        try { ctx = new AC({ sampleRate: 44100, latencyHint: "interactive" }); }
        catch (e) { ctx = new AC({ latencyHint: "interactive" }); }
        const resumed = ctx.resume();                  /* (inside the press: a browser's rule) */
        if (!ctx.audioWorklet) throw new Error("no AudioWorklet");
        await ctx.audioWorklet.addModule(BASE + "gear-worklet.js?v=" + V.worklet);
        await wasmP;
        if (!bytes) throw new Error("no wasm");
        node = new AudioWorkletNode(ctx, "mono2k-move", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
        gain = ctx.createGain();
        gain.gain.value = muted ? 0 : volume;
        node.connect(gain).connect(ctx.destination);
        node.port.onmessage = (ev) => {
          const m = ev.data;
          if (m.t === "frame") take(m);
          else if (m.t === "ready") {
            state = "running";
            const q = pending; pending = [];
            if (q.length) node.port.postMessage({ t: "list", list: q });
            sound();
          }
        };
        node.port.postMessage({ t: "wasm", bytes: bytes.slice(0), seed });
        await resumed;
      } catch (e) {
        /* no sound here: the same core on this thread, silent, so the drawing still plays */
        if (state === "running") return;                /* (the worklet came up after all) */
        if (node) node.port.onmessage = null;
        if (ctx) ctx.close().catch(() => {});
        await wasmP;
        if (!module) return;
        local = localCore(module, seed);
        state = "local";
        const q = pending; pending = [];
        for (const c of q) local.send(c);
        let t0 = performance.now(), due = 0;
        setInterval(() => {
          const now = performance.now();
          due += (now - t0) * 44.1; t0 = now;
          let k = Math.min(64, Math.floor(due / 128)); due -= k * 128;
          local.blocks(k);
          take(local.frame());
        }, 1000 * 1536 / 44100);
        status("No sound in this browser (" + e.message + "): the Move plays silently.");
        sound();
      }
    }

    function sound() {
      if (!sndBtn) return;
      const on = state === "running" && !muted;
      sndBtn.setAttribute("aria-pressed", on ? "true" : "false");
      sndBtn.classList.toggle("is-on", on);
      sndBtn.querySelector("span").textContent = state === "running" ? (muted ? "Muted" : "Sound on") : state === "local" ? "No sound" : "Sound";
    }
    function setGain() {
      if (gain) gain.gain.setTargetAtTime(muted ? 0 : volume, ctx.currentTime, 0.02);
      drawVol();
    }
    if (sndBtn) sndBtn.addEventListener("click", () => {
      if (state === "idle") { muted = false; start(); return; }
      if (state !== "running") return;
      muted = !muted;
      setGain();
      sound();
    });

    /* ------------------------------------------------------------------------------------ the frames shown */
    function take(f) {
      last = f;
      if (!drawQueued && visible && !document.hidden) { drawQueued = true; requestAnimationFrame(draw); }
    }
    document.addEventListener("visibilitychange", () => { if (!document.hidden && last) take(last); });
    if ("IntersectionObserver" in window) {
      new IntersectionObserver((es) => { visible = es[es.length - 1].isIntersecting; if (visible && last) take(last); }).observe(root);
    }

    /* the glass: 128 x 64 drawn a whole number of times over (each pixel a k x k block, k the nearest to the glass's
     * size in the screen's pixels), filling its place with no smoothing: crisp, the blocks within a pixel of even */
    const small = document.createElement("canvas");
    small.width = 128; small.height = 64;
    const sg = small.getContext("2d"), img = sg.createImageData(128, 64);
    let glass = { k: 0, zk: 0 };
    const kOf = (cssW, dpr) => Math.max(1, Math.round(cssW * dpr / 128));
    function fit() {
      const r = dev.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
      if (!r.width) return;
      const s = r.width / W, k = kOf(OLED.w * s, dpr);
      oled.width = 128 * k; oled.height = 64 * k;
      Object.assign(oled.style, { width: OLED.w * s + "px", height: OLED.h * s + "px", left: OLED.x * s + "px", top: OLED.y * s + "px" });
      glass.k = k;
      const narrow = r.width < 560;                  /* a phone: the screen again, large, over the drawing */
      root.classList.toggle("mg--narrow", narrow);
      if (narrow) {
        const zw = Math.min(r.width - 12, 512), zk = kOf(zw, dpr);
        zoom.width = 128 * zk; zoom.height = 64 * zk;
        zoom.style.width = zw + "px"; zoom.style.height = zw / 2 + "px";
        glass.zk = zk;
      } else glass.zk = 0;
      if (last) { shown.fb = null; take(last); }
    }
    if ("ResizeObserver" in window) new ResizeObserver(fit).observe(dev);
    window.addEventListener("resize", fit);

    function drawGlass(fb) {
      const d = img.data;
      for (let i = 0, p = 0; i < 1024; i++) {
        const b = fb[i];
        for (let m = 128; m; m >>= 1, p += 4) {
          const on = b & m;
          d[p] = on ? 244 : 0; d[p + 1] = on ? 246 : 0; d[p + 2] = on ? 240 : 0; d[p + 3] = 255;
        }
      }
      sg.putImageData(img, 0, 0);
      for (const [c, k] of [[oled, glass.k], [zoom, glass.zk]]) {
        if (!k) continue;
        const g = c.getContext("2d");
        g.imageSmoothingEnabled = false;
        g.drawImage(small, 0, 0, 128 * k, 64 * k);
      }
    }

    const setFill = (node_, col, off) => { node_.style.fill = col || off; };
    function draw() {
      drawQueued = false;
      const f = last;
      if (!f) return;
      if (!glass.k) fit();
      if (shown.fb !== f.fb) { drawGlass(f.fb); shown.fb = f.fb; }
      const L = f.leds, P = shown.leds;
      for (let i = 0; i < 16; i++) if (P[i] !== L[i]) { setFill(ctl["step:" + i].led, lit[L[i]], ""); P[i] = L[i]; }
      for (let i = 0; i < 32; i++) {
        const j = 16 + i;
        if (P[j] !== L[j]) { setFill(ctl["pad:" + i].led, lit[L[j]], ""); ctl["pad:" + i].g.classList.toggle("is-lit", !!lit[L[j]]); P[j] = L[j]; }
      }
      for (let b = 0; b < 4; b++) { const j = 48 + b; if (P[j] !== L[j]) { setFill(ctl["trk:" + b].led, lit[L[j]], ""); P[j] = L[j]; } }
      for (const cc in BTN_SLOT) {
        const j = 52 + BTN_SLOT[cc];
        if (P[j] === L[j]) continue;
        P[j] = L[j];
        const ic = ctl["btn:" + cc].icon;
        const col = +cc === 86 || +cc === 118 ? (lit[L[j]] || "") : white(L[j]);
        ic.style.stroke = col || ""; if (ic.classList.contains("mg-btn__icon--fill")) ic.style.fill = col || "";
      }
      const st = f.st, ps = shown.st;
      if (!ps || ps[0] !== st[0]) {                    /* Play: MONO2K's transport (Move keeps its own LED) */
        const ic = ctl["btn:85"].icon, col = st[0] ? lit[11] : white(16);
        ic.style.fill = ic.style.stroke = col;
      }
      drawShift(st[2]);
      for (let k = 0; k < 8; k++) drawKnob(k, st);
      shown.st = st;
    }

    function drawShift(on) {
      const ic = ctl["btn:49"].icon, col = white(on || shift.latch ? 124 : 16);
      ic.style.stroke = col;
      for (const cc of [49, 60, 119]) ctl["btn:" + cc].g.classList.toggle("is-latched", !!latched[cc]);
    }

    /* a knob's ring: its value as the page has it, in the page's colour (a lock white); a free knob none */
    function drawKnob(k, st) {
      const ps = shown.st, kk = st[16 + k], fl = st[24 + k], raw = st[32 + 2 * k] | st[33 + 2 * k] << 8;
      if (ps && ps[16 + k] === kk && ps[24 + k] === fl && (ps[32 + 2 * k] | ps[33 + 2 * k] << 8) === raw && ps[4] === st[4]) return;
      const c = ctl["knob:" + k], { cx, cy, r } = c.geo, free = fl & 1;
      const q = Math.max(0, Math.min(1, raw / (kk === 2 ? 255 : 127)));
      const a = -135 + 270 * q, bip = fl & 4;
      c.val.setAttribute("d", free ? "" : bip ? arcPath(cx, cy, r + 6, Math.min(0, a), Math.max(0, a)) : arcPath(cx, cy, r + 6, -135, a));
      c.val.style.stroke = fl & 2 ? "#f4f4f0" : FAM[st[4]] || FAM[3];
      c.cap.style.transform = `rotate(${free ? 0 : a}deg)`;
      c.g.classList.toggle("is-free", !!free);
    }
    function drawVol() {
      const c = ctl.vol, { cx, cy, r } = c.geo, a = -135 + 270 * (muted ? 0 : volume);
      c.val.setAttribute("d", arcPath(cx, cy, r + 6, -135, a));
      c.cap.style.transform = `rotate(${a}deg)`;
    }
    drawVol();

    /* -------------------------------------------------------------------------------------------- the hands */
    const btnSend = (cc, v) => send({ t: "btn", i: cc, v });
    const latched = {};                                 /* Shift, Copy, Delete held by a click */
    const shift = { ptr: 0, key: 0, latch: 0, sent: 0 };
    function shiftSync() {
      const on = shift.ptr || shift.key || shift.latch ? 1 : 0;
      if (on !== shift.sent) { shift.sent = on; btnSend(49, on ? 127 : 0); }
      drawShift(on);
    }
    const holds = new Map();                            /* pointerId -> what it holds */
    let anyHeld = 0;

    function status(s) { if (say) { say.textContent = s; say.classList.toggle("is-tip", !!s); if (!s) keysHint(); } }
    function keysHint() {
      if (!say || opts.keys === false) { if (say) say.textContent = ""; return; }
      say.innerHTML = '<span class="mg__keys"><kbd>A</kbd>–<kbd>K</kbd> <kbd>W</kbd>–<kbd>O</kbd> the pads · ' +
        "<kbd>Z</kbd><kbd>X</kbd> octave · <kbd>Space</kbd> play · <kbd>Shift</kbd> shift</span>";
    }
    keysHint();
    function nameOf(key) {
      const [kind, a] = key.split(":"), i = +a, st = last && last.st;
      if (kind === "pad") {
        if (i >= 16) return PAGE_PADS[i - 16] + (i - 16 === 6 ? " · the sounds" : i - 16 === 7 ? " · effects on the keys" : " page");
        if (st && st[7]) { const n = i >= 8 ? PERF_TOP[i - 8] : PERF_LOW[st[15] ? 1 : 0][i]; return n ? "PERFORM · " + n : "PERFORM"; }
        return KEY_NOTE[i] ? "Keys · " + KEY_NOTE[i] : "Keys";
      }
      if (kind === "step") return "Step " + (i + 1) + " · a trig · hold it and turn a knob: a lock";
      if (kind === "knob") return "Knob " + (i + 1) + " · drag or scroll";
      if (kind === "trk") return "Track " + (i + 1) + " · Shift: " + (i + 5);
      if (kind === "jog") return "Jog · the patterns · a click loads";
      if (kind === "vol") return "Volume";
      if (kind === "btn") { const b = BUTTONS.find((x) => x.cc === i); return b ? b.name : ""; }
      return "";
    }

    /* each control's own pointer: pads, steps, track buttons, buttons press and release; knobs and the jog drag */
    function down(e) {
      const g = e.target.closest && e.target.closest("[data-c]");
      if (!g || !svg.contains(g)) return;
      e.preventDefault();
      root.classList.add("mg--ptr");                    /* (focused by a press: no keyboard ring) */
      root.focus({ preventScroll: true });
      const key = g.getAttribute("data-c"), [kind, a] = key.split(":"), i = +a;
      const h = { key, kind, i, g, x: e.clientX, y: e.clientY, t: performance.now(), moved: 0, acc: 0 };
      try { g.setPointerCapture(e.pointerId); } catch (err) { /* (a synthetic event) */ }
      holds.set(e.pointerId, h);
      anyHeld++;
      for (const o of holds.values()) if (o !== h) o.used = 1;
      g.classList.add("is-down");
      status(nameOf(key));
      gesture = true;
      try { pressed(h, e); } finally { gesture = false; }
    }

    function pressed(h, e) {
      const { key, kind, i, g } = h;
      if (kind === "pad") {
        const r = ctl[key].led.getBoundingClientRect();
        const v = Math.round(64 + 63 * Math.max(0, Math.min(1, 1 - (e.clientY - r.top) / Math.max(1, r.height))));
        g.style.setProperty("--v", (v / 127).toFixed(2));
        send({ t: "pad", i, v });
      } else if (kind === "step") send({ t: "step", i, v: 1 });
      else if (kind === "trk") btnSend(43 - i, 127);
      else if (kind === "btn") {
        const b = ctl[key].b;
        if (b.inert) return;
        if (b.latch) {
          h.wasLatched = !!latched[i];
          if (!h.wasLatched) { if (i === 49) { shift.ptr = 1; shiftSync(); } else btnSend(i, 127); }
          return;
        }
        btnSend(i, 127);
      } else if (kind === "knob") { touch(i, 1, "drag"); h.kind = "knob"; }
      else if (kind === "jog") {
        const r = svg.getBoundingClientRect(), s = r.width / W;
        h.cx = r.left + JOG.x * s; h.cy = r.top + JOG.y * s;
        h.a = Math.atan2(e.clientY - h.cy, e.clientX - h.cx);
      }
    }

    function move(e) {
      const h = holds.get(e.pointerId);
      if (!h) return;
      const dx = e.clientX - h.x, dy = e.clientY - h.y;
      if (Math.abs(dx) + Math.abs(dy) > 3) h.moved = 1;
      if (h.kind === "knob" || h.key === "vol") {
        const now = performance.now(), dt = Math.max(1, now - (h.lt || h.t));
        const px = h.y - e.clientY;                       /* up turns up */
        h.y = e.clientY; h.x = e.clientX; h.lt = now;
        if (!px) return;
        if (h.key === "vol") { volume = Math.max(0, Math.min(1, volume + px / 220)); if (volume > 0) muted = false; setGain(); sound(); return; }
        turn(h.i, px, Math.abs(px) * 1000 / dt, h);
      } else if (h.kind === "jog") {
        const a = Math.atan2(e.clientY - h.cy, e.clientX - h.cx);
        let d = a - h.a;
        if (d > Math.PI) d -= 2 * Math.PI;
        if (d < -Math.PI) d += 2 * Math.PI;
        if (Math.hypot(e.clientX - h.cx, e.clientY - h.cy) < 6) return;   /* (the middle: no angle) */
        h.a = a;
        h.acc += d * 180 / Math.PI;
        jogBy(h, 15);
      }
    }

    function up(e) {
      const h = holds.get(e.pointerId);
      if (!h) return;
      holds.delete(e.pointerId);
      anyHeld = Math.max(0, anyHeld - 1);
      const { kind, i, g } = h;
      g.classList.remove("is-down");
      if (kind === "pad") send({ t: "pad", i, v: 0 });
      else if (kind === "step") send({ t: "step", i, v: 0 });
      else if (kind === "trk") btnSend(43 - i, 0);
      else if (kind === "btn") {
        const b = ctl[h.key].b;
        if (b.inert) return;
        if (b.latch) {
          const quick = performance.now() - h.t < 450 && !h.used;
          if (h.wasLatched) { latched[i] = 0; release(i); }
          else if (quick && e.type !== "pointercancel") latched[i] = 1;     /* (a click: held until clicked again) */
          else release(i);
          if (i === 49) { shift.latch = latched[49] ? 1 : 0; shift.ptr = 0; shiftSync(); }
          drawShift(shift.sent);
          return;
        }
        btnSend(i, 0);
      } else if (kind === "knob") touch(i, 0, "drag");
      else if (kind === "jog" && !h.moved) { btnSend(3, 127); btnSend(3, 0); jogClick(); }
    }
    function release(cc) { if (cc !== 49) btnSend(cc, 0); }

    /* a knob's touch: hovered or held (either keeps it) */
    const touched = new Array(8).fill(0);
    function touch(k, on, why) {
      if (k === undefined || isNaN(k)) return;
      const was = touched[k] ? 1 : 0;
      touched[k] = on ? touched[k] | (why === "hover" ? 1 : 2) : touched[k] & ~(why === "hover" ? 1 : 2);
      const now = touched[k] ? 1 : 0;
      if (was !== now) send({ t: "touch", i: k, v: now });
      ctl["knob:" + k].g.classList.toggle("is-touch", !!now);
    }

    /* the knob's feel: a range's steps from the drag's pixels by its speed (slow fine, a flick fast), a short list an
     * option a stretch, the tempo (whole BPM here: MONO2K's own clock) as a list a little quicker */
    const kstate = Array.from({ length: 8 }, () => ({ acc: 0, last: 0 }));
    function turn(k, px, speed, h) {
      const st = last && last.st, kk = st ? st[16 + k] : 0;
      if (kk === 3) return;
      const ks = kstate[k], now = performance.now();
      if (now - ks.last > 400 || Math.sign(ks.acc) !== Math.sign(px)) ks.acc = 0.5 * Math.sign(px);   /* (a fresh turn moves at once) */
      ks.last = now;
      const t = Math.max(0, Math.min(1, (speed - 250) / 1250)), g = 0.45 + (1.6 - 0.45) * t * t * (3 - 2 * t);
      ks.acc += px * (kk === 1 ? 1 / 16 : kk === 2 ? g / 5 : g);
      const n = Math.trunc(ks.acc);
      if (!n) return;
      ks.acc -= n;
      send({ t: "knob", i: k, v: n });
      if (h) h.used = 1;
    }
    function jogBy(h, per) {
      while (Math.abs(h.acc) >= per) {
        const d = Math.sign(h.acc);
        h.acc -= d * per;
        send({ t: "jog", v: d });
        jogTurn(d);
      }
    }
    let jogA = 0;
    let jogLit = 0;
    function jogTurn(d) {
      jogA += d * 15;
      ctl.jog.body.classList.add("is-turn");
      clearTimeout(jogLit);
      jogLit = setTimeout(() => ctl.jog.body.classList.remove("is-turn"), 160);
    }
    function jogClick() { const g = ctl.jog.g; g.classList.remove("is-click"); void g.getBBox(); g.classList.add("is-click"); }

    svg.addEventListener("pointerdown", down);
    svg.addEventListener("pointermove", move);
    svg.addEventListener("pointerup", up);
    svg.addEventListener("pointercancel", up);
    svg.addEventListener("contextmenu", (e) => { if (e.target.closest("[data-c]")) e.preventDefault(); });

    /* the hover: a knob touched, the status line names the control */
    let hoverKey = "";
    svg.addEventListener("pointerover", (e) => {
      const g = e.target.closest && e.target.closest("[data-c]");
      const key = g ? g.getAttribute("data-c") : "";
      if (key === hoverKey) return;
      leaveHover(e.pointerType);
      hoverKey = key;
      if (!key) return;
      status(nameOf(key));
      if (e.pointerType === "mouse" && key.startsWith("knob:")) touch(+key.split(":")[1], 1, "hover");
    });
    svg.addEventListener("pointerleave", (e) => { leaveHover(e.pointerType); hoverKey = ""; if (!anyHeld) status(""); });
    function leaveHover() {
      if (hoverKey.startsWith("knob:")) touch(+hoverKey.split(":")[1], 0, "hover");
    }

    /* the wheel: knobs and the volume by their feel, the jog a detent a notch */
    svg.addEventListener("wheel", (e) => {
      const g = e.target.closest && e.target.closest("[data-c]");
      if (!g) return;
      const key = g.getAttribute("data-c");
      if (!(key.startsWith("knob:") || key === "vol" || key === "jog")) return;
      e.preventDefault();
      const dy = e.deltaMode === 1 ? e.deltaY * 33 : e.deltaMode === 2 ? e.deltaY * 400 : e.deltaY;
      if (key === "vol") { volume = Math.max(0, Math.min(1, volume - dy / 1200)); if (volume > 0) muted = false; setGain(); sound(); return; }
      if (state === "idle") return;                     /* (a scroll is no gesture: the first press starts it all) */
      if (key === "jog") {
        const h = wheelJog;
        h.acc += -dy / 50 * 15;
        jogBy(h, 15);
        return;
      }
      const k = +key.split(":")[1];
      touch(k, 1, "hover");
      turn(k, -dy / 4, Math.abs(dy) * 6);
    }, { passive: false });
    const wheelJog = { acc: 0 };

    /* ------------------------------------------------------------------------------------------- the keys */
    const KEYS = { KeyA: 0, KeyS: 1, KeyD: 2, KeyF: 3, KeyG: 4, KeyH: 5, KeyJ: 6, KeyK: 7,
                   KeyW: 8, KeyE: 9, KeyR: 10, KeyT: 11, KeyY: 12, KeyU: 13, KeyI: 14, KeyO: 15 };
    const KBTN = { KeyZ: 54, KeyX: 55, Space: 85 };
    const keysDown = new Map();
    let over = false;
    root.addEventListener("pointerenter", () => { over = true; });
    root.addEventListener("pointerleave", () => { over = false; });
    const armed = () => over || root.contains(document.activeElement);
    function keyDown(e) {
      if (opts.keys === false || e.metaKey || e.ctrlKey || e.altKey) return;
      const t = e.target;
      if (t && t !== root && (t.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(t.tagName))) return;
      if (e.key === "Shift") { if (!armed() || e.repeat) return; shift.key = 1; gesture = true; shiftSync(); gesture = false; return; }
      const pad = KEYS[e.code], cc = KBTN[e.code];
      if (pad === undefined && cc === undefined) return;
      if (!armed()) return;
      e.preventDefault();
      if (e.repeat || keysDown.has(e.code)) return;
      keysDown.set(e.code, 1);
      gesture = true;
      if (pad !== undefined) { send({ t: "pad", i: pad, v: 100 }); ctl["pad:" + pad].g.classList.add("is-down"); ctl["pad:" + pad].g.style.setProperty("--v", "0.79"); }
      else { btnSend(cc, 127); ctl["btn:" + cc].g.classList.add("is-down"); }
      gesture = false;
    }
    function keyUp(e) {
      if (e.key === "Shift" && shift.key) { shift.key = 0; shiftSync(); return; }
      if (!keysDown.has(e.code)) return;
      keysDown.delete(e.code);
      const pad = KEYS[e.code], cc = KBTN[e.code];
      if (pad !== undefined) { send({ t: "pad", i: pad, v: 0 }); ctl["pad:" + pad].g.classList.remove("is-down"); }
      else { btnSend(cc, 0); ctl["btn:" + cc].g.classList.remove("is-down"); }
    }
    function keysOff() {                                /* (the window left with keys down: nothing hangs) */
      for (const code of Array.from(keysDown.keys())) keyUp({ code, key: "" });
      if (shift.key) { shift.key = 0; shiftSync(); }
    }
    document.addEventListener("keydown", keyDown);
    document.addEventListener("keyup", keyUp);
    window.addEventListener("blur", keysOff);
    root.addEventListener("blur", () => root.classList.remove("mg--ptr"));

    requestAnimationFrame(fit);
    const api = { start, mute(on) { muted = on === undefined ? !muted : !!on; setGain(); sound(); } };
    root._moveGear = api;
    return api;
  }

  return { init };
})();
