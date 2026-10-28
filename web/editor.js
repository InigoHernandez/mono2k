// SPDX-License-Identifier: GPL-3.0-only
// The patch editor. The firmware runs in worklet.js as on the emulator page, from the same saves (and saving to
// them); a second copy of the core here, with no sound, names and formats each control as the screen does. Each
// slider sets one parameter of the open pattern's track; a chosen MIDI output gets the Monomachine's CC for it.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);
  const cstr = (x, p) => { const m = new Uint8Array(x.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
  const css = () => getComputedStyle(document.documentElement);
  const hex = (c) => { const m = c.replace("#", ""); return [parseInt(m.slice(0, 2), 16), parseInt(m.slice(2, 4), 16), parseInt(m.slice(4, 6), 16)]; };
  const status = (s) => { $("ed-status").textContent = s; };

  /* the saves: this browser's, shared with the emulator page, or the FM-1's patterns read over the link, kept apart */
  const STORE_KEY = "mono2k.store.v2", FM1_KEY = "mono2k.fm1.v1", SESSION_KEY = "mono2k.editor.session", WHEN_KEY = "mono2k.fm1.when";
  const keyOf = (sess) => (sess === "fm1" ? FM1_KEY : STORE_KEY);
  const getLS = (k) => { try { return localStorage.getItem(k); } catch (e) { return null; } };
  const setLS = (k, v) => { try { localStorage.setItem(k, v); } catch (e) { /* a private window: kept for this visit only */ } };
  let session = getLS(SESSION_KEY) === "fm1" && getLS(FM1_KEY) ? "fm1" : "browser", pendingSession = null;
  const loadStore = (sess) => {
    try {
      const s = localStorage.getItem(keyOf(sess || session));
      if (!s) return null;
      const b = atob(s), u = new Uint8Array(b.length);
      for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i);
      return u.buffer;
    } catch (e) { return null; }
  };
  const saveStore = (bytes) => {
    try {
      let s = "";
      for (let i = 0; i < bytes.length; i += 4096) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 4096));
      localStorage.setItem(keyOf(session), btoa(s));
    } catch (e) { status("Could not save in this browser (a private window?)."); }
  };

  let pre = null, wasmBytes = null, node = null, ctx = null;
  let kit = null, n = 0, machines = [0, 0, 0, 0, 0, 0], trk = 0, curPat = -1, playing = false, oct = 0;
  let midiOut = null, midiAccess = null;
  const send = (m) => { if (node) node.port.postMessage(m); };

  // ------------------------------------------------------------------------------------- the pages' knobs
  const PG = { HOME: 0, SYN: 1, AMP: 2, FILT: 3, FX: 4, LFO: 5, ARP: 6 };
  function blocks() {
    const x = pre;
    const knobs = (page, subs, only) => subs.flatMap((sub) => [0, 1, 2, 3].map((k) => x.w_kcode(page, sub, k))).filter((c) => c >= 0 && c < 255 && (!only || only.includes(c)));
    const LEVL = x.w_kcode(PG.HOME, 0, 0), PTCH = x.w_kcode(PG.HOME, 0, 1);
    return [
      ["SYN", "EDIT", knobs(PG.SYN, [0, 1])],
      ["TRACK", "HOME", [LEVL, PTCH]],
      ["AMP", "ENV", knobs(PG.AMP, [0, 1])],
      ["FILTER", "SEL", knobs(PG.FILT, [0, 1])],
      ...[0, 1, 2, 3].map((sub) => [cstr(x, x.w_page_label(PG.FX, sub)), "FX", knobs(PG.FX, [sub])]),
      ...[0, 1, 2].map((l) => [`LFO ${l + 1}`, "LFO", knobs(PG.LFO, [2 * l, 2 * l + 1])]),
      ["ARP", "ARP", knobs(PG.ARP, [0])],
    ];
  }
  /* a control's short and long name, as the screen has them for this track's machine */
  const names = (code) => {
    const x = pre;
    for (const [page, subs] of [[PG.SYN, [0, 1]], [PG.HOME, [0]], [PG.AMP, [0, 1]], [PG.FILT, [0, 1]], [PG.FX, [0, 1, 2, 3]], [PG.LFO, [0, 1, 2, 3, 4, 5]], [PG.ARP, [0]]])
      for (const sub of subs) for (let k = 0; k < 4; k++)
        if (x.w_kcode(page, sub, k) === code) return [cstr(x, x.w_knob(page, sub, machines[trk], k)), cstr(x, x.w_kname(page, sub, machines[trk], k))];
    return ["", ""];
  };
  const val = (code) => kit[trk * n + code];
  const label = (code, v) => cstr(pre, pre.w_fmt(trk, code, v));

  function render() {
    if (!kit) return;
    for (let t = 0; t < 6; t++) pre.w_mirror_machine(t, machines[t]);
    $("ed-pat").textContent = "ABCD"[curPat >> 4] + String((curPat & 15) + 1).padStart(2, "0");
    /* the tracks, as the emulator lists them */
    $("ed-tracks").innerHTML = machines.map((m, t) => `<li class="${t === trk ? "on" : ""}"><button class="tsel" type="button" data-t="${t}" aria-pressed="${t === trk}"><b>T${t + 1}</b>${esc(cstr(pre, pre.w_mi(m, 0)))}</button></li>`).join("");
    $("ed-machine").value = String(machines[trk]);
    $("ed-mach-text").textContent = `${cstr(pre, pre.w_mi(machines[trk], 3))} · ${cstr(pre, pre.w_mi(machines[trk], 2))}`;
    drawMark();
    /* the pages: a card each, a slider for each knob */
    $("ed-pages").innerHTML = blocks().map(([title, btn, codes]) => {
      const rows = codes.map((code) => {
        const [s4, ln] = names(code);
        if (!ln || s4 === "-") return "";
        const ch = pre.w_kchoices(trk, code), v = val(code);
        const [mn, mx, sv] = ch ? [0, ch - 1, pre.w_choice(trk, code, v)] : [pre.w_kmin(trk, code), pre.w_kmax(trk, code), v];
        return `<div class="ectl"><label for="c${code}"><b>${esc(s4)}</b><span>${esc(ln)}</span></label><output id="o${code}">${esc(label(code, v))}</output>` +
               `<input type="range" id="c${code}" data-code="${code}" data-ch="${ch}" min="${mn}" max="${mx}" step="1" value="${sv}"></div>`;
      }).join("");
      return rows ? `<section class="ecard"><h3>${esc(title)}<span>${esc(btn)}</span></h3>${rows}</section>` : "";
    }).join("");
  }
  function drawMark() {
    const cv = $("ed-mark"), g = cv.getContext("2d"), img = g.createImageData(72, 72);
    const I = hex(css().getPropertyValue("--ink").trim() || "#0b0b0b"), P = hex(css().getPropertyValue("--paper").trim() || "#f2f1ec");
    const bits = new Uint8Array(pre.memory.buffer, pre.w_mark(machines[trk]), 512);
    for (let p = 0; p < 72 * 72; p++) { const o = p * 4; img.data[o] = P[0]; img.data[o + 1] = P[1]; img.data[o + 2] = P[2]; img.data[o + 3] = 255; }
    for (let i = 0; i < 512; i++) for (let j = 0; j < 8; j++) {
      if (!((bits[i] >> (7 - j)) & 1)) continue;
      const o = (((i >> 3) + 4) * 72 + (i & 7) * 8 + j + 4) * 4;
      img.data[o] = I[0]; img.data[o + 1] = I[1]; img.data[o + 2] = I[2];
    }
    g.putImageData(img, 0, 0);
  }

  /* a slider moved: the value to the core (and to MIDI), its label as the screen would show it */
  $("ed-pages").addEventListener("input", (ev) => {
    const el = ev.target;
    if (!el.dataset || el.dataset.code === undefined) return;
    const code = +el.dataset.code, ch = +el.dataset.ch;
    const v = ch ? pre.w_enc(+el.value, ch) : +el.value;
    kit[trk * n + code] = v;
    send({ t: "param", trk, code, v });
    $("o" + code).textContent = label(code, v);
    if (midiOut) {
      const cc = pre.w_cc_of(code);
      if (cc >= 0) midiOut.send([0xb0 | trk, cc, Math.min(127, v)]);
    }
  });
  $("ed-tracks").addEventListener("click", (ev) => {
    const b = ev.target.closest("[data-t]");
    if (!b) return;
    trk = +b.dataset.t;
    render();
  });
  $("ed-machine").addEventListener("change", (ev) => {
    const m = +ev.target.value;
    send({ t: "machine", trk, m });
    status(`T${trk + 1}: ${cstr(pre, pre.w_mi(m, 0))}.`);
  });
  $("ed-pattern").addEventListener("change", (ev) => {
    send({ t: "pattern", n: +ev.target.value });
    setTimeout(() => send({ t: "kit" }), 60);
  });
  const tap = (b) => { send({ t: "btn", b, d: 1 }); send({ t: "btn", b, d: 0 }); };
  $("ed-play").addEventListener("click", () => tap(10));
  $("ed-save").addEventListener("click", () => { tap(7); status(`Saved ${$("ed-pat").textContent} in the project.`); });

  // -------------------------------------------------------------------------------------------- audition
  const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const base = () => 48 + 12 * oct;
  function keys() {
    const kb = $("ed-kb");
    kb.innerHTML = "";
    for (let i = 0; i < 24; i++) {
      const note = base() + i, black = [1, 3, 6, 8, 10].includes(i % 12);
      const b = document.createElement("button");
      b.type = "button";
      b.className = black ? "k b" : "k";
      b.dataset.note = String(note);
      b.setAttribute("aria-label", NOTE[note % 12] + (Math.floor(note / 12) - 1));
      if (!black && i % 12 === 0) b.textContent = NOTE[note % 12] + (Math.floor(note / 12) - 1);
      kb.appendChild(b);
    }
    $("ed-octv").textContent = "C" + (Math.floor(base() / 12) - 1);
  }
  const on = new Map();
  const noteOn = (note, id) => { if (on.has(id)) return; on.set(id, note); send({ t: "midi", s: 0x90 | trk, a: note, b: 100 }); if (midiOut) midiOut.send([0x90 | trk, note, 100]); };
  const noteOff = (id) => { const note = on.get(id); if (note === undefined) return; on.delete(id); send({ t: "midi", s: 0x80 | trk, a: note, b: 0 }); if (midiOut) midiOut.send([0x80 | trk, note, 0]); };
  $("ed-kb").addEventListener("pointerdown", (ev) => { const k = ev.target.closest("[data-note]"); if (!k) return; ev.preventDefault(); k.setPointerCapture(ev.pointerId); k.classList.add("down"); noteOn(+k.dataset.note, "p" + ev.pointerId); });
  const up = (ev) => { const k = ev.target.closest("[data-note]"); if (k) k.classList.remove("down"); noteOff("p" + ev.pointerId); };
  $("ed-kb").addEventListener("pointerup", up);
  $("ed-kb").addEventListener("pointercancel", up);
  const KEYS = { KeyZ: 0, KeyS: 1, KeyX: 2, KeyD: 3, KeyC: 4, KeyV: 5, KeyG: 6, KeyB: 7, KeyH: 8, KeyN: 9, KeyJ: 10, KeyM: 11,
                 KeyQ: 12, Digit2: 13, KeyW: 14, Digit3: 15, KeyE: 16, KeyR: 17, Digit5: 18, KeyT: 19, Digit6: 20, KeyY: 21, Digit7: 22, KeyU: 23 };
  window.addEventListener("keydown", (ev) => {
    if (!node || ev.repeat || ev.metaKey || ev.ctrlKey || ev.altKey) return;
    if (ev.target.closest && ev.target.closest("select, input")) return;
    if (KEYS[ev.code] === undefined) return;
    ev.preventDefault();
    noteOn(base() + KEYS[ev.code], ev.code);
  });
  window.addEventListener("keyup", (ev) => { if (KEYS[ev.code] !== undefined) noteOff(ev.code); });
  window.addEventListener("blur", () => { for (const id of Array.from(on.keys())) noteOff(id); });
  $("ed-octd").addEventListener("click", () => { oct = Math.max(-2, oct - 1); keys(); });
  $("ed-octu").addEventListener("click", () => { oct = Math.min(2, oct + 1); keys(); });

  // ---------------------------------------------------------------------------------------------- MIDI out
  /* asked for only when the list is opened: the browser's permission comes with a gesture */
  async function midiList() {
    if (midiAccess || !navigator.requestMIDIAccess) return;
    try {
      midiAccess = await navigator.requestMIDIAccess();
      const sel = $("ed-midi");
      const fill = () => {
        const keep = sel.value;
        sel.innerHTML = '<option value="">None</option>' + Array.from(midiAccess.outputs.values()).map((o) => `<option value="${esc(o.id)}">${esc(o.name)}</option>`).join("");
        sel.value = keep;
      };
      fill();
      midiAccess.onstatechange = fill;
    } catch (e) { status("MIDI is not available in this browser."); }
  }
  $("ed-midi").addEventListener("pointerdown", midiList);
  $("ed-midi").addEventListener("focus", midiList);
  $("ed-midi").addEventListener("change", (ev) => {
    midiOut = midiAccess && ev.target.value ? midiAccess.outputs.get(ev.target.value) : null;
    status(midiOut ? `Changes go to ${midiOut.name} too, on the Monomachine's CC numbers, channel = track.` : "");
  });

  // ------------------------------------------------------------------------------------- the FM-1, over USB
  /* MONO2K on the FM-1 answers SysEx F0 7D 'M' '2' 'K' cmd pack7(body) F7 (firmware/fm1/m2k_link.c): its store read
   * out 128 bytes a reply (saved on the FM-1 first), one written back 256 bytes a request, each request answered
   * before the next; a status first in every answer. Its patterns are edited here as a session of their own. */
  const LK = { HELLO: 1, INFO: 2, READ: 3, BEGIN: 4, WRITE: 5, END: 6 };
  const LK_ERR = ["", "The FM-1 is playing: stop it (PLAY) and try again.", "That is too large for the FM-1's store.",
    "The FM-1 had no transfer open.", "A piece arrived out of order.", "The FM-1's flash refused the write: it keeps what it had.",
    "The data arrived damaged: the FM-1 keeps what it had.", "The FM-1 could not read that store: it keeps what it had.",
    "The FM-1 did not understand the request: update MONO2K on it."];
  const seed = () => (Math.random() * 4294967295) >>> 0;
  const pack7 = (b) => {                       /* 8 bits to 7, LSB first (the platform's ota_pack7) */
    const o = [];
    let acc = 0, nb = 0;
    for (const v of b) { acc |= v << nb; nb += 8; while (nb >= 7) { o.push(acc & 0x7f); acc >>= 7; nb -= 7; } }
    if (nb) o.push(acc & 0x7f);
    return o;
  };
  const unpack7 = (b) => {
    const o = [];
    let acc = 0, nb = 0;
    for (const v of b) { acc |= (v & 0x7f) << nb; nb += 7; if (nb >= 8) { o.push(acc & 0xff); acc >>= 8; nb -= 8; } }
    return new Uint8Array(o);
  };
  const CRC = new Uint32Array(256).map((_, i) => { let c = i; for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xedb88320 : c >>> 1; return c >>> 0; });
  const crc32 = (b) => { let c = 0xffffffff; for (const v of b) c = CRC[(c ^ v) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const rd32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
  const le32 = (v) => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];

  let fm1 = null, fm1Wait = null, linkBusy = false;
  const linkSay = (s, kind) => { const el = $("ed-link-state"); el.textContent = s; el.className = "small" + (kind ? " " + kind : ""); };
  const prog = (f) => { const el = $("ed-prog"); el.hidden = f === null; if (f !== null) el.value = f; };
  function linkButtons() {
    $("ed-fm1-go").textContent = fm1 ? "Connected" : "Connect";
    $("ed-fm1-go").setAttribute("aria-pressed", String(!!fm1));
    $("ed-fm1-read").disabled = !fm1 || linkBusy;
    $("ed-fm1-send").disabled = !fm1 || linkBusy;
    $("ed-fm1-go").disabled = linkBusy;
    $("ed-file-save").disabled = linkBusy;
    $("ed-file-open").disabled = linkBusy;
  }
  function sessionShow() {
    const when = getLS(WHEN_KEY);
    $("ed-src").textContent = session === "fm1"
      ? `Editing the FM-1's patterns and sounds${when ? ", as read " + when : ""}. Kept apart from this browser's saves: send them back to the FM-1, or save a file.`
      : "Editing this browser's saves, the emulator's. Read from the FM-1 to edit its patterns instead.";
    $("ed-src-back").hidden = session !== "fm1";
  }
  const fm1Msg = (ev) => {
    const d = ev.data;
    if (d.length < 8 || d[0] !== 0xf0 || d[1] !== 0x7d || d[2] !== 0x4d || d[3] !== 0x32 || d[4] !== 0x4b) return;
    const w = fm1Wait;
    if (!w || d[5] !== (w.cmd | 0x40)) return;
    fm1Wait = null;
    w.done(unpack7(d.subarray(6, d.length - 1)));
  };
  const ask = (port, cmd, body, ms) => new Promise((res, rej) => {
    const t = setTimeout(() => { fm1Wait = null; rej(new Error("no answer")); }, ms || 3000);
    fm1Wait = { cmd, done: (b) => { clearTimeout(t); res(b); } };
    try { port.output.send([0xf0, 0x7d, 0x4d, 0x32, 0x4b, cmd, ...pack7(body || []), 0xf7]); }
    catch (e) { clearTimeout(t); fm1Wait = null; rej(e); }
  });
  const askAgain = async (cmd, body, ms) => {   /* a request lost on the way is asked again, twice */
    for (let k = 0; ; k++) {
      try { return await ask(fm1, cmd, body, ms); }
      catch (e) { if (k === 2) throw new Error("The FM-1 stopped answering. Check the cable and connect again."); }
    }
  };

  async function fm1Connect() {
    if (fm1 || linkBusy) return;
    if (!navigator.requestMIDIAccess) return linkSay("This browser has no Web MIDI: use Chrome or Edge on a computer.", "err");
    let acc;
    try { acc = await navigator.requestMIDIAccess({ sysex: true }); }
    catch (e) { return linkSay("MIDI with SysEx was not allowed. Allow it for this site (the icon in the address bar) and connect again.", "err"); }
    linkSay("Looking for the FM-1…");
    const outs = [...acc.outputs.values()];
    for (const input of acc.inputs.values()) {
      if (input.state === "disconnected" || !/fm-1|felucca|composite|sinco|usb-midi|mono2k/i.test(input.name || "")) continue;
      const output = outs.find((o) => o.name === input.name && o.state !== "disconnected");
      if (!output) continue;
      try { await input.open(); await output.open(); } catch (e) { continue; }
      const port = { input, output };
      input.addEventListener("midimessage", fm1Msg);
      try {
        const h = await ask(port, LK.HELLO, [], 1500);
        if (h[0] === 0 && h[1] >= 1) {
          const fw = String.fromCharCode(...h.subarray(8, 24)).replace(/\0.*$/, "");
          fm1 = { ...port, patterns: h[2], projects: h[3], sounds: h[4], room: h.length >= 28 ? rd32(h, 24) : 0 };
          acc.onstatechange = (ev) => {
            if (fm1 && ev.port.state === "disconnected" && (ev.port === fm1.input || ev.port === fm1.output)) {
              fm1.input.removeEventListener("midimessage", fm1Msg);
              fm1 = null;
              linkSay("The FM-1 was disconnected.", "err");
              linkButtons();
            }
          };
          linkSay(`${fw} on the FM-1, ${h[2]} patterns${h[5] ? ", playing" : ""}. Read its patterns, or send these.`, "ok");
          linkButtons();
          return;
        }
      } catch (e) { /* not MONO2K, or an older one: no link */ }
      input.removeEventListener("midimessage", fm1Msg);
    }
    linkSay("No FM-1 running MONO2K answered. Connect it over USB and close other pages that use it (the Installer). A MONO2K older than this one has no link: update it from the Installer.", "err");
  }

  async function fm1Read() {
    if (!fm1 || linkBusy) return;
    linkBusy = true; linkButtons();
    try {
      linkSay("Saving on the FM-1…");
      const i = await askAgain(LK.INFO, [], 8000);
      if (i[0]) throw new Error(LK_ERR[i[0]] || `The FM-1 refused (${i[0]}).`);
      const len = rd32(i, 1), crc = rd32(i, 5), b = new Uint8Array(len);
      linkSay("Reading the FM-1's patterns and sounds…");
      for (let at = 0; at < len;) {
        const n = Math.min(128, len - at);
        const r = await askAgain(LK.READ, [...le32(at), n]);
        if (r[0] || rd32(r, 1) !== at || r[5] !== n) throw new Error(LK_ERR[r[0]] || "A piece arrived wrong: read again.");
        b.set(r.subarray(6, 6 + n), at);
        at += n;
        prog(at / len);
      }
      if (crc32(b) !== crc) throw new Error("The store arrived damaged: read again.");
      pendingSession = "fm1";
      send({ t: "load", bytes: b, seed: seed() });
      setLS(WHEN_KEY, new Date().toLocaleString([], { day: "numeric", month: "short", hour: "2-digit", minute: "2-digit" }));
      linkSay(`Read ${(len / 1024).toFixed(1)} KB from the FM-1.`, "ok");
    } catch (e) {
      linkSay(e.message, "err");
    } finally {
      linkBusy = false; linkButtons(); prog(null);
    }
  }

  async function fm1Write(b) {
    if (!fm1 || linkBusy) return;
    linkBusy = true; linkButtons();
    try {
      linkSay("Sending to the FM-1…");
      const r0 = await askAgain(LK.BEGIN, [...le32(b.length), ...le32(crc32(b))]);
      if (r0[0]) throw new Error(LK_ERR[r0[0]] || `The FM-1 refused (${r0[0]}).`);
      for (let at = 0; at < b.length;) {
        const n = Math.min(256, b.length - at), body = new Uint8Array(4 + n);
        body.set(le32(at));
        body.set(b.subarray(at, at + n), 4);
        const r = await askAgain(LK.WRITE, body, 2000);
        if (!(r[0] === 0 || (r[0] === 4 && rd32(r, 1) === at + n))) throw new Error(LK_ERR[r[0]] || "The FM-1 refused a piece.");
        at += n;
        prog(at / b.length);
      }
      linkSay("The FM-1 is reading it…");
      const e = await askAgain(LK.END, [], 8000);
      if (e[0]) throw new Error(LK_ERR[e[0]] || `The FM-1 refused (${e[0]}).`);
      linkSay("Sent: the FM-1 plays these patterns and sounds now, and keeps them.", "ok");
    } catch (e) {
      linkSay(e.message, "err");
    } finally {
      linkBusy = false; linkButtons(); prog(null);
    }
  }

  /* the worklet's answers: a store loaded in place of the open one, a store packed for a file or the FM-1 */
  function loaded(m) {
    const to = pendingSession;
    pendingSession = null;
    if (!m.ok) {
      status("That store could not be read: nothing changed.");
      send({ t: "load", bytes: loadStore(), seed: seed() });   /* back to the session's own */
      return;
    }
    if (to) { session = to; setLS(SESSION_KEY, session); saveStore(m.bytes); }
    sessionShow();
    send({ t: "names" });
  }
  /* a store's images' sizes (version 3: the header, the settings, then each image's name, size, lengths, data) */
  function imageSizes(b) {
    if (b.length < 10 || b[4] < 3) return [];
    const np = b[8], nk = b[9], out = [];
    let at = 10 + 16;
    for (let i = 0; i < 1 + nk && at + 12 <= b.length; i++) {
      const used = rd32(b, at + 8);
      out.push(used);
      at += 12 + 2 * np + used;
    }
    return out;
  }
  /* the most one pattern packs to (m2k_proj.c's BLOB_MAX): the FM-1 keeps that much free in its image, for the
   * pattern playing to grow into (its patterns open only so), so a store must leave it */
  const BLOB_MAX = 3596;
  function packed(m) {
    if (m.why === "send") {
      const big = Math.max(0, ...imageSizes(m.bytes));
      if (fm1 && fm1.room && big + BLOB_MAX > fm1.room)
        return linkSay(`These patterns take ${(big / 1024).toFixed(1)} KB; the FM-1 keeps ${((fm1.room - BLOB_MAX) / 1024).toFixed(1)} KB, room for the pattern playing to grow kept. Clear or INIT some patterns first.`, "err");
      fm1Write(m.bytes);
      return;
    }
    if (m.why !== "file") return;
    const d = new Date(), day = `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([m.bytes], { type: "application/octet-stream" }));
    a.download = `mono2k-${m.fm1 ? "fm1" : "browser"}-${day}.m2ks`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 2000);
    status(`Saved ${a.download}: ${m.fm1 ? "the FM-1's patterns and its sounds" : "this browser's projects and sounds"}.`);
  }

  $("ed-fm1-go").addEventListener("click", fm1Connect);
  $("ed-fm1-read").addEventListener("click", () => {
    if (session === "fm1" && !confirm("Read the FM-1 again? Its patterns replace the copy edited here.")) return;
    fm1Read();
  });
  $("ed-fm1-send").addEventListener("click", () => {
    const what = session === "fm1" ? "These patterns and sounds" : "Bank A of this browser's open project, and its sounds,";
    if (!confirm(`${what} replace what the FM-1 keeps. Send them?`)) return;
    send({ t: "pack", fm1: true, patterns: fm1.patterns, why: "send" });
  });
  $("ed-file-save").addEventListener("click", () => send({ t: "pack", fm1: session === "fm1", patterns: fm1 ? fm1.patterns : 64, why: "file" }));
  $("ed-file-open").addEventListener("change", async (ev) => {
    const f = ev.target.files[0];
    ev.target.value = "";
    if (!f) return;
    const b = new Uint8Array(await f.arrayBuffer());
    if (b.length < 10 || b[0] !== 0x4d || b[1] !== 0x32 || b[2] !== 0x4b || b[3] !== 0x32) { status(`${f.name} is not a MONO2K store (.m2ks).`); return; }
    const fm1File = b[4] >= 3 && b[9] === 1;
    if (!confirm(fm1File ? `Open ${f.name} as the FM-1's patterns? It replaces the FM-1 copy edited here (the FM-1 itself only when you send it).`
                         : `Open ${f.name} in place of this browser's saves? The emulator plays it too.`)) return;
    pendingSession = fm1File ? "fm1" : "browser";
    send({ t: "load", bytes: b, seed: seed() });
  });
  $("ed-src-back").addEventListener("click", () => { pendingSession = "browser"; send({ t: "load", bytes: loadStore("browser"), seed: seed() }); });

  // -------------------------------------------------------------------------------------------- the core
  async function boot() {
    const r = await fetch("m2k.wasm?v=89");
    wasmBytes = await r.arrayBuffer();
    pre = new WebAssembly.Instance(new WebAssembly.Module(wasmBytes), {}).exports;
    if (pre._initialize) pre._initialize();
    pre.w_init(2000);
    n = pre.w_pcount();
    /* the machines in ALGORITHM's order, each its number (an older m2k.wasm without w_morder: by number) */
    $("ed-machine").innerHTML = Array.from({ length: pre.w_machines() }, (_, k) => pre.w_morder ? pre.w_morder(k) : k)
      .map((m) => `<option value="${m}">${esc(cstr(pre, pre.w_mi(m, 0)))}</option>`).join("");
    keys();
  }
  async function start() {
    $("ed-go").disabled = true;
    try {
      if (!pre) await boot();
      try { ctx = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" }); }
      catch (e) { ctx = new AudioContext({ latencyHint: "interactive" }); }
      await ctx.audioWorklet.addModule("worklet.js?v=35");
      node = new AudioWorkletNode(ctx, "mono2k", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
      node.connect(ctx.destination);
      node.port.onmessage = (ev) => {
        const m = ev.data;
        if (m.t === "frame") {
          if (m.store) saveStore(m.store);
          if (m.names) send({ t: "names" });
          const info = m.info, p = info[3], pl = !!info[4];
          if (p !== curPat || machines.some((v, t) => v !== info[17 + t])) send({ t: "kit" });
          if (pl !== playing) { playing = pl; $("ed-play").setAttribute("aria-pressed", String(pl)); $("ed-play").textContent = pl ? "Stop" : "Play"; }
        } else if (m.t === "kit") {
          kit = m.kits; n = m.n; machines = m.machines; curPat = m.pat;
          $("ed-pattern").value = String(curPat);
          render();
        } else if (m.t === "names") {
          const sel = $("ed-pattern"), keep = curPat;
          sel.innerHTML = m.pats.map((nm, i) => `<option value="${i}">${"ABCD"[i >> 4]}${String((i & 15) + 1).padStart(2, "0")} ${esc(nm)}</option>`).join("");
          if (keep >= 0) sel.value = String(keep);
        } else if (m.t === "store") {
          saveStore(m.bytes);
        } else if (m.t === "loaded") {
          loaded(m);
        } else if (m.t === "packed") {
          packed(m);
        } else if (m.t === "ready") {
          send({ t: "set", i: 0, v: window.M2K_SITE && window.M2K_SITE.dark() ? 0 : 127 });
          send({ t: "kit" });
        }
      };
      node.port.postMessage({ t: "wasm", bytes: wasmBytes, seed: seed(), store: loadStore() });
      sessionShow();
      await ctx.resume();
      $("ed-start").hidden = true;
      $("ed-main").hidden = false;
    } catch (e) {
      status("Could not start: " + e.message);
      $("ed-go").disabled = false;
    }
  }
  $("ed-go").addEventListener("click", start);
  window.addEventListener("m2k-theme", () => { if (kit) drawMark(); });
  boot().catch((e) => { $("ed-go").disabled = true; $("ed-go").innerHTML = "Unavailable<small>" + esc(e.message) + "</small>"; });
})();
