# The screen's grid

MONO2K's 240 x 240 1-bit screen, after the sound library page the owner loved, Torso's S4, Hortator and the MNM-1's
menu: every page one picture as its hero, its parameters quiet and aligned under the four knobs, very few lines and no
boxes for their own sake. MONO2K's own stays: the Orbitron wordmark and the splash, the machines' marks and badges,
the ink header and footer bars, the bar under each knob's value. Code: the constants at the top of `m2k_ui.c`'s
drawing section, `m2k_gfx.c` (rules, cells, bars, handles, markers); `tools/gen_font.py` (the fonts). The host's text
audit enforces the rules below.

## The 4 px grid

Every position, size, gap and text baseline is a multiple of 4. A text sits on its baseline (the row under its
capitals): `tx_b` draws it there. The content keeps 4 px from the screen's sides, `[4, 236)`.

```
y 0-15    header   ink bar: [A01] CIRCUIT            (GRID) [HOLD] o 126 >
y 16-59   knobs    four columns of 60, frameless: label (baseline 32), value (48), a thin bar (52-53), 4 px in
y 60      rule     dotted, the only line between bands
y 64-199  stage    the page's picture across [4, 236) x [68, 180); its info line on baseline 196
y 204-223 steps    16 cells 8 x 8, 12 apart, a beat's four under each knob (x 60k + 8)
y 224-239 footer   ink bar: [bars] T2 > DRM-SNAR > AMP                 1/2
```

## Type

Silkscreen (Jason Kottke, The Silkscreen Project Authors; SIL OFL 1.1), the owner's pick for its resolution on the
FM-1: a pixel font on a 1 px grid, its capitals 5 rows at its native 8 px. `tools/gen_font.py` renders each glyph at
that size and the firmware draws it a whole number of times larger (2 or 3: every design pixel a 2 x 2 or 3 x 3
block, never a half pixel; the generator holds each glyph to the font rendered at that size, pixel for pixel). The
glyphs are kept at their native size (a byte a row) and widened as they are drawn (`FONT_Z2`, `FONT_Z3`). Four
fonts, a ladder by width that `fit_font` steps down:

| font | face, scale | capitals | a character | for |
|---|---|---|---|---|
| **F_B** | Regular x2 | 10 rows | ~10 px | the light face: labels, units, hints, names in pictures, the bars' type; a value's fallback |
| **F_S** | Bold x2 | 10 rows | ~12 px | the primary: list names, the knobs' values, values in the info line, actions |
| **F_M** | Regular x3 | 15 rows | ~15 px | a size up (the MIDI page's channels) |
| **F_L** | Bold x3 | 15 rows | ~18 px | a page's one big word (SYN's model, TIME's tempo, SET's project) |

The weight says what is primary (bold) and what is about it (regular), both one height, so a knob's value in either
never moves the row. x1 (5-row capitals) is too small to read on the 1.54-inch screen and is not used. The full stop
sits on the baseline (its outline floats half a design pixel, which at 8 px rounded up into a dash). Nothing is
dithered in type.

## The bands

- **Header** (ink, rows 0-15): the pattern's slot a paper tag and its name, the modes as tags (GRID, PADS; HOLD,
  READY, FILL solid; MUTE blinking while a mute waits for the bar), REC, the tempo, the transport (to x 235). A
  message takes it a moment (its inverse, a gradient tail).
- **Knobs** (rows 16-59): column k is `[60k, 60k + 60)`, its content `[60k + 4, 60k + 56)`. The label in the light
  face, solid (a box 2 px round it) while its knob turns; the value in F_S where the knob's every value fits 52 px,
  else in F_B (one height either way), solid while locked; the bar 2 rows, a dotted track and the part up to the value
  solid (from the middle for a two-sided value, a tick there; a segment a choice for 8 or fewer), its end standing up a
  moment as it turns. A free knob shows its track alone. A dotted rule on row 60.
- **Stage** (rows 64-199): the picture `[4, 236) x [68, 180)`, the info line on baseline 196. The info line says what
  turns (the knob's full name at the left, its value and unit at the right, LOCK solid or REC blinking before them),
  a held step (its number, its condition, ratchet, trig type, accent, slide solid, its note at the right), else the
  picture's own names (AMP's A H D R under their segments, FILT's decades, LFO's rates, GLO's times ...).
- **Steps** (rows 204-223): 16 cells 8 x 8 at `60k + 8 + 12i`, rows 208-215. HOME shows its four bars there.
- **Footer** (ink, rows 224-239): the pattern's four bars (8 x 8 at x 4, 16, 28, 40: in view solid, used framed,
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
- **Names are the light face**, on the info line or beside their row.

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
catalogue, a question, the clear's ring (and LFO held, compact, the page moving on under it). One frame, the stage
cleared round it; the title in the light face solid on its top line, centred; rows 16 apart from baseline 96
(`pairs`: a key an outlined tag, its action primary, aligned); the keys' hints on the foot's baseline 196 (`hints`:
`[KEY] ACTION`, as many as fit). A layer's panel is x 4-235, rows 72-203. No shadow. A table's actions in F_S
where they all fit, else F_B.

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
Silkscreen's 4-38 % less again than Orbitron's.
