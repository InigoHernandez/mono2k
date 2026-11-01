# The Move's screen: 128 x 64, 1 bit

MONO2K's pages redrawn for the Ableton Move's OLED, white on black. They use Elektron-style pixel type and pages with thin 1 px line pictures in Ableton's spirit, all MONO2K's own drawings. The renderer draws them from the core's real state, so these sheets are the real screen.

**Look first:** `build/move/sheets/overview.png` (every page, labelled). Then `png/*.png` (each x6; `*_grid.png` shows the pixel grid) and `motion_knob.gif`, `motion_pages.gif`, `motion_play.gif`.

```sh
cc -O2 -DM2K_TRACKS=8 -Ifirmware/src -Ifirmware/gen -o build/move/screen_sheets move/screen/sheets.c
build/move/screen_sheets build/move/sheets          # frames, the text audit, the time a frame
.venv/bin/python move/screen/sheets.py build/move/sheets
```

## The grid: 2 px

The FM-1's 4 px grid is two design pixels of its x2 type. At x1 that is 2 px, so the Move keeps the FM-1's proportions.
- Every box and rule sits on even rows and columns.
- Text sits on a baseline, the row under its 5-row capitals, so a tag round a word is 7 rows (0..6).
- A 4 px grid would leave 16 rows, too coarse for a 5-row type.

| Rows | Band |
|---|---|
| 0–6 | Header: the page's tag, the track, its machine's icon and model; the pattern, REC, SYNC's link, the tempo, the transport |
| 9–11 | Playhead strip: the track's 64 steps at 2 px each; trigs, beats, the 16 the step keys edit, the playhead |
| 14–38 | Stage: the page's one live picture. A touched knob takes it over: full name, value at x3 and unit, a ruler |
| 42–53 | 8 knob pictures, 16 px columns under the 8 knobs |
| 57–61 | Knob names (baseline 62). A line over a name means a held step's lock. Row 63 underlines the touched knob |

The header is dark with a lit tag rather than a lit bar. A static lit band would age an OLED, and Elektron's OLED pages do the same.

## The type

The owner's "Elektron Pixel Font" by savingaurora (CC BY-SA 3.0; MONO2K's adaptation CC BY-SA 4.0, `assets/fonts/mono2k-pixel`) runs at its native 8 px, x1. The renderer reads `tools/gen_font.py`'s own bitmaps (`F_S`, kept native).
- **Native size:** capitals 5 rows, 2 descender rows, glyphs 1–5 px wide, 1 px tracking.
- **Every glyph reads at x1** (`png/type.png`):
  - O is round and 0 square; S and 5, B and 8, I and 1 all differ.
  - N is the font's arch.
  - There is no lowercase, and MONO2K writes none.
- **One weight.** Large values are the same glyphs at x2 or x3 (each design pixel z x z), never bold.
- **Knob names:** at most 15 px, 2–3 px apart.
  - A 4-letter name is 17–20 px, so a 16 px column takes the Move's 3-letter forms (`ms_label`: ATK HLD DST POR BAS WID...).
  - The full name shows when the knob is touched.
- **The text audit** (`sheets.c`) checks every page, on every machine, for every value:
  - names fit their columns and are at least 2 px apart;
  - a value a column writes fits 14 px, with only its unit, sign, thousands and decimals left out;
  - a touched value fits the screen.

## The knobs' pictures

Each value is drawn as what it is, in 1 px lines, 14 x 12 (m2k_ui.c's `icon_of` kinds at 16 px).

| Picture | Used for |
|---|---|
| Dial (270° arc lit to the value, the rest dotted, a pointer) | Amounts |
| Bipolar dial (arc from the top) | Tunes, intervals |
| Fader (rounded slot, lit to the level) | Levels |
| Envelope segment (a rise, hold or fall as long as its time) | Times |
| Filter band (its edges, a resonance's peak) | Filter |
| Wave | Wave choices; DIGI-WAV draws its own wavetable's cycle |
| Pan (a handle on a dotted line) | Pan |
| Switch | On/off |
| Value written over a ruler or the choices' cells | Choices and numbers whose every value fits 14 px |
| Cells alone | Choices that don't fit |
| Dotted square | A free knob |

Stage pictures follow one language:
- curves are 1 px polylines;
- an area is a tint (a dot every 4th column and 2nd row);
- the touched part is drawn in hairlines;
- dotted floors and zero lines;
- 3 x 3 handles and markers in a dark ring.

## Icons

7 x 5, the capitals' height, so they sit on a baseline like glyphs. 1 px strokes on a 1 px grid, original (`png/icons.png`).
- **Pages:** HOME SYN AMP FILT EFX DSTR ARP LFO GLO GRID SET SOUNDS PERFORM.
- **Transport and states:** play, stop, REC, SYNC (a link), mute, speaker, lock, hold.
- **Machine families:**
  - GND: sine, perc, noise;
  - SID: a chip;
  - FM: two operators;
  - SWAV: saw, pulse, chord;
  - DIGI-WAV: a stepped wave;
  - VOX: a speech bubble;
  - DRM: kick, snare, hats, cymbal;
  - FX: chorus, ring, dynamics.

## Motion

Calm, the owner's default:
- pages cut, with no transitions or glitches;
- what moves is the sound: the playhead, the amp's cursor, the filter's live curve, the LFO's marker, the meters, the delay's echoes, the modulation sweep.

## The pages

The jog turns them, and a click opens the page list.

| Page | Knobs | Stage |
|---|---|---|
| HOME | LEV PCH BPM SW TLN SPD PRB LAN (names only) | 8 lanes: number, machine icon, 16 steps, level; the playhead's cells inverted |
| SYN | the machine's A–H | class, style, model at x3; punch, metal, plastic |
| AMP | ATK HLD DEC REL DST VOL PAN POR | the envelope, one live curve |
| FILT | BAS WID HPQ LPQ ATK DEC BOF WOF | the response, one live curve; the envelope's reach dotted |
| EFX | EQF EQG SRR DTM DSN DFB DBS DWD | EQ and rate steps; echoes (ping-pong each side in turn, dotted with no send) |
| DSTR | BIT FLD XOR VRB TYP SPD DPT MIX | a sine through the crush; the modulation's sweep |
| ARP | PCH LEV ARP SPD RNG GAT | the held notes as a staircase, the one sounding lit; HOLD |
| LFO1–3 | TRK DES WAV TRG MUL SPD PHS DPT | the wave as deep as it swings, live; where it goes, its cycle |
| GLO | BPM SW LEN ACC DCY DMP ADP DRV | the tempo at x3 (SYNC's link when the Move's); the reverb's tail |
| GRID | X Y CHA RES BD SD HH LAN | Grids' map; the three lanes' cycles |
| SET | CHN SYI SYO NOU NIN THR MUT MOT | MIDI's ins and outs, lit when on |
| SOUNDS | (jog) | the library's rows, the one landed on lit and playing; machine, note, category |
| PERFORM | DCY DMP ADP DRV | the pads as the Move has them, the effects playing lit, named large |

## The renderer

- **Files:** `m2k_screen.h` holds the API and the page-state struct. `m2k_screen.c` is part of the core's unity build: include it after `firmware/src/m2k_core.c`.
- **The controller fills `m2k_scr_t`:** the page, the track, the touched knob, the held step, the step page, the list and catalogue cursors, SYNC, Shift, and the time.
- **Calls:**
  - `m2k_screen()` returns 1 KB, rows of 16 bytes, MSB first, 1 = lit;
  - `m2k_screen_param()` names each knob's parameter.
- **Constraints:** integer only, nothing allocated. It clears `mon.peak` as `m2k_frame` does, so call one or the other.
- **Cost on the host (M-series Mac, -O2):** a frame takes 0.5–2.9 µs, at most 65k instructions (FILT). On the Move's A72 that is roughly tens of µs.
