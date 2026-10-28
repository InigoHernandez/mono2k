# The screen's grid

MONO2K's 240 x 240 1-bit screen: one grid on every page and window, whole-pixel type in its own pixel font,
crisp 1 px strokes, dithering only where it means something. Hortator's screens set the discipline (inverted bars
top and bottom, dial cells, a big name, square steps); the letterforms, the wordmark, the marks and the splash are
MONO2K's. Code: the constants at the top of `m2k_ui.c`'s drawing section; `m2k_gfx.c` (rules, cells, tags,
windows, handles); `tools/gen_font.py` (the font). The host's text audit enforces the rules below.

## Type

One pixel font, 5 x 7 capitals drawn a pixel at a time in `tools/gen_font.py` (squared, the round letters chamfered
a pixel like the wordmark's rounded blocks, a slashed zero, a flat-middled 2), generated at whole scales: **F_S** 1x
(labels, values, lines: 7 px, 1 px strokes, 1 px tracking), **F_M** 2x (the stage's names: 14 px), **F_L** 3x (the
big values and page titles: 21 px). A text's y is its capitals' top row; its last row is y + h - 1.
`build/fonts.png` (`gen_font.py --sheet`) shows them.

## Bands and columns

```
y 0-10    header  ink bar: > 120 [bars]          T2          GRID (o) A01
y 11-37   knobs   four cells of 60, solid rules at x 60 120 180; label / dotted rule / dial + value; solid rule 37
y 38-205  stage   value column [4, 101)  |  picture [108, 237) x [42, 203)
y 206-228 steps   dotted rule 206; 16 boxes 11 x 11 at rows 210-220; a beat mark under each group's first
y 229-239 footer  ink bar: T2 > DRM-SNAR > AMP                ENV 1/2
```

- **Header** (rows 0-10, ink): the transport (a 4 x 7 triangle or a 7 x 7 square), the tempo, the pattern's four
  bars (5 x 5 boxes 7 apart: in view solid, used framed, past the end dotted, the playing one a dot); the track in
  the middle; the pattern at the right, REC before it, the modes (GRID PADS READY FILL) before that, dropped from
  the last when they would reach the track. All type paper, y 2.
- **Knobs** (rows 11-37): cell k is `[60k, 60k + 60)`, its content `[60k + 4, 60k + 57)`. The label centred, y 13
  (inverted while turned: rows 12-20); a dotted rule on row 22; a ranged value right-aligned to `60k + 56`, y 26,
  a dial left of it (11 x 11 about `(60k + 10, 29)`: a ring open at its foot, -135 to +135 degrees, a needle from
  the centre easing to the value, the dial solid for 0.2 s as it turns); a choice or an action centred, no dial; a
  locked value inverted (rows 25-33). Solid rule on row 37.
- **Stage** (rows 38-205): the value's column `[4, 101)` and the picture `[108, 237)` x rows 42-202 (129 x 161);
  no rule between them. Lines: a tag or label on y 42 (a tag's box rows 41-49), the big name or value from y 56,
  then lines of the small type 8 under it, 10 or 12 apart; a picture's names on y 194.
- **Steps** (rows 206-228): a dotted rule on 206; 16 boxes 11 x 11 at rows 210-220, a beat's four in each knob cell
  at `60k + 4 + 14i`; a 3 x 2 mark under each beat's first (rows 224-225). HOME shows its four bars there instead
  (48 x 11, a beat 12 px).
- **Footer** (rows 229-239, ink): the breadcrumb at x 4 (track ▸ machine ▸ page, a 3 x 5 triangle between; the
  machine's badge where long; GLO ▸ page on GLO's), and at the right the section's button and the page among its
  pages (ENV 1/2; HOME: the pattern's name). Type paper, y 231.
- **HOME's lanes**: six of 16 rows from y 44, 26 apart, the track's number in a 14 x 16 box at x 0, 16 cells of 11 x
  16 from x 18 (13 apart, 56 a beat).

## Elements

- **Tag**: the small type on a solid box with 1 px of ink above and below it and 2 each side (9 rows). Off, the same
  type plain in the same place. Windows use solid tags for their keys.
- **Dial**: as above; the knobs' only level indicator (a picture's amount, MOD's mix, keeps a 3-row bar).
- **Step box**: empty an outline; a trig solid; with locks a checker in its outline; on a condition its outline
  and an inset block; on some trig tracks a third filled for each (AMP FILTER LFO), NONE a dot; accent a notch, slide
  a dotted edge, ratchets dots along the foot; past the end a centre dot; the playing step framed 2 px out; a toggled
  one inverted 160 ms.
- **Meter**: a 1 px frame, the level solid inset 2.
- **Rule**: a band's solid (the knobs' cells and foot), a cell's or a reference dotted on the screen's checker,
  (x + y) even, so dotted lines meet in step.
- **Window**: paper, a 1 px frame, a 2 px solid shadow; x 4-232 from y 41; its title a tag 4 px in, rows of key
  tags 12 apart, lines 10 apart.
- **Picture**: curves 2 px (3 for the part being turned), axes 1 px solid, references dotted, areas hatched (the part
  being turned solid), handles 5 x 5 framed or 7 x 7 solid; the LFOs' waves filled with a checker to their middle.
- **Message**: the header's inverse (paper, ink type) for a moment, a gradient tail into the bar.

## Dithering

Ordered (Bayer 8 x 8, `T_BAYER`, `T_BMASK`), only where it means something: the machines' 3D marks and shadows, the
splash, the glitches between screens (`cyber`, the band slip, a big value's slip as it changes), a message's tail,
the SET page's disc. Checkers (not dither) for locks and the LFOs' fills. Dark mode inverts the whole screen.

## The splash

2.1 s at power on, then a 0.4 s dithered wipe into HOME; any key, button or knob (not MASTER) skips it, the press and
its release swallowed. A slab of MONO2K's own shapes in dithered 3D on ink (the wordmark's letters extruded, GND-SIN's
dome, the wavefolds, the wave bank as a waterfall), each rolling in over the last, folding back into the wordmark;
the type in the pixel font. `m2k_host DIR --splash`, then `tools/splash.py DIR` for a GIF and a montage.

## What the audit enforces

`M2K_TEXT_AUDIT` (`ta_check`, `host/m2k_host.c`), on every frame of the tour and on `--text-sweep`: no two texts
touching or a pixel apart; none cut by the screen or a clip; none ending within 2 px of its window's frame; none
leaving the content of the cell its ink begins in (`CELL`: the header, the knobs, the stage's two columns, the
footer, HOME's lanes, a message); none within 2 px of a rule (`rule_h`, `rule_v`; a fill over a rule hides it).
Fit with `text_fit` (tracked, tight, a shorter form) or `fit_font` (a size down), never by guessing widths.

## Costs

`host/m2k_draw.c` counts a frame's instructions as the FM-1 builds the core:
`cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen -o
build/host/m2k_draw host/m2k_draw.c && build/host/m2k_draw [--splash [-v]]`.
