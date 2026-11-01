// SPDX-License-Identifier: GPL-3.0-only
// The device switch in every page's header, on both sites (style.css, .seg): FM-1 | MOVE, the device in view under a
// block of ink. Picked, the ink slides across (stepped, as the screen's transitions) and the page goes to the same page
// on the other device: home, manual, installer; a manual's page where both manuals have one. The links work without it.
(() => {
  "use strict";
  /* a manual's page and the other manual's nearest */
  const PAGE = {
    introduction: "introduction", status: "status", "the-panel": "controls", controls: "the-panel", pages: "pages",
    tracks: "tracks", machines: "machines", steps: "steps", locks: "steps", "trig-types": "steps", conditions: "steps",
    recording: "steps", projects: "saving", saving: "projects", credits: "credits", changelog: "changelog",
  };
  const still = () => matchMedia("(prefers-reduced-motion: reduce)").matches;
  const segs = [...document.querySelectorAll("nav .seg")];
  const home = (seg) => Math.max(0, [...seg.querySelectorAll(".seg__o")].findIndex((a) => a.getAttribute("aria-current") === "page"));
  const reset = () => segs.forEach((seg) => { seg.classList.remove("go"); seg.style.setProperty("--on", home(seg)); });
  segs.forEach((seg) => {
    const opts = [...seg.querySelectorAll(".seg__o")], cur = home(seg);
    seg.style.setProperty("--n", opts.length);
    opts.forEach((a, i) => a.addEventListener("click", (e) => {
      if (i === cur) { e.preventDefault(); return; }
      const h = decodeURIComponent(location.hash.slice(1));
      if (h && PAGE[h] && /manual\.html$/.test(new URL(a.href).pathname)) a.hash = PAGE[h];
      if (e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey || still()) return;   /* a new tab, or at once */
      e.preventDefault();
      seg.style.setProperty("--on", i);
      seg.classList.add("go");
      setTimeout(() => { location.href = a.href; }, 230);
    }));
  });
  /* a page brought back from the browser's cache comes back with its ink on its own device */
  window.addEventListener("pageshow", (e) => { if (e.persisted) reset(); });
  reset();
})();
