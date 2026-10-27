// SPDX-License-Identifier: GPL-3.0-only
// "How it was made": the fidelity suite's numbers and two of its stories, drawn in one bit. Every number here is the
// repository's own: the suite from a run of tools/ref/regress.py check (944 cases against tools/ref/regress_ref.json),
// the clip ceilings from tools/gen_tables.py's DIST_M.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  if (!$("made")) return;
  const NS = "http://www.w3.org/2000/svg";
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  const minus = (v, d = 1) => (v < 0 ? "−" : v > 0 ? "+" : "") + Math.abs(v).toFixed(d);

  /* regress.py check, 2026-10-27, on main's 7cd78d0 (MONO2K 1.1): [group, PASS, FAIL, XFAIL, INFO] */
  const SUITE = [["levels", 18, 0, 0, 0], ["dist", 20, 0, 0, 3], ["dist_loud", 46, 0, 0, 0], ["machine_level", 14, 0, 2, 1],
    ["filter_mag", 49, 0, 7, 6], ["res_map", 26, 0, 0, 0], ["res_level", 24, 0, 2, 0], ["res_fit", 11, 0, 3, 0], ["clip", 15, 0, 3, 1],
    ["hp_drive", 0, 0, 0, 9], ["hp_sat", 8, 0, 21, 2], ["tune", 20, 0, 0, 0], ["ens", 120, 0, 3, 0], ["ens_stereo", 5, 0, 1, 0],
    ["ens_accept", 13, 0, 0, 3], ["bend", 24, 0, 0, 0], ["eq", 21, 0, 3, 0], ["srr", 7, 0, 1, 0], ["delay_time", 3, 4, 0, 0],
    ["delay_send", 2, 0, 6, 0], ["delay_fb", 3, 0, 0, 0], ["delay_pan", 2, 0, 2, 0], ["delay_filter", 7, 0, 1, 8], ["fenv", 8, 0, 1, 0],
    ["fm", 85, 0, 4, 0], ["fmstep", 80, 0, 0, 0], ["ring", 17, 0, 2, 0], ["hold", 12, 0, 0, 0], ["vo6", 39, 0, 0, 0], ["sid", 45, 0, 6, 0],
    ["kick", 1, 0, 16, 0], ["pul", 31, 0, 6, 0], ["gnoi", 41, 0, 0, 0]];
  const GNAME = {
    levels: "VOL, LEV, PAN", dist: "DIST", dist_loud: "DIST, driven hard", machine_level: "Each machine's level", filter_mag: "Filter response",
    res_map: "Resonance: where", res_level: "Resonance: how high", res_fit: "Resonance: its shape", clip: "The clip and the resonance",
    hp_drive: "HP into the drive", hp_sat: "HP saturation", tune: "The filter follows TUNE", ens: "SWAV-ENS's voices", ens_stereo: "SWAV-ENS's chorus",
    ens_accept: "Bank A's chords", bend: "A resonant sweep's bend", eq: "EQ", srr: "Rate reduction", delay_time: "Delay time", delay_send: "Delay send",
    delay_fb: "Delay feedback", delay_pan: "Ping-pong", delay_filter: "The echoes' filters", fenv: "Filter envelope", fm: "FM+ operators",
    fmstep: "FM+ ratio steps", ring: "FX-RING", hold: "HOLD in beats", vo6: "VOX-6", sid: "SID-6581", kick: "GND-SIN as a kick", pul: "SWAV-PUL", gnoi: "GND-NOIS",
  };
  const ST = [["PASS", "solid"], ["FAIL", "hatch"], ["XFAIL", "check"], ["INFO", "dots"]];
  const ST_TEXT = { PASS: "PASS: within its tolerance", FAIL: "FAIL", XFAIL: "XFAIL: a known gap, held at its size", INFO: "INFO: measured, not asserted" };
  /* tools/gen_tables.py DIST_M: each machine's clip against GND-SIN's, dB, measured on the Monomachine */
  const DIST = [["SWAV-SAW", -10.5], ["DIGI-WAV", -7.0], ["SWAV-PUL", -2.75], ["FM+STAT", -1.4], ["VOX-6", -1.4], ["GND-SIN", 0], ["GND-NOIS", 0],
    ["SID-6581", 0], ["FM+DYN", 0], ["FM+PAR", 0.3], ["SWAV-ENS", 1.7]];

  const svgEl = (tag, a, p) => { const e = document.createElementNS(NS, tag); for (const k in a) e.setAttribute(k, a[k]); if (p) p.appendChild(e); return e; };
  const text = (p, x, y, s, a = {}) => { const t = svgEl("text", { x, y, fill: "currentColor", "font-size": 11, ...a }, p); t.textContent = s; return t; };
  function defs(svg) {
    const d = svgEl("defs", {}, svg);
    const pat = (id, w, cells) => { const p = svgEl("pattern", { id, width: w, height: w, patternUnits: "userSpaceOnUse" }, d); for (const [x, y] of cells) svgEl("rect", { x, y, width: 1, height: 1, fill: "currentColor" }, p); };
    pat("mk-check", 2, [[0, 0], [1, 1]]);
    pat("mk-dots", 3, [[1, 1]]);
    pat("mk-hatch", 4, [[0, 3], [1, 2], [2, 1], [3, 0], [0, 0]]);
  }
  const fillOf = (k) => (k === "solid" ? "currentColor" : `url(#mk-${k})`);
  function legend(box, items) {
    box.innerHTML = items.map(([lab, kind, line]) => `<span class="key"><svg viewBox="0 0 16 12" aria-hidden="true">${line
      ? `<line x1="0" y1="6" x2="16" y2="6" stroke="currentColor" stroke-width="2"/>${line === "sq" ? '<rect x="4" y="2" width="8" height="8" fill="var(--paper)" stroke="currentColor" stroke-width="2"/>' : '<circle cx="8" cy="6" r="4" fill="currentColor"/>'}`
      : `<rect width="16" height="12" fill="${kind === "solid" ? "currentColor" : "none"}"/>${kind !== "solid" ? `<rect width="16" height="12" fill="url(#lg-${kind})"/><defs><pattern id="lg-${kind}" width="${kind === "dots" ? 3 : kind === "hatch" ? 4 : 2}" height="${kind === "dots" ? 3 : kind === "hatch" ? 4 : 2}" patternUnits="userSpaceOnUse">${(kind === "check" ? [[0, 0], [1, 1]] : kind === "dots" ? [[1, 1]] : [[0, 3], [1, 2], [2, 1], [3, 0], [0, 0]]).map(([x, y]) => `<rect x="${x}" y="${y}" width="1" height="1" fill="currentColor"/>`).join("")}</pattern></defs>` : ""}`}</svg>${esc(lab)}</span>`).join("");
  }
  /* one tooltip a chart, under the pointer or the focused mark: the value first */
  function tipper(plot) {
    const tip = document.createElement("div");
    tip.className = "tip";
    tip.hidden = true;
    plot.appendChild(tip);
    return {
      show(html, x, y) {
        tip.innerHTML = html; tip.hidden = false;
        const w = plot.clientWidth, tw = tip.offsetWidth;
        tip.style.left = Math.max(0, Math.min(w - tw, x - tw / 2)) + "px";
        tip.style.top = Math.max(0, y - tip.offsetHeight - 8) + "px";
      },
      hide() { tip.hidden = true; },
    };
  }

  // ------------------------------------------------------------------------------------------ the tiles
  const tot = SUITE.reduce((a, r) => [a[0] + r[1], a[1] + r[2], a[2] + r[3], a[3] + r[4]], [0, 0, 0, 0]);
  const tiles = [["Cases", tot.reduce((a, b) => a + b, 0)], ["PASS", tot[0]], ["Known gaps", tot[2]], ["INFO", tot[3]], ["FAIL", tot[1]]];
  $("made-tiles").innerHTML = tiles.map(([l, v], i) => `<div class="tile"><span class="label">${l}</span><b data-v="${v}">${v}</b></div>`).join("");
  const still = matchMedia("(prefers-reduced-motion: reduce)").matches;
  function count(b) {                               /* 0 to its value in stepped frames, as the screen counts */
    const v = +b.dataset.v, t0 = performance.now(), T = 700 + Math.min(500, v);
    const f = (t) => { const p = Math.min(1, (t - t0) / T); b.textContent = String(Math.round(v * (1 - Math.pow(1 - p, 3)))); if (p < 1) requestAnimationFrame(f); };
    requestAnimationFrame(f);
  }
  if (!still && "IntersectionObserver" in window) {
    const bs = [...document.querySelectorAll("#made-tiles b[data-v]")];
    bs.forEach((b) => { b.textContent = "0"; });
    const io = new IntersectionObserver((es) => { if (es.some((e) => e.isIntersecting)) { bs.forEach(count); io.disconnect(); } }, { threshold: 0.4 });
    io.observe($("made-tiles"));
  }

  // ------------------------------------------------------------------------------------- the suite chart
  function suite() {
    const plot = $("ch-suite-p");
    plot.innerHTML = "";
    const W = Math.max(280, plot.clientWidth), narrow = W < 560;
    const rows = SUITE.map((r) => ({ g: r[0], v: [r[1], r[2], r[3], r[4]], n: r[1] + r[2] + r[3] + r[4] })).sort((a, b) => b.n - a.n);
    const LW = narrow ? 128 : 190, RW = 36, BH = 10, RH = 20, top = 4, H = top + rows.length * RH + 22;
    const max = Math.max(...rows.map((r) => r.n)), sx = (W - LW - RW) / max;
    const svg = svgEl("svg", { width: W, height: H, viewBox: `0 0 ${W} ${H}`, role: "img", "aria-label": "The suite's cases by group and result; the table under it has the numbers" }, plot);
    defs(svg);
    const tip = tipper(plot);
    for (const t of [0, 25, 50, 75, 100, 125]) {
      if (t > max) break;
      const x = LW + t * sx;
      svgEl("rect", { x, y: top, width: 1, height: rows.length * RH, fill: "currentColor", opacity: 0.18 }, svg);
      text(svg, x, H - 6, String(t), { "text-anchor": "middle", class: "ax" });
    }
    rows.forEach((r, i) => {
      const y = top + i * RH, g = svgEl("g", { class: "mk" }, svg);   /* (the table under it is the keyboard's way in) */
      svgEl("rect", { x: 0, y, width: W, height: RH, fill: "transparent" }, g);
      text(g, LW - 8, y + RH / 2 + 4, narrow && GNAME[r.g].length > 20 ? GNAME[r.g].slice(0, 19) + "…" : GNAME[r.g], { "text-anchor": "end" });
      let x = LW;
      ST.forEach(([, kind], j) => {
        const v = r.v[j];
        if (!v) return;
        const w = Math.max(1, v * sx - (x > LW ? 2 : 0));
        svgEl("rect", { x: x + (x > LW ? 2 : 0), y: y + (RH - BH) / 2, width: w, height: BH, fill: fillOf(kind), class: "bar", style: `--i:${i}` }, g);
        x += v * sx;
      });
      text(g, x + 6, y + RH / 2 + 4, String(r.n), { class: "val" });
      const show = () => {
        const parts = ST.map(([s], j) => (r.v[j] ? `<span>${r.v[j]} ${s}</span>` : "")).join("");
        tip.show(`<b>${r.n} cases</b><span>${esc(GNAME[r.g])}</span>${parts}`, Math.min(W - 60, LW + r.n * sx / 2), y);
      };
      g.addEventListener("pointerenter", show);
      g.addEventListener("pointerleave", () => tip.hide());
    });
  }
  legend($("ch-suite-l"), ST.map(([s, k]) => [ST_TEXT[s], k]));
  $("ch-suite-t").innerHTML = `<table><thead><tr><th>Group</th><th>Cases</th><th>PASS</th><th>XFAIL</th><th>INFO</th><th>FAIL</th></tr></thead><tbody>${SUITE.map((r) =>
    `<tr><td>${esc(GNAME[r[0]])} <code>${r[0]}</code></td><td>${r[1] + r[2] + r[3] + r[4]}</td><td>${r[1]}</td><td>${r[3]}</td><td>${r[4]}</td><td>${r[2]}</td></tr>`).join("")}</tbody></table>`;

  // ---------------------------------------------------------------------------------- the DIST ceilings
  function dist() {
    const plot = $("ch-dist-p");
    plot.innerHTML = "";
    const W = Math.max(280, plot.clientWidth), LW = 84, RH = 22, BH = 12, top = 4, H = top + DIST.length * RH + 22;
    const lo = -12, hi = 3, sx = (W - LW - 16) / (hi - lo), x0 = LW + -lo * sx;
    const svg = svgEl("svg", { width: W, height: H, viewBox: `0 0 ${W} ${H}`, role: "img", "aria-label": "Each machine's clip ceiling against GND-SIN's, in dB; the table has the numbers" }, plot);
    const tip = tipper(plot);
    for (const t of [-12, -9, -6, -3, 0, 3]) {
      const x = LW + (t - lo) * sx;
      svgEl("rect", { x, y: top, width: 1, height: DIST.length * RH, fill: "currentColor", opacity: t === 0 ? 0.9 : 0.18 }, svg);
      text(svg, x, H - 6, t === 0 ? "0 dB" : minus(t, 0), { "text-anchor": "middle", class: "ax" });
    }
    DIST.forEach(([name, v], i) => {
      const y = top + i * RH, g = svgEl("g", { tabindex: 0, class: "mk", "aria-label": `${name}: ${minus(v, 2)} dB` }, svg);
      svgEl("rect", { x: 0, y, width: W, height: RH, fill: "transparent" }, g);
      text(g, LW - 8, y + RH / 2 + 4, name, { "text-anchor": "end" });
      const w = Math.abs(v) * sx, x = v < 0 ? x0 - w : x0;
      if (v) svgEl("rect", { x, y: y + (RH - BH) / 2, width: Math.max(1, w), height: BH, fill: "currentColor", class: v < 0 ? "bar neg" : "bar", style: `--i:${i}` }, g);
      else svgEl("rect", { x: x0 - 1, y: y + (RH - BH) / 2, width: 3, height: BH, fill: "currentColor" }, g);
      const lab = v ? minus(v, Math.abs(v) % 1 ? (Math.abs(v * 10) % 1 ? 2 : 1) : 1) : "0";
      const inside = v < 0 && x - LW < 40;             /* no room at the tip: the value inside the bar, in paper */
      text(g, inside ? x + 6 : v < 0 ? x - 6 : x + Math.max(3, w) + 6, y + RH / 2 + 4, lab,
        { "text-anchor": v < 0 && !inside ? "end" : "start", class: "val", ...(inside ? { fill: "var(--paper)" } : {}) });
      const show = () => tip.show(`<b>${minus(v, 2)} dB</b><span>${esc(name)}'s clip against GND-SIN's</span>`, x0 + (v < 0 ? -w / 2 : w / 2), y);
      g.addEventListener("pointerenter", show); g.addEventListener("focus", show);
      g.addEventListener("pointerleave", () => tip.hide()); g.addEventListener("blur", () => tip.hide());
    });
  }
  $("ch-dist-t").innerHTML = `<table><thead><tr><th>Machine</th><th>Ceiling against GND-SIN's</th></tr></thead><tbody>${DIST.map(([n, v]) => `<tr><td>${n}</td><td>${minus(v, 2)} dB</td></tr>`).join("")}</tbody></table>`;

  const all = () => { suite(); dist(); };
  all();
  let rz = 0, lastW = window.innerWidth;
  window.addEventListener("resize", () => { if (window.innerWidth === lastW) return; lastW = window.innerWidth; clearTimeout(rz); rz = setTimeout(all, 150); });
})();
