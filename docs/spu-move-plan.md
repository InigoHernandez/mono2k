# GRIT and the PS1-era reverbs on the Move

Two of AFFINE's PS1-era sounds, on the Move alone (the owner, 2026-11-17: too heavy for the FM-1, whose builds must not
change). Phase 1 built and measured the DSP; phase 2 (2026-11-18) wired it into the bus and the Move's controls with
the owner's defaults (below), on top of PERFORM bank C. This is the record as built.

## What is where

| What | Where |
|---|---|
| The flag `M2K_SPU` (the Move's builds alone) | `move/build.sh` (dsp.so), `web/build.sh` (`web/move/m2k_move.wasm`), `move/proof.sh` and `tools/check.sh` (the eight-track host), the sheets (`move/screen/sheets.c`'s line). `firmware/src/m2k_core.c` includes `m2k_spu.c` under it, after `m2k_dsp.c`, before `m2k_fx.c` |
| GRIT and the reverbs | `firmware/src/m2k_spu.c`: `spu_grit_block`, `spu_rv_block`, `spu_init`; state `spu_g`, `srv[2]` (two reverbs, 128 KB each) |
| The chip's numbers, the laws | `tools/gen_spu.py` -> `firmware/gen/m2k_spu_tables.h` (psx-spx's tables, credited) |
| The bus | `m2k_fx.c` `bus_block`: the type before `reverb_block`, GRIT before bank C's colours |
| The pattern | `G_RVTY` (`m2k.h`, under the flag), formats 26 and 27 (`m2k_proj.c`) |
| PERFORM | `PF_GRIT` (under the flag); `PF_KEYS`, the FM-1's black keys' effects, leaves it out |
| The Move | GLO's knob 6 VERB (`m2k_screen.c`), PERFORM page 2's 4th top key (`m2k_move_ui.c`), the view's `rv rvsc grit` (`m2k_move.c`) |
| Tests | `host/m2k_spu_test.c` (in `check.sh`), the host's self-test (format 26: both ways), `tools/ref/mutate_formats.py` (4 mutants of 26 and 27), the Move's bench (`move/test/bench.c`: VERB, GRIT, the lock) |

## The decisions (the owner's defaults, 2026-11-18)

- **GRIT** is one character, **11K**: 11.025 kHz, filtered, CRUNCH 0.2, the chip's gaussian playback (AFFINE's POLY PAD
  colour).
  - It is a colour on the Move's PERFORM page 2, its 4th top key.
  - It plays on the bus's output after the limiter, KILL and GATE, and before bank C's colours. That is AFFINE's order:
    the codec, then the console.
  - It glides in and out over 10 ms, after 3 ms unheard while its playback fills.
  - Double-tapped, it stays on. `pf_unlatch` lets it go, and it fades by itself.
  - The FM-1 has no GRIT key: `PF_KEYS` keeps it off the black keys' banks wherever the flag is on.
  - GND-PERC's "8-BIT GRIT" knob keeps its name.
- **The reverb type** is `G_RVTY`, the pattern's: `(v * 10) >> 7`, where 0 is the FDN and 1..9 are ROOM, STUDIO S,
  STUDIO M, STUDIO L, HALL, HALF ECHO, SPACE ECHO, ECHO (psx-spx's Chaos Echo) and DELAY.
  - It is chosen on GLO's knob 6, VERB. GLO's picture names the type at its tail's far end.
  - Type 0 runs the FDN's path as ever: the chip's reverb returns at its first test.
  - With a type chosen, the FDN rests (DCAY 0's fade). SHIMMER's wash still plays on the FDN, fed SHIMMER's part of the
    send alone.
- **DCAY and DAMP keep their meaning** on every type, and each type is exact at its home: its registers as psx-spx gives
  them.
  - DCAY is the FDN's RT60 law. The wall's gain is g0^(RT0/RT); shorter than the type's own, the all-passes scale too.
    Measured, every type is within ~10 % of DCAY's time from 0.3 s to 5 s.
  - DAMP is the reflections' one-pole on a log scale, open to 700 Hz.
  - The level is matched to the FDN at the same DCAY and DAMP, within ~1.5 dB on A02, A03 and A04.
- **Changing type crossfades.** The sounding reverb stops taking sends and fades out over 23 ms on its own tail, while
  the other starts at once. That needs two reverbs, two work areas.
- **DELAY, ECHO and SPACE ECHO lock to the tempo**, as AFFINE's DUB SPACE plays SPACE ECHO on the beat.
  - The whole set of addresses is scaled so the feedback loop (mLSAME - dLSAME) is the division nearest its own, by ratio,
    that the area holds: a sixteenth to a whole note, dotted ones between.
  - The tempo comes from the bus's record (`bsps`).
  - A new tempo glides as AFFINE's SIZE glide does. The farthest head moves 0.2 cells a tick at most, a bend of 3
    semitones at most. Reads fall between two cells, and each write head writes every cell it passes.
  - The work area is 128 KB on the Move, so the lock can reach twice the console's own (a half note from DELAY's 0.742 s).
  - DCAY's time is not made up for the scale: a shorter echo dies sooner.
  - The other six keep their registers whatever the tempo.
- **Formats 26 and 27** are 24's and 25's layouts with the type's byte after the globals.
  - A pattern packs as 26/27 only when its choice is a type. The knob's FDN, a byte of 6, packs as before and comes back
    0: the same choice.
  - Every build reads 26 and 27. A build without the flag (the FM-1's 1.12, the browser's) passes the byte over, so the
    pattern plays its FDN.
  - `BLOB_ANY` grows by one byte (4321), so a store holding a format-27 pattern is read anywhere.
  - The FM-1's only change is this reading.

## The numbers

Mac instructions a sample, each module alone, built as the FM-1 builds the core (`m2k_spu_test --bench`):

| | Cost |
|---|---|
| The reverb, any type, still (A03's sends) | 276 (the FDN: 100) |
| DELAY gliding to a new tempo | ~600 while it glides (389 averaged with 35 % of the blocks gliding) |
| Two at once on a type change | twice, for 23 ms |
| Asleep | 7.3 |
| GRIT 11K (the colour) / 22K CLEAN / 8K CRUNCH | 302 / 388 / 154 |
| GRIT off | 0 (the bus tests its flag first) |

Bank A's whole render on one core is 1983 (A01) to 3980 (A04), and DENSE 5374. DENSE with a type in the FDN's place
and GRIT on comes to about 5850 (+9 %), and about 6180 (+15 %) while DELAY glides.

The Move's build, the proof's `bench time` (dsp.so in the container on the Mac's cores, a 128-frame block of 2902 us):
A04 27.9 us (1.0 %), with DELAY 29.7 us (the `+DELY` row), with GRIT held too 33.4 us (1.2 %, `+GRIT`).

Memory, the Move's alone: two work areas of 128 KB, the reverbs' state (2 x 684 B), GRIT's 492 B, tables 8.5 KB.

Against AFFINE's float model of the same registers, built read-only in a scratch folder:

- RT60 matches within 2-9 % and the octave spectrum within 0.5 dB.
- With a band-limited pulse, the waveforms match to -50 dB on 8 of 9 types.
- AFFINE plays SPACE ECHO at 0.94 of the console's addresses; MONO2K plays the exact registers.

## What stays byte for byte

- The FM-1 and the six-track builds compile none of the DSP. `m2k_draw --hash` (bank A, DENSE, FXS; one core and two)
  is the effects branch's.
- The eight-track host with the flag renders bank A, DENSE and a pattern of six as the six-track host does
  (`--render-hash`).
- With type 0 and GRIT off, the bus runs the effects branch's code. The proof's dsp.so hashes for A01..A04 hold it.

## Bugs found on the way

- **The output interpolator's window was a tick off** (phase 1). On a block's last tick its newest tap read a cell past
  `oh[sd]`: for the left, the right side's history; for the right, what followed in the struct. The tap is -1/16384, so
  the error was 1-3 LSB. ASan cannot see an overrun inside one struct; a struct grown by the second work area moved what
  it read, and the renders changed by 3 LSB at 2.8 s. Fixed.
- **The test harness's lengths must be whole blocks.** A key pressed at `f == FS / 2` never fires when f steps by 16.

## Open, for the owner's ear

- The character of GRIT (renders of the three in `build/move/spu/`).
- The DCAY and DAMP laws on the console's types, and the lock (`build/move/spu2/`).

## Credits

The laws are after AFFINE's PS1-era bus (`affine/app/dsp/psx.c`, MPL-2.0), read and written again, none of its code.
The chip's numbers come from psx-spx: the register sets, the resampler's taps, the ADPCM filters and the gaussian table.
They are credited in `README.md`, `tools/gen_spu.py` and the Move's manual.
