// SPDX-License-Identifier: GPL-3.0-only
// The pages around the emulator (the manual, the installer, the patch editor, and the Move's pages): the theme switch,
// as the emulator's (Auto, Light, Dark, remembered under the same key), a "m2k-theme" event for pages that draw in the
// theme, the dithered textures, and on a page of sections the reveal as they scroll in.
(() => {
  "use strict";
  const THEMES = ["auto", "light", "dark"];
  const btn = document.getElementById("theme");
  let theme = "auto";
  try { theme = localStorage.getItem("mono2k.theme") || "auto"; } catch (e) { theme = "auto"; }
  const label = () => { if (btn) btn.textContent = theme === "auto" ? "Auto" : theme === "light" ? "Light" : "Dark"; };
  function apply() {
    if (theme === "auto") document.documentElement.removeAttribute("data-theme");
    else document.documentElement.setAttribute("data-theme", theme);
    try { if (theme === "auto") localStorage.removeItem("mono2k.theme"); else localStorage.setItem("mono2k.theme", theme); } catch (e) { /* private mode */ }
    label();
    window.dispatchEvent(new Event("m2k-theme"));
  }
  /* the dithered textures the style sheet draws with (as app.js makes them on the emulator page), in the theme's ink */
  const BAYER = (() => {
    const b = (n) => {
      if (n === 1) return [[0]];
      const q = b(n / 2), m = n / 2, r = [];
      for (let y = 0; y < n; y++) { r.push([]); for (let x = 0; x < n; x++) r[y].push(4 * q[y % m][x % m] + [[0, 2], [3, 1]][(y / m) | 0][(x / m) | 0]); }
      return r;
    };
    return b(8);
  })();
  function textures() {
    const cs = getComputedStyle(document.documentElement);
    const ink = cs.getPropertyValue("--ink").trim() || "#0b0b0b", paper = cs.getPropertyValue("--paper").trim() || "#f2f1ec";
    const make = (w, h, f, col = ink) => {
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
    r.setProperty("--g-across", make(256, 8, (x, y) => BAYER[y & 7][x & 7] < 64 - x / 4));
    r.setProperty("--g-down", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < 64 - y));
    r.setProperty("--g-down-inv", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < 64 - y, paper));
    r.setProperty("--g-up", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < y));
    r.setProperty("--d-check-p", make(2, 2, (x, y) => (x + y) % 2 === 0, paper));   /* the checker in paper, on ink */
    /* the reveal's masks (as app.js's): solid, then the screen's dither falling away, then nothing */
    const ramp = (i, b) => i < 32 || (i < 64 && b < (64 - i) * 2);
    r.setProperty("--m-wipe", make(96, 8, (x, y) => ramp(x, BAYER[y & 7][x & 7])));
    r.setProperty("--m-scan", make(8, 96, (x, y) => ramp(y, BAYER[y & 7][x & 7])));
  }
  window.addEventListener("m2k-theme", textures);
  textures();
  if (btn) btn.addEventListener("click", () => { theme = THEMES[(THEMES.indexOf(theme) + 1) % THEMES.length]; apply(); });
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => { if (theme === "auto") window.dispatchEvent(new Event("m2k-theme")); });
  /* an accordion a person opens deals its contents in (style.css, gl-deal); one opened by the page does not */
  document.addEventListener("click", (e) => { const s = e.target.closest && e.target.closest("details > summary"); if (s) s.parentElement.classList.add("gl-dealt"); });
  label();
  /* a page of sections (the Move's home): each part arrives once as it comes into view, as on the emulator's page
   * (app.js's reveal); nothing moves for those who ask for less motion */
  if (document.querySelector(".sec") && "IntersectionObserver" in window && !matchMedia("(prefers-reduced-motion: reduce)").matches) {
    document.documentElement.classList.add("reveal");
    const io = new IntersectionObserver((es) => {
      for (const e of es) if (e.isIntersecting) {
        const t = e.target;
        t.classList.add("in");
        io.unobserve(t);
        if (t.matches(".wipe, .scan")) setTimeout(() => t.classList.add("done"), 1600);
      }
    }, { rootMargin: "0px 0px -10% 0px", threshold: 0 });
    const watch = (sel, cls) => document.querySelectorAll(sel).forEach((el) => {
      if (cls) el.classList.add(cls);
      const sib = el.parentElement ? [...el.parentElement.children].filter((c) => c.matches(sel)) : [];
      el.style.setProperty("--i", String(Math.max(0, sib.indexOf(el))));
      io.observe(el);
    });
    watch(".sec");
    watch(".sec__n, .hero__sub > *, .sec__body .lede, .lbls > span, .made__facts > li", "rv-up");
    watch(".sec__body .display", "wipe");
    watch(".wipe, .scan");
  }
  window.M2K_SITE = { dark: () => { const t = document.documentElement.getAttribute("data-theme"); return t ? t === "dark" : matchMedia("(prefers-color-scheme: dark)").matches; } };
})();
