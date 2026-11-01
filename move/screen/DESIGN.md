# The Move's screen and pads: 128 x 64, 1 bit, 32 pads

MONO2K's pages for the Ableton Move's OLED, white on black, and the pads that lead to them. Elektron-style pixel type, thin 1 px line pictures in Ableton's spirit, all MONO2K's own drawings. dsp.so's own code draws the sheets from real states, so they are the real screen and the real LEDs.

**Look first:** `build/move/sheets/overview.png` (every page), `padmap.png` (the pads, lit), then `motion_pages.gif` (the page pads pressed), `motion_knob.gif` (a knob touched and turned), `motion_play.gif` (the playhead). `png/*.png` are each frame x6 (`*_grid.png` with the pixel grid).

```sh
cc -O2 -DM2K_TRACKS=8 -Ifirmware/src -Ifirmware/gen -o build/move/screen_sheets move/screen/sheets.c
build/move/screen_sheets build/move/sheets          # frames, LEDs, the audits, the cost a frame
.venv/bin/python move/screen/sheets.py build/move/sheets
```

## The owner's verdict on step 3 (2026-11-01), and what changed

| Verdict | Now |
|---|---|
| "Names on touch only" | The knob row is the 8 pictures alone. Touched or turned, a knob's full name and its value large take the stage. |
| "Good but calmer" | The playhead strip, the machine card's stats, the decade ticks, the LFO's 1-2-3 tags, the EQ's rate picture and the SOUNDS footer are gone. HOME is eight quiet lanes. The header holds the pad, the page, the track and the pattern. |
| The pads: pages and functions on the top two rows | Every page a pad, one press. The page in view's pad is drawn in the header. |
| "Sequencer is always available" | The steps are the sequencer on every page. HOME's lanes and the header's bar cells show where. |

## The grid: 2 px

The FM-1's 4 px grid at the type's x1: every box and rule sits on even rows and columns, text on a baseline (the row under its 5-row capitals).

| Rows | Band |
|---|---|
| 0–7 | Header: the pad map, the page's tag, the track and its machine; the bars, the pattern, REC, the transport |
| 12–43 | Stage: the page's one live picture. A touched knob takes it over: full name, value at x3 (x2 if long), unit, a ruler |
| 47 | A held step's lock: a dash over its knob's picture |
| 50–61 | The 8 knob pictures, 16 px columns, 14 x 12 each. Nothing under them |
| 63 | The knob touched, a line under its picture |

Rows 8–11 and 44–49 stay black: the calm.

## The header

```
:▪::::::  FILT  T1 ≡           ▪..  A02 ▶
└ pad map └ tag └ track, machine └ bars └ pattern, transport
```

- **The pad map** (x 0–22): the two page rows of pads as two rows of eight dots, the top row (the sound) over the second (the rest). The pad that leads to the page in view is a 2 x 2 block, and PERFORM's too while its keys are on. The eye goes from the lit pad to the same place on the screen.
- **The tag:** filled for the sound pages (the top row, orange on the pads), the word underlined for the rest (azure, and the functions magenta). The same split as the pads' colours.
- **The track** on the pages that are a track's, then its machine's icon and a mute, if there is room.
- **The right:** the transport, **INT** beside it when MONO2K plays its own tempo (SYNC, Move's, is the rule and says nothing), REC, the pattern (`>A04` when one is queued; `BANK A` while Shift is held, `MUTE` while Mute is), and the bars as 2 x 3 cells when the track runs past 16 steps (the one the steps show filled) and there is room.
- The right is drawn first and the left's extras give way to it, the bars too (the audit holds the fullest header on every page: track 8 muted, REC, INT, a pattern queued, four bars).
- The header is dark with a small lit tag rather than a lit bar: a static lit band would age an OLED.

## The type

The owner's "Elektron Pixel Font" by savingaurora (CC BY-SA 3.0; MONO2K's adaptation CC BY-SA 4.0, `assets/fonts/mono2k-pixel`) at its native 8 px, x1. The renderer reads `tools/gen_font.py`'s own bitmaps (`F_S`, kept native).
- Capitals 5 rows, 2 descender rows, glyphs 1–5 px wide, 1 px tracking. O is round and 0 square; S and 5, B and 8, I and 1 differ. No lowercase.
- **One weight.** Large values are the same glyphs at x2 or x3, never bold.
- **Names on touch only.** A knob's full name (A–H before a machine's, the track before MIX's), or its four letters where the full one will not fit beside LOCK and STEP 64.
- **The audit** (`sheets.c`): every page, on every machine, every value: what a column writes fits its 14 px whole; a touched name fits; a touched value and its unit fit at x2.

## The knobs' pictures

Each value drawn as what it is, in 1 px lines, 14 x 12 (m2k_ui.c's `icon_of` kinds at 16 px). A free knob draws nothing.

| Picture | Used for |
|---|---|
| Dial (270° arc lit to the value, the rest dotted, a pointer) | Amounts |
| Bipolar dial (arc from the top) | Tunes, intervals |
| Fader (rounded slot, lit to the level) | Levels (all of MIX's) |
| Envelope segment (a rise, hold or fall as long as its time) | Times |
| Filter band (its edges, a resonance's peak) | Filter |
| Wave | Wave choices; DIGI-WAV draws its own wavetable's cycle |
| Pan (a handle on a dotted line) | Pan |
| Switch | On/off |
| Value over a ruler, or over the choices' cells | Choices and numbers whose every value fits 14 px |
| Cells alone | Choices that don't fit |

Stage pictures share one language: 1 px curves, an area as a tint (a dot every 4th column and 2nd row), the touched part in hairlines, dotted floors and zero lines, 3 x 3 handles and markers in a dark ring.

## Icons

7 x 5, the capitals' height. 1 px strokes, original (`zz_icons` in the sheets): the pages, play, stop, REC, SYNC's link, mute, speaker, lock, hold, MIX's faders, and the machine families (GND sine, perc, noise; SID chip; FM operators; SWAV saw, pulse, chord; DIGI-WAV stepped wave; VOX speech bubble; DRM kick, snare, hats, cymbal; FX chorus, ring, dynamics).

## Motion

Calm: pages cut, no transitions, no glitches. What moves is the sound: HOME's playheads, the amp's cursor, the filter's live curve, the LFO's marker, MIX's meters, the modulation's sweep, PERFORM's keys. A frame every two of the panel's (34.8 ms, ~28.7 a second: `move/README.md`, The screen).

## The pages

| Pad | Page | Knobs | Stage |
|---|---|---|---|
| top 1 | SYN | the machine's A–H | its class and style; its model at x3 |
| top 2 | AMP | ATK HLD DEC REL DST VOL PAN POR | the envelope, one live curve |
| top 3 | FILT | BAS WID HPQ LPQ ATK DEC BOF WOF | the response, one live curve; the envelope's reach dotted |
| top 4 | EFX | EQF EQG SRR DTM DSN DFB DBS DWD | the EQ's band; the echoes (ping-pong each side in turn, dotted with no send) |
| top 5 | DSTR | BIT FLD XOR VRB TYP SPD DPT MIX | a sine through the crush; the modulation's sweep |
| top 6–8 | LFO1–3 | TRK DES WAV TRG MUL SPD PHS DPT | where it goes and its cycle, small; the wave as deep as it swings, live |
| 2nd 1 | HOME | LEVL PTCH BPM SWNG TLEN SPD PROB LANE (no pictures) | 8 lanes: the number (the track in view's marked), 16 steps by the beat, a trig a block, one that may not play framed, a muted lane's dashes, each playhead a line under its step |
| 2nd 2 | TRK | PCH LEV ARP SPD RNG GAT (BNK) | the arp's notes as a staircase, the one sounding lit; HOLD |
| 2nd 3 | PAT | BPM SW LEN ACC TLN SPD PRB LAN | the tempo at x3 (SYNC's link when the Move's, INT when MONO2K's own), the bar's beats; the pattern's length and the track's, each playhead a marker |
| (Shift + knob 1) | any | the tempo | as a knob touched: TEMPO, its value at x3 with its tenths, SYNC or INT beside it, a ruler from 30 to 285 |
| 2nd 4 | GRID | X Y CHA RES BD SD HH LAN | Grids' map; the three lanes' cycles, the track's in a tag |
| 2nd 5 | MIX (a proposal) | the 8 tracks' LEVL, a knob a track | a strip a track: its number, its level live, a muted one's speaker |
| 2nd 6 | GLO | DCY DMP ADP DRV MUTE | the reverb's tail across the width |
| 2nd 7 | SOUNDS | (jog, knob 1 the slot, knob 2 the category) | the library's rows, the one heard lit; the category picked (else the track) in the header |
| 2nd 8 | PERFORM | DCY DMP ADP DRV | the keys as the pads lay them (bank B over bank A), the effects playing lit; what plays named |
| (jog) | PATTERNS | | the patterns' rows, the one open with the transport's mark, the one queued NEXT |

A message (the octave, a copy, a save) shows a moment in a dark box over the stage, its first line at x2. Changing patterns shows none: the header says it.

## The pads

The top two rows lead to the pages; the bottom two are the keyboard; the 16 steps are the sequencer. `padmap.png` shows three states as they light.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | Colour (in view / not) |
|---|---|---|---|---|---|---|---|---|---|
| Top | SYN | AMP | FILT | EFX | DSTR | LFO1 | LFO2 | LFO3 | orange 3 / 69 |
| 2nd | HOME | TRK | PAT | GRID | MIX | GLO | SOUNDS | PERFORM | azure 16 / 95; the two functions magenta 26 / 115 |
| 3rd | C# | D# | · | F# | G# | A# | · | · | dark grey 119; PERFORM: bank B, pink 109 |
| Bottom | C | D | E | F | G | A | B | C | grey 123; PERFORM: bank A, magenta 115 |

- The keyboard's root is lit orange (the sound's colour), the notes played white, by hand or by the sequencer. The pads no key has are dark.
- With PERFORM on, its 11 + 11 effects play from the keyboard rows 8 at a time (bank A on the bottom row, B on the top); Up shows each bank's 9–11, Down their 1–8.
- The steps keep step 2's colours: a trig azure, a locked trig dimmer, the playhead and a held step white, a beat's first step faint.
- **Why these:** a pad a page puts every page one press away (the owner's "no eternal pagination"); the sound on the top row, the row nearest the screen and the knobs; HOME first on the row under it, where the hand starts; the two functions at the right end, apart in their own colour; MIX next to GLO (the mix and the bus). The piano's rows keep the Elektron trig-key shape the owner named; the root in the sound's orange ties the keyboard to the sound pages above it.

## The renderer

- **Files:** `m2k_screen.h` holds the pages, the pad map, the controller's moment (`m2k_scr_t`) and the plain view (`ms_view_t`). `m2k_screen.c` is part of the core's unity build.
- **Two halves:**
  - `m2k_screen_gather` (the audio thread, at each publish) reads the core and the moment into the view: numbers and strings, the knobs' pictures as kinds, positions and texts, each stage's few values.
  - `m2k_screen_draw` (the drawer thread) draws the view alone, with the type, the icons, the core's const tables and its pure functions (`env_level`, `filt_db`, `lfo_shape`, `sinp`...).
  - `m2k_screen()` does both into its own frame (the sheets).
- **Held to it:** every still in the sheets is drawn again at the end from its kept view, with the core in another pattern and state: the same frame, every time.
- **Constraints:** integer only, nothing allocated. `m2k_screen_init` sweeps every kind's values once at create (which values a column can write), so no gather ever does. The gather reads `mon.peak` and clears it, as `m2k_frame` does.
- **Cost a frame:** on the Mac (hot, -O2) a publish (the controller's state, the LEDs, the gather) 0.3–0.6 µs, 6–15 k instructions; a draw 0.3–2.3 µs, 10–55 k (FILT the most). In the Move's container paced to real time: a publish 11.8 µs mean, a draw 15.0 µs mean on the drawer thread (cold caches; the block's own render is 6x slower there than hot). `move/README.md` has the table.
