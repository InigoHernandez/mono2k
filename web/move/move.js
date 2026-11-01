// SPDX-License-Identifier: GPL-3.0-only
// The Move's home: the wordmark (../wordmark.js), the pads played here (each page pad shows the Move's screen for it,
// PERFORM turns the keys into its effects, the steps play on), PERFORM's two rows, the tempo's three states, the numbers
// that count up, and the machines (../engines.js, the FM-1's, with a copy of the firmware: loaded as they come near).
// The screens are stills of the Move's own (move/screen's sheets, screens/*.png); nothing here makes a sound but the
// machines' Hit. The playable Move is gear.js's.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const still = () => matchMedia("(prefers-reduced-motion: reduce)").matches;
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);
  const scr = (n) => `screens/${n}.png`;
  /* a part that runs only while it is seen */
  const seen = (el, on, margin = "0px") => {
    if (!el) return;
    if (!("IntersectionObserver" in window)) { on(true); return; }
    new IntersectionObserver((es) => es.forEach((e) => on(e.isIntersecting)), { rootMargin: margin }).observe(el);
  };

  // ------------------------------------------------------------------------------------------- the wordmark
  const wm = $("wordmark");
  if (wm && window.M2K_WORDMARK) {
    const WM = window.M2K_WORDMARK(wm);
    window.addEventListener("m2k-theme", () => WM.draw());
  }

  // ------------------------------------------------------------------------------------------- the pads
  /* the two page rows (move/README.md, the controls): name, family, the screen it leads to, what it shows */
  const PAGES = [
    ["SYN", "snd", "04_syn_0", "The machine: its kind and name; A–H on the knobs."],
    ["AMP", "snd", "05_amp", "The envelope, drawn live."],
    ["FILT", "snd", "06_filt", "The filter's response, live; the envelope's reach dotted."],
    ["EFX", "snd", "07_efx", "The EQ's band and the echoes."],
    ["DSTR", "snd", "08_dstr", "A sine through the crush, and the modulation."],
    ["LFO1", "snd", "09_lfo1", "Where it goes, and its cycle."],
    ["LFO2", "snd", "09_lfo2", "Its wave, as deep as it swings."],
    ["LFO3", "snd", "09_lfo3", "Three LFOs on every track."],
    ["HOME", "rest", "02_home_play", "Eight lanes, each playhead its own."],
    ["TRK", "rest", "10_trk", "Pitch, level and the arp; Loop holds it."],
    ["PAT", "rest", "11_pat", "Tempo, swing, length; the track's own length and speed."],
    ["GRID", "rest", "12_grid", "Grids' map: three lanes of drums."],
    ["MIX", "rest", "13_mix", "The eight levels, on the eight knobs."],
    ["GLO", "rest", "14_glo", "The bus: reverb, ADPCM, drive; mutes at the bar."],
    ["SOUNDS", "fn", "18_cat", "96 sounds: the jog plays each; a click keeps it."],
    ["PERFORM", "fn", "20_perf", "The keys become its effects. Again: the keys back."],
  ];
  const SHORT = { SOUNDS: "SNDS", PERFORM: "PERF", SHIMMER: "SHIM", STUTTER: "STUT", REVERSE: "REV", SCATTER: "SCAT" };   /* a phone's narrow pads */
  const label = (n) => SHORT[n] ? `<span><i class="lng">${esc(n)}</i><i class="sht">${SHORT[n]}</i></span>` : `<span>${esc(n)}</span>`;
  const BLACK = ["C#", "D#", "", "F#", "G#", "A#", "", ""], WHITE = ["C", "D", "E", "F", "G", "A", "B", "C"];
  const COLOURS = ["LPF", "DUB", "HPF", "GATE", "CRUSH", "DRIVE", "WOW", "SHIMMER"];
  const MOVES = [["RPT 1/4", "RPT 1/8", "RPT 1/16", "RPT 1/32", "ROLL", "STUTTER", "REVERSE", "STOP"],
                 ["FREEZE", "SCATTER", "OCT DN", "OCT UP", "SPIN", "KILL", "", ""]];
  const mx = $("mx");
  if (mx) {
    const oled = $("mx-oled"), cap = $("mx-cap"), padsEl = $("mx-pads"), stepsEl = $("mx-steps"), trkEl = $("mx-trk"), shiftBtn = $("mx-shift");
    let page = 8, perf = false, perfView = false, track = 0, shift = false;
    const show = (n, name, text, alt) => {
      if (!oled.src.endsWith(scr(n))) oled.src = scr(n);
      oled.alt = `The Move's screen: ${alt || name}`;
      cap.innerHTML = `<b>${esc(name)}</b>${esc(text)}`;
    };
    const showPage = (i) => { const p = PAGES[i]; show(p[2], p[0], p[3]); };
    const rest = () => { if (perfView) show("20_perf", "PERFORM", "Colours on the top keys, moves on the bottom."); else showPage(page); };
    /* the 32 pads, top to bottom as the Move has them under its screen */
    const pads = [];
    PAGES.forEach((p, i) => {
      const b = document.createElement("button");
      b.type = "button"; b.className = `mpad ${p[1]}`;
      b.innerHTML = label(p[0]);
      b.setAttribute("aria-label", `${p[0]} pad`);
      b.addEventListener("pointerenter", () => showPage(i));
      b.addEventListener("focus", () => showPage(i));
      b.addEventListener("pointerleave", rest);
      b.addEventListener("blur", rest);
      b.addEventListener("click", () => {
        if (p[0] === "PERFORM") { perf = !perf; perfView = perf; keys(); }   /* on across pages; again: the keys back */
        else { page = i; perfView = false; }
        lights(); rest();
      });
      padsEl.appendChild(b); pads.push(b);
    });
    const keyEls = [];
    for (let r = 0; r < 2; r++) for (let c = 0; c < 8; c++) {
      const b = document.createElement("button");
      b.type = "button"; b.dataset.r = r; b.dataset.c = c;
      b.addEventListener("pointerdown", () => {
        if (b.classList.contains("none")) return;
        if (!perf) { b.classList.add("lit"); setTimeout(() => b.classList.remove("lit"), 160); return; }
        const name = b.dataset.name;
        if (r === 0) { b.classList.toggle("lit"); cap.innerHTML = `<b>${esc(name)}</b>A colour: it stacks with the others.`; }
        else {
          const was = b.classList.contains("lit");
          keyEls.filter((k) => k.dataset.r === "1").forEach((k) => k.classList.remove("lit"));
          if (!was) b.classList.add("lit");
          cap.innerHTML = `<b>${esc(name)}</b>A move: one at a time, the next takes over.`;
        }
      });
      padsEl.appendChild(b); keyEls.push(b);
    }
    function keys() {
      keyEls.forEach((b) => {
        const r = +b.dataset.r, c = +b.dataset.c;
        const name = perf ? (r === 0 ? COLOURS[c] : MOVES[0][c]) : (r === 0 ? BLACK[c] : WHITE[c]);
        b.className = "mpad " + (!name ? "none" : perf ? (r === 0 ? "pc" : "pm") : r === 0 ? "blk" : "wht") + (!perf && r === 1 && c === 0 ? " root" : "");
        b.innerHTML = name ? label(name) : "";
        b.dataset.name = name;
        b.setAttribute("aria-label", name ? (perf ? `PERFORM: ${name}` : `Key ${name}`) : "No key");
        b.disabled = !name;
      });
    }
    function lights() { pads.forEach((b, i) => { const on = (i === page && !perfView) || (i === 15 && perf); b.classList.toggle("on", on); b.setAttribute("aria-pressed", String(on)); }); }
    keys(); lights();
    /* the track buttons, 1-4; Shift held on the Move, here a press: 5-8 */
    const trks = [0, 1, 2, 3].map((i) => {
      const b = document.createElement("button");
      b.type = "button"; b.className = "mtrk";
      b.addEventListener("click", () => { track = (shift ? 4 : 0) + i; trkLights(); cap.innerHTML = `<b>T${track + 1}</b>The track in view: every page shows it.`; });
      trkEl.appendChild(b); return b;
    });
    function trkLights() { trks.forEach((b, i) => { const t = (shift ? 4 : 0) + i; b.textContent = t + 1; b.setAttribute("aria-label", `Track ${t + 1}`); b.setAttribute("aria-pressed", String(t === track)); }); }
    shiftBtn.addEventListener("click", () => { shift = !shift; shiftBtn.setAttribute("aria-pressed", String(shift)); trkLights(); });
    trkLights();
    /* the steps: a beat of trigs, the playhead running at 120 while they are seen; a press places or takes a trig */
    const trig = new Set([0, 4, 7, 8, 11, 12, 14]), steps = [];
    for (let i = 0; i < 16; i++) {
      const b = document.createElement("button");
      b.type = "button"; b.className = "mstep";
      b.setAttribute("aria-label", `Step ${i + 1}`);
      b.addEventListener("click", () => { if (trig.has(i)) trig.delete(i); else trig.add(i); paint(-1); });
      stepsEl.appendChild(b); steps.push(b);
    }
    let ph = -1, timer = 0;
    function paint(p) {
      if (p >= 0) ph = p;
      steps.forEach((b, i) => { b.classList.toggle("trig", trig.has(i)); b.classList.toggle("ph", i === ph); b.setAttribute("aria-pressed", String(trig.has(i))); });
    }
    function tick() {
      paint((ph + 1) % 16);
      if (trig.has(ph) && !perf) { const root = keyEls[8]; root.classList.add("lit"); setTimeout(() => root.classList.remove("lit"), 90); }
    }
    paint(-1);
    seen(mx, (on) => {
      clearInterval(timer); timer = 0;
      if (on && !still()) timer = setInterval(tick, 125);
    });
  }

  // ------------------------------------------------------------------------------------------- PERFORM
  const pfC = $("pf-c"), pfM = $("pf-m");
  if (pfC && pfM) {
    const now = $("pf-now"), up = $("pf-up");
    const on = new Set();
    let movePage = 0, move = "";
    const say = () => {
      const names = COLOURS.filter((c) => on.has(c));
      if (move) names.unshift(move);
      now.textContent = names.length ? names.join(" + ") : "Hold a key";
    };
    const mk = (row) => {
      const b = document.createElement("button");
      b.type = "button"; b.className = "pfk";
      row.appendChild(b);
      return b;
    };
    COLOURS.forEach((n) => {
      const b = mk(pfC);
      b.innerHTML = `<span>${esc(n)}</span>`;
      b.setAttribute("aria-pressed", "false");
      b.addEventListener("click", () => { if (on.has(n)) on.delete(n); else on.add(n); b.setAttribute("aria-pressed", String(on.has(n))); say(); });
    });
    const mKeys = Array.from({ length: 8 }, (_, i) => {
      const b = mk(pfM);
      b.addEventListener("click", () => {
        const n = MOVES[movePage][i];
        if (!n) return;
        move = move === n ? "" : n;
        paintMoves(); say();
      });
      return b;
    });
    function paintMoves() {
      mKeys.forEach((b, i) => {
        const n = MOVES[movePage][i];
        b.innerHTML = n ? `<span>${esc(n)}</span>` : "";
        b.disabled = !n;
        b.setAttribute("aria-pressed", String(!!n && n === move));
      });
    }
    up.addEventListener("click", () => { movePage ^= 1; up.setAttribute("aria-pressed", String(movePage === 1)); paintMoves(); });
    paintMoves(); say();
  }

  // ------------------------------------------------------------------------------------------- the tempo
  const tp = $("tp");
  if (tp) {
    const TP = [
      ["22_tempo_shift", "Any page", "Shift + knob 1 sets the tempo: tenths turned slowly, whole BPM fast. With Link on, Move's too.", "the tempo, large, with SYNC"],
      ["11_pat", "Sync", "Move's tempo, locked to its bar. One Play starts both.", "PAT with Move's tempo"],
      ["22_tempo_alone", "Int", "Move didn't follow (Link off)? MONO2K plays on alone at its own tempo, and says INT.", "INT: Move's Link is off"],
    ];
    const seg = tp.querySelector(".seg"), opts = [...seg.querySelectorAll(".seg__o")], img = $("tp-oled");
    TP.forEach(([n]) => { const im = new Image(); im.src = scr(n); });   /* (ready before they are picked) */
    seg.style.setProperty("--n", TP.length);
    let cur = 0, auto = 0, touched = false;
    const pick = (i) => {
      cur = i;
      seg.style.setProperty("--on", i);
      opts.forEach((b, j) => b.setAttribute("aria-pressed", String(j === i)));
      img.src = scr(TP[i][0]); img.alt = `The Move's screen: ${TP[i][3]}`;
      $("tp-t").textContent = TP[i][1]; $("tp-p").textContent = TP[i][2];
    };
    opts.forEach((b, i) => b.addEventListener("click", () => { touched = true; clearInterval(auto); pick(i); }));
    seen(tp, (on) => {
      clearInterval(auto); auto = 0;
      if (on && !touched && !still()) auto = setInterval(() => pick((cur + 1) % TP.length), 4200);
    });
    pick(0);
  }

  // ------------------------------------------------------------------------------------------- the numbers
  document.querySelectorAll("[data-count]").forEach((b) => {
    const n = +b.dataset.count;
    if (still() || !("IntersectionObserver" in window)) return;
    b.textContent = "0";
    let done = false;
    seen(b, (on) => {
      if (!on || done) return;
      done = true;
      const t0 = performance.now(), T = 700;
      const run = (t) => { const p = Math.min(1, (t - t0) / T); b.textContent = String(Math.round(n * (1 - Math.pow(1 - p, 3)))); if (p < 1) requestAnimationFrame(run); };
      requestAnimationFrame(run);
    });
  });

  // ------------------------------------------------------------------------------------------- the machines
  /* the FM-1's machines section, shared: its scripts and a copy of the firmware (the FM-1's six-track build: the
   * machines are the same) fetched only as the section comes near */
  const eng = $("machines");
  if (eng) {
    let loaded = false;
    const load = () => {
      if (loaded) return;
      loaded = true;
      const bytes = fetch("../m2k.wasm?v=112").then((r) => { if (!r.ok) throw new Error(r.status); return r.arrayBuffer(); });
      window.M2K_CORE = window.M2K_CORE || { bytes, module: bytes.then((b) => WebAssembly.compile(b)) };
      const chain = ["../help.js?v=9", "../screens.js?v=3", "../engines.js?v=4"];
      const next = (i) => { if (i >= chain.length) return; const s = document.createElement("script"); s.src = chain[i]; s.onload = () => next(i + 1); document.body.appendChild(s); };
      next(0);
    };
    seen(eng, (on) => { if (on) load(); }, "900px 0px");
  }
})();
