# MONO2K on the Ableton Move: the plan

These are the owner's decisions from 2026-10-31:
- **Shape:** a full takeover. MONO2K owns the pads, the steps, the knobs and the screen.
- **Device:** the owner's Move, with Schwung installed.
- **Core:** shared with the FM-1, so the Move plays the same samples and the sound stays the same.

The route is Schwung (github.com/charlesvestal/schwung, MIT, unofficial). The research was read at Schwung commit faebd16, release v1.8.2, and checked against Move OS 2.1.0 and 2.1.1.

## The shape

```
Move hardware ─┬─ pads, steps ───────────────► dsp.so on_midi (audio thread, sample-exact)
               └─ knobs, jog, buttons, AT ──► ui.js ──bulk set──► dsp.so

dsp.so (core 3, 128-frame blocks = 8 x M2K_SUB)
  ├─ firmware/src core: engines, sequencer, arps, PERFORM, bus     (unchanged code)
  ├─ m2k_par_* on real-time workers, cores 0-2                     (the FM-1's 2nd core, x3)
  └─ 128x64 1-bit screen: gathered on the audio thread, drawn on a worker   (screen/m2k_screen.c)
                       │ one bulk read every 2 panel frames (34.8 ms): 1 KB page + the LEDs
                       ▼
ui.js: paints the page, sets the LEDs, forwards input. No logic.
```

Saving runs on a saver thread: it writes a temp file, then renames it.

The files live in `move/`, a fourth platform beside `fm1/`, `web/` and `host/`:
- `m2k_move.c`: the Schwung plugin v2;
- `ui.js`;
- `module.json`: overtake, with `suspend_keeps_js`;
- `build.sh`: Docker `debian:bookworm` with `gcc-aarch64-linux-gnu`, glibc 2.35 or lower.

## The controls

| Move | MONO2K |
|---|---|
| 16 steps | The sequencer on every page: steps, p-locks held, bar view. |
| 8 knobs + touch | One page of 8 parameters, as on the Monomachine. A touch shows the name and the value. |
| 32 pads | The top two rows the pages (a pad each) and SOUNDS and PERFORM; the bottom two the keyboard (a two-row piano), PERFORM's effects while it is on. |
| Jog | The lists: the patterns, the catalogue. A click loads or keeps. |
| Track buttons (4) + Shift | Tracks 1–4. With Shift, 5–8. |
| Play / Rec / Mute / Copy / Delete / Undo | Their MONO2K jobs. |
| Master knob | Move's volume (kept by the host). |
| Shift + volume touch + jog click | Back to Move (reserved, always works). |

## The steps

1. **First sound.**
   - `dsp.so` plays bank A on the Move, and Play starts and stops it.
   - The steps light the playhead.
   - The screen shows only "MONO2K".
   - **Proof:** the host build on aarch64 matches the FM-1 hash bit for bit, and the device's CPU per block is measured.
2. **Hands on.**
   - Steps, knobs, pads, jog, the track buttons and the LEDs.
   - Patterns save and load.
   - The owner plays it.
3. **The screen.**
   - Every page is redrawn for 128x64 1-bit: the owner's pixel font at x1, and Ableton-style thin line icons drawn as MONO2K's own.
   - PNG sheets for the owner first, then the device.
4. **The power.**
   - The workers move to cores 0–2.
   - Move build flags open the limits: more tracks, voices and delay time, patterns kept unpacked, and no CPU FULL.
   - The FM-1 build stays as it is.
5. **Audit and evolve.**
   - The owner's goals, one proposal at a time, each with renders to listen to:
     - the engines;
     - the effects;
     - PERFORM;
     - the sequencer's workflows.
   - Each is an addition, not a change to approved sounds. The suite (819 comparisons) guards both devices.
6. **Release.**
   - A GitHub release `mono2k-move-X.tar.gz` and a PR to Schwung's `module-catalog.json`.
   - Licence GPL-3.0-only (the catalog already lists GPL-3.0 modules).

## Decided since

- **8 tracks on the Move** (owner, 2026-10-31). `M2K_TRACKS` becomes a build option; the FM-1 stays at 6. A 6-track pattern loads on the Move with tracks 7–8 empty, and the FM-1 takes the first 6 tracks of an 8-track pattern.
- **Open source** (owner, 2026-11-01): the Move version is GPL-3.0-only like the FM-1's, with Grids and the DRM- drums as they are, in the same public repo.
- **In time with the Move** (owner): Move's tempo, one Play starts both.
- **The pads** (owner): every page one pad press away (the top two rows), the keyboard the bottom two, the steps the sequencer on every page; the jog scrolls lists, never pages. MIX kept (knobs 1–8 the tracks' volumes); PERFORM's 16 main effects on the keyboard rows, Up swaps in WOW DRIVE KILL DUB SHIMMER SPIN.
- **The screen** (owner): calmer; the knob names only on touch.
- **Builds go on the Move over Wi-Fi** (the SSH key from Schwung's installer, `move.local`): a copy to a temp name, then a rename.
- **In time with Move** (owner, 2026-11-01): MONO2K takes Move's tempo and transport, one Play starts both, and Move's own tracks play along. Built in step 2 (README, In time with Move): Move's pulses through the core's own clock in, a gentle phase lock, the period pinned to Move's tempo. MONO2K plays alone (INT) only when Move sends no clock.
- **Step 2, hands on** (2026-11-01): the controls, the screen's page state with a plain renderer, saving to `/data/UserData/schwung/mono2k/mono2k.m2ks`, the LEDs. The table is in README.md.
- **The screen, calmer; the pads** (owner, 2026-11-01, on step 3's sheets): "names on touch only", "good but calmer"; "use the 2 rows pads of move for pages and functions too", "to avoid eternal pagination on the main wheel knob"; "sequencer is always available as there is a dedicated row in the bottom". Built: every page a pad (the top row the sound, the second the rest and SOUNDS and PERFORM), the jog for lists only, the keyboard a two-row piano, PERFORM's effects on it while on, the knob names gone from the screen until touched, dsp.so drawing step 3's renderer through the controller's page state, 28.7 frames a second (README, The screen). MIX (the eight levels) is a proposal for the owner.

- **The tempo** (owner, 2026-11-01): "It sets the Move's tempo too": turning BPM in MONO2K changes both, MONO2K at once, Move catching up; "Shift + knob 1, any page", the BPM shown large. Built: the DSP plays the tempo asked at once and names it in its frame. ui.js writes Schwung's `desired-tempo` (at most once a second, the last value at rest). MONO2K follows Move again when Move reaches it, re-locking on Move's bar without a jump. Not followed in 2.5 s: MONO2K alone (INT, MOVE: LINK OFF) until Move's tempo next changes, Move's next start, or Delete + the tempo knob. Needs Link on Move and Schwung's Global Settings → Audio → Move->Schwung (README, The tempo from MONO2K).
- **PADS and Capture** (owner, 2026-11-02; the Move's alone, the FM-1 stays lighter): the **Sample** button (CC 118) turns the keyboard rows into PADS, the 8 tracks at their roots on the bottom row and their mutes over them (REC records each hit into its own track; PERFORM still takes the rows); **Capture** (CC 52) writes what was just played on the keyboard into the steps in whole bars ("quantized by bars?": yes), each note on the bar and step it fell on while playing, the phrase from bar 1's downbeat while stopped, overdubbed, one Undo (README, The controls).
- **The knobs' feel** (owner, 2026-11-01: "the knobs are too sensitive"): MONO2K's own curve in ui.js instead of Schwung's ×10 acceleration. Half a step a detent up to a brisk turn, 2.5 at most on a flick; short lists 3.5 detents an option; the tempo 0.1 BPM slow, whole BPM fast. 0–127 in ~1.2 turns at a brisk turn (README, The knobs' feel).

- **A held step's card** (owner, 2026-11-07: "yes", on the screen saying nothing of a step's condition, ratchet or note while they were set): the stage gives way to a step held past a tap, or the moment it is edited: the step, its condition, note and ratchet at x2, each over the gesture that turns it. A tap stays quiet; a knob touched keeps the stage (`d_held`, README, The controls).
- **A note's length** (owner, 2026-11-07: "like opxy holding the step and tap in another step"): built on the step's own locks, GATE and HOLD 127, so no format change; the FM-1's GRID has it too (owner, the same day), one code in m2k_ui.c. Measured through the gesture on a plucky sound at 120 BPM: full level for 1.97 steps at 2, 2.97 at 3 ... 6.97 at 7, 7.89 at 8. The first step must be held past a tap, so two fingers still place two trigs (README, The controls).
- **The jog's two jobs** (owner, 2026-11-07: "the jog wheel is far from the shift btn", "make better the gesture and easier"): holding a step, Shift and the jog at once takes more reach than two hands have, so a jog click with a step held turns the jog from the note to the condition, one hand holding and one on the jog. It stays there for the next step held, so a run of chances takes one click, and the card tags JOG under what it sets. Nothing else needed the click: with a step held it only opened the patterns' list. Shift + jog stays. The Move's alone: the FM-1 has PRESETS. Played on the owner's Move the same day: "much better".

## Open questions

- **The pads:** MIX on the freed pad (a proposal); PERFORM's 22 effects on the keyboard's 16 keys, 8 + 8 at a time (Up shows each bank's 9–11); the jog click, free outside the lists, opens the patterns' list; Menu closes a list; the FM-1's MIDI settings have no page (the Move routes no MIDI to them).

- **Name:** the catalog already has "Mono" (timncox/schwung-mono, MIT), a six-track Elektron-style machine synth. MONO2K keeps its full name. Read its code only for its API use, never copy it.
- **Move's own audio under MONO2K:** mixed for now (ui.js `MUTE_MOVE`), muting is one flag.
- **Move's tempo from MONO2K:** built (below, Decided since). Still to hear on the device: whether Move takes tenths of BPM through Link, and how late Move's tracks sit once Move->Schwung rebuilds them (~16–29 ms against MONO2K: `clock_lag`).
- **Aftertouch:** the core takes none. It could drive a parameter (a filter, an LFO's depth) if the owner wants one.
- **Saving on close:** on (`save_on_close`), unlike the FM-1, which keeps only what SAVE kept.
- **Notes over 8 steps:** GATE stops at 127 (7.9 steps). Longer needs a length of its own in the step, a new pattern format the FM-1 shares; the gesture would stay the same.
- **FILL:** the Move has no gesture for it (the FM-1's GLO + white 7), so a step on FILL never plays and one on !FIL always does. A button if the owner wants one.
- **The machine changed:** Shift + jog puts the new machine's A–H on the track and takes no undo, so a machine passed through loses the track's A–H edits (the FM-1's ALGO does the same).
- **The clock's lag:** 64 samples (half a block) matches the model. Check by ear against Move's own tracks on the device (`clock_lag`).

## Risks

- **Move OS updates:** expect a Schwung reinstall after each one.
- **Stability:** Schwung calls itself experimental. Guard every newer call (bulk params, beat position).
- **Screen channel:** each read is about 2.8 ms. If one bulk read a tick isn't enough, ask upstream for a `display_blit`, or go standalone (the Movy route, where C owns the screen).
- **CPU:** core 3 has about 2.0 ms usable per 2.9 ms block, shared with Schwung's slots.
