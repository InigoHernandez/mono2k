/* move/ui.js run in Node against a mock of Schwung's shadow UI and of dsp.so's params:
 *   node move/test/ui_test.mjs
 * The reads' rate, the bulk sets, PLAY, the track buttons with and without Shift, the LED budget, the parked tick. */
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const src = readFileSync(new URL('../ui.js', import.meta.url), 'utf8')
    .replace(/^import[\s\S]*?from\s+'[^']+';\s*$/gm, '');      /* the host's modules: given by the mock below */

let clock = 1000;
const sets = [], leds = [], gets = [];
const dsp = { play: 0, step: -1, track: 0 };
let calls = 0;
const ctx = {
    MovePlay: 85, MoveShift: 49, MoveRow1: 43, MoveRow2: 42, MoveRow3: 41, MoveRow4: 40,
    MoveSteps: Array.from({ length: 16 }, (_, i) => 16 + i),
    Black: 0, White: 120, DarkGrey: 119, Blue: 33, BrightGreen: 8,
    setLED: (n, c) => leds.push(['n', n, c]),
    setButtonLED: (n, c) => leds.push(['cc', n, c]),
    invalidateLedCache: () => {},
    clear_screen: () => calls++, print: () => calls++, fill_rect: () => calls++, draw_rect: () => calls++,
    text_width: (t) => t.length * 5, get_font_height: () => 8,
    host_module_get_params: (blob) => {
        gets.push(clock);
        const v = `${dsp.play} ${dsp.step} 120 ${dsp.track} 6 0 16 8888 410 650 A01 CIRCUIT`;
        return `1\n${v.length}\n${v}`;
    },
    host_module_set_params: (blob) => {
        const f = blob.split('\n');
        sets.push(blob);
        /* "2\n4\nplay1\n1": count, then <len>\n<bytes> records */
        let p = blob.indexOf('\n') + 1;
        const items = [];
        while (p < blob.length) {
            const q = blob.indexOf('\n', p), len = parseInt(blob.slice(p, q), 10);
            items.push(blob.substr(q + 1, len));
            p = q + 1 + len;
        }
        for (let i = 0; i + 1 < items.length; i += 2) dsp[items[i]] = parseInt(items[i + 1], 10);
        return f.length > 0;
    },
    Date: { now: () => clock },
};
ctx.globalThis = ctx;
vm.createContext(ctx);
vm.runInContext(src, ctx);

let fails = 0;
const check = (ok, what) => { console.log(`  ${ok ? 'ok  ' : 'FAIL'}  ${what}`); fails += !ok; };

ctx.init();
for (let i = 0; i < 100; i++) { ctx.tick(); clock += 1; }      /* 100 ticks over 100 ms */
check(gets.length >= 2 && gets.length <= 4, `reads at most ~30 a second (${gets.length} in 100 ms of ticks)`);
const perTick = [];
leds.length = 0;
ctx.init();
for (let i = 0; i < 3; i++) { const n = leds.length; ctx.tick(); perTick.push(leds.length - n); clock += 3; }
check(Math.max(...perTick) <= 16, `LEDs at most 16 a tick (${perTick.join(', ')})`);
check(calls > 0, 'the screen is drawn');

ctx.onMidiMessageInternal([0xB0, 85, 127]);
check(sets.at(-1) === '2\n4\nplay1\n1' && dsp.play === 1, 'PLAY sends play 1 as one bulk set');
ctx.onMidiMessageInternal([0xB0, 85, 0]);
check(sets.length === 1, 'the release sends nothing');
ctx.onMidiMessageInternal([0xB0, 42, 127]);
check(dsp.track === 1, 'track button 2 (CC 42): track 2');
ctx.onMidiMessageInternal([0xB0, 49, 127]);
ctx.onMidiMessageInternal([0xB0, 43, 127]);
check(dsp.track === 4, 'Shift + track button 1: track 5');
ctx.onMidiMessageInternal([0xB0, 42, 127]);
check(dsp.track === 5, 'Shift + track button 2: track 6');
const before = sets.length;
ctx.onMidiMessageInternal([0xB0, 41, 127]);
check(sets.length === before && dsp.track === 5, 'Shift + track button 3: nothing (6 tracks)');
ctx.onMidiMessageInternal([0xB0, 49, 0]);
ctx.onMidiMessageInternal([0xB0, 3, 127]);                    /* jog click: never taken (the escape is the host's) */
check(sets.length === before, 'a jog click sends nothing');

const g = gets.length, l = leds.length;
ctx.overtakeParked = true;
for (let i = 0; i < 50; i++) { ctx.tick(); clock += 40; }
ctx.overtakeParked = false;
check(gets.length === g && leds.length === l, 'parked: no reads, no LEDs');

if (fails) { console.log(`${fails} failed`); process.exit(1); }
console.log('ui.js: all passed');
