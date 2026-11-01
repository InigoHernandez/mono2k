/* SPDX-License-Identifier: GPL-3.0-only */
/* move/ui.js run in Node against a mock of Schwung's shadow UI and of dsp.so's params:
 *   node move/test/ui_test.mjs
 * The controls forwarded (knobs batched and summed, the jog, buttons at once, the knobs' touch; nothing of the host's,
 * the pads' or the steps': the pages, the keyboard and PERFORM are the DSP's), the frame read every two panel frames
 * (34.83 ms, ~28.7 a second, evenly, never caught up after a stall) and painted by rows and runs (only the rows that
 * changed, nothing for the same frame), the LEDs at most 16 a tick and to the right notes and CCs (the 32 pads on
 * 68.., the page pads among them), a failed send kept, the parked tick, the resume. The knobs' curve (MONO2K's own:
 * slow turns half a step a detent, a fast burst capped, a reversal starting fresh, a full sweep's detents, a short list
 * slower, the tempo in tenths and whole BPM, Shift + knob 1 the tempo); the tempo asked of Move written to
 * desired-tempo (host_write_file mocked): after the knob rests, at most once a second while turning, never two writes
 * within 1.05 s, the DSP told ("W n ok"), the first frame's request not written again. */
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const src = readFileSync(new URL('../ui.js', import.meta.url), 'utf8')
    .replace(/^import[\s\S]*?from\s+'[^']+';\s*$/gm, '');      /* the host's modules: given by the mock below */

let clock = 1000;
const sets = [], gets = [], fills = [], leds = [], writes = [];
let clears = 0, prints = 0, setOk = true, writeOk = true;

/* the DSP's frame: "M2K1", its number, 64 rows of 22 characters, the LEDs (63, two hex digits each), the knobs' kinds
 * (8), the tempo asked: its number (2 hex), tenths (4 hex) */
const blankRow = '0'.repeat(22);
let frameRows = new Array(64).fill(blankRow), frameSeq = 1, frameLeds = new Array(63).fill(0);
let frameKinds = '00000000', tqSeq = 5, tqB10 = 1200;
function rowFrom(bits) {                 /* bits: a 128-character string of 0/1 */
    let r = '';
    for (let c = 0; c < 22; c++) {
        let v = 0;
        for (let b = 0; b < 6; b++) { const x = c * 6 + b; v = v << 1 | (x < 128 && bits[x] === '1' ? 1 : 0); }
        r += String.fromCharCode(48 + v);
    }
    return r;
}
function frame() {
    const hex = (v) => v.toString(16).toUpperCase().padStart(2, '0');
    return 'M2K1' + frameSeq.toString(16).toUpperCase().padStart(8, '0') + frameRows.join('') + frameLeds.map(hex).join('')
        + frameKinds + hex(tqSeq) + tqB10.toString(16).toUpperCase().padStart(4, '0');
}

const ctx = {
    MoveBack: 51, MoveMainKnob: 14, MoveMaster: 79,
    MoveSteps: Array.from({ length: 16 }, (_, i) => 16 + i),
    MovePads: Array.from({ length: 32 }, (_, i) => 68 + i),
    setLED: (n, c) => leds.push(['n', n, c]),
    setButtonLED: (n, c) => leds.push(['cc', n, c]),
    invalidateLedCache: () => {},
    decodeDelta: (v) => (v >= 1 && v <= 63 ? v : v >= 65 && v <= 127 ? v - 128 : 0),
    host_write_file: (path, content) => { writes.push([clock, path, content]); return writeOk; },
    clear_screen: () => clears++, print: () => prints++,
    fill_rect: (x, y, w, h, c) => fills.push([x, y, w, h, c]),
    host_module_get_params: (blob) => { gets.push(clock); const v = frame(); return `1\n${v.length}\n${v}`; },
    host_module_set_params: (blob) => {
        if (!setOk) return false;
        let p = blob.indexOf('\n') + 1;
        const items = [];
        while (p < blob.length) {
            const q = blob.indexOf('\n', p), len = parseInt(blob.slice(p, q), 10);
            items.push(blob.substr(q + 1, len));
            p = q + 1 + len;
        }
        sets.push(items);
        return true;
    },
    Date: { now: () => clock },
};
ctx.globalThis = ctx;
vm.createContext(ctx);
vm.runInContext(src, ctx);

let fails = 0;
const check = (ok, what) => { console.log(`  ${ok ? 'ok  ' : 'FAIL'}  ${what}`); fails += !ok; };
const tick = (n = 1, dt = 1) => { for (let i = 0; i < n; i++) { ctx.tick(); clock += dt; } };
const lastIn = () => (sets.length ? sets.at(-1)[1] : null);

ctx.init();
check(clears === 1 && prints === 1, 'init: the screen cleared, MONO2K until the first frame');
tick(100);                              /* 100 ticks over 100 ms */
check(gets.length === 3, `frames read every 34.83 ms (${gets.length} in 100 ms of ticks)`);
check(fills.length === 64 && fills.every((f) => f[2] === 128 && f[4] === 0), 'a blank first frame: each row cleared once');

/* a row with runs, then the same frame again, then one row changed */
fills.length = 0;
frameRows[3] = rowFrom('1'.repeat(10) + '0'.repeat(5) + '11' + '0'.repeat(100) + '1'.repeat(11));
frameSeq++;
tick(36, 1);
const r3 = fills.filter((f) => f[1] === 3);
check(fills.length === 4 && r3.length === 4, `only the changed row painted (${fills.length} calls)`);
check(r3[1][0] === 0 && r3[1][2] === 10 && r3[2][0] === 15 && r3[2][2] === 2 && r3[3][0] === 117 && r3[3][2] === 11,
      'its lit runs as fill_rects: 0+10, 15+2, 117+11 (to the edge)');
fills.length = 0;
tick(36, 1);
check(fills.length === 0, 'the same frame number: nothing painted');

/* the pace over a second of 2 ms ticks: 28 or 29 reads, 34 or 35 ms apart (the panel's frame twice); a stall of 200 ms
 * is not caught up (one read, then the pace again) */
gets.length = 0;
tick(500, 2);
const gaps = gets.slice(1).map((t, i) => t - gets[i]);
check(gets.length >= 28 && gets.length <= 29 && gaps.every((g) => g >= 34 && g <= 36),
      `~28.7 reads a second, evenly (${gets.length}; gaps ${Math.min(...gaps)}-${Math.max(...gaps)} ms)`);
gets.length = 0;
clock += 200;
tick(36, 1);
check(gets.length === 2, `after a 200 ms stall: one read at once, then the pace (${gets.length} in 36 ms)`);

/* the LEDs: at most 16 a tick, each to its note or CC */
frameLeds = Array.from({ length: 63 }, (_, i) => 1 + (i % 100));
frameSeq++;
leds.length = 0;
const per = [];
for (let i = 0; i < 6; i++) { const n = leds.length; tick(1, 35); per.push(leds.length - n); }
check(Math.max(...per) <= 16, `LEDs at most 16 a tick (${per.join(', ')})`);
check(leds.length === 63, `every LED sent once over the ticks (${leds.length})`);
const step1 = leds.find((l) => l[0] === 'n' && l[1] === 16), pad1 = leds.find((l) => l[0] === 'n' && l[1] === 68);
const trk1 = leds.find((l) => l[0] === 'cc' && l[1] === 43), rec = leds.find((l) => l[0] === 'cc' && l[1] === 86);
const pad17 = leds.find((l) => l[0] === 'n' && l[1] === 84), pad32 = leds.find((l) => l[0] === 'n' && l[1] === 99);
check(step1 && step1[2] === 1 && pad1 && pad1[2] === 17 && trk1 && trk1[2] === 49 && rec && rec[2] === 53,
      'steps on notes 16.., pads on 68.., track 1 on CC 43, Rec on CC 86');
check(pad17 && pad17[2] === 33 && pad32 && pad32[2] === 48, 'the page pads: the second row from note 84, the top row to 99');
check(!leds.some((l) => l[0] === 'cc' && l[1] === 85), "PLAY's LED left to Move");
leds.length = 0;
tick(3, 35);
check(leds.length === 0, 'unchanged LEDs: nothing sent');

/* the controls */
sets.length = 0;
ctx.onMidiMessageInternal([0xB0, 71, 1]);
clock += 60;
ctx.onMidiMessageInternal([0xB0, 71, 1]);
clock += 60;
ctx.onMidiMessageInternal([0xB0, 71, 1]);
ctx.onMidiMessageInternal([0xB0, 72, 127]);
check(sets.length === 0, 'knob turns wait for the tick');
tick(1);
check(sets.length === 1 && sets[0][0] === 'in' && lastIn() === 'K0 2;K1 -1',
      `one send: knob 1's three slow detents two steps, knob 2's one a step (${lastIn()})`);
ctx.onMidiMessageInternal([0xB0, 14, 127]);
ctx.onMidiMessageInternal([0xB0, 14, 2]);
tick(1);
check(lastIn() === 'J 1', `the jog's turns summed, unaccelerated (${lastIn()})`);
sets.length = 0;
ctx.onMidiMessageInternal([0xB0, 49, 127]);
check(sets.length === 1 && lastIn() === 'B 49 127', 'Shift goes at once (before the step it modifies)');
ctx.onMidiMessageInternal([0xB0, 85, 127]);
check(lastIn() === 'B 85 127', 'PLAY goes to the DSP too (Move has it through button_passthrough)');
ctx.onMidiMessageInternal([0xB0, 42, 127]);
ctx.onMidiMessageInternal([0xB0, 42, 0]);
check(lastIn() === 'B 42 0' && sets.length === 4, 'a track button, press and release');
const before = sets.length;
ctx.onMidiMessageInternal([0xB0, 51, 127]);
ctx.onMidiMessageInternal([0xB0, 79, 1]);
ctx.onMidiMessageInternal([0x90, 68, 100]);
ctx.onMidiMessageInternal([0x90, 16, 100]);
ctx.onMidiMessageInternal([0xA0, 68, 50]);
ctx.onMidiMessageInternal([0x90, 8, 127]);
tick(1);
check(sets.length === before, "Back, the master knob, pads, steps, aftertouch, the volume's touch: not sent");
ctx.onMidiMessageInternal([0x90, 3, 127]);
check(lastIn() === 'T 3 1', 'knob 4 touched');
ctx.onMidiMessageInternal([0x80, 3, 0]);
check(lastIn() === 'T 3 0', 'and let go');

/* the knobs' curve. turn(k, n, msgs, dt): msgs CCs of n detents each, dt ms apart, each sent at the next tick; the
 * steps knob k's "K" tokens carried */
const enc = (n) => (n >= 0 ? n : 128 + n);
const toks = () => sets.flatMap((x) => (x[0] === 'in' ? x[1].split(';') : []));
const stepsOf = (pre) => toks().filter((t) => t.startsWith(pre + ' ')).reduce((a, t) => a + parseInt(t.split(' ')[1], 10), 0);
function turn(k, n, msgs, dt) {
    const per = [];
    for (let i = 0; i < msgs; i++) {
        const before = stepsOf('K' + k);
        ctx.onMidiMessageInternal([0xB0, 71 + k, enc(n)]);
        ctx.tick();
        per.push(stepsOf('K' + k) - before);
        clock += dt;
    }
    return per;
}
const rest = () => { clock += 1000; tick(1); };
rest();
sets.length = 0;
turn(4, 1, 10, 60);
check(stepsOf('K4') === 5, `slow (a detent each 60 ms): half a step a detent, 10 detents ${stepsOf('K4')} steps`);
rest();
sets.length = 0;
const burst = turn(4, 3, 60, 2);                 /* 1500 detents a second: a flick */
check(Math.max(...burst) <= 8 && stepsOf('K4') <= 2.5 * 180 + 1 && stepsOf('K4') >= 2.0 * 180,
      `a flick capped: at most 2.5 steps a detent (${stepsOf('K4')} for 180 detents, most ${Math.max(...burst)} a message)`);
rest();
sets.length = 0;
turn(4, 1, 2, 60);                               /* +1 step, half a step left over */
const rev = turn(4, -1, 1, 60);
check(stepsOf('K4') === 0 && rev[0] === -1, `a reversal starts fresh: its first detent a step back at once (${rev})`);
rest();
sets.length = 0;
turn(4, 1, 1, 60);
clock += 500;                                    /* (a pause: fresh again) */
const again = turn(4, 1, 1, 60);
check(again[0] === 1, 'after a pause the first detent moves at once');
rest();
sets.length = 0;
let swept = 0;                                   /* a brisk turn: a detent each 5 ms (~a turn a second) */
for (let total = 0; total < 127 && swept < 1000; swept++) { total += turn(4, 1, 1, 5)[0]; }
check(swept >= 210 && swept <= 315, `0..127 at a brisk turn takes ${swept} detents (${(swept / 210).toFixed(2)} turns of ~210)`);
rest();
sets.length = 0;
frameKinds = '01200000';                         /* knob 2 a short list, knob 3 the tempo (HOME's BPM) */
frameSeq++;
tick(40, 1);
turn(1, 1, 7, 60);
check(stepsOf('K1') === 2, `a short list: 3.5 detents an option (7 slow detents, ${stepsOf('K1')} options)`);
rest();
sets.length = 0;
turn(2, 1, 4, 60);
const pt = toks().filter((t) => t.startsWith('P2 '));
check(pt.length && pt.every((t) => / 0$/.test(t)) && pt.reduce((a, t) => a + parseInt(t.split(' ')[1], 10), 0) === 2 &&
      !toks().some((t) => t.startsWith('K2 ')), `the tempo knob, slow: tenths of BPM (${pt.join(';')})`);
rest();
sets.length = 0;
turn(2, 1, 21, 5);                               /* brisk: whole BPM */
const pc = toks().filter((t) => t.startsWith('P2 ') && / 1$/.test(t));
const whole = pc.reduce((a, t) => a + parseInt(t.split(' ')[1], 10), 0);
check(pc.length && whole % 10 === 0 && whole >= 20 && whole <= 40, `brisk: whole BPM (${pc.join(';')}: ${whole / 10} BPM in 20 detents)`);
rest();
sets.length = 0;
ctx.onMidiMessageInternal([0xB0, 49, 127]);       /* Shift + knob 1: the tempo on any page */
turn(0, 1, 3, 60);
ctx.onMidiMessageInternal([0xB0, 49, 0]);
turn(0, 1, 1, 60);
check(toks().some((t) => t.startsWith('P0 ')) && stepsOf('K0') === 1 && toks()[0] === 'B 49 127',
      `Shift + knob 1 the tempo (P0), after Shift knob 1 its page's again (${toks().join(';')})`);
frameKinds = '00000000';
frameSeq++;
tick(40, 1);

/* the tempo asked of Move, written to desired-tempo */
check(writes.length === 0, 'the first frame\'s request (asked before) is not written again');
const askAt = (b10) => { tqSeq = tqSeq === 255 ? 1 : tqSeq + 1; tqB10 = b10; };
sets.length = 0;
askAt(1205);
const t0 = clock;
tick(200, 1);
check(writes.length === 0, 'turned: nothing written while it may still move (250 ms of rest first)');
tick(120, 1);
check(writes.length === 1 && writes[0][1] === '/data/UserData/schwung/desired-tempo' && writes[0][2] === '120.5000\n' &&
      writes[0][0] - t0 >= 250 && writes[0][0] - t0 <= 300, `at rest: 120.5000 written ${writes[0] && writes[0][0] - t0} ms on`);
check(toks().includes('W ' + tqSeq + ' 1'), 'the DSP told it went (W n 1)');
const w0 = writes[0][0];
writes.length = 0;
const t1 = clock;                                /* three seconds of turning: a new value every 50 ms */
for (let i = 0; i < 60; i++) { askAt(1210 + i); tick(50, 1); }
const t2 = clock;
tick(2000, 1);
const wgaps = [writes[0][0] - w0].concat(writes.slice(1).map((w, i) => w[0] - writes[i][0]));
check(writes.length >= 3 && wgaps.every((g) => g >= 1050), `writes at least 1.05 s apart (${wgaps.join(', ')} ms)`);
check(writes.slice(0, -1).every((w) => w[0] < t2), 'turning: a write about once a second meanwhile');
const fin = writes[writes.length - 1];
check(fin[2] === ((1210 + 59) / 10).toFixed(4) + '\n' && writes.filter((w) => w[2] === fin[2]).length === 1 && fin[0] > t2 - 50,
      `the last value (126.9) written once, after the turn, ${fin[0] - t2} ms on (the write before 1.05 s earlier)`);
writes.length = 0;
writeOk = false;
sets.length = 0;
askAt(1300);
tick(1500, 1);
check(toks().includes('W ' + tqSeq + ' 0'), 'a write that fails: the DSP told (W n 0)');
writeOk = true;
writes.length = 0;
tick(3000, 1);
check(writes.length === 0, 'nothing new asked: nothing written');

/* a send the channel refused is kept for the next tick */
setOk = false;
sets.length = 0;
ctx.onMidiMessageInternal([0xB0, 86, 127]);
tick(1);
setOk = true;
tick(1);
check(sets.length === 1 && lastIn() === 'B 86 127', 'a refused send goes with the next tick');

/* parked, resumed */
const g = gets.length, l = leds.length, f = fills.length;
ctx.overtakeParked = true;
frameSeq++;
tick(50, 40);
ctx.overtakeParked = false;
check(gets.length === g && leds.length === l && fills.length === f, 'parked: no reads, no painting, no LEDs');
ctx.onResume();
fills.length = 0;
leds.length = 0;
tick(10, 35);
check(fills.filter((x) => x[2] === 128).length === 64 && leds.length === 63, 'resumed: every row and LED again');

if (fails) { console.log(`${fails} failed`); process.exit(1); }
console.log('ui.js: all passed');
