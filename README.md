# MONO2K

**Six mono machines for the M-VAVE FM-1.** A Monomachine-style synthesizer firmware: six tracks, each one machine,
a 64-step sequencer with parameter locks, trig conditions and ratchets, and a lofi bus that breaks the sound
beautifully. A 1-bit screen: dithered type and machine marks, Swiss grid, PS1-era glitches.

> **Status: 1.0.** The core (sound, sequencer, screen) runs in the browser, on a computer and on the FM-1. The FM-1
> build (`firmware/fm1/`: the open FM-1 platform around this core) keeps one project of 64 patterns (bank A the
> demos, B–D blank). MONO2K 1.0 for the FM-1 installs from [The Firmware Hub](https://thefirmwarehub.com) or the
> [installer](https://inigohernandez.github.io/mono2k/installer.html).

**Try it in your browser:** <https://inigohernandez.github.io/mono2k/> (the firmware's own core, compiled to
WebAssembly, with sound).

## On the FM-1

- **What it keeps.** MONO2K saves into its own store, the last 128 KiB of the FM-1's data area (`0xC0000`–`0xE0000`):
  one project of 64 patterns and its sound library. It does not read the projects, presets or samples of other
  firmwares, and its first save overwrites whatever another firmware kept in that area. Back up with the firmware
  you are on before you switch.
- **Back up MONO2K.** The [Patch Editor](https://inigohernandez.github.io/mono2k/editor.html) reads the FM-1's store
  over USB (Chrome or Edge) and saves it as a `.m2ks` file; it writes one back the same way.
- **Leave it.** Install another firmware as usual (the FM-1 Firmware Hub, or its maker's installer): MONO2K starts the
  new package's update loader, as Felucca does, whose loader it carries.
- **USB.** It shows up as a MIDI device named "MONO2K FM-1" and answers the identity FM-1_980. While it updates, the
  update loader shows up as "Felucca Update", the loader being Felucca's.
- **Rescue.** Hold OCT− while switching on: the FM-1 starts the updater only and waits for a package over USB.

## Run it

```bash
sh web/build.sh
```

```bash
python3 -m http.server 8742 --directory web
```

Then open <http://localhost:8742>, press Start, click the panel and play (Z… for keys, Space PLAY, A GRID, 5 SYN).

Host build (renders the factory patterns to WAV, tours the pages and saves the screens as PBM; with `--analyze`, renders
every pattern and solos every track, flagging what is too quiet, clicky or clipping):

```bash
cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm && build/host/m2k_host build/host
```

## What is in it

| | |
|---|---|
| Machines | GND-SIN, GND-NOIS, GND-PERC, SID-6581, FM+STAT, FM+PAR, FM+DYN, SWAV-SAW, SWAV-PUL, SWAV-ENS, DIGI-WAV, VOX-6, FX-CHORUS, FX-RINGMOD, FX-DYNAMIX (on the track before): clean-room designs after the Monomachine's families, integer only. DRM-KICK, DRM-SNAR, DRM-HATS, DRM-CYMB: drums after the LXR's voices, on its laws |
| Per track | the Monomachine's chain and pages: SYN A–H · AMP (ATK HOLD DEC REL DIST VOL PAN PORT) · FILT (HP / LP as base and width following the note, resonance, envelope) · FX (EQ, SRR, a delay of its own in 8-bit mu-law, ping-pong on positive sends) · DSTR (bits, fold, xor, SPU send) · 3 LFOs (any track's parameter) · ARP |
| Projects | the modern Elektron way: 4 projects × 4 banks (A–D) × 16 patterns (on the FM-1, one project of 64), each pattern with its own sounds. SAVE keeps the pattern (its reload point), GLO + SAVE reloads it, SAVE held keeps the project; GLO + REC / PLAY / SEQ copy, paste, clear (the pattern on HOME, the track's sound on its pages); GLO > SET picks and loads a project. On the web: a project picker, export / import as `.m2kp` files. Stored packed (a typical pattern 0.5–1 KB; the DEMO project 32 KB) |
| Factory | project DEMO, bank A: four parts, CIRCUIT, TIDE, SUSPEND and LANTERN (`docs/bank-a.md`: how they were made, key and idea of each), every sound picked by the owner from sound sheets; A05–A16 and banks B–D blank. A store saved with earlier demos takes the new ones where the old still stand |
| Sequencer | 6 × 64 steps, note / accent / slide, parameter locks, the Monomachine's trig types (each step trigs the amp, the filter's envelope and the LFOs independently: step + VOL, ALL ... NONE, a trigless lock), trig conditions (chance, A:B, 1ST, PRE, FILL), ratchets, Grids' lanes (Mutable Instruments' map), each track's own length (polymeter, 1-64 steps) and speed, mutes on the bar, swing, live recording and step entry (REC while stopped), pages held to set the length, patterns queued at the bar |
| Bus | GLO > BUS: a feedback-delay-network reverb at 22 kHz (DCAY, RT60 0.3–24 s; DAMP, the top's decay 1/1–1/30), a 4-bit ADPCM crunch (ADPC) and a drive into the soft clip (DRIV), each on a law measured over its whole range; PERFORM's 22 effects over them (repeats, reverse, stop, stutter, crush, wow, freeze, scatter, octaves, filters, dub, shimmer, spin), every change crossfaded |
| MIDI | tracks 1–6 on six channels from GLO > MIDI's CHAN (1–6 at first), the others play the track in view: notes and the Monomachine's CC map (SYN 48–55, AMP 56–63, FILT 72–79, FX 80–87, LFO 1/2/3 on 88/104/112, level 7, mute 3). SYNC IN follows a MIDI clock (its tempo, a step every sixth pulse, the swing kept; start, continue, stop) from USB, the TRS jack or both, SYNC OUT sends one (24 a beat, start, stop), NOTES OUT each track's trigs on its channel. GLO > IN: where notes and CCs come from (both, USB, TRS, none), and a thru of the jack's MIDI out over USB. The emulator picks a MIDI device in (as USB) and one out |
| Screen | 240 × 240 at 1 bit (a 7.2 KB framebuffer): the owner's pixel font (Elektron Pixel Font, adapted) at whole scales (2x and 3x: crisp on the FM-1), each knob's value as a picture (dials, sliders, boxes, waves, curves) after Elektron's parameter pages, the Orbitron wordmark, Model:Cycles-style machine cards, machine marks as one family of isotypes on a 12-unit grid, each moving in its own way in time with the pattern, dissolves, glitches, live envelopes, filters, LFOs and scopes; light or dark (GLO > SET > SCRN; on the web it follows the page's theme) |

## Where things are

- `firmware/src/m2k.h`: the API the platform calls (render, keys, buttons, encoders, MIDI, frame, LEDs, store).
- `firmware/src/m2k_core.c`: the unity build; `m2k_dsp.c` machines and the track chain, `m2k_fx.c` delays and the bus,
  `m2k_seq.c` sequencer, `m2k_factory.c` the factory patterns (a step grammar), `m2k_proj.c` projects, banks and the
  store, `m2k_gfx.c` 1-bit drawing, `m2k_ui.c` pages and controls.
- `tools/gen_tables.py`, `gen_font.py`, `gen_icons.py`: the tables, fonts and marks, into `firmware/gen/` (Pillow and
  numpy in `.venv`).
- `web/`: the site: the emulator page, the manual, the installer (MONO2K for the FM-1, the same file as the latest GitHub release; it is on The Firmware Hub too) and the patch editor; `m2k_web.c` is the core plus exports, `worklet.js` runs it, `app.js` draws the panel, `manual.js` and `editor.js` read the machines and their knobs off the core.
- `host/m2k_host.c`: the host harness.

## The next steps

1. **On the FM-1** (`firmware/fm1/README.md`): MONO2K 1.0.1 is released, on the installer, as GitHub releases and on
   The Firmware Hub. Next: the CPU load read on the device with a `--diag` build (A03 SUSPEND and A04 LANTERN are over
   Felucca's yardstick on paper), and a store that survives a power cut during a save (two slots in the flash).
2. **Calibration against the Monomachine**, machine by machine: levels, spectra, the machines' own envelopes. The amp
   envelope, SWAV-SAW, SWAV-PUL, SWAV-ENS, the three FM+, SID-6581, GND-SIN, GND-NOIS, FX-RING, VO-6's voice and the
   whole track chain are done (below); VO-6's consonants and V-SW's switch are next. Driven hard, all of them meet its
   ceilings.
3. More machines (DPRO-BBOX, FX-REVERB and the rest of the FX family) and songs.

## Author

MONO2K is made by Sequencist: [GitHub](https://github.com/InigoHernandez) ·
[Bandcamp](https://sequencist.bandcamp.com/) · [YouTube](https://www.youtube.com/@sequencist) ·
[Instagram](https://www.instagram.com/sequencist.fm) · [SoundCloud](https://soundcloud.com/sequencist)

## Calibration

MONO2K's machines are its own, but they are measured against the Monomachine. `tools/ref/mm_ref` boots an owner's
Monomachine OS image in [gearmulator-md-mm](https://github.com/joelanders/gearmulator-md-mm)'s emulation and plays it
over MIDI only, as the instrument is played; `host/m2k_ref.c` plays MONO2K the same way; the scripts compare them. The
OS image is read from the path given and never copied: no ROM, no ROM code and no recordings of it are in this repo.

```
cmake -S tools/ref -B build/ref -G Ninja -DGEARMULATOR=/path/to/gearmulator-md-mm && cmake --build build/ref
build/ref/mm_ref /path/to/os.bin render build/ref/mm < tools/ref/amp.spec > /dev/null
build/host/m2k_ref build/ref/m2k < tools/ref/amp.spec
.venv/bin/python tools/ref/envtimes.py build/ref/mm build/ref/m2k
```

Done so far, all measured on OS 1.32B:

- **The amp envelope.** The attack rises in a straight line in 0.90 ms × e^(0.0814·v), decay and release fall 20 dB
  in 8.3 ms × e^(0.0665·v), the hold lasts v / 32 of a beat at the tempo (32 is a beat, 8 a sixteenth: measured at 60,
  120 and 240 BPM within 0.3 %, `tools/ref/chain_hold46`, group `hold`; it was a fixed 14.6 ms a step until format 13,
  fit at one tempo); a key let go ends the hold and the decay. MONO2K followed within a few percent from 0 to 127 until
  pattern format 16, when the owner chose laws of MONO2K's own for ATK, DEC and REL (gen_tables.py): the knob spent
  where the times are useful (ATK 0.4 s at 64, 10 s at 127; DEC and REL 0.8 s at 64, 20 s at 127), the attack the
  square of its time, the time shown the one heard (ATK to full, DEC and REL to -60 dB). Older patterns convert to the
  value that sounds nearest; the kick cases measured against the Monomachine's DEC are XFAIL for it (group `kick`).
  The LFOs too: SPD a cycle in 2^((84 − SPD) / 12) beats, DPTH squared about 64.
- **SWAV-SAW** on SWAVE-SAW's page: the unison pairs' detune and levels, the square and sine subs, within 0.2 c and
  0.5 dB (`tools/ref/swave_saw*.spec`).
- **SWAV-ENS** on SWAVE-ENS's page (`tools/ref/chain_ens15..20.spec`, `ensacc`): PCH2–4 off its interval table (−12
  to +12 semitones, 61–63 OFF, the just 3/2 4/3 5/4 6/5 beside their equal-tempered neighbours: all 66 group ends
  within 0.4 c); a voice two saws an octave under its note, the second PW/128 of a period on, WAVE fading their sum
  out and their difference in at 0.63 (levels within 0.8 dB over WAVE × PW); no level shared between the voices
  (0.4 dB); TUNE ±2 semitones (0.7 c); CHRL the chorus's mix (within 1.5 dB). Bank A's chords, converted, against the
  Monomachine's voice by voice: within 0.5 c. The chorus is stereo, as the Monomachine's (`chain_ens41.spec`, group
  `ens_stereo`, 12 s held): two copies off a half-rate line, each delayed by a sine at 0.2 Hz swinging ±16.8 c, the
  second a quarter turn on; CHRW sets them apart across the stereo (the left hears (1 + w) / 2 of one, the right of the
  other, w = 0.355 (CHRW / 127)^1.15), each channel at one copy's level. L/R correlation 1.000, 0.991, 0.954, 0.887,
  0.795 at CHRW 0, 32, 64, 96, 127 against the Monomachine's 1.000, 0.990, 0.956, 0.890, 0.787; the copy's side within
  0.5 dB of its. Known gap: at CHRW 32 the Monomachine's channels already swing a quarter turn apart (±16.7 c each)
  where MONO2K's stay closer (28°, ±11.6 c). The side runs through the track's own filters, amp and SRR (linear: the
  clip's ceiling holds it; the fold and XOR, MONO2K's own, leave it).
- **FM+STAT, FM+PAR and FM+DYN** on their pages (`tools/ref/chain_fm26.spec`, group `fm`): a sine carrier on the
  note at half GND-SIN's level, phase-modulated by sine operators side by side, every phase reset at the trig, the
  carrier's half a cycle on from its modulators (as the Monomachine's harmonics' phases read: a ratio-1 modulator
  turned over, a ratio-2 one not).
  - FM+STAT (1FRQ 1FIN 1ENV 1FB | 2FRQ 2VOL TONE TUNE): each ratio on one 24-step table, 1/64 to 4 (the screen's
    labels for steps 5–9 are their neighbours'; the sidebands say which), 1FIN a factor of 0.75–1.25 on the first;
    1ENV its depth up to 64 (index 1.57), over 64 that depth falling from the trig (24 s at 72 to 6 ms at 127); 2VOL
    the second's depth on the same law to 64, then straight on to 14 at 127; 1FB feeds the first operator back on the
    mean of its last two outputs. Both modulators pass two one-poles on their way to the carrier, TONE their cutoff:
    30 Hz at 0, 1.37 kHz at 64 (where a modulator at 523 Hz goes in at 0.88 of its depth, at 2 kHz 0.30, at 4 kHz
    0.10), ×1.61 every 8 steps, open at 127. Depths within 3 % from 65 Hz to 4 kHz and over TONE, the decays within
    0.03 of the index, the feedback within 1.5 dB up to 40.
  - FM+PAR (1FRQ 1ENV 2FRQ 2ENV | 3FRQ 3ENV TONE TUNE), new: three of FM+STAT's operators on the same table, law and
    lowpass; an ENV up to 64 its depth, over 64 the depth 2VOL has at that value, falling from the trig on 1ENV's time.
    Within 0.01 of the index, two at the same ratio adding as one.
  - FM+DYN (1FRQ 1FEN 1VOL 1VEN | 2FRQ 2ENV 2FB TUNE): no lowpass, and 1.125 times FM+STAT's depth law. 1FRQ is v/64
    (2 at 127), 2FRQ (v/64)² (4 at 127). 1VEN over 64 pushes 1VOL up its law from the trig by 4 steps a step (128 at
    most), falling back on a time of its own (6 s at 72 to 4 ms at 127); under 64's depth the push adds straight, twice
    as fast, and stops there. Under 64, 1VEN cuts 1VOL by 1/32 a step, and past half the operator turns over (a cut of
    32 at 1VOL 64 goes to the whole depth inverted and back). 1FEN pushes the first operator's frequency at the trig
    by 1.35 Hz × (v − 64)², the same at every note and added before the ratio, a quarter of that downward under 64,
    falling back on 1VEN's time. 2ENV is FM+STAT's 1ENV, 2FB its 1FB. Depths within 1.5 %, 1VEN's push and cut within
    about 0.1 of the index, 1FEN within 1 %.

  Known gaps: from about 44 of feedback the Monomachine's carrier moves off the note (by 0.14 of it at 48, a quarter
  at 56–72, noise above) where MONO2K's stays harmonic until it aliases; a deep 1VEN cut (under 32) at 1VOL over 64
  recovers through the low depths up to 0.4 of the index apart for a few hundred ms.
- **SID-6581** on its page (`tools/ref/chain_sid36.spec`, group `sid`; PW PWAD PWRS WAVE | MOD MSRC MFRQ TUNE): an
  oscillator on the note, TRI SAW PULS MIX NOIS at one peak level (a square 2.3 dB under full scale), band-limited.
  The pulse's duty is |PW − 64| / 128: a square at 0 and 127, silent at 64. PWAD sweeps PW on from the trig at 2.70
  steps a second a step, round and round; PWRS ON starts it again at each trig. A second oscillator at MFRQ (C3 at 0,
  an octave every 32 steps, the same at every note) or on the previous track's note (MSRC PRCH): SYNC plays it in the
  wave, hard-synced to the note; RING multiplies the two triangles, unipolar, whatever the wave; R+S rings with the
  second synced. NOIS is the chip's 23-bit register clocked by the cycle, MIX the triangle AND the saw. Levels within
  0.3 dB (the 7 dB gap closes), duty and sweep within 0.5 dB, ring and sync partials within 1–3 dB. Known gap: MIX's
  spectrum (the Monomachine combines the two waves otherwise; its level matches).
- **FX-CHORUS, FX-RINGMOD, FX-DYNAMIX** (FX-CHORS, FX-RING, FX-DYNMX on the screen), new, on the Monomachine's pages
  but MONO2K's own designs. On the emulator an FX machine hears the track before only once its KIT > EDIT KIT TRACK
  page has been opened (`mm_ref`'s loop does it); then RINGMOD and REVERB pass it, while THRU, CHORUS and DYNAMIX stay
  at digital silence under every input and knob, so FX-CHORS and FX-DYNMX stay unmeasured, and FX-RING is rebuilt on
  RINGMOD's measurements (below). CHORUS and DYNAMIX take the previous track's output after its amp and before its VOL
  (so VOL 0 keeps a track out of the mix and in the FX), INP its level (100 as it is); every FX opens on its own trigs
  as the Monomachine's FX tracks do (HOLD 127 by default). CHORUS (DEL DEP SPD MIX | FB WID LP INP): two taps swept in
  opposite ways off one line, their mean the chorus, their difference its width added across the stereo after the pan,
  fed back through the first (FB up to +-0.9: at its top a short DEL rings as a comb, the Monomachine's metal clangs;
  it reached only +-0.11 until fixed), the wet lowpassed. RINGMOD (WAVE EXT – MIX | – – – INP), measured on the OS
  (`tools/ref/ring45.loops`, group `ring`: two-track loops, the FX panned away from its source): it hears the track
  before after its VOL, its clip and its LEV, as the Monomachine's FX do (LEV 40 is 15.9 dB down on both, LEV 0
  silent); INP a square law, unity at 45, into a clip of its own that starts near INP 82 (its odd products within
  0.5 dB of the Monomachine's); times a carrier on the note, WAVE's cycle built from the Monomachine's measured odd
  harmonics 1..15 (none even at any WAVE): a sine, a triangle by 64, a square 10.4 dB down by 127, each harmonic
  within 0.1 dB; MIX a linear crossfade; EXT fades the carrier out linearly, on the Monomachine to its audio input,
  here to the track before the input (the FM-1 has none). Levels within 0.2 dB of the Monomachine's at INP 16..127,
  VOL, LEV, MIX, EXT and DIST 48 / 64. Known gap: its carrier is peakier above WAVE 96 (4.4 dB more peak over the
  fundamental at 127): harmonics past the 15th. A ring saved at INP 100 drives its input harder than before. DYNAMIX (ATK REL THRS MIX | RAT GAIN RMS INP): a compressor on each block's peak and RMS (RMS the blend),
  followed in log2, over a threshold of −60..0 dB by a ratio of 1:1..20:1 (a limiter at 127), made up by up to 24 dB,
  MIX parallel. The self-test holds what each does.
- **VOX-6** on VO-6's page (`tools/ref/chain_vo47.spec`, group `vo6`; VOC1 VOC2 V-SW VOIC | CONS CLEN CVOL TUNE,
  format 14, `vox_v13`), measured on the OS as a cascade formant voice: an impulse a period (a flat spectrum) and white
  noise crossfaded by VOIC (measured every 4), a 188 Hz high-pass, four resonators in series, F1 on VOC1 (187 .. 705
  Hz), F2 on VOC2 (451 .. 2757 Hz), F3 2495 and F4 3527 Hz fixed, widths 101 63 127 246 Hz (its spectra within 1.7 dB);
  its pitch whole hertz, rounded down (TUNE +-2 semitones). Its level is a compressor's: the Monomachine's loudness
  over 117 vowels is the cascade's at a tenth of the dB (10:1), followed on the cascade's mean square (25 ms), the gain
  down at once and up at most 0.23 dB a ms from a low start at each trig, which is the voice's fade-in. F1 and F2
  within 0.2 % of the Monomachine's peaks, level within 0.3 dB over vowels, VOIC and C1 .. C6. Still to come: the
  consonants (CONS CLEN CVOL: 20, B .. Z, the screen's) and V-SW's switch (the vowel changing during the fade, a pop).
- **SWAV-PUL**, new, on SWAVE-PULS's page (`tools/ref/chain_pul38.spec`, group `pul`; UNIL UNIW SUB1 SUB2 | PW PWAD
  PWRS TUNE): a pulse whose duty is PW / 128 (a square at 64, silent at 0 and 127), against GND-SIN's sine at 0.105,
  its voices started together at each trig; PWAD sweeps PW on at 5.38 steps a second a step, PWRS ON starting it again
  at each trig; a unison pair ∓0.211 c a step of UNIW off (SWAV-SAW's extended pair's spread), each as loud as UNIL
  says; sine subs an octave and two down, the first up to full scale. Its DIST ceiling sits 2.75 dB under GND-SIN's.
  Levels within 0.3 dB, the unison's beating and the sweep's timing as the Monomachine's.
- **The MOD effect** (MONO2K's own, every track: CHOR FLNG PHAS, its rate 0.1-10 Hz, depth and mix): the chorus (7-17
  ms) and the flanger (0.5-5 ms, fed back) are stereo, two taps swept in opposite ways, the left hearing one and the
  right the other; their difference, the side, goes through VOL, two high-passes at 160 Hz (the low end stays in the
  middle: an F1 sine 0.997 alike left and right) and the track's SRR, added after the pan. A saw through the chorus at
  full depth: L/R 0.62-0.87 by MIX, its side 3-7 dB under its mid over 250 Hz, 30 dB under 60 Hz. The phaser stays
  in the middle.
- **GND-NOIS** on the Monomachine's page (`tools/ref/chain_gnoi42.spec`, group `gnoi`): ST RED STON on A B C, TUNE
  on H, nothing between. White noise, the same at every note (the filter follows the note, and TUNE moves it +-2
  semitones). RED colours it, white at 0, pink at 64 (-3 dB an octave), red at 127 (-6 dB), 11 dB quieter there: white,
  pink and brown made from the same noise, their weights fitted at every 16th value (`tools/ref/gnoi_fit.py`). ST mixes
  in a second noise, ST / 127 of it: in the middle without STON (3 dB louder at 127), across the stereo with it, the
  left the first plus the second, the right the first less it (L/R 0.88 0.60 0.29 0.01 at ST 32 64 96 127). Octaves
  within 1 dB of the Monomachine's, its level within 0.2 dB. The machine runs 12 dB down and the chain's input takes it
  back (pink and red peak at 2.3 x full scale before the filter takes their lows).
- **GND-PERC**, MONO2K's own noise percussion, was GND-NOIS until the Monomachine's took the name: COLR S&H META MSPR
  PDEP PDEC GRIT, unchanged. It keeps the machine number, so every saved pattern and bank A play as they did (bank A
  renders byte for byte the same).
- **GND-SIN** on its page (`tools/ref/chain_kick40.spec`, group `kick`): the Monomachine's GND-SIN is a sine with
  TUNE alone, its page's eighth knob (CC 55). MONO2K's TUNE sits there too (format 12); FOLD SUB NOIS PDEP PDEC CLIK
  HARM on A..G are its own (a fold, a sine sub an octave down, noise, a pitch drop of up to 24 semitones and its time,
  a click, a second harmonic), nothing at init, so a new track is the Monomachine's sine. As a kick, through decays 40
  and 60, clean and driven, it keeps the Monomachine's level (0.5 dB), body (10 ms) and bands (1-1.5 dB). Known gap:
  the Monomachine's sine starts each note where its free-running phase is, after a ~0.5 ms burst: 14-28 dB more over
  2 kHz in a kick's first 4 ms than MONO2K's, which starts at zero (not modelled: it may be the emulator's; CLIK is
  MONO2K's kick's click), and so its shortest kicks (decay 20) differ by about 1 dB. Its beatbox kicks (DPRO-BBOX,
  samples) sweep from about 175-315 Hz to 65-100 Hz in 25 ms; MONO2K's PDEP 127 with PDEC 60 sweeps 190 to 80 Hz.
- **The track chain** (`tools/ref/chain.py` writes the specs, `chain_anal.py` reads them), in the Monomachine's order:
  - VOL and LEV squared; PAN cos / sin; the track clips per channel after the pan, just under full scale at LEV 127.
    MONO2K matches within 0.02 dB.
  - DIST: below 64 headroom, 1.35 dB a step; 65–72 nothing; above, a gain into a hard clip whose ceiling falls from
    +5.8 dB to −3.2 dB of a full-scale GND-SIN between 64 and 80. Within 0.15 dB. The ceiling is the machine's own
    and nothing else moves it (`tools/ref/chain_dist21.spec`: sine, saw, saw with subs or unison, one to four
    SWAVE-ENS voices and noise, each at several input levels and DIST 64–127). Driven hard, a saw, the saw with both
    subs and the saw with unison all land within 0.8 dB of each other. One to four voices land within 0.6 dB, and a
    sine at full scale or 15 dB down within 0.9 dB. So there is no output compensation and no ceiling that follows
    the input. Between machines the ceilings differ: against GND-SIN's, SWAV-SAW's is 10.5 dB lower, DIGI-WAV's 7.0,
    SWAV-PUL's 2.75, FM+STAT's and VO-6's 1.4; SWAV-ENS's 1.7 and FM+PAR's 0.3 higher; GND-NOIS, SID and FM+DYN clip
    at GND-SIN's. A bass driven past about 80 gains harmonics and almost no level. MONO2K now follows within 1 dB on all
    of these (`regress.py`, group `dist_loud`).

    **Saved patterns with high DIST sound different.** Before this fix, every machine clipped at GND-SIN's ceiling,
    so SWAV-SAW, DIGI-WAV, SWAV-ENS, FM+STAT and VO-6 came out of a hard DIST louder than the Monomachine's (a
    SWAV-SAW bass up to 10.5 dB louder) and were clipped harder. This is a fidelity fix, so patterns are not converted:
    they play as the Monomachine would play them. To get the old sound back, lower DIST until the clip starts where it
    used to (a SWAV-SAW at 88 to 75, a DIGI-WAV at 74 to 66) and raise VOL or LEV by the level lost (here 9.1 and
    1.0 dB).
  - The power test (`tools/ref/power.loop`, `power.py`): one loop on machines both instruments have (a GND-SIN
    kick, a SWAV-SAW bass with both subs driven at DIST 100, a GND-NOIS hat), played on the Monomachine by
    `mm_ref ROM loop DIR` and on MONO2K by `m2k_ref DIR --loop [DRIVE]` with the same controls. The two now match
    within 0.3 dB in rms and in the energy under 120 Hz, and within 0.6 dB on the bass (before the fix, MONO2K's bass
    was 10.7 dB louder).
  - The filter follows the note: a cutoff of 2^((step − 16)/8) × the note, for the HP (BASE) and the LP (BASE + WDTH),
    exact from C1 to C7. The LP is a resonant two-pole (1.35 × the cutoff at LPQ 0, onto it as the Q rises, up to
    Q ≈ 1000 and +54 dB without oscillating), the clip, then two one-poles at 1.55 ×; the HP a resonant two-pole and
    a one-pole at 1.75 ×, 18 dB an octave. Within 0.1 dB (LP) and 0.4 dB (HP, to −44 dB); the resonance within 0.3 %,
    and from Q 96 up both peaks sit 0.18 % under the cutoff, as the Monomachine's do. The filter follows GND-SIN's TUNE
    too (±2 semitones, 3.125 c a step). Its coefficients are exact down to 20 Hz: a low cutoff's damping used to round
    away (at 131 Hz HPQ 127 rang on without decaying).
  - The amp envelope comes after the filter and the drive: a note that decays keeps its distortion, and the
    Monomachine's oscillator keeps feeding the filter between notes.
  - BOFS / WOFS move the cutoff 7·d·|d|/256 steps; the filter envelope runs in straight lines (decay 863 ms at 64,
    ×1.565 every 8 steps; 127 holds).
  - EQ: a band added to the signal, Q 2.2, +36 / −31 dB, its cut the inverse of a boost as deep. SRR: a rate from
    44.1 kHz to 2.8 kHz, the samples joined by straight lines (its images match within 1 dB).
  - The delay: DTIM in 256ths of a note plus 7.6 ms, the send squared, negative keeping the source's side and positive
    ping-pong, feedback DFB / 64, the echoes through a one-pole HP and LP. Times within 0.5 ms.

  Known gaps (`tools/ref/regress.py`, each held to its measured size): the Monomachine's HP saturates inside its
  resonance (from about −16 dBFS out at DIST 64, a square through a high-pass) where MONO2K's stays near clean, and
  falls 7–9 dB further an octave under a resonant cutoff; its Q moves with the cutoff (HPQ 96: ~230 at 8.4 kHz, ~160
  at 4.2 kHz), MONO2K takes one value per setting. Tried and set aside: a clip on the two-pole's output with a one-pole
  at the cutoff after it matches the saturated tone (`tools/ref/hpsat.py`: h1 within 0.7 dB, h3 h5 within 1 dB) but
  clips every unresonant drive (a square at the ceiling overshoots the two-pole; the Monomachine passes that); the
  pole alone, appearing from HPQ 24 (`tools/ref/hpfit.py`: four poles fit the resonant skirt on noise at C4), closes
  the passband an octave over a resonant cutoff at C3 but not at C4, where the Monomachine's is level: the shape, like
  the Q, depends on the cutoff. The next candidate: the HP's Q and its skirt pole from a table over HPQ × cutoff (two
  dimensions, an octave a column), fitted on resonant sweeps at several notes, and the saturation inside the
  resonance. The delay keeps half-rate
  8-bit lines, so ping-pong past 0.37 s (and any delay past 0.74 s) slows the line's clock and darkens, where the
  Monomachine stays bright; SWAV-SAW's top harmonics at C7 are 2–6 dB short. Each machine's own level, clean (DIST
  64, group `machine_level`): GND-SIN, SWAV-SAW (alone, with unison, with SUB1 up to 80) and SWAV-ENS within 0.8 dB;
  the three FM+, SID-6581, GND-NOIS and VO-6 within 0.2 dB; SWAV-SAW's sine sub at C1 1.1–1.2 dB under at SUB1 127. DIGI-WAV is not compared:
  it plays MONO2K's own waves, not the DigiPro's.

Patterns saved before are moved onto each new scale as they load (version 1: the envelope; 5: the chain, each old
setting to the one giving the same Hz, dB or time, the filter placed against the note each track plays most; 6:
GND-SIN's TUNE, which moved a semitone a step, kept to the semitone within the new ±2; 7: SWAV-ENS's page, each
old interval folded into −11..+12 by octaves, one folding to 0 to +12, keeping every chord's notes; its DTUN (a fixed
spread, not the chorus's moving copy), shape, level and sub have no counterpart and reset; a trig the old level
silenced has its intervals locked OFF; an LFO aimed at the old page is cleared, intervals included, since the new axis
has OFF and an octave down in it; 8: the wave bank's choice, MONO2K's 32 for every older pattern; 9: the bus's
controls on their new laws, each old value to the new one that measures the same, HISS by its crackle; 10: FM+STAT's
and FM+DYN's pages, MONO2K's old ones carried over as near as the new pages go, not as they sounded: the old
operators ran in series, the new side by side; each ratio, depth, decay and feedback to the new page's nearest, SWEP
to 1FEN; a lock goes with its parameter where it has a place; 11: SID's page, the first oscillator's wave, duty and
PWM, MODE and the second's interval carried over, WAV2, FINE and MIX without a place).

The bus's controls are MONO2K's own, so their laws are a design choice, made by measurement (`m2k_host DIR --bus`,
`tools/bus_curves.py`: every value of each on a reference loop). Before, most of each range sat past where it was
already intense: GLCH 10 replaced the mix a tenth of the time and DRIV 10 drove it 2.9 dB into the knee; HISS's
crackle came in long before its hiss could be heard; TAPE's wow grew 0.33 c a step to ±42 c; ADPC had three sounds
over 64–127. Now GLCH, DRIV and HISS's crackle put the old 10 at 64 and grow by a ratio a step (0.3 % a sixteenth at
GLCH 1); TAPE puts the old 20 (±6.4 c) at 64; HISS's hiss has its own law (65 dB under the mix at 1, 40 at 64);
ADPC keeps 0–64 and its rate falls continuously above. The old extremes stay at 127. Measured on the old engine and
the new, every old value converts to one that measures alike (the hiss there as loud or louder: its law brings it
in sooner); `tools/check.sh` holds both.

Pattern format 17 rebuilt the bus's effects at the owner's word ("the glitch sounds intense low", "the reverb always
sounds the same", "the first third of the delay's send does nearly nothing"). The old reverb's damping left little
but a dull 0.2 s burst and a bass rumble at most settings (its octaves over 500 Hz died in 0.2 s from DAMP 64): the new
one is a four-line feedback delay network (Hadamard matrix, two lines turned into each other by a swaying angle,
every recursive product carrying its remainder, asleep once silent), RT60 0.3–24 s with the room's size, DAMP a
one-pole a line giving the top's RT60 1/1–1/30 of the bottom's. The sends (VERB, DSND) are straight in level. GLCH
went from full-level replacements (16 dB under the mix even at 1) to a law in rate, level, length, the dry's share
and the modes (44 dB under at 1). PERFORM's repeats lock to the grid, every effect crossfades in and out, DRIVE holds
its level. Old patterns convert by measurement (`tools/conv17.py`: the reverb to the pair nearest by its octaves'
RT60s and each send by the level that keeps it as loud; GLCH by its glitch's energy and events; the sends' squares to
straight levels); bank A is turned so at build (its reverb's return within 2 dB of the approved, its octaves within 3 dB at 250 Hz and
1 kHz). The delay's send leaves the Monomachine's square there (regress.py's delay_send: XFAIL).

1.5 cut the bus's GLCH (and its SIZE), TAPE and HISS, for the FM-1's time on heavy patterns (the owner: "the
important are: ADPC, reverb, drive"): GLO's GLITCH and VERB pages became one, BUS (DCAY DAMP ADPC DRIV, PERFORM's knobs
too). The pattern keeps their bytes and their conversions, unheard; the tape plays only for PERFORM's WOW, the glitch's
ring only for PERFORM's repeats. The bus costs ~46 Mac instructions a sample less on every bank A pattern, ~161 on A01
(its tape). Bank A's bytes are the same; A01 lost its tape and all four their hiss.

Pattern format 18 adds Grids (ARP's GRIDS and LANES pages: X, Y, CHAOS, its cycle, the three densities, each track's
lane, after the speeds) and the chances 90 60 40 10 % (after the other conditions, so a saved one keeps its number;
a condition past 17's read as ALWAYS, as 17 read it). Older patterns load with no lane followed. A track on a lane
plays the lane's trigs on its own clock: its K-th step since the pattern's start is Grids' step 2 (K mod 16) (RES 16:
Grids' clock at 4 ppqn, a bar) or K mod 32 (RES 32, two bars), CHAOS's byte a hash of the cycle and the part (the
same every PLAY, on every core). GLO > PLAY's MUTE (BAR, the default; NOW): a mute, a solo or an unmute made while
playing waits for the next of the pattern's bars of 16 steps (its start always one).

DIGI-WAV plays either bank, set on SYN's third page (BANK: M2K, MONO2K's 32 waves across WAVE's range as before; M64,
the owner's 64, two WAVE values a wave: an even one is that wave exactly, an odd one between two). The owner's play
without their DC (each cycle's mean, up to -29 % of the peak, taken out at playback, the files and the table kept as
they are): with it a trig stepped, 13-17 dB more under 60 Hz at the onset on OS13, OS36 and OS48; four that would
pass 16 bits so (OS29, OS37, OS48, OS58) play 0.4-1.4 dB down. MONO2K's 32 play as before. Every wave of both
at C2, C4 and C6 (`tools/wave_check.py`): the owner's in tune within 0.2 c, their levels and aliasing within the range
of MONO2K's own (the aliasing of a 256-point table read without a band limit, both banks alike), their thump at a
trig within MONO2K's worst (one of its own, 9.1 dB, kept: saved patterns play it). The
DigiPro waves and BeatBox samples are the OS's data and are not copied: DIGI-WAV plays MONO2K's own waves.

## Credits and licence

GPL-3.0-only. Built on the open FM-1 work: Felucca by Leo Kuroshita (Hügelton Instruments), CS-1, X0X, FoMni, zp12
and the others. Type: Elektron Pixel Font by savingaurora (FontStruct), CC BY-SA 3.0, adapted
(fontstruct.com/fontstructions/show/70152; `assets/fonts/mono2k-pixel`, the owner's `mono2k-font.otf` with its
`license.txt` and `readme.txt`, which travel together), the screen's type; the firmware carries 1-bit renderings of it
(`firmware/gen/m2k_fonts.h`, under CC BY-SA 3.0 as the font). Orbitron by The League of Moveable Type: SIL OFL 1.1
(`assets/fonts/orbitron/OFL.txt`), the wordmark's and the site's. The FM-1's panel geometry as measured for
CoralFlower's emulator. The machines were measured against a Monomachine running in Joe Landers' Monomachine
emulator, [gearmulator-md-mm](https://github.com/joelanders/gearmulator-md-mm) (a fork of Gearmulator by The Usual
Suspects), with the owner's own OS image: measurements only, none of the emulator's code or data is in MONO2K.
Monomachine and Machinedrum are trademarks of Elektron; M-VAVE and FM-1 are trademarks of their owners. MONO2K is not
affiliated with Elektron or M-VAVE.

The DRM- drums' voice design is after the Erica Synths LXR drum voices (lxr-desktop-app, GPL-3.0-only; originally
Julian Schmidt's Sonic Potions LXR): their laws (the sweep, its curve and time, the PM hats, the clap's repeats),
written anew in fixed point and measured against a float model of them; none of its code or transient samples is
here. LXR is Erica Synths' mark.

GRIDS (`firmware/src/m2k_grids.c`) is Émilie Gillet's Grids, Mutable Instruments' topographic drum sequencer
([pichenettes/eurorack](https://github.com/pichenettes/eurorack), `grids/`, GPL-3.0): its 25 nodes of pattern data
and its rules (ReadDrumMap, EvaluateDrums) ported, with credit, to MONO2K's integer core; `tools/ref/grids_ref.py`
holds the port to the original's rules. Grids is Mutable Instruments' name.

The 64-wave bank (`assets/waves/mono2k64/`) is the project owner's own waveforms, licensed with the project
(GPL-3.0-only) unless the owner says otherwise.

Bank A's parts are MONO2K's own: their sounds were drawn at random and picked by ear, their harmony and craft from
research written up in `docs/composing.md`; the Model:Cycles demos and the IDM references there were measured or read
about, never quoted.