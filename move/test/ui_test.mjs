/* SPDX-License-Identifier: GPL-3.0-only */
/* move/ui.js run in Node against a mock of Schwung's shadow UI and of dsp.so's params:
 *   node move/test/ui_test.mjs
 * The controls forwarded (knobs batched and summed, the jog, buttons at once, the knobs' touch; nothing of the host's,
 * the pads' or the steps'), the frame read ~30 times a second and painted by rows and runs (only the rows that
 * changed, nothing for the same frame), the LEDs at most 16 a tick and to the right notes and CCs, a failed send
 * kept, the parked tick, the resume. */
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const src = readFileSync(new URL('../ui.js', import.meta.url), 'utf8')
    .replace(/^import[\s\S]*?from\s+'[^']+';\s*$/gm, '');      /* the host's modules: given by the mock below */

let clock = 1000;
const sets = [], gets = [], fills = [], leds = [];
let clears = 0, prints = 0, setOk = true;

/* the DSP's frame: "M2K1", its number, 64 rows of 22 characters, the LEDs (63, two hex digits each) */
const blankRow = '0'.repeat(22);
let frameRows = new Array(64).fill(blankRow), frameSeq = 1, frameLeds = new Array(63).fill(0);
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
    return 'M2K1' + frameSeq.toString(16).toUpperCase().padStart(8, '0') + frameRows.join('') + frameLeds.map(hex).join('');
}

const ctx = {
    MoveBack: 51, MoveMainKnob: 14, MoveMaster: 79,
    MoveSteps: Array.from({ length: 16 }, (_, i) => 16 + i),
    MovePads: Array.from({ length: 32 }, (_, i) => 68 + i),
    setLED: (n, c) => leds.push(['n', n, c]),
    setButtonLED: (n, c) => leds.push(['cc', n, c]),
    invalidateLedCache: () => {},
    decodeDelta: (v) => (v >= 1 && v <= 63 ? v : v >= 65 && v <= 127 ? v - 128 : 0),
    decodeAcceleratedDelta: (v) => 3 * (v >= 1 && v <= 63 ? v : v >= 65 && v <= 127 ? v - 128 : 0),
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
check(gets.length >= 3 && gets.length <= 4, `frames read ~30 a second (${gets.length} in 100 ms of ticks)`);
check(fills.length === 64 && fills.every((f) => f[2] === 128 && f[4] === 0), 'a blank first frame: each row cleared once');

/* a row with runs, then the same frame again, then one row changed */
fills.length = 0;
frameRows[3] = rowFrom('1'.repeat(10) + '0'.repeat(5) + '11' + '0'.repeat(100) + '1'.repeat(11));
frameSeq++;
tick(34, 1);
const r3 = fills.filter((f) => f[1] === 3);
check(fills.length === 4 && r3.length === 4, `only the changed row painted (${fills.length} calls)`);
check(r3[1][0] === 0 && r3[1][2] === 10 && r3[2][0] === 15 && r3[2][2] === 2 && r3[3][0] === 117 && r3[3][2] === 11,
      'its lit runs as fill_rects: 0+10, 15+2, 117+11 (to the edge)');
fills.length = 0;
tick(34, 1);
check(fills.length === 0, 'the same frame number: nothing painted');

/* the LEDs: at most 16 a tick, each to its note or CC */
frameLeds = Array.from({ length: 63 }, (_, i) => 1 + (i % 100));
frameSeq++;
leds.length = 0;
const per = [];
for (let i = 0; i < 6; i++) { const n = leds.length; tick(1, 34); per.push(leds.length - n); }
check(Math.max(...per) <= 16, `LEDs at most 16 a tick (${per.join(', ')})`);
check(leds.length === 63, `every LED sent once over the ticks (${leds.length})`);
const step1 = leds.find((l) => l[0] === 'n' && l[1] === 16), pad1 = leds.find((l) => l[0] === 'n' && l[1] === 68);
const trk1 = leds.find((l) => l[0] === 'cc' && l[1] === 43), rec = leds.find((l) => l[0] === 'cc' && l[1] === 86);
check(step1 && step1[2] === 1 && pad1 && pad1[2] === 17 && trk1 && trk1[2] === 49 && rec && rec[2] === 53,
      'steps on notes 16.., pads on 68.., track 1 on CC 43, Rec on CC 86');
check(!leds.some((l) => l[0] === 'cc' && l[1] === 85), "PLAY's LED left to Move");
leds.length = 0;
tick(3, 34);
check(leds.length === 0, 'unchanged LEDs: nothing sent');

/* the controls */
sets.length = 0;
ctx.onMidiMessageInternal([0xB0, 71, 1]);
ctx.onMidiMessageInternal([0xB0, 71, 2]);
ctx.onMidiMessageInternal([0xB0, 72, 127]);
check(sets.length === 0, 'knob turns wait for the tick');
tick(1);
check(sets.length === 1 && sets[0][0] === 'in' && lastIn() === 'K0 9;K1 -3', `one send, knob 1's turns summed (${lastIn()})`);
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
tick(10, 34);
check(fills.filter((x) => x[2] === 128).length === 64 && leds.length === 63, 'resumed: every row and LED again');

if (fails) { console.log(`${fails} failed`); process.exit(1); }
console.log('ui.js: all passed');
