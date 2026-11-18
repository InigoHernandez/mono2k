// SPDX-License-Identifier: GPL-3.0-only
// The components' small behaviours, on every page of both sites (style.css has their look): the shared tooltip
// ([data-tip]), toasts (M2K_UI.toast), copy buttons ([data-copy]: the label morphs, a check draws itself), numbers
// that pop in when they change ([data-pop]), an error that shakes (.inst__log.err, M2K_UI.shake), a place a script
// fills (.skel), the border beam on the main call to action (.beam), and the screens and pictures coming in as a
// mosaic of their own pixels. The motion is transitions.dev's (Jakub Antalik; free to use, the library not
// redistributed: each piece marked where it is used) and two ideas of Libraries.dev's (MIT: border-beam, img-fx),
// rewritten here in a few lines, in one bit. Nothing moves for those who ask for less motion.
window.M2K_UI = (() => {
  "use strict";
  const still = () => matchMedia("(prefers-reduced-motion: reduce)").matches;
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
  const ms = (name, fb) => { const v = parseFloat(getComputedStyle(document.documentElement).getPropertyValue(name)); return Number.isFinite(v) ? v : fb; };

  // ------------------------------------------------------------------------------------------------ tooltip
  /* transitions.dev — Tooltip open/close: one bubble for every [data-tip], above its trigger (below where there is no
   * room); it waits a beat before it shows, leaves at once, and travels from one trigger to the next while it shows */
  let tt = null, ttFor = null;
  function tipEl() {
    if (tt) return tt;
    tt = document.createElement("div");
    tt.className = "tt"; tt.id = "m2k-tt"; tt.setAttribute("role", "tooltip"); tt.setAttribute("aria-hidden", "true");
    document.body.appendChild(tt);
    return tt;
  }
  function tipShow(trg) {
    const t = tipEl(), showing = t.dataset.show === "true";
    t.textContent = trg.dataset.tip;
    const r = trg.getBoundingClientRect(), w = t.offsetWidth, h = t.offsetHeight;
    const x = Math.round(Math.max(8, Math.min(innerWidth - w - 8, r.left + r.width / 2 - w / 2)) + scrollX);
    const below = r.top - h - 8 < 0;
    const y = Math.round((below ? r.bottom + 8 : r.top - h - 8) + scrollY);
    t.toggleAttribute("data-below", below);
    if (!showing) { t.style.transition = "none"; t.style.setProperty("--tt-x", x + "px"); t.style.setProperty("--tt-y", y + "px"); void t.offsetWidth; t.style.transition = ""; }
    else { t.style.setProperty("--tt-x", x + "px"); t.style.setProperty("--tt-y", y + "px"); }
    t.dataset.show = "true"; t.setAttribute("aria-hidden", "false");
    if (ttFor && ttFor !== trg) ttFor.removeAttribute("aria-describedby");
    trg.setAttribute("aria-describedby", t.id);
    ttFor = trg;
  }
  function tipHide() {
    if (!tt) return;
    tt.dataset.show = "false"; tt.setAttribute("aria-hidden", "true");
    if (ttFor) { ttFor.removeAttribute("aria-describedby"); ttFor = null; }
  }
  document.addEventListener("pointerover", (e) => { const t = e.target.closest && e.target.closest("[data-tip]"); if (t) tipShow(t); });
  document.addEventListener("pointerout", (e) => { const t = e.target.closest && e.target.closest("[data-tip]"); if (t && !t.contains(e.relatedTarget)) tipHide(); });
  document.addEventListener("focusin", (e) => { const t = e.target.closest && e.target.closest("[data-tip]"); if (t && t.matches(":focus-visible")) tipShow(t); });
  document.addEventListener("focusout", tipHide);
  document.addEventListener("keydown", (e) => { if (e.key === "Escape") tipHide(); });
  addEventListener("scroll", tipHide, { passive: true });

  // -------------------------------------------------------------------------------------------------- toast
  /* transitions.dev — Toast open / close: rises in, leaves faster; the words read out too (a polite live region) */
  let toasts = null;
  function toast(text, hold = 2600) {
    if (!toasts) {
      toasts = document.createElement("div");
      toasts.className = "toasts"; toasts.setAttribute("role", "status"); toasts.setAttribute("aria-live", "polite");
      document.body.appendChild(toasts);
    }
    const t = document.createElement("div");
    t.className = "toast";
    t.textContent = text;
    toasts.appendChild(t);
    void t.offsetWidth;
    t.classList.add("is-open");
    setTimeout(() => { t.classList.remove("is-open"); setTimeout(() => t.remove(), still() ? 0 : ms("--duration-medium", 350)); }, hold);
  }

  // ------------------------------------------------------------------------------------------- text morph
  /* transitions.dev — Text morph: the letters both labels share stay, the part that differs swaps (here wiped, one
   * bit, where the original blurs) while its width eases */
  function morph(el, next) {
    const prev = el.dataset.text ?? el.textContent;
    if (prev === next) return;
    el.dataset.text = next;
    clearTimeout(el._morphTimer);
    if (still()) { el.textContent = next; return; }
    let a = 0;
    while (a < prev.length && a < next.length && prev[a] === next[a]) a++;
    let b = 0;
    while (b < prev.length - a && b < next.length - a && prev[prev.length - 1 - b] === next[next.length - 1 - b]) b++;
    const swap = document.createElement("span");
    swap.className = "t-morph-swap";
    const out = document.createElement("span");
    out.className = "t-morph-old"; out.setAttribute("aria-hidden", "true"); out.textContent = prev.slice(a, prev.length - b);
    const inn = document.createElement("span");
    inn.className = "t-morph-new"; inn.textContent = next.slice(a, next.length - b);
    swap.append(out, inn);
    el.textContent = "";
    el.append(next.slice(0, a), swap, next.slice(next.length - b));
    swap.style.width = out.getBoundingClientRect().width + "px";
    void swap.offsetWidth;
    swap.classList.add("is-morphing");
    swap.style.width = inn.getBoundingClientRect().width + "px";
    el._morphTimer = setTimeout(() => { el.textContent = next; }, 280);
  }

  // -------------------------------------------------------------------------------------------------- copy
  /* a [data-copy] button copies its value (or the code beside it); its label goes Copy -> Copied and a check draws
   * itself (transitions.dev — Success check), then it comes back */
  const CHECK = '<span class="t-success-check" data-state="out" aria-hidden="true"><svg viewBox="0 0 14 14"><path d="M2 7.5 5.5 11 12 3.5"/></svg></span>';
  document.addEventListener("click", async (e) => {
    const btn = e.target.closest && e.target.closest("[data-copy]");
    if (!btn) return;
    const box = btn.closest(".code"), code = box && box.querySelector("code");
    const text = btn.dataset.copy || (code ? code.textContent : "");
    let ok = true;
    try { await navigator.clipboard.writeText(text); } catch (err) {
      ok = false;
      if (code) { const r = document.createRange(); r.selectNodeContents(code); const s = getSelection(); s.removeAllRanges(); s.addRange(r); }
    }
    const lab = btn.querySelector(".t-morph") || btn;
    let ic = btn.querySelector(".t-success-check");
    if (!ic) { btn.insertAdjacentHTML("afterbegin", CHECK); ic = btn.querySelector(".t-success-check"); }
    const prev = btn.dataset.label || (btn.dataset.label = lab.dataset.text ?? lab.textContent);
    morph(lab, ok ? "Copied" : "Selected");
    if (ok) { ic.dataset.state = "out"; void ic.offsetWidth; ic.dataset.state = "in"; ic.hidden = false; }
    clearTimeout(btn._back);
    btn._back = setTimeout(() => { morph(lab, prev); ic.hidden = true; ic.dataset.state = "out"; }, 1800);
  });

  // ------------------------------------------------------------------------------------------- number pop-in
  /* transitions.dev — Number pop-in: a [data-pop] number that changes swaps only its changed digits, the old leaving
   * upward, the new rising after it with a small spring (the editor's pattern, its octave) */
  function pop(el, str) {
    const cur = el._popCur ?? el.textContent;
    el._popCur = str;
    const quiet = (f) => { if (el._popMo) el._popMo.disconnect(); f(); if (el._popMo) { el._popMo.takeRecords(); el._popMo.observe(el, POPOPT); } };
    if (still() || cur === str) { quiet(() => { el.textContent = str; }); return; }
    let same = 0;
    while (same < cur.length && same < str.length && cur[same] === str[same]) same++;
    const g = document.createElement("span");
    g.className = "t-digit-group";
    [...str].forEach((ch, i) => {
      const fresh = document.createElement("span");
      fresh.className = "t-digit"; fresh.textContent = ch;
      if (i < same) { fresh.dataset.same = ""; g.appendChild(fresh); return; }
      const slot = document.createElement("span");
      slot.className = "t-digit-slot";
      const fromEnd = str.length - 1 - i;
      if (fromEnd === 1) slot.dataset.stagger = "1"; else if (fromEnd === 0) slot.dataset.stagger = "2";
      const oldCh = cur[cur.length - (str.length - i)];
      if (oldCh) { const o = document.createElement("span"); o.className = "t-digit-old"; o.setAttribute("aria-hidden", "true"); o.dataset.ch = oldCh; slot.appendChild(o); }   /* (drawn by CSS: the text stays the new number) */
      slot.appendChild(fresh);
      g.appendChild(slot);
    });
    quiet(() => el.replaceChildren(g));
    void g.offsetHeight;
    g.classList.add("is-animating");
    el._popG = g;
    clearTimeout(el._popT);
    el._popT = setTimeout(() => { el._popG = null; quiet(() => { el.textContent = str; }); }, 800);
  }
  const POPOPT = { childList: true, characterData: true, subtree: true };
  $$("[data-pop]").forEach((el) => {
    el._popCur = el.textContent;
    el._popMo = new MutationObserver(() => {
      const now = el.textContent;
      if (now !== el._popCur) pop(el, now);
      else if (el._popG && !el.contains(el._popG)) {          /* the same number written again mid-pop: the pop goes on */
        el._popMo.disconnect(); el.replaceChildren(el._popG); el._popMo.takeRecords(); el._popMo.observe(el, POPOPT);
      }
    });
    el._popMo.observe(el, POPOPT);
  });

  // -------------------------------------------------------------------------------------------- text swap
  /* transitions.dev — Text states swap, its enter half: a label that changes in place (the theme's: Auto, Light, Dark)
   * rises the last 4 px into place */
  function swapIn(el) {
    if (still()) return;
    el.classList.add("t-text-swap", "is-enter-start");
    void el.offsetWidth;
    el.classList.remove("is-enter-start");
  }
  /* the words are written by the page's scripts as plain text (the theme's by site.js / app.js, the installer's
   * buttons' second lines by its own): wrapped here in a span of their own, so the words move and the button stays */
  function watchSwap(el) {
    const mo = new MutationObserver(() => {
      if (el.querySelector(".t-text-swap")) return;
      mo.disconnect();
      const s = document.createElement("span");
      s.textContent = el.textContent;
      el.replaceChildren(s);
      swapIn(s);
      mo.takeRecords();
      mo.observe(el, { childList: true });
    });
    mo.observe(el, { childList: true });
  }
  $$("#theme, [data-swap]").forEach(watchSwap);

  // ---------------------------------------------------------------------------------------------- the shake
  /* transitions.dev — Error state shake: replayed whenever an error comes in (remove, reflow, add) */
  function shake(el) {
    if (!el || still()) return;
    el.classList.remove("is-shaking");
    void el.offsetWidth;
    el.classList.add("is-shaking");
    clearTimeout(el._shakeT);
    el._shakeT = setTimeout(() => el.classList.remove("is-shaking"), 300);
  }
  $$(".inst__log").forEach((log) => {
    let was = log.classList.contains("err"), last = log.textContent;
    new MutationObserver(() => {
      const err = log.classList.contains("err"), txt = log.textContent;
      if (err && (!was || txt !== last)) shake(log);
      was = err; last = txt;
    }).observe(log, { attributes: true, attributeFilter: ["class"], childList: true, characterData: true, subtree: true });
  });

  // ----------------------------------------------------------------------------------------------- skeleton
  /* transitions.dev — Skeleton loader and reveal: a .skel place pulses on the dither until its script writes it, then
   * its words come in */
  $$(".skel").forEach((el) => {
    const first = el.textContent;
    const mo = new MutationObserver(() => {
      if (el.textContent === first) return;
      mo.disconnect();
      el.classList.remove("skel");
      if (!still()) el.classList.add("skel-in");
    });
    mo.observe(el, { childList: true, characterData: true, subtree: true });
  });

  // ---------------------------------------------------------------------------------------------- the beam
  /* Libraries.dev's border beam, its idea in one bit (style.css): on while its button can be pressed, paused when
   * it is out of view, never for those who ask for less motion */
  const beams = $$(".beam");
  if (beams.length && !still()) {
    const io = "IntersectionObserver" in window ? new IntersectionObserver((es) => {
      for (const e of es) e.target.toggleAttribute("data-paused", !e.isIntersecting);
    }, { rootMargin: "256px" }) : null;
    const set = (b) => b.classList.toggle("beam-on", !b.disabled && b.getAttribute("aria-disabled") !== "true");
    beams.forEach((b) => {
      set(b);
      new MutationObserver(() => set(b)).observe(b, { attributes: true, attributeFilter: ["disabled", "aria-disabled"] });
      if (io) io.observe(b);
    });
  }

  // --------------------------------------------------------------------------------------------- the mosaic
  /* after Libraries.dev's img-fx (its grid-locked mosaic dissolving into the picture), in one bit: a screen or a
   * picture comes in as its own pixels in blocks, coarse to fine (16, 8, 4, 2, then itself), each block the picture's
   * own two colours by an ordered dither of what it covers; once, as it scrolls in. A Move screen that changes (a pad
   * pointed at) comes in again, quicker. */
  const BAYER4 = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
  const MZ = ".scr canvas, #eng-lcd, .fig canvas[data-scr], img.oled, .mgear-fb img";
  const ready = (el) => el.tagName === "CANVAS" ? el.width > 0 : el.complete && el.naturalWidth > 0;
  function inks(src, W, H) {
    /* the picture's own two colours, its darkest and its lightest, read at its own size */
    const c = document.createElement("canvas"); c.width = W; c.height = H;
    const g = c.getContext("2d", { willReadFrequently: true });
    g.drawImage(src, 0, 0, W, H);
    const p = g.getImageData(0, 0, W, H).data;
    let lo = [255, 255, 255, 765], hi = [0, 0, 0, -1];
    for (let i = 0; i < p.length; i += 4) {
      const l = p[i] + p[i + 1] + p[i + 2];
      if (l < lo[3]) lo = [p[i], p[i + 1], p[i + 2], l];
      if (l > hi[3]) hi = [p[i], p[i + 1], p[i + 2], l];
    }
    return [lo, hi];
  }
  function frameOf(src, W, H, b, [lo, hi]) {
    /* the picture shrunk to its blocks (each block's average), then each block one of the picture's two colours */
    const w = Math.max(1, Math.ceil(W / b)), h = Math.max(1, Math.ceil(H / b));
    const c = document.createElement("canvas"); c.width = w; c.height = h;
    const g = c.getContext("2d", { willReadFrequently: true });
    g.imageSmoothingEnabled = true; g.imageSmoothingQuality = "high";
    g.drawImage(src, 0, 0, w, h);
    const d = g.getImageData(0, 0, w, h), p = d.data;
    const span = Math.max(1, hi[3] - lo[3]);
    for (let y = 0, i = 0; y < h; y++) for (let x = 0; x < w; x++, i += 4) {
      const v = (p[i] + p[i + 1] + p[i + 2] - lo[3]) / span, t = (BAYER4[(y & 3) * 4 + (x & 3)] + 0.5) / 16;
      const c2 = v > t ? hi : lo;
      p[i] = c2[0]; p[i + 1] = c2[1]; p[i + 2] = c2[2]; p[i + 3] = 255;
    }
    g.putImageData(d, 0, 0);
    return c;
  }
  function mosaic(el, sizes, step) {
    if (!ready(el)) { el.classList.remove("mz-wait"); return; }
    const host = el.parentElement;
    if (getComputedStyle(host).position === "static") host.style.position = "relative";
    const cs = getComputedStyle(el);
    const bl = parseFloat(cs.borderLeftWidth) + parseFloat(cs.paddingLeft), bt = parseFloat(cs.borderTopWidth) + parseFloat(cs.paddingTop);
    const bw = el.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight), bh = el.clientHeight - parseFloat(cs.paddingTop) - parseFloat(cs.paddingBottom);
    const W = el.tagName === "CANVAS" ? el.width : el.naturalWidth, H = el.tagName === "CANVAS" ? el.height : el.naturalHeight;
    let ov = el._mz;
    if (!ov) {
      ov = el._mz = document.createElement("canvas");
      ov.className = "mz"; ov.setAttribute("aria-hidden", "true");
      host.appendChild(ov);
    }
    ov.width = W; ov.height = H;
    Object.assign(ov.style, { left: el.offsetLeft + bl + "px", top: el.offsetTop + bt + "px", width: bw + "px", height: bh + "px", borderRadius: cs.borderRadius });
    const g = ov.getContext("2d");
    g.imageSmoothingEnabled = false;
    el.classList.remove("mz-wait");
    let k = 0, two;
    try { two = inks(el, W, H); } catch (e) { ov.remove(); el._mz = null; return; }
    clearTimeout(el._mzT);
    const next = () => {
      if (k >= sizes.length) { ov.remove(); el._mz = null; return; }
      const b = Math.max(1, Math.round(sizes[k++] * W / 240));      /* blocks of the screen's own pixels */
      try { g.drawImage(frameOf(el, W, H, b, two), 0, 0, W, H); } catch (e) { ov.remove(); el._mz = null; return; }
      el._mzT = setTimeout(next, step);
    };
    next();
  }
  if (!still() && "IntersectionObserver" in window) {
    const io = new IntersectionObserver((es) => {
      for (const e of es) if (e.isIntersecting) {
        io.unobserve(e.target);
        const el = e.target;
        const go = () => mosaic(el, [16, 8, 4, 2], ms("--duration-micro", 80));
        if (el.tagName === "IMG" && !ready(el)) el.addEventListener("load", go, { once: true }); else requestAnimationFrame(go);
      }
    }, { rootMargin: "0px 0px -8% 0px" });
    const seen = new WeakSet();
    const watch = () => $$(MZ).forEach((el) => {
      if (seen.has(el)) return;
      seen.add(el);
      const r = el.getBoundingClientRect();
      if (r.top > innerHeight) el.classList.add("mz-wait");          /* below the fold: hidden until its turn */
      if (el.tagName === "IMG") el.addEventListener("error", () => el.classList.remove("mz-wait"), { once: true });   /* (its alt shows) */
      io.observe(el);
      if (el.tagName === "IMG" && el.id) {                             /* a Move screen that a pad changes: again, quicker */
        new MutationObserver(() => { if (!el.classList.contains("mz-wait")) el.addEventListener("load", () => mosaic(el, [8, 4], 40), { once: true }); })
          .observe(el, { attributes: true, attributeFilter: ["src"] });
      }
    });
    watch();
    /* the manual's screens are on pages that open later: look again when the page changes */
    addEventListener("hashchange", () => requestAnimationFrame(watch));
    /* a safety: nothing stays hidden if its turn never comes */
    setTimeout(() => $$(".mz-wait").forEach((el) => { const r = el.getBoundingClientRect(); if (r.bottom < 0 || r.height === 0) el.classList.remove("mz-wait"); }), 4000);
  }

  return { toast, shake, morph, pop, tip: { show: tipShow, hide: tipHide } };
})();
