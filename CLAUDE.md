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
  `tools/gen_icons.py [--sheet build/icons.png]` (`--move`: the Move's 36 px marks, `move/screen/m2k_icons_move.h`, SYN's).
- Host: `cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm && build/host/m2k_host build/host`,
  then `.venv/bin/python tools/montage.py build/host build/screens.png 2` to see every page.
  `build/host/m2k_host --selftest` checks the projects (save, reload, store, export / import, full images);
  `-DM2K_TRACKS=8 ... -o build/host/m2k_host8` is the eight-track host (the Move's; the FM-1 and the browser stay
  six, bit for bit): its self-test, `--text-sweep`, and `--render-hash` (bank A, DENSE: the same as the six's).
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
  and a sub-header: the emulator (`index.html`, `app.js`), the manual (`manual.html`, `manual.js`: in the order you make music since 2026-11-16, after a player found the old order erratic: Start here, Make a track (demos, beat, notes, sound, effects, movement, variation), Make a song, Connect, Reference; each page a few numbered steps first, then its sections, `h2` ids an address can name (an old page's id lands on the section its text went to, `MOVED` for the three that became pages; the contents list the page's sections; the Move's manual the same order); one page at a time,
  the machines and knobs read off `m2k.wasm`, the screens `web/manual/*.png` from the host's tour and its manual
  scenes, `m2k_host DIR`, each PBM saved as a 1-bit PNG of the same name), the installer
  (`installer.html`: installs `web/firmware/mono2k.fwsc`, or a `.fwsc` from a file; while it writes, `installing.js` takes the page: the flash's 256 sectors filled from the updater's steps, one percentage, the four steps, MONO2K's isotype at the end; `?demo` rehearses it with nothing sent, `?demo=fail` stops it) and the patch editor (`editor.html`, `editor.js`: the same worklet and
  saves; the FM-1 link's client, the FM-1's patterns a session of their own under `mono2k.fm1.v1`, `.m2ks` store files).
  `panel.js` is the FM-1's layout, `site.js` the subpages' theme and textures. `style.css` opens with the site's tokens on the device's 4 px grid and its components (buttons, cards, tables, accordions, tabs, tooltips, kbd), after shadcn/ui's system; `ui.js` (every page, 2026-11-18) their motion: tooltips, toasts, copy that confirms, number pop-in, error shake, skeleton, the border beam on Install / Download, the screens' mosaic, transitions.dev's free transitions with their comments kept and Libraries.dev's ideas written again; none of it under prefers-reduced-motion. On the emulator page `help.js` says what
  every control, page and layer does (the status line under the panel, the guide's list: from the manual and m2k_ui.c),
  `screens.js` runs copies of the core apart from the one that plays (from `app.js`'s compiled module; a script of
  panel actions each), `guide.js` is How to play, `engines.js` the machines' Hit (`w_audition`, `w_hit` in `m2k_web.c`:
  a machine's INIT at its home note, an FX machine after a plain SWAV-SAW) and `made.js` How it was made's charts (the
  suite's numbers a snapshot of a `regress.py check` run: update them with it). Every push adds what it changes for a
  player to `web/changelog.js` (the manual's Change log: an entry per day of updates, newest first, a kind on every
  line: machines sound sequencer workflow patterns screen web fixes), and bumps `changelog.js?v=` in `manual.html`. Every
  update also adds what it taught to `../FM1-FIRMWARE-HANDBOOK.md` (outside the repo: the owner's handbook of MONO2K's
  and Demake's lessons, for the next firmware), in its section and its Updates log. After a wasm build bump `m2k.wasm?v=` in
  `app.js`, `manual.js`, `editor.js` and `move/move.js` alike (and `worklet.js?v=` in `app.js` and `editor.js` with the worklet).
  The playable Move (`web/move/`): `web/build.sh` also builds `web/move/m2k_move.wasm` (`m2k_move_web.c`: the Move's
  build, eight tracks, on `move/m2k_move_glue.c`, the glue dsp.so shares; MONO2K's own clock, nothing saved); `gear.js`
  draws the Move and plays it (embed and options at its top), `gear-worklet.js` runs the wasm, `gear.css` its look,
  `gear-demo.html` alone. Bump `V` in `gear.js` with each changed file, and `gear.js?v=` where it is embedded.
  The Move's site is `web/move/` (inigohernandez.github.io/mono2k/move/), on the same `style.css`: its home (`index.html`,
  `move.js`: the pads, PERFORM, the tempo, the numbers, and `../engines.js` loaded as the machines come near; `#move-gear`
  for `gear.js` / `gear.css`, the playable Move, with the pads' motion as a picture until it fills), its manual
  (`manual.html` on `../manual.js`, which finds its files beside itself; `changelog.js` the Move version's log) and
  `install.html` (the download "Coming soon" until `RELEASE` is set in its script; set 2026-11-01 for 0.2, 2026-11-07 for 0.4 and 0.4.1). The Move's releases: tags `move-vX.Y.Z` (the version `move/module.json`'s), the package `mono2k-module.tar.gz` (`move/build.sh`; `MOVE_NATIVE=1` on an aarch64 bookworm), never marked Latest (the FM-1 Hub takes Latest), `release.json` at the root (Schwung Manager reads it: the version and the URL), `.github/workflows/move-release.yml` building and attaching on a tag when the release lacks the package. 0.2 released 2026-11-01 at the owner's "release it"; 0.4 2026-11-07 at "it works, merge and publish" and "share the new move firmware with all the changes", its notes everything since 0.2 (0.3 went no further than the owner's Move), its dsp.so the one the owner had just played; 0.4.1 the same day (a jog click turns the jog on a held step to its chance) at "much better" and "yes", its dsp.so again the one played. 0.5 2026-11-16 (the ADSRs, as the FM-1's 1.10) at the owner's "Both now" (the FM-1's and the Move's together), before a Move had run it: the owner's Move was off the network. The same day the owner ran it on their Move: "works very good". 0.6 (POLY on TRK, and the machines' marks on SYN:
  `tools/gen_icons.py --move`, `move/screen/m2k_icons_move.h`, `g_mark` in m2k_screen.c) merged 2026-11-17 with the
  FM-1's 1.11, released the same day with it at the owner's "ready yes" (package sha c4240530, the tested package
  8a9d36f9 but module.json's version), before a Move had run it: the owner's Move was off the network. Its screens are stills of the Move's,
  `move/screens/*.png` (move/screen's sheets' PBMs as 1-bit PNGs, lit in the site's paper on its ink), `move/img/` the pad
  map and the pages' motion. Every page on both has the FM-1 | MOVE switch in its header (`switch.js`: the same page on
  the other device; the editor goes to the Move's home) and `wordmark.js` draws both homes' hero.
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
  The workflow after Chuck Noise's review (1.9, 2026-11-08; ideas from Loom-8's and JIANT's manuals and
  FMS's cells, GPL-3.0 or paid, none of their code): PRESETS is the panel's word, on HOME and GLO the pattern, on every other
  page the catalogue, each sound heard (`cat.quick`: `cat_settle` keeps it at any button or after 3 s untouched, undoable;
  OCT- alone puts the track's own back; a held step's condition and step entry's cursor as ever); `go` opens the page left
  there (`ui.psub`); `knob_speed` scales wide ranges and lists by ms a click (x2 x4 x6; no time between turns, as a script's, x1);
  LFO's pages are LFO 1 2 3 (DEST WAVE SPD DPTH), then 1+ 2+ 3+ (TRCK TRIG MULT PHAS), `lfo_page` a row each with the cable from
  what it sends to where it goes; a DEST from OFF at DPTH 64 starts at `LFO_DPTH_ON` (`lfo_wake`); GLO + LFO arms a link
  (`link_arm`, `link_take`: the next knob of a kit's parameter, any page or track, is the LFO's DEST and TRCK, the first free or
  the one in view; 10 s, LFO, or GLO + LFO again lets go; not LFO twice, which a quick hand turning pages would arm); SELECT on
  LFO's pages LFO 1 2 3, the half kept (an armed link follows it), the track on every other page (the owner's, 2026-11-08); `knob_mod`
  a marker under any knob an LFO moves; HOME in GRID or step entry the track's notes (`note_cells`, four rows under the knobs'
  columns; a Grids lane track keeps the lanes); `step_card` a held step's card after 500 ms, gone while a knob's value shows;
  GLO > HELP the manual on the device (`HELP`, eleven pages, KNOB 1; the tenth POLY's, the eleventh LIVE, 1.11). UI only: `m2k_draw --hash` the same, its page rows at
  their worst for the audit of both cores (the owner's rule, 2026-11-07: every build, the screen's cost included).
  PERFORM latched (1.11, the owner's: "I would like to lock the screen of performance effects some how"): FX
  double-tapped survives every page button (`latch_frees` passes FX's; GLO's, SEQ's and HOME's latches as ever, one
  latch at a time); over the page it was latched on it is PERFORM's own view (`perf_own`: its panel, the bus's knobs,
  OCT its banks), once the page, sub, GRID, PADS or the catalogue changed (`ui.paway`, sticky, `ui.lp_*` the page) the
  black keys stay PERFORM's, the white keys the track's (GRID's steps, lit as such), the knobs, OCT and the screen the
  page's, a steady FX tag in the header; FX held there its own view while held, FX tapped lets go (on the release).
  An effect's key double-tapped (`pf_press`, `pf_released`, `ui.pf_lat`; both devices: the FM-1's black keys, the
  Move's keyboard pads) stays on, lit steady, toast LATCHED; tapped again LET GO; colours stack, a move (`pf_ring`) lets
  go of a latched move as it takes over (KILL stacks); leaving PERFORM (FX let go or its latch let go, the Move's
  PERFORM pad: `pf_unlatch`) lets go of them all, held ones too. A repeat's lap is the sixteenth's to a 4096th of a
  sample (`g_lenq`, Q12 from `bsps`: whole samples drifted 5 ms a minute at 120 BPM). Self-test: PERFORM latched
  (pages, keys, LEDs, the peek, the header), effects latched, a latched RPT 1/16 on the grid after a minute;
  `tools/perform_mutants.py` (check.sh, 13 mutants then); the Move's bench ctl (latch, stack, a move's place, slow taps,
  across pages, the PERFORM pad letting go). Stills 29p (away) and 29q (FX held there).
  PERFORM's bank C (1.12, the owner's picks from AFFINE, `docs/affine-effects-plan.md`; each key a fixed character, no
  format change): `PF_SKIP` .. `PF_LOFI` (22..25, `PF_BANKS` 3; keys 5-11 of C none, dark). FX held: OCT+ steps A B C,
  OCT- back (`ui.pfb` 0..2; the OCT LEDs A OCT-, B OCT+, C both), the panel PERFORM BANK C its four down the left, the
  foot OCT- A B C OCT+. SKIP a ring move (`GM_SKIP`, `gl_skip`, out of line): at each sixteenth (the record's grid,
  stopped a free one) 9 in 10 by `bus.grng` the sixteenth just played repeats 1-3 times, its read slowing 0.45 a slice
  to a quarter (AFFINE's SEEK 0.3), restarts and its way in and out crossfaded over GL_XF, its level the event's
  (`g_fi` 0); `gl_kept` keeps its slice only while it reads. CONSOLE, DUCK and LO-FI (`bus_colour`, out of line): a pass
  over the bus's out after its limiter, run only while one is on or gliding (`bus.c_run`), so off is bank A's bytes and
  costs a test a block: LO-FI an 11 kHz hold whose period wanders +-45 %, 10 bits TPDF-dithered; CONSOLE two TPT
  one-poles 20 to 8 kHz (`T_CON_G`, Q12: their states run past the input), a 10 Hz DC blocker, mono over 93 ms,
  x - 0.1 x^3, held and settled the sum through one side (`co_mono`); DUCK 1 - 0.7 (1 - the beat's place)^3, the dip in
  on an S over 3 ms. The Move: Up swaps both rows (`MV_PERF_TOP[2]`: CONSOLE DUCK LO-FI, the 4th kept for GRIT; SKIP
  the moves' 7th), PAGE 1 / PAGE 2. Self-test: bank C (gestures, LEDs, empty keys), each on A01 (heard, bounds, the dry's
  again let go and unlatched), each law; `perform_mutants.py` 31; the bench ctl's page swap. Cost a sample while on
  (`m2k_draw --perform`): SKIP +82 (a repeat's +60), CONSOLE +45 (settled), DUCK +47, LO-FI +27; off 0. Still 29f_perform_c.
  An arp a track (1.6.1, the owner's: "keep HOLD when switching to another track. So each arp per track"): each track
  its own keys, held notes, HOLD and clock (`sq.keys[t]`, `held[t]`, `arp_hold[t]`, `arp_block`), the panel's keys the
  track in view's, MIDI's their channel's; switching tracks lets go of the keys (a track without HOLD stops, nothing
  hangs) and every held arp plays on; ARP twice the track in view's HOLD, CC 64 its channel's; PLAY starts every arp
  with the pattern, STOP and a pattern coming in keep the holds; HOME a staircase on each lane whose arp holds. With
  REC playing every playing arp writes its notes as played notes are (`rec_step`), the last landing on a step staying
  (a 1/32's on-beat note).
  POLY (1.11, the owner's; `poly_*` in m2k_seq.c, `pv` in m2k_dsp.c): ARP's MODE OFF POLY UP DOWN U/D RND ORDR (POLY
  the pattern's `G_POLY1` + t, the voices 2..8, format 24; the arp's P_AMOD as ever, OFF under POLY: `amod_value`,
  `amod_set`; the last track passes POLY by, said), KNOB 2 VOIC (`K_PVOI`, 2 to the tracks after it; not lockable, no
  LFO target: a global), RNG a free knob; on the Move TRK's knobs 3 and 4 likewise; on the FM-1 also GLO + the black
  keys printed MONO and POLY (A#4 and C#5, the 8th and 9th: `BK_MONO` `BK_POLY`, the owner's), POLY three voices, MONO
  POLY off leaving an arp running, lit while GLO is held, undoable. The lead borrows the tracks right after it, left
  to right (a track inside a lead's voices: its own POLY waits): each voice a whole track rendering the lead's machine
  (`rmach`, set each block, so `track_block`'s code stays as it was: a voice-machine lookup there cost +20..60 Mac
  instructions a sample in bank A by its code alone) and its `eff` (`poly_eff` a block: every knob, the lead's step's
  locks; its LFOs aimed at the lead aimed at the voice, others off on it; `poly_lfo` at a voice's trig: their phases
  the lead's), its own kit, steps and settings kept. Another track's LFO aimed at the lead moves its voices too (`fanq`,
  apart from the LFOs' own queue, untouched). Keys (`poly_key_on`), MIDI on the lead's or a voice's channel and the
  Move's pads take a voice (`poly_pick`: the one holding the key, a free one let go longest, past VOIC the oldest
  note's; a voice the CPU guard rests or is cutting, `held` / `park`, only when every other is); ARP twice (the Move's Loop, CC 64) latches the chord until a key after all were
  let go. Recorded (`poly_note`: live REC, keys within 62 ms one chord; step entry, keys held together; the Move's
  Capture, a step's notes): sorted, the lowest the lead's step, then T+1's..., past VOIC dropped. The sequencer fires a
  voice only from its lead (`poly_fire`, the lead's step and clock): where the lead has a trig, its condition, PROB,
  trig type, accent, slide, ratchets and locks for every voice's note there; where it has none, a voice's own trig on
  its own step's condition (the lead's loops), in the lead's sound. A voice follows the lead's mute and solo (its own
  keys act on the lead: `mute_to`, `solo_key`); never in view (`track_to`: the lead, said; SELECT passes it); HOME and
  GLO > PLAY name its lead, the Move's HOME brackets it. Its notes out on the lead's channel. A change of the leads (MODE,
  VOIC, a pattern) lets go of what the tracks it touches played. Without POLY none of it runs past one check a block
  (`pv.on`): bank A, DENSE and FXS render bit for bit (`m2k_draw --hash`), `--cpu` at its noise. `poly_lead_of`,
  `poly_oldest`: the CPU guard's to ask, `poly_steal` its one change (a cut voice's key freed). Fixtures: `host/m2k_poly.c` (`m2k_draw --poly-cpu`, `m2k_host DIR
  --poly-demo`).
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
  until 1.5, their rows kept in `bus_v8.json` / `bus_v16.json`, their conversions held by the self-test alone); the FM-1's CPU guard (the
  guard bench plain and under the sanitizers, `tools/guard_mutants.py`: ~4 min).
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
  in the browser, `--fm1-store`). The flash chip (1.9.2): every FM-1, the Puya `0x856014` and the newer units'
  UC25HQ80 `0xB36014`, accepted in one place (`FL_JEDEC_OK`, `hal/fm1_flash.h`) by the app, the rescue and the update
  loader. An unknown id writes nothing and is shown ("FLASH ?? id"). A busy chip gets 2 s an erase and 50 ms a page,
  then a reset (66h 99h) and one more try, and never XIP while busy. `host/fm1_flash_test.c [OLD NEW]` runs the
  driver on a simulated chip (both ids, slow, overrunning, stuck), the store through it, and with two packages an
  update (the app's staging, then the loader) from OLD to NEW on each. The update's screen (1.9.3,
  `src/upd_screen.c`, MONO2K's type through `lcd.c` alone): the app's `ota_show` and the update loader both draw the
  app area's 143 sectors filling, a box while the loader waits or after a stop, the isotype at the end; the loader sets
  the panel up itself after one command that must not time out, else it draws nothing. `host/upd_screen_draw.c DIR`
  draws every state (check.sh runs it). The core is its own unit there (`core.o`): reach it
  through `m2k.h` alone. RAM: 96 KiB `.bss` + 336 KiB pool; the FM-1 keeps one project of 64 patterns, two of them
  unpacked (`M2K_PAT_SLOTS` 2: `PAT(i)` is pattern i everywhere, a cache over the working image on the FM-1; reach
  patterns only through it, and never hold two `PAT()` pointers to patterns other than the open one), the rest
  packed in a 46 KB image, a blank one taking no bytes; a pattern opens only if the open one keeps room to grow
  (`pat_can_open`). `M2K_PATTERNS`, `M2K_PAT_SLOTS`, `M2K_PROJECTS`, `M2K_IMG_DATA` are set by build.py; check.sh holds
  bank A through the cache to the browser's, byte for byte. Never publish a package on the site that has not
  run on an FM-1, unless the owner asks: then it goes in `web/firmware/` (the package and `mono2k.json`, its file,
  SHA-256, identity, version, date and a note the installer shows); `sh web/build.sh` after that record, since the wasm's boot screen reads its version from it (missed at 1.6.2 and 1.8). 2026-10-24: `mono2k.fwsc`, 0.1, FM-1_980, the
  new bank, published at the owner's request (first as a test build, then as the latest at their word) before any
  FM-1 had run it. 2026-10-25: the hub listed v0.1; 1.0 (out of the hub's beta, which marks anything under 1.0)
  released after the owner had tried it on their FM-1. 2026-10-27: 1.1 (formats 16 and 17) on the installer and as
  the GitHub release at the owner's word ("when you fix all of this upload the next version"), before an FM-1 had run
  it. 2026-10-28: 1.2 (the new screen, the arp's HOLD) on the installer and as the GitHub release at the owner's word
  ("when you finish upload everything to website, github and for firmware hub"), before an FM-1 had run it. 2026-10-28 later: 1.3 (Grids, mutes on
  the bar, finer chances) likewise, at the owner's word ("Publish 1.3 now"); 1.3.1 (the splash's morph, the windows, FILT) at "looks good
  when you finish publish everything please"; 1.4 (PERFORM's second bank) at "publish"; 2026-10-29 the owner installed 1.4 on their FM-1: "it works"; later that day 1.4.2 (FILT's one curve, the cores split by any set, the screen paced by the sound and calm by default) ran on their FM-1 ("it works") and was published at their word ("Publish 1.4.2 now"); 1.4.1 was never released on its own. 2026-10-30: 1.5 (the new screen on the 4 px grid in Silkscreen, SELECT track / ALGORITHM machine / EDIT held catalogue, per-track lengths (format 19), the bus cut, the 96-sound library) on the installer and as the GitHub release at the owner's word ("upload"), before an FM-1 had run this build (earlier 1.5 tests had been sent). 2026-10-30 later: 1.6 (the owner's pixel font and Elektron-style knob pictures, HOME 2, PROB (format 20), the FM-1's CPU meter and CPU FULL, an exact engine pass) at the owner's word ("all good go ahead"), before an FM-1 had run it; the font's adaptation relicensed CC BY-SA 4.0 at the same word. 2026-10-31: 1.6.1 (the bus a block behind on the second core, the split by measure, an arp a track, the arps recorded) at the owner's word ("finish and push everything"), before an FM-1 had run it. 2026-11-01: 1.6.2 (no clicks on legato notes and accents, VO-6's level on short notes, long delays' first echo on time; the FM-1 still six tracks, `M2K_TRACKS` 8 only on the Move) at the owner's word ("yes"), before an FM-1 had run this build (a 1.6.2 test was sent). 2026-11-02: 1.7 (the CPU guard, CPU FULL naming the tracks, the manual's costs, the Hub's card picture from `fmhub.json`'s `image`) at the owner's word ("1 and 2 good for 1.7 can you publish"), FM-1 only, before an FM-1 had run it. 2026-11-07: 1.8 (a note's length, `span_tap`) at the owner's word ("it works, merge and publish"); a test build of the same code (0.0, sha cbf4829b) had been sent that day, and this file itself, version set, has not run on a device. 2026-11-08: 1.9 (the workflow after a player's review: PRESETS the sounds, pages remembered, knob speed, LFO's rows and GLO + LFO, HOME's note cells, a held step's card, GLO > HELP) at the owner's word ("it works, merge and publish") after a test build of the same code (sha 9aa60543) ran on their FM-1; mono2k-1.9.fwsc (sha 87c680b7) differs from it in 19 bytes, the version and checksums, and has not run on a device itself. 2026-11-09: 1.9.1 (SELECT picks LFO 1 2 3 on LFO's pages) at the owner's word ("yes") after its test build (0.0, sha 0a27b414) ran on their FM-1 ("work"); mono2k-1.9.1.fwsc (sha 4fb65f62) is the same code: a longer version moves the image (8293 bytes from the test build), so it was built again as 0.0.0, 22 bytes from 1.9.1. 2026-11-15: 1.9.2 (every FM-1, the newer units' UC25HQ80 flash `0xB36014` too: one chip check in the app, the rescue and the loader, slower chips waited for and reset, the watchdog and the installer's wait kept; PR 12) at the owner's word ("Tested, publish it") after their hardware tests; mono2k-1.9.2.fwsc (sha 55281dc2), the package they were given. 2026-11-16: 1.9.3 (the update on the FM-1's screen: the app's staging and the update loader draw the sectors filling, the waiting and stopped boxes, the isotype; the Installer's own screen while it writes, `installing.js`) at the owner's word ("when you finish upload"), before an FM-1 had run it: its loader drives the panel for the first time, and only the host's drawing and update test have run it. The same day the owner ran it on their FM-1: "1.9.3 runs on my FM-1". 2026-11-16 later: 1.10 (the amp's and the filter's envelopes as regular ADSRs: SUS, FSUS, FREL, format 22/23; SUS and FSUS at 0 the envelope as before, bank A rendered the same) at the owner's word ("all good, publish") after its test build (0.0, sha 1dd596e6) was sent; mono2k-1.10.fwsc (sha 075e231e) is the same code: built again from the merge, the test build gives 1dd596e6 to the byte. 2026-11-17: 1.11 (POLY, the calmer CPU guard, PERFORM latched across pages and an effect latched by a double tap, repeats on the exact step) at the owner's word ("ready yes") after its test build (0.0, sha dda5d944) was sent; mono2k-1.11.fwsc (sha c01a4ed6) is the same code: built again from the merge, the test build gives dda5d944 to the byte, and a 0.00 build differs from 1.11 in 34 bytes; it has not run on a device itself. The installer page (`installer.html`, Felucca's `web/fm1/fm1ota.js`, `fm1pkg.js`, unchanged) installs
  a package from a file meanwhile. 1.11 (POLY, and a calmer CPU guard, a chord's voice its rung between a tail and a
  rest; one change log entry, 2026-11-17) was merged from its two branches on 2026-11-17 and sent as a test build;
  not released.
- The FM-1's budget: Felucca's heaviest patch is ~1570 Mac instructions a sample (its tests/cpu_baseline.txt) and
  runs on one core; count MONO2K's the same way, built as the FM-1 builds it (`cc -O2 -fno-builtin -fno-vectorize
  -fno-slp-vectorize -DM2K_MUL_OUT`, the 64-bit products out of line; `proc_pid_rusage` ri_instructions). The core's
  byte loops (m_set, m_cpy) are word-wise for it.
  Two cores: the render hands the second any set of the tracks (`par_mask`) through `m2k_par_begin` / `m2k_par_end`
  (the FM-1's glue fills them with CS-1's audio worker; elsewhere they are unset and the core does it all), every FX
  machine kept with the track it hears; the self-test renders bank A with the second core's tracks first and last,
  and bank A, DENSE and six FX-chained tracks with every set there (`par_force`), each held to the one-core render
  sample for sample. Since 1.6.1 (the owner's: their dense pattern read CPU 97 % / CPU2 74 %, DROP climbing) the bus
  plays a block behind (`brec`, m2k_fx.c): block n's tracks sum into its record, and block n - 1's bus plays from the
  record before on the second core, before its share of block n's tracks (on one core, before them), so the sound comes
  out 16 samples (0.36 ms) later and otherwise as it was (the owner's accepted edges: that, and a PERFORM key between
  two renders reaching the bus a block early). What the bus reads of a block goes into its record (the tempo `bsps`,
  its steps for the grid `bus_step_q`, the pattern's controls, the master, running), DUB's line runs on the bus's side
  (`dub_snap`, its track's send and settings recorded, `delay_run`), MIDI out is stamped a block on (`mx_out`);
  `m2k_draw --hash 16 441088` against an older build's `--hash` proves the renders the old ones 16 samples later, and
  `host/m2k_ref` writes from sample 16 (a block more rendered), so `regress.py check` prints as before. The split
  minimises the slower of the first core's tracks and the bus plus the second's, weighed by `m2k_clock` where the
  platform sets it (the FM-1's ticks: each track's block and delay, the bus and the first core's own part timed where
  they run, smoothed, `par_meas`; estimates scaled to it until a track is measured or after its machine or stages
  change; the host and the browser, no clock, on the estimates: the same every time); GLO > PLAY a bar of each
  track's load. `m2k_draw --split` simulates the split by measure with the instruction counter as the clock. The glue paces the screen by the sound (a frame waits while the ring is under half full,
  frames further apart while the render is busy). The meter, always (1.6): every half second the glue
  (`meter_tick`, m2k_fm1.c) hands the core `m2k_cpu(first core %, second's or -1, the ring's shorts since power-on)`;
  GLO > PLAY shows CPU, CPU2 and DROP (dashes where none came: the browser, the host), and a short more than the last
  toasts CPU FULL (at most every 5 s). The CPU guard (1.7, the owner's, after a player's "running out of CPU juice
  fast with some audio dropouts"; 1.11 calmer, the owner's after playing 1.10: "a bit intense cutting the track ...
  give more room and also give the option to which one and/or be smarter"): before every render the glue
  (`m2k_pace.h`, firmware/fm1, shared with the bench: `pace_guard`, `pace_rendered`, `pace_busy`, `pace_frame_due`)
  hands the core `m2k_guard(render load %, blocks ahead, RING, shorts since power-on)`; the core judges the load less
  what its last steps will still take off the ~8-render average (`grd.credit`, a step's saving `grd_saving`: its
  voice and chain's share of `trk_cost`, half on two cores). Danger: over (the render over its real time, the ring
  empty within GRD_SEC / 8 at that rate), tight (over 95 % with a third left: the screen gives way first,
  `m2k_guard_gap` 200 ms between frames for 2 s, the sound only if still tight 0.1 s on), a short the render's (over
  90 %; over 80 % a second episode within a second, or one that has not refilled the ring in a third of a second;
  episodes end when the ring is half full again). A short at less (a flash write, a stall) only moves the screen. In
  danger one step, then GRD_SETTLE (4) renders: a tail first (`grd_tail`: a note ended that only rings, astage 4 or 3
  past its gate, or its delay's echoes alone, `dcut`, the line faded over PARK_N and wiped), the quietest for what it
  saves, not resting (`trk_park(.., hold 0)`; `env_park` gives way to a note that comes in its fade); then
  `guard_voice_steal()`: a voice of a POLY chord faded (`trk_park(.., 0)`, its next note as ever; a chord on the
  keys gives up `poly_oldest`'s, the note POLY would give up for a new key, freed by `poly_steal`; a chord from the
  steps, its notes started together, the quietest of its voices for what it saves, never the lead's; none through its
  attack, `env_park` giving way to a note's start; a chord keeps its last voice; the chord in view last); voices cut
  and a short within a second after (a chord's next onset brings them back: `grd.vfail_t`), tracks rest first for 2 s;
  then a rest (`held`), only when the danger is over / a
  short / the ~64-render load over 100 %: REST's (`S_REST`, GLO > PLAY KNOB 2: AUTO, T1..T6 first, NONE never; an
  old store's 0 AUTO; the browser's MOTN on KNOB 3), AUTO the quietest for what it costs by `grd.lv` (its `lvl` held
  ~0.4 s, `GRD_FALL`, then followed ~1.5 s: a hit a beat counts as loud, a rare one little), never the track in view.
  Back from its next trig once for a second (`grd.calm`, doubled up to 8 s for one back only to rest again within
  4 s) nothing was in danger and the ~64-render load with its cost was under 85 %, the ring half full, the last to go
  first; the track in view back at once unless REST names it. CPU FULL says what it did (`T2 TAIL CUT`, `T5 VOICE
  CUT`, `T2 RESTS, T4 HEAVIEST`; a rest after a tail's cut at once); GLO > PLAY's cells REST, and CUT 1.5 s. Never called elsewhere
  (the browser, the host, the Move): the sound bit for bit (`m2k_draw --hash`). The guard bench
  (`host/m2k_guard_bench.c`, check.sh): the FM-1's ring, interrupt, main loop (frames, strips, flash writes, stalls)
  and guard on the core's own render timed by its cost model through `m2k_clock` (DENSE's render = the FM-1's real
  time), 1.7's rule copied in for the comparison, no guard too; traces near the limit, spikes, a ramp, bars heavy and
  light, a fall, stalls at 70-85 %, bank A as it is and 15 % slower, DENSE, REST T3 and NONE, a POLY lead of six SID
  voices (`m2k_poly.c`'s) over the limit, its chords from the steps and held on the keys at ~115 % and from the steps
  at ~190 %; it holds bank A never shedding, shorts never above 1.7's, a stall cutting nothing, no cascade, no more
  rests or longer, a tail first, the return, REST, a POLY chord's voices cut holding 115 % with no rest, at 190 %
  rests after them; `--wav DIR` writes each run, `-v` each step. `tools/guard_mutants.py` (check.sh): 28 mutants of
  the guard, its fades, POLY's side and the glue, in copies under build/guard_mut, each caught by the bench or the
  self-test's GUARD and GUARD POLY (the ladder, the fades without a step, a note in a tail's fade, REST, AUTO's level,
  CPU FULL's words; the voice cut called, not resting, its key freed, never the lead's of a chord of steps, a chord's
  last kept, `poly_pick` past what the guard cuts or rests).
  `--diag` packages show the same CPU, W (the second's) and F (drawing) in %
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
  2284 (A04), 3181 -> 3062 (DENSE). 1.6.2's declicks cost +3 (A01) to +7 (DENSE) one core, the wall +2..4: none of
  them runs per sample where nothing changes (a crossfade only through its 2.9 ms, the accent's ramp a block of its own).
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
  byte; the blob's head is 60 (BLOB_MAX 3608 until 22, 3626 since, editor.js's too). 21 eight tracks (`M2K_TRACKS` 8, the Move's build):
  20's layout eight wide, only for a pattern that uses T7 or T8 (`pat_wide`: anything not `track_blank`'s, or an LFO's
  TRCK 128/129 aimed there); else it packs as 18-20, byte for byte. A build of six reads 21 as its first six (an LFO
  aimed past them off, `trck_past`), a build of eight reads 18-20 with T7 T8 blank (SID, GND-PERC INIT, no trigs);
  the self-test holds both ways on `FMT21`, the eight build's bytes. 22 the amp's and the filter's ADSRs (the owner's,
  2026-11-16, after a player's "why not using regular ADSR?"): `P_SUS` `P_FSUS` `P_FREL` after the bank (71..73,
  `P_COUNT` 74; `P_COUNT_V21` the kit before), 20's layout with kits 74 wide, only for a pattern that uses them
  (`pat_adsr`: a kit's SUS FSUS FREL off 0 0 32, a lock on one, an LFO's DEST or a DEST lock on one), else it packs as
  18-21, byte for byte; 23 is 22 eight tracks wide (21's layout, kits 74), in a build of eight. An older format loads with
  them at 0 0 32 (`kit_v21`), their locks dropped and an LFO's DEST 72..74 (OFF there, past its 71) still OFF, as an
  older build reads it; the self-test holds 22 and 23 both ways (FMT21 widened). The store keeps a sound's record as
  before (`SND_REC` 80) and the three of every slot in a section of their own after 'R', `'A'` count, 3 a slot, only
  when a named sound uses them: a library without them stores byte for byte as before (LIB_HASH, LIB_OLD, no LIB_REV,
  FACTORY_REV or store version), an older build reads it without them; a store without 'A' reads its sounds at 0 0 32.
  24 POLY (1.11): `G_POLY1` + t after the chances, 22's layout (kits 74) with it, only for a pattern with a track on
  POLY (`pat_poly`), else it packs as 18-22, byte for byte (1.10 reads those); 25 is 24 eight wide, in a build of eight
  (`pat_wide` sees a POLY on T7 or T8). An older format loads with none; a voices' byte of 1 is none, past 8 is 8
  (`pat_clamp`); a build of six reads 25 as its first six, a lead's voices clamped to the tracks there. BLOB_MAX 3632
  since 24 (editor.js's), BLOB_ANY 4320. The store, the library and the sounds unchanged (POLY is the pattern's).
  The engine: `env_block` as it was where SUS and FSUS are 0 (and no LFO on them), bit for bit (`m2k_draw --hash`), one
  check before it (its cost at `--cpu`'s noise, +0..9 Mac instructions a sample: inlined, a fuller env_block cost +20);
  the rest out of line, `env_adsr`: SUS past 0 the decay
  falls on DEC's law toward (SUS / 127)^2 of full (`T_SUS`, Q30; shown in dB, `T_SUS_DB10`), held while the note lasts,
  the note's end (a key let go, the step's gate run out) the release from wherever it is, in any stage (HOLD 127 and
  DEC 127 as ever: full while the note lasts); FSUS past 0 the filter's decay stops at FSUS / 127 of full, held, the
  note's end a straight release on FDEC's law timed by FREL (127 stays; 32 by default), for a FILTER trig in a note
  (`fnote`; one with the gate shut plays its AD). The pages (the owner's): AMP 1 ATCK DEC SUS REL, 2 DIST VOL PAN PORT,
  3 HOLD; FILT 1 BASE WDTH HPQ LPQ, 2 ATCK DEC SUS REL, 3 B-OF W-OF (`AMP_K`, `FILT_K`; the Move's AMP and FILT two
  of them a page, AMP+ and FILT+ its pad again); no CC for the three; LFO targets 72..74. Older ones are
  converted on load. The
  Monomachine's tempo is set on its panel (TEMPO, the LEVEL knob a BPM a detent; `chain_hold46.panel`): the loop and
  spec modes leave it at 120.
  1.6.2 (the owner's "fix it"; the OS image was not on the Mac, so from the stored calibration): a trig over a note
  still heard (the amp over -60 dB; an ATK 0 hit only over a track heard past -40 dBFS, `XF_HIT`: under it the hit
  masks the step) on a machine that restarts at a trig (the FM+, DIGI-WAV, SWAV-PULS, SWAV-ENS, SID-6581's PWRS sweep,
  GND-SIN) plays its old state on 2.9 ms, crossfaded into the restarted one (`xf_block`: the machine twice, a smooth
  step, the old from no louder than it was heard, SWAV-ENS's chorus line given the crossfade), MONO2K's own declick:
  the Monomachine restarts them at every trig (chain_fm26, ens18, pul38, sid36). Its GND-SIN runs free (chain_kick40)
  where MONO2K's starts at zero as calibrated; run free on legato alone it moved bank A's basses against their kicks
  (A04 0.8 dB louder), so it crossfades too. A fresh note, an ATK 0 hit over a quiet tail and the drums restart as they
  did. FX-DYNMX's compressor and VO-6's gain follow on through such a trig (VO-6's new vowel glides 11.6 ms, `VO_GL`);
  a fresh VO-6 note rises 4 x as fast as the Monomachine's fade (`VO_UPT`: a 1/16 at 120 at its level by ~40 ms;
  `vo47_lv_064_064` XFAIL, the owner's departure). An accent coming or going on a sounding note ramps across its block
  (a pre-pass in `track_block` on that block alone: what VOL scales, by acc_i / acc). A silent delay line takes its time
  at once (`dquiet`): the slowed clock's glide had its first echo up to 19 ms early (delay_time PASS since; `d2_wid_032`
  XFAIL, 3.2 dB on a 3 dB tolerance). Self-test: LEGATO (the high band about a trig against the same trig without its
  AMP part), ACCENT, VO-6, DELAY. Bank A: its legato tracks' clicks gone (A04's GND-SIN bass 18 dB less over 2 kHz),
  levels and the rest as they were, the drums and hits sample for sample.
  A note's length (the OP-XY's gesture, the owner's 2026-11-07; `span_tap`, `span_of`, m2k_ui.c, the FM-1's GRID and the
  Move's steps alike): a step held past a tap (`TAP_MS`), a later empty step pressed: two of the step's locks, GATE
  (16 a step, 8 steps at most) and HOLD 127, the amp open that long whatever the kit's HOLD and DEC; on a sound with a
  sustain (SUS past 0, format 22) GATE alone, the ADSR holding its sustain that long. No format change.
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
