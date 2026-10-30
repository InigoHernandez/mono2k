// SPDX-License-Identifier: GPL-3.0-only
// The manual: one page at a time (the address's #name), its breadcrumb and its neighbours; the screens drawn in the
// page's ink; the FM-1 drawn from panel.js; and what the firmware itself knows (m2k.wasm, no sound): the machines,
// their marks and knobs, every page's knobs, bank A's names.
(() => {
  "use strict";
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
  const css = () => getComputedStyle(document.documentElement);
  const ink = () => css().getPropertyValue("--ink").trim() || "#0b0b0b";
  const paper = () => css().getPropertyValue("--paper").trim() || "#f2f1ec";
  const hex = (c) => { const m = c.replace("#", ""); return [parseInt(m.slice(0, 2), 16), parseInt(m.slice(2, 4), 16), parseInt(m.slice(4, 6), 16)]; };
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);

  // ------------------------------------------------------------------------------------------- the pages
  const arts = $$(".doc article");
  const side = $("#side");
  document.body.classList.remove("noscript-all");
  arts.forEach((a) => {
    const h = $("h1", a);
    const crumb = document.createElement("p");
    crumb.className = "crumb";
    crumb.innerHTML = `<span>Manual</span><span>${esc(a.dataset.group)}</span><span>${esc(h.textContent)}</span>`;
    a.insertBefore(crumb, h);
  });
  function show(id, scroll) {
    const i = Math.max(0, arts.findIndex((a) => a.id === id));
    arts.forEach((a, j) => a.classList.toggle("on", j === i));
    $$("a", side).forEach((l) => { if (l.getAttribute("href") === "#" + arts[i].id) l.setAttribute("aria-current", "page"); else l.removeAttribute("aria-current"); });
    const pg = $("#pager");
    const link = (a, cls, what) => a ? `<a class="${cls}" href="#${a.id}"><small>${what}</small><b>${esc($("h1", a).textContent)}</b></a>` : "";
    pg.innerHTML = link(arts[i - 1], "prev", "Previous") + link(arts[i + 1], "next", "Next");
    document.title = `${$("h1", arts[i]).textContent} · MONO2K Manual`;
    if (matchMedia("(max-width: 720px)").matches) side.open = false;
    if (scroll) window.scrollTo({ top: 0 });
  }
  /* an address naming a page would also scroll to it, as to an anchor: the page starts at the top instead */
  if ("scrollRestoration" in history) history.scrollRestoration = "manual";
  requestAnimationFrame(() => window.scrollTo({ top: 0 }));
  const fromHash = (scroll) => show(decodeURIComponent(location.hash.slice(1)) || "introduction", scroll);
  window.addEventListener("hashchange", () => fromHash(true));
  fromHash(false);
  const wide = () => { if (!matchMedia("(max-width: 720px)").matches) side.open = true; };
  window.addEventListener("resize", wide);
  wide();

  // ----------------------------------------------------------------------------------------- the screens
  /* each a 1-bit PNG of the firmware's own screen (the tour in host/m2k_host.c), drawn in the page's two colours */
  const shots = new Map();
  function drawScreen(cv) {
    const img = shots.get(cv.dataset.scr);
    if (!img || !img.complete) return;
    cv.width = 240; cv.height = 240;
    const g = cv.getContext("2d");
    g.drawImage(img, 0, 0);
    const d = g.getImageData(0, 0, 240, 240), I = hex(ink()), P = hex(paper());
    for (let p = 0; p < d.data.length; p += 4) {
      const c = d.data[p] < 128 ? I : P;
      d.data[p] = c[0]; d.data[p + 1] = c[1]; d.data[p + 2] = c[2]; d.data[p + 3] = 255;
    }
    g.putImageData(d, 0, 0);
  }
  $$("canvas[data-scr]").forEach((cv) => {
    const n = cv.dataset.scr;
    if (!shots.has(n)) { const im = new Image(); im.src = `manual/${n}.png`; shots.set(n, im); }
    const im = shots.get(n);
    if (im.complete) drawScreen(cv); else im.addEventListener("load", () => drawScreen(cv));
    cv.setAttribute("role", "img");
    cv.setAttribute("aria-label", (cv.parentElement.querySelector("figcaption") || {}).textContent || "MONO2K's screen");
  });

  // ------------------------------------------------------------------------------------------- the FM-1
  /* the panel as lines, from the emulator's own geometry; the screen shows the HOME page */
  const NS = "http://www.w3.org/2000/svg";
  function panelSvg(kind) {
    const P = window.M2K_PANEL;
    const s = document.createElementNS(NS, "svg");
    s.setAttribute("viewBox", "0 0 904 566");
    s.setAttribute("role", "img");
    s.setAttribute("aria-label", "The FM-1's panel: the screen, eight knobs, fourteen buttons and twenty-seven keys, with what MONO2K puts on each");
    const el = (tag, a, p = s) => { const e = document.createElementNS(NS, tag); for (const k in a) e.setAttribute(k, a[k]); p.appendChild(e); return e; };
    const L = { fill: "none", stroke: "currentColor", "stroke-width": 1.5 };
    const t = (x, y, str, size, a = {}) => Object.assign(el("text", { x, y, "font-size": size, "text-anchor": "middle", "font-weight": 700, "letter-spacing": "0.04em", fill: "currentColor", ...a }), { textContent: str });
    el("rect", { x: 22, y: 22, width: 860, height: 522, rx: 38, ...L, "stroke-width": 2 });
    el("rect", { x: 237, y: 49, width: 221, height: 221, rx: 14, ...L });
    if (kind === "home") el("image", { href: "manual/01_home_stopped.png", x: 247, y: 59, width: 201, height: 201, style: "image-rendering: pixelated" });
    else { el("rect", { x: 247, y: 59, width: 201, height: 201, ...L, "stroke-dasharray": "2 3" }); t(347.5, 165, "SCREEN", 11, { opacity: 0.7 }); }
    for (let k = 0; k < 27; k++) {
      const r = P.KEYR[k], cx = r[0] + 21;
      el("rect", { x: r[0], y: r[1], width: r[2], height: r[3], rx: 6, ...L });
      const m = 53 + k;
      if (kind === "labels") {
        t(cx, r[1] + 50, P.NOTE[m % 12] + (Math.floor(m / 12) - 1), 8);
        if (P.isBlack(k)) t(cx, r[1] + 64, P.BLACK_ROLE[P.KEYR.slice(0, k).filter((_, j) => P.isBlack(j)).length], 8, { "font-weight": 900 });
      }
    }
    for (let i = 0; i < 14; i++) {
      const r = P.BTNR[i];
      el("rect", { x: r[0], y: r[1], width: r[2], height: r[3], rx: 3, ...L });
      t(r[0] + r[2] / 2, r[1] + (i >= 12 ? 15 : 22), P.BTN[i], 8.5, { "font-weight": 800 });
      if (kind === "labels" && P.ROLE[i]) t(r[0] + r[2] / 2, r[1] - 5, P.ROLE[i], 7, { "font-weight": 900 });
    }
    for (let i = 0; i < 8; i++) {
      const cx = P.ENC_CX[i], cy = P.ENC_CY[i];
      el("circle", { cx, cy, r: 21, ...L });
      el("circle", { cx, cy, r: 9, ...L, "stroke-width": 1 });
      el("line", { x1: cx, y1: cy - 10, x2: cx, y2: cy - 19, stroke: "currentColor", "stroke-width": 3 });
      t(cx, cy - 37, P.ENC[i], 6.5, { "font-weight": 600 });
      if (kind === "labels") t(cx, cy - 28, P.EROLE[i], 8, { "font-weight": 900 });
    }
    const iso = P.ISO, isoG = el("g", { transform: `translate(58 34) scale(${15 / iso.h})`, fill: "currentColor" });
    for (const [d, rule] of iso.paths) el("path", { d, "fill-rule": rule }, isoG);
    return s;
  }
  $$("[data-panel]").forEach((d) => d.appendChild(panelSvg(d.dataset.panel)));

  // ------------------------------------------------------------------------------------ from the firmware
  const MACH_TEXT = {
    "GND-SIN": "The Monomachine's sine, TUNE on H, and MONO2K's own around it: a wavefold, a sub an octave down, noise, a pitch drop (depth and time) for kicks, a click, a second harmonic. Kicks, subs, bells.",
    "GND-NOIS": "The Monomachine's noise: RED colours it from white through pink to red, ST adds a second noise, STON spreads it across the stereo. Hats, snares' noise, air.",
    "GND-PERC": "MONO2K's own noise percussion: a colour, sample and hold, a metal mix and its spread, a pitch drop and grit. Snares, claps, metal hits.",
    "SID-6581": "After the chip: its pulse width and the width's sweep, a wave (triangle, saw, pulse, their mix, noise), and ring, sync or both from a frequency of its own or the pitch of the track before. Chip leads and basses.",
    "FM+STAT": "Two operators on the Monomachine's page: each ratio on its 24 steps, the first with a fine tune, a depth or a depth falling from the trig, and feedback; the second's depth; a tone. Bells, keys, glass.",
    "FM+PAR": "Three modulators side by side on one carrier, each a ratio on 24 steps and a depth, static or falling from the trig; a tone. The Monomachine's metal: plucky operators into FX-CHORUS.",
    "FM+DYN": "Two operators whose ratios sweep: the first's continuous to 2:1, the second's on a square law to 4:1, both smooth under an LFO; ratio and depth moved by the envelope; feedback. Basses, sweeps.",
    "SWAV-SAW": "A supersaw: the unison's level and width, an extended pair, drift, a square an octave down and sines one and two octaves down. Big basses and leads.",
    "SWAV-PUL": "A pulse with its unison, subs, its width and the width's sweep.",
    "SWAV-ENS": "An ensemble: three more voices at their intervals over the note, a wave, a pulse width, a chorus and its width. Chords, pads and stabs on one track.",
    "DIGI-WAV": "MONO2K's own wavetables: a wave, scanning through the bank and its speed, sync, bits, detune, a second wave mixed in, stepped scanning.",
    "VOX-6": "The Monomachine's VO-6: a formant voice. VOC1 sets the first formant, VOC2 the second (together, the vowel), VOIC whispers it, and its level holds itself as the Monomachine's does, fading in from each trig. The consonants (CONS CLEN CVOL) come next.",
    "FX-CHORS": "An FX machine: a chorus on the track before, its delay, depth and speed, its feedback (all the way up, a comb that rings), width and lowpass. The heart of the metal sounds.",
    "FX-RING": "An FX machine: the track before (after its VOL and LEV) ring-modulated by a carrier on the note, a sine through a triangle to a square by WAVE; INP drives it into a clip of its own; EXT takes the carrier from the track before that instead. As the Monomachine's RINGMOD, measured.",
    "FX-DYNMX": "An FX machine: dynamics on the track before, attack, release, threshold, ratio, make-up gain, RMS or peak.",
    "DRM-KICK": "A drum voice after the LXR's: a carrier (sine, triangle, saw, square) swept down from up to 33 times its pitch on the LXR's laws (PDEP the depth, PDEC the time, PCRV the curve, a step every 1.45 ms as its), FM from a sine whose depth rides the sweep, a snap of pitch, a click. Kicks and toms.",
    "DRM-SNAR": "The LXR's snare: a swept tone and a sample-and-hold noise at a rate of its own, crossfaded by MIX, the noise alone through a lowpass or a highpass (NFLT); CLAP repeats the hit up to 15 times at CSPC's spacing. Snares and claps.",
    "DRM-HATS": "The LXR's hats: a carrier phase-modulated by a sine and a held noise, each tuned in semitones from the note, at half level; a decay of its own, closed (CDEC) or open (OPEN: lock it on the open steps), each hit choking the last. Hats and metal ticks.",
    "DRM-CYMB": "The hats' engine at full level for cymbals: a swell (ATK) and a long fall (DEC) on the LXR's laws. Crashes, rides, reverse swells.",
  };
  const cstr = (x, p) => { const m = new Uint8Array(x.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
  async function firmware() {
    const r = await fetch("m2k.wasm?v=106");
    const x = new WebAssembly.Instance(new WebAssembly.Module(await r.arrayBuffer()), {}).exports;
    if (x._initialize) x._initialize();
    x.w_init(2000);
    /* the machines, in ALGORITHM's order (an older m2k.wasm without w_morder: by number): their marks drawn as the
     * emulator's cards draw them */
    const ml = $("[data-machines]");
    if (ml) {
      ml.innerHTML = "";
      for (let i = 0; i < x.w_machines(); i++) {
        const m = x.w_morder ? x.w_morder(i) : i;
        const name = cstr(x, x.w_mi(m, 0)), style = cstr(x, x.w_mi(m, 2)), cls = cstr(x, x.w_mi(m, 3));
        const knobs = Array.from({ length: 8 }, (_, i) => {
          const s4 = cstr(x, x.w_syn(m, i, 0)), ln = cstr(x, x.w_syn(m, i, 1));
          return `<li><b>${"ABCDEFGH"[i]} ${esc(s4)}</b>${ln === "-" ? "" : esc(ln)}</li>`;
        }).join("");
        const sec = document.createElement("section");
        sec.innerHTML = `<canvas width="72" height="72" role="img" aria-label="${esc(name)}'s mark"></canvas><div><h3>${esc(name)}</h3><p class="meta">${esc(cls)} · ${esc(style)}</p><p>${esc(MACH_TEXT[name] || "")}</p></div><ol>${knobs}</ol>`;
        ml.appendChild(sec);
        const bits = new Uint8Array(x.memory.buffer, x.w_mark(m), 512).slice();
        const cv = $("canvas", sec);
        const draw = () => {
          const g = cv.getContext("2d"), img = g.createImageData(72, 72), I = hex(ink()), P = hex(paper());
          for (let p = 0; p < 72 * 72; p++) { const o = p * 4; img.data[o] = P[0]; img.data[o + 1] = P[1]; img.data[o + 2] = P[2]; img.data[o + 3] = 255; }
          for (let i = 0; i < 512; i++) for (let j = 0; j < 8; j++) {
            if (!((bits[i] >> (7 - j)) & 1)) continue;
            const o = (((i >> 3) + 4) * 72 + (i & 7) * 8 + j + 4) * 4;
            img.data[o] = I[0]; img.data[o + 1] = I[1]; img.data[o + 2] = I[2];
          }
          g.putImageData(img, 0, 0);
        };
        draw();
        cv._redraw = draw;
      }
    }
    /* the pages and their knobs, as the panel names them */
    const P = { HOME: 0, SYN: 1, AMP: 2, FILT: 3, FX: 4, LFO: 5, ARP: 6, GLO: 7 };
    const knobs = (page, subs) => subs.flatMap((sub) => [0, 1, 2, 3].map((k) => ({ s4: cstr(x, x.w_knob(page, sub, 0, k)), name: cstr(x, x.w_kname(page, sub, 0, k)) }))).filter((r) => r.name);
    const rows = (page, subs) => knobs(page, subs).map((r) => `<tr><td>${esc(r.s4)}</td><td>${esc(r.name.toLowerCase().replace(/^./, (c) => c.toUpperCase()))}</td></tr>`).join("");
    const BLOCKS = [
      ["HOME", "HOME", P.HOME, [0]], ["AMP", "ENV", P.AMP, [0, 1]], ["FILTER", "SEL", P.FILT, [0, 1]],
      ...[0, 1, 2, 3].map((sub) => [cstr(x, x.w_page_label(P.FX, sub)), "FX", P.FX, [sub]]),
      ...[0, 1, 2].map((l) => [`LFO ${l + 1}`, "LFO", P.LFO, [2 * l, 2 * l + 1]]),
      ["ARP", "ARP", P.ARP, [0]],
      ...Array.from({ length: x.w_subs(P.GLO) }, (_, sub) => [cstr(x, x.w_page_label(P.GLO, sub)), "GLO", P.GLO, [sub]]),   /* TIME BUS SET MIDI IN PLAY */
    ];
    const pages = $("[data-pages]");
    if (pages) pages.innerHTML = `<table><thead><tr><th>Page</th><th>Button</th><th>Knobs</th></tr></thead><tbody>${BLOCKS.map(([t, b, page, subs]) =>
      `<tr><td>${esc(t)}</td><td>${esc(b)}</td><td>${knobs(page, subs).map((r) => esc(r.s4)).join(" · ")}</td></tr>`).join("")}</tbody></table>`;
    const params = $("[data-params]");
    if (params) params.innerHTML = BLOCKS.map(([t, b, page, subs]) => `<h3>${esc(t)} <span style="opacity:.6">· ${esc(b)}</span></h3><table><tbody>${rows(page, subs)}</tbody></table>`).join("");
    /* bank A: its four parts, and what each is (docs/bank-a.md); A05-A16 blank */
    const BANK_TEXT = {
      CIRCUIT: "126 · A minor · electro: a broken kick, a sine bass moving through each chord and stepping into the next, chord hits in the gaps, FM keys whose arpeggio is its own melody; the snare and hats in from the second pass",
      TIDE: "122 · E dorian · Em7 and A9 on the ENS pad, an FM bass off the beat, FM keys answering and ending two ways; the snare from the second pass, the hats on the third and fourth",
      SUSPEND: "110 · D · sus2 chords moved whole (D, Bb, G), their 3rd coming in every other pass; a sparse kick, a sine rim, a sub walking down at the end; FM keys moving with the chords",
      LANTERN: "100 · A · add9 chord hits moved whole (A, F, C, G), swung; FM keys on each chord's 3rd, 5th and 9th; a kick, a sine rim, a sub; the cymbal from the third pass",
    };
    const bank = $("[data-bank]");
    if (bank) {
      const n = [];
      for (let i = 0; i < 16; i++) {
        const nm = cstr(x, x.w_pat_name(i));
        n.push(`<tr><td>A${String(i + 1).padStart(2, "0")}</td><td>${esc(nm)}</td><td>${esc(BANK_TEXT[nm] || "")}</td></tr>`);
      }
      bank.innerHTML = `<table><thead><tr><th>Slot</th><th>Pattern</th><th>What it is</th></tr></thead><tbody>${n.join("")}</tbody></table>`;
    }
  }
  firmware().catch(() => { const ml = $("[data-machines]"); if (ml) ml.innerHTML = "<p>The machines could not be read from the firmware here.</p>"; });

  // ------------------------------------------------------------------------------------------ the change log
  /* changelog.js's updates, newest first; the kinds as toggles (one at a time, or all), an update with none of the
   * kind picked hidden */
  const log = $("[data-changelog]"), CH = window.M2K_CHANGES, KINDS = window.M2K_KINDS;
  if (log && CH && KINDS) {
    const count = {};
    CH.forEach((u) => u.items.forEach(([k]) => { count[k] = (count[k] || 0) + 1; }));
    const when = (d) => new Date(d + "T12:00:00").toLocaleDateString("en-GB", { day: "numeric", month: "long", year: "numeric" });
    const chip = (k, label, n) => `<button type="button" data-kind="${k}" aria-pressed="${k === "all"}">${esc(label)}<small>${n}</small></button>`;
    log.innerHTML = `<div class="log__kinds" role="group" aria-label="Kinds of change">${chip("all", "All", CH.reduce((a, u) => a + u.items.length, 0))}${
      Object.keys(KINDS).filter((k) => count[k]).map((k) => chip(k, KINDS[k], count[k])).join("")}</div>` +
      CH.map((u) => `<section class="log__up"><header><time datetime="${esc(u.date)}">${esc(when(u.date))}</time><h2>${esc(u.title)}</h2></header><ul>${
        u.items.map(([k, t]) => `<li data-kind="${esc(k)}"><span class="log__kind">${esc(KINDS[k] || k)}</span><span>${esc(t)}</span></li>`).join("")}</ul></section>`).join("");
    const pick = (kind) => {
      $$(".log__kinds button", log).forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.kind === kind)));
      $$(".log__up", log).forEach((sec) => {
        let any = false;
        $$("li", sec).forEach((li) => { const on = kind === "all" || li.dataset.kind === kind; li.hidden = !on; any = any || on; });
        sec.hidden = !any;
      });
    };
    $(".log__kinds", log).addEventListener("click", (e) => { const b = e.target.closest("button"); if (b) pick(b.dataset.kind); });
  }

  /* the theme changed: the screens and the marks in the new colours */
  window.addEventListener("m2k-theme", () => {
    $$("canvas[data-scr]").forEach(drawScreen);
    $$(".mlist canvas").forEach((c) => c._redraw && c._redraw());
  });
})();
