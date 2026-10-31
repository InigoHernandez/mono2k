/* MONO2K on the Move, step 1: a thin layer over dsp.so. The pads and the steps go to the DSP straight from the host
 * (its on_midi); this file forwards PLAY and the track buttons, reads the DSP's state ~30 times a second, and draws
 * the screen and the LEDs from it. No sequencing or sound logic lives here.
 *
 * The escape (Shift + touching the volume knob + jog click) is the host's and is never taken here; with
 * suspend_keeps_js, Back parks MONO2K (it keeps playing) and Shift + Back closes it. */
import {
    MovePlay, MoveShift, MoveRow1, MoveRow2, MoveRow3, MoveRow4, MoveSteps,
    Black, White, DarkGrey, Blue, BrightGreen
} from '/data/UserData/schwung/shared/constants.mjs';
import { setLED, setButtonLED, invalidateLedCache } from '/data/UserData/schwung/shared/input_filter.mjs';

const POLL_MS = 34;            /* the DSP read at most ~30 times a second (a read is ~2.8 ms on the param channel) */
const LED_BUDGET = 16;         /* LED messages a tick at most; the rest wait for the next */
const MUTE_MOVE = false;       /* Move's own audio under MONO2K: mixed (false) or muted (the owner's call) */
const TRACK_CC = [MoveRow1, MoveRow2, MoveRow3, MoveRow4];   /* CC 43..40: tracks 1..4, with Shift 5..8 */

/* the DSP's state, as get_param("ui") reports it (m2k_move.c, ui_string) */
const st = {
    playing: 0, step: -1, bpm: 0, track: 0, tracks: 6, page: 0, len: 16, trigs: 0,
    cpuMean: 0, cpuMost: 0, slot: "", name: "", ok: false
};
let lastPoll = 0;
let holdPlayUntil = 0;         /* a PLAY just pressed: its state kept over a read that predates it */
let shift = false;
let dirty = true;
let pending = {};              /* params to send, as one bulk set */
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

function send(key, value) { pending[key] = value; }

/* one bulk set for what waits (a single set is dropped when the channel is busy; the bulk one is answered) */
function flush() {
    const keys = Object.keys(pending);
    if (!keys.length) return;
    let ok = false;
    if (typeof host_module_set_params === "function") {
        const items = [];
        for (const k of keys) items.push(k, pending[k]);
        ok = host_module_set_params(bulk(items)) === true;
    }
    if (!ok && typeof host_module_set_param_blocking === "function") {
        ok = true;
        for (const k of keys) ok = !!host_module_set_param_blocking(k, String(pending[k]), 100) && ok;
    }
    if (ok) pending = {};
}

function parse(s) {
    if (typeof s !== "string") return;
    const f = s.split(" ");
    if (f.length < 11) return;
    const now = Date.now();
    const next = {
        playing: now < holdPlayUntil ? st.playing : (f[0] === "1" ? 1 : 0),
        step: parseInt(f[1], 10), bpm: parseInt(f[2], 10), track: parseInt(f[3], 10),
        tracks: parseInt(f[4], 10), page: parseInt(f[5], 10), len: parseInt(f[6], 10),
        trigs: parseInt(f[7], 16), cpuMean: parseInt(f[8], 10), cpuMost: parseInt(f[9], 10),
        slot: f[10], name: f.slice(11).join(" "), ok: true
    };
    if ("track" in pending) next.track = st.track;
    for (const k in next) if (st[k] !== next[k]) { st[k] = next[k]; dirty = true; }
}

function poll() {
    let s = null;
    if (typeof host_module_get_params === "function") {
        const blob = host_module_get_params(bulk(["ui"]));
        if (typeof blob === "string") s = unbulk(blob)[0];
    }
    if (s == null && typeof host_module_get_param === "function") s = host_module_get_param("ui");
    parse(s);
}

/* ------------------------------------------------------------------------------------------------ the screen */

function fontH() { return typeof get_font_height === "function" ? get_font_height() : 8; }
function textW(t) { return typeof text_width === "function" ? text_width(t) : t.length * 6; }

function draw() {
    const h = fontH();
    clear_screen();
    fill_rect(0, 0, 128, h + 4, 1);
    print(3, 2, "MONO2K", 0);
    const tr = "T" + (st.track + 1);
    print(125 - textW(tr), 2, tr, 0);
    if (!st.ok) { print(3, 20, "STARTING", 1); return; }
    if (shift) {
        /* Shift held: what a block costs the DSP (a Move block is 2902 us) */
        const pct = Math.round(st.cpuMean * 100 / 2902);
        print(3, h + 9, "CPU " + st.cpuMean + " US " + pct + "%", 1);
        print(3, 2 * h + 14, "MOST " + st.cpuMost + " US", 1);
    } else {
        print(3, h + 9, st.slot + " " + st.name, 1);
        const bpm = st.bpm + " BPM";
        print(3, 2 * h + 14, bpm, 1);
        const pl = st.playing ? "PLAY" : "STOP";
        print(125 - textW(pl), 2 * h + 14, pl, 1);
    }
    /* the page's 16 steps: a trig filled, an empty step framed, past the track's length nothing; the playhead under */
    const first = st.page * 16;
    for (let k = 0; k < 16; k++) {
        if (first + k >= st.len) break;
        const x = k * 8 + 1;
        if ((st.trigs >> k) & 1) fill_rect(x, 48, 6, 6, 1);
        else draw_rect(x, 48, 6, 6, 1);
        if (st.playing && st.step === first + k) fill_rect(x, 57, 6, 3, 1);
    }
}

/* -------------------------------------------------------------------------------------------------- the LEDs */

function stepColor(k) {
    const s = st.page * 16 + k;
    if (s >= st.len) return Black;
    if (st.playing && st.step === s) return White;
    if ((st.trigs >> k) & 1) return Blue;
    return (k & 3) === 0 ? DarkGrey : Black;     /* the beats faint, as the FM-1's GRID marks them */
}

function trackColor(b) {
    if (st.track === b) return White;
    if (st.track === b + 4) return Blue;          /* (tracks 5.. on Shift: their button in another colour) */
    return b < st.tracks ? DarkGrey : Black;
}

/* what changed since the last send, at most LED_BUDGET a tick */
function leds() {
    let n = 0;
    const want = (key, color, fn, id) => {
        if (n >= LED_BUDGET || shown[key] === color) return;
        fn(id, color);
        shown[key] = color;
        n++;
    };
    for (let k = 0; k < 16; k++) want("s" + k, stepColor(k), setLED, MoveSteps[k]);
    for (let b = 0; b < 4; b++) want("t" + b, trackColor(b), setButtonLED, TRACK_CC[b]);
    want("play", st.playing ? BrightGreen : DarkGrey, setButtonLED, MovePlay);
}

function ledsReset() {
    shown = {};
    if (typeof invalidateLedCache === "function") invalidateLedCache();
}

/* ------------------------------------------------------------------------------------------- the host's hooks */

globalThis.init = function () {
    ledsReset();
    if (MUTE_MOVE && typeof host_mute_move_audio === "function") host_mute_move_audio(1);
    lastPoll = 0;
    dirty = true;
};

globalThis.onResume = function () {
    ledsReset();
    dirty = true;
};

globalThis.onUnload = function () {
    if (MUTE_MOVE && typeof host_mute_move_audio === "function") host_mute_move_audio(0);
};

globalThis.tick = function () {
    if (globalThis.overtakeParked) return;        /* parked in the background: no reads, no drawing, no LEDs */
    const now = Date.now();
    if (now - lastPoll >= POLL_MS) {
        lastPoll = now;
        flush();
        poll();
    }
    if (dirty) { draw(); dirty = false; }
    leds();
};

globalThis.onMidiMessageInternal = function (data) {
    const status = data[0] & 0xF0, d1 = data[1], d2 = data[2];
    if (status !== 0xB0) return;                  /* (pads and steps: the DSP's, through on_midi) */
    if (d1 === MoveShift) { shift = d2 > 0; dirty = true; return; }
    if (d2 === 0) return;
    if (d1 === MovePlay) {
        st.playing = st.playing ? 0 : 1;
        holdPlayUntil = Date.now() + 150;
        send("play", st.playing);
        flush();
        dirty = true;
        return;
    }
    const b = TRACK_CC.indexOf(d1);
    if (b >= 0) {
        const t = shift ? b + 4 : b;
        if (t >= st.tracks) return;
        st.track = t;
        send("track", t);
        flush();
        dirty = true;
    }
};

globalThis.onMidiMessageExternal = function () {};
