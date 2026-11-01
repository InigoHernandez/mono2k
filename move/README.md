# MONO2K on the Move

The core (`firmware/src`), unchanged, as a [Schwung](https://github.com/charlesvestal/schwung) overtake module: eight tracks, the same samples as the FM-1 and the browser, in time with Move. The owner's plan is `move/PLAN.md`. Licence: GPL-3.0-only, as the rest of the repo.

## The controls

The owner's layout (2026-11-01): every page one press away on the top two rows of pads, the keyboard on the bottom two, the sequencer always on the 16 steps. The pad map, lit as the controller lights it: `build/move/sheets/padmap.png`.

```
          1      2      3      4      5      6      7      8
T1 ┃  SYN    AMP    FILT   EFX    DSTR   LFO1   LFO2   LFO3     the sound (orange)
T2 ┃  HOME   TRK    PAT    GRID   MIX    GLO    SOUNDS PERFORM  the rest (azure), the two functions (magenta)
T3 ┃  C#     D#     ·      F#     G#     A#     ·      ·        the keyboard: black keys
T4 ┃  C      D      E      F      G      A      B      C        white keys (PERFORM on: bank B over bank A)
     [1 ][2 ][3 ][4 ][5 ][6 ][7 ][8 ][9 ][10][11][12][13][14][15][16]   the sequencer
```

| Move | MONO2K |
|---|---|
| **Top pad row** | The sound of the track in view, a page a pad: SYN AMP FILT EFX DSTR LFO1 LFO2 LFO3. |
| **Second pad row** | HOME (the pattern), TRK (the track's pitch, level and arp), PAT (the time), GRID, MIX (a proposal: the 8 levels), GLO (the bus, mutes at the bar), then the functions: **SOUNDS** (the catalogue) and **PERFORM**. |
| **Page pads' lights** | A family a colour: the sound orange, the rest azure, the functions magenta. The page in view (or a function on) bright, the rest dim. |
| **Knobs 1–8** | The page's 8 parameters. A touch (or a turn) shows the knob's full name and its value large. MONO2K's own feel (ui.js, below): two detents a value, slow or brisk; up to 2.5 values a detent on a flick; a short list (a wave, a mode) 3.5 detents an option. |
| **Shift + knob 1** | The tempo, on every page (PAT's knob 1 and HOME's knob 3 too). Shown large while touched or turned, with SYNC or INT. 0.1 BPM a step turned slowly, whole BPM turned fast. Under SYNC it is Move's and MONO2K's: MONO2K moves at once, Move catches up (below, In time with Move). **Delete +** it: Move's tempo again. |
| **Bottom two pad rows** | The keyboard, a two-row piano: the white keys C D E F G A B C on the bottom row, the black keys over theirs (C# D# · F# G# A# · ·). The pads no key has are dark, the root is lit orange, the notes played (by hand or by the sequencer) white. Velocity: an accent when REC records (over 110). |
| **Up / Down** | The track's octave (−3 … +3), said on the screen a moment. With PERFORM on: the moves' page (MOVES 1, MOVES 2). |
| **SOUNDS** (pad) | The sound catalogue (the FM-1's EDIT held). The jog or knob 1 previews each sound on the track, knob 2 the category. A click or Up keeps it (undoable); the pad again, Menu or Down puts the track's own sound back. |
| **PERFORM** (pad) | Its effects on the keyboard rows while it is on. **Top row, the colours** (they stack, held with anything): LPF DUB HPF GATE CRUSH DRIVE WOW SHIMMER. **Bottom row, the moves** (one at a time, the next taking over): RPT 1/4 RPT 1/8 RPT 1/16 RPT 1/32 ROLL STUTTER REVERSE STOP. Each column a pair that plays together. Up: the moves' second page (FREEZE SCATTER OCT DN OCT UP SPIN KILL), the colours staying; Down: back. It stays on across pages; the pad again gives the keyboard back. Its page's knobs are the bus's four. |
| **Steps** | The sequencer on every page. A press places a trig; a quick tap on a trig takes it away. **Hold** a step and turn a knob: a p-lock. Held, the jog sets its note, Shift + jog its condition, Up/Down its ratchet. |
| **Shift + steps** | The bank's 16 patterns (at the bar's end while playing; again before it: now). |
| **Shift + Left / Right** | The bank, A–D. |
| **Left / Right** | The bars of a pattern longer than 16 steps. Both together: follow the playhead again. |
| **Jog** | The lists, never the pages. Turned: the patterns' list opens at the pattern playing and the cursor moves; a **click** loads the one under it (at the bar's end while playing; a second click before it: now). Left alone 3 s, it closes. In SOUNDS it scrolls the catalogue. |
| **Shift + jog** | The track's machine (the FM-1's ALGO), through the catalogue of machines. |
| **Jog click** | In a list: load or keep. Otherwise it opens the patterns' list. |
| **Menu** | Closes a list (SOUNDS: the track's own sound back). |
| **Track buttons** | Tracks 1–4. **Shift +** a button: tracks 5–8. |
| **Mute held** | Steps 1–8 and the track buttons mute and unmute tracks. Mute tapped alone: the track in view. |
| **Rec** | REC (live record: keys, knobs as locks on the step playing). |
| **Shift + Rec** | Save (below). |
| **Copy held** | A step, then other steps: pasted there. A track button, then another: steps and sound. Shift + a step (a pattern), then another: the pattern. Steps held when Copy is pressed: those steps are the source. |
| **Delete held** | A step: its trig away. A track button: its steps cleared. Shift + a step: that pattern's steps cleared. A knob: the held step's lock away, else the value back to its INIT. Steps held when Delete is pressed: their locks cleared. |
| **Undo** | Undo. **Shift + Undo**: redo. |
| **Loop** | The track's arp HOLD. |
| **Play** | Move's and MONO2K's: one press starts both (below). |
| **Back** | Parks MONO2K (it plays on). **Shift + Back** closes it. Both the host's. |
| **Shift + volume touch + jog click** | Back to Move, always (the host's). |

Gone since step 2: the jog turning pages and its page list (every page has a pad), Shift + pads for PERFORM (its pad), Menu held for the catalogue (SOUNDS' pad). Shift + a knob did nothing of its own before the tempo came (the page's knob, as without Shift): only knob 1 changed; Shift + knobs 2–8 are still the page's.

**The knobs' feel** (the owner: "too sensitive"; ui.js, its constants together at the top to retune):
- A Move knob sends ~210 detents a turn (Schwung's `docs/EC4_SURFACE.md`, counted). Schwung's `decodeAcceleratedDelta` gave up to 10 steps a detent whenever they came 25 ms apart or closer, which any steady turn does.
- MONO2K's curve, a knob each: the speed is the detents a second over the last 4 messages. Up to 250 a second (~1.2 turns a second) a detent is half a step; it rises smoothly to 2.5 steps by 900 a second (a flick).
- The fractions carry over. A fresh turn's first detent always moves one step. A reversal or a 400 ms pause starts fresh.
- 0–127 at a brisk turn (a turn a second): ~1.2 turns (253 detents). A short list (32 options or fewer, flagged by the DSP in the frame) moves 1/1.75 as fast: 3.5 detents an option.
- The tempo: 0.05 BPM a detent (0.1 BPM in two) under 150 detents a second; above, whole BPM at 0.3 of the curve (~0.15 BPM a detent brisk, 0.75 a flick), landing on whole numbers.
- The DSP adds each step once (`mv_knob`; a list moves as many options as it is sent): no second acceleration. The jog stays a step a detent.

### The pages

| Page | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| SYN | A | B | C | D | E | F | G | H |
| AMP | ATCK | HOLD | DEC | REL | DIST | VOL | PAN | PORT |
| FILT | BASE | WDTH | HPQ | LPQ | ATK | DEC | B-OF | W-OF |
| EFX | EQF | EQG | SRR | DTIM | DSND | DFB | DBAS | DWID |
| DSTR | BITS | FOLD | XOR | VERB | TYPE | RATE | DPTH | MIX |
| LFO1–3 | TRCK | DEST | WAVE | TRIG | MULT | SPD | PHAS | DPTH |
| HOME | LEVL | PTCH | BPM² | SWNG | TLEN | SPD | PROB | LANE |
| TRK | PTCH | LEVL | MODE | SPD | RNG | GATE | BANK¹ | – |
| PAT | BPM² | SWNG | LEN | ACNT | TLEN | SPD | PROB | LANE |
| GRID | X | Y | CHAOS | RES | BD | SD | HH | LANE |
| MIX³ | T1 LEVL | T2 | T3 | T4 | T5 | T6 | T7 | T8 |
| GLO | DCAY | DAMP | ADPC | DRIV | MUTE⁴ | – | – | – |
| PERFORM | DCAY | DAMP | ADPC | DRIV | – | – | – | – |

¹ DIGI-WAV's wave bank only. ² The tempo, as Shift + knob 1: Move's and MONO2K's under SYNC (turning it asks Move for it), the pattern's own (whole BPM) under INT. ³ A proposal for the owner: one knob a track, the eight levels (no locks: MIX edits the kits). ⁴ MUTES: a mute made while playing comes at the next bar, or at once (a setting, kept with the store).

SYN to LFO3, TRK's are the track's (m2k.h's Monomachine pages); TRK holds the arp (the FM-1's ARP). PAT is the pattern's time and the track's own (the FM-1's HOME 2 and SEQ: there is no SEQ pad), HOME the FM-1's HOME 1 and 2 on one page, GRID the pattern's Grids and the track's lane, GLO the bus (the FM-1's GLO > BUS) and the one setting that matters on the Move (GLO > PLAY's MUTE). The FM-1's MIDI settings have no use here (the Move's glue routes no MIDI out or in), so they have no page.

Gestures that depend on a modifier (Shift, Mute, Copy, Delete) need it held first. The DSP hears the step or pad at once, but ui.js sends the modifier, so pressing both within a few ms may lose the order.

## In time with Move

- **One Play.** Play goes to Move (`button_passthrough: [85]`), and Move sends its start and stop to MONO2K's `on_midi`. MONO2K feeds them, and Move's clock, into the core's own clock in (`m2k_midi_from`, Move's clock in place of USB's).
- **Steady.** Move's 24-ppqn pulses arrive once a block, up to a block (or two) late, so they are not fed as they come.
  - A phase follows Move's count of pulses. It corrects 1/32 of the error a pulse, plus an integrator for a tempo slightly off.
  - Each pulse goes to the core at the 16-sample block where it falls.
  - The core's period is pinned to `host->get_bpm()` (Move's own tempo, held steady by Schwung). So the tempo, swing, gates, LFOs and synced delays never wobble.
  - Without a host tempo, the pulses are measured (a least-squares line over 4 beats) and held.
- **No clock from Move** (no set, a setting): a Play press with no answer from Move within ~70 ms plays MONO2K on its own clock. The screen says **INT** (in the header, and beside the tempo) and shows the pattern's own tempo. Move's next start brings back **SYNC**.
- **Join:** following works while MONO2K is parked behind Move. Move playing when MONO2K opens: MONO2K joins at Move's next start.
- `clock_lag` (module.json `defaults`, samples, 64 by default) moves MONO2K's steps against Move's pulses, if the ear says they sit early or late on the device.

### The tempo from MONO2K (the owner: "it sets the Move's tempo too")

A module cannot set Move's tempo through any API (the plugin and JS APIs only read it). Schwung's one route is its Link sidecar: it reads `/data/UserData/schwung/desired-tempo` and gives that tempo to the Link session, which Move follows.

- **Turning the tempo under SYNC** (Shift + knob 1, PAT's knob 1, HOME's knob 3):
  - MONO2K moves at once. The core plays the tempo asked, its phase running free of Move's pulses (`m2k_move.c` `tq_ask`, `ck.free`).
  - The DSP names the tempo in its frame (its number and tenths). ui.js writes it as `120.5000\n` with `host_write_file`. The DSP touches no file.
  - **The writes:** the sidecar sees a change by the file's mtime, in whole seconds. So ui.js writes at least 1.05 s apart, at most once a second while the knob turns, and the last value once it rests 250 ms. Then it tells the DSP (`W n ok`).
  - **Move follows:** when Move's `get_bpm()` reaches the tempo (within 0.05 BPM), MONO2K follows Move again. Its phase goes back to Move's bar nearest it with a capped nudge (1/16 of a pulse a pulse, ~6 %). No jump, so no step glitch. In the bench, 120 → 130 re-locks in 1.9 s and ends within 12 samples of Move's pulses.
  - **Move does not follow** within 2.5 s of the write: no Link, Move->Schwung off, or another Link peer. MONO2K plays on alone at its tempo: **INT** in the header, a moment's **INT / MOVE: LINK OFF**. Stopped, there is nothing to keep: the hint says so and Move's tempo stays.
  - **Back to SYNC:** when Move's tempo next changes (its own knob, or Link), or with Delete + the tempo knob. Not at Move's next start: a tempo set must not be lost at the next Play when Move has no Link (the owner's); Move's Play still starts and stops MONO2K. A tempo change on Move is what the owner does to fix it, so MONO2K follows. The re-lock lands on Move's bar nearest MONO2K's phase (at most half a bar away), so a long run alone takes up to ~15 s to glide in (the bench: 5.6 s).
  - Under INT (Move sent no clock) the tempo knob is the pattern's own, whole BPM, as on the FM-1.
- **What the owner switches on:**
  1. **Move:** Link on, in Move's settings (`"isLinkEnabled": true` in `/data/UserData/settings/Settings.json`; Schwung warns "requires Link enabled in Move System Settings").
  2. **Schwung:** Global Settings → Audio → **Move->Schwung** (key `link_audio_routing`, saved in `/data/UserData/schwung/shadow_config.json`; off by default). Its monitor starts the sidecar `/data/UserData/schwung/link-subscriber` only while this is on, and `"link_audio_enabled": true` stands in `/data/UserData/schwung/config/features.json`. The installer writes that one true. Schwung Manager's web Settings → Audio → Move->Schwung is that features.json key, not the device's toggle: both must be on.
  3. **Move alone in the Link session:** with another peer (Live on the network) the sidecar skips the tempo.
- **What Move->Schwung costs** (Schwung faebd16, read in the source):
  - **CPU:** the `link-subscriber` process on cores 0–2 (SCHED_OTHER), awake every 10 ms. It is listed on Schwung Manager's System → CPU page; the source gives no figure for it. Move publishes its four tracks for it. Schwung measured that publisher breaking up under any sustained real-time load on cores 0–2 (`docs/plans/2026-08-27-osirus-linkaudio-breakup-HANDOFF.md`).
  - **Network:** Move's four tracks as Link Audio, UDP/IPv6 packets of 574 bytes (125 frames). That is ~353 a second a track, ~0.8 MB/s for the four, to the sidecar on the Move itself. Link also joins the network's session (Move's own Link does that already).
  - **The sound:** the shim drops Move's own mix and rebuilds it from the four track streams.
    - Move's tracks reach the output ~16–29 ms later than now. MONO2K, an overtake module, is mixed undelayed, so Move's tracks sit later than MONO2K's (Latency Comp delays Schwung's slots, not an overtake module). Check by ear; `clock_lag` moves MONO2K's steps.
    - Move's metronome is not in the rebuilt mix (Schwung's Metronome: Follow plays its own).
    - Move's speaker EQ is replaced by Schwung's emulation (Spkr EQ: Auto).
    - Move's tracks can drop out if the stream starves.
  - **Writes:** the sidecar keeps `/data/UserData/schwung/last-tempo` at every session tempo change.
- **Known edges:** `host_write_file` truncates, then writes. A sidecar read in between would see an empty file in the same second and skip the value. It is rare; MONO2K then falls back to INT after 2.5 s, and a nudge of the knob asks again. Move's own tempo resolution is not known here: if Move rounds the Link tempo coarser than 0.05 BPM, a tenth asked would read as not followed.

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

- **The design:** `move/screen/DESIGN.md` (calm: knob names on touch only, more black, every page one picture), drawn by `move/screen/m2k_screen.c`.
- **Page state:** once a panel frame (6 blocks, 17.4 ms), and 3 blocks after a press at the soonest, the controller fills a page state (`m2k_move_view.h`: the page, the knobs, the steps, the overlays, the LEDs, and the screen's own view), on the audio thread.
  - Its screen part is the renderer's **gather** (`m2k_screen_gather`): it reads the core (the pattern, the kits, the sequencer, the monitor) into plain numbers and strings, nothing that points into the core.
  - It is published by a sequence lock the drawer reads without ever making the audio thread wait.
- **Drawing:** the drawing thread (SCHED_OTHER, cores 0–2) draws each new state's screen with `m2k_screen_draw`, from its copy alone. The sheets check it: every still drawn again later, with the core elsewhere, is the same frame. Frames go through a triple buffer, swapped by pointer.
- **`get_param("frame")`:**
  - `M2K1`, then the frame's number (8 hex).
  - 64 rows of 22 characters: 6 pixels each from `'0'`.
  - 63 LEDs as two hex digits: 16 steps, 32 pads, 4 track buttons, then Rec Mute Undo Copy Delete Left Right Up Down Menu Loop.
  - How each knob turns, a character each (`MV_KK_*`: 0 a range, 1 a short list, 2 the tempo, 3 nothing), then the tempo asked of Move: its number (2 hex, 00 never) and its tenths (4 hex).
- **ui.js** reads one frame every two of the OLED's frames, **34.83 ms (~28.7 a second)**, on a fractional clock that never drifts and never catches up after a stall. It repaints only the rows that changed, as runs of `fill_rect`, and sends at most 16 LEDs a tick.
- **Why 28.7 a second** (Schwung faebd16, read in the source):
  - Overtake modules run in `src/shadow/shadow_ui.c`, whose loop ticks every **2 ms** for an overtake module (`overtake_mode >= 2`), and packs the display into shared memory whenever the JS drew.
  - The OLED is pulled: the XMOS asks for one of six 172-byte slices each SPI frame (128 samples, 2.902 ms), latched at slice 1 (`src/host/display_pull.h`). A whole panel frame is 17.4 ms: **57.4 a second at most**.
  - `host_set_refresh_rate` (the "~11 Hz default" of `docs/API.md`) exists only in the standalone host, `src/schwung_host.c`, where it sets `display_refresh_interval` and nothing reads it. `shadow_ui.c` does not define it. It is not a limit here.
  - A param read waits for the shim's next SPI frame, ~2.8 ms, and holds the UI tick (and the knobs' forwarding) meanwhile.
  - Every second panel frame shows each of ours for exactly two of the panel's (an even cadence), keeps the param channel ~8 % busy, and stays under the FM-1's 30 frames a second, where the owner felt a faster refresh overload the device. Every third (19 a second) is calmer still but steps a turned knob; every one (57) would keep the channel busy 16 % for frames no eye needs.
- **The cost a frame** (`bench time`, A04 playing, every page, real time in the container; `sheets.c` on the Mac, hot):

  | | The container, paced to real time | The Mac, hot loop |
  |---|---|---|
  | Publish (the audio thread: the controller's state, the LEDs, the screen's gather) | 11.8 µs mean | 0.3–0.6 µs (6–15 k instructions, GRID the most) |
  | Draw (the drawer thread) | 15.0 µs mean | 0.3–2.3 µs (10–55 k instructions, FILT the most) |
  | The render of a block, for scale | 161 µs mean (25 µs in a hot loop) | |

  The paced numbers are cold caches in the VM: the render itself costs 6× its hot-loop time there, and the maxima are the VM's preemptions (a block's own most was 8.8 ms). Either way a publish is a few percent of one block's render, once in 6 blocks. On the Move's A72, measure with `bench time` on the device (step 4).
- **The LEDs:**
  - **Steps:** trig azure, p-locked trig dimmer azure, playhead and held white, beats faint. With Shift held, the bank's patterns; with Mute held, the tracks.
  - **Pads:** the page pads in their families' colours (the page in view bright); the keyboard's white keys grey, black keys darker, the root orange, notes playing white; PERFORM's keys magenta (bank A) and pink (bank B), the effects playing white.
  - **Track buttons:** the track in view white, the others dim, muted ones dark red.
  - **Menu:** lit while a list is open (it closes it). **Up / Down:** the octave's side, or PERFORM's keys' page.
  - **Play's LED** stays Move's.
  - **The budget:** 63 LEDs, sent only when they change, at most 16 a 2 ms tick (Schwung's API: under ~60 a frame).

## Build, prove, deploy

```sh
sh move/build.sh        # dsp.so in an aarch64 Debian container, checked; packaged
sh move/proof.sh        # Mac vs container: self-tests (6 and 8 tracks), bank A bit for bit; dsp.so's benches; ui.js
sh move/deploy.sh       # to move.local over SSH (MOVE_HOST=user@host for another)
```

The screen's and the pads' design sheets (`move/screen/DESIGN.md`), drawn by dsp.so's own code (the plugin built for the Mac and driven through its API) into `build/move/sheets/`:

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
  | `rt` | 20 000 blocks of every control, Move's clock and saves asked for, the tempo turned and written, Move's tempo moving. Every allocation, file, thread, sleep and log call on the audio thread is counted: none. |
  | `ctl` | Every control (92 checks): the page pads and their lights, the keyboard's keys, PERFORM's keys, SOUNDS, the jog's patterns' list, MIX's knobs, the knobs' kinds for ui.js, the tempo (asked, followed, not followed, landing on whole BPM, Delete, Shift + knob 1 on all 16 page pads, INT's whole BPM). |
  | `clock` | Synthetic Move clocks, each pulse seen at the next block boundary, 1 % of frames a block late: 120 BPM, a tempo change, stop, continue, start, stale, no host tempo, no clock, Play answered; the tempo turned in MONO2K (Move following 0.55 s on; not following: alone, then re-synced by Move's tempo; Move's restart; Delete): every two steps timed, no glitch, back within 32 samples of Move's pulses. |
  | `save` | The round trip. |
  | `draw` | The screens as PBM, in `build/move/proof/screens/`. |
  | `time` | The render's cost; the screen's a frame (publish, draw). |

- **Output:** the module in `build/move/modules/overtake/mono2k/` (`dsp.so`, `module.json`, `ui.js`), and `build/move/mono2k-move.tar.gz`, with `mono2k/` at its top (Schwung Manager: Install Custom Module).
- **deploy.sh** copies to `/data/UserData/schwung/.mono2k-incoming`, then renames it to `modules/overtake/mono2k`. Close an open MONO2K first (Shift + Back): a parked one keeps the old `dsp.so`.
- **Opening it:** MONO2K is in Schwung's Tools menu, under Overtake Modules. If it was open, close it first with Shift + Back: a parked MONO2K keeps the old `dsp.so` loaded.

## How it fits

- **`m2k_move.c`** is Schwung's side: the plugin API, the saver, the drawer, the frames. It includes `m2k_core.c` (as the browser's glue does), then **`m2k_move_glue.c`**, the part every build of MONO2K-on-the-Move shares: the queue, the clock follower and the tempo asked of Move, the block (`mv_block`), `m2k_move_ui.c` (the controller: it calls the FM-1 UI's own helpers in `m2k_ui.c`) and `screen/m2k_screen.c` (the renderer).
  - One instance: the core is a set of statics.
  - One core: `m2k_par_begin` stays unset.
  - The site's playable Move (`web/move/gear.js`) runs the same glue in the browser (`web/move/m2k_move_web.c`, built by `web/build.sh` into `web/move/m2k_move.wasm`): on its own clock (INT), nothing saved.
- **The audio thread** runs render, on_midi, set_param and get_param. Nothing there allocates, locks, logs or touches a file.
  - Pads, steps and Move's transport (on_midi), and ui.js's controls (`set_param("in", "K i d;P i d c;J d;B cc v;T i v;W n ok")`: K a knob's steps, P the tempo in tenths (c: whole BPM), W the tempo written), go into one queue. The next render drains it in order.
  - The render runs the core 16 samples at a time, with Move's pulses between.
- **create / destroy** run on Schwung's loader thread. They load the store, start and join the two threads, and save on closing.
- **Other params:**
  - `get_param`: `"ui"` (step 1's line), `"view"`, `"dump"`, `"clock"` and `"cost"` (text for the bench).
  - `set_param`: `"play"` and `"track"` (the bench's and the tools').
- **module.json `defaults`**, all optional: `seed`, `pattern`, `data_dir` (`""`: no store), `save_on_close`, `draw_thread` (0: draw in the render, every other publish), `clock_lag`.
