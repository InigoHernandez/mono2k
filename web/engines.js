// SPDX-License-Identifier: GPL-3.0-only
// The machines: all nineteen in ALGORITHM's order, each with what it is and what its INIT is for, the FM-1's own
// screen for it, and a Hit: a copy of the firmware of its own (never the emulator's) renders the machine's INIT at its
// home note (w_audition, w_hit: an FX machine after a plain SWAV-SAW on the track before, as its INIT was voiced),
// with its eight knobs as set here; the page plays the samples and draws their waveform and spectrum.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const S = window.M2K_SCREENS, H = window.M2K_HELP;
  if (!$("eng")) return;
  const cstr = (x, p) => { const m = new Uint8Array(x.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  const SR = 44100;
  /* what each is in a sentence, and what its INIT is (docs/init-sounds.md, MACH_INIT in m2k_seq.c, the manual) */
  const TEXT = {
    "GND-SIN": ["The Monomachine's sine, with MONO2K's own fold, sub, noise, pitch drop and click.", "a round sine bass"],
    "SWAV-SAW": ["A supersaw: unison, drift, a square sub and two sine subs.", "a wide, lush pad"],
    "SWAV-PUL": ["A pulse: its width and sweep, a unison pair, two sine subs.", "a hollow reed lead"],
    "SWAV-ENS": ["Four voices at their intervals, chorused across the stereo: chords on one track.", "a minor-7 pad"],
    "FM+STAT": ["Two FM operators on 24 ratio steps, with a falling depth, feedback and tone.", "an e-piano"],
    "FM+PAR": ["Three FM modulators side by side on one carrier.", "a gong-like bell"],
    "FM+DYN": ["Two FM operators whose ratio and depth an envelope sweeps.", "an FM bass"],
    "VOX-6": ["The Monomachine's VO-6 formant voice: two formants make the vowel.", "an “aah” choir"],
    "DIGI-WAV": ["Digital waves, MONO2K's 32 or the owner's 64, scanned through as they play.", "a pluck that scans as it falls"],
    "SID-6581": ["After the chip: five waves, a swept pulse width, ring and sync.", "a sync lead"],
    "DRM-KICK": ["A kick on the LXR's laws: a swept carrier, FM, a snap and a click.", "a kick"],
    "DRM-SNAR": ["The LXR's snare: a swept tone, a held noise and a clap's repeats.", "a snare"],
    "DRM-HATS": ["The LXR's hats: a carrier modulated by a sine and a noise, closed or open.", "a closed hat"],
    "DRM-CYMB": ["The hats' engine for cymbals: a swell and a long fall.", "a crash"],
    "GND-PERC": ["MONO2K's own noise percussion: colour, metal, a pitch drop and grit.", "a metal hit"],
    "GND-NOIS": ["The Monomachine's noise, white to red, its filter on the key.", "a wind you play"],
    "FX-CHORS": ["A chorus on the track before, here a plain SWAV-SAW.", "the track before made wide"],
    "FX-DYNMX": ["A compressor on the track before, here a plain SWAV-SAW.", "the track before punched"],
    "FX-RING": ["The track before ring-modulated, here a plain SWAV-SAW, as the Monomachine's RINGMOD.", "the track before turned to metal"],
  };
  const GROUPS = [["Notes", 10], ["Drums", 4], ["Noise", 2], ["FX", 3]];   /* ALGORITHM's order, in its families */

  let core = null, names = [], order = [], m = 0, t = 0, vals = new Array(8).fill(0), init = new Array(8).fill(0);
  let hctx = null, src = null, hit = null;
  const lcd = $("eng-lcd"), wave = $("eng-wave"), spec = $("eng-spec");

  // ------------------------------------------------------------------------------------------- the core
  /* the machine at its INIT on a fresh core, the knobs as set here, the SYN page (E-H's when a knob there moved) */
  function prepare(sub) {
    const x = core;
    x.w_init(7919);
    if (x.w_skip_splash) x.w_skip_splash();        /* (the splash would take the presses below) */
    x.w_setting(0, S.pageDark() ? 0 : 127);
    x.w_frame(1); x.w_frame(2100);
    t = x.w_audition(m);
    for (let k = 0; k < 8; k++) if (vals[k] !== init[k]) x.w_param(t, k, vals[k]);
    if (sub) { x.w_button(4, 1); x.w_button(4, 0); }
    x.w_frame(2400);
    return x;
  }
  const frameAt = (ms) => new Uint8Array(core.memory.buffer, core.w_frame(ms >>> 0), 7200);
  function render() {
    const x = prepare(0);
    x.w_hit();
    const max = Math.round(4 * SR / 128), L = new Float32Array(max * 128), R = new Float32Array(max * 128), frames = [];
    const a = new Int16Array(x.memory.buffer, x.w_audio(), 256);
    let quiet = 0, n = 0;
    for (let b = 0; b < max; b++) {
      x.w_render(128);
      let pk = 0;
      for (let i = 0; i < 128; i++) {
        const l = a[2 * i] / 32768, r = a[2 * i + 1] / 32768;
        L[n] = l; R[n] = r; n++;
        pk = Math.max(pk, Math.abs(l), Math.abs(r));
      }
      quiet = pk < 3e-4 ? quiet + 1 : 0;               /* -70 dBFS for a quarter of a second: done */
      if ((b * 128) % 1455 < 128) frames.push(frameAt(2400 + (b * 128 * 1000) / SR).slice());
      if (quiet > 86 && n > SR / 4) break;
    }
    let end = n;                                        /* the hit, to where it falls under -60 dBFS */
    while (end > 1 && Math.abs(L[end - 1]) < 1e-3 && Math.abs(R[end - 1]) < 1e-3) end--;
    end = Math.min(n, Math.max(Math.round(SR * 0.3), end + Math.round(SR * 0.05)));
    if (end === max * 128) for (let i = 0; i < 6615; i++) { const g = i / 6615; L[end - 1 - i] *= g; R[end - 1 - i] *= g; }   /* cut at 4 s: faded */
    return { L: L.subarray(0, end), R: R.subarray(0, end), frames, n: end };
  }

  // ----------------------------------------------------------------------------------------- analysis
  function fft(re, im) {                               /* in place, radix 2 */
    const n = re.length;
    for (let i = 1, j = 0; i < n; i++) {
      let bit = n >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; }
    }
    for (let len = 2; len <= n; len <<= 1) {
      const a = (-2 * Math.PI) / len, wr = Math.cos(a), wi = Math.sin(a);
      for (let i = 0; i < n; i += len) {
        let cr = 1, ci = 0;
        for (let k = 0; k < len / 2; k++) {
          const ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci, vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
          re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
          const nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
        }
      }
    }
  }
  /* the hit's spectrum: 4096-point Hann frames, half overlapped, their power averaged over the sound (to 1.5 s) */
  function spectrum(h) {
    const N = 4096, P = new Float64Array(N / 2), w = new Float64Array(N);
    for (let i = 0; i < N; i++) w[i] = 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (N - 1));
    const re = new Float64Array(N), im = new Float64Array(N), stop = Math.min(h.n, SR * 1.5);
    let frames = 0;
    for (let o = 0; o + N / 4 < stop; o += N / 2) {
      for (let i = 0; i < N; i++) { const j = o + i; re[i] = j < h.n ? ((h.L[j] + h.R[j]) / 2) * w[i] : 0; im[i] = 0; }
      fft(re, im);
      for (let k = 0; k < N / 2; k++) P[k] += re[k] * re[k] + im[k] * im[k];
      frames++;
    }
    let mx = 1e-30, mk = 1, sw = 0, swf = 0;
    for (let k = 1; k < N / 2; k++) {
      const f = (k * SR) / N;
      if (f >= 25 && f <= 16000 && P[k] > mx) { mx = P[k]; mk = k; }
      sw += P[k]; swf += P[k] * f;
    }
    /* the peak between its bins: a parabola through the log power of the three */
    const la = Math.log(P[mk - 1] + 1e-30), lb = Math.log(P[mk] + 1e-30), lc = Math.log(P[mk + 1] + 1e-30), den = la - 2 * lb + lc;
    const off = den < 0 ? Math.max(-0.5, Math.min(0.5, (0.5 * (la - lc)) / den)) : 0;
    return { P, N, max: mx, peakHz: ((mk + off) * SR) / N, centroid: swf / Math.max(1e-30, sw), frames };
  }
  const db = (v) => (v > 0 ? 20 * Math.log10(v) : -Infinity);
  const fmtHz = (f) => (f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 1 : 2) + " kHz" : Math.round(f) + " Hz");
  const fmtDb = (v) => (v === -Infinity ? "−∞" : (v < 0 ? "−" : "") + Math.abs(v).toFixed(1));
  const noteOf = (f) => H.note(Math.round(69 + 12 * Math.log2(f / 440)));

  // ------------------------------------------------------------------------------------------- drawing
  const col = () => { const cs = getComputedStyle($("eng")); return { ink: cs.getPropertyValue("--ink").trim() || "#f2f1ec", paper: cs.getPropertyValue("--paper").trim() || "#0b0b0b" }; };
  function sized(cv) {
    const dpr = Math.min(2, window.devicePixelRatio || 1), w = Math.max(80, Math.round(cv.clientWidth * dpr)), h = Math.max(60, Math.round(cv.clientHeight * dpr));
    if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
    return { g: cv.getContext("2d"), w, h, dpr };
  }
  let checker = null;
  function dither(g, ink) {                            /* the 1-bit fill: a checker of the ink */
    const c = document.createElement("canvas");
    c.width = c.height = 2;
    const cg = c.getContext("2d");
    cg.fillStyle = ink; cg.fillRect(0, 0, 1, 1); cg.fillRect(1, 1, 1, 1);
    return g.createPattern(c, "repeat");
  }
  const AX = 16;                                       /* the axis band at the foot, in CSS px */
  /* the waveform: each column's min and max; while it plays, drawn up to the playhead, the rest a dotted line */
  function drawWave(h, at) {
    const { g, w, h: H0, dpr } = sized(wave), { ink, paper } = col();
    g.fillStyle = paper; g.fillRect(0, 0, w, H0);
    const ax = AX * dpr, ph = H0 - ax, mid = ph / 2, upto = at == null ? w : Math.round(at * (w - 1));
    g.fillStyle = ink;
    for (let x = 0; x < w; x += 3 * dpr) g.fillRect(x, Math.round(mid), dpr, 1);
    if (!h) return;
    g.font = `${10 * dpr}px "Helvetica Neue", Helvetica, Arial, sans-serif`;
    g.textBaseline = "middle";
    const dur = h.n / SR, step = dur > 2 ? 1 : dur > 1 ? 0.5 : dur > 0.5 ? 0.25 : 0.1;
    for (let s = 0; s <= dur + 1e-9; s += step) {
      const x = Math.round((s / dur) * (w - 1));
      g.globalAlpha = 0.35; g.fillRect(x, 0, 1, ph); g.globalAlpha = 1;
      g.textAlign = s === 0 ? "left" : x > w - 30 * dpr ? "right" : "center";
      g.fillText(s === 0 ? "0 s" : +s.toFixed(2) + "", x, ph + ax / 2);
    }
    for (let x = 0; x < upto; x++) {
      const a = Math.floor((x / w) * h.n), b = Math.max(a + 1, Math.floor(((x + 1) / w) * h.n));
      let lo = 0, hi = 0;
      for (let i = a; i < b; i++) { const v = (h.L[i] + h.R[i]) / 2; if (v < lo) lo = v; if (v > hi) hi = v; }
      const y0 = Math.round(mid - hi * mid), y1 = Math.round(mid - lo * mid);
      g.fillRect(x, y0, 1, Math.max(1, y1 - y0));
    }
    if (at != null) g.fillRect(upto, 0, dpr, ph);
  }
  const F0 = 30, F1 = 16000, DB = 80;
  /* the spectrum: the hit's average, or while it plays the 2048 samples under the playhead */
  function liveSpec(h, at) {
    const N = 2048, re = new Float64Array(N), im = new Float64Array(N), o = Math.max(0, Math.min(h.n - N, Math.floor(at * h.n) - N / 2));
    for (let i = 0; i < N; i++) { const j = o + i; re[i] = j < h.n ? ((h.L[j] + h.R[j]) / 2) * (0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (N - 1))) : 0; }
    fft(re, im);
    const P = new Float64Array(N / 2);
    for (let k = 0; k < N / 2; k++) P[k] = re[k] * re[k] + im[k] * im[k];
    return { P, N, max: (hit.sp.max / Math.max(1, hit.sp.frames)) * (N / hit.sp.N) ** 2 };   /* against the average's peak: it breathes */
  }
  function drawSpec(sp) {
    const { g, w, h: H0, dpr } = sized(spec), { ink, paper } = col();
    g.fillStyle = paper; g.fillRect(0, 0, w, H0);
    const ax = AX * dpr, ph = H0 - ax;
    g.fillStyle = ink;
    g.font = `${10 * dpr}px "Helvetica Neue", Helvetica, Arial, sans-serif`;
    g.textBaseline = "middle";
    const xOf = (f) => (Math.log(f / F0) / Math.log(F1 / F0)) * (w - 1);
    for (const [f, t] of [[100, "100"], [1000, "1k"], [10000, "10k"]]) {
      const x = Math.round(xOf(f));
      g.globalAlpha = 0.35; g.fillRect(x, 0, 1, ph); g.globalAlpha = 1;
      g.textAlign = "center"; g.fillText(t, x, ph + ax / 2);
    }
    for (const d of [20, 40, 60]) { const y = Math.round((d / DB) * ph); g.globalAlpha = 0.2; g.fillRect(0, y, w, 1); g.globalAlpha = 1; }
    g.textAlign = "left"; g.fillText("Hz", 2 * dpr, ph + ax / 2);
    if (!sp) return;
    const ys = new Float32Array(w);
    for (let x = 0; x < w; x++) {                      /* the strongest bin under each column, against the peak */
      const fa = F0 * Math.pow(F1 / F0, x / w), fb = F0 * Math.pow(F1 / F0, (x + 1) / w);
      const ka = Math.max(1, Math.floor((fa * sp.N) / SR)), kb = Math.max(ka + 1, Math.ceil((fb * sp.N) / SR));
      let p = 0;
      for (let k = ka; k < kb && k < sp.N / 2; k++) p = Math.max(p, sp.P[k]);
      const d = Math.max(-DB, Math.min(0, 10 * Math.log10(p / sp.max + 1e-30)));
      ys[x] = (-d / DB) * ph;
    }
    if (!checker) checker = dither(g, ink);
    g.fillStyle = checker;
    g.beginPath(); g.moveTo(0, ph);
    for (let x = 0; x < w; x++) g.lineTo(x, ys[x]);
    g.lineTo(w - 1, ph); g.closePath(); g.fill();
    g.strokeStyle = ink; g.lineWidth = 2 * dpr; g.lineJoin = "round";
    g.beginPath();
    for (let x = 0; x < w; x++) (x ? g.lineTo(x, ys[x]) : g.moveTo(x, ys[x]));
    g.stroke();
    spec._ys = ys; spec._ph = ph;
  }
  /* the machine's own mark on the Hit, in the button's paper: still, or turning with the sound as the screen's does */
  const markCv = $("eng-mark");
  let markF = 0;
  function drawMark(bits) {
    if (!markCv) return;
    const g = markCv.getContext("2d"), img = g.createImageData(64, 64), cs = getComputedStyle(markCv);
    const P = cs.color.match(/\d+/g).map(Number);
    for (let i = 0; i < 512; i++) for (let j = 0; j < 8; j++) {
      if (!((bits[i] >> (7 - j)) & 1)) continue;
      const o = ((i >> 3) * 64 + (i & 7) * 8 + j) * 4;
      img.data[o] = P[0]; img.data[o + 1] = P[1]; img.data[o + 2] = P[2]; img.data[o + 3] = 255;
    }
    g.putImageData(img, 0, 0);
  }
  const markStill = () => drawMark(new Uint8Array(core.memory.buffer, core.w_mark(m), 512));
  const hitBtn = $("eng-hit");

  // ---------------------------------------------------------------------------------------- the hit
  async function play() {
    if (!core) return;
    if (!hctx) {
      try { hctx = new AudioContext({ sampleRate: SR, latencyHint: "interactive" }); } catch (e) { hctx = new AudioContext({ latencyHint: "interactive" }); }
    }
    if (hctx.state !== "running") await hctx.resume();
    const h = render(), sp = spectrum(h);
    hit = { h, sp };
    if (src) { try { src.stop(); } catch (e) { /* ended */ } }
    const buf = hctx.createBuffer(2, h.n, SR);
    buf.copyToChannel(h.L, 0); buf.copyToChannel(h.R, 1);
    src = hctx.createBufferSource();
    src.buffer = buf;
    src.connect(hctx.destination);
    const t0 = hctx.currentTime + 0.02;
    src.start(t0);
    checker = null;
    let pk = 0, tail = h.n;
    for (let i = 0; i < h.n; i++) pk = Math.max(pk, Math.abs(h.L[i]), Math.abs(h.R[i]));
    while (tail > 1 && Math.abs(h.L[tail - 1]) < 1e-3 && Math.abs(h.R[tail - 1]) < 1e-3) tail--;
    $("eng-nums").textContent = `Peak ${fmtDb(db(pk))} dBFS · ${(tail / SR).toFixed(2)} s · centroid ${fmtHz(sp.centroid)}`;
    $("eng-nums").title = `Strongest partial ${fmtHz(sp.peakHz)} (${noteOf(sp.peakHz)}); length above −60 dBFS`;
    const me = src, live = S.motion();
    /* while it plays: the screen's frames, the waveform drawn to the playhead, the spectrum under it, the Hit's level */
    const anim = () => {
      if (src !== me) return;
      const el = hctx.currentTime - t0, p = el / (h.n / SR);
      if (p >= 1) {
        drawWave(h, null); drawSpec(sp); S.draw(lcd, h.frames[h.frames.length - 1]);
        hitBtn.style.setProperty("--lv", "0"); markStill();
        return;
      }
      const at = Math.max(0, p), i0 = Math.floor(at * h.n);
      let s2 = 0;
      for (let i = i0; i < Math.min(h.n, i0 + 1024); i++) s2 += (h.L[i] * h.L[i] + h.R[i] * h.R[i]) / 2;
      const lv = Math.max(0, Math.min(1, (10 * Math.log10(s2 / 1024 + 1e-12) + 48) / 42));
      drawWave(h, at);
      drawSpec(el > 0 ? liveSpec(h, at) : null);
      S.draw(lcd, h.frames[Math.min(h.frames.length - 1, Math.max(0, Math.floor((el * SR) / 1455)))]);
      hitBtn.style.setProperty("--lv", lv.toFixed(3));
      markF += 0.15 + lv * 0.9;
      drawMark(new Uint8Array(core.memory.buffer, core.w_icon(m, Math.floor(markF) % 12), 512));
      requestAnimationFrame(anim);
    };
    if (live) requestAnimationFrame(anim);
    else { drawWave(h, null); drawSpec(sp); S.draw(lcd, h.frames[Math.min(h.frames.length - 1, 4)]); }
  }

  // ----------------------------------------------------------------------------------- the machine
  function choose(k, sound) {
    m = order[k];
    const x = core, name = names[m];
    $("eng-name").textContent = name;
    const [d, ini] = TEXT[name] || ["", ""];
    $("eng-desc").textContent = d;
    $("eng-init").textContent = `INIT: ${ini}, home ${H.note(x.w_home_note(m))}.`;
    $("eng-hit-n").textContent = H.note(x.w_home_note(m));
    document.querySelectorAll("#eng-pick button").forEach((b) => b.setAttribute("aria-pressed", String(+b.dataset.m === m)));
    /* the knobs: the machine's A-H at its INIT */
    prepare(0);
    const kit = new Uint8Array(x.memory.buffer, x.w_kit(t), 8);
    for (let i = 0; i < 8; i++) init[i] = vals[i] = kit[i];
    knobs();
    S.draw(lcd, frameAt(2400));
    markStill();
    hit = null;
    if (src) { try { src.stop(); } catch (e) { /* ended */ } src = null; }
    hitBtn.style.setProperty("--lv", "0");
    drawWave(null); drawSpec(null);
    $("eng-wave-r").textContent = ""; $("eng-spec-r").textContent = "";
    $("eng-nums").textContent = " "; $("eng-nums").title = "";
    if (sound) play();
  }
  function knobs() {
    const x = core, box = $("eng-knobs");
    box.innerHTML = Array.from({ length: 8 }, (_, k) => {
      const s4 = cstr(x, x.w_syn(m, k, 0)), ln = cstr(x, x.w_syn(m, k, 1)), off = s4 === "-" || !s4;
      const lo = off ? 0 : x.w_kmin(t, k), hi = off ? 127 : x.w_kmax(t, k);
      return `<div class="ek${off ? " off" : ""}" title="${esc(off ? "" : ln)}"><label for="ek${k}"><b>${"ABCDEFGH"[k]} ${esc(off ? "—" : s4)}</b></label><output id="ek${k}o">${off ? "" : esc(cstr(x, x.w_fmt(t, k, vals[k])))}</output>` +
        `<input type="range" id="ek${k}" min="${lo}" max="${hi}" value="${vals[k]}" ${off ? "disabled" : ""} aria-label="${esc(off ? "unused" : ln)}"></div>`;
    }).join("");
    box.querySelectorAll("input").forEach((inp, k) => {
      inp.addEventListener("input", () => {
        vals[k] = +inp.value;
        prepare(k >= 4);
        $("ek" + k + "o").textContent = cstr(core, core.w_fmt(t, k, vals[k]));
        S.draw(lcd, frameAt(2400));
      });
      inp.addEventListener("change", () => play());
    });
  }
  function picker() {
    let k = 0;
    $("eng-pick").innerHTML = GROUPS.map(([g, n]) => {
      const bs = Array.from({ length: n }, () => { const mm = order[k++]; return `<button type="button" data-m="${mm}" data-k="${k - 1}" aria-pressed="false">${esc(names[mm])}</button>`; }).join("");
      return `<div class="eng__grp"><h4 class="label">${g}</h4><div class="eng__btns">${bs}</div></div>`;
    }).join("");
    $("eng-pick").addEventListener("click", (ev) => { const b = ev.target.closest("button[data-k]"); if (b) choose(+b.dataset.k, true); });
  }

  // ------------------------------------------------------------------------------------------ readouts
  wave.addEventListener("pointermove", (ev) => {
    if (!hit) return;
    const r = wave.getBoundingClientRect(), p = Math.min(1, Math.max(0, (ev.clientX - r.left) / r.width));
    const a = Math.floor(p * hit.h.n), b = Math.min(hit.h.n, a + Math.max(1, Math.floor(hit.h.n / r.width)));
    let pk = 0;
    for (let i = a; i < b; i++) pk = Math.max(pk, Math.abs(hit.h.L[i]), Math.abs(hit.h.R[i]));
    $("eng-wave-r").textContent = `${(a / SR).toFixed(3)} s · peak ${fmtDb(db(pk))} dBFS`;
  });
  spec.addEventListener("pointermove", (ev) => {
    if (!hit || !spec._ys) return;
    const r = spec.getBoundingClientRect(), p = Math.min(1, Math.max(0, (ev.clientX - r.left) / r.width));
    const f = F0 * Math.pow(F1 / F0, p), y = spec._ys[Math.min(spec._ys.length - 1, Math.floor(p * spec._ys.length))];
    $("eng-spec-r").textContent = `${fmtHz(f)} · ${fmtDb((-y / spec._ph) * DB)} dB`;
  });
  for (const cv of [wave, spec]) cv.addEventListener("pointerleave", () => { $(cv === wave ? "eng-wave-r" : "eng-spec-r").textContent = ""; });

  // ---------------------------------------------------------------------------------------------- go
  $("eng-hit").addEventListener("click", () => play());
  $("eng-reset").addEventListener("click", () => { for (let i = 0; i < 8; i++) vals[i] = init[i]; knobs(); prepare(0); S.draw(lcd, frameAt(2400)); play(); });
  function redraw() {
    checker = null;
    if (!core) return;
    prepare(0);
    S.draw(lcd, hit ? hit.h.frames[hit.h.frames.length - 1] : frameAt(2400));
    markStill();
    drawWave(hit && hit.h); drawSpec(hit && hit.sp);
  }
  window.addEventListener("m2k-theme", redraw);
  let rz = 0;
  window.addEventListener("resize", () => { clearTimeout(rz); rz = setTimeout(() => { drawWave(hit && hit.h); drawSpec(hit && hit.sp); }, 120); });
  (async () => {
    core = await S.instance();
    core.w_init(7919);
    const n = core.w_machines();
    names = Array.from({ length: n }, (_, i) => cstr(core, core.w_mi(i, 0)));
    order = Array.from({ length: n }, (_, i) => core.w_morder(i));
    picker();
    choose(0, false);
  })().catch((e) => { $("eng-nums").textContent = "The machines could not load here: " + e.message; });
})();
