# The screen's grid

MONO2K's 240 x 240 1-bit screen: one grid on every page and window, whole-pixel type in MONO2K's own Orbitron,
crisp 1 px strokes, dithering only where it means something. Hortator's screens set the discipline (inverted bars
top and bottom, chunky type, a big name, square steps); the letterforms, the wordmark, the marks, the
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
| **F_B** | 11 px, 700 | 8 rows | 2 (the name 3 where it fits) | the header and the footer, paper on their bars (the first redesign's small type) |
| **F_S** | 12 px, 800 | 9 rows, strokes 2 px | 2 | every label, value, line, tag and window |
| **F_M** | 16 px, 800 | 12 rows | 2 | the big type a size down (where F_L does not fit) |
| **F_L** | 24 px, 800 | 18 rows | 3 | the stage's big name or value, the page's title |

`build/fonts.png` (`gen_font.py --sheet`) shows them. A knob's 51 px hold ~5 F_S capitals (a capital 8-10 px, a W
12, digits 8, 1 and I 3-4, each 2 apart) or ~4 of F_M (11-13 px, digits 11): plan cells by characters and give long
values short forms.

## Bands and columns

```
y 0-15    header  ink bar: [A01] CIRCUIT            (GRID) [HOLD] o 126 >
y 16-60   knobs   four cells of 60, solid rules at x 60 120 180; label / value / bar, 5 px in; solid rule 60
y 61-201  stage   value column [4, 101)  |  picture [108, 237) x [64, 198)
y 202-223 steps   dotted rule 202; 16 boxes 11 x 11 at rows 205-215; a beat mark under each group's first, 219-220
y 224-239 footer  ink bar: [bars] T2 > DRM-SNAR > AMP                 1/2
```

- **Header** (rows 0-15, ink; F_B paper, capitals rows 4-11, 4 clear above and below): the pattern's slot a paper tag
  at x 2 (ink type, rows 2-13), its name 6 px after it (whole or not at all); from the right, the transport (a 5 x 8
  triangle or an 8 x 8 square, x 229-236), the tempo 6 before it, REC (an 8 x 8 disc, blinking) 6 before that, then
  the modes as tags: GRID and PADS a paper frame round paper type, the arp's HOLD, READY and FILL paper boxes with ink
  type, MUTE (a mute waiting for the bar) turning between the two every 128 ms in its place, 4 apart, dropped from the
  last when they would reach the name's room.
- **Knobs** (rows 16-60): cell k is `[60k, 60k + 60)`, its content `[60k + 5, 60k + 56)`: 4 blank columns each side
  of it before the rules. All left-aligned on `60k + 5`: the label, rows 21-29 (inverted while turned: rows 20-30,
  x `60k + 3` to `60k + 57`); the value, its last row 47 (F_M, rows 36-47, where every value of all four knobs fits
  the 51 px, else F_S, rows 39-47, the whole row alike so turning never changes the size; a locked value inverted, a
  pixel round it); the bar, rows 52-54, 51 px: a dotted track on its last row and the part up to the value solid,
  easing to it, from the middle for a two-sided value (a tick there, rows 50-54); for a few choices (8 or fewer) a
  segment each, the chosen solid, the others a line; nothing for an action. Turning, the solid part's end (or the
  chosen segment) stands up 3 rows for 0.2 s. Solid rule on row 60.
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
- **Bar**: the knobs' level, as above; a picture's amount (MOD's mix) the same 3 rows.
- **Step box**: empty an outline; a trig solid; with locks a checker in its outline; on a condition its outline
  and an inset block; on some trig tracks a third filled for each (AMP FILTER LFO), NONE a dot; accent a notch, slide
  a dotted edge, ratchets dots along the foot; past the end a centre dot; the playing step framed 2 px out; a toggled
  one ringed 160 ms.
- **Meter**: a 1 px frame, the level solid inset 2.
- **Rule**: a band's solid (the knobs' cells and foot), a cell's or a reference dotted on the screen's checker,
  (x + y) even, so dotted lines meet in step.
- **Window**: see Windows below.
- **Picture**: curves 2 px (3 for the part being turned), axes 1 px solid, references dotted, areas hatched (the part
  being turned solid), handles 5 x 5 framed or 7 x 7 solid; the LFOs' waves filled with a checker to their middle.
- **Message**: over the header, its inverse (paper, F_S ink type, capitals rows 3-11; two lines 31 rows, the second
  from row 18) for a moment, a gradient tail into the bar.

## Windows

Every window over the stage is one system (`window()`, `pairs()`, `white_keys()` in m2k_ui.c): the held layers (GLO,
GLO latched, HOME, HOME latched, EDIT, SEQ, SEQ latched, LFO), the button cards (SEL ENV ARP PLAY OCT- OCT+), the
question (`ask`), the clear's ring and HOME's knob popup. PERFORM still draws its own and should take this one.

- **Box**: x 3-235 (w 233), from y 64 (the question 85, the ring and the popup 75, narrower), h whole rows as below,
  its shadow no lower than row 201. Paper, a 1 px ink frame, a 2 px ink shadow right and below (`win`).
- **Title bar**: inside the frame, rows y + 1 .. y + 14, ink across the whole inner width. Its title in F_B, paper, x
  + 5, capitals y + 4 .. y + 11 (cut to fit, never past the mark); a mark right-aligned to x + w - 5 in F_B paper
  (LATCHED; a fact of the layer: GLO's SELECT: BPM, HOME's BLACK 1-4: BANKS; the popup's knob). No tag in the bar.
- **Content**: x + 5 .. x + w - 6 (4 blank columns each side of it), its first row's top y + 20 (5 blank rows under
  the bar), rows 15 apart (a tag's 11 rows and 4), lines of F_S 13 apart; 5 blank rows under the last before the
  frame: h = 22 + 15 x rows for a window of rows. Nothing within 3 px of the frame (the audit: a text in a window
  begins 4 px in and ends 4 px in).
- **Pairs** (`pairs`): a true table of key -> action, one or two columns (a gutter of 7), filled row by row; every
  key's tag in the table one width, the widest key's plus 2 each side, its type centred, ink with paper type, rows
  y .. y + 10; every action 4 after the tags, aligned, F_S ink (fit to its column, a shorter form where it does not).
  Two tables in one window (SEQ's tools, then REC / PLAY) each keep their own tag width.
- **White keys** (`white_keys`, GLO and GLO latched): the 15 white keys drawn, 12 x 14 at a 14 px pitch (208 px,
  centred in the content), their groups labelled in F_S: FILL (7) and ALL (15) over their keys, MUTE (1-6), TAP (8)
  and SOLO (9-14) under theirs; 38 rows with the labels. The mute keys show the tracks (heard solid, muted framed,
  soloed away a dot, one waiting for the bar blinking between), a solo key solid while held, FILL solid while one is
  queued. It replaces the white keys' text.
- **Text**: a line only where a picture cannot say it (TAP GLO TO LET GO, AGAIN: AT ONCE), F_S, at x + 5.
- **The message** (toast) is not a window: the header's inverse for a moment, as above.

## Dithering

Ordered (Bayer 8 x 8, `T_BAYER`, `T_BMASK`), only where it means something: the machines' 3D marks and shadows, the
splash, the glitches between screens (`cyber`, the band slip, a big value's slip as it changes), a message's tail,
the SET page's disc. Checkers (not dither) for locks and the LFOs' fills. Dark mode inverts the whole screen.

## The splash

2.1 s at power on, then a 0.4 s dithered wipe into HOME; any key, button or knob (not MASTER) skips it, the press and
its release swallowed. One surface in dithered 3D on ink, its height morphing the whole time from one of MONO2K's
shapes into the next, eased, everywhere at once (no front, no cut): the wordmark's letters stretched over the slab
(0-120 ms), into the wave bank as a waterfall (120-440), GND-SIN's dome (540-840), the wavefolds (920-1220) and the
letters' relief (1300-1700), each holding ~100 ms between, still moving (the waves roll, the dome breathes, the folds
travel). The camera swings once (30 degrees to -16, home to 0) and rises over the relief from 1150 ms as the slab
narrows to the logotype's band; the light turns from the left to the front with it. The relief dissolves into the
crisp wordmark (1640-1880), then the type. The FM-1 shows ~13 frames a second: every stage is 100 ms or longer. A
frame works out two shapes a sample only mid-morph, with a quarter fewer rows (eased in), so the heaviest frame
stays near 700 k instructions. `m2k_host DIR --splash`, then `tools/splash.py DIR` for a GIF and a montage;
`m2k_draw --splash -a` every frame's cost.

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
