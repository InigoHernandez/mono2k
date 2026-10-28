# The screen's grid

MONO2K's 240 x 240 1-bit screen: one grid on every page and window, whole-pixel type in MONO2K's own Orbitron,
crisp 1 px strokes, dithering only where it means something. Hortator's screens set the discipline (inverted bars
top and bottom, chunky type, dial cells, a big name, square steps); the letterforms, the wordmark, the marks, the
splash and the bars' layout (the first redesign's) are MONO2K's. Code: the constants at the top of `m2k_ui.c`'s
drawing section; `m2k_gfx.c` (rules, cells, tags, windows, handles); `tools/gen_font.py` (the fonts). The host's
text audit enforces the rules below. Every number here is a whole pixel.

## Type

Orbitron (SIL OFL 1.1, `assets/fonts/orbitron`), rendered 1-bit by `tools/gen_font.py`: each glyph cropped to its ink
and set a whole tracking apart, every capital and digit between the same two rows (no overshoot), a descender's rows
under them. A text's y is its capitals' top row; `base` is their height (the last row y + base - 1), `h` adds the
descenders.

| font | size, weight | capitals | tracking | for |
|---|---|---|---|---|
| **F_B** | 11 px, 700 | 8 rows | 1 | the header and the footer, paper on their bars (the first redesign's small type) |
| **F_S** | 12 px, 800 | 9 rows, strokes 2 px | 2 | every label, value, line, tag and window |
| **F_M** | 16 px, 800 | 12 rows | 2 | the big type a size down (where F_L does not fit) |
| **F_L** | 24 px, 800 | 18 rows | 3 | the stage's big name or value, the page's title |

`build/fonts.png` (`gen_font.py --sheet`) shows them. A cell holds ~5 F_S capitals in 53 px (a capital 8-10 px, a W
12, digits 8, 1 and I 3-4, each 2 apart): plan cells by characters and give long values short forms.

## Bands and columns

```
y 0-15    header  ink bar: [A01] CIRCUIT            (GRID) [HOLD] o 126 >
y 16-60   knobs   four cells of 60, solid rules at x 60 120 180; label / dotted rule / dial + value; solid rule 60
y 61-201  stage   value column [4, 101)  |  picture [108, 237) x [64, 198)
y 202-223 steps   dotted rule 202; 16 boxes 11 x 11 at rows 205-215; a beat mark under each group's first, 219-220
y 224-239 footer  ink bar: [bars] T2 > DRM-SNAR > AMP                 1/2
```

- **Header** (rows 0-15, ink; F_B paper, capitals rows 4-11, 4 clear above and below): the pattern's slot a paper tag
  at x 2 (ink type, rows 2-13), its name 6 px after it (whole or not at all); from the right, the transport (a 5 x 8
  triangle or an 8 x 8 square, x 229-236), the tempo 6 before it, REC (an 8 x 8 disc, blinking) 6 before that, then
  the modes as tags: GRID and PADS a paper frame round paper type, the arp's HOLD, READY and FILL paper boxes with ink
  type, 4 apart, dropped from the last when they would reach the name's room.
- **Knobs** (rows 16-60): cell k is `[60k, 60k + 60)`, its content `[60k + 4, 60k + 57)`. The label centred on
  `60k + 30`, rows 20-28 (inverted while turned: rows 19-29); a dotted rule on row 33; a ranged value right-aligned
  to `60k + 56`, rows 43-51, a dial left of it (13 x 13 about `(60k + 9, 47)`, 2 px clear of the cell's rule and 3 of
  the value: a ring open at its foot, -135 to +135 degrees, a needle from the centre easing to the value, the dial
  solid for 0.2 s as it turns); a value too wide for the dial (over 38 px), a choice or an action centred, no dial;
  a locked value inverted (rows 42-52). Solid rule on row 60.
- **Stage** (rows 61-201): the value's column `[4, 101)` and the picture `[108, 237)` x rows 64-197 (129 x 134); no
  rule between them. Lines: a tag or label on y 64 (a tag's box rows 63-73), the big name or value from y 80 (F_L,
  rows 80-97), then lines of the small type 8 under it, 13 to 15 apart; a picture's names on y 188.
- **Steps** (rows 202-223): a dotted rule on 202; 16 boxes 11 x 11 at rows 205-215, a beat's four in each knob cell at
  `60k + 4 + 14i`; a 3 x 2 mark under each beat's first (rows 219-220). HOME shows its four bars there instead.
- **Footer** (rows 224-239, ink; F_B paper, capitals rows 228-235): the pattern's four bars at x 4 (9 x 8 boxes 11
  apart: in view solid, used framed, past the end dotted, the playing one a 3 x 2 mark), the breadcrumb from x 52
  (track ▸ machine ▸ page, a 3 x 5 triangle between, 11 px; the machine's badge where long; GLO ▸ page on GLO's),
  the page among its section's right-aligned to x 236 (1/3).
- **HOME's lanes**: six of 16 rows from y 64, 22 apart, the track's number in a 14 x 16 box at x 0, 16 cells of 11 x
  16 from x 18 (13 apart, 56 a beat).

## Elements

- **Tag**: F_S on a solid box, 1 px of ink above and below its capitals and 2 each side (11 rows). Off, the same type
  plain in the same place. On a bar (`bar_tag`): F_B in a paper box, or paper type in a paper frame, 12 rows.
- **Dial**: as above; the knobs' only level indicator (a picture's amount, MOD's mix, keeps a 3-row bar).
- **Step box**: empty an outline; a trig solid; with locks a checker in its outline; on a condition its outline
  and an inset block; on some trig tracks a third filled for each (AMP FILTER LFO), NONE a dot; accent a notch, slide
  a dotted edge, ratchets dots along the foot; past the end a centre dot; the playing step framed 2 px out; a toggled
  one ringed 160 ms.
- **Meter**: a 1 px frame, the level solid inset 2.
- **Rule**: a band's solid (the knobs' cells and foot), a cell's or a reference dotted on the screen's checker,
  (x + y) even, so dotted lines meet in step.
- **Window**: paper, a 1 px frame, a 2 px solid shadow; x 4-232 from y 64, its shadow no lower than row 201; its
  title a tag 4 px in, rows of key tags 15 apart from y + 21 (a column's text after its widest key), lines 13 apart.
- **Picture**: curves 2 px (3 for the part being turned), axes 1 px solid, references dotted, areas hatched (the part
  being turned solid), handles 5 x 5 framed or 7 x 7 solid; the LFOs' waves filled with a checker to their middle.
- **Message**: over the header, its inverse (paper, F_S ink type, capitals rows 3-11; two lines 31 rows, the second
  from row 18) for a moment, a gradient tail into the bar.

## Dithering

Ordered (Bayer 8 x 8, `T_BAYER`, `T_BMASK`), only where it means something: the machines' 3D marks and shadows, the
splash, the glitches between screens (`cyber`, the band slip, a big value's slip as it changes), a message's tail,
the SET page's disc. Checkers (not dither) for locks and the LFOs' fills. Dark mode inverts the whole screen.

## The splash

2.1 s at power on, then a 0.4 s dithered wipe into HOME; any key, button or knob (not MASTER) skips it, the press and
its release swallowed. A slab of MONO2K's own shapes in dithered 3D on ink (the wordmark's letters extruded, GND-SIN's
dome, the wavefolds, the wave bank as a waterfall), each rolling in over the last, folding back into the wordmark;
its type F_S. `m2k_host DIR --splash`, then `tools/splash.py DIR` for a GIF and a montage.

## What the audit enforces

`M2K_TEXT_AUDIT` (`ta_check`, `host/m2k_host.c`), on every frame of the tour and on `--text-sweep`: no two texts
touching or a pixel apart; none cut by the screen or a clip; none ending within 2 px of its window's frame; none
leaving the content of the cell its ink begins in (`CELL`: the header, the knobs, the stage's two columns, the
footer, HOME's lanes, a message); none within 2 px of a rule (`rule_h`, `rule_v`; a fill over a rule hides it).
Fit with `text_fit` (a shorter form) or `fit_font` (a size down), never by guessing widths.
`M2K_SWEEP_DIR=DIR m2k_host --text-sweep` also writes every machine's every page to DIR, to look at.

## Costs

`host/m2k_draw.c` counts a frame's instructions as the FM-1 builds the core:
`cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen -o
build/host/m2k_draw host/m2k_draw.c && build/host/m2k_draw [--splash [-v]]`.
