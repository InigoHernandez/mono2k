# The screen's grid

MONO2K's 240 x 240 1-bit screen, after the sound library page the owner loved, Torso's S4, Hortator and the MNM-1's
menu, and (1.6, the owner's: "use this font I created and use as a reference the Elektron screens") Elektron's
parameter pages: every page one picture as its hero, each knob's value drawn as a picture of it under the four knobs,
very few lines. MONO2K's own stays: the Orbitron wordmark and the splash, the machines' marks and badges, the ink
header and footer bars. Code: the constants at the top of `m2k_ui.c`'s drawing section, `m2k_gfx.c` (rules, cells,
bars, handles, markers, rounded boxes, the 2 px stroke); `tools/gen_font.py` (the fonts). The host's text audit
enforces the rules below.

## The 4 px grid

Every position, size, gap and text baseline is a multiple of 4. A text sits on its baseline (the row under its
capitals): `tx_b` draws it there. The content keeps 4 px from the screen's sides, `[4, 236)`.

```
y 0-15    header   ink bar, its ends rounded: [A01] CIRCUIT            (GRID) [HOLD] o 126 >
y 16-59   knobs    four columns of 60: the value as a picture (rows 20-39), the label under it (baseline 56), 4 px in
y 60      rule     dotted, the only line between bands
y 64-199  stage    the page's picture across [4, 236) x [68, 180); its info line on baseline 196
y 204-223 steps    16 cells 8 x 8, 12 apart, a beat's four under each knob (x 60k + 8)
y 224-239 footer   ink bar: [bars] T2 > DRM-SNAR > AMP                 1/2
```

## Type

The owner's `mono2k-font.otf` (1.6): the FontStruction "Elektron Pixel Font" by savingaurora (FontStruct, CC BY-SA
3.0, adapted; `assets/fonts/mono2k-pixel`, its license and readme beside it; the bitmaps in `firmware/gen/m2k_fonts.h`
are under the same licence). One weight on a 1 px grid, its capitals 5 rows at its native 8 px, strokes 1 px, 3 to 5
wide (I 1, M W 5). `tools/gen_font.py` renders each glyph at that size and the firmware draws it a whole number of
times larger (every design pixel a 2 x 2 or 3 x 3 block, never a half pixel; the generator holds each glyph to the
font rendered at 16 and 24 px, pixel for pixel, and the full stop and the comma to the baseline). The glyphs are kept
at their native size (a byte a row) and widened as they are drawn (`FONT_Z2`, `FONT_Z3`). Two fonts, the ladder
`fit_font` steps down:

| font | scale | capitals | a character | for |
|---|---|---|---|---|
| **F_S** | x2 | 10 rows | ~10 px (I 4, M W 12) | every line: labels, values, names, hints, the bars' type, actions |
| **F_L** | x3 | 15 rows | ~15 px | a page's one big word (SYN's model, TIME's tempo, SET's project, MIDI's channels) |

No bold (the owner's: "don't use bold anymore"): what matters is said by place and by inversion (a solid tag), not by
weight. x1 (5-row capitals) is too small to read on the 1.54-inch screen and is not used. The question mark, which
rose a row over the capitals, is redrawn within them (its stem a row shorter). The comma's tail runs 4 rows under the
baseline, so a message on the header's line sits a row higher than the bar's type. Nothing is dithered in type.

## The knobs' pictures

Elektron's parameter pages, in MONO2K's own drawings (never Elektron's icons): each knob's value is a picture of it,
rows 20-39 of its column, its label under it on baseline 56, all in the type's 2 px stroke (`line2`, `rframe`, the
2 x 2 corners of `rfill`), on the 4 px grid. One set, chosen by the parameter's kind (`icon_of`):

| picture | for | how it reads |
|---|---|---|
| dial | an amount (K_NUM's, depths, speeds, decays, chances) | a 20 px ring, its pointer from -135 to +135 degrees, - and + at its ends; two-sided: a notch at its top |
| slider | a level (VOL, LEVL, MIX, SUB, VERB) | a rounded 10 x 20 bar filled from its foot, ticks each side |
| number | a value whose number is the point (BPM, TUNE, PTCH, LEN, ratios, Hz, divisions) | the value over a 40 px ruler: segments for 8 choices or fewer, else a dotted track, the part up to it solid |
| box | a named choice (TRCK, DEST, TRIG, MODE, SCRN, the slots, the actions) | its name in a rounded 52 x 20 box (where it fits in 44 px, else the number's) |
| wave | a wave (the LFOs', the drums', the SID's) | the wave between braces, a cycle |
| envelope | a segment's time (ATCK, HOLD, DEC, REL, the drops) | in a rounded box, the segment rising, holding or falling, its run the value |
| filter | BASE, WDTH, HPQ, LPQ | in a rounded box, the response: its cutoff, its width, a resonance's peak |
| pan | PAN | a dotted line, its middle marked, a solid block where it sits |

A knob a page leaves free is a dotted square (an unused slot). Turning a knob, its value takes its label's place in a
solid tag (the number and the box already say it, so theirs keeps the label); a lock makes its label solid. The
pictures ease to a new value (a third of the way each 33 ms), calm or not.

## The bands

- **Header** (ink, rows 0-15, its four corners rounded as Elektron's title bars): the pattern's slot a paper tag and
  its name, the modes as tags (GRID, PADS; HOLD, READY, FILL solid; MUTE blinking while a mute waits for the bar),
  REC, the tempo, the transport (to x 235). A message takes it a moment (its inverse, a gradient tail); one of two
  lines covers the knobs' pictures too (rows 0-41), their labels showing under it.
- **Knobs** (rows 16-59): column k is `[60k, 60k + 60)`, its content `[60k + 4, 60k + 56)`: the value's picture (rows
  20-39, above) and the label under it on baseline 56. A dotted rule on row 60.
- **Stage** (rows 64-199): the picture `[4, 236) x [68, 180)`, the info line on baseline 196. The info line says what
  turns (the knob's full name at the left, its value and unit at the right, LOCK solid or REC blinking before them),
  a held step (its number, its condition, ratchet, trig type, accent, slide solid, its note at the right), else the
  picture's own names (AMP's A H D R under their segments, FILT's decades, LFO's rates, GLO's times ...).
- **Steps** (rows 204-223): 16 cells 8 x 8 at `60k + 8 + 12i`, rows 208-215. HOME shows its four bars there.
- **Footer** (ink, rows 224-239, its corners rounded): the pattern's four bars (8 x 8 at x 4, 16, 28, 40: in view solid, used framed,
  past the end dotted, the playing one marked), the breadcrumb from x 56 (track > machine > page, the machine's badge
  where long; GLO > page), the page among its section's right-aligned to x 236.

## One language for every picture

- **Areas are bars**: two columns on, one off (`bar_col`), from a dotted floor or a dotted zero line (`bar_bi` for
  both sides). All solid at rest; while a knob turns, its part stays solid and the rest goes light (one column in
  three). The envelope, the filter's response, the LFOs, the destruction, the EQ, the echoes, the reverb's tail, the
  arp's notes, the Grids lanes.
- **References are dotted** on the screen's checker (`(x + y)` even), so they meet in step: floors, zero lines, full
  and half levels, cutoffs, cursors.
- **Editable points are handles**: 5 x 5 framed on paper, 7 x 7 solid while their knob turns.
- **Live things are markers**: 5 x 5 solid in a ring of paper (a voice's level, an LFO's value, Grids' place), with a
  dotted cursor where it runs.
- **Discrete things are cells**: solid on, framed off, dotted gone (steps, beats, tracks, white keys, machines).
- **Names are the type**, on the info line or beside their row; what is picked or pressed a solid tag, its corners
  rounded.

Dithering only where it means something: the machines' 3D marks and shadows, the splash, the SET page's disc, a
message's tail. Dark mode inverts the whole screen.

## The pages

- **HOME** (Hortator's tracks): six lanes on baselines 80 to 180, 20 apart: the number and the machine's model (solid
  for the track in view, or a moment as it trigs), the 16 steps of the page in view from x 68, 8 apart (a beat 40): a
  rest a short line, a trig a block 6 wide and 8 tall (an accent 10, one on a condition 4), locks a frame round a
  checker, muted tracks dotted, the playing step a bar under it; the level live at the right. Turning a knob: the info
  line.
- **SYN**: the class and style, the model in F_L, punch, metal, plastic five cells each; the mark at the right, a cell
  a machine under it; at the foot which machine and `[ALGO] MACHINE`. SND: the library as a menu.
- **AMP**: the envelope on the voice's curves; page 2 a picture under each knob (drive, level, pan, glide).
- **FILT**: the response, its cutoffs' handles, the width on the floor while WDTH turns, the decades; page 2 the
  curve where the envelope takes it and the envelope.
- **FX**: EQ and RATE each a row named at its left; DELAY's echoes; DSTR's sine; MOD's sweep and its mix.
- **LFO** (Hortator's): the three side by side, each its number (solid for the one the knobs turn) and destination,
  its wave in bars from its zero line, a cursor and a marker live, its rate at the foot.
- **ARP**: the notes as a staircase, the sounding one solid; GRIDS Hortator's map (a dot a node, the four read solid,
  the place a marker) and the three lanes; LANES the three, each its followers.
- **GLO**: TIME the tempo in F_L and the four beats under the knobs; BUS the reverb's tail; SET the project and the
  disc; MIDI the channels and clocks; IN the ports; PLAY the mutes.

## Lists

The menu's (the MNM-1's): a row 16 px apart, the slot in the light face, the name primary from 36 px in, the machine
at the right; the one picked a solid bar across the row (rows b - 12 .. b + 1); no boxes, no rules; a scroll at the
right edge (the rows shown solid on a dotted track).

## Panels

Only where a layer covers the page: GLO held and latched, HOME held, SEQ held, PERFORM, the button cards, the
catalogue, a question, the clear's ring (and LFO held, compact, the page moving on under it). Elektron's title bar:
solid, its corners rounded, the title in paper from the left (its capitals rows 3-12 of the bar's 16), over a 1 px
frame round the rest, the stage cleared round it; rows 16 apart from baseline 96 (`pairs`: a key an outlined tag, its
action beside it, aligned); the keys' hints on the foot's baseline 196 (`hints`: `[KEY] ACTION`, as many as fit). A
layer's panel is x 4-235, rows 64-203. No shadow.

## What the audit enforces

`M2K_TEXT_AUDIT` (`ta_check`, `host/m2k_host.c`), on every frame of the tour and on `--text-sweep`: no two texts
touching or a pixel apart; none cut by the screen or a clip; none ending within 2 px of a panel's frame; none leaving
the content of the cell its ink begins in (`CELL`: the header, the knobs' columns, the stage, the footer); none within
2 px of a rule (`rule_h`, `rule_v`, a card's or a panel's edges; a fill over a rule hides it). Fit with `text_fit` /
`lite_fit` (a shorter form) or `fit_font` (a size down), never by guessing widths. `M2K_SWEEP_DIR=DIR m2k_host
--text-sweep` also writes every machine's every page to DIR, to look at.

## The splash

2.1 s at power on, then a 0.4 s dithered wipe into HOME; any key, button or knob (not MASTER) skips it, the press and
its release swallowed. One surface in dithered 3D on ink, its height morphing from one of MONO2K's shapes into the
next (the wordmark's letters, the wave bank, GND-SIN's dome, the wavefolds, the letters' relief), the camera swinging
once and rising as it resolves into the crisp wordmark, then the type. `m2k_host DIR --splash`, `tools/splash.py DIR`;
`m2k_draw --splash -a` every frame's cost.

## Costs

`host/m2k_draw.c` counts a frame's instructions as the FM-1 builds the core:
`cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen -o
build/host/m2k_draw host/m2k_draw.c && build/host/m2k_draw [--splash [-v]]`. The drawing's primitives work a byte at a
time (a one-byte-wide rect a byte a row, dotted lines a byte at a time, a glyph's native row widened once and laid on
its 2 or 3 rows, empty rows skipped): the 2026-10 restyle's frames cost 5-40 % less than the boxed ones before it, and
Silkscreen's 4-38 % less again than Orbitron's. 1.6's pictures cost 2-16 % less than 1.5's frames: the small type
writes its two rows together, a glyph cut by the screen's foot (the footer's descenders) keeps the fast path, and the
pictures are sprites (`spr24`) and 2 x 2 blocks a byte each (`blk2`).
