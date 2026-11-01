# MONO2K on the Move

The core (`firmware/src`), unchanged, as a [Schwung](https://github.com/charlesvestal/schwung) overtake module: eight tracks, the same samples as the FM-1 and the browser, in time with Move. The owner's plan is `move/PLAN.md`. Licence: GPL-3.0-only, as the rest of the repo.

## The controls (step 2)

| Move | MONO2K |
|---|---|
| **Knobs 1–8** | The page's 8 parameters, relative, accelerated. A touch shows the value large. |
| **Jog** | Turns the pages (below). A **click** opens the page list: turn, click to go. |
| **Shift + jog** | The track's machine (the FM-1's ALGO), through the catalogue of machines. |
| **Menu held** (400 ms) | The sound catalogue (the FM-1's EDIT held). The jog or knob 1 previews each sound on the track, knob 2 the category. A click or Up keeps it (undoable); Menu or Down puts the track's own sound back. |
| **Steps** | A press places a trig; a quick tap on a trig takes it away. **Hold** a step and turn a knob: a p-lock. Held, the jog sets its note, Shift + jog its condition, Up/Down its ratchet. |
| **Shift + steps** | The bank's 16 patterns (at the bar's end while playing; again before it: now). |
| **Shift + Left / Right** | The bank, A–D. |
| **Left / Right** | The bars of a pattern longer than 16 steps. Both together: follow the playhead again. |
| **Track buttons** | Tracks 1–4. **Shift +** a button: tracks 5–8. |
| **Pads** | The track chromatically, rows a fourth apart. The second row starts on the C of the track's octave, so that octave sits in the middle. Roots lit, held notes white. Velocity: an accent when REC records (over 110). |
| **Up / Down** | The track's octave (−3 … +3). |
| **Shift + pads** | PERFORM while held: bank A's 11 effects on the lower two rows, bank B's on the upper two. |
| **Mute held** | Steps 1–8 and the track buttons mute and unmute tracks. Mute tapped alone: the track in view. |
| **Rec** | REC (live record: pads, knobs as locks on the step playing). |
| **Shift + Rec** | Save (below). |
| **Copy held** | A step, then other steps: pasted there. A track button, then another: steps and sound. Shift + a step (a pattern), then another: the pattern. Steps held when Copy is pressed: those steps are the source. |
| **Delete held** | A step: its trig away. A track button: its steps cleared. Shift + a step: that pattern's steps cleared. A knob: the held step's lock away, else the value back to its INIT. Steps held when Delete is pressed: their locks cleared. |
| **Undo** | Undo. **Shift + Undo**: redo. |
| **Loop** | The track's arp HOLD. |
| **Play** | Move's and MONO2K's: one press starts both (below). |
| **Back** | Parks MONO2K (it plays on). **Shift + Back** closes it. Both the host's. |
| **Shift + volume touch + jog click** | Back to Move, always (the host's). |

### The pages

| Page | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| SYN | A | B | C | D | E | F | G | H |
| AMP | ATCK | HOLD | DEC | REL | DIST | VOL | PAN | PORT |
| FILT | BASE | WDTH | HPQ | LPQ | ATCK | DEC | B-OF | W-OF |
| EFX | EQF | EQG | SRR | DTIM | DSND | DFB | DBAS | DWID |
| DSTR | BITS | FOLD | XOR | VERB | TYPE | RATE | DPTH | MIX |
| TRK | PTCH | LEVL | MODE | SPD | RNG | GATE | BANK¹ | – |
| LFO1–3 | TRCK | DEST | WAVE | TRIG | MULT | SPD | PHAS | DPTH |
| SEQ | TLEN | SPD | PROB | LANE | – | – | – | – |
| GRID | X | Y | CHAOS | RES | BD | SD | HH | LANE |
| PAT | BPM² | SWNG | LEN | ACNT | DCAY | DAMP | ADPC | DRIV |

¹ DIGI-WAV's wave bank only. ² Move's tempo while following Move (turning it does nothing).

SYN to LFO3 are the track's (m2k.h's Monomachine pages). SEQ is the track's sequencing (the FM-1's HOME 2), GRID the pattern's Grids and the track's lane, and PAT the pattern's tempo, swing, length, accent and the bus.

Gestures that depend on a modifier (Shift, Mute, Copy, Delete) need it held first. The DSP hears the step or pad at once, but ui.js sends the modifier, so pressing both within a few ms may lose the order.

## In time with Move

- **One Play.** Play goes to Move (`button_passthrough: [85]`), and Move sends its start and stop to MONO2K's `on_midi`. MONO2K feeds them, and Move's clock, into the core's own clock in (`m2k_midi_from`, Move's clock in place of USB's).
- **Steady.** Move's 24-ppqn pulses arrive once a block, up to a block (or two) late, so they are not fed as they come.
  - A phase follows Move's count of pulses. It corrects 1/32 of the error a pulse, plus an integrator for a tempo slightly off.
  - Each pulse goes to the core at the 16-sample block where it falls.
  - The core's period is pinned to `host->get_bpm()` (Move's own tempo, held steady by Schwung). So the tempo, swing, gates, LFOs and synced delays never wobble.
  - Without a host tempo, the pulses are measured (a least-squares line over 4 beats) and held.
- **No clock from Move** (no set, a setting): a Play press with no answer from Move within ~70 ms plays MONO2K on its own clock. The screen says **INT** and shows the pattern's own tempo. Move's next start brings back **SYNC**.
- **Join:** following works while MONO2K is parked behind Move. Move playing when MONO2K opens: MONO2K joins at Move's next start.
- **Move's tempo can't be set from a module.** The plugin and JS APIs only read it (`get_bpm`, `move_info`). The tempo knob shows Move's BPM and says MOVE SETS IT.
  - The one route is Schwung's internal `desired-tempo` file, which its Link sidecar reads. It needs Link on and Move alone in the session, and changes at most once a second. Not used.
- `clock_lag` (module.json `defaults`, samples, 64 by default) moves MONO2K's steps against Move's pulses, if the ear says they sit early or late on the device.

## Saving

- **Where:** `/data/UserData/schwung/mono2k/mono2k.m2ks`. It sits outside the module's folder, so a redeploy keeps it. It is the core's store (format `M2K2` 5: patterns in pattern formats 18–21, kits, settings, projects, the sound library), so the Patch Editor reads it too.
- **When:**
  - **Shift + Rec** saves. As the FM-1's SAVE, the open pattern's reload point goes into its project first, then everything.
  - Closing MONO2K (Shift + Back) saves too. `"save_on_close": 0` in `defaults` turns that off.
  - A setting changed saves once it stands (the core's own rule).
- **How:** never from render.
  - The audio thread copies a snapshot a piece a block (~13 blocks).
  - The saver thread packs it with the core's `pat_pack`, writes `mono2k.m2ks.tmp`, fsyncs, and renames it over the last.
  - Closing writes with the core's own `m2k_store_pack` (the instance is retired by then). The bench proves both files are the same bytes.
- **Load:** at `create_instance`. A file the core refuses is kept as `mono2k.m2ks.bad`, and the factory plays.

## The screen and the LEDs

- **Page state:** each few blocks the controller fills a page state (`m2k_move_view.h`: the page, the 8 labels and values, the track, the pattern, BPM and SYNC, the steps, the overlays, the LEDs), on the audio thread. Its strings are made there, so a renderer needs nothing else.
- **Drawing:** the drawing thread (SCHED_OTHER, cores 0–2) draws it at 128 × 64 in the owner's pixel font at x1 (`m2k_move_draw.c`, plain). The final design replaces that file and reads the same state. Frames go through a triple buffer, swapped by pointer.
- **`get_param("frame")`:**
  - `M2K1`, then the frame's number (8 hex).
  - 64 rows of 22 characters: 6 pixels each from `'0'`.
  - 63 LEDs as two hex digits: 16 steps, 32 pads, 4 track buttons, then Rec Mute Undo Copy Delete Left Right Up Down Menu Loop.
- **ui.js** reads one frame ~30 times a second. It repaints only the rows that changed, as runs of `fill_rect`, and sends at most 16 LEDs a tick.
- **The LEDs:**
  - **Steps:** trig azure, p-locked trig dimmer azure, playhead and held white, beats faint. With Shift held, the bank's patterns; with Mute held, the tracks.
  - **Pads:** roots lit, held and sounding notes white. With Shift held, PERFORM's effects.
  - **Track buttons:** the track in view white, the others dim, muted ones dark red.
  - **Play's LED** stays Move's.

## Build, prove, deploy

```sh
sh move/build.sh        # dsp.so in an aarch64 Debian container, checked; packaged
sh move/proof.sh        # Mac vs container: self-tests (6 and 8 tracks), bank A bit for bit; dsp.so's benches; ui.js
sh move/deploy.sh       # to move.local over SSH (MOVE_HOST=user@host for another)
```

The screen's design sheets (step 3, `move/screen/DESIGN.md`), drawn by the renderer from real states into `build/move/sheets/`:

```sh
cc -O2 -DM2K_TRACKS=8 -Ifirmware/src -Ifirmware/gen -o build/move/screen_sheets move/screen/sheets.c && build/move/screen_sheets build/move/sheets && .venv/bin/python move/screen/sheets.py build/move/sheets
```

- **build.sh**
  - Every glibc symbol is at or under 2.35, the Move's.
  - Only `move_plugin_init_v2` is exported.
  - Every import is on a known list: memory, the clock, threads, sleeps, the store's file, and nothing that allocates or logs.
- **proof.sh** runs `move/test/bench.c` against `dsp.so`, through the plugin API as Schwung drives it:

  | Mode | What it proves |
  |---|---|
  | `hash` | A01–A04 bit for bit against `m2k_draw`. |
  | `rt` | 20 000 blocks of every control, Move's clock and saves asked for. Every allocation, file, thread, sleep and log call on the audio thread is counted: none. |
  | `ctl` | Every control (63 checks). |
  | `clock` | Synthetic Move clocks, each pulse seen at the next block boundary, 1 % of frames a block late: 120 BPM, a tempo change, stop, continue, start, stale, no host tempo, no clock, Play answered. |
  | `save` | The round trip. |
  | `draw` | The screens as PBM, in `build/move/proof/screens/`. |
  | `time` | The render's cost. |

- **Output:** the module in `build/move/modules/overtake/mono2k/` (`dsp.so`, `module.json`, `ui.js`), and `build/move/mono2k-move.tar.gz`, with `mono2k/` at its top (Schwung Manager: Install Custom Module).
- **deploy.sh** copies to `/data/UserData/schwung/.mono2k-incoming`, then renames it to `modules/overtake/mono2k`. Close an open MONO2K first (Shift + Back): a parked one keeps the old `dsp.so`.
- **Opening it:** MONO2K is in Schwung's Tools menu, under Overtake Modules. If it was open, close it first with Shift + Back: a parked MONO2K keeps the old `dsp.so` loaded.

## How it fits

- **`m2k_move.c`** is the glue: the plugin API, the queue, the clock follower, the saver, the frames. It includes `m2k_core.c` (as the browser's glue does), then `m2k_move_ui.c` (the controller: it calls the FM-1 UI's own helpers in `m2k_ui.c`) and `m2k_move_draw.c`.
  - One instance: the core is a set of statics.
  - One core: `m2k_par_begin` stays unset.
- **The audio thread** runs render, on_midi, set_param and get_param. Nothing there allocates, locks, logs or touches a file.
  - Pads, steps and Move's transport (on_midi), and ui.js's controls (`set_param("in", "K i d;J d;B cc v;T i v")`), go into one queue. The next render drains it in order.
  - The render runs the core 16 samples at a time, with Move's pulses between.
- **create / destroy** run on Schwung's loader thread. They load the store, start and join the two threads, and save on closing.
- **Other params:**
  - `get_param`: `"ui"` (step 1's line), `"view"`, `"dump"` and `"clock"` (text for the bench).
  - `set_param`: `"play"` and `"track"` (the bench's and the tools').
- **module.json `defaults`**, all optional: `seed`, `pattern`, `data_dir` (`""`: no store), `save_on_close`, `draw_thread` (0: draw in the render, every 8th block), `clock_lag`.
