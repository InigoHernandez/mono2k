// SPDX-License-Identifier: GPL-3.0-only
// "How to play": the FM-1 in outline (panel.js's layout) that stays put while the list of every control, page, layer
// and mode scrolls beside it (help.js). Pointing at a control on the drawing or at a line of the list lights both, puts
// its text in the box under the drawing and plays what it does on the drawing's screen: a copy of the firmware of its
// own (screens.js), never the emulator's.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const H = window.M2K_HELP, P = window.M2K_PANEL, S = window.M2K_SCREENS;
  const NS = "http://www.w3.org/2000/svg";
  const svg = $("gpanel"), lcd = $("glcd"), list = $("glist");
  if (!svg || !list) return;
  const el = (tag, attrs, parent = svg) => { const e = document.createElementNS(NS, tag); for (const k in attrs) e.setAttribute(k, attrs[k]); parent.appendChild(e); return e; };
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

  // ------------------------------------------------------------------------------------------ the drawing
  const ctl = {};                                    /* data-c -> its group */
  function drawing() {
    svg.innerHTML = "";
    const defs = el("defs", {});
    const pat = el("pattern", { id: "gHi", width: 2, height: 2, patternUnits: "userSpaceOnUse" }, defs);
    el("rect", { width: 1, height: 1, fill: "currentColor" }, pat);
    el("rect", { x: 1, y: 1, width: 1, height: 1, fill: "currentColor" }, pat);
    const L = { fill: "none", stroke: "currentColor", "stroke-width": 1.5 };
    const T = (x, y, s, size, p, a = {}) => Object.assign(el("text", { x, y, "font-size": size, "text-anchor": "middle", "font-weight": 700, "letter-spacing": "0.04em", fill: "currentColor", ...a }, p), { textContent: s });
    el("rect", { x: 22, y: 22, width: 860, height: 522, rx: 38, ...L, "stroke-width": 2 });
    el("rect", { x: 484, y: 162, width: 338, height: 124, rx: 8, ...L, "stroke-width": 1, "stroke-dasharray": "2 3" });
    el("rect", { x: 77, y: 243, width: 127, height: 36, rx: 6, ...L, "stroke-width": 1, "stroke-dasharray": "2 3" });
    el("rect", { x: 52, y: 306, width: 800, height: 198, rx: 20, ...L, "stroke-width": 1, "stroke-dasharray": "2 3" });
    const g = (id) => { const e = el("g", { "data-c": id, class: "gc" }); ctl[id] = e; return e; };
    const sc = g("lcd");
    el("rect", { x: 233, y: 45, width: 229, height: 229, rx: 18, ...L, class: "gc__hit" }, sc);
    for (let k = 0; k < 27; k++) {
      const r = P.KEYR[k], kg = g("k" + k);
      el("rect", { x: r[0], y: r[1], width: r[2], height: r[3], rx: 6, ...L, class: "gc__hit" }, kg);
      if (P.isBlack(k)) T(r[0] + 21, r[1] + 46, P.BLACK_ROLE[H.BLACK.indexOf(k)], 11, kg, { class: "gc__t" });
    }
    for (let i = 0; i < 14; i++) {
      const r = P.BTNR[i], bg = g("b" + i), oct = i >= 12;
      el("rect", { x: r[0], y: r[1], width: r[2], height: r[3], rx: 3, ...L, class: "gc__hit" }, bg);
      T(r[0] + r[2] / 2, r[1] + (oct ? 15 : 22), P.BTN[i], oct ? 9 : 8.5, bg, { class: "gc__t" });
      if (!oct && P.ROLE[i]) T(r[0] + r[2] / 2, r[1] - 5, P.ROLE[i], 7, svg, { "font-weight": 900 });
    }
    for (let i = 0; i < 8; i++) {
      const cx = P.ENC_CX[i], cy = P.ENC_CY[i], eg = g("e" + i);
      el("circle", { cx, cy, r: 21, ...L, class: "gc__hit" }, eg);
      el("line", { x1: cx, y1: cy - 10, x2: cx, y2: cy - 19, stroke: "currentColor", "stroke-width": 3 }, eg);
      T(cx, cy - 30, P.ENC[i], 7.5, svg, { "font-weight": 800 });
    }
    /* the frame that glides from what was lit to what is (light) */
    mark = el("rect", { class: "gmark", rx: 8, fill: "none", stroke: "currentColor", "stroke-width": 2, "stroke-dasharray": "3 3", opacity: 0 });
    box0 = null;
  }
  let mark = null, box0 = null, tw = 0;
  function glide(ids) {
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (const id of ids) {
      const e = ctl[id];
      if (!e) continue;
      const b = e.getBBox();
      x0 = Math.min(x0, b.x); y0 = Math.min(y0, b.y); x1 = Math.max(x1, b.x + b.width); y1 = Math.max(y1, b.y + b.height);
    }
    if (x0 > x1 || !mark) return;
    const to = [x0 - 6, y0 - 6, x1 - x0 + 12, y1 - y0 + 12], from = box0 || to, t0 = performance.now();
    box0 = to;
    const set = (b) => { mark.setAttribute("x", b[0]); mark.setAttribute("y", b[1]); mark.setAttribute("width", b[2]); mark.setAttribute("height", b[3]); };
    mark.setAttribute("opacity", 1);
    cancelAnimationFrame(tw);
    if (!S.motion()) { set(to); return; }
    const step = (t) => {                          /* eased, over a quarter of a second */
      const p = Math.min(1, (t - t0) / 240), e = 1 - Math.pow(1 - p, 3);
      set(from.map((v, i) => v + (to[i] - v) * e));
      if (p < 1) tw = requestAnimationFrame(step);
    };
    tw = requestAnimationFrame(step);
  }

  // --------------------------------------------------------------------------------------------- the list
  const byId = {};
  const groups = [];
  for (const it of H.G) {
    byId[it.id] = it;
    let gr = groups.find((x) => x.name === it.g);
    if (!gr) groups.push((gr = { name: it.g, items: [] }));
    gr.items.push(it);
  }
  const isCtl = (id) => /^[ebk]/.test(id) || id === "lcd";
  const whereOf = (it) => (it.where ? (isCtl(it.id) ? "Computer: " : "On the panel: ") + it.where : "");
  /* the list is the names alone, grouped; what each does comes up in the box under the drawing (its short line the
   * name's tooltip) */
  list.innerHTML = groups.map((gr) => `<section class="gl"><h3 class="label">${esc(gr.name)}</h3><ul>${gr.items.map((it) =>
    `<li data-g="${it.id}"><button type="button" class="gl__item" aria-pressed="false" aria-controls="gbox" title="${esc(it.short)}">${esc(it.tag)}</button></li>`).join("")}</ul></section>`).join("");
  const items = {};
  list.querySelectorAll("li[data-g]").forEach((li) => { items[li.dataset.g] = li; });

  // ------------------------------------------------------------------------------------------- selecting
  /* a control on the drawing -> its line in the list */
  const lineOf = (c) => (c[0] === "e" ? (+c.slice(1) >= 3 && +c.slice(1) <= 6 ? "eK" : c) : c === "b12" || c === "b13" ? "bO"
    : c[0] === "k" ? (P.isBlack(+c.slice(1)) ? "kb" : "kw") : c);
  let cur = "", curScreen = null, player = null, want = null;
  function light(ids) {
    for (const id in ctl) ctl[id].classList.toggle("on", ids.includes(id));
    glide(ids);
  }
  function select(id, from) {
    const key = from && from[0] === "k" ? from : "";
    const it = byId[id];
    if (!it || cur === id + key) return;
    cur = id + key;
    const k = key ? H.key(+key.slice(1)) : null;
    $("gbox-t").textContent = k ? k.tag : it.tag;
    $("gbox-p").textContent = k ? k.long : it.long;
    $("gbox-w").textContent = k ? "Computer: " + k.where : whereOf(it);
    light(k ? [key] : it.hi);
    for (const g in items) {
      const on = g === id;
      items[g].classList.toggle("on", on);
      items[g].firstChild.setAttribute("aria-pressed", String(on));
    }
    screen(it.screen);
  }
  /* the drawing's screen: the entry's script on the guide's own core, then on in time while the guide is in view */
  function screen(script) {
    want = script;
    if (!player) return;
    if (curScreen === script) return;
    curScreen = script;
    S.draw(lcd, player.play(script));
  }
  list.addEventListener("pointerover", (ev) => { const li = ev.target.closest && ev.target.closest("li[data-g]"); if (li) select(li.dataset.g); });
  list.addEventListener("focusin", (ev) => { const li = ev.target.closest && ev.target.closest("li[data-g]"); if (li) select(li.dataset.g); });
  list.addEventListener("click", (ev) => { const li = ev.target.closest && ev.target.closest("li[data-g]"); if (li) select(li.dataset.g); });
  svg.addEventListener("pointerover", (ev) => {
    const g = ev.target.closest && ev.target.closest("[data-c]");
    if (g) select(lineOf(g.dataset.c), g.dataset.c);
  });
  lcd.addEventListener("pointerenter", () => select("lcd"));

  // ------------------------------------------------------------------------------------------ in view
  let live = false, last = 0;
  function tick(t) {
    const dt = Math.min(100, t - last);
    last = t;
    if (live && player && S.motion() && !document.hidden) { player.step(dt); S.draw(lcd, player.frame()); }
    requestAnimationFrame(tick);
  }
  async function wake() {
    if (player) return;
    player = await S.player();
    curScreen = null;
    screen(want || "w 300");
    requestAnimationFrame(tick);
  }
  const sec = $("guide");
  if ("IntersectionObserver" in window) {
    new IntersectionObserver((es) => { for (const e of es) { live = e.isIntersecting; if (live) wake(); } }, { rootMargin: "200px" }).observe(sec);
  } else { live = true; wake(); }
  window.addEventListener("m2k-theme", () => { if (player) { player.theme(); player.step(1); S.draw(lcd, player.frame()); } });
  drawing();
})();
