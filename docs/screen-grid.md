# The screen's grid

MONO2K's 240 x 240 1-bit screen, its design system: one grid on every page and window, crisp 1 px strokes, type
in Orbitron at four sizes, dithering only where it means something. The code is `firmware/src/m2k_ui.c` (the
constants at the top of its drawing section) and `m2k_gfx.c` (rules, cells, windows, handles). The host's text
audit enforces the rules below on the tour and on `m2k_host --text-sweep`.

## Columns and bands

```
x    0   4                 60  64              120 124             180 184              236 240
     |   |      knob 1      |   |    knob 2     |   |    knob 3     |   |    knob 4      |  |
y 0  [A01][CIRCUIT]                              [GRID][READY]  (o) 130  >     header      0-14, rule 15
  16 [LEVL       :PTCH       :BPM        :SWNG       ]                          knobs  16-56, rule 57
  58 [ value's column  : picture                     ]                          stage  58-186, rule 187
     | 4 .. 100     104 108 .. 236                   |
 188 [##][  ][  ][  ]  [  ]...    16 steps, a beat to a cell                    steps 188-211, rule 212
 213 [#][ ][ ][ ]  T2 > DRM-SNAR > AMP           1/2                            footer 213-239
```

- **Bands**: header 0-15, knobs 16-57, stage 58-187, steps 188-212, footer 213-239. Each band's last row is its
  rule, solid and full width (rows 15, 57, 187, 212); the footer has none.
- **Knob cells**: four of 60 px, `[60k, 60k + 60)`, under the FM-1's four knobs. Between them a dotted rule on
  x = 60, 120, 180, rows 18-54. A cell's content: `[60k + 4, 60k + 57)`, 53 px.
- **The stage**: a dotted rule on x = 104 (rows 60-184). The value's column `[0, 104)`, content `[4, 101)`, 97 px
  (`VX`, `VW`). The picture `[104, 240)`, content `[108, 237)` x rows 62-183: 129 x 122 (`PX0`, `PIC_W`, `PY0`,
  `PIC_H`).
- **Insets**: a cell's content begins 4 px past its left edge (3 clear px after a rule, 4 from the screen's left)
  and ends 3 clear px before its right one (or the screen's edge). Every text keeps to it.
- **Steps**: 16 square boxes of 11 x 11, rows 194-204, four to a knob cell at `60k + 4 + 14i` (3 px apart, 7 px
  between beats, the knob rules' line in the middle). HOME's lanes are their own grid: six of 16 rows from y 62,
  20 apart, the track's number in a 14 x 16 box at x 0, 16 cells of 11 x 16 from x 18 (13 apart, 56 a beat).
- **Footer**: the bars at x 4 (four boxes of 9 x 8, 11 apart), the breadcrumb from x 52, the page count right on
  236, all on the text line at y 221.

## Baselines

Type sits on fixed lines. A text's y is its top; its capitals are the rows below it (F_S 1-8, F_M 1-11, F_L
1-18, F_V 2-27), its baseline the capitals' last row (`f->base - 1`).

| Where | Line (top) | Capitals |
| --- | --- | --- |
| Header, a tag's text | 2 | 3-10 (tags: rows 0-13) |
| Knob label | 19 | 20-27 (the turned one's bar: rows 17-30) |
| Knob value | baseline 43 | F_M 33-43; F_S 36-43 (a locked one's box: rows 31-45) |
| Knob level | rows 49-51 | the track on 51 |
| Stage: label (`L_TOP`) | 61 | 62-69 |
| Stage: the big value | baseline 118 (`BIG_BASE`) | F_V 93-118, F_L 101-118, F_M 108-118 |
| Stage: lines (`L_B3` .. `L_B0`) | 132, 146, 160, 174 | 133-140 ... 175-182; a tag on one: rows y-2 .. y+11 |
| Footer | 221 | 222-229 |

The value's column on every page: its label on `L_TOP` (the knob's name, PAGE, STEP 21, the machine's style),
the big value on `BIG_BASE`, then the lower lines (the unit and LOCK or REC on `L_B3`, a held step's tags from
`L_B2`, the machine card's class on `L_B3` and its stats from `L_B2`), a focused knob's level along the foot
(rows 179-181). The picture's names (A H D R, HP LP, a mode) sit on `L_B0`, the value column's last line.

## Elements

- **Label**: F_S, tracked +1 where it fits (tight where not), ink.
- **Value**: F_M in a knob cell (F_S when wider than 53); F_V on the stage, else F_L, else F_M (`value_font`:
  one size for all of a parameter's values, so turning it never changes the size).
- **Bar** (a level): 3 rows; a dotted track on its last row, the fill solid from the left (from the middle for a
  two-sided value, a 5-row tick on the middle); a few choices (2..8) as segments, 2 px apart, the chosen solid,
  the others a 1 px line on the last row.
- **Meter** (a live level: HOME's tracks): a 1 px frame, the fill solid, inset 2 (1 clear px).
- **Tag**: 14 rows, its text 3 px in (2 clear px each side of the ink). Inverted (solid, paper text): on, active,
  what is being turned. Framed (1 px): available, off.
- **Rule**: a band's solid, a cell's dotted. Dotted is always on the screen's checker, (x + y) even, so every
  dotted line, outline and rule meets the others in step (`dots_h`, `dots_v`, `dots_frame`).
- **Frame**: 1 px. A window over the stage: paper, a 1 px frame, a 2 px solid shadow down and right (`win`); its
  title a tag 4 px in, its type 4 px in, rows of tags 16 or 17 apart, lines of type 11 apart. The held windows
  are x 4-232 (shadow to 234), from y 61.
- **Picture**: curves two dots thick (three for the part being turned); axes and baselines solid, 1 px;
  references (0 dB, half level, a cutoff, a middle line) dotted on the checker; areas hatched (PT_HATCH, a hairline
  every fourth column), the part being turned solid (or PT_VLINES); handles 5 x 5 framed on paper, 7 x 7 solid
  while their knob turns. Absent or unused things are dotted (an empty step's outline, a bar past the pattern's
  end, an INIT slot); a step past the end is a single dot at its centre.

## Dithering

Ordered (Bayer 8 x 8, `T_BAYER`; a level's eight rows as bytes, `T_BMASK`) and only where it means something:

- the machines' 3D marks and their shadows (generated, `tools/gen_icons.py`);
- the splash (its lit 3D slab, the wordmark's fill, the wipe into HOME);
- the glitches kept between screens: the machine change's and a page's dissolve (`cyber`), the two-beat band slip
  (`glitch`), a big value's slip as it changes (`row_slip`), the message's gradient tail, the GRID toast's;
- the SET page's display disc; dark mode is the whole screen inverted (`present`), nothing dithered for it.

Never for a bar, a meter, a tag, a rule or text.

## The splash

2.1 s at power on (`SPLASH_MS`), then a dithered wipe into HOME (`WIPE_MS`, 0.4 s); any key, button or knob (not
MASTER) skips it, and that press and its release are the splash's alone (`skip_keys`, `skip_btns`). On ink: a slab
of MONO2K's waves drawn front to back with a floating horizon (each pixel once), 2 px columns, each strip lit by its
slope through a tanh knee (`T_SAT`), the light through `T_BMASK` a byte at a time: GND-SIN's dome, the wavefolds,
the wave bank as a waterfall (its strips dark under lit lines), each rolling in from the back over the last; then
the camera rises, the turn stops, the slab narrows to the logotype's band and the letters rise out of the waves;
the relief resolves into the wordmark (white on ink, its fill dithered), the type comes in, a glitch once. See
every frame: `build/host/m2k_host DIR --splash && .venv/bin/python tools/splash.py DIR` (a GIF and a montage).

## What the audit enforces

The host's text audit (`M2K_TEXT_AUDIT`, `ta_check` in `host/m2k_host.c`) runs on every frame of the tour and on
`--text-sweep` (every page, every machine, every knob at its least, middle and most, a held step with every tag,
the library's categories, each latched layer, each help card), and fails on:

- two texts whose ink touches or comes within a pixel;
- a text cut by the screen's edge or a clip;
- a text begun in a window (a paper fill) that ends within 2 px of its frame;
- a text whose ink begins anywhere in a **cell** (a knob's, the stage's value column and picture, the header, the
  footer, HOME's lanes, a message's band: registered as drawn, `CELL`) and leaves the cell's content box;
- a text whose ink comes within 2 px of a **rule** (the bands' and the cells', registered as drawn by `rule_h` and
  `rule_v`; a fill over a rule hides it there).

A fill of paper (a window) or a broad one of ink hides the texts and the rules under it. The splash, a transition
and a message sweeping in are not audited. Fit text with `text_fit` (the small type: tracked, tight, a shorter
form) or `fit_font` (a size down), never by guessing widths; a self-test case holds the audit itself.

## Costs

`host/m2k_draw.c` counts the instructions of a frame (drawn and presented), built as the FM-1 builds the core:

```
cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen \
   -o build/host/m2k_draw host/m2k_draw.c && build/host/m2k_draw && build/host/m2k_draw --splash -v
```
