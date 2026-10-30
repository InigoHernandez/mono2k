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
- The screen's grid (m2k_ui.c, `docs/screen-grid.md`; the 2026-10 restyle after the sound library page, Torso's S4,
  Hortator and the MNM-1's menu, the owner's): a 4 px grid for every position, size and text baseline (`tx_b` draws on
  a baseline); the type since 1.6 (2026-10-30) the owner's `mono2k-font.otf`, the FontStruction "Elektron Pixel Font"
  by savingaurora, CC BY-SA 3.0, adapted (`assets/fonts/mono2k-pixel`: the .otf, license.txt and readme.txt travel
  together; the generated bitmaps, an adaptation, CC BY-SA 4.0 as 3.0's section 4(b) allows, GPL-3.0-compatible, the
  owner's choice 2026-10-30, `ADAPTATION.md` beside them; credited in README and the manual; Orbitron stays the
  wordmark's): `tools/gen_font.py` renders each glyph at its native 8 px and the firmware draws it 2 or 3 times larger
  (kept native, a byte a row, widened by `FONT_Z2` / `FONT_Z3` as drawn; the generator holds every glyph to the font
  rendered at 16 and 24 px, and the full stop and comma to the baseline), one weight (the owner: "don't use bold
  anymore"): F_S x2 every line, F_L x3 a page's one big word; capitals 10 and 15 rows, the ladder for `fit_font`. The
  look after Elektron's parameter pages (the owner's, 1.6): each knob's value a picture of it (`icon_of`, `icon`:
  dial, slider, number over a ruler, a choice's box, wave, envelope segment, filter curve, pan; a free knob a dotted
  square), MONO2K's own drawings in the type's 2 px stroke, never Elektron's icons; title bars and tags with rounded
  corners (`rfill`, `rframe`). Bands: the header an ink bar 0-15, its corners rounded (the pattern's slot a paper tag
  and its name, the modes, REC, tempo, transport), the knobs 16-59 in columns of 60 (the picture rows 20-39, the label
  on baseline 56, 4 px in; turning, the value in the label's place, solid; a lock the label solid; a dotted rule on
  60), the stage 64-199 (the page's picture across [4, 236) x [68, 180), its info line on baseline 196:
  what turns and its value, a held step, else the picture's names), the steps 204-223 (16 cells 8 x 8, 12 apart, a
  beat's four under each knob), the footer an ink bar 224-239 (the bars, the breadcrumb, the page). One picture
  language: areas in bars (`bar_col`, `bar_bi`: two columns on, one off; the other state one in three) from dotted
  floors and zero lines, handles, markers, cells. Panels only over the layers (`panel`: Elektron's title bar, solid,
  over a 1 px frame, rows 64-203, the keys' hints at its foot). The host's text audit (`M2K_TEXT_AUDIT`: every text's ink box; the cells and
  rules as drawn, `CELL`, `rule_h` / `rule_v`, a card's or panel's edges; a paper fill hides what it covers) fails the
  tour and `m2k_host --text-sweep` (every page, machine and knob value) on any two texts that touch, one cut, one past
  its cell or within 2 px of a rule: run it after any change to the screen, and fit text with `text_fit` / `lite_fit`
  / `fit_font` rather than guessing widths. The screen's motion is calm by default (S_MOTN, SET > MOTN on the FM-1,
  GLO > PLAY with projects: below 64 no transitions, no glitch on an action, no slip of a changing value; GLTCH brings
  them back), after a player found the
  glitches tiring (the owner's pick). The splash (2.1 s,
  then a 0.4 s wipe; a press skips it and is swallowed; `w_skip_splash` for the site's scripted cores):
  `m2k_host DIR --splash`, `tools/splash.py DIR`; a frame's instructions, `host/m2k_draw.c` (built as the FM-1 builds).
- The controls (the owner's, 2026-10-30: "to pick the track is the select knob, and the ALGO always changes the synth
  engine"): SELECT picks the track (Demake's synth; GLO + SELECT the tempo, a held step's note; GRID's black keys pick
  tracks too); ALGORITHM the track's machine on every page (`set_machine`: off SYN a message names it); each section's
  button turns its own pages (`go`). EDIT held alone 400 ms opens the sound catalogue (`cat_*` in m2k_ui.c, Hortator's):
  SLOT or SELECT puts each sound landed on on the track and plays it (a step's note at its note), CAT filters, OCT+
  keeps it (undoable), OCT- / EDIT / a page / PRESET put the track's own sound back byte for byte, ALGO does nothing
  meanwhile; the EDIT + SEQ REC PLAY SAVE combos act on the track's own sound. The self-test holds all of it.
  HOME has two pages (1.6, the owner's): HOME 1 LEVL PTCH BPM SWNG, HOME 2 the track in view's sequencing, TLEN SPD
  PROB LANE (the same globals as SEQ held's TLEN and SPD and ARP > LANES' LANE); its picture the six lanes, a trig on
  a track under 100 % framed and filled to its chance.
- Checks: `sh tools/check.sh` (~25 min since 1.6's format-20 mutants): clang (and GCC, when installed) -Wall -Wextra with no warnings; the core built
  for armv7m soft float calls no float helper and no 64-bit division (flash / RAM printed); the self-test, every
  screen, bank A and every reference spec under ASan + UBSan, clean; the format's conversions mutated
  (`tools/ref/mutate_formats.py`: every mutant of `fm_v9` / `sid_v10` caught by the self-test but the one equivalent);
  the wave banks (`tools/wave_check.py`); the bus's controls (`tools/bus_curves.py`: `m2k_host DIR --bus` measures
  every value of ADPC DRIV DCAY DAMP, held to `tools/ref/bus_m2k.json`; each old ADPC and DRIV through `T_V9_*` to the
  old engine's, `tools/ref/bus_v8.json`, and `T_V17_RV` to `tools/ref/conv17.json`; the reference mix's kits turned to
  today's amp, so under -25 dB DRIV's residual against the old engine may only not grow; GLCH HISS TAPE measured too
  until 1.5, their rows kept in `bus_v8.json` / `bus_v16.json`, their conversions held by the self-test alone).
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
  it. 2026-10-28: 1.2 (the new screen, the arp's HOLD) on the installer and as the GitHub release at the owner's word
  ("when you finish upload everything to website, github and for firmware hub"), before an FM-1 had run it. 2026-10-28 later: 1.3 (Grids, mutes on
  the bar, finer chances) likewise, at the owner's word ("Publish 1.3 now"); 1.3.1 (the splash's morph, the windows, FILT) at "looks good
  when you finish publish everything please"; 1.4 (PERFORM's second bank) at "publish"; 2026-10-29 the owner installed 1.4 on their FM-1: "it works"; later that day 1.4.2 (FILT's one curve, the cores split by any set, the screen paced by the sound and calm by default) ran on their FM-1 ("it works") and was published at their word ("Publish 1.4.2 now"); 1.4.1 was never released on its own. 2026-10-30: 1.5 (the new screen on the 4 px grid in Silkscreen, SELECT track / ALGORITHM machine / EDIT held catalogue, per-track lengths (format 19), the bus cut, the 96-sound library) on the installer and as the GitHub release at the owner's word ("upload"), before an FM-1 had run this build (earlier 1.5 tests had been sent). 2026-10-30 later: 1.6 (the owner's pixel font and Elektron-style knob pictures, HOME 2, PROB (format 20), the FM-1's CPU meter and CPU FULL, an exact engine pass) at the owner's word ("all good go ahead"), before an FM-1 had run it; the font's adaptation relicensed CC BY-SA 4.0 at the same word. The installer page (`installer.html`, Felucca's `web/fm1/fm1ota.js`, `fm1pkg.js`, unchanged) installs
  a package from a file meanwhile.
- The FM-1's budget: Felucca's heaviest patch is ~1570 Mac instructions a sample (its tests/cpu_baseline.txt) and
  runs on one core; count MONO2K's the same way, built as the FM-1 builds it (`cc -O2 -fno-builtin -fno-vectorize
  -fno-slp-vectorize -DM2K_MUL_OUT`, the 64-bit products out of line; `proc_pid_rusage` ri_instructions). The core's
  byte loops (m_set, m_cpy) are word-wise for it.
  Two cores: the render hands the second any set of the tracks (`par_mask`) through `m2k_par_begin` / `m2k_par_end`
  (the FM-1's glue fills them with CS-1's audio worker; elsewhere they are unset and the core does it all), chosen on
  the tracks' own costs (the sequencer, LFOs and bus run before and after them, the second core waiting: they weigh
  nothing), every FX machine kept with the track it hears; the self-test renders bank A with the second core's tracks
  first and last, and bank A and six FX-chained tracks with every set there (`par_force`), each held to the one-core
  render sample for sample. The glue paces the screen by the sound (a frame waits while the ring is under half full,
  frames further apart while the render is busy). The meter, always (1.6): every half second the glue
  (`meter_tick`, m2k_fm1.c) hands the core `m2k_cpu(first core %, second's or -1, the ring's shorts since power-on)`;
  GLO > PLAY shows CPU, CPU2 and DROP (dashes where none came: the browser, the host), and a short more than the last
  toasts CPU FULL (at most every 5 s). `--diag` packages show the same CPU, W (the second's) and F (drawing) in %
  over the screen's top. The 2026-10-29 owner: "dont make the sound engine worst": CPU savings must keep the sound
  bit for bit; 1.6's owner (a pattern of every track's effects, LFOs, chances and locks overloaded the FM-1): nothing
  cut from the engine (the Ableton Move port will have the power), FM-1-only measures fine. The yardstick:
  `host/m2k_dense.c`'s DENSE (six tracks, every stage of the chain, the MOD effect, both delay layouts, three LFOs
  each, chances, 192 locks), `m2k_draw --cpu` (bank A's four and DENSE: one core, the first core's work, the second's,
  the wall) and `m2k_draw --hash` (their renders' FNV-1a on one core and two, and FXS, the FX machines hearing a full
  chain: a change to the engine holds them the same); the self-test holds DENSE on every set of the second core's
  tracks. 1.6's pass (the chain's coefficients and an LFO's rate kept while their inputs stand, the MOD effect a block
  at a time, tout / tvol only where an FX machine listens, the general case's loop without the machine's side): one
  core 1984 -> 1944 (A01), 4058 -> 3942 (A04), 5507 -> 5333 (DENSE) Mac instructions a sample; the wall 2358 ->
  2284 (A04), 3181 -> 3062 (DENSE).
- The bus (1.5, for the FM-1's time; the owner: "the important are: ADPC, reverb, drive"): GLO's pages TIME BUS SET
  MIDI IN PLAY (`GS_*` in m2k_ui.c: name them, never their numbers), BUS's knobs DCAY DAMP ADPC DRIV (`BUS_K`, PERFORM's
  knobs too), its picture the reverb's tail lifted by DRIV and stepped by ADPC. HISS, TAPE, GLCH and SIZE were cut: the
  pattern keeps their bytes (`G_TAPE` .. no format change, the conversions as they were), unheard; the tape runs only for
  PERFORM's WOW (`WOW_Q8`, the old TAPE 127's depth, its glide in and out kept), the glitch's ring is written always for
  PERFORM's repeats and `bus_step` keeps only the grid's places. Saved ~46 Mac instructions a sample on every bank A
  pattern (the hiss), ~161 on A01 (its tape too); the self-test holds TAPE HISS GLCH SIZE at 127 to 0, sample for sample.
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
  dice, every event crossfaded; cut in 1.5), the reverb (an FDN, DCAY's RT60 0.3 .. 21 s, DAMP the top's time as a share of the
  bottom's) and the sends linear, the owner's departure from the Monomachine's square DSND (`fx_v16`: DCAY and DAMP
  through a 9 x 9 grid measured on the old engine, `T_V17_RV`, `tools/conv17.py`; the VERB send by its heard level,
  DSND |d|^2 / 64, `T_V17_GLCH`; the store's version 5 converts the sounds through `fx_kit_v16`); 18 Grids (Mutable
  Instruments' map, GPL-3.0, credited: `G_GRX`..`G_LANE1`+5 after the speeds, `grids_v17` the defaults, no lane) and
  the chances 90 60 40 10 % after the conditions (indices 21-24, `COND_ORDER` the knob's order; FACTORY_REV 14); 19 the
  tracks' own lengths (`G_TLEN1`+t after the lanes, 1..64, 0 the pattern's LEN; SEQ held, KNOB 2 TLEN: the layer's
  knobs LEN TLEN SPD SWNG, the tempo on HOME and GLO > TIME): a pattern with none packs as 18, byte for byte (`pat_pack`),
  so bank A's bytes, the blanks, OLD_PACKED and 1.4's reads stay, and no FACTORY_REV. A track runs free on its length
  (`tlen_of`: at 1x its step the pattern's count `n1x` round it, off 1x `k % tlen`; its loops at its own wrap), all
  start together at PLAY and a pattern coming in; a track on a Grids lane keeps Grids' cycle. 20 the tracks' chances
  (`G_TPRB1`+t after the lengths; PROB, HOME 2's KNOB 3: its step 0..15, `PROB_PCT` 100 95 90 85 80 75 66 60 50 40
  33 25 20 10 5 0 %, 0 every trig): `prob_pass` after the step's condition (and a lane's hit), a draw of 256 from the
  track's own generator (`sq.prng`, the sequencer's, first core: the same order everywhere, the conditions', LFOs'
  and arp's draws untouched; none at 100 % or 0 %); a pattern with every track at 100 % packs as 19 or 18, byte for
  byte; the blob's head is 60 (BLOB_MAX 3608, editor.js's too). Older ones are
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
  The sound library's factory is `firmware/src/m2k_lib.c` (factory revision 15): 96 sounds, twelve a category in
  CAT_N's order (S001..S096; S097..S128 INIT), each a machine's blank kit (kit_default on format 16's laws) and its
  changes, its note and its character in a few words (host only); within a category different characters, not
  variations, none on the bus's TAPE HISS GLCH or the reverb. Made by measurement, the owner's ears still to come:
  `sh tools/lib_audition.sh` renders each in a phrase for its category at 120 BPM (`m2k_host DIR --library`,
  `host/m2k_lib_play.c`), counts its cost (`m2k_draw --library`) and measures it (`tools/lib_audition.py`: loudness
  to its category's, one distance over the spectrum, decay, flatness, clarity, width, sustain and brightness; `--level`
  sets VOL and LEVL) into `build/lib_audition/index.json`, for the owner's keep / redo page. A store from before
  (revision under `LIB_REV`) has each slot still holding a factory sound of before (`LIB_OLD`, every commit's since the
  library came in: `tools/lib_old.py`) or nothing take the library's, the owner's kept (the library's for that slot to
  S097 on); `LIB_HASH` holds the library's bytes in the self-test: a library changed is a new LIB_REV and FACTORY_REV,
  its sounds of before into LIB_OLD. LOAD sets a sound's LFOs to the track it goes on.
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

C99, 4 spaces, unity build, static everything, short comments that say why. The screen's grid is the 4 px grid and the
bands above (header bar 0..15, knobs 16..59 under the physical knobs, stage 64..199, steps 204..223, footer bar
224..239), `docs/screen-grid.md`.
