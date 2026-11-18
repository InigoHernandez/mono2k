// SPDX-License-Identifier: GPL-3.0-only
// The installer's screen while an update runs. The FM-1's flash is drawn as its 256 sectors of 4 KB, in the site's
// dither: the boot (never written), the update loader staged at 0xE0000, then the firmware written from 0x04000 to
// 0x93000 as the loader reads the package, the patterns' area left alone. Beside it, the whole update in one number,
// the four steps, and at the end MONO2K's isotype in the sectors. installer.html feeds it the updater's steps
// (fm1ota.js: start, verify, loader, write, reboot, done); it writes nothing and talks to nothing.

const COLS = 16, CELL = 8, PX = COLS * CELL;            /* 16 x 16 sectors, each 7 x 7 pixels and a pixel apart */
const BOOT = [0x00, 0x04], FW = [0x04, 0x93], KEEP = [0xC0, 0xE0], STG = [0xE0, 0xE5];
const STAGE_READS = 48;          /* the running firmware's reads to stage the loader: the head, the loader, keepalives */
const IN = (s, r) => s >= r[0] && s < r[1];
const BAYER = (() => {
  const b = (n) => {
    if (n === 1) return [[0]];
    const q = b(n / 2), m = n / 2, r = [];
    for (let y = 0; y < n; y++) { r.push([]); for (let x = 0; x < n; x++) r[y].push(4 * q[y % m][x % m] + [[0, 2], [3, 1]][(y / m) | 0][(x / m) | 0]); }
    return r;
  };
  return b(8);
})();
const ISO = [   /* MONO2K's isotype (brand/mono2k-iso.svg), filled into the sectors at the end */
  "M371.177 322.938C346.353 322.511 320.193 322.919 295.267 322.922L295.273 104.054C295.267 99.0664 294.898 83.9219 295.427 79.542L296.438 78.8486C320.811 79.2205 346.759 79.0906 371.157 78.835L371.177 322.938Z",
  "M0.669267 78.8184C25.8069 79.0513 50.9474 79.1065 76.0863 78.9844C76.3341 89.9106 76.2328 101.312 76.1214 112.248C75.4061 182.366 77.1693 252.854 76.0511 322.926L0.211259 322.914L0.239579 136.432C0.240178 128.986 -0.521935 82.3877 0.669267 78.8184Z",
  "M294.993 0.0126953L295.059 50.0684C295.073 58.1959 295.558 70.9773 294.282 78.6768C293.851 81.2748 286.387 92.4032 284.354 95.624C256.977 143.305 237.109 194.92 225.451 248.654C199.163 248.347 172.382 248.585 146.057 248.601C136.258 206.935 122.268 166.368 104.299 127.521C100.06 118.751 95.4768 110.151 90.5609 101.741C74.8422 74.5385 76.2209 83.3897 76.264 51.0928L76.3607 0.0556641L119.721 0.078125C146.678 50.4618 164.631 89.8462 185.732 143.171C204.51 94.0237 226.566 46.1929 251.748 0L294.993 0.0126953Z",
];
const STEPS = ["Check", "Restart", "Write", "Start"];
const fmt = (s) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, "0")}`;
const calm = () => matchMedia("(prefers-reduced-motion: reduce)").matches;

/* the isotype at the sectors' resolution: which of the 256 cells it covers, centred, 14 wide */
function isoCells() {
  const c = document.createElement("canvas"), W = 14, H = 12;
  c.width = W; c.height = H;
  const g = c.getContext("2d");
  g.setTransform(W / 372, 0, 0, H / 323, 0, 0);
  for (const d of ISO) g.fill(new Path2D(d));
  const a = g.getImageData(0, 0, W, H).data, on = new Set();
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (a[(y * W + x) * 4 + 3] > 100) on.add((y + 2) * COLS + x + 1);
  return on;
}

export class Live {
  /* title: "Installing MONO2K 1.9.2"; total: the package's 512-byte reads; mine: MONO2K's own package (the
   * patterns' area marked kept, the isotype at the end); done: what the end says; retry: Try again, when it stops */
  constructor({ title, total, mine = true, done = "", retry = null }) {
    this.title = title; this.total = Math.max(1, total); this.mine = mine; this.doneText = done; this.retry = retry;
    this.lvl = new Float32Array(256);        /* each sector's fill, 0..64, as drawn */
    this.aim = new Float32Array(256);        /* and where it is going */
    this.kind = new Uint8Array(256);         /* 0 free, 1 boot, 2 firmware, 3 kept, 4 staging */
    for (let s = 0; s < 256; s++) this.kind[s] = IN(s, BOOT) ? 1 : IN(s, FW) ? 2 : IN(s, KEEP) && mine ? 3 : IN(s, STG) ? 4 : 0;
    this.stage = -1; this.pct = 0; this.head = -1; this.t0 = 0; this.tStage = 0; this.tWrite = 0; this.nWrite = 0;
    this.state = "run"; this.glitch = 0; this.scan = -1; this.raf = 0; this.calm = calm();
  }

  open() {
    const el = document.createElement("div");
    el.className = "lv";
    el.setAttribute("role", "dialog"); el.setAttribute("aria-modal", "true"); el.setAttribute("aria-labelledby", "lv-t");
    el.tabIndex = -1;
    const legend = [[1, "Boot, never written"], [4, "Update loader"], [2, "Firmware"]].concat(this.mine ? [[3, "Your patterns, kept"]] : []);
    el.innerHTML = `
      <header class="lv__top"><span class="label" id="lv-t"></span><span class="label lv__clock">0:00</span></header>
      <div class="lv__body">
        <figure class="lv__map">
          <div class="lv__grid"><ol class="lv__addr" aria-hidden="true">${Array.from({ length: 16 }, (_, r) => `<li>0x${(r * 0x10000).toString(16).toUpperCase().padStart(5, "0")}</li>`).join("")}</ol>
          <canvas class="lv__cv" aria-hidden="true"></canvas></div>
          <figcaption class="lv__legend">${legend.map(([k, t]) => `<span><canvas data-k="${k}" width="7" height="7"></canvas>${t}</span>`).join("")}</figcaption>
        </figure>
        <div class="lv__side">
          <p class="lv__pct" aria-hidden="true"><b>0</b><span>%</span></p>
          <p class="lv__stage">Check</p>
          <ol class="lv__rail">${STEPS.map((s, i) => `<li><i></i><span>${String(i + 1).padStart(2, "0")}</span>${s}</li>`).join("")}</ol>
          <p class="lv__say" role="status" aria-live="polite"></p>
          <p class="lv__stats"></p>
          <p class="lv__acts" hidden><button class="bigbtn alt lv__again" type="button" hidden>Try again</button><button class="bigbtn alt lv__close" type="button">Close</button></p>
        </div>
      </div>`;
    this.el = el;
    this.q = (s) => el.querySelector(s);
    this.q("#lv-t").textContent = this.title;
    this.cv = this.q(".lv__cv");
    this.q(".lv__close").addEventListener("click", () => this.close());
    this.q(".lv__again").addEventListener("click", () => { const r = this.retry; this.close(); if (r) r(); });
    el.addEventListener("keydown", (e) => { if (e.key === "Escape" && this.state !== "run") this.close(); });
    this.inert = [...document.body.children].filter((c) => !c.inert && c.tagName !== "SCRIPT");
    for (const c of this.inert) c.inert = true;
    document.body.appendChild(el);
    document.documentElement.classList.add("lv-open");
    this.doc = document.title;
    this.t0 = performance.now();
    this.colours();
    this.onTheme = () => { this.colours(); this.swatches(); };
    window.addEventListener("m2k-theme", this.onTheme);
    this.onResize = () => this.fit();
    window.addEventListener("resize", this.onResize);
    this.fit(); this.swatches();
    this.goto(0);
    this.say("The FM-1 checks the package.");
    el.focus();
    this.wake();
    requestAnimationFrame(() => el.classList.add("in"));
    this.loop();
  }

  close() {
    cancelAnimationFrame(this.raf);
    window.removeEventListener("m2k-theme", this.onTheme);
    window.removeEventListener("resize", this.onResize);
    for (const c of this.inert || []) c.inert = false;
    document.documentElement.classList.remove("lv-open");
    document.title = this.doc;
    if (this.lock) this.lock.release().catch(() => {});
    if (this.el) this.el.remove();
    this.el = null;
    if (this.onClose) this.onClose();
  }

  /* the computer stays awake while the FM-1 is written */
  async wake() { try { if (navigator.wakeLock) this.lock = await navigator.wakeLock.request("screen"); } catch (e) { this.lock = null; } }

  colours() {
    const cs = getComputedStyle(document.documentElement);
    this.ink = cs.getPropertyValue("--ink").trim() || "#0b0b0b";
    this.paper = cs.getPropertyValue("--paper").trim() || "#f2f1ec";
  }

  /* whole device pixels for every flash pixel, so the dither stays crisp at any width */
  fit() {
    const dpr = window.devicePixelRatio || 1;
    const css = this.q(".lv__map").clientWidth - (this.q(".lv__addr").offsetWidth || 0) - 8;
    const room = Math.min(css, window.innerHeight * (innerWidth > 860 ? 0.62 : 0.44));
    this.k = Math.max(1, Math.floor(Math.max(128, room) * dpr / PX));
    this.cv.width = this.cv.height = PX * this.k;
    this.cv.style.width = this.cv.style.height = `${PX * this.k / dpr}px`;
    this.q(".lv__addr").style.height = this.cv.style.height;
  }

  swatches() {
    for (const c of this.el.querySelectorAll(".lv__legend canvas")) {
      const g = c.getContext("2d"), k = +c.dataset.k;
      g.clearRect(0, 0, 7, 7);
      this.cell(g, 0, 0, k, k === 2 ? 64 : k === 4 ? 40 : k === 3 ? 6 : 0, 1, false, 0);
    }
  }

  /* one sector: its kind's texture, then its fill in the 8 x 8 Bayer order, the head blinking */
  cell(g, ox, oy, kind, lvl, k, head, t) {
    g.fillStyle = this.ink;
    const px = (x, y) => g.fillRect(ox + x * k, oy + y * k, k, k);
    for (let y = 0; y < 7; y++) for (let x = 0; x < 7; x++) {
      const edge = x === 0 || y === 0 || x === 6 || y === 6;
      let on = BAYER[y][x] < lvl;
      if (!on) {
        if (kind === 0) on = x === 3 && y === 3;                         /* free flash: a dot */
        else if (kind === 1) on = (x + y) % 3 === 0;                     /* the boot: hatched, locked */
        else if (kind === 2 || kind === 4) on = edge;                    /* to be written: an outline */
        else if (kind === 3) on = edge && (x + y) % 2 === 0;             /* kept: a dotted outline */
      }
      if (head && (t & 1)) on = !on;                                    /* the write head: the sector inverted */
      if (on) px(x, y);
    }
  }

  draw(t) {
    const g = this.cv.getContext("2d"), k = this.k, S = CELL * k;
    g.fillStyle = this.paper; g.fillRect(0, 0, this.cv.width, this.cv.height);
    const blink = (t / 160) | 0;
    for (let s = 0; s < 256; s++) {
      let ox = (s % COLS) * S, oy = ((s / COLS) | 0) * S;
      if (this.glitch > 0) {                                             /* rows slipped sideways, in hard bands */
        const band = (((s / COLS) | 0) >> 1) * 7 + ((t / 70) | 0);
        ox += ((band * 2654435761) >>> 28) % 3 === 0 ? (((band * 40503) >>> 7) % 5 - 2) * 2 * k : 0;
      }
      let lvl = this.lvl[s];
      const kind = this.kind[s];
      if (this.scan >= 0 && ((s / COLS) | 0) === this.scan) lvl = 64 - lvl;  /* the restart's scan, row by row */
      this.cell(g, ox, oy, kind, lvl, k, s === this.head && this.state === "run" && !this.calm, blink);
    }
  }

  loop() {
    const frame = (now) => {
      if (!this.el) return;
      const dt = Math.min(64, now - (this.last || now)); this.last = now;
      this.calm = calm();
      const rate = this.calm ? 64 : dt * 0.35;                              /* a sector fills in about 180 ms */
      for (let s = 0; s < 256; s++) {
        const d = this.aim[s] - this.lvl[s];
        if (d) this.lvl[s] += Math.sign(d) * Math.min(Math.abs(d), rate);
      }
      if (this.glitch > 0) this.glitch -= dt;
      const el = (now - this.t0) / 1000, st = (now - this.tStage) / 1000;
      if (this.state === "run") this.q(".lv__clock").textContent = fmt(el);
      /* the waits creep: the FM-1 restarts, then starts the new firmware */
      if (this.state === "run" && this.stage === 1) {
        this.setPct(5 + 3 * (1 - Math.exp(-st / 8)), 1 - Math.exp(-st / 8));
        for (let s = STG[0]; s < STG[1]; s++) this.aim[s] = this.calm ? 64 : (((now / 300) | 0) + s) % 2 ? 64 : 24;
        this.stats(`Waiting for its update mode · ${fmt(st)}`);
      }
      if (this.state === "run" && this.stage === 3) {
        this.setPct(97 + 2.9 * (1 - Math.exp(-st / 5)), 1 - Math.exp(-st / 5));
        this.scan = this.calm ? -1 : ((now / 55) | 0) % 24;
        this.stats(`Waiting for it to start · ${fmt(st)}`);
      } else this.scan = -1;
      this.draw(now);
      this.raf = requestAnimationFrame(frame);
    };
    this.raf = requestAnimationFrame(frame);
  }

  say(t) { this.q(".lv__say").textContent = t; }
  stats(t) { const s = this.q(".lv__stats"); if (s.textContent !== t) s.textContent = t; }

  /* the whole update as one number, and the current step's own bar */
  setPct(p, part) {
    p = Math.max(this.pct, Math.min(100, p));
    const n = Math.floor(p), b = this.q(".lv__pct b");
    if (b.textContent !== String(n)) {
      if (((n / 10) | 0) !== ((+b.textContent / 10) | 0) && !this.calm) { b.classList.remove("gl"); void b.offsetWidth; b.classList.add("gl"); }
      b.textContent = String(n);
      document.title = `${n} % · ${this.title}`;
    }
    this.pct = p;
    const li = this.q(".lv__rail").children[this.stage];
    if (li && part !== undefined) li.style.setProperty("--p", `${Math.round(Math.min(1, part) * 100)}%`);
  }

  goto(i) {
    if (i === this.stage) return;
    const rail = this.q(".lv__rail").children;
    for (let j = 0; j < STEPS.length; j++) {
      rail[j].classList.toggle("done", j < i);
      rail[j].classList.toggle("on", j === i);
      if (j < i) rail[j].style.setProperty("--p", "100%");
    }
    this.stage = i; this.tStage = performance.now();
    const w = this.q(".lv__stage");
    w.textContent = ["Checking", "Restarting", "Writing", "Starting"][i];
    if (!this.calm) { w.classList.remove("gl"); void w.offsetWidth; w.classList.add("gl"); }
    if (i >= 1) for (let s = STG[0]; s < STG[1]; s++) this.aim[s] = 64;
  }

  /* the updater's steps (fm1ota.js) */
  step(k, a) {
    if (!this.el || this.state !== "run") return;
    if (k === "start") { this.goto(0); this.stats(`Found ${a}`); }
    else if (k === "verify") {
      this.goto(0);
      const f = Math.min(1, a / STAGE_READS), n = STG[1] - STG[0];
      for (let i = 0; i < n; i++) this.aim[STG[0] + i] = Math.max(0, Math.min(64, (f * n - i) * 64));
      this.head = STG[0] + Math.min(n - 1, Math.floor(f * n));
      this.setPct(5 * f, f);
      this.stats(`Read ${a} · staging the update loader at 0xE0000`);
    } else if (k === "loader") { this.goto(1); this.head = -1; this.say("The FM-1 restarts into its update mode."); }
    else if (k === "write") {
      if (this.stage !== 2) {
        this.goto(2); this.tWrite = performance.now(); this.nWrite = a;
        this.say("Writing. Leave the cable in.");
      }
      const f = Math.min(1, a / this.total), n = FW[1] - FW[0], done = f * n;
      for (let i = 0; i < n; i++) this.aim[FW[0] + i] = Math.max(0, Math.min(64, (done - i) * 64));
      this.head = FW[0] + Math.min(n - 1, Math.floor(done));
      this.setPct(8 + 89 * f, f);
      const secs = (performance.now() - this.tWrite) / 1000, got = a - this.nWrite;
      const kbs = secs > 1 ? got * 0.5 / secs : 0, left = kbs ? (this.total - a) * 0.5 / kbs : 0;
      this.pos = `${a} / ${this.total} · 0x${(0x4000 + Math.floor(done) * 0x1000).toString(16).toUpperCase().padStart(5, "0")}`;
      this.stats(this.pos + (kbs ? ` · ${kbs.toFixed(1)} KB/s · ${fmt(left)} left` : ""));
    } else if (k === "reboot") {
      this.goto(3); this.head = -1;
      for (let s = FW[0]; s < FW[1]; s++) this.aim[s] = 64;
      this.say("Written. The FM-1 starts again.");
    } else if (k === "done") this.finish(a);
  }

  finish(id) {
    this.state = "done";
    this.q(".lv__clock").textContent = fmt((performance.now() - this.t0) / 1000);
    const rail = this.q(".lv__rail").children;
    for (const li of rail) { li.classList.add("done"); li.classList.remove("on"); li.style.setProperty("--p", "100%"); }
    this.setPct(100);
    this.q(".lv__stage").textContent = "Done";
    this.say(this.doneText || `Done: the FM-1 runs ${id}.`);
    this.stats(`${id} · in ${fmt((performance.now() - this.t0) / 1000)}`);
    this.el.classList.add("is-done");
    document.title = `Done · ${this.title}`;
    if (this.mine) {                                     /* the sectors give way to MONO2K's isotype, in a wave */
      const iso = isoCells();
      const order = [...Array(256).keys()].sort((a, b) => ((a % COLS) + (a / COLS | 0)) - ((b % COLS) + (b / COLS | 0)));
      order.forEach((s, i) => setTimeout(() => {
        if (!this.el) return;
        this.aim[s] = iso.has(s) ? 64 : 0;
        if (!iso.has(s)) this.kind[s] = 0;
      }, this.calm ? 0 : 200 + i * 4));
    } else for (let s = 0; s < 256; s++) if (this.kind[s] === 2) this.aim[s] = 64;
    this.acts();
  }

  fail(msg) {
    if (!this.el) return;
    this.state = "fail";
    this.q(".lv__clock").textContent = fmt((performance.now() - this.t0) / 1000);
    this.glitch = this.calm ? 0 : 420;
    this.q(".lv__stage").textContent = "Stopped";
    const on = this.q(".lv__rail .on");
    if (on) on.classList.add("bad");
    this.say(msg);
    this.stats(this.pos ? `Stopped at ${this.pos}` : "");
    this.q(".lv__again").hidden = !this.retry;
    this.el.classList.add("is-fail");
    if (window.M2K_UI) window.M2K_UI.shake(this.q(".lv__say"));   /* the error comes in with a shake (ui.js, transitions.dev's) */
    document.title = `Stopped · ${this.title}`;
    this.acts();
  }

  acts() {
    this.q(".lv__acts").hidden = false;
    this.q(this.state === "fail" && this.retry ? ".lv__again" : ".lv__close").focus();
  }
}
