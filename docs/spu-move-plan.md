# GRIT and the PS1-era reverb on the Move: the plan

Two of AFFINE's PS1-era sounds, Move only (the owner, 2026-11-17: too heavy for the FM-1, whose builds must not change).
Phase 1 (done): the DSP as standalone modules, tested and rendered. Phase 2 (this plan): wiring them into the bus and the
Move's controls, after the SKIP / CONSOLE / DUCK / LO-FI branch.

## Phase 1, as built

| What | Where |
|---|---|
| The flag `M2K_SPU` (the Move's builds alone) | `move/build.sh` (dsp.so), `web/build.sh` (`web/move/m2k_move.wasm`), `move/proof.sh` and `tools/check.sh` (the eight-track host). `firmware/src/m2k_core.c` includes `m2k_spu.c` under it, after `m2k_dsp.c`, before `m2k_fx.c` |
| GRIT and the reverb | `firmware/src/m2k_spu.c`: `spu_grit_block`, `spu_rv_block`; state `spu_g`, `spu_r`, the work area `srv_wk` (64 KB) |
| The chip's numbers and the laws | `tools/gen_spu.py` -> `firmware/gen/m2k_spu_tables.h` (psx-spx's tables, credited; the laws' tables) |
| Tests, measurements, renders | `host/m2k_spu_test.c` (self-test; `--rt --level --ir --measure --render --bench`) |

Nothing calls the modules yet. Proved byte for byte against HEAD (`fd20be0`):

- the FM-1 package: sha256 `dda5d944…` both ways, image 553 040 B;
- `m2k_draw --hash` (bank A, DENSE, FXS; one core and two), and the `m2k_draw` binary itself;
- `web/m2k.wasm` and `web/move/m2k_move.wasm` (built with the flag): the same bytes as the committed ones;
- the Move's `dsp.so` (built with the flag): the same bytes as without it;
- the eight-track host with the flag: `--render-hash` the same as the six's, and its self-test passes.

## The numbers

Mac instructions a sample, each module alone, built as the FM-1 builds the core:

| | On | Idle |
|---|---|---|
| The reverb, any type (A03's sends) | 300-302 | 3.8 asleep |
| The FDN it replaces, for scale | 100 | ~3 asleep |
| GRIT 11K (the colour proposed) | 302 | 2.4 (the call's check: 0 once the bus tests the flag inline) |
| GRIT 22K CLEAN / 8K CRUNCH | 387 / 155 | |

Memory: the work area 64 KB, the reverb's state 444 B, GRIT's 492 B, tables 8.5 KB (Move only).

Against AFFINE's float model of the same registers (its own code, built read-only in a scratch folder), the reverb's RT60
matches within 2-9 % and its octave spectrum within 0.5 dB. With a band-limited pulse the two waveforms match to -50 dB
on eight of nine types. SPACE ECHO is the exception: AFFINE plays it at 0.94 of the console's addresses (its span check
counts dAPF1 on top of the farthest address, 31 576 cells, which fits the 32 768-cell area), and MONO2K's plays them
exactly.

## Phase 2: the reverb

**In the bus.** In `bus_block`, where `reverb_block` is called:

```c
#ifdef M2K_SPU
    spu_rv_block(g[G_RVTY], verb, damp, rs, L, R);      /* a type: the chip's reverb; 0: at rest, at once */
    if (g[G_RVTY] && !shim) verb = 0;                    /* (the FDN's own 6 ms fade, as DCAY 0) */
#endif
    reverb_block(verb, damp, rs, L, R, shim);
```

- **Type 0 (FDN, the default)** runs the same code as today. `spu_rv_block(0, ...)` returns before it touches anything.
- **Switching** between FDN and a type: each side fades by itself. The FDN fades in 6 ms (its DCAY 0); the console's
  reverb fades in 5.8 ms, then empties. A new type starts from silence. At the switches the second difference was 214
  against the wet's own 429: no click.
- **SHIMMER** keeps its wash on the FDN, the only reverb with the lines it reads. While it plays over a console type,
  both reverbs run: the FDN takes SHIMMER's part of the send alone (`rs2 - rsend`; the sketch above passes it all, so
  phase 2 splits it), and the console's type takes the pattern's.

**The sends.** The tracks' VERB sends (mono, as the FDN hears them) go into the 16-bit cells at x2: bank A's loudest,
A03's, peaks at -19 dBFS. Every send at 127 saturates there, as the console's did: bounded, and no overflow under the
sanitizers.

**DCAY and DAMP (a proposal).** Both keep their meaning on every type, so a pattern keeps its time and colour when its
type changes:

- **DCAY** is the RT60 of the FDN's law.
  - The wall's gain (vWALL) is solved for it as g0^(RT0 / RT).
  - When the time is shorter than the type's own, the two all-passes scale the same way. They ring on their own: HALL's
    holds 1.8 s.
  - Measured, every reverb type is within ~10 % of DCAY's time from 0.3 to 5 s. At DCAY 110 (13 s) they land at 10-12 s.
  - HALF ECHO stops at 4.9 s: its 100-tick loop reaches the 0.992 cap.
- **DAMP** sets the reflections' one-pole (vIIR) on a log scale: open at 0, 700 Hz at 127.
- **The level out** follows the FDN's law: as loud as the FDN at the same DCAY and DAMP. Measured on A02, A03 and A04,
  every type is within about 1.5 dB of it at home.
- **Home** is where each type plays the console's own registers exactly. The table below gives each home.
- **DELAY** has no feedback of its own. Its echoes repeat for DCAY's time, and its home is DCAY 1: one echo.
- **ECHO's** own feedback (650 s) is past DCAY's 24 s, so its own law runs 0.3 s to 650 s at 127.

| Type | Own RT60 | Home DCAY | Home DAMP |
|---|---|---|---|
| ROOM | 0.84 s | 31 | 54 |
| STUDIO S | 0.97 s | 35 | 51 |
| STUDIO M | 0.99 s | 35 | 51 |
| STUDIO L | 2.34 s | 60 | 52 |
| HALL | 2.03 s | 56 | 65 |
| HALF ECHO | 1.38 s | 45 | 51 |
| SPACE ECHO | 4.12 s | 76 | 29 |
| ECHO (psx-spx's Chaos Echo) | 650 s | 127 | 0 |
| DELAY | one echo | 1 | 0 |

**Where it is chosen.**

- A Move-only pattern global, `G_RVTY` (0 FDN, 1..9 the types), after the POLY block and under `M2K_SPU`. The Move's
  `G_COUNT` goes 59 -> 60; the FM-1's stays 49.
- On the Move it is **GLO's knob 6, VERB**, next to DCAY DAMP ADPC DRIV and MUTQ: FDN ROOM STUDIO S … DELAY. The screen
  shows the type's name and marks its home on the DCAY and DAMP knobs. It could also be PERFORM's knob 5.
- Saved only when used, the MONO2K way:
  - **Format 26** is 24's layout plus the byte; **27** is 25's (eight wide) plus the byte.
  - A pattern packs as 26/27 only when `G_RVTY` is not 0 (`pat_rvty`); otherwise it packs as 18-25, byte for byte.
  - Builds without the flag (the FM-1, the six-track browser and host) read 26/27 and drop the byte: they play the FDN.
  - That is a change to the FM-1's loader (`pat_unpack` accepts up to 27, `G_COUNT_OF` knows the byte), in the FM-1's
    next release. Its renders and bank A stay the same.
  - Until then, an FM-1 1.11 that is handed such a pattern (through the editor or a project file) loads it blank.
  - The other route is question 5.

## Phase 2: GRIT

**In the bus.** GRIT goes after `bus_block`'s sample loop, on the block's output: after the limiter, KILL and GATE, in
the codec's own 16 bits. The renders were made exactly there.

```c
#ifdef M2K_SPU
    if (perf.on[PF_GRIT] || spu_g.run) spu_grit_block(out, M2K_SUB, perf.on[PF_GRIT], 0);
#endif
```

**The colour.** The colour proposed is character 0, **11K**: 11.025 kHz, filtered, CRUNCH 0.2, the console's gaussian
playback. It is AFFINE's POLY PAD colour. Why:

- It is the console's commonest sample rate.
- The interpolator's dull, velvety top is the sound most people remember.
- The codec's grain shows on the tails.
- It is furthest from CRUSH, which is bright 4-bit IMA steps near the full rate.

The alternatives were rendered too: 22K CLEAN (AFFINE's SUBTLE GLUE: subtle) and 8K CRUNCH (CRUNCHY TAIL: RAW, harsh).

**On and off.**

- Held: it runs unheard for 3 ms until its playback reads what it encoded, then fades in over 10 ms. Let go: it fades
  out over 10 ms, then idles.
- Its ~3 ms latency is crossfaded over, never jumped. At the toggles the high band peaked at 77.5 dB against the
  music's 94.
- Latched and let go by `pf_unlatch`, it fades out by itself, with no tail to keep. Nothing else is needed.

**The pad.** GRIT takes the 4th slot of the top row on PERFORM's page 2 on the Move (the other branch reserves it).
`PF_GRIT` exists only under `M2K_SPU`, so `PF_COUNT` grows only in the Move's builds; `pf_lat` stays under 32. The FM-1
gets no key. Its name goes into the pad tables copied in `web/move/gear.js` and `web/move/move.js`.

## The browser's playable Move

`web/move/m2k_move.wasm` already builds with the flag. Phase 2 brings it the VERB knob and GRIT's pad through the same C
controller, and three edits to the site:

- GRIT's name in `gear.js` / `move.js`'s pad tables;
- the `V` bump and `gear.js?v=`;
- the Move manual's line on VERB and GRIT.

The work area fits the wasm's 8 MB. Both modules on cost about 600 Mac instructions a sample more at worst.

## CPU against the Move's budget

- The Move renders on one core: A01..A04 cost 1983 / 2789 / 3336 / 3980 Mac instructions a sample, and DENSE 5374
  (`m2k_draw --cpu`, one core).
- A console type adds +200 over the FDN it replaces, and GRIT +302 while held. The worst case, DENSE with both, comes to
  about 5880 (+9 %).
- The Move's README measures A04 at 25 µs a 128-frame block in a hot loop (161 µs paced, cold) of the block's 2902 µs,
  with about 2 ms usable. Scaled up, the worst case stays under 10 % of that.
- Phase 2 measures it on the device with `move/test/bench.c time`.

## What bank A on the Move keeps byte for byte

With type 0 and GRIT off, the bus runs today's code: the FDN called as before, `spu_rv_block` out at its first test, and
GRIT's test false. Phase 2 proves it with:

- `m2k_draw --hash` and the FM-1 package, unchanged: the six-track builds don't compile the module;
- the eight-track host with the flag, whose `--render-hash` equals the six's (check.sh's eight-track step, now built with
  the flag);
- `move/proof.sh`'s dsp.so hashes for A01..A04.

## Phase 2 checklist

- Wire as above, `m2k_init` calling `spu_init()` (an init silences everything it owns). Then add `host/m2k_spu_test.c` to `tools/check.sh` (warnings, self-test, ASan + UBSan) and a self-test
  case in the eight-track host: a type chosen and given back renders as the FDN once its fade is over.
- Add the eight-track host's `-DM2K_SPU` and the tests to `CLAUDE.md`, and the lessons to the handbook.
- Send renders to the owner, then a Move test package; publish only at the owner's word.

## Questions for the owner

1. **DCAY and DAMP on the console's types.**
   - Keep their meaning (time and colour; each type exact at its home), or play each type's own registers whatever they
     say?
   - Hear `a03_room_dcay51_damp118` and `a03_hall_dcay51_damp118`.
2. **DELAY, ECHO and SPACE ECHO on the tempo?**
   - Their 0.74 s isn't on bank A's beat. AFFINE scales them to an eighth or a dotted sixteenth.
   - That needs the addresses scaled when the tempo changes: a fade and clear, or AFFINE's per-tick glide (+170 while
     it glides).
3. **GRIT's character:** 11K (proposed), 22K CLEAN, or 8K CRUNCH?
4. **The name.** GND-PERC already has a knob called GRIT (knob G, "8-BIT GRIT"). Keep GRIT for the pad, or another name?
5. **Where the type is saved.**
   - Route A, format 26/27: the FM-1's next release reads the byte and drops it.
   - Route B, a Move-only store section ('V' after 'A'): the FM-1 doesn't change at all, but the type stays in the Move's
     store and doesn't travel with a pattern copied or exported.
   - Recommended: A.
6. **The VERB knob:** GLO's knob 6 (proposed), and PERFORM's knob 5 too?
7. **SHIMMER over a console type:** its wash on the FDN (proposed)?

## Credits

The laws are after AFFINE's PS1-era bus (`affine/app/dsp/psx.c`, MPL-2.0), read and written again, none of its code.
The chip's numbers come from psx-spx: the register sets, the resampler's taps, the ADPCM filters and the gaussian table.
They are credited in `README.md` and `tools/gen_spu.py`.
