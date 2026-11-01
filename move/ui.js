/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: a thin layer over dsp.so, which holds the controller, the screen and the LEDs (m2k_move_ui.c,
 * m2k_move_draw.c). This file forwards the controls the DSP does not hear itself (the knobs, the jog, the buttons, the
 * knobs' touch: the pads, the steps and Move's clock go to it straight, through its on_midi), reads one frame ~30 times
 * a second (the 128 x 64 page and the LEDs, one bulk get), paints the rows that changed and sets the LEDs that changed,
 * at most 16 a tick. No sequencing, sound or page logic lives here.
 *
 * The host keeps: Back (parks MONO2K; Shift + Back closes it), the master knob, and the escape (Shift + touching the
 * volume knob + jog click). PLAY reaches Move too (module.json's button_passthrough), so one press starts both and
 * Move keeps its LED; MONO2K follows Move's start and stop, or plays alone if Move sends no clock. */
import {
    MoveBack, MoveMainKnob, MoveMaster, MoveSteps, MovePads
} from '/data/UserData/schwung/shared/constants.mjs';
import {
    setLED, setButtonLED, invalidateLedCache, decodeDelta, decodeAcceleratedDelta
} from '/data/UserData/schwung/shared/input_filter.mjs';

const POLL_MS = 33;            /* a frame read at most ~30 times a second (a read is ~2.8 ms on the param channel) */
const LED_BUDGET = 16;         /* LED messages a tick at most; the rest wait for the next */
const MUTE_MOVE = false;       /* Move's own audio under MONO2K: mixed (false) or muted (the owner's call) */
const ROWS = 64, ROW_CH = 22, HEAD = 12;                  /* the frame: "M2K1", its number (8 hex), 64 rows of 22 */
const TRACK_CC = [43, 42, 41, 40];                         /* tracks 1..4 (with Shift 5..8): the DSP's order */
const BTN_CC = [86, 88, 56, 60, 119, 62, 63, 55, 54, 50, 58];   /* m2k_move_view.h MV_BTN_CC: Rec Mute Undo Copy
                                                            * Delete Left Right Up Down Menu Loop */
const HOST_CC = new Set([MoveBack, MoveMaster]);          /* the host's */

let lastPoll = 0;
let events = [];               /* controls to send, in order, as one "in" value */
let rows = new Array(ROWS).fill(null);   /* each row as last painted */
let lastSeq = "";
let want = null;               /* the LEDs as the DSP wants them: [{key, kind, id, color}] */
let shown = {};                /* LED key -> colour as last sent */

/* the bulk param format: "<count>\n" then "<len>\n<bytes>" per item (shim_handle_param_bulk) */
function bulk(items) {
    let s = items.length + "\n";
    for (const it of items) { const v = String(it); s += v.length + "\n" + v; }
    return s;
}
function unbulk(blob) {
    const out = [];
    let p = blob.indexOf("\n");
    if (p < 0) return out;
    const n = parseInt(blob.slice(0, p), 10);
    p++;
    for (let i = 0; i < n; i++) {
        const q = blob.indexOf("\n", p);
        if (q < 0) break;
        const len = parseInt(blob.slice(p, q), 10);
        out.push(blob.substr(q + 1, len));
        p = q + 1 + len;
    }
    return out;
}

/* a knob's or the jog's turns add up until the next send; a button goes at once (a modifier must be there before the
 * step or pad it modifies, which reaches the DSP straight) */
function push(tok) {
    const last = events.length ? events[events.length - 1] : null;
    const m = /^([KJ])(\d*) (-?\d+)$/.exec(tok), lm = last && /^([KJ])(\d*) (-?\d+)$/.exec(last);
    if (m && lm && m[1] === lm[1] && m[2] === lm[2]) {
        events[events.length - 1] = m[1] + m[2] + " " + (parseInt(lm[3], 10) + parseInt(m[3], 10));
        return;
    }
    events.push(tok);
}

function flush() {
    if (!events.length) return;
    const v = events.join(";");
    let sent = false;
    if (typeof host_module_set_params === "function") sent = host_module_set_params(bulk(["in", v])) === true;
    if (!sent && typeof host_module_set_param_blocking === "function") sent = !!host_module_set_param_blocking("in", v, 100);
    if (sent) events = [];
}

/* ------------------------------------------------------------------------------------------------ the screen */

/* a row of 22 characters, 6 pixels each from '0' (MSB first): cleared, then its lit runs */
function paintRow(y, r) {
    fill_rect(0, y, 128, 1, 0);
    let start = -1;
    for (let x = 0; x < 128; x++) {
        const on = ((r.charCodeAt((x / 6) | 0) - 48) >> (5 - x % 6)) & 1;
        if (on && start < 0) start = x;
        else if (!on && start >= 0) { fill_rect(start, y, x - start, 1, 1); start = -1; }
    }
    if (start >= 0) fill_rect(start, y, 128 - start, 1, 1);
}

function paint(s) {
    const seq = s.substr(4, 8);
    if (seq === lastSeq) return;
    lastSeq = seq;
    for (let y = 0; y < ROWS; y++) {
        const r = s.substr(HEAD + y * ROW_CH, ROW_CH);
        if (r === rows[y]) continue;
        rows[y] = r;
        paintRow(y, r);
    }
}

function parseLeds(s) {
    let p = HEAD + ROWS * ROW_CH;
    const next = () => { const v = parseInt(s.substr(p, 2), 16); p += 2; return v; };
    const w = [];
    for (let k = 0; k < 16; k++) w.push({ key: "s" + k, note: true, id: MoveSteps[k], color: next() });
    for (let k = 0; k < 32; k++) w.push({ key: "p" + k, note: true, id: MovePads[k], color: next() });
    for (let k = 0; k < 4; k++) w.push({ key: "t" + k, note: false, id: TRACK_CC[k], color: next() });
    for (let k = 0; k < BTN_CC.length; k++) w.push({ key: "b" + k, note: false, id: BTN_CC[k], color: next() });
    if (w.every((l) => !isNaN(l.color))) want = w;
}

function poll() {
    let s = null;
    if (typeof host_module_get_params === "function") {
        const blob = host_module_get_params(bulk(["frame"]));
        if (typeof blob === "string") s = unbulk(blob)[0];
    }
    if (s == null && typeof host_module_get_param === "function") s = host_module_get_param("frame");
    if (typeof s !== "string" || !s.startsWith("M2K1") || s.length < HEAD + ROWS * ROW_CH) return;
    paint(s);
    parseLeds(s);
}

/* -------------------------------------------------------------------------------------------------- the LEDs */

function leds() {
    if (!want) return;
    let n = 0;
    for (const l of want) {
        if (n >= LED_BUDGET) break;
        if (shown[l.key] === l.color) continue;
        if (l.note) setLED(l.id, l.color); else setButtonLED(l.id, l.color);
        shown[l.key] = l.color;
        n++;
    }
}

function resetAll() {
    shown = {};
    rows = new Array(ROWS).fill(null);
    lastSeq = "";
    if (typeof invalidateLedCache === "function") invalidateLedCache();
    if (typeof clear_screen === "function") clear_screen();
}

/* ------------------------------------------------------------------------------------------- the host's hooks */

globalThis.init = function () {
    resetAll();
    if (typeof print === "function") print(3, 28, "MONO2K", 1);   /* (until the first frame) */
    if (MUTE_MOVE && typeof host_mute_move_audio === "function") host_mute_move_audio(1);
    lastPoll = 0;
};

globalThis.onResume = function () { resetAll(); };

globalThis.onUnload = function () {
    if (MUTE_MOVE && typeof host_mute_move_audio === "function") host_mute_move_audio(0);
};

globalThis.tick = function () {
    if (globalThis.overtakeParked) return;        /* parked in the background: no reads, no drawing, no LEDs */
    flush();
    const now = Date.now();
    if (now - lastPoll >= POLL_MS) { lastPoll = now; poll(); }
    leds();
};

globalThis.onMidiMessageInternal = function (data) {
    const status = data[0] & 0xF0, d1 = data[1], d2 = data[2];
    if (status === 0xB0) {
        if (HOST_CC.has(d1)) return;
        if (d1 >= 71 && d1 <= 78) { const d = decodeAcceleratedDelta(d2, d1); if (d) push("K" + (d1 - 71) + " " + d); return; }
        if (d1 === MoveMainKnob) { const d = decodeDelta(d2); if (d) push("J " + d); return; }
        if (d1 === 114 || d1 === 115) return;     /* (the jacks' detection) */
        push("B " + d1 + " " + d2);
        flush();
        return;
    }
    if ((status === 0x90 || status === 0x80) && d1 < 8) {       /* a knob touched (127) or let go */
        push("T " + d1 + " " + (status === 0x90 && d2 >= 64 ? 1 : 0));
        flush();
    }
    /* (the pads and the steps: the DSP's own, through on_midi; aftertouch: the core takes none) */
};

globalThis.onMidiMessageExternal = function () {};
