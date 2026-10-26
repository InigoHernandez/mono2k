<!-- The design sheet the per-engine INITs came from (2026-10-26). Take A of each engine is built in: MACH_INIT in firmware/src/m2k_seq.c. B and C are kept here so the owner can swap one in: replace that engine's row with its paste-ready line. The sheet's WAVs were scratch renders and are not kept.  Every ATK, DEC, REL and LFO SPD / DPTH value here (the paste-ready lines and the text) is on pattern format 16's laws: the sheet was made on format 15's, and its values went through feel_v15's tables (T_AMP15, T_SPD15, T_DPTH15), so each take sounds as it was rendered: the DEC and REL times quoted still hold, an ATK's is the old law's (the new rise, matched by its loudness, shows about 0.73 of it). -->

# MONO2K: new INIT sounds, sheet 1

Every take is MONO2K's engine (host/m2k_host.c through `inits.c`) on a fresh INIT pattern (`pattern_init_blank`: 120 BPM, reverb decay 40, bus drive 0), the machine on track 1 with kit_default's chain and MACH_DEF's page, then the changes listed. 4 trigs over 2 s (steps 1 5 9 13). Tonal engines play A2 C3 E3 A3 (bass parts) or A3 C4 E4 A4 (keys, leads, pads, voice, metal); DRM-KICK plays C2 four times (track 1's new trigs take C2); the other drums and noise C4 four times (the middle key, the note new trigs take on tracks 2-6). FX machines are on track 2 after a plain SWAV-SAW (its init page and chain, LEVL 127 VOL 127: its loudest) on track 1, the FX track trigged with it.

Each WAV: today's init, then A, B, C; each take its 2 s phrase and a 0.5 s gap its tail fades across. `00_overview_A_new_order.wav`: A of every engine in the proposed order. A, B and C are levelled to -18 LUFS (BS.1770, tools/sheet/loud.py, over the 2.5 s take) through LEVL, then VOL; no take over -1 dBFS and no track clip (the levelling stops there: the short drums and the FX stop at the peak or at VOL 127; their LUFS is in the table). Today's takes are as they are.

Measures: centroid (Hz, energy-weighted), flatness (spectral, 50 Hz-16 kHz: noise near 0.55, a band of noise or a tone near 0), dec30 (the last hit to -30 dB from its peak; 'hold': it does not fall), clarity (normalized autocorrelation peak 15-95 ms after each trig: 1 a clear pitch, under 0.3 noise), harm (energy on the note's first 16 harmonics), sus (the last hit 250 ms on against its peak, dB), width (side over mid, dB; -44 is mono), p20 (the dominant partial at 20 ms against 150 ms, semitones: a pitch-drop 'laser' on a drum reads high; meaningless on noise), cdrop (the centroid's fall from 0-20 ms to 40-100 ms, octaves: a filter 'sword' on a drum reads high).

## 1. Today's inits, measured

All on the common phrase (tonal A2 C3 E3 A3; noise and drums C4, the kick also C2), the FX after a plain SWAV-SAW at its init level (what a player hears today).

| Engine | Note | LUFS | Peak | Centroid | Flatness | dec30 s | Clarity | Harm | sus dB | Width dB |
|---|---|---|---|---|---|---|---|---|---|---|
| GND-SIN | LOW | -22.5 | -13.8 | 158 | 0.000 | 0.86 | 1.00 | 0.99 | -8.7 | -44.2 |
| SID-6581 | LOW | -26.6 | -19.5 | 165 | 0.000 | 0.88 | 1.00 | 0.99 | -8.6 | -44.2 |
| FM+STAT | LOW | -29.2 | -16.0 | 245 | 0.000 | 0.84 | 1.00 | 0.33 | -9.5 | -44.1 |
| FM+DYN | LOW | -28.1 | -14.9 | 338 | 0.000 | 0.79 | 1.00 | 0.22 | -10.0 | -44.1 |
| SWAV-SAW | LOW | -42.3 | -24.0 | 769 | 0.004 | 0.83 | 1.00 | 0.94 | -10.2 | -44.0 |
| SWAV-PUL | LOW | -38.6 | -26.2 | 371 | 0.000 | 0.88 | 1.00 | 0.97 | -9.7 | -44.1 |
| SWAV-ENS | LOW | -30.0 | -14.8 | 617 | 0.008 | 0.67 | 0.54 | 0.33 | -12.7 | -44.1 |
| DIGI-WAV | LOW | -27.8 | -16.6 | 317 | 0.001 | 0.87 | 1.00 | 0.98 | -9.5 | -44.2 |
| VOX-6 | LOW | -40.2 | -27.8 | 637 | 0.000 | 1.39 | 1.00 | 0.93 | 5.8 | -44.0 |
| FM+PAR | LOW | -26.7 | -16.7 | 846 | 0.000 | 0.84 | 1.00 | 0.30 | -8.1 | -44.2 |
| GND-NOIS | C4 | -24.1 | -15.0 | 7410 | 0.551 | 0.88 | 0.06 | 0.09 | -9.0 | -44.2 |
| GND-NOIS | C2 | -24.1 | -15.0 | 7361 | 0.552 | 0.88 | 0.06 | 0.03 | -9.0 | -44.2 |
| GND-PERC | C4 | -21.2 | -12.1 | 7410 | 0.551 | 0.88 | 0.06 | 0.09 | -9.0 | -44.2 |
| GND-PERC | C2 | -21.2 | -12.1 | 7361 | 0.552 | 0.88 | 0.06 | 0.03 | -9.0 | -44.2 |
| DRM-KICK | C4 | -22.9 | -12.9 | 329 | 0.000 | 0.86 | 0.67 | 0.99 | -9.7 | -44.2 |
| DRM-KICK | C2 | -24.8 | -11.4 | 87 | 0.000 | 0.77 | 0.97 | 0.99 | -8.5 | -44.2 |
| DRM-SNAR | C4 | -25.2 | -14.4 | 2610 | 0.206 | 0.85 | 0.59 | 0.66 | -9.4 | -44.2 |
| DRM-SNAR | C2 | -25.9 | -13.2 | 2634 | 0.216 | 0.84 | 0.60 | 0.62 | -8.1 | -44.2 |
| DRM-HATS | C4 | -42.6 | -20.2 | 2534 | 0.176 | 0.04 | 0.26 | 0.17 | -170.7 | -44.1 |
| DRM-HATS | C2 | -43.9 | -22.8 | 1104 | 0.053 | 0.04 | 0.35 | 0.16 | -170.6 | -44.1 |
| DRM-CYMB | C4 | -24.2 | -11.6 | 3182 | 0.202 | 0.47 | 0.14 | 0.17 | -17.0 | -44.2 |
| DRM-CYMB | C2 | -26.1 | -13.0 | 1164 | 0.051 | 0.45 | 0.20 | 0.19 | -18.4 | -44.2 |
| SRC (SWAV-SAW alone) | LOW | -42.3 | -24.0 | 769 | 0.004 | 0.83 | 1.00 | 0.94 | -10.2 | -44.0 |
| FX-CHORS | LOW | -39.5 | -20.8 | 830 | 0.004 | 0.75 | 1.00 | 0.93 | -11.2 | -15.9 |
| FX-DYNMX | LOW | -37.1 | -11.8 | 830 | 0.009 | 0.69 | 1.00 | 0.93 | -12.7 | -44.0 |
| FX-RING | LOW | -41.6 | -23.8 | 714 | 0.004 | 0.81 | 1.00 | 0.95 | -10.7 | -43.9 |

Which sound alike. One distance over the spectrum's shape (24 bands, 40 dB deep: 6 dB RMS a unit), the decay (a doubling a unit), flatness (0.15), clarity (0.3), width and sustain (10 dB each); under 1.0 is alike, 1.0-1.5 close:

| Pair | Distance | Shape apart |
|---|---|---|
| GND-NOIS ~ GND-PERC | 0.00 | 0.0 dB |
| SWAV-SAW ~ FX-RING | 0.22 | 1.3 dB |
| SWAV-SAW ~ FX-DYNMX | 0.45 | 1.6 dB |
| FX-DYNMX ~ FX-RING | 0.48 | 2.3 dB |
| SWAV-PUL ~ DIGI-WAV | 0.66 | 4.0 dB |
| SWAV-PUL ~ FX-RING | 1.03 | 6.1 dB |
| FM+DYN ~ FM+PAR | 1.15 | 6.8 dB |
| SWAV-SAW ~ SWAV-PUL | 1.16 | 6.9 dB |
| SWAV-PUL ~ FX-DYNMX | 1.24 | 6.9 dB |
| GND-SIN ~ SID-6581 | 1.34 | 8.0 dB |
| FM+STAT ~ FM+DYN | 1.35 | 8.1 dB |
| FX-CHORS ~ FX-RING | 1.42 | 0.8 dB |
| SWAV-SAW ~ FX-CHORS | 1.44 | 1.5 dB |
| FX-CHORS ~ FX-DYNMX | 1.47 | 2.1 dB |

13 of 19 fall to -30 dB in 0.75-0.90 s (DEC 64 shared by every machine, 79 on format 16's laws); levels span 21.4 dB (-42.6 to -21.2 LUFS).

## 2. The engines, in the proposed order: purpose, A B C

Changes from kit_default + MACH_DEF only; the LEVL/VOL each take was levelled to are listed apart (the factory-style line at the end of each includes them). Values in brackets are readings of the value.

### 01 GND-SIN (machine 0): the sine bass: round, warm low end with a touch of fold and 2nd harmonic

`01_GND-SIN.wav`. Page: FOLD SUB NOIS PDEP PDEC CLIK HARM TUNE.

- **A** (A2 C3 E3 A3): FOLD 18, HARM 50, AMP HOLD 24 (24/32 beat), AMP DEC 113 (9.8 s to -60 dB), AMP REL 37 (183 ms to -60 dB). Level: LEVL 104. Measured -17.9 LUFS, peak -12.2 dBFS.
  `SA, 18, SG, 50, P_HOLD, 24, P_DEC, 113, P_REL, 37, P_LEVL, 104`
- **B** (A2 C3 E3 A3): SUB 48, AMP ATK 1 (2 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 49 (355 ms to -60 dB), GATE 56 (3.5 step). Level: LEVL 95. Measured -17.8 LUFS, peak -14.8 dBFS.
  `SB, 48, P_ATK, 1, P_HOLD, 127, P_DEC, 127, P_REL, 49, P_GATE, 56, P_LEVL, 95`
- **C** (A2 C3 E3 A3): FOLD 56, HARM 24, AMP HOLD 6 (6/32 beat), AMP DEC 66 (900 ms to -60 dB), AMP REL 49 (355 ms to -60 dB), WDTH 40 (LP 8 x note), LPQ 24. Level: LEVL 109. Measured -17.8 LUFS, peak -9.4 dBFS.
  `SA, 56, SG, 24, P_HOLD, 6, P_DEC, 66, P_REL, 49, P_WDTH, 40, P_LPQ, 24, P_LEVL, 109`

### 02 SWAV-SAW (machine 5): the supersaw: wide, lush pads and leads, big basses

`02_SWAV-SAW.wav`. Page: UNIL UNIW UNIX DRFT SUBX SUB1 SUB2 TUNE.

- **A** (A3 C4 E4 A4): UNIL 100, UNIW 60, UNIX 80, DRFT 10, AMP ATK 14 (23 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 87 (2.6 s to -60 dB), GATE 56 (3.5 step), BASE 8 (HP 0.5 x note), WDTH 40 (LP 16 x note), LPQ 20, SPU 40. Level: LEVL 127, VOL 112. Measured -17.9 LUFS, peak -5.0 dBFS.
  `SA, 100, SB, 60, SC, 80, SD, 10, P_ATK, 14, P_HOLD, 127, P_DEC, 127, P_REL, 87, P_GATE, 56, P_BASE, 8, P_WDTH, 40, P_LPQ, 20, P_SPU, 40, P_LEVL, 127, P_VOL, 112`
- **B** (A2 C3 E3 A3): UNIL 90, UNIW 24, UNIX 60, SUB1 60, AMP HOLD 10 (10/32 beat), AMP DEC 69 (1.0 s to -60 dB), AMP REL 49 (355 ms to -60 dB), WDTH 32 (LP 4 x note), LPQ 30. Level: LEVL 127, VOL 127. Measured -18.7 LUFS, peak -3.1 dBFS.
  `SA, 90, SB, 24, SC, 60, SF, 60, P_HOLD, 10, P_DEC, 69, P_REL, 49, P_WDTH, 32, P_LPQ, 30, P_LEVL, 127, P_VOL, 127`
- **C** (A3 C4 E4 A4): UNIL 100, UNIW 30, UNIX 60, AMP ATK 7 (10 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 61 (690 ms to -60 dB), GATE 48 (3 step), BASE 8 (HP 0.5 x note), WDTH 30 (LP 6.73 x note), LPQ 24, FATK 36, FDEC 40, WOF 84, SPU 30. Level: LEVL 127, VOL 118. Measured -17.8 LUFS, peak -4.6 dBFS.
  `SA, 100, SB, 30, SC, 60, P_ATK, 7, P_HOLD, 127, P_DEC, 127, P_REL, 61, P_GATE, 48, P_BASE, 8, P_WDTH, 30, P_LPQ, 24, P_FATK, 36, P_FDEC, 40, P_WOF, 84, P_SPU, 30, P_LEVL, 127, P_VOL, 118`

### 03 SWAV-PUL (machine 10): the pulse: a hollow reed lead, a warm PWM pad, a square bass

`03_SWAV-PUL.wav`. Page: UNIL UNIW SUB1 SUB2 PW PWAD PWRS TUNE.

- **A** (A3 C4 E4 A4): AMP ATK 11 (17 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 54 (463 ms to -60 dB), GATE 48 (3 step), BASE 8 (HP 0.5 x note), WDTH 36 (LP 11.3 x note), LPQ 12, SPU 24, LFO1 -> TUNE (SIN, 5.0 Hz at 120, depth +3). Level: LEVL 127, VOL 127. Measured -18.2 LUFS, peak -10.6 dBFS.
  `P_ATK, 11, P_HOLD, 127, P_DEC, 127, P_REL, 54, P_GATE, 48, P_BASE, 8, P_WDTH, 36, P_LPQ, 12, P_SPU, 24, L1 + L_DEST, 8, L1 + L_WAVE, 27, L1 + L_MULT, 72, L1 + L_SPD, 52, L1 + L_DPTH, 78, P_LEVL, 127, P_VOL, 127`
- **B** (A3 C4 E4 A4): UNIL 80, UNIW 50, PW 40, PWAD 10, AMP ATK 36 (119 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 60 (3.8 step), BASE 8 (HP 0.5 x note), WDTH 40 (LP 16 x note), LPQ 16, SPU 40. Level: LEVL 127, VOL 116. Measured -17.8 LUFS, peak -4.5 dBFS.
  `SA, 80, SB, 50, SE, 40, SF, 10, P_ATK, 36, P_HOLD, 127, P_DEC, 127, P_REL, 100, P_GATE, 60, P_BASE, 8, P_WDTH, 40, P_LPQ, 16, P_SPU, 40, P_LEVL, 127, P_VOL, 116`
- **C** (A2 C3 E3 A3): SUB1 20, AMP HOLD 8 (8/32 beat), AMP DEC 71 (1.2 s to -60 dB), AMP REL 49 (355 ms to -60 dB), WDTH 34 (LP 4.76 x note), LPQ 20. Level: LEVL 127, VOL 127. Measured -18.4 LUFS, peak -7.4 dBFS.
  `SC, 20, P_HOLD, 8, P_DEC, 71, P_REL, 49, P_WDTH, 34, P_LPQ, 20, P_LEVL, 127, P_VOL, 127`

### 04 SWAV-ENS (machine 6): chords on one key: a minor-7 pad, a dub stab, open fifths

`04_SWAV-ENS.wav`. Page: PCH2 PCH3 PCH4 WAVE PW CHRL CHRW TUNE.

- **A** (A3 C4 E4 A4): PCH2 73 (+3), PCH4 116 (+10), WAVE 82, PW 56, CHRL 44, CHRW 71, AMP ATK 11 (18 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 113 (9.8 s to -60 dB), BASE 10 (HP 0.59 x note), WDTH 46 (LP 32 x note), LPQ 40, GATE 60 (3.8 step), SPU 52, MODT 21, MODR 18, MODD 83, MODM 58. Level: LEVL 127, VOL 124. Measured -17.9 LUFS, peak -4.9 dBFS.
  `SA, 73, SC, 116, SD, 82, SE, 56, SF, 44, SG, 71, P_ATK, 11, P_HOLD, 127, P_DEC, 127, P_REL, 113, P_BASE, 10, P_WDTH, 46, P_LPQ, 40, P_GATE, 60, P_SPU, 52, P_MODT, 21, P_MODR, 18, P_MODD, 83, P_MODM, 58, P_LEVL, 127, P_VOL, 124`
- **B** (A3 C4 E4 A4): PCH2 73 (+3), PCH4 116 (+10), CHRL 40, CHRW 60, AMP HOLD 4 (4/32 beat), AMP DEC 49 (355 ms to -60 dB), AMP REL 37 (183 ms to -60 dB), BASE 8 (HP 0.5 x note), WDTH 34 (LP 9.51 x note), LPQ 30, DTIM 48 (3/16), DSND 98 (28 %), DFB 56, DBAS 40, DWID 60. Level: LEVL 127, VOL 104. Measured -19.0 LUFS, peak -1.1 dBFS.
  `SA, 73, SC, 116, SF, 40, SG, 60, P_HOLD, 4, P_DEC, 49, P_REL, 37, P_BASE, 8, P_WDTH, 34, P_LPQ, 30, P_DTIM, 48, P_DSND, 98, P_DFB, 56, P_DBAS, 40, P_DWID, 60, P_LEVL, 127, P_VOL, 104`
- **C** (A3 C4 E4 A4): PCH2 100 (+7), PCH3 125 (+12), PCH4 0 (-12), WAVE 64, CHRL 70, CHRW 90, AMP ATK 24 (53 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), BASE 8 (HP 0.5 x note), WDTH 38 (LP 13.5 x note), LPQ 16, GATE 60 (3.8 step), SPU 40. Level: LEVL 127, VOL 94. Measured -17.8 LUFS, peak -3.4 dBFS.
  `SA, 100, SB, 125, SC, 0, SD, 64, SF, 70, SG, 90, P_ATK, 24, P_HOLD, 127, P_DEC, 127, P_REL, 100, P_BASE, 8, P_WDTH, 38, P_LPQ, 16, P_GATE, 60, P_SPU, 40, P_LEVL, 127, P_VOL, 94`

### 05 FM+STAT (machine 3): FM keys: an e-piano, a soft bell, a mallet

`05_FM+STAT.wav`. Page: 1FRQ 1FIN 1ENV 1FB 2FRQ 2VOL TONE TUNE.

- **A** (A3 C4 E4 A4): 1FRQ 98 (x1.75), 1FIN 63, 1ENV 105, 1FB 8, 2FRQ 109 (x2.5), 2VOL 19, TONE 55, AMP HOLD 7 (7/32 beat), AMP DEC 80 (1.9 s to -60 dB), AMP REL 93 (3.6 s to -60 dB), BASE 9 (HP 0.55 x note), WDTH 54 (LP 58.7 x note), HPQ 7, LPQ 49, DTIM 48 (3/16), DSND 88 (14 %), DFB 43, DWID 92, SPU 22. Level: LEVL 127, VOL 81. Measured -17.8 LUFS, peak -9.6 dBFS.
  `SA, 98, SB, 63, SC, 105, SD, 8, SE, 109, SF, 19, SG, 55, P_HOLD, 7, P_DEC, 80, P_REL, 93, P_BASE, 9, P_WDTH, 54, P_HPQ, 7, P_LPQ, 49, P_DTIM, 48, P_DSND, 88, P_DFB, 43, P_DWID, 92, P_SPU, 22, P_LEVL, 127, P_VOL, 81`
- **B** (A3 C4 E4 A4): 1FRQ 125 (x4), 1ENV 96, 1FB 0, 2VOL 0, TONE 96, AMP DEC 95 (3.9 s to -60 dB), AMP REL 95 (3.9 s to -60 dB), SPU 40. Level: LEVL 127, VOL 76. Measured -17.9 LUFS, peak -9.2 dBFS.
  `SA, 125, SC, 96, SD, 0, SF, 0, SG, 96, P_DEC, 95, P_REL, 95, P_SPU, 40, P_LEVL, 127, P_VOL, 76`
- **C** (A3 C4 E4 A4): 1FRQ 114 (x3), 1ENV 104, 1FB 0, 2FRQ 104 (x2), 2VOL 10, TONE 70, AMP DEC 54 (463 ms to -60 dB), AMP REL 49 (355 ms to -60 dB), SPU 24. Level: LEVL 127, VOL 113. Measured -17.8 LUFS, peak -3.0 dBFS.
  `SA, 114, SC, 104, SD, 0, SE, 104, SF, 10, SG, 70, P_DEC, 54, P_REL, 49, P_SPU, 24, P_LEVL, 127, P_VOL, 113`

### 06 FM+PAR (machine 9): FM metal: a gong-like bell, a kalimba tine, a clang

`06_FM+PAR.wav`. Page: 1FRQ 1ENV 2FRQ 2ENV 3FRQ 3ENV TONE TUNE.

- **A** (A3 C4 E4 A4): 1FRQ 120 (x3.5), 1ENV 80, 2FRQ 98 (x1.75), 2ENV 76, 3FRQ 88 (x1.25), 3ENV 72, AMP DEC 92 (3.4 s to -60 dB), AMP REL 92 (3.4 s to -60 dB), SPU 40. Level: LEVL 127, VOL 67. Measured -17.9 LUFS, peak -11.0 dBFS.
  `SA, 120, SB, 80, SC, 98, SD, 76, SE, 88, SF, 72, P_DEC, 92, P_REL, 92, P_SPU, 40, P_LEVL, 127, P_VOL, 67`
- **B** (A3 C4 E4 A4): 1FRQ 125 (x4), 1ENV 100, 2ENV 24, 3FRQ 82 (x1), 3ENV 0, TONE 60, AMP DEC 61 (690 ms to -60 dB), AMP REL 49 (355 ms to -60 dB), SPU 24. Level: LEVL 127, VOL 97. Measured -17.8 LUFS, peak -5.3 dBFS.
  `SA, 125, SB, 100, SD, 24, SE, 82, SF, 0, SG, 60, P_DEC, 61, P_REL, 49, P_SPU, 24, P_LEVL, 127, P_VOL, 97`
- **C** (A3 C4 E4 A4): 1FRQ 114 (x3), 1ENV 44, 2FRQ 109 (x2.5), 2ENV 40, 3FRQ 93 (x1.5), 3ENV 36, TONE 70, AMP HOLD 4 (4/32 beat), AMP DEC 54 (463 ms to -60 dB), AMP REL 44 (272 ms to -60 dB), BASE 16 (HP 1 x note), SPU 20. Level: LEVL 127, VOL 127. Measured -18.2 LUFS, peak -5.3 dBFS.
  `SA, 114, SB, 44, SC, 109, SD, 40, SE, 93, SF, 36, SG, 70, P_HOLD, 4, P_DEC, 54, P_REL, 44, P_BASE, 16, P_SPU, 20, P_LEVL, 127, P_VOL, 127`

### 07 FM+DYN (machine 4): the FM bass: a modulator whose depth moves, a pluck that turns warm

`07_FM+DYN.wav`. Page: 1FRQ 1FEN 1VOL 1VEN 2FRQ 2ENV 2FB TUNE.

- **A** (A2 C3 E3 A3): 1VOL 32, 1VEN 96, 2FRQ 127 (x4), 2ENV 90, 2FB 0, AMP HOLD 10 (10/32 beat), AMP DEC 69 (1.0 s to -60 dB), AMP REL 49 (355 ms to -60 dB), WDTH 40 (LP 8 x note), LPQ 16. Level: LEVL 127, VOL 72. Measured -17.9 LUFS, peak -10.9 dBFS.
  `SC, 32, SD, 96, SE, 127, SF, 90, SG, 0, P_HOLD, 10, P_DEC, 69, P_REL, 49, P_WDTH, 40, P_LPQ, 16, P_LEVL, 127, P_VOL, 72`
- **B** (A2 C3 E3 A3): 1VOL 0, 1VEN 64, 2FRQ 64 (x1), 2ENV 56, 2FB 72, AMP HOLD 10 (10/32 beat), AMP DEC 74 (1.3 s to -60 dB), AMP REL 49 (355 ms to -60 dB), WDTH 40 (LP 8 x note), LPQ 30. Level: LEVL 127, VOL 75. Measured -17.9 LUFS, peak -5.9 dBFS.
  `SC, 0, SD, 64, SE, 64, SF, 56, SG, 72, P_HOLD, 10, P_DEC, 74, P_REL, 49, P_WDTH, 40, P_LPQ, 30, P_LEVL, 127, P_VOL, 75`
- **C** (A2 C3 E3 A3): 1FRQ 127 (x2), 1VOL 56, 1VEN 80, 2FRQ 127 (x4), 2ENV 40, 2FB 0, AMP HOLD 16 (16/32 beat), AMP REL 49 (355 ms to -60 dB), WDTH 44 (LP 11.3 x note), LPQ 12. Level: LEVL 127, VOL 75. Measured -18.0 LUFS, peak -12.5 dBFS.
  `SA, 127, SC, 56, SD, 80, SE, 127, SF, 40, SG, 0, P_HOLD, 16, P_REL, 49, P_WDTH, 44, P_LPQ, 12, P_LEVL, 127, P_VOL, 75`

### 08 VOX-6 (machine 8): the voice: an 'aah' choir, an 'ooh', a breathy 'oh'

`08_VOX-6.wav`. Page: VOC1 VOC2 V-SW VOIC CONS CLEN CVOL TUNE.

- **A** (A3 C4 E4 A4): VOC1 120 (F1 651 Hz), VOC2 56 (F2 1045 Hz), VOIC 10, AMP ATK 14 (23 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 87 (2.6 s to -60 dB), GATE 60 (3.8 step), SPU 50, MODT 21, MODR 30, MODD 80, MODM 60, LFO1 -> TUNE (SIN, 5.0 Hz at 120, depth +2). Level: LEVL 127, VOL 115. Measured -17.9 LUFS, peak -9.7 dBFS.
  `SA, 120, SB, 56, SD, 10, P_ATK, 14, P_HOLD, 127, P_DEC, 127, P_REL, 87, P_GATE, 60, P_SPU, 50, P_MODT, 21, P_MODR, 30, P_MODD, 80, P_MODM, 60, L1 + L_DEST, 8, L1 + L_WAVE, 27, L1 + L_MULT, 72, L1 + L_SPD, 52, L1 + L_DPTH, 75, P_LEVL, 127, P_VOL, 115`
- **B** (A3 C4 E4 A4): VOC1 40 (F1 292 Hz), VOC2 36 (F2 801 Hz), AMP ATK 7 (10 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 74 (1.3 s to -60 dB), GATE 60 (3.8 step), SPU 40, LFO1 -> TUNE (SIN, 5.0 Hz at 120, depth +2). Level: LEVL 127, VOL 113. Measured -17.8 LUFS, peak -9.5 dBFS.
  `SA, 40, SB, 36, P_ATK, 7, P_HOLD, 127, P_DEC, 127, P_REL, 74, P_GATE, 60, P_SPU, 40, L1 + L_DEST, 8, L1 + L_WAVE, 27, L1 + L_MULT, 72, L1 + L_SPD, 52, L1 + L_DPTH, 75, P_LEVL, 127, P_VOL, 113`
- **C** (A3 C4 E4 A4): VOC1 84 (F1 468 Hz), VOC2 32 (F2 776 Hz), VOIC 60, AMP ATK 4 (6 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 61 (690 ms to -60 dB), GATE 48 (3 step), SPU 40. Level: LEVL 127, VOL 109. Measured -17.8 LUFS, peak -7.3 dBFS.
  `SA, 84, SB, 32, SD, 60, P_ATK, 4, P_HOLD, 127, P_DEC, 127, P_REL, 61, P_GATE, 48, P_SPU, 40, P_LEVL, 127, P_VOL, 109`

### 09 DIGI-WAV (machine 7): the wavetable voice: a pluck that scans as it falls, a moving pad, an organ

`09_DIGI-WAV.wav`. Page: WAVE SCAN STIM SYNC BITS DTUN MIX2 STEP.

- **A** (A3 C4 E4 A4): WAVE 13 (TRI), SCAN 70, STIM 76, DTUN 65 (+12 c), MIX2 40, AMP DEC 71 (1.2 s to -60 dB), AMP REL 61 (690 ms to -60 dB), BASE 8 (HP 0.5 x note), WDTH 52 (LP 45.3 x note), SPU 30. Level: LEVL 127, VOL 99. Measured -17.9 LUFS, peak -1.3 dBFS.
  `SA, 13, SB, 70, SC, 76, SF, 65, SG, 40, P_DEC, 71, P_REL, 61, P_BASE, 8, P_WDTH, 52, P_SPU, 30, P_LEVL, 127, P_VOL, 99`
- **B** (A3 C4 E4 A4): WBNK 127, WAVE 16 (OS09), SCAN 72, STIM 110, DTUN 66 (+25 c), MIX2 70, AMP ATK 24 (53 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 60 (3.8 step), BASE 8 (HP 0.5 x note), WDTH 48 (LP 32 x note), LPQ 16, SPU 40. Level: LEVL 127, VOL 65. Measured -17.8 LUFS, peak -5.8 dBFS.
  `P_WBNK, 127, SA, 16, SB, 72, SC, 110, SF, 66, SG, 70, P_ATK, 24, P_HOLD, 127, P_DEC, 127, P_REL, 100, P_GATE, 60, P_BASE, 8, P_WDTH, 48, P_LPQ, 16, P_SPU, 40, P_LEVL, 127, P_VOL, 65`
- **C** (A3 C4 E4 A4): WAVE 16 (ORGN), DTUN 64 (+0 c), AMP ATK 1 (2 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 37 (183 ms to -60 dB), GATE 48 (3 step), BASE 8 (HP 0.5 x note), WDTH 44 (LP 22.6 x note), SPU 24. Level: LEVL 114. Measured -17.8 LUFS, peak -9.4 dBFS.
  `SA, 16, SF, 64, P_ATK, 1, P_HOLD, 127, P_DEC, 127, P_REL, 37, P_GATE, 48, P_BASE, 8, P_WDTH, 44, P_SPU, 24, P_LEVL, 114`

### 10 SID-6581 (machine 2): the chip voice: a sync lead, a PWM bass, the nasal combined wave

`10_SID-6581.wav`. Page: PW PWAD PWRS WAVE MOD MSRC MFRQ TUNE.

- **A** (A3 C4 E4 A4): WAVE 38 (SAW), MOD 80 (SYNC), MFRQ 80 (740 Hz), AMP ATK 1 (2 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 61 (690 ms to -60 dB), GATE 48 (3 step), BASE 8 (HP 0.5 x note), WDTH 36 (LP 11.3 x note), LPQ 20, SPU 30. Level: LEVL 105. Measured -17.8 LUFS, peak -10.2 dBFS.
  `SD, 38, SE, 80, SG, 80, P_ATK, 1, P_HOLD, 127, P_DEC, 127, P_REL, 61, P_GATE, 48, P_BASE, 8, P_WDTH, 36, P_LPQ, 20, P_SPU, 30, P_LEVL, 105`
- **B** (A2 C3 E3 A3): PW 32, PWAD 6, WAVE 64 (PULS), AMP HOLD 8 (8/32 beat), AMP DEC 74 (1.3 s to -60 dB), AMP REL 49 (355 ms to -60 dB), WDTH 36 (LP 5.66 x note), LPQ 30. Level: LEVL 106. Measured -17.8 LUFS, peak -7.3 dBFS.
  `SA, 32, SB, 6, SD, 64, P_HOLD, 8, P_DEC, 74, P_REL, 49, P_WDTH, 36, P_LPQ, 30, P_LEVL, 106`
- **C** (A3 C4 E4 A4): WAVE 89 (MIX), AMP HOLD 4 (4/32 beat), AMP DEC 61 (690 ms to -60 dB), AMP REL 49 (355 ms to -60 dB), BASE 8 (HP 0.5 x note), WDTH 48 (LP 32 x note), LPQ 10, SPU 24. Level: LEVL 127, VOL 127. Measured -20.2 LUFS, peak -7.8 dBFS.
  `SD, 89, P_HOLD, 4, P_DEC, 61, P_REL, 49, P_BASE, 8, P_WDTH, 48, P_LPQ, 10, P_SPU, 24, P_LEVL, 127, P_VOL, 127`

### 11 DRM-KICK (machine 15): the kick

`11_DRM-KICK.wav`. Page: WAVE PDEP PDEC PCRV FM FMFR SNAP CLIK.

- **A** (C2 x4): WAVE 16 (SIN), PDEP 18 (x1.6 at the hit), PDEC 10 (13 ms), PCRV 12, SNAP 30, CLIK 30, AMP HOLD 10 (10/32 beat), AMP DEC 51 (405 ms to -60 dB), AMP REL 26 (94 ms to -60 dB). Level: LEVL 127, VOL 68. Measured -18.0 LUFS, peak -8.4 dBFS.
  `SA, 16, SB, 18, SC, 10, SD, 12, SG, 30, SH, 30, P_HOLD, 10, P_DEC, 51, P_REL, 26, P_LEVL, 127, P_VOL, 68`
- **B** (C2 x4): WAVE 2 (SIN), PDEP 22 (x2 at the hit), PDEC 8 (10 ms), PCRV 12, FM 34, FMFR 27, SNAP 72, CLIK 76, AMP HOLD 40 (40/32 beat), AMP DEC 97 (4.4 s to -60 dB), AMP REL 15 (42 ms to -60 dB), BASE 22 (HP 1.7 x note), WDTH 115 (LP open), HPQ 51, LPQ 63, FATK 55, FDEC 74, BOF 33, WOF 56, EQF 93, EQG 41 (-23). Level: LEVL 103. Measured -17.9 LUFS, peak -9.0 dBFS.
  `SA, 2, SB, 22, SC, 8, SD, 12, SE, 34, SF, 27, SG, 72, SH, 76, P_HOLD, 40, P_DEC, 97, P_REL, 15, P_BASE, 22, P_WDTH, 115, P_HPQ, 51, P_LPQ, 63, P_FATK, 55, P_FDEC, 74, P_BOF, 33, P_WOF, 56, P_EQF, 93, P_EQG, 41, P_LEVL, 103`
- **C** (C2 x4): WAVE 48 (TRI), PDEP 26 (x2.3 at the hit), PDEC 6 (8 ms), PCRV 12, SNAP 50, CLIK 50, AMP HOLD 4 (4/32 beat), AMP DEC 44 (272 ms to -60 dB), AMP REL 26 (94 ms to -60 dB), WDTH 60 (LP 45.3 x note). Level: LEVL 127, VOL 87. Measured -17.8 LUFS, peak -3.1 dBFS.
  `SA, 48, SB, 26, SC, 6, SD, 12, SG, 50, SH, 50, P_HOLD, 4, P_DEC, 44, P_REL, 26, P_WDTH, 60, P_LEVL, 127, P_VOL, 87`

### 12 DRM-SNAR (machine 16): the snare and the clap

`12_DRM-SNAR.wav`. Page: TONE PDEP PDEC NOIS MIX NFLT CLAP CSPC.

- **A** (C4 x4): TONE 16 (SIN), PDEP 0 (no sweep), PDEC 0 (1 ms), NOIS 100 (17.4 kHz), MIX 80, AMP DEC 35 (160 ms to -60 dB), AMP REL 20 (63 ms to -60 dB), BASE 8 (HP 0.5 x note), WDTH 52 (LP 45.3 x note), HPQ 18, LPQ 9, EQF 72, EQG 53 (-11), SPU 24. Level: LEVL 127, VOL 105. Measured -20.0 LUFS, peak -1.0 dBFS.
  `SA, 16, SB, 0, SC, 0, SD, 100, SE, 80, P_DEC, 35, P_REL, 20, P_BASE, 8, P_WDTH, 52, P_HPQ, 18, P_LPQ, 9, P_EQF, 72, P_EQG, 53, P_SPU, 24, P_LEVL, 127, P_VOL, 105`
- **B** (C4 x4): TONE 16 (SIN), PDEP 0 (no sweep), PDEC 0 (1 ms), NOIS 110 (19.1 kHz), MIX 118, NFLT 90 (HP 654 Hz), CLAP 32 (4 hits), CSPC 8 (10 ms), AMP DEC 42 (238 ms to -60 dB), AMP REL 26 (94 ms to -60 dB), SPU 40. Level: LEVL 127, VOL 114. Measured -21.6 LUFS, peak -1.1 dBFS.
  `SA, 16, SB, 0, SC, 0, SD, 110, SE, 118, SF, 90, SG, 32, SH, 8, P_DEC, 42, P_REL, 26, P_SPU, 40, P_LEVL, 127, P_VOL, 114`
- **C** (C4 x4): TONE 16 (SIN), PDEP 0 (no sweep), PDEC 0 (1 ms), NOIS 110 (19.1 kHz), MIX 110, NFLT 30 (LP 2.0 kHz), AMP ATK 2 (3 ms), AMP DEC 44 (272 ms to -60 dB), AMP REL 26 (94 ms to -60 dB), SPU 24. Level: LEVL 127, VOL 127. Measured -19.0 LUFS, peak -1.7 dBFS.
  `SA, 16, SB, 0, SC, 0, SD, 110, SE, 110, SF, 30, P_ATK, 2, P_DEC, 44, P_REL, 26, P_SPU, 24, P_LEVL, 127, P_VOL, 127`

### 13 DRM-HATS (machine 17): the hats: closed, open, metallic

`13_DRM-HATS.wav`. Page: CARR M1FR M1 M2FR M2 WAVE CDEC OPEN.

- **A** (C4 x4): CARR 127 (+63 st), M1FR 70 (+6 st), M1 10, M2FR 84 (+20 st), M2 100, WAVE 117 (NOIS), CDEC 12 (152 ms), AMP HOLD 7 (7/32 beat), AMP DEC 65 (842 ms to -60 dB), AMP REL 20 (59 ms to -60 dB), VOL 100, BASE 46 (HP 13 x note), WDTH 40 (LP open), HPQ 23, FATK 21, FDEC 20, BOF 71, EQF 108, EQG 80 (+16). Level: LEVL 127, VOL 121. Measured -20.5 LUFS, peak -1.0 dBFS.
  `SA, 127, SB, 70, SC, 10, SD, 84, SE, 100, SF, 117, SG, 12, P_HOLD, 7, P_DEC, 65, P_REL, 20, P_VOL, 121, P_BASE, 46, P_WDTH, 40, P_HPQ, 23, P_FATK, 21, P_FDEC, 20, P_BOF, 71, P_EQF, 108, P_EQG, 80, P_LEVL, 127`
- **B** (C4 x4): CARR 127 (+63 st), M1FR 70 (+6 st), M1 10, M2FR 84 (+20 st), M2 100, WAVE 117 (NOIS), CDEC 12 (152 ms), OPEN 40 (668 ms), AMP HOLD 7 (7/32 beat), AMP DEC 87 (2.6 s to -60 dB), AMP REL 49 (355 ms to -60 dB), VOL 100, BASE 46 (HP 13 x note), WDTH 40 (LP open), HPQ 23, EQF 108, EQG 80 (+16). Level: LEVL 125, VOL 100. Measured -18.3 LUFS, peak -1.0 dBFS.
  `SA, 127, SB, 70, SC, 10, SD, 84, SE, 100, SF, 117, SG, 12, SH, 40, P_HOLD, 7, P_DEC, 87, P_REL, 49, P_VOL, 100, P_BASE, 46, P_WDTH, 40, P_HPQ, 23, P_EQF, 108, P_EQG, 80, P_LEVL, 125`
- **C** (C4 x4): CARR 100 (+36 st), M1FR 107 (+43 st), M1 110, M2FR 124 (+60 st), M2 20, WAVE 13 (SIN), CDEC 8 (98 ms), AMP HOLD 4 (4/32 beat), AMP DEC 49 (355 ms to -60 dB), AMP REL 20 (59 ms to -60 dB), BASE 40 (HP 8 x note), WDTH 60 (LP open). Level: LEVL 127, VOL 127. Measured -24.3 LUFS, peak -5.7 dBFS.
  `SA, 100, SB, 107, SC, 110, SD, 124, SE, 20, SF, 13, SG, 8, P_HOLD, 4, P_DEC, 49, P_REL, 20, P_BASE, 40, P_WDTH, 60, P_LEVL, 127, P_VOL, 127`

### 14 DRM-CYMB (machine 18): the cymbals: a crash, a ride, a swell

`14_DRM-CYMB.wav`. Page: CARR M1FR M1 M2FR M2 WAVE ATK DEC.

- **A** (C4 x4): CARR 94 (+30 st), M1FR 83 (+19 st), M1 90, M2FR 119 (+55 st), M2 100, WAVE 65 (SAW), DEC 90 (3.5 s), AMP DEC 100 (5.1 s to -60 dB), AMP REL 74 (1.3 s to -60 dB), BASE 36 (HP 5.7 x note), WDTH 20 (LP 32 x note), SPU 30. Level: LEVL 127, VOL 101. Measured -17.9 LUFS, peak -3.3 dBFS.
  `SA, 94, SB, 83, SC, 90, SD, 119, SE, 100, SF, 65, SH, 90, P_DEC, 100, P_REL, 74, P_BASE, 36, P_WDTH, 20, P_SPU, 30, P_LEVL, 127, P_VOL, 101`
- **B** (C4 x4): CARR 88 (+24 st), M1FR 95 (+31 st), M1 100, M2FR 114 (+50 st), M2 20, WAVE 13 (SIN), DEC 60 (1.3 s), AMP DEC 100 (5.1 s to -60 dB), AMP REL 74 (1.3 s to -60 dB), BASE 32 (HP 4 x note), WDTH 60 (LP open), SPU 24. Level: LEVL 127, VOL 86. Measured -17.8 LUFS, peak -4.1 dBFS.
  `SA, 88, SB, 95, SC, 100, SD, 114, SE, 20, SF, 13, SH, 60, P_DEC, 100, P_REL, 74, P_BASE, 32, P_WDTH, 60, P_SPU, 24, P_LEVL, 127, P_VOL, 86`
- **C** (C4 x4): CARR 94 (+30 st), M1FR 83 (+19 st), M1 90, M2FR 119 (+55 st), M2 100, WAVE 65 (SAW), ATK 80 (247 ms), DEC 40 (668 ms), AMP DEC 100 (5.1 s to -60 dB), AMP REL 74 (1.3 s to -60 dB), BASE 36 (HP 5.7 x note), WDTH 20 (LP 32 x note), SPU 50. Level: LEVL 127, VOL 127. Measured -21.2 LUFS, peak -2.7 dBFS.
  `SA, 94, SB, 83, SC, 90, SD, 119, SE, 100, SF, 65, SG, 80, SH, 40, P_DEC, 100, P_REL, 74, P_BASE, 36, P_WDTH, 20, P_SPU, 50, P_LEVL, 127, P_VOL, 127`

### 15 GND-PERC (machine 1): metal and wood percussion: a metal hit, a shaker, a woody tok

`15_GND-PERC.wav`. Page: COLR S&H META MSPR PDEP PDEC GRIT -.

- **A** (C4 x4): META 127, AMP DEC 42 (238 ms to -60 dB), BASE 24 (HP 2 x note), WDTH 48 (LP open), SPU 20. Level: LEVL 127, VOL 103. Measured -19.1 LUFS, peak -1.1 dBFS.
  `SC, 127, P_DEC, 42, P_BASE, 24, P_WDTH, 48, P_SPU, 20, P_LEVL, 127, P_VOL, 103`
- **B** (C4 x4): COLR 80, AMP ATK 7 (10 ms), AMP DEC 42 (238 ms to -60 dB), AMP REL 26 (94 ms to -60 dB), BASE 40 (HP 8 x note), WDTH 16 (LP 32 x note). Level: LEVL 127, VOL 117. Measured -17.8 LUFS, peak -2.0 dBFS.
  `SA, 80, P_ATK, 7, P_DEC, 42, P_REL, 26, P_BASE, 40, P_WDTH, 16, P_LEVL, 127, P_VOL, 117`
- **C** (C4 x4): S&H 60, COLR 40, GRIT 30, AMP DEC 32 (140 ms to -60 dB), BASE 12 (HP 0.71 x note), WDTH 24 (LP 5.66 x note), LPQ 64. Level: LEVL 127, VOL 119. Measured -24.4 LUFS, peak -1.1 dBFS.
  `SB, 60, SA, 40, SG, 30, P_DEC, 32, P_BASE, 12, P_WDTH, 24, P_LPQ, 64, P_LEVL, 127, P_VOL, 119`

### 16 GND-NOIS (machine 14): noise you play: wind, sea, breath (the filter follows the key)

`16_GND-NOIS.wav`. Page: ST RED STON - - - - TUNE.

- **A** (A3 C4 E4 A4): ST 64, RED 64, STON 127, AMP ATK 24 (53 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 87 (2.6 s to -60 dB), GATE 48 (3 step), BASE 12 (HP 0.71 x note), WDTH 20 (LP 4 x note), HPQ 20, LPQ 56, SPU 40. Level: LEVL 78. Measured -17.8 LUFS, peak -9.2 dBFS.
  `SA, 64, SB, 64, SC, 127, P_ATK, 24, P_HOLD, 127, P_DEC, 127, P_REL, 87, P_GATE, 48, P_BASE, 12, P_WDTH, 20, P_HPQ, 20, P_LPQ, 56, P_SPU, 40, P_LEVL, 78`
- **B** (C4 x4): ST 127, RED 127, STON 127, AMP ATK 66 (608 ms), AMP HOLD 127 (while the gate is open), AMP DEC 127 (none), AMP REL 106 (7.1 s to -60 dB), GATE 127 (7.9 step), BASE 8 (HP 0.5 x note), WDTH 40 (LP 16 x note), SPU 30. Level: LEVL 127, VOL 110. Measured -17.8 LUFS, peak -6.9 dBFS.
  `SA, 127, SB, 127, SC, 127, P_ATK, 66, P_HOLD, 127, P_DEC, 127, P_REL, 106, P_GATE, 127, P_BASE, 8, P_WDTH, 40, P_SPU, 30, P_LEVL, 127, P_VOL, 110`
- **C** (C4 x4): ST 40, RED 64, STON 127, AMP ATK 7 (10 ms), AMP DEC 49 (355 ms to -60 dB), AMP REL 37 (183 ms to -60 dB), BASE 24 (HP 2 x note), WDTH 16 (LP 8 x note), LPQ 20, SPU 20. Level: LEVL 127, VOL 122. Measured -18.2 LUFS, peak -1.1 dBFS.
  `SA, 40, SB, 64, SC, 127, P_ATK, 7, P_DEC, 49, P_REL, 37, P_BASE, 24, P_WDTH, 16, P_LPQ, 20, P_SPU, 20, P_LEVL, 127, P_VOL, 122`

### 17 FX-CHORS (machine 11): chorus: the track before made wide; a comb that rings; a flanger

`17_FX-CHORS.wav`. Page: DEL DEP SPD MIX FB WID LP INP.

- **A** (A2 C3 E3 A3): DEP 60, MIX 127, WID 127, LP 100, INP 127, AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 127. Measured -21.5 LUFS, peak -4.8 dBFS.
  `SB, 60, SD, 127, SF, 127, SG, 100, SH, 127, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 127`
- **B** (A2 C3 E3 A3): DEL 20, DEP 0, SPD 0, MIX 127, FB 122, WID 60, LP 90, INP 127, AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 127. Measured -18.8 LUFS, peak -3.3 dBFS.
  `SA, 20, SB, 0, SC, 0, SD, 127, SE, 122, SF, 60, SG, 90, SH, 127, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 127`
- **C** (A2 C3 E3 A3): DEL 4, DEP 100, SPD 10, FB 100, WID 100, LP 110, INP 127, AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 127. Measured -20.7 LUFS, peak -3.6 dBFS.
  `SA, 4, SB, 100, SC, 10, SE, 100, SF, 100, SG, 110, SH, 127, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 127`

### 18 FX-DYNMX (machine 13): dynamics: the track before punched and held, squashed, glued in parallel

`18_FX-DYNMX.wav`. Page: ATK REL THRS MIX RAT GAIN RMS INP.

- **A** (A2 C3 E3 A3): ATK 70, REL 30, THRS 50, RAT 120, GAIN 100, WDTH 48 (LP 16 x note), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 78. Measured -17.9 LUFS, peak -3.8 dBFS.
  `SA, 70, SB, 30, SC, 50, SE, 120, SF, 100, P_WDTH, 48, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 78`
- **B** (A2 C3 E3 A3): ATK 10, THRS 50, RAT 115, GAIN 110, RMS 64, WDTH 40 (LP 8 x note), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 126. Measured -20.0 LUFS, peak -1.1 dBFS.
  `SA, 10, SC, 50, SE, 115, SF, 110, SG, 64, P_WDTH, 40, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 126`
- **C** (A2 C3 E3 A3): ATK 10, THRS 40, MIX 64, RAT 110, GAIN 110, RMS 64, WDTH 48 (LP 16 x note), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 127. Measured -21.8 LUFS, peak -1.4 dBFS.
  `SA, 10, SC, 40, SD, 64, SE, 110, SF, 110, SG, 64, P_WDTH, 48, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 127`

### 19 FX-RING (machine 12): ring modulation: the track before turned to metal, a hollow bell, a bright twelfth

`19_FX-RING.wav`. Page: WAVE EXT - MIX - - - INP.

- **A** (A2 C3 E3 A3): PTCH 70 (+6 st), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 82. Measured -23.4 LUFS, peak -1.1 dBFS.
  `P_PTCH, 70, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 82`
- **B** (A2 C3 E3 A3): PTCH 71 (+7 st), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 84. Measured -22.8 LUFS, peak -1.1 dBFS.
  `P_PTCH, 71, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 84`
- **C** (A2 C3 E3 A3): WAVE 64, PTCH 83 (+19 st), AMP DEC 127 (none), AMP REL 100 (5.1 s to -60 dB), GATE 127 (7.9 step). Level: LEVL 127, VOL 109. Measured -22.0 LUFS, peak -1.1 dBFS.
  `SA, 64, P_PTCH, 83, P_DEC, 127, P_REL, 100, P_GATE, 127, P_LEVL, 127, P_VOL, 109`

## 3. Every take, measured

| # | Engine | Take | LUFS | Peak | Centroid | Flatness | dec30 s | Clarity | Harm | sus dB | Width dB | p20 st | cdrop oct |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 01 | GND-SIN | today | -22.5 | -13.8 | 158 | 0.000 | 0.86 | 1.00 | 0.99 | -8.7 | -44.2 | 0.1 | -0.00 |
| 01 | GND-SIN | A | -17.9 | -12.2 | 480 | 0.000 | hold | 1.00 | 1.00 | -0.9 | -44.2 | 0.0 | -0.00 |
| 01 | GND-SIN | B | -17.8 | -14.8 | 157 | 0.000 | 0.60 | 1.00 | 0.98 | -0.7 | -44.2 | 0.1 | -0.02 |
| 01 | GND-SIN | C | -17.8 | -9.4 | 927 | 0.000 | 0.54 | 1.00 | 1.00 | -10.8 | -44.2 | 0.0 | 0.00 |
| 02 | SWAV-SAW | today | -42.4 | -24.8 | 1366 | 0.002 | 0.85 | 1.00 | 0.95 | -9.6 | -43.9 | -0.1 | -0.00 |
| 02 | SWAV-SAW | A | -17.9 | -5.0 | 1542 | 0.002 | 1.54 | 0.92 | 0.71 | -5.7 | -16.3 | -11.9 | -0.78 |
| 02 | SWAV-SAW | B | -18.7 | -3.1 | 356 | 0.000 | 0.68 | 0.99 | 0.73 | -11.8 | -44.2 | 0.0 | -0.23 |
| 02 | SWAV-SAW | C | -17.8 | -4.6 | 1284 | 0.000 | 0.68 | 0.98 | 0.98 | -2.9 | -21.5 | 0.0 | -0.96 |
| 03 | SWAV-PUL | today | -38.1 | -26.7 | 648 | 0.000 | 0.86 | 1.00 | 0.98 | -9.2 | -44.1 | -0.1 | -0.00 |
| 03 | SWAV-PUL | A | -18.2 | -10.6 | 627 | 0.000 | 0.63 | 1.00 | 0.91 | -0.4 | -24.0 | -0.2 | 0.02 |
| 03 | SWAV-PUL | B | -17.8 | -4.5 | 1085 | 0.000 | 2.44 | 0.99 | 0.96 | -4.9 | -15.0 | 0.0 | -1.19 |
| 03 | SWAV-PUL | C | -18.4 | -7.4 | 188 | 0.000 | 0.63 | 1.00 | 0.58 | -16.0 | -44.2 | 0.0 | -0.03 |
| 04 | SWAV-ENS | today | -29.6 | -14.3 | 1054 | 0.005 | 0.82 | 0.69 | 0.33 | -9.8 | -44.1 | 0.3 | 0.16 |
| 04 | SWAV-ENS | A | -17.9 | -4.9 | 1640 | 0.028 | hold | 0.64 | 0.10 | -3.0 | -7.3 | -11.9 | 0.29 |
| 04 | SWAV-ENS | B | -19.0 | -1.1 | 1183 | 0.000 | 1.41 | 0.71 | 0.29 | -14.7 | -27.5 | 0.0 | 0.13 |
| 04 | SWAV-ENS | C | -17.8 | -3.4 | 874 | 0.001 | 2.49 | 0.92 | 0.62 | -1.3 | -13.2 | -0.0 | 0.47 |
| 05 | FM+STAT | today | -28.3 | -16.1 | 461 | 0.000 | 0.84 | 1.00 | 0.36 | -10.1 | -44.1 | 0.1 | -0.02 |
| 05 | FM+STAT | A | -17.8 | -9.6 | 329 | 0.000 | 0.79 | 0.99 | 0.98 | -6.4 | -26.1 | -0.1 | 0.25 |
| 05 | FM+STAT | B | -17.9 | -9.2 | 502 | 0.000 | 1.93 | 1.00 | 0.99 | -4.4 | -15.7 | -0.1 | 0.99 |
| 05 | FM+STAT | C | -17.8 | -3.0 | 488 | 0.000 | 0.22 | 1.00 | 0.96 | -32.7 | -24.6 | -0.1 | 0.74 |
| 06 | FM+PAR | today | -25.4 | -16.7 | 1437 | 0.000 | 0.85 | 1.00 | 0.36 | -8.6 | -44.2 | 0.0 | 0.04 |
| 06 | FM+PAR | A | -17.9 | -11.0 | 2056 | 0.001 | 1.66 | 1.00 | 0.21 | -5.4 | -19.3 | -0.1 | 0.05 |
| 06 | FM+PAR | B | -17.8 | -5.3 | 1521 | 0.000 | 0.35 | 0.99 | 0.98 | -22.0 | -26.1 | 19.0 | 1.49 |
| 06 | FM+PAR | C | -18.2 | -5.3 | 903 | 0.000 | 0.29 | 1.00 | 0.68 | -31.7 | -29.1 | -0.1 | 0.02 |
| 07 | FM+DYN | today | -28.1 | -14.9 | 338 | 0.000 | 0.79 | 1.00 | 0.22 | -10.0 | -44.1 | -0.4 | -0.16 |
| 07 | FM+DYN | A | -17.9 | -10.9 | 883 | 0.000 | 0.63 | 0.88 | 0.79 | -12.8 | -44.2 | -0.0 | 0.20 |
| 07 | FM+DYN | B | -17.9 | -5.9 | 287 | 0.000 | 0.78 | 1.00 | 0.99 | -6.6 | -44.2 | 0.1 | -0.01 |
| 07 | FM+DYN | C | -18.0 | -12.5 | 1323 | 0.000 | 1.25 | 1.00 | 0.92 | 1.5 | -44.2 | -5.4 | -0.05 |
| 08 | VOX-6 | today | -41.1 | -30.6 | 521 | 0.000 | 1.36 | 1.00 | 0.96 | 4.8 | -44.0 | 0.0 | -0.04 |
| 08 | VOX-6 | A | -17.9 | -9.7 | 852 | 0.000 | 1.77 | 0.91 | 0.95 | 1.7 | -6.5 | 1.5 | 0.07 |
| 08 | VOX-6 | B | -17.8 | -9.5 | 475 | 0.000 | 1.58 | 0.88 | 0.92 | 18.0 | -15.1 | 0.0 | 0.26 |
| 08 | VOX-6 | C | -17.8 | -7.3 | 578 | 0.000 | 0.76 | 0.82 | 0.80 | 2.4 | -16.9 | 2.8 | 0.17 |
| 09 | DIGI-WAV | today | -27.2 | -16.2 | 608 | 0.003 | 0.87 | 1.00 | 0.98 | -9.1 | -44.2 | -0.1 | -0.01 |
| 09 | DIGI-WAV | A | -17.9 | -1.3 | 552 | 0.000 | 0.58 | 1.00 | 0.99 | -13.4 | -20.5 | -0.1 | 1.26 |
| 09 | DIGI-WAV | B | -17.8 | -5.8 | 1165 | 0.006 | hold | 0.97 | 0.76 | -7.3 | -16.4 | 12.2 | 1.28 |
| 09 | DIGI-WAV | C | -17.8 | -9.4 | 646 | 0.000 | 0.48 | 1.00 | 1.00 | -1.4 | -25.1 | -0.1 | 0.02 |
| 10 | SID-6581 | today | -26.2 | -19.4 | 330 | 0.000 | 0.87 | 1.00 | 0.99 | -9.0 | -44.2 | -0.1 | 0.00 |
| 10 | SID-6581 | A | -17.8 | -10.2 | 1430 | 0.000 | 0.71 | 1.00 | 1.00 | -0.6 | -24.2 | 0.0 | 0.01 |
| 10 | SID-6581 | B | -17.8 | -7.3 | 413 | 0.000 | 0.74 | 1.00 | 0.99 | -8.8 | -44.2 | 0.0 | 0.00 |
| 10 | SID-6581 | C | -20.2 | -7.8 | 674 | 0.004 | 0.40 | 1.00 | 0.97 | -21.5 | -24.4 | -0.1 | 0.06 |
| 11 | DRM-KICK | today | -24.8 | -11.4 | 87 | 0.000 | 0.77 | 0.97 | 0.99 | -8.5 | -44.2 | 9.3 | 1.35 |
| 11 | DRM-KICK | A | -18.0 | -8.4 | 76 | 0.000 | 0.33 | 1.00 | 0.96 | -23.7 | -44.2 | -0.4 | 0.28 |
| 11 | DRM-KICK | B | -17.9 | -9.0 | 74 | 0.000 | 1.45 | 0.99 | 0.99 | -9.1 | -44.2 | -2.7 | 1.29 |
| 11 | DRM-KICK | C | -17.8 | -3.1 | 95 | 0.000 | 0.19 | 0.99 | 0.90 | -48.1 | -44.2 | -0.4 | 0.47 |
| 12 | DRM-SNAR | today | -25.2 | -14.4 | 2610 | 0.206 | 0.85 | 0.59 | 0.66 | -9.4 | -44.2 | 1.2 | 0.06 |
| 12 | DRM-SNAR | A | -20.0 | -1.0 | 3054 | 0.212 | 0.08 | 0.54 | 0.33 | -49.6 | -24.5 | -0.2 | -0.06 |
| 12 | DRM-SNAR | B | -21.6 | -1.1 | 5457 | 0.408 | 0.15 | 0.08 | 0.15 | -48.1 | -20.2 | 11.5 | 0.06 |
| 12 | DRM-SNAR | C | -19.0 | -1.7 | 1357 | 0.005 | 0.14 | 0.27 | 0.24 | -46.0 | -25.8 | 23.3 | -0.11 |
| 13 | DRM-HATS | today | -42.6 | -20.2 | 2534 | 0.176 | 0.04 | 0.26 | 0.17 | -170.7 | -44.1 | 30.4 | -0.11 |
| 13 | DRM-HATS | A | -20.5 | -1.0 | 6538 | 0.074 | 0.10 | 0.19 | 0.05 | -189.3 | -44.2 | 91.3 | -0.01 |
| 13 | DRM-HATS | B | -18.3 | -1.0 | 6354 | 0.078 | 0.32 | 0.16 | 0.05 | -24.5 | -44.2 | 1.5 | -0.05 |
| 13 | DRM-HATS | C | -24.3 | -5.7 | 10331 | 0.110 | 0.07 | 0.72 | 0.02 | -185.9 | -44.2 | 88.3 | -0.00 |
| 14 | DRM-CYMB | today | -24.2 | -11.6 | 3182 | 0.202 | 0.47 | 0.14 | 0.17 | -17.0 | -44.2 | 17.8 | 0.04 |
| 14 | DRM-CYMB | A | -17.9 | -3.3 | 6430 | 0.214 | 0.60 | 0.10 | 0.09 | -17.3 | -26.3 | 1.4 | -0.01 |
| 14 | DRM-CYMB | B | -17.8 | -4.1 | 5790 | 0.085 | 0.33 | 0.72 | 0.21 | -26.0 | -29.3 | -0.0 | 0.01 |
| 14 | DRM-CYMB | C | -21.2 | -2.7 | 6305 | 0.214 | 1.01 | 0.11 | 0.10 | 16.6 | -17.3 | -88.4 | -0.48 |
| 15 | GND-PERC | today | -21.2 | -12.1 | 7410 | 0.551 | 0.88 | 0.06 | 0.09 | -9.0 | -44.2 | -5.3 | -0.05 |
| 15 | GND-PERC | A | -19.1 | -1.1 | 2960 | 0.031 | 0.11 | 0.61 | 0.30 | -55.8 | -30.7 | -0.3 | 0.02 |
| 15 | GND-PERC | B | -17.8 | -2.0 | 7067 | 0.173 | 0.13 | 0.12 | 0.06 | -63.8 | -44.2 | -2.5 | -0.01 |
| 15 | GND-PERC | C | -24.4 | -1.1 | 1108 | 0.000 | 0.06 | 0.56 | 0.15 | -181.8 | -44.2 | 0.3 | 0.18 |
| 16 | GND-NOIS | today | -24.1 | -15.1 | 7428 | 0.549 | 0.88 | 0.06 | 0.09 | -9.0 | -44.2 | -5.3 | -0.05 |
| 16 | GND-NOIS | A | -17.8 | -9.2 | 1188 | 0.000 | 1.23 | 0.70 | 0.31 | -5.6 | -5.7 | 0.4 | 0.07 |
| 16 | GND-NOIS | B | -17.8 | -6.9 | 633 | 0.001 | hold | 0.25 | 0.07 | -0.8 | -0.1 | 1.1 | 0.07 |
| 16 | GND-NOIS | C | -18.2 | -1.1 | 1732 | 0.000 | 0.19 | 0.20 | 0.16 | -41.5 | -10.0 | 3.7 | 0.12 |
| 17 | FX-CHORS | today | -25.8 | -7.4 | 782 | 0.004 | 0.83 | 1.00 | 0.94 | -10.3 | -29.6 | 0.1 | 0.00 |
| 17 | FX-CHORS | A | -21.5 | -4.8 | 899 | 0.005 | 0.80 | 1.00 | 0.92 | -9.9 | 0.5 | 0.1 | 0.31 |
| 17 | FX-CHORS | B | -18.8 | -3.3 | 799 | 0.001 | 0.89 | 1.00 | 0.91 | -9.6 | -44.2 | -0.1 | 0.34 |
| 17 | FX-CHORS | C | -20.7 | -3.6 | 603 | 0.002 | 0.83 | 1.00 | 0.95 | -10.1 | -9.2 | 0.1 | 0.00 |
| 18 | FX-DYNMX | today | -25.3 | -4.7 | 783 | 0.005 | 0.83 | 1.00 | 0.94 | -9.9 | -44.2 | 0.1 | 0.00 |
| 18 | FX-DYNMX | A | -17.9 | -3.8 | 473 | 0.001 | 0.91 | 1.00 | 0.99 | -8.0 | -44.2 | -0.2 | -0.30 |
| 18 | FX-DYNMX | B | -20.0 | -1.1 | 416 | 0.001 | 1.15 | 1.00 | 0.98 | -8.6 | -44.2 | 0.0 | 0.04 |
| 18 | FX-DYNMX | C | -21.8 | -1.4 | 525 | 0.001 | 1.11 | 1.00 | 0.98 | -9.6 | -44.2 | 0.0 | 0.01 |
| 19 | FX-RING | today | -25.5 | -7.7 | 713 | 0.003 | 0.81 | 1.00 | 0.95 | -10.7 | -44.1 | 12.1 | -0.00 |
| 19 | FX-RING | A | -23.4 | -1.1 | 834 | 0.034 | 0.61 | 0.77 | 0.49 | -14.6 | -44.2 | 0.1 | -0.09 |
| 19 | FX-RING | B | -22.8 | -1.1 | 873 | 0.022 | 0.70 | 1.00 | 0.49 | -19.5 | -44.2 | -0.0 | 0.05 |
| 19 | FX-RING | C | -22.0 | -1.1 | 1038 | 0.003 | 0.65 | 1.00 | 0.92 | -10.7 | -44.2 | -0.1 | 0.02 |

The new A takes against each other (the same distance): the closest pairs

| Pair | Distance |
|---|---|
| SWAV-SAW ~ FM+PAR | 1.60 |
| SWAV-PUL ~ SID-6581 | 1.61 |
| SWAV-PUL ~ DIGI-WAV | 1.67 |
| FM+DYN ~ FX-DYNMX | 1.72 |
| VOX-6 ~ GND-NOIS | 1.98 |
| FM+DYN ~ DIGI-WAV | 2.10 |
| FX-DYNMX ~ FX-RING | 2.10 |
| DIGI-WAV ~ SID-6581 | 2.17 |
| FM+STAT ~ DIGI-WAV | 2.30 |
| SWAV-SAW ~ SID-6581 | 2.35 |

Today's closest pair was 0.00 (GND-NOIS ~ GND-PERC); the new A's closest is 1.60.

Within each engine, and today's init against A:

| Engine | A-B | A-C | B-C | today-A |
|---|---|---|---|---|
| GND-SIN | 4.35 | 4.44 | 4.24 | 4.20 |
| SWAV-SAW | 4.56 | 1.46 | 4.14 | 1.89 |
| SWAV-PUL | 2.57 | 4.00 | 4.75 | 1.60 |
| SWAV-ENS | 3.82 | 2.33 | 2.53 | 3.85 |
| FM+STAT | 2.43 | 3.56 | 4.45 | 2.58 |
| FM+PAR | 3.09 | 4.41 | 2.00 | 2.13 |
| FM+DYN | 1.53 | 2.29 | 2.51 | 1.63 |
| VOX-6 | 2.46 | 2.07 | 2.12 | 2.76 |
| DIGI-WAV | 3.81 | 1.55 | 4.32 | 1.83 |
| SID-6581 | 3.65 | 3.02 | 3.32 | 2.89 |
| DRM-KICK | 2.67 | 2.02 | 4.39 | 2.10 |
| DRM-SNAR | 2.62 | 2.62 | 3.65 | 4.67 |
| DRM-HATS | 2.26 | 2.04 | 3.39 | 4.81 |
| DRM-CYMB | 2.64 | 3.58 | 5.23 | 3.33 |
| GND-PERC | 2.77 | 3.83 | 5.06 | 6.22 |
| GND-NOIS | 3.69 | 4.98 | 7.07 | 5.92 |
| FX-CHORS | 3.45 | 1.14 | 2.69 | 3.06 |
| FX-DYNMX | 0.72 | 0.59 | 0.44 | 1.08 |
| FX-RING | 0.98 | 1.61 | 1.48 | 1.53 |

The FX takes against the plain SWAV-SAW alone (the same source, the FX track silent):

| Engine | today | A | B | C |
|---|---|---|---|---|
| FX-CHORS | 0.06 | 3.11 | 1.99 | 2.13 |
| FX-DYNMX | 0.07 | 1.08 | 1.07 | 0.76 |
| FX-RING | 0.23 | 1.51 | 1.35 | 0.76 |

## 4. The proposed selector order

    M_ORDER = {M_GSIN, M_SAW, M_PUL, M_ENS, M_FMS, M_FMP, M_FMD, M_VOX, M_WAV, M_SID,
               M_DKIK, M_DSNR, M_DHAT, M_DCYM, M_GPRC, M_GNOI, M_FXC, M_FXD, M_FXR};

1-10 tonal, 11-14 the drums, 15-16 noise and percussion, 17-19 FX. Machine numbers do not change (patterns store
them); only the display order does.

- GND-SIN stays first: it is the INIT pattern's track 1 and the Monomachine's first, and its A is the plainest
  musical sound (a sine bass).
- Then the SWAV family together (a supersaw pad, a reed lead, a chord pad): the three most "synth" sounds, each a
  different role, are the next things a beginner hears turning right.
- The FM+ family together, as the Monomachine keeps it (keys, metal, bass).
- VOX-6 and DIGI-WAV next (a voice, a wavetable pluck); SID-6581 last of the tonal: the chip voice, the one closest to
  the "toy" sounds the owner rejected.
- The drums in kit order (kick, snare, hats, cymbal), then GND-PERC and GND-NOIS, then the FX, which are silent
  without a track before them.
- Today the first seven steps from GND-SIN are six noise or percussion engines (GND-NOIS, GND-PERC, the four DRM-);
  here the first ten are tonal.
- M_ORDER wraps: one step left from GND-SIN is the last entry. Today that is FX-RING and it stays FX-RING (silent on
  track 1); if that matters, ALGO could stop at the ends instead of wrapping.

## 5. Notes for building it

- **The kick's note.** DRM-KICK only sounds like a kick around C2 (A/B/C are on C2): the keys at OCT 0 play F3-G5,
  where any kick is a high tom or a zap (today's at C4: centroid 329 Hz, clarity 0.67). Track 1's new trigs take C2,
  tracks 2-6 take C4. PTCH cannot move it down cleanly: the filter follows the note, not PTCH (track_block keys the
  cutoffs on t->note), so at PTCH -24 played from C4 the high-pass floor (BASE 0: its two-pole at ~0.31 x the note,
  its one-pole at ~0.44 x) sits over a C2 kick's fundamental. Options for the init path: leave it (play it at
  OCT -2 or from track 1's steps), or have picking DRM-KICK set the track's new-trig note to C2.
- **FX tracks and set_machine.** kit_default sets HOLD 127 for the FX machines; set_machine() copies MACH_DEF's page
  only, so an FX picked with ALGO on an untouched chain keeps HOLD 0 and its amp decays from each trig (DEC 64). The
  FX inits here carry their amp: HOLD 127 (kit_default's), DEC 127, REL 80, GATE 127, so one trig opens them for as
  long as the gate lasts and they let go slowly.
- **GATE acts only with HOLD 127 or DEC 127** (env_block): elsewhere a GATE in the kit does nothing (bank A's sine bass
  carries GATE 127 with HOLD 24). The lists above leave such GATEs out.
- **Levels.** 38 of the 57 new takes sit at -18 +- 0.5 LUFS; 19 stop short, at the -1 dBFS peak (short or peaky
  sounds: snares A -20.0 and B -21.6, hats A -20.5, GND-PERC A -19.1 and C -24.4, the dub stab -19.0, FX-DYNMX B/C
  -20.0/-21.8, FX-RING -22.0..-23.4) or at LEVL 127 VOL 127 (SWAV-SAW B -18.7, SID C -20.2, snare C -19.0, hats C
  -24.3, the swell -21.2, FX-CHORS -18.8..-21.5).
  DIST was tried as a gain stage and dropped: its ceiling falls faster than its gain rises (SWAV-SAW B at DIST 84
  gained 2.7 dB of a nominal 7.8, with a residual of -9.5 dB: clipping).
- **The FX source.** The plain SWAV-SAW is quiet (-42.3 LUFS at its init, -26.3 at LEVL/VOL 127) and peaky (crest
  ~18 dB: its note-following high-pass leaves a spike at each reset). Today the FX after it add +0.7 (RING), +2.8
  (CHORS) and +5.2 dB (DYNMX). FX-CHORS and FX-DYNMX hear it before its VOL, so a source at VOL 0 is heard only
  through them: the clearest way to hear them, but a setting of the source, not of the FX's init.
- **FX-DYNMX** passes each note's first peak about 15 dB over the peaks around it (measured on its output alone: one
  peak under 1 ms long, ~9 ms into every note, -14 dB against -28..-31 dB; its gain is ramped from the last block's,
  set on a quieter block). That peak, not the loudness, limited its takes. Its three takes are the least distinct here
  (0.44-0.72 apart, 0.76-1.08 from the dry saw): a dynamics effect on a decaying saw with the dry under it changes the
  envelope more than anything these measures weigh.
- **DIGI-WAV's small bank** crossfades between neighbours at most values: MACH_DEF's WAVE 40 shows HOLW but plays 76 %
  HOLW, 24 % CHRM (pos = WAVE x 31 / 127); the factory's WAVE(i) macro lands below the wave it names the same way.
- **The INIT pattern** (pattern_init_blank) puts GND-PERC on tracks 2 and 3: today that is two white-noise tracks.
- **Badge**: SWAV-PUL's badge is "SWAV", SWAV-SAW's and SWAV-ENS's "SWV" (MI in m2k_ui.c).
- **Not heard**: every judgement here is by measure. Five takes come from bank A's approved kits: GND-SIN A
  (CIRCUIT's sine bass), FM+STAT A (the FM keys, without CIRCUIT's slow LFO on 2VOL), SWAV-ENS A (TIDE's pad: an m7
  in place of its locked chords, LPQ 40 for 51, REL 90 for 114, no LFOs or filter envelope), DRM-KICK B (the bank's
  kick as it is), DRM-SNAR A (snare3 without its LFO on FOLD), DRM-HATS A (hats3 centred, its carrier at +63 for +55,
  BASE 46 for 50 and +16 dB at 7.7 kHz for loudness, no LFO on DEC). The rest are mine and need the owner's ear.

## Files

`inits.c` (the harness: `#include "host/m2k_host.c"`, jobs on stdin), `lib.py` (rendering, BS.1770 from
tools/sheet/loud.py, the measures), `designs.py` (the candidates), `sheet.py` (`level`: the levelling;
`audition`), `build.py` (these WAVs), `report.py` (this file), `today.py` and `sim.py` (step 1). Build the harness
from the repo root: `cc -O2 -I. -Ifirmware/src -Ifirmware/gen -o inits inits.c -lm`.

