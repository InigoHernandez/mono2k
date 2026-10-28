# MONO2K

Monomachine-style firmware for the M-VAVE FM-1 (six mono machines, 64-step sequencer with locks, conditions and
ratchets, a lofi bus, a 1-bit dithered Swiss screen). Plan and status: README.md.

## Hard rules

- Target: JieLi AC791N (a single-precision FPU, by fm1-base's flags, but no libgcc): integer DSP only, so the FM-1,
  the browser and the host render bit for bit alike, **no 64-bit division**, no floats in `firmware/src`.
  Tables come from `tools/gen_tables.py`.
- **64-bit multiplies only through m2k_core.c's helpers** (`mul_sh`, `mulu_hi`, `mul64_sh`, `dot`, `mul2_q15`): on the
  FM-1 a 32 x 32 -> 64-bit multiply inlined beside a load now and then leaves a register a bit off (crashes, garbage:
  felucca-bsp docs/CPU.md), so there (`M2K_MUL_OUT`) they are out of line and use the product at once, and
  `tools/fm1/build.py` fails a build with any 64-bit multiply beside a load (`mul64_check`). A product that provably
  fits 32 bits stays a plain multiply, its range in a comment. (The format conversions at load and `track_place` keep
  a few inline, clear of loads as the check finds them.)
- Memory: big buffers are `POOL` (Felucca's .pool section on the device). Keep RAM use in mind: delays 96 KB,
  glitch 64 KB, patterns ~31 KB.
- Machines are clean-room. Never copy or decompile code from Elektron's OS (the gearmulator ROM is a reference to
  listen to and measure, never a source). No Elektron or M-VAVE marks in the product name.
- The browser runs the same core: after any change to `firmware/src`, rebuild with `sh web/build.sh`.

## Build, test

- Generators: `.venv/bin/python tools/gen_tables.py`, `tools/gen_font.py [--sheet build/fonts.png]`,
  `tools/gen_icons.py [--sheet build/icons.png]`.
- Host: `cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm && build/host/m2k_host build/host`,
  then `.venv/bin/python tools/montage.py build/host build/screens.png 2` to see every page.
  `build/host/m2k_host --selftest` checks the projects (save, reload, store, export / import, full images);
  `build/host/m2k_host build/patterns --analyze` solos every track of bank A and writes each pattern's WAV.
  `build/host/m2k_host build/pitch --pitch` then `.venv/bin/python tools/demo_pitch.py build/pitch [--steps]`:
  each trig's pitch against its note, held notes, resonant rings against the key, semitone clashes; run it on
  every new demo (docs/bank-a-diagnosis.md; keys from docs/bank-a.md's table, `--ea69d53` for the old bank's fixture).
  `build/host/m2k_host DIR --demo [SECONDS] [SLOT ...]` renders patterns for listening. Bank A is made from sound
  sheets (`tools/sheet/README.md`): random settings the owner picks by ear, parts written to `docs/composing.md` in
  `tools/sheet/compose.py`, fixed by measurement (`fit.py`: each track's share on the bar's chord; spectrograms for
  rings and sweeps), and written into `factory()` by `mel.c`'s `cdump`, so the firmware plays the renders the owner
  approved. New parts go to the owner as ~30-40 s renders and are committed once approved. (`tools/demo_levels.py`
  balances by role and was the old banks' tool; the owner rejected a fixed mix rule.)
- Web: `sh web/build.sh`, serve `web/` (`.claude/launch.json`: mono2k-web on port 8742). Four pages share `style.css`
  and a sub-header: the emulator (`index.html`, `app.js`), the manual (`manual.html`, `manual.js`: one page at a time,
  the machines and knobs read off `m2k.wasm`, the screens `web/manual/*.png` from the host's tour and its manual
  scenes, `m2k_host DIR`, each PBM saved as a 1-bit PNG of the same name), the installer
  (`installer.html`: installs `web/firmware/mono2k.fwsc`, or a `.fwsc` from a file) and the patch editor (`editor.html`, `editor.js`: the same worklet and
  saves; the FM-1 link's client, the FM-1's patterns a session of their own under `mono2k.fm1.v1`, `.m2ks` store files).
  `panel.js` is the FM-1's layout, `site.js` the subpages' theme and textures. On the emulator page `help.js` says what
  every control, page and layer does (the status line under the panel, the guide's list: from the manual and m2k_ui.c),
  `screens.js` runs copies of the core apart from the one that plays (from `app.js`'s compiled module; a script of
  panel actions each), `guide.js` is How to play, `engines.js` the machines' Hit (`w_audition`, `w_hit` in `m2k_web.c`:
  a machine's INIT at its home note, an FX machine after a plain SWAV-SAW) and `made.js` How it was made's charts (the
  suite's numbers a snapshot of a `regress.py check` run: update them with it). Every push adds what it changes for a
  player to `web/changelog.js` (the manual's Change log: an entry per day of updates, newest first, a kind on every
  line: machines sound sequencer workflow patterns screen web fixes), and bumps `changelog.js?v=` in `manual.html`. Every
  update also adds what it taught to `../FM1-FIRMWARE-HANDBOOK.md` (outside the repo: the owner's handbook of MONO2K's
  and Demake's lessons, for the next firmware), in its section and its Updates log. After a wasm build bump `m2k.wasm?v=` in
  `app.js`, `manual.js` and `editor.js` alike (and `worklet.js?v=` in `app.js` and `editor.js` with the worklet).
- The screen's grid (m2k_ui.c, `docs/screen-grid.md`): MONO2K's own pixel font (`tools/gen_font.py`: 5 x 7 capitals
  drawn a pixel at a time, F_S 1x, F_M 2x, F_L 3x); five bands: the header an ink bar 0-10 (transport, tempo, bars,
  track, pattern), the knobs 11-37 (four cells of 60 on solid rules: label, dotted rule, dial and value), the stage
  38-205 (the value's column [4, 101), the picture `PIC_W` x `PIC_H`, 129 x 161 from 108, 42), the steps 206-228, the
  footer an ink bar 229-239 (breadcrumb, section and page); held windows (GLO, HOME, EDIT, SEQ) over the stage. The
  host's text audit (`M2K_TEXT_AUDIT`: every text's ink box; the cells and rules as drawn, `CELL`, `rule_h` /
  `rule_v`; a paper fill hides what it covers) fails the tour and `m2k_host --text-sweep` (every page, machine and
  knob value) on any two texts that touch, one cut, one past its cell or within 2 px of a rule: run it after any
  change to the screen, and fit text with `text_fit` / `fit_font` rather than guessing widths. The splash (2.1 s,
  then a 0.4 s wipe; a press skips it and is swallowed; `w_skip_splash` for the site's scripted cores):
  `m2k_host DIR --splash`, `tools/splash.py DIR`; a frame's instructions, `host/m2k_draw.c` (built as the FM-1 builds).
- Checks: `sh tools/check.sh` (~6 min): clang (and GCC, when installed) -Wall -Wextra with no warnings; the core built
  for armv7m soft float calls no float helper and no 64-bit division (flash / RAM printed); the self-test, every
  screen, bank A and every reference spec under ASan + UBSan, clean; the format's conversions mutated
  (`tools/ref/mutate_formats.py`: every mutant of `fm_v9` / `sid_v10` caught by the self-test but the one equivalent);
  the wave banks (`tools/wave_check.py`); the bus's controls (`tools/bus_curves.py`: `m2k_host DIR --bus` measures
  every value of GLCH HISS ADPC DRIV TAPE DCAY DAMP, held to `tools/ref/bus_m2k.json`; each old value through `T_V9_*`
  to the old engine's, `tools/ref/bus_v8.json`, and through `T_V17_*` to format 16's, `tools/ref/bus_v16.json`; the
  reference mix's kits turned to today's amp, so under -25 dB DRIV's residual against the old engine may only not grow).
- The FM-1 build (`firmware/fm1/README.md`): `.venv/bin/python tools/fm1/build.py [--diag] [--release X.Y]` makes
  `build/fm1/mono2k.fwsc` (identity FM-1_980 for every build and release: the hub's FM-1_981 and _983 are others'; a
  release sets APP_VER and names the file `mono2k-X.Y.fwsc`, the one the installer and the GitHub release share) with JieLi's toolchain in a linux/amd64 container
  (`~/.jieli/toolchain`, the AC79 SDK in `~/fw-AC79_AIoT_SDK`, colima with Rosetta, `DOCKER_CONFIG` ~/.jieli/docker).
  The platform (`firmware/fm1/{hal,loader,src}`) is CS-1's (Felucca, SLOOP, Dinghy, FM-106), GPL-3.0, its headers
  kept; its USB names are MONO2K's ("MONO2K FM-1" by Sequencist, `usb.c` STR1 / STR2: "FM-1" in the
  name, so the hub's installer probes the port), the update loader's Felucca's ("Felucca Update": the hub finds a
  waiting loader by them); `m2k_fm1.c` is the glue (the core in the main loop, the audio ISR copying a 17 ms ring), `m2k_link.c` the store
  (a stream into the flash, 0xC0000..0xE0000, when the sequencer stands; the update loader stages at 0xE0000) and the Patch Editor's link to it (SysEx
  `F0 7D 'M2K' cmd pack7(body) F7` through the platform's `APP_SYSEX` hook; `host/m2k_link_test.c` runs both on a NOR
  flash in memory, and `check.sh` with the browser's store for the FM-1, `m2k_host --web-store`, and the FM-1's read
  in the browser, `--fm1-store`). The core is its own unit there (`core.o`): reach it
  through `m2k.h` alone. RAM: 96 KiB `.bss` + 336 KiB pool; the FM-1 keeps one project of 64 patterns, two of them
  unpacked (`M2K_PAT_SLOTS` 2: `PAT(i)` is pattern i everywhere, a cache over the working image on the FM-1; reach
  patterns only through it, and never hold two `PAT()` pointers to patterns other than the open one), the rest
  packed in a 46 KB image, a blank one taking no bytes; a pattern opens only if the open one keeps room to grow
  (`pat_can_open`). `M2K_PATTERNS`, `M2K_PAT_SLOTS`, `M2K_PROJECTS`, `M2K_IMG_DATA` are set by build.py; check.sh holds
  bank A through the cache to the browser's, byte for byte. Never publish a package on the site that has not
  run on an FM-1, unless the owner asks: then it goes in `web/firmware/` (the package and `mono2k.json`, its file,
  SHA-256, identity, version, date and a note the installer shows). 2026-10-24: `mono2k.fwsc`, 0.1, FM-1_980, the
  new bank, published at the owner's request (first as a test build, then as the latest at their word) before any
  FM-1 had run it. 2026-10-25: the hub listed v0.1; 1.0 (out of the hub's beta, which marks anything under 1.0)
  released after the owner had tried it on their FM-1. 2026-10-27: 1.1 (formats 16 and 17) on the installer and as
  the GitHub release at the owner's word ("when you fix all of this upload the next version"), before an FM-1 had run
  it. The installer page (`installer.html`, Felucca's `web/fm1/fm1ota.js`, `fm1pkg.js`, unchanged) installs
  a package from a file meanwhile.
- The FM-1's budget: Felucca's heaviest patch is ~1570 Mac instructions a sample (its tests/cpu_baseline.txt) and
  runs on one core; count MONO2K's the same way, built as the FM-1 builds it (`cc -O2 -fno-builtin -fno-vectorize
  -fno-slp-vectorize -DM2K_MUL_OUT`, the 64-bit products out of line; `proc_pid_rusage` ri_instructions). The core's
  byte loops (m_set, m_cpy) are word-wise for it.
  Two cores: the render hands the second tracks [split, 6) of each block through `m2k_par_begin` / `m2k_par_end` (the
  FM-1's glue fills them with CS-1's audio worker; elsewhere they are unset and the core does it all), the split keeping
  every FX machine with the track it hears; the self-test renders bank A with the second core's tracks first and last
  and holds both to the one-core render, sample for sample. `--diag` packages show CPU (the first core's render, the
  waits included), W (the second's) and F (drawing) in % over the screen's top.
- MIDI (GLO > MIDI, settings S_MCH S_SYNI S_SYNO S_NOTO; GLO > IN, S_NTIN S_THRU): `m2k_midi_out(msg, played)` hands
  out what is due once the sound has reached sample `played` (`m2k_samples`); the FM-1 sends it as the ISR takes the
  ring, the worklet as it renders (the page stamps it with the audio latency). In: `m2k_midi_from(src, ...)`, USB or
  TRS (the platform's ring marks the jack's as cable `MIDI_TRS`, USB's forced to 0; `m2k_midi` is USB, the browser's
  and the host's): the clock and transport from SYIN's (OFF USB TRS BOTH: `syin_choice`, an old switch's OFF and ON
  kept), notes, CCs and mutes from NOTE IN's, THRU the jack's out over USB (not its clock while SYNC OUT is on). The
  update loader is built from the same usb.c: platform changes there go under `#ifndef FELUCCA_LOADER`, and its
  loader.bin / ota.bin are compared with an untouched build's. The self-test holds the clock out, the clock in, the
  notes out, the channel and each source rule.
- Calibration (README, Calibration): `tools/ref/mm_ref` (gearmulator, the user's OS image at a path, never in the
  repo; it logs to stdout, so send stdout to /dev/null) against `host/m2k_ref.c`, same specs in `tools/ref/*.spec`.
  Pattern format: 2 amp ATK/HOLD/DEC/REL on the measured curve; 4 SWAV-SAW's page; 5 the track chain on the
  Monomachine's (DIST/DSND/EQG bipolar, the filter follows the note); 6 GND-SIN's TUNE +-2 semitones (3.125 c a step);
  7 SWAV-ENS on SWAVE-ENS's page (`ens_v6`); 8 the wave bank's choice (`P_WBNK`, kits 71 wide; the self-test packs the
  older layouts with `pat_pack_v`); 9 the bus's controls on new laws (`bus_v9`, tables `T_V9_*`); 10 FM+STAT's and
  FM+DYN's pages (`fm_v9`); 11 SID-6581's page (`sid_v10`); 12 GND-SIN's page (TUNE on H, CC 55, as the
  Monomachine's; MONO2K's own FOLD..HARM on A..G, nothing at init: `gsin_v11`); 13 HOLD in beats, v / 32 of one at the
  pattern's tempo, as the Monomachine's (`hold_v12` takes 14.6 ms steps to the nearest at the part's tempo; bank A is
  written in them and turned at build); 14 VOX-6 on VO-6's page (VOC1 VOC2 V-SW VOIC | CONS CLEN CVOL TUNE: `vox_v13`
  takes the old vowel to the nearest F1 F2, BRTH to VOIC; A02 CUTUP's voice turned at build); 15 the tracks' speeds;
  16 the amp's ATK DEC REL and the LFOs' SPD and DPTH on the owner's laws (`feel_v15`: T_AMP15 by the time heard,
  ATK's rise by its loudness, T_SPD15 by the rate, T_DPTH15 by the swing, an LFO on a DEC or REL by the new law's
  steps; the store's version 4 converts a version-3 store's sounds alike); 17 the bus's GLCH (subtle to wild, its own
  dice, every event crossfaded), the reverb (an FDN, DCAY's RT60 0.3 .. 21 s, DAMP the top's time as a share of the
  bottom's) and the sends linear, the owner's departure from the Monomachine's square DSND (`fx_v16`: DCAY and DAMP
  through a 9 x 9 grid measured on the old engine, `T_V17_RV`, `tools/conv17.py`; the VERB send by its heard level,
  DSND |d|^2 / 64, `T_V17_GLCH`; the store's version 5 converts the sounds through `fx_kit_v16`). Older ones are
  converted on load. The
  Monomachine's tempo is set on its panel (TEMPO, the LEVEL knob a BPM a detent; `chain_hold46.panel`): the loop and
  spec modes leave it at 120.
  A step's flags bits 3..5 are its trig types (`ST_NOAMP`, `ST_NOFLT`, `ST_NOLFO`; clear is ALL, as every older pattern
  has them: no format change). FM+'s 24 ratio steps decode as the Monomachine's sound does, `fr24` (v * 24 + 8) >> 7,
  not as its screen labels them (`chain_fmstep43`, FM+STAT's 2FRQ `chain_fmstep44`). The owner's sheet (FM MACHINE
  RATIOS.xlsx) against the Monomachine's sound, every seven ratio knobs swept: FM+DYN's columns are right (1FRQ / 64,
  (2FRQ / 64)^2); FM-PAR's copy the screen (five boundaries a value late, steps 5..9 out of order); FM STATIC's (v / 64)
  are not what it plays: FM+STAT steps the same 24 as FM+PAR, both ratios. MONO2K matched at every value.
  GND-NOIS is the Monomachine's (ST RED STON, TUNE on H; machine 14, the newest); MONO2K's own noise is GND-PERC and
  keeps machine number 1 (`M_GPRC`), so no conversion: patterns store machine numbers, new machines go last.
  Bank A is four parts, CIRCUIT TIDE SUSPEND LANTERN (factory revision 13, `docs/bank-a.md`), written on today's
  pages with `part()` in `factory()` (their kits as `SET`, their steps and locks as tables, from `cdump`), their amp,
  LFO and bus values on format 15's and 16's laws, as the owner approved them, turned at build (`feel_v15`, then
  `fx_v16`; revision 13's bytes).
  The bank before it (MAMMOTH .. SPLICE, written on the old pages and turned at build) is kept only as a test fixture,
  `host/m2k_bank_rev9.c` (`factory_rev9()`), so the format tests and the conversions' mutants still run on real old
  parts; it is never built into the instrument. A batch that replaces demos bumps `FACTORY_REV`, adds the names it
  replaced to `OLD_DEMO` and the new revision's row to
  `OLD_PACKED` (m2k_proj.c: each demo's packed length and FNV-1a; the self-test prints the row wanted), so a saved
  store takes the new parts only in the slots whose bytes are still the ones that revision shipped: a demo the owner
  edited, and A05 onwards, are kept. The reference renderer writes 32-bit float WAVs; `tools/ref/sweep.py`'s `load` reads
  both.
- The FX machines: on the emulator an FX machine assigned by MIDI hears nothing until its track's KIT > EDIT KIT
  TRACK page has been opened and closed once (`mm_ref`'s loop mode does it after a `T` line with an FX machine).
  After it RINGMOD and REVERB carry the track before; THRU, CHORUS and DYNAMIX stay silent under every input (NEIBOR,
  BUS AB / CD / EF) and every knob, so FX-CHORS and FX-DYNMX cannot be measured: MONO2K's own designs on the
  Monomachine's pages, held by the self-test. FX-RING can be (two-track loops, the FX panned away from its source),
  and is rebuilt on its measurements (group `ring`, `tools/ref/ring45.loops`: a loop spec, `regress.py mmloops ROM`
  renders the Monomachine's side): it hears the track before after its VOL, its clip and its LEV (`tvol`), as the
  Monomachine's FX do; FX-CHORS and FX-DYNMX still read `tout` (after its amp, before its VOL: MONO2K's own, and
  MAMMOTH's chorus is voiced on it). Either way a track's order in `m2k_render` matters. Read a product's level with
  a flat-top window (the two instruments' carriers sit 0.04 Hz apart: a Hanning's scalloping reads it 1 dB off). `mm_ref ROM panel DIR < SCRIPT` drives the OS's panel (tap / enc / chord /
  assign / midi / wait / shot / record) to find where a setting lives; `Q track step` in a loop places the OS's own
  trigs (A01's demo trigs cleared first).
- Fidelity regressions: `.venv/bin/python tools/ref/regress.py check [GROUP ...]` renders MONO2K with the chain specs
  and compares against `tools/ref/regress_ref.json` (numbers measured from the Monomachine's output; `capture` rebuilds
  it from `build/ref/mm_c*`). PASS / FAIL, XFAIL for a known gap (with its reason), INFO not asserted. Each XFAIL's
  size is held in `tools/ref/regress_gaps.json` (`regress.py gaps`; it only narrows unless `--widen`, a decision the
  diff shows): a gap that grows past it plus the tolerance FAILs, and XPASS fails too (re-mark the case PASS). Every
  DSP fix gets a case that failed before it. `m2k_host --selftest` covers the pattern formats.
- Loud signals: test the chain with the machine's own signal, not only a sine. Bank A's demos were written on a DIST
  whose ceiling was GND-SIN's for every machine, while the Monomachine's is per machine (`T_DIST_M`; SWAV-SAW's
  10.5 dB lower). The sine cases all passed, and a driven SWAV-SAW bass came out 10.7 dB louder than the
  Monomachine's. The cases that catch this: `chain_dist21` and group `dist_loud`. The power test is a whole loop on
  both instruments: `build/ref/mm_ref ROM loop build/power < tools/ref/power.loop > /dev/null`, then
  `build/host/m2k_ref build/power --loop [DRIVE] < tools/ref/power.loop`, then
  `.venv/bin/python tools/ref/power.py build/power`.
- The Monomachine's renders in one `mm_ref` run were not independent: its oscillator keeps feeding the filter between
  notes (the amp envelope comes after the filter and the drive), so a line inherited the last one's resonance; another
  machine assigned does not reset it, a note played on it does (`chain_order11`). `mm_ref` now plays a short GND-NOIS
  note (GND-SIN for a noise line) before every line, always. References captured before (`regress_ref.json`, up to
  `chain_tune13`) were rendered without it: steady measurements are unaffected, slow build-ups are (`hp_drive`: INFO).
  It keeps every control from the line before too (assigning a machine restores none): a spec line sends every
  control it depends on, the machine's page included (`chain.py` DELAY_INIT; the delay references were re-rendered
  so, and two known gaps were that).
- `assets/waves/mono2k64/` holds the owner's own 64 waves (GPL-3.0 with the project), the originals untouched;
  `tools/gen_waves.py` makes the device's bank from them (`firmware/gen/m2k_waves64.h`), never edited by hand; audit
  and processing beside them. The Monomachine's DigiPro waves are never copied.

## Style

C99, 4 spaces, unity build, static everything, short comments that say why. The screen's grid is the five bands above
(header bar 0..10, knobs 11..37 under the physical knobs, stage 38..205, steps 206..228, footer bar 229..239),
`docs/screen-grid.md`.
