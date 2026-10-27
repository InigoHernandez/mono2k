# The MONO2K handbook

What was built, how, and what was learned, written so the next firmware (four tracks, a pure synth for lofi, on
your own waveforms and samples, with a workflow and a look of its own) can start from what works and skip what
cost time. Facts are as of 2026-10-23. Where a number is an estimate it says so; check it on the FM-1 before you
depend on it.

**Contents**

1. [What MONO2K is](#1-what-mono2k-is)
2. [The hardware: the M-VAVE FM-1](#2-the-hardware-the-m-vave-fm-1)
3. [The platform and the licence](#3-the-platform-and-the-licence)
4. [Toolchain, package, install](#4-toolchain-package-install)
5. [One core, three homes](#5-one-core-three-homes)
6. [Integer DSP: the rules and the parts](#6-integer-dsp-the-rules-and-the-parts)
7. [The sequencer](#7-the-sequencer)
8. [Data: patterns, formats, the store](#8-data-patterns-formats-the-store)
9. [The FM-1 link (SysEx)](#9-the-fm-1-link-sysex)
10. [The screen](#10-the-screen)
11. [The workflow: what we arrived at, and why](#11-the-workflow-what-we-arrived-at-and-why)
12. [The website](#12-the-website)
13. [Testing and measuring](#13-testing-and-measuring)
14. [How the work was run](#14-how-the-work-was-run)
15. [Starting the next firmware](#15-starting-the-next-firmware)
16. [Appendix: commands and files](#16-appendix-commands-and-files)

---

## 1. What MONO2K is

A Monomachine-style firmware for the M-VAVE FM-1: six mono tracks, each one "machine" (sine, noise, chip, three
FMs, supersaw, pulse, chord, digital waves, a voice, three FX machines), a 64-step sequencer with parameter locks,
trig conditions, ratchets and trig types, a lofi bus (SPU-style reverb, beat repeat, tape, ADPCM crunch, hiss,
drive), and a 240 x 240 screen drawn in one bit. The same C core runs on the FM-1, in the browser (WebAssembly in
an AudioWorklet) and on a computer (tests, renders, screenshots).

Repo: `~/Documents/Music/Git/mono2k`, public at github.com/InigoHernandez/mono2k. Site: inigohernandez.github.io/mono2k
(GitHub Pages serves `web/`).

---

## 2. The hardware: the M-VAVE FM-1

| Part | What there is | What it meant for MONO2K |
| --- | --- | --- |
| Chip | JieLi AC791N: two 32-bit pi32v2 cores, **no FPU** | Integer DSP only; the build links **no libgcc**, so no float and no 64-bit division (a 64-bit multiply is fine) |
| RAM | 96 KiB for `.data + .bss`, 336 KiB "pool" (Felucca's linker section) | Big buffers (delay lines, the pattern image, the sound library) go to `POOL`; MONO2K uses 91,948 B of RAM and 332,452 B of pool (the second core's 8 KiB of stacks come out of the pool's room: a 3.5 KB buffer had to move to RAM) |
| Flash | an app slot of 581,564 B (`0x8DFBC`) for code, tables and waves; a data area at `0x93000..0xE0000`; OTA staging at `0xE0000` | MONO2K's image is 500,880 B. Its store is the last 128 KiB of the data area, `0xC0000..0xE0000`, which other firmwares (Felucca) also use for their projects and samples |
| Screen | 240 x 240 colour LCD over SPI at 12 MHz (RGB565) | A whole frame is ~77 ms of SPI: send only the 8-line strips that changed, one strip per pass while the audio renders |
| Keys | 27: F3..G5, 16 lower ("white") and 11 upper ("black"); analog sensing exists (fm1-velocity) | MONO2K plays every note alike (no velocity yet) |
| Buttons | 14: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT− OCT+ | Every combination in the workflow is built from these |
| Encoders | SELECT, ALGORITHM, PRESETS, KNOB 1..4 (endless) | Four knobs on top of the screen: the knob row of the screen sits right under them |
| MASTER | a **pot**, not an encoder | It is the output level; it can drift, so it never counts as "touched" |
| LEDs | one per key and per button, **on or off** | The core sends 0..255; the FM-1 lights from 64. Design the LED language for on/off and blinking, brightness is a bonus on the web |
| Audio | 44.1 kHz stereo out; USB-MIDI and TRS MIDI in | |
| Rescue | hold OCT− at power-on for the USB rescue; two failed boots also go there | Any firmware can be replaced from the site, whatever it does |

---

## 3. The platform and the licence

MONO2K's `firmware/fm1/` is the open FM-1 platform of **CS-1** (twochannels), which stands on **Felucca** (Leo
Kuroshita, Hügelton), **SLOOP** (isod89), **Dinghy** (zvenson), **FM-106** (PatttF, the second core as an audio
worker) and **fm1-velocity** (Charles Vestal, the key sensing). It gives you the boot, the update entry, the USB
rescue, the boot guard, the panel scan, the LCD, MIDI in, and the second core.

- **It is GPL-3.0-only, so anything built on it is too.** Keep the headers, keep `LICENSES/`, credit every project.
- MONO2K changed three things in it: the app can set `BANK_AT` / `BANK_LEN` (where its store goes); an app SysEx hook
  (`APP_SYSEX`) sees each frame before the update entry; the LCD transfer's end is not waited for.
- The JieLi AC79 SDK's `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` go into the package at build time, read from
  the SDK (Apache-2.0); they are not in the tree.
- `m2k_fm1.c` is MONO2K's glue: the core in the main loop, the audio ISR copying a 17 ms ring (six blocks of 128
  frames), the screen strips, the LED mapping, the store in flash. `m2k_link.c` is the store's stream and the link.

Other open FM-1 firmwares worth reading for ideas (not code to copy unless the licence allows): Felucca, SLOOP,
Dinghy, FM-106, CS-1, Hortator (`~/Documents/Music/Git/hortator-drum-v0.23.2`), ghoulbox
(`~/Documents/Music/Git/ghoulbox-fm1-dungeon-synth-ghoulbox`), MNM-1, and the awesome-fm-1 list.

**Marks and clean room.** No Elektron or M-VAVE marks in a product name. MONO2K never copied, decompiled or
disassembled Elektron's OS: the Monomachine was a black box played over MIDI in gearmulator and measured. The same
rule applies to any instrument you emulate. The LXR's code is GPL-3.0 (compatible), but its trademark terms forbid
"LXR" in a product name, and its transient samples have no stated licence: don't copy them.

---

## 4. Toolchain, package, install

```bash
.venv/bin/python tools/fm1/build.py                 # build/fm1/mono2k.fwsc, identity FM-1_980
.venv/bin/python tools/fm1/build.py --diag          # a CPU meter along the screen's top
.venv/bin/python tools/fm1/build.py --release 0.1   # mono2k-0.1.fwsc, version 0.1, identity FM-1_980
```

- **Compiler:** JieLi's clang 4.0.1 for pi32v2 (`~/.jieli/toolchain`), Linux only, so on a Mac it runs in a
  linux/amd64 container: colima with Rosetta (`colima start --vm-type vz --vz-rosetta`), `DOCKER_CONFIG` in
  `~/.jieli/docker`. The AC79 SDK sits in `~/fw-AC79_AIoT_SDK`.
- **The core is its own unit** (`core.o`), reached only through `m2k.h`. Build settings come in as `-D` flags from
  `build.py`: `M2K_FW=1`, `M2K_PATTERNS=64`, `M2K_PAT_SLOTS=2`, `M2K_PROJECTS=1`, `M2K_IMG_DATA=47104`.
- **build.py checks** (after Felucca and CS-1): the entry at the XIP address, no RAM code calling out of RAM, no
  reference to the chip's ROM, no float or 64-bit division helper, the image within the app slot, `.data + .bss`
  within 96 KiB, the pool within 336 KiB.
- **Package:** `.fwsc` made by Felucca's `fm1pkg_make.py` and `lz4blk.py`, unchanged.
- **Identity numbers:** each firmware family has its own range (Felucca 9XY, CoralFlower 930, CS-1 95N, zp12 97N,
  MONO2K 98N, Dinghy 99N). **Pick a free range for the new firmware and write it in build.py.**
- **Install:** the site's installer page (Felucca's `web/fm1/fm1ota.js`, `fm1pkg.js`), Chrome or Edge over USB.
  It also says what the FM-1 runs and can take it back to M-VAVE's firmware.
- **Never publish a package on the site that has not run on an FM-1.**
- Auto-run tools may refuse to run other repos' build scripts: write your own (MONO2K's `build.py` follows CS-1's).

---

## 5. One core, three homes

```
firmware/src/      the core: C99, a unity build (m2k_core.c includes the rest), static everything, integer only
firmware/gen/      generated headers: tables, fonts, marks, waves (never edited by hand)
firmware/fm1/      the platform + MONO2K's glue (m2k_fm1.c, m2k_link.c)
web/m2k_web.c      the core + w_* exports for the browser
host/m2k_host.c    tests, renders, screenshots, the text audit
```

**The API** (`m2k.h`) is small and the same everywhere:

```c
void m2k_init(uint32_t seed);
void m2k_render(int16_t *lr, int frames);        /* interleaved stereo, a multiple of 16 */
void m2k_key(int k, int down);  void m2k_button(int b, int down);  void m2k_encoder(int e, int delta);
void m2k_midi(int status, int d1, int d2);      /* as from USB; m2k_midi_from(src, ...) names USB or TRS */
const uint8_t *m2k_frame(uint32_t ms);           /* 240 x 240, 1 bit, MSB first, 1 = ink */
const uint8_t *m2k_leds(void);                   /* 27 keys then 14 buttons, 0..255 */
/* store: m2k_store_buf / _pack / _unpack / _pack_as; projects; MIDI out with a sample stamp */
```

- **FM-1:** the main loop renders ahead into a ring; the ISR only copies it out, so no interrupt lands mid-edit. A
  pass renders at most half the ring: a slow core stutters but never starves the panel or the watchdog.
- **Two cores:** each 16-sample block the core hands tracks `[split, 6)` to the second core (`m2k_par_begin` /
  `m2k_par_end`), renders the rest, then the bus. Every FX machine stays with the track it hears. The self-test
  renders bank A with the second core's tracks first and last and holds both to the one-core render, sample for
  sample. The second core's loop (CS-1's audio worker) runs from RAM with the flash's XIP off: its code goes in
  `.ram_text` (24 KiB of RAM code in all), it may call nothing (its helpers inline), and it has its own 8 KiB of
  stacks; build.py checks all three.
- **Browser:** `web/build.sh` builds `m2k.wasm` with Emscripten, `STANDALONE_WASM`, no JS glue; `worklet.js`
  instantiates it in the AudioWorklet and posts the screen, LEDs and state to the page. Same sample rate (44.1 kHz,
  resampled by the browser otherwise).
- **Host:** the same core plus test code: `--selftest`, tours with screenshots (PBM), renders (WAV), the text audit,
  the store and link round trips.

**Why it pays:** every change is tried in the browser and on the host within seconds; the FM-1 only confirms. Bugs
in the UI were found by the host tour, sound bugs by the self-test and measurements, long before a flash.

**The CPU budget:** count instructions as the FM-1 builds the core (`cc -O2 -fno-builtin -fno-vectorize
-fno-slp-vectorize`, `proc_pid_rusage` `ri_instructions` on a Mac). Felucca's heaviest patch runs ~1570 Mac
instructions a sample on one core: a usable yardstick. Byte loops (`m_set`, `m_cpy`) are word-wise for it. A `--diag`
package shows the real load on the FM-1's screen.

---

## 6. Integer DSP: the rules and the parts

**Rules**
- No floats, no 64-bit division in `firmware/src`. 32-bit division is allowed but keep it out of per-sample loops.
- Anything that needs a float (exp, log, tan, curves, frequency laws) is a table from `tools/gen_tables.py`.
- Saturate, never wrap (`sat16`). Watch left shifts of negative values (UBSan catches them; use `* 128`).
- Big buffers in `POOL`.

**Building blocks that worked (in `m2k_core.c`, `m2k_dsp.c`, `m2k_fx.c`)**

| Part | How |
| --- | --- |
| Phase | `uint32_t`, one cycle = 2^32 |
| Pitch | 1/64 semitone units; `inc_of(pq)` from a top-octave table (`T_INC_TOP`, 769 entries) shifted per octave |
| Sine | 1024 + guard samples, linear interpolation on 16 bits of fraction (`sinp`) |
| Saw, pulse | PolyBLEP (`blep_saw`, `inv = 0xFFFFFFFF / inc` once a block); a pulse is a saw minus a shifted saw |
| Noise | xorshift32 (`noise_step`); sample-and-hold for "tuned" noise |
| Filters | a zero-delay-feedback SVF in fixed point (`zsvf_t`, Q30 coefficients, remainders kept), one-poles (`zop_t`); cutoffs from tables indexed in 1/96 octave over MIDI note 0, so the filter follows the note |
| Envelopes | per control block of 16 samples (0.36 ms), in Q30; the amp ramp interpolated per sample across the block; times from tables |
| Delay | 8-bit mu-law at half rate (16 KB a track = 0.74 s); silent lines skipped |
| Reverb | PS1 SPU-style at 22 kHz |
| Lofi bus | beat repeat, tape wow and flutter, 4-bit ADPCM, hiss and dust, a soft-clip drive; every control on a law measured over its whole range |
| Waves | the owner's 64 single cycles, 256 samples each, band-limited by `tools/gen_waves.py` (see 15.4) |

**Lessons that cost time**
- **Test with the machine's own signal, not only a sine.** A sine test passed while a driven saw came out 10.7 dB too
  loud (each machine needs its own distortion ceiling).
- Read a level with a **flat-top window**: a Hanning's scalloping read two close carriers 1 dB apart.
- A track's order in the render matters when one track hears another (FX machines).
- Keep the control rate fine (16 samples): fast pitch sweeps at a coarse rate step audibly.
- Put a "headroom" shift between a loud machine and the chain, and take it back after (GND-NOIS, FX-RING).

---

## 7. The sequencer

- 6 tracks x 64 steps; each step a note, flags (trig, accent, slide, three trig-type bits), a condition, a ratchet.
- 384 parameter locks a pattern, as a pool of (track, step, param, value), with a lock map rebuilt on change.
- Trig types after the Monomachine: each trig restarts the amp, the filter envelope and the LFOs on their own
  (ALL ... NONE; NONE is a trigless lock).
- Conditions after modern Elektron: chances, A:B, 1ST, PRE, FILL and their opposites; ratchets 1..4.
- Swing, accent amount, per-track speed (1/8x .. 2x, triplet speeds included; pattern format 15), patterns queued at
  the bar's end, "jump now", fill held, fill on the next loop.
- Live recording (to the nearest step), step entry (stopped: keys write, EDIT a rest, PRESETS the cursor).
- MIDI clock in and out, notes out a channel per track, the Monomachine's CC map. `m2k_midi_out(msg, played)` hands
  out what is due once the sound has reached sample `played`, so timing matches what you hear. USB and the TRS jack
  kept apart (the platform marks the jack's as cable 1): the clock from SYNC IN's (OFF USB TRS BOTH), notes and CCs
  from NOTE IN's, and a thru of the jack's out over USB.

---

## 8. Data: patterns, formats, the store

**A pattern** holds everything: six machines, six kits (every parameter of every page), the steps, the locks, the
globals (tempo, swing, length, the bus), a name. A typical one packs to 0.5–1 KB; a blank one to 0 bytes.

**Versioned formats with converters.** The pattern format is at version 15. Each change that moves a parameter's
meaning bumps it and adds a converter (`fm_v9`, `sid_v10`, `gsin_v11`, `hold_v12`, `vox_v13`, ...): old patterns
are converted on load, never broken. New machines go last so stored machine numbers stay valid.
`tools/ref/mutate_formats.py` mutates each converter and requires the self-test to catch every mutant: it found
tests that had silently stopped testing anything. (A mutation site that moved in the code now fails the run.)

**The pattern cache (FM-1).** 64 patterns don't fit RAM unpacked. `PAT(i)` is pattern i everywhere; on the FM-1 it
is a cache of two unpacked slots over a packed working image (46 KB) beside the saved one. Rules: reach patterns only
through `PAT()`, never hold two `PAT()` pointers to patterns other than the open one, and a pattern opens only if
the open one keeps room to grow (`pat_can_open`; the screen says "PATTERNS FULL"). `check.sh` holds bank A through
the cache to the browser's, byte for byte.

**The store** (format v3, sections, e.g. 'S' sounds, 'C' categories) is one blob: the browser keeps it in its
storage; the FM-1 streams it into flash 4 KiB sector by sector, **its header (magic, length, CRC-32) last**, so a
save cut short reads as none. It is written only when the sequencer stands (a sector erase stops the sound ~50 ms).

**The sound library:** 128 sounds kept apart from the patterns (a machine and all its settings, and a category),
loaded onto any track. Categories are guessed from the machine and its notes when saved under ALL.

---

## 9. The FM-1 link (SysEx)

The Patch Editor (web) reads and writes the FM-1's store over USB-MIDI SysEx, one request at a time, each answered
with a status first:

```
F0 7D 'M' '2' 'K' cmd pack7(body) F7
HELLO (version, counts, name, image size)  INFO (length, CRC-32)  READ (128 B a reply)
BEGIN  WRITE (256 B a request)  END (the header written once length and CRC agree; the core reloads as at boot)
```

- Only while the sequencer stands; a write refused, cut or stalled (4 s) writes the FM-1's own state back.
- `host/m2k_link_test.c` runs both ends on a NOR flash in memory, with a store the browser wrote for the FM-1 and
  the FM-1's read back in the browser.
- 0x7D is the non-commercial SysEx ID: **use a different magic for the next firmware** (not 'M2K').

---

## 10. The screen

- One bit, 240 x 240, a 7.2 KB framebuffer, MSB first. Light or dark (a setting; the web follows the page's theme).
  Ink and paper become RGB565 on the FM-1.
- **Fonts** from `tools/gen_font.py`: Orbitron (SIL OFL) rendered without anti-aliasing at 11, 15, 24 and 36 px,
  every capital between the same two rows. One size for every big value, one for titles: consistency was asked for
  more than once.
- **Marks** from `tools/gen_icons.py`: one isotype per machine on a 12-unit grid, flat (64 px) and extruded in 3D,
  ray-marched, sides dithered, 12 frames that move as the machine does.
- **The grid (MONO2K's):** five bands, each on its own rule: header 0–15; knob row 16–57 (four cells under the
  four knobs); stage 58–186 (the value's column 104 wide, the picture beside it); the steps 188–211; footer
  213–239. Held windows (maps, cards, questions) sit over the stage with a two-dot shadow.
- **The text audit is the most useful screen tool we built.** In the host build (`M2K_TEXT_AUDIT`), every `text()`
  records its ink box; a paper fill hides what it covers; `ta_check` reports two texts that touch (or are a dot
  apart), a text cut by a clip or the screen, and a text running past its window. It runs over the whole tour and
  over `--text-sweep` (every page, machine and knob value, every latched map and help card) in `check.sh`. Fit text
  with `text_fit` (tracking, then a shorter string) instead of guessing widths. The owner's rule: **no overlapping
  text, ever** — the audit made that enforceable. It does not see a text crowding a frame line (e.g. a title
  against a tag); look at the screenshots too.
- Animations: page dissolves, glitches where things move, live envelopes, filters, LFOs and scopes. The boot splash
  resolves the logotype.

---

## 11. The workflow: what we arrived at, and why

Researched against Elektron (FUNC, YES/NO, trig + knob), the OP-XY (held layers), Felucca, SLOOP, Hortator,
ghoulbox, DUELL and others; the full notes are in the session's research. The scheme MONO2K ended with:

1. **Fixed verbs:** REC copies, PLAY pastes, SEQ clears, everywhere. **The button held is the noun:** GLO the pattern
   (in GRID the track's steps), HOME the patterns picked, EDIT the track's sound, a step held in GRID that step, REC
   the track just recorded (REC + SEQ clears it).
2. **Again undoes:** the same paste or clear a second time puts things back; GLO + OCT− undo, GLO + OCT+ redo (one
   level).
3. **Hold the clear for INIT:** a ring fills on screen for a second. No silent timing gestures that destroy.
4. **Only the flash asks** (YES = OCT+, NO = OCT−): a sound over another, wipe everything, demos back, load a
   project. RAM edits never ask (they undo).
5. **Tap = page, hold = layer, double-tap = latch.** The first tap is never delayed. A combo shows its map at once;
   a button held alone shows its map after ~350 ms; any other button held 1 s alone shows its help card.
6. **Held is temporary, latched is lasting:** solo while held, PERFORM effects while held, LFO-held knobs snap back
   (OCT+ keeps them).
7. **OCT− held + a knob:** that knob back to its default; with a step held, its lock removed (Elektron's "trig + knob
   push" for knobs without a push).
8. **The keys light for the layer held:** mutes and solos, used/open/queued patterns, effects on, tools.

**Lessons**
- Write the rules first (one page), then map buttons. Every new combo was checked against the rules; that caught
  conflicts before they shipped.
- **A double-tap latch conflicts with tapping a page button to step through its pages.** MONO2K's window is 250 ms;
  slower taps page. The second tap must undo the first.
- **A latched layer keeps its keys and knobs, but button combos need the button physically held**: otherwise a
  performer with GLO latched for mutes presses PLAY to stop and pastes over the pattern.
- **A modifier that was used does nothing on release** (REC + SEQ must not arm recording or start FILL; a help card
  read must not change the octave).
- Destructive things are visible (a ring, a question), undoable, or both.
- Rubber domes are rarely simultaneous: accept a second press shortly after the first (CoralFlower's 50 ms).
- Avoid OCT− + OCT+ together (other firmwares use it for update mode).
- Each held layer has an on-screen map; it beats any manual page.
- Tests in the host must advance time between taps, or two taps at the same instant read as a double-tap.

---

## 12. The website

- **Pages** (`web/`, served by GitHub Pages): the emulator (`index.html`, `app.js`, the panel drawn in SVG), the
  manual (`manual.html`, `manual.js`: one article at a time, routed by `#id`; the machines and knobs read off
  `m2k.wasm`; screens are the host tour's PBMs saved as 1-bit PNGs in `web/manual/`), the installer, the Patch
  Editor (`editor.js`, its own session for the FM-1's patterns, `.m2ks` store files). `style.css` and a sub-header
  are shared; `site.js` holds the theme and textures, `panel.js` the FM-1's layout.
- **Cache busting by hand:** every asset is loaded with `?v=N`. After a wasm build bump `m2k.wasm?v=` in `app.js`,
  `manual.js` and `editor.js`; bump `app.js?v=`, `editor.js?v=`, `manual.js?v=`, `changelog.js?v=` in their pages
  when those files change, `style.css?v=` in all four pages, `worklet.js?v=` when the worklet changes. Forgetting
  one serves a stale file to everyone.
- **A change log on the site** (`web/changelog.js`): an entry a day, newest first, a kind on every line, written
  for the player ("what changed for you"), not the developer. Every push adds to it.
- In-page anchors break a hash-routed manual (`#latch` would open the first article): link to articles only.
- The emulator keeps the work in the browser's storage; export and import files exist for projects.

---

## 13. Testing and measuring

**`sh tools/check.sh`** (~6 min, never kill it mid-run: it mutates `m2k_factory.c` in place):

1. clang (and GCC when installed) `-Wall -Wextra`: no warnings, and a file that doesn't build fails.
2. The core built for armv7m soft float: no float helper, no 64-bit division (`nm -u`); flash and RAM printed.
3. ASan + UBSan: the self-test, the FM-1 store and link (both ends), every screen with the text audit, the text
   sweep, bank A, every reference spec, the power loop. Old binaries are deleted first so a failed build runs nothing.
4. The format converters mutated: every mutant caught, every mutation site found.
5. The wave banks (in tune, level, aliasing); the bus's controls against recorded curves.

**The host self-test** (`m2k_host --selftest`) holds what matters by behaviour: projects (save, reload, export,
import, full images), formats, MIDI clock in and out, two cores against one, every workflow combo, latches, cards,
LEDs. Each new behaviour came with a test; each fix with a test that failed before it.

**Calibration** against the Monomachine: `tools/ref/mm_ref` boots the owner's OS image in gearmulator (never copied
into the repo) and plays it over MIDI as an instrument is played; `host/m2k_ref.c` plays MONO2K the same way; the
Python scripts compare levels, spectra and envelopes. `tools/ref/regress.py check` holds every measured case
(PASS / FAIL, XFAIL for a known gap whose size may only narrow). For the next firmware the same idea works with any
reference you can play (a hardware synth recorded, a plugin): **measure, compare, never fabricate a number.**

**Demo tools:** `demo_levels.py` balances a pattern's parts by measurement; `demo_pitch.py` checks every trig's pitch
against the key; `--analyze` solos every track and flags what is too quiet, clicky or clipping.

---

## 14. How the work was run

- **Small, reversible changes**, each with its test, committed separately (engine work apart from demo content).
- **The owner hears before it's kept:** demos went out in batches of four as ~30 s WAVs with their numbers, and were
  committed once approved. New machines go out as WAVs of a few settings.
- **Every push:** check.sh passes, the wasm rebuilt, versions bumped, the change log written.
- **The FM-1 is the judge:** a package is built and sent for a try on the owner's FM-1 before anything is published.
- **Memory notes across sessions:** the queue, decisions, and "never do X again" lessons (for example, killing
  check.sh mid-run once shipped a mutant).
- **Ask when it's the owner's decision** (names, licences, identity numbers); decide and say so when it isn't.

---

## 15. Starting the next firmware

The brief: four tracks, a pure synth for lofi, on your own waveforms and samples, with a workflow and a visual style
that are not MONO2K's. What to take, what to leave, and the decisions to make first.

### 15.1 Take as it is (renamed)

| From MONO2K | Why |
| --- | --- |
| `firmware/fm1/` (the platform, minus `m2k_fm1.c` / `m2k_link.c`) | Boot, update, rescue, panel, LCD, MIDI, the second core: months of other people's work. GPL-3.0 with credits |
| `tools/fm1/build.py` (renamed identity, flags) | The container build and its size checks |
| The core / browser / host split and a small `xx.h` API | The fastest loop we had |
| `web/build.sh` + the worklet pattern | The emulator for free |
| `tools/gen_tables.py`'s approach | Every float law as a table |
| The text audit + tour + sweep | No overlapping text, enforced |
| `check.sh`'s stages | Warnings, the soft-float symbol check, sanitizers, mutations |
| The store stream (header last, CRC) and the link protocol | Safe saves, an editor on the web |
| The installer page (Felucca's files) | Installing from the site |

### 15.2 Rewrite on purpose

- **The UI layer** (`m2k_ui.c`, `m2k_gfx.c`'s style, fonts, marks): the new look and workflow. Keep the mechanics
  (frame drawing, text records for the audit, overlays, toasts) and replace the design.
- **The sequencer's shape** if four tracks want something else (longer patterns, polyphony per track, song mode).
- **The sound engines:** a pure synth (oscillators on your waves, filters, envelopes, modulation) replaces the
  machines; the lofi bus is a good starting point to keep or reshape.

### 15.3 Decide before writing code

1. **The voice:** mono or polyphonic tracks? With four tracks on two cores, each track has roughly 1.5x MONO2K's
   per-track budget (estimate; measure with the instruction count in 5).
2. **The workflow rules** on one page (as in 11): verbs, nouns, undo, what asks, tap / hold / double-tap, what the
   LEDs say. Then the button map.
3. **The screen grid:** bands and their rules, one type size per role, the window style for held layers. Build the
   empty shell and its audit first.
4. **Names:** the product, the identity range for build.py, the SysEx magic, the store's flash region (two
   firmwares that both use `0xC0000..0xE0000` overwrite each other's saves; say so in the installer if you share it).
5. **Licence:** GPL-3.0-only (the platform requires it).

### 15.4 Your waveforms and samples

- **Single-cycle waves:** follow `tools/gen_waves.py` (read the file as it is, find its whole cycles from the
  spectrum, resample one cycle's harmonics exactly to 256 points, scale from its own peak, round to 16 bits, record
  every step in a `PROCESSING.md`, never hand-edit). 64 waves x 256 x 2 B = 32 KB. For less aliasing on high notes,
  keep two or three band-limited versions per wave (an octave table), as the LXR does.
- **Samples: the flash budget decides.** The app slot (581 KB) holds code, tables and data together; MONO2K's image
  is 501 KB with 32 KB of waves, so ~80 KB were left. Rough sizes (estimates): 16-bit at 22.05 kHz = 44 KB/s; 8-bit mu-law at
  22.05 kHz = 22 KB/s; 4-bit ADPCM = 11 KB/s. So 200 KB of samples is ~9 s of mu-law or ~18 s of ADPCM. Lofi
  welcomes both. More room means samples in the data area (`0x93000..0xC0000` is ~180 KB, shared with other
  firmwares' data), loaded through the link: plan the format (a header, a CRC, loop points) the way the store is.
- A sample pipeline like the waves': resample (band-limited), trim, normalise to a recorded peak, loop points
  found or given, encoded, its processing written down, the originals untouched in `assets/`.

### 15.5 A first week, in order

1. New repo; copy the platform with its licences; a core with `init`, `render` (a sine), `frame` (a test
   pattern), `leds`; the host harness; the browser build. Hear the sine on the FM-1.
2. The screen shell and the text audit; the grid; fonts generated.
3. The workflow rules page; the button map; held-layer maps.
4. One voice, measured; then four tracks; the CPU meter (`--diag`) on the FM-1.
5. The sequencer; the store; the link; the website's pages and change log.

---

## 16. Appendix: commands and files

```bash
cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm
build/host/m2k_host build/host            # the tour: every screen as PBM, the text audit
build/host/m2k_host --selftest
build/host/m2k_host --text-sweep
.venv/bin/python tools/montage.py build/host build/screens.png 2
sh web/build.sh                           # web/m2k.wasm
sh tools/check.sh                         # everything; don't interrupt it
.venv/bin/python tools/fm1/build.py       # the FM-1 package
```

| File | What |
| --- | --- |
| `firmware/src/m2k.h` | the API, the enums (buttons, encoders, parameters, machines), the pattern struct |
| `firmware/src/m2k_core.c` | the unity build, the shared helpers (sine, increments, saturation) |
| `firmware/src/m2k_dsp.c` | machines, the track chain, envelopes, LFOs, the pattern cache's `PAT()` |
| `firmware/src/m2k_fx.c` | delays, the bus, PERFORM |
| `firmware/src/m2k_seq.c` | the sequencer, kits' defaults |
| `firmware/src/m2k_proj.c` | patterns packed, projects, the sound library, the store |
| `firmware/src/m2k_factory.c` | bank A, written in a step grammar; format converters |
| `firmware/src/m2k_gfx.c` | 1-bit drawing, text, the audit's records |
| `firmware/src/m2k_ui.c` | pages, the workflow, maps, cards, LEDs |
| `firmware/fm1/m2k_fm1.c`, `m2k_link.c` | the FM-1 glue, the store in flash, the link |
| `host/m2k_host.c`, `m2k_link_test.c`, `m2k_ref.c` | tests, the link on a flash in memory, the reference player |
| `tools/gen_*.py` | tables, fonts, marks, waves |
| `tools/check.sh`, `tools/ref/` | the checks, the calibration |
| `web/` | the site; `changelog.js` the change log |
| `CLAUDE.md` | the rules a coding session follows |
