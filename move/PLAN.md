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
  └─ 128x64 1-bit screen renderer in C, on a worker                (new, m2k_ui_move.c)
                       │ one bulk read per tick: 1 KB page + ~100 values
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
| 16 steps | The trig keys: steps, p-locks held, bar view. |
| 8 knobs + touch | One page of 8 parameters, as on the Monomachine. A touch shows the value. |
| 32 pads | The keyboard and the arp (velocity and aftertouch). Shift+pads opens PERFORM. |
| Jog | Pages, the catalogue and the library. A click enters. |
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
- **Builds go on the Move over Wi-Fi** (the SSH key from Schwung's installer, `move.local`): a copy to a temp name, then a rename.

## Open questions

- **Name:** the catalog already has "Mono" (timncox/schwung-mono, MIT), a six-track Elektron-style machine synth. MONO2K keeps its full name. Read its code only for its API use, never copy it.
- **Move's own audio under MONO2K:** muted (`host_mute_move_audio`) or mixed?
- **Clock:** run MONO2K's own clock, or follow Move's tempo and play (`move_info`, MIDI clock in)?

## Risks

- **Move OS updates:** expect a Schwung reinstall after each one.
- **Stability:** Schwung calls itself experimental. Guard every newer call (bulk params, beat position).
- **Screen channel:** each read is about 2.8 ms. If one bulk read a tick isn't enough, ask upstream for a `display_blit`, or go standalone (the Movy route, where C owns the screen).
- **CPU:** core 3 has about 2.0 ms usable per 2.9 ms block, shared with Schwung's slots.
