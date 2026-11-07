# The Move's screen and pads: 128 x 64, 1 bit, 32 pads

MONO2K's pages for the Ableton Move's OLED, white on black, and the pads that lead to them. Elektron-style pixel type, thin 1 px line pictures in Ableton's spirit, all MONO2K's own drawings. dsp.so's own code draws the sheets from real states, so they are the real screen and the real LEDs.

**Look first:** `build/move/sheets/overview.png` (every page), `knob_rows.png` (every page's knob row, SYN on every machine, x6), `padmap.png` (the pads, lit), then `motion_pages.gif` (the page pads pressed), `motion_knob.gif` (a knob touched and turned), `motion_play.gif` (the playhead). `png/*.png` are each frame x6 (`*_grid.png` with the pixel grid).

```sh
cc -O2 -DM2K_TRACKS=8 -Ifirmware/src -Ifirmware/gen -o build/move/screen_sheets move/screen/sheets.c
build/move/screen_sheets build/move/sheets          # frames, LEDs, the knob rows, the audits, the cost a frame
.venv/bin/python move/screen/sheets.py build/move/sheets [--before OLD]   # OLD: another build's sheets: knobs_before_after.png
```

## The owner's ask on the knob row (2026-11-02), and what changed

"Align better and refresh the icons to fill more on grid and less messy crowded... manage better the space. I like the icons but it is not very clean." The family stays (dials, faders, segments, filter bands, waves, pan, switches, choices, values); the row is rebuilt on one box.

| On the device | Now |
|---|---|
| Neighbours 1–2 px apart: one noisy strip | Every picture in the same 13 x 13 box at columns 2–14 of its 16: a 3 px gutter between any two |
| Mixed heights and baselines | One top row (49) and one bottom row (61) for every box, and every line picture (dial, fader, segment, filter, wave, pan, places) touches both at every value; a switch and a written value centred on the box's middle rows. (The owner's "good but align this", 2026-11-02: the filters had sat 3 rows low) |
| Dials as scattered dots | A solid ring, the value a solid band inside it, a pointer from the middle |
| Dotted floors and rulers under segments, filters, numbers, pan | No dots in the row. One language: a thin line is the range or the shape, solid is the value |
| Values pushed out of their columns, touching the next | A value is written only if every value of its kind fits the box (13 px), centred in it; else it is drawn. Names (an LFO's destination) up to 14, within their column |

## The owner's verdict on step 3 (2026-11-01), and what changed

| Verdict | Now |
|---|---|
| "Names on touch only" | The knob row is the 8 pictures alone. Touched or turned, a knob's full name and its value large take the stage. |
| "Good but calmer" | The playhead strip, the machine card's stats, the decade ticks, the LFO's 1-2-3 tags, the EQ's rate picture and the SOUNDS footer are gone. HOME is eight quiet lanes. The header holds the pad, the page, the track and the pattern. |
| The pads: pages and functions on the top two rows | Every page a pad, one press. The page in view's pad is drawn in the header. |
| "Sequencer is always available" | The steps are the sequencer on every page. HOME's lanes and the header's bar cells show where. |

## The grid: 2 px

The FM-1's 4 px grid at the type's x1: every box and rule sits on even rows and columns, text on a baseline (the row under its 5-row capitals). The knob row is the exception: its boxes are 13 px, odd, so every dial, fader, pan and word has a middle column and a middle row.

| Rows | Band |
|---|---|
| 0–7 | Header: the pad map, the page's tag, the track and its machine; the bars, the pattern, REC, the transport |
| 12–43 | Stage: the page's one live picture. A touched knob takes it over: full name, value at x3 (x2 if long), unit, a ruler. A step held past a tap takes it too (HOME: to row 60), unless a knob is touched: STEP n, how many more and LEN n if it has a length, tags for its trig type, accent and slide, then its condition, note and ratchet at x2 at columns 2, 62 and 102, each over what turns it: JOG in a tag under what the jog sets, its letters under the value's, CLICK under the other |
| 47 | A held step's lock: a line over its knob's box, as wide as the box |
| 49–61 | The 8 knob pictures: 16 px columns, each picture in a 13 x 13 box at columns 2–14 of its column (a 3 px gutter between two; the screen's left edge 2 px, its right 1) |
| 63 | The knob touched: a line under its box, as wide as the box |

Rows 8–11, 44–46, 48 and 62 stay black: the calm.

## The header

```
:▪::::::  FILT  T1 ≡           ▪..  A02 ▶
└ pad map └ tag └ track, machine └ bars └ pattern, transport
```

- **The pad map** (x 0–22): the two page rows of pads as two rows of eight dots, the top row (the sound) over the second (the rest). The pad that leads to the page in view is a 2 x 2 block, and PERFORM's too while its keys are on. The eye goes from the lit pad to the same place on the screen.
- **The tag:** filled for the sound pages (the top row, orange on the pads), the word underlined for the rest (azure, and the functions magenta). The same split as the pads' colours.
- **The track** on the pages that are a track's, then its machine's icon and a mute, if there is room.
- **PADS' mark** (the owner's, 2026-11-02): while the keyboard rows are PADS (the Sample button), four 2 x 2 pads, 5 px, after the track's icons (after the tag where a page has no track), before HOME's pattern name: short of room it goes before the track's icons do, and HOME's name before it.
- **The right:** the transport, **INT** beside it when MONO2K plays its own tempo (SYNC, Move's, is the rule and says nothing), REC, the pattern (`>A04` when one is queued; `BANK A` while Shift is held, `MUTE` while Mute is), and the bars as 2 x 3 cells when the track runs past 16 steps (the one the steps show filled) and there is room.
- The right is drawn first and the left's extras give way to it, the bars too (the audit holds the fullest header on every page: track 8 muted, REC, INT, a pattern queued, four bars, PADS).
- The header is dark with a small lit tag rather than a lit bar: a static lit band would age an OLED.

## The type

The owner's "Elektron Pixel Font" by savingaurora (CC BY-SA 3.0; MONO2K's adaptation CC BY-SA 4.0, `assets/fonts/mono2k-pixel`) at its native 8 px, x1. The renderer reads `tools/gen_font.py`'s own bitmaps (`F_S`, kept native).
- Capitals 5 rows, 2 descender rows, glyphs 1–5 px wide, 1 px tracking. O is round and 0 square; S and 5, B and 8, I and 1 differ. No lowercase.
- **One weight.** Large values are the same glyphs at x2 or x3, never bold.
- **Names on touch only.** A knob's full name (A–H before a machine's, the track before MIX's), or its four letters where the full one will not fit beside LOCK and STEP 64.
- **The audit** (`sheets.c`): every page, on every machine, every value: what a column writes fits its box whole (13 px; a name 14, its column less a pixel a side); every knob's picture stays in its 13 x 13 box, and a line picture touches its top row and its foot (DIGI-WAV's waves in both banks); a touched name fits; a touched value and its unit fit at x2.

## The knobs' pictures

Each value drawn as what it is (m2k_ui.c's `icon_of` kinds), in one 13 x 13 box, so every picture shares the row's top and bottom and its gutter. One language, the same on every kind: **a thin 1 px line is the range or the shape, solid is the value**; nothing dotted. A free knob draws nothing.

| Picture (`MP_*`) | In the box | Used for |
|---|---|---|
| Dial (`DIAL`) | A knob from above: its 13 px ring whole; inside it a solid band from the sweep's start (7:30, 270° round to 4:30) to the value; a pointer from the middle to the ring | Amounts |
| Bipolar dial (`BIDIAL`) | The same, the band from 12 o'clock to either side; at the middle the pointer alone, straight up | Tunes, intervals, bipolar amounts |
| Fader (`SLIDER`) | A rounded slot 7 wide, the box's height; solid inside it from the floor to the level, 3 wide | Levels (all of MIX's) |
| Envelope segment (`ENV`) | A rise then held (ATK), a hold (HLD), a fall then out (DEC, REL): its run across the box the time | Times |
| Filter band (`FILT`) | The passband on the box's top row, each edge a slope 3 wide down to the foot (an open side runs out flat): BASE moves the low edge, WDTH the high one; HPQ and LPQ a solid corner under their edge, up to 6 rows as the Q rises (a peak cannot rise past the top row) | The filter |
| Wave (`WAVE`, `DWAV`) | A cycle across the box, its lowest and highest on the box's foot and top (a quiet wavetable's shape shown whole) | Wave choices; DIGI-WAV draws its own wavetable's cycle |
| Pan (`PAN`) | A line across the middle, its middle marked; the place a solid handle 3 wide, the box's height, in a dark ring | Pan |
| Switch (`TOGGLE`) | A pill the box's width, 7 tall: off, its frame and a solid knob at the left; on, solid with the knob dark at the right | On/off |
| Places (`SEGS`) | A floor, a tick a place on it, the one chosen standing solid, the box's height and at least 3 wide | Choices of up to 6 that don't fit the box (7 or 8, rates and multipliers: a dial) |
| Value (`CHOICE`, `TEXT`, `NUM`) | The word alone, centred, its capitals the box's rows 4–8; no ruler, no cells | Choices, names and numbers whose every value fits 13 px (a name 14) |

A held step's lock is a line over the box (row 47), the knob touched a line under it (row 63), each the box's width.

Stage pictures share one language: 1 px curves, an area as a tint (a dot every 4th column and 2nd row), the touched part in hairlines, dotted floors and zero lines, 3 x 3 handles and markers in a dark ring. (The dots stay on the stage's large pictures, where they read as a tint; the knob row has none.)

## Icons

7 x 5, the capitals' height. 1 px strokes, original (`zz_icons` in the sheets): the pages, play, stop, REC, SYNC's link, mute, speaker, lock, hold, MIX's faders, PADS' four pads (5 wide), and the machine families (GND sine, perc, noise; SID chip; FM operators; SWAV saw, pulse, chord; DIGI-WAV stepped wave; VOX speech bubble; DRM kick, snare, hats, cymbal; FX chorus, ring, dynamics).

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

A message (the octave, a copy, a save) shows a moment in a dark box over the stage, its first line at x2 where it fits (else x1), each line 104 px at most (`MS_TOAST_TW`). Changing patterns shows none: the header says it. PADS and Capture say theirs: PADS / T1-T8, MUTES ABOVE; KEYS / PLAY THE TRACK (under PERFORM: WHEN PERFORM IS OFF); CAPTURED 2 BARS / T1 T2 T3; NOTHING TO CAPTURE. Capture's tracks are written to fit (`m2k_screen_text_w`; runs, T1-T8, if a list were too wide), and the audit says every one of them, the fullest (4 bars, all 8 tracks) among them, fits.

## The pads

The top two rows lead to the pages; the bottom two are the keyboard (or PADS); the 16 steps are the sequencer. `padmap.png` shows four states as they light: HOME, keys held, PERFORM, PADS.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | Colour (in view / not) |
|---|---|---|---|---|---|---|---|---|---|
| Top | SYN | AMP | FILT | EFX | DSTR | LFO1 | LFO2 | LFO3 | orange 3 / 69 |
| 2nd | HOME | TRK | PAT | GRID | MIX | GLO | SOUNDS | PERFORM | azure 16 / 95; the two functions magenta 26 / 115 |
| 3rd | C# | D# | · | F# | G# | A# | · | · | dark grey 119; PERFORM: bank B, pink 109 |
| Bottom | C | D | E | F | G | A | B | C | grey 123; PERFORM: bank A, magenta 115 |

- The keyboard's root is lit orange (the sound's colour), the notes played white, by hand or by the sequencer. The pads no key has are dark.
- With PERFORM on, its 11 + 11 effects play from the keyboard rows 8 at a time (bank A on the bottom row, B on the top); Up shows each bank's 9–11, Down their 1–8.
- **PADS** (the Sample button, the owner's, 2026-11-02): the bottom row the 8 tracks, each at its root, dim azure 95, white 120 while sounding; the row over it their mutes, azure 16 heard, dark red 66 muted (Mute's layer's colours), a mute waiting for the bar blinking dark every 128 ms (the FM-1's GLO > PLAY). PERFORM still takes both rows while it is on. Sample's own LED azure while PADS is on.
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
- **Cost a frame:** on the Mac (hot, -O2) a publish (the controller's state, the LEDs, the gather) 0.3–0.6 µs, 6–15 k instructions; a draw 0.4–2.4 µs, 10–59 k (FILT the most; a solid dial ~2.5 k). In the Move's container paced to real time: a publish 11.8 µs mean, a draw 15.0 µs mean on the drawer thread (cold caches; the block's own render is 6x slower there than hot). `move/README.md` has the table.
