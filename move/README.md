# MONO2K on the Move

The core (`firmware/src`), unchanged, as a [Schwung](https://github.com/charlesvestal/schwung) overtake module: it owns the pads, the steps, the buttons and the screen, and plays the same samples as the FM-1 and the browser. The owner's plan is `move/PLAN.md`.

## Step 1: first sound

- **Power-on:** the factory's bank A at A01, stopped, as the FM-1 powers on. Nothing is saved yet.
- **Play** starts and stops.
- **Track buttons** pick tracks 1–4; with Shift, tracks 5–6 (5–8 once there are 8).
- **Steps:** a press toggles that step's trig on the track in view. While playing, the page is the playhead's; stopped, it stays where it was.
- **Pads** play the track in view as the FM-1's keys do: held, legato, the track's arp. One semitone a pad, from F3 at the track's octave.
- **Screen:** MONO2K, the track, the pattern, BPM, PLAY or STOP, the 16 steps and the playhead. Hold Shift to see what a block costs (2902 µs of sound).
- **Step LEDs:** the playhead white, trigs blue, beats faint.
- **Leaving:** Back parks MONO2K, and it plays on behind Move. Shift + Back closes it. Shift + touching the volume knob + jog click always escapes (Schwung's).

## Build, prove, deploy

```sh
sh move/build.sh        # dsp.so in an aarch64 Debian container, checked; packaged
sh move/proof.sh        # Mac vs container: self-test, bank A bit for bit; dsp.so's hashes, rules, time a block
node move/test/ui_test.mjs
sh move/deploy.sh       # to move.local over SSH (MOVE_HOST=user@host for another)
```

The screen's design sheets (step 3, `move/screen/DESIGN.md`), drawn by the renderer from real states into `build/move/sheets/`:

```sh
cc -O2 -DM2K_TRACKS=8 -Ifirmware/src -Ifirmware/gen -o build/move/screen_sheets move/screen/sheets.c && build/move/screen_sheets build/move/sheets && .venv/bin/python move/screen/sheets.py build/move/sheets
```

- **Build output:** the module in `build/move/modules/overtake/mono2k/` (`dsp.so`, `module.json`, `ui.js`), and `build/move/mono2k-move.tar.gz`, with `mono2k/` at its top.
- **The container:** Docker on aarch64 (colima). The first build makes the image `mono2k-move-build:bookworm`: debian:bookworm-slim arm64 with gcc 12.
- **What build.sh checks:** every glibc symbol is at or under 2.35, the Move's. Only `move_plugin_init_v2` is exported. Nothing that allocates, locks, logs or does I/O is imported.
- **The tarball** can also be installed from Schwung Manager (port 7700): Install Custom Module, then the file. It unpacks into `modules/overtake/`.
- **deploy.sh** copies the module to `/data/UserData/schwung/.mono2k-incoming`, outside the module scan, then renames it to `modules/overtake/mono2k`. An old `dsp.so` is renamed away, never written over.
- **Opening it:** MONO2K is in Schwung's Tools menu, under Overtake Modules. If it was open, close it first with Shift + Back: a parked MONO2K keeps the old `dsp.so` loaded.

## How it fits

- **`m2k_move.c`** includes `m2k_core.c`, as the browser's glue does.
  - Schwung's plugin API v2. The header is vendored, unchanged, in `vendor/schwung/` (MIT, from Schwung faebd16, v1.8.2).
  - One instance: the core is a set of statics.
  - One core: `m2k_par_begin` stays unset, so the split never runs.
- **The audio thread** (core 3) runs render, on_midi, set_param and get_param. Nothing there allocates, locks, logs or calls libc beyond memset, memcpy and the vDSO's clock_gettime (the CPU meter).
  - Pad and step notes go into a lock-free ring, which the next render applies.
  - `play` and `track` are atomics, also taken by the next render.
  - Each render publishes what `ui.js` reads.
  - create_instance runs on Schwung's loader thread. It only runs `m2k_init`, about 0.3 ms.
- **`get_param("ui")`** returns one line: `PLAY STEP BPM TRACK TRACKS PAGE LEN TRIGS CPU_MEAN CPU_MOST SLOT NAME`.
  - STEP is the track's step from 0, or -1 when stopped.
  - TRIGS is the page's 16 trigs in hex.
  - CPU is in µs a block, over the last 128 blocks.
  - `ui.js` reads it with one bulk get at most every 34 ms, and sends with bulk sets.
- **module.json `defaults`**, both optional, as Schwung hands them to create:
  - `"seed"` fixes the dice; the tests use 2000, else the clock's.
  - `"pattern"` sets the pattern opened.
- **`tool_config`** is only for `component_type: "tool"`. An overtake module is listed by `component_type` alone (shadow_ui.js `scanForOvertakeModules`), so module.json has none.
- **No licence headers** in `move/` yet: the owner decides the Move version's licence.
