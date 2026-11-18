# Four of AFFINE's effects in PERFORM (1.12 / the Move's 0.7)

The owner's picks (2026-11-17): SKIP, CONSOLE, DUCK and LO-FI, on both devices, each a key with a fixed character (no
knob, no pattern format change). Read from AFFINE's float code (`app/dsp/psx.c`, `app/dsp/ovl.c`, MPL-2.0) for the laws
and numbers only, and written again here in integers. Brief: `../AFFINE-EFFECTS-FOR-MONO2K.md`.

| Effect | Kind | Law (integers) | Where | FM-1 key | Move pad | Cost while on (measured) |
|---|---|---|---|---|---|---|
| SKIP | a move (the ring) | at each sixteenth, 9 in 10 (`bus.grng`): the last sixteenth repeats, 1-3 times; its read slows from 1 by 0.45 a slice (AFFINE's SEEK 0.3), floored at 1/4; every restart and the way in and out crossfaded over 2.9 ms | `GM_SKIP` in `gl_tick`, reading `gl_buf`; the grid from `bus_step` (`brec`), stopped a free sixteenth at the tempo | bank C, black 1 | page 2, bottom 7 | +82 (a repeat's +60) |
| CONSOLE | a colour | two one-poles (TPT) from 20 kHz to 8 kHz with the amount (`T_CON_G`), a DC blocker at 10 Hz, the sides narrowed to mono, then `x - 0.1 x^3`; the amount glides in 10 ms, the width in 93 ms | a pass over the bus's block after the limiter (the TV speaker last, as AFFINE has it) | bank C, black 2 | page 2, top 1 | +45 settled (+100 its first 93 ms) |
| DUCK | a colour | `1 - 0.7 (1 - beat phase)^3` a beat (CHOPPAH's), the dip's start an S over 3 ms, the key's depth over 5 ms | the same pass, last | bank C, black 3 | page 2, top 2 | +47 |
| LO-FI | a colour | a sample held at a clock of 11 kHz whose period wanders +-45 % (jitter 0.5), 10 bits with TPDF dither (`bus.grng`); in and out over 10 ms | the same pass, first | bank C, black 4 | page 2, top 3 | +27 |

- Costs: Mac instructions a sample over the dry, one core, `m2k_draw --perform`, the same on A01-A04.
- Off, each costs one test a block and the sound is bank A's byte for byte (`m2k_draw --hash`, the 30 s renders).
- Memory: no new buffer; SKIP reads `gl_buf` as the repeats do. About 100 bytes of state in `bus`.
- The alternatives rendered for the owner's ears: DUCK at 1.0 (AFFINE's amount), and a second LO-FI (8 kHz, jitter
  0.8, 8 bits).
- FM-1: PERFORM's third bank C (OCT+ from B; OCT- back C, B, A; both OCT lit on C); its keys 5-11 empty, unlit.
- Move: Up swaps both rows: page 2's top row CONSOLE, DUCK, LO-FI, the 4th kept for GRIT; its bottom row as before with
  SKIP in the 7th.
- Credits: AFFINE for SKIP, CONSOLE and LO-FI; CHOPPAH (Laurens Profittlich) for DUCK's idea.
