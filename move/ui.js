/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: a thin layer over dsp.so, which holds the controller, the screen and the LEDs (m2k_move_ui.c,
 * screen/m2k_screen.c). This file forwards the controls the DSP does not hear itself (the knobs, the jog, the buttons,
 * the knobs' touch: the pads, the steps and Move's clock go to it straight, through its on_midi), reads one frame every
 * two of the OLED's frames (~28.7 a second: the 128 x 64 page and the LEDs, one bulk get), paints the rows that
 * changed and sets the LEDs that changed, at most 16 a tick. No sequencing, sound or page logic lives here. Two things
 * do: the knobs' feel (their detents into steps, MONO2K's own curve: below) and the tempo asked of Move, written to
 * Schwung's desired-tempo file (the DSP may not touch a file: it names the tempo in its frame, this writes it).
 *
 * The pace (README, The screen): the overtake tick runs every 2 ms (Schwung's shadow_ui.c), the OLED takes one of its
 * six slices an SPI frame (128 samples, 2.9 ms; display_pull.h), so a whole panel frame is 17.4 ms (57.4 a second), and
 * a param read waits ~2.8 ms for the shim's next SPI frame. A frame read every second panel frame shows each frame for
 * exactly two of the panel's, and keeps the channel 8 % busy. host_set_refresh_rate is not ours to call: shadow_ui.c
 * (where overtake modules run) has none, and schwung_host.c's ~11 Hz is a variable its loop never reads.
 *
 * The host keeps: Back (parks MONO2K; Shift + Back closes it), the master knob, and the escape (Shift + touching the
 * volume knob + jog click). PLAY reaches Move too (module.json's button_passthrough), so one press starts both and
 * Move keeps its LED; MONO2K follows Move's start and stop, or plays alone if Move sends no clock. */
import {
    MoveBack, MoveMainKnob, MoveMaster, MoveSteps, MovePads
} from '/data/UserData/schwung/shared/constants.mjs';
import {
    setLED, setButtonLED, invalidateLedCache, decodeDelta
} from '/data/UserData/schwung/shared/input_filter.mjs';

/* ------------------------------------------------------------------- the knobs' feel: retune here (the owner's) */
/* A Move knob sends ~210 detents a turn (Schwung's docs/EC4_SURFACE.md, counted); the shadow UI batches a 2 ms tick's
 * into one CC (decodeDelta: the count). Schwung's decodeAcceleratedDelta multiplied each by up to 10 whenever they came
 * 25 ms apart or closer, which any steady turn does (a turn a second is ~5 ms a detent): a twist leapt (the owner: "too
 * sensitive"). MONO2K's curve instead, a knob each: steps a detent from the turn's speed (detents a second over the last
 * K_AVG messages), K_SLOW up to K_SLOW_RATE, rising smoothly to K_FAST by K_FAST_RATE; the fractions kept (a fresh
 * turn's first detent rounds up, so a nudge always moves), dropped at a reversal or a K_IDLE_MS pause. A brisk turn (a
 * turn a second) takes 0..127 in ~1.2 turns; a short list (MV_KK_CHOICE) 3.5 detents an option; the tempo tenths of
 * BPM slow, whole BPM fast. The DSP adds each step once (m2k_move_ui.c mv_knob): nothing stacks. */
const K_SLOW = 0.5;            /* steps a detent, slow to brisk: two detents a value */
const K_FAST = 2.5;            /* steps a detent at most: a flick */
const K_SLOW_RATE = 250;       /* detents a second up to which a turn is slow (~1.2 turns a second) */
const K_FAST_RATE = 900;       /* detents a second from which it is a flick (~4.3 turns a second) */
const K_AVG = 4;               /* messages in the speed's moving average */
const K_IDLE_MS = 400;         /* a pause this long: a fresh turn, the fraction dropped */
const K_CHOICE = 1 / 1.75;     /* a short list's options a step: 3.5 detents an option, slow */
const T_FINE = 0.5;            /* the tempo turned slowly: tenths of BPM a detent (0.1 BPM in 2, ~10 BPM a turn) */
const T_COARSE_RATE = 150;     /* detents a second from which the tempo moves in whole BPM */
const T_COARSE = 0.3;          /* whole BPM a step of the curve (~0.15 BPM a detent brisk, 0.75 a flick) */

/* the tempo asked of Move: written here for Schwung's Link sidecar, which reads the file's mtime (whole seconds) every
 * 10 ms and gives the session the tempo (Move alone in it). So two writes are never within a second, a turn writes at
 * most once a second, and its last value goes once the knob rests, a second after the write before */
const TEMPO_FILE = "/data/UserData/schwung/desired-tempo";
const TEMPO_GAP_MS = 1050;     /* writes at least this far apart: each in a later second than the last */
const TEMPO_EVERY_MS = 1000;   /* turning: a write at most this often */
const TEMPO_REST_MS = 250;     /* the knob at rest this long: its value written */

const FRAME_MS = 2 * 6 * 128 * 1000 / 44100;   /* 34.83 ms: two panel frames (six slices, one each 128 samples) */
const LED_BUDGET = 16;         /* LED messages a tick at most; the rest wait for the next */
const MUTE_MOVE = false;       /* Move's own audio under MONO2K: mixed (false) or muted (the owner's call) */
const ROWS = 64, ROW_CH = 22, HEAD = 12;                  /* the frame: "M2K1", its number (8 hex), 64 rows of 22 */
const TRACK_CC = [43, 42, 41, 40];                         /* tracks 1..4 (with Shift 5..8): the DSP's order */
const BTN_CC = [86, 88, 56, 60, 119, 62, 63, 55, 54, 50, 58];   /* m2k_move_view.h MV_BTN_CC: Rec Mute Undo Copy
                                                            * Delete Left Right Up Down Menu Loop */
const HOST_CC = new Set([MoveBack, MoveMaster]);          /* the host's */
const LED_N = 16 + 32 + 4 + BTN_CC.length;                /* the frame's LEDs, then its tail: the knobs' kinds (8), the
                                                            * tempo asked: its number (2 hex), tenths (4 hex) */
const KK_RANGE = 0, KK_CHOICE = 1, KK_TEMPO = 2, KK_NONE = 3;   /* m2k_move_view.h MV_KK_* */
const SHIFT_CC = 49;

let nextPoll = 0;              /* when the next frame is read (ms, fractional: the pace never drifts) */
let events = [];               /* controls to send, in order, as one "in" value */
let rows = new Array(ROWS).fill(null);   /* each row as last painted */
let lastSeq = "";
let want = null;               /* the LEDs as the DSP wants them: [{key, kind, id, color}] */
let shown = {};                /* LED key -> colour as last sent */
let kinds = "00000000";        /* how each knob turns (KK_*), from the frame */
let shiftHeld = false;         /* Shift (as sent to the DSP, in order): knob 1 is the tempo */
let knobs = [];                /* each knob's turn: {dir, last, acc, mode, kind, hist} */
let tw = null;                 /* the tempo writer: {seen, b10, changed, first, wrote, wseq} */

function knobsReset() {
    knobs = Array.from({ length: 8 }, () => ({ dir: 0, last: -1e9, acc: 0, mode: 0, kind: -1, hist: [] }));
}
function tempoReset() { tw = { seen: -1, b10: 0, changed: 0, first: 0, wrote: -1e9, wseq: -1 }; }
knobsReset();
tempoReset();

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

/* a knob's or the jog's turns add up until the next send ("K0 3", "J -1", the tempo's "P0 5 0": the same knob and kind
 * summed); a button goes at once (a modifier must be there before the step or pad it modifies, which reaches the DSP
 * straight) */
const TURN = /^([KJP])(\d*) (-?\d+)( \d)?$/;
function push(tok) {
    const last = events.length ? events[events.length - 1] : null;
    const m = TURN.exec(tok), lm = last && TURN.exec(last);
    if (m && lm && m[1] === lm[1] && m[2] === lm[2] && (m[4] || "") === (lm[4] || "")) {
        events[events.length - 1] = m[1] + m[2] + " " + (parseInt(lm[3], 10) + parseInt(m[3], 10)) + (m[4] || "");
        return;
    }
    events.push(tok);
}

/* --------------------------------------------------------------------------------------------- the knobs' curve */

/* knob i turned n detents (signed) at now: steps out by its kind's curve (the constants at the top) */
function knobTurn(i, n, now) {
    const kind = i === 0 && shiftHeld ? KK_TEMPO : (parseInt(kinds[i], 10) | 0);
    if (!n || kind === KK_NONE) return;
    const st = knobs[i], dir = n > 0 ? 1 : -1;
    const fresh = dir !== st.dir || now - st.last > K_IDLE_MS || kind !== st.kind;
    if (fresh) st.hist = [];
    else {
        st.hist.push([now - st.last, Math.abs(n)]);
        if (st.hist.length > K_AVG) st.hist.shift();
    }
    st.dir = dir;
    st.last = now;
    st.kind = kind;
    let dt = 0, dn = 0;
    for (const h of st.hist) { dt += h[0]; dn += h[1]; }
    const rate = dn ? dn * 1000 / Math.max(dt, 1) : 0;           /* detents a second (0: a fresh turn's first) */
    const t = Math.min(1, Math.max(0, (rate - K_SLOW_RATE) / (K_FAST_RATE - K_SLOW_RATE)));
    const g = K_SLOW + (K_FAST - K_SLOW) * t * t * (3 - 2 * t);   /* smooth from slow to fast */
    const mode = kind === KK_TEMPO && rate >= T_COARSE_RATE ? 1 : 0;
    const per = kind === KK_TEMPO ? (mode ? g * T_COARSE : T_FINE) : kind === KK_CHOICE ? g * K_CHOICE : g;
    if (fresh || mode !== st.mode) st.acc = 0.5 * dir;            /* (its first step a detent away, not two) */
    st.mode = mode;
    st.acc += n * per;
    const out = Math.trunc(st.acc);
    st.acc -= out;
    if (!out) return;
    if (kind === KK_TEMPO) push("P" + i + " " + (mode ? 10 * out : out) + " " + mode);
    else push("K" + i + " " + out);
}

/* ------------------------------------------------------------------------------------ the tempo asked of Move */

/* the frame's tempo asked (its number and tenths): a new number is a new value to write. The first frame's is not
 * ours to write again (asked before this UI) */
function tempoSeen(seq, b10, now) {
    if (tw.seen < 0) { tw.seen = seq; tw.b10 = b10; tw.wseq = seq; return; }
    if (seq === tw.seen) return;
    tw.seen = seq;
    tw.b10 = b10;
    tw.changed = now;
    if (!tw.first) tw.first = now;
}

/* each tick: the value not yet written goes when the knob has rested (or has turned a second), a second after the
 * last write; the DSP hears it went ("W n ok": its wait for Move starts) */
function tempoTick(now) {
    if (tw.seen <= 0 || tw.seen === tw.wseq) return;
    if (now - tw.wrote < TEMPO_GAP_MS) return;
    if (now - tw.changed < TEMPO_REST_MS && now - tw.first < TEMPO_EVERY_MS) return;
    let ok = false;
    if (typeof host_write_file === "function") {
        try { ok = host_write_file(TEMPO_FILE, (tw.b10 / 10).toFixed(4) + "\n") === true; } catch (e) { ok = false; }
    }
    tw.wrote = now;
    tw.wseq = tw.seen;
    tw.first = 0;
    push("W " + tw.seen + " " + (ok ? 1 : 0));
    flush();
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

/* the tail: how each knob turns, the tempo asked of Move */
function parseTail(s, now) {
    let p = HEAD + ROWS * ROW_CH + 2 * LED_N;
    if (s.length < p + 14) return;
    const kk = s.substr(p, 8);
    if (/^[0-3]{8}$/.test(kk)) kinds = kk;
    p += 8;
    const seq = parseInt(s.substr(p, 2), 16), b10 = parseInt(s.substr(p + 2, 4), 16);
    if (!isNaN(seq) && !isNaN(b10)) tempoSeen(seq, b10, now);
}

function poll(now) {
    let s = null;
    if (typeof host_module_get_params === "function") {
        const blob = host_module_get_params(bulk(["frame"]));
        if (typeof blob === "string") s = unbulk(blob)[0];
    }
    if (s == null && typeof host_module_get_param === "function") s = host_module_get_param("frame");
    if (typeof s !== "string" || !s.startsWith("M2K1") || s.length < HEAD + ROWS * ROW_CH) return;
    paint(s);
    parseLeds(s);
    parseTail(s, now);
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
    nextPoll = 0;
    knobsReset();
    tempoReset();
    shiftHeld = false;
};

globalThis.onResume = function () { resetAll(); };

globalThis.onUnload = function () {
    if (MUTE_MOVE && typeof host_mute_move_audio === "function") host_mute_move_audio(0);
};

globalThis.tick = function () {
    if (globalThis.overtakeParked) return;        /* parked in the background: no reads, no drawing, no LEDs */
    flush();
    const now = Date.now();
    if (now >= nextPoll) {                        /* (late by more than a frame: start again from now, no catching up) */
        nextPoll = now - nextPoll < FRAME_MS ? nextPoll + FRAME_MS : now + FRAME_MS;
        poll(now);
    }
    tempoTick(now);
    leds();
};

globalThis.onMidiMessageInternal = function (data) {
    const status = data[0] & 0xF0, d1 = data[1], d2 = data[2];
    if (status === 0xB0) {
        if (HOST_CC.has(d1)) return;
        if (d1 >= 71 && d1 <= 78) { knobTurn(d1 - 71, decodeDelta(d2), Date.now()); return; }   /* (MONO2K's curve) */
        if (d1 === MoveMainKnob) { const d = decodeDelta(d2); if (d) push("J " + d); return; }   /* (a detent a step) */
        if (d1 === 114 || d1 === 115) return;     /* (the jacks' detection) */
        if (d1 === SHIFT_CC) shiftHeld = d2 > 0;
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
