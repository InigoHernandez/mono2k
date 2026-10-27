// SPDX-License-Identifier: GPL-3.0-only
// What every control on the FM-1 does under MONO2K, and every page, layer and mode: one table for the emulator's
// status line (app.js: a line for what the pointer is over, from the core's state) and the guide's list and drawing
// (guide.js). The text is the manual's and m2k_ui.c's (its buttons, keys and knobs), not another firmware's.
window.M2K_HELP = (() => {
  "use strict";
  const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const note = (n) => NOTE[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);
  /* the 27 keys: white (the lower row, 16) and black (the upper, 11), as the FM-1 has them from F3 */
  const isBlack = (k) => (0x54a >> ((k + 5) % 12)) & 1;
  const WHITE = [], BLACK = [];
  for (let k = 0; k < 27; k++) (isBlack(k) ? BLACK : WHITE).push(k);
  /* the computer's keys (app.js's map) */
  const KW = "Z X C V B N M Q W E R T Y U I O".split(" ");
  const KB = "S D F H J 2 3 4 6 7 9".split(" ");
  const BK = ["P", "0", "8", "L", "5", "G", "1", "\\", "K", "A", "Space", "Enter", "-", "="];
  const BNAME = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT−", "OCT+"];
  const ENAME = ["SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4", "MASTER"];
  const EK = ["; '", ", .", "[ ]", "Tab, ↑ ↓", "Tab, ↑ ↓", "Tab, ↑ ↓", "Tab, ↑ ↓", "/"];
  const GLO_W = ["mutes T1", "mutes T2", "mutes T3", "mutes T4", "mutes T5", "mutes T6", "FILL on the next loop",
    "taps the tempo", "solos T1 while held", "solos T2 while held", "solos T3 while held", "solos T4 while held",
    "solos T5 while held", "solos T6 while held", "unmutes every track", ""];
  const SEQ_W = ["CLEAR the track's steps and locks", "REVERSE them", "SHIFT them a step earlier",
    "SHIFT them a step later", "RANDOM: a quarter of the steps", "LEN ×2: the pattern twice as long",
    "COOK: a few notes moved", "HALF: the pattern half as long"];
  const PERF = ["REPEAT a quarter note", "REPEAT an eighth", "REPEAT a sixteenth", "REPEAT a thirty-second",
    "REVERSE the last beat", "STOP: a tape stop", "STUTTER", "CRUSH: the ADPCM at its most", "WOW: the tape's wow at its most",
    "DRIVE the bus hard, held to its level", "KILL: silence"];
  const BROLE = ["T1", "T2", "T3", "T4", "T5", "T6", "P1", "P2", "P3", "P4", "MUTE"];
  const TTYPE = ["ALL", "AMP", "FILTER", "LFO", "NONE"];
  const COND = ["100%", "50%", "1:2", "1:4", "FILL", "!FIL"];
  const PAGE = ["HOME", "SYN", "AMP", "FILT", "FX", "LFO", "ARP", "GLO"];

  /* the status line: what the control under the pointer does now (ctx: the core's state, app.js's w_info and the
   * knobs' names on this page) */
  function line(id, c) {
    if (!id) return "";
    const kind = id[0], i = +id.slice(1);
    const mode = c.grid ? "GRID" : c.pads ? "PADS" : "KEYS";
    if (id === "lcd") return "The screen: the page, the track and its machine on top, the four knobs under it, then what you touch (a value, the machine's card, a held step) beside the page's picture, the track's 16 steps, the pattern's bars, its name and tempo.";
    if (kind === "e") {
      const k = `${ENAME[i]} (${EK[i]})`;
      switch (i) {
        case 0: return `${k}: the track, now T${c.trk + 1}. With GLO held, the tempo; with a step held in GRID, the step's note, a semitone a click.`;
        case 1: return c.page === 1
          ? `${k}: on SYN, the track's machine, now ${c.mach}, in ALGORITHM's order. SEQ held: the accent's amount; a step held: accent and slide.`
          : `${k}: this page's pages (${c.label} now; on EDIT's SYN it picks the machine). SEQ held: the accent's amount; a step held: accent and slide.`;
        case 2: return `${k}: the pattern, ${c.pat} now: at once when stopped, at the bar's end while playing. A step held: its condition; in step entry, the cursor.`;
        case 7: return `${k}: the volume. With a step held in GRID, its trig type, ALL to NONE.`;
        default: {
          const kn = c.knobs && c.knobs[i - 3];
          const what = kn && kn.s4 ? `${kn.s4}, ${kn.name.toLowerCase()} on ${c.label}` : `nothing on ${c.label}`;
          return `${k}: ${what}. A step held in GRID: a lock on it. OCT− held and a turn: back to its INIT. SEQ held: LEN SPD SWNG BPM; FX held: GLCH TAPE ADPC DRIV.`;
        }
      }
    }
    if (kind === "b") {
      const k = `${BNAME[i]} (${BK[i]}${i < 10 ? ", F" + (i + 1) : ""})`;
      const on = (pg) => (c.page === pg ? "; again, its next page" : "");
      switch (i) {
        case 0: return `${k}: FX: EQ and rate, DELAY, DSTR (bits, fold, XOR, the reverb's send), MOD${on(4)}. Held: PERFORM, the bus's effects on the black keys. Double-tap: latched.`;
        case 1: return `${k}: FILT: the high-pass and low-pass as a band, their resonance, the filter's envelope${on(3)}. Held a second alone: its card.`;
        case 2: return `${k}: AMP: ATCK HOLD DEC REL, then DIST VOL PAN PORT${on(2)}. Held a second alone: its card.`;
        case 3: return `${k}: LFO: three LFOs, two pages each${on(5)}. Held: what the knobs move comes back when you let go (OCT+ keeps it).`;
        case 4: return `${k}: SYN: the machine's card and its knobs A–D, E–H, then SND, the sound library. Held: + SEQ the sound INIT, + SAVE into the library, + REC / PLAY copy, paste.`;
        case 5: return `${k}: GLO: TIME, GLITCH, VERB, SET, MIDI, IN. Held: REC copy, PLAY paste, SEQ clear, OCT− OCT+ undo, redo; white keys mute, fill, tap, solo. Double-tap: latched.`;
        case 6: return `${k}: HOME: the pattern in six lanes; the knobs the track's level and pitch, the tempo, the swing. Held: the white keys pick patterns, black 1–4 the bank.`;
        case 7: return `${k}: keeps the pattern; held, the whole project. GLO + SAVE reloads the saved pattern.`;
        case 8: return `${k}: ARP: the track's arpeggiator, MODE SPD RNG GATE; hold keys and it plays them in turn. GLO + ARP: PADS on or off.`;
        case 9: return `${k}: ${c.grid ? "back to KEYS: the keys play notes" : "GRID: the keys become the steps, sixteen at a time"}. Held: the step tools on white 1–8, the sequencer on the knobs. Double-tap: latched.`;
        case 10: return `${k}: ${c.playing ? "stops" : c.rec && !c.grid && !c.ready ? "Ready: REC is on, so the first note you play starts the pattern" : "starts the pattern"}. Held a second alone: its card.`;
        case 11: return `${k}: ${c.rec ? "recording is on; a tap turns it off" : c.playing ? "a tap records live: notes land on the nearest step" : "a tap: step entry, each note the next step"}. Held: FILL. Held + SEQ: the track's steps cleared.`;
        default: {
          const up = i === 13;
          const what = c.grid ? `the page of steps ${up ? "on" : "back"}` : `T${c.trk + 1}'s octave ${up ? "up" : "down"}`;
          return `${k}: ${what}; a held step's ratchet. In a question, ${up ? "YES" : "NO"}.${up ? "" : " Held and a knob turned: that knob back to its INIT."}`;
        }
      }
    }
    if (kind === "k") {
      const w = WHITE.indexOf(i), b = BLACK.indexOf(i), n = 53 + i + 12 * (c.oct || 0);
      if (w >= 0) {
        const s = c.spage * 16 + w + 1;
        const now = mode === "GRID" ? `step ${s}: a tap places a trig or takes one away, held it is the step you edit`
          : mode === "PADS" ? (w < 6 ? `hits T${w + 1} at its root` : w >= 8 && w < 14 ? `hits T${w - 7} at its root` : "nothing in PADS")
          : `plays ${note(n)} on T${c.trk + 1}`;
        const glo = GLO_W[w] ? ` GLO held: ${GLO_W[w]}.` : "";
        const seq = w < 8 ? ` SEQ held: ${SEQ_W[w]}.` : "";
        return `White ${w + 1} (${KW[w]}), ${mode}: ${now}.${glo} HOME held: pattern ${w + 1} of the bank.${seq}`;
      }
      const now = mode === "KEYS" ? `plays ${note(n)} on T${c.trk + 1}`
        : b < 6 ? `T${b + 1}, the track in view` : b < 10 ? (mode === "GRID" ? `page P${b - 5} of the steps; held, the pattern ends there` : "nothing in PADS") : "mutes the track in view";
      const home = b < 4 ? ` HOME held: bank ${"ABCD"[b]}.` : "";
      const step = mode === "GRID" ? ` A step held: ${b < 5 ? "trig type " + TTYPE[b] : "condition " + COND[b - 5]}.` : "";
      return `Black ${b + 1} (${KB[b]}, ${BROLE[b]}), ${mode}: ${now}. FX held: ${PERF[b]}.${home}${step}`;
    }
    return "";
  }

  /* the guide: every control and every page, layer and mode. hi: the controls lit on the drawing (e: knobs, b:
   * buttons, k: keys; kw / kb every white / black key); screen: what the FM-1 shows for it, played on a core of its own
   * from the factory's A01 (guide.js: "b X" a tap, "B X" held down, "u X" let go, "w ms", "e knob d", "kd i" a white
   * key held, "k i" / "K i" a white / black key tapped) */
  const ALLW = WHITE.map((k) => "k" + k), ALLB = BLACK.map((k) => "k" + k);
  const G = [
    { g: "Knobs", id: "e0", tag: "SELECT", hi: ["e0"], short: "The track, T1 to T6.", screen: "e 0 1; w 400",
      long: "SELECT picks the track; the keys play the one in view (in GRID and PADS the black T1–T6 do too). With GLO held it turns the tempo; with a step held in GRID, the step's note, a semitone a click, heard as you turn.", where: "; '" },
    { g: "Knobs", id: "e1", tag: "ALGORITHM", hi: ["e1"], short: "On SYN the machine; elsewhere the page's pages.", screen: "b EDIT; w 300; e 1 1; w 500",
      long: "On EDIT's SYN page ALGORITHM changes the track's machine, in this order: the ten that play notes, the four drums, GND-PERC and GND-NOIS, the three FX. A track still at its INIT takes the new machine's whole INIT; a sound you shaped keeps its chain, LFOs and steps. On every other page it turns that page's pages: LFO 1A to 3B, FX's four, GLO's TIME to IN. SEQ held: the accent's amount; a step held: no accent, accent, slide, both.", where: ", ." },
    { g: "Knobs", id: "e2", tag: "PRESETS", hi: ["e2"], short: "The pattern: queued to the bar's end while playing.", screen: "e 2 1; w 400",
      long: "PRESETS picks the pattern: at once when stopped, at the end of the bar while playing, so patterns chain as you play. A step held in GRID: its condition (100%, a chance, A:B, 1ST, PRE, FILL). In step entry: the cursor, back and on.", where: "[ ]" },
    { g: "Knobs", id: "eK", tag: "KNOB 1–4", hi: ["e3", "e4", "e5", "e6"], short: "The four parameters of the page in view.", screen: "b ENV; w 200; e 3 4; w 300",
      long: "The four knobs turn the four values the screen's knob row names, page by page. Hold a step in GRID and turn one: the value is locked to that step. Hold OCT− and turn one: that knob goes back to its machine's INIT. While SEQ is held they are LEN, SPEED, SWING and TEMPO; while FX is held, the bus's GLCH, TAPE, ADPC and DRIV; while LFO is held, what you turn comes back when you let go.", where: "Tab picks, ↑ ↓ turn (Shift faster)" },
    { g: "Knobs", id: "e7", tag: "MASTER", hi: ["e7"], short: "The volume; a held step's trig type.", screen: "e 7 -4; w 300",
      long: "MASTER is the volume. With a step held in GRID it is the step's trig type: ALL, AMP, FILTER, LFO, AMP+FLT, AMP+LFO, FLT+LFO or NONE.", where: "/ (a held step's trig type)" },
    { g: "Buttons", id: "b4", tag: "EDIT", hi: ["b4"], short: "SYN: the machine's knobs A–H, then the sound library.", screen: "b EDIT; w 400",
      long: "EDIT opens SYN: the machine's card and its first four knobs, A–D; again, E–H; again, SND, the sound library (a BANK page before it on DIGI-WAV). Held, it is the track's sound: + SEQ starts it afresh at its INIT, + SAVE keeps it in the library's first INIT slot, + REC and + PLAY copy and paste it. In step entry, EDIT leaves a rest.", where: "5 or F5" },
    { g: "Buttons", id: "b2", tag: "ENV", hi: ["b2"], short: "AMP: the envelope, distortion, volume, pan.", screen: "b ENV; w 400",
      long: "ENV opens AMP: ATCK, HOLD, DEC and REL, drawn as the note's shape, each showing the time you hear; again, DIST, VOL, PAN and PORT. HOLD counts in beats at the pattern's tempo (32 a beat), as on the Monomachine. Held a second with nothing else touched: its card.", where: "8 or F3" },
    { g: "Buttons", id: "b1", tag: "SEL", hi: ["b1"], short: "FILT: the filters as a band.", screen: "b SEL; w 400",
      long: "SEL opens FILT: a high-pass and a low-pass as one band, BASE its bottom and WDTH its width, HPQ and LPQ their resonance, following the note; again, the filter's envelope, ATCK and DEC, moving the base and the width by B-OF and W-OF. Held a second alone: its card.", where: "0 or F2" },
    { g: "Buttons", id: "b0", tag: "FX", hi: ["b0"], short: "FX's four pages; held, PERFORM.", screen: "b FX; w 400",
      long: "FX opens the track's effects, four pages: FX (an EQ band, the rate reduction, the delay's time), DELAY (its send, feedback and filters, in 8-bit mu-law), DSTR (bits, wavefold, XOR, the send to the bus's reverb) and MOD (a chorus, flanger or phaser). Held: PERFORM, the bus's effects on the black keys.", where: "P or F1" },
    { g: "Buttons", id: "b3", tag: "LFO", hi: ["b3"], short: "Three LFOs, two pages each.", screen: "b LFO; w 400",
      long: "LFO opens three LFOs per track, two pages each, 1A to 3B: which track and parameter it moves (any track's), its wave, what a trig does to it, then its multiplier, its cycle in bars and notes, its start and its depth. Held: what the knobs move comes back when LFO is let go; OCT+ while it is held keeps the change.", where: "L or F4" },
    { g: "Buttons", id: "b8", tag: "ARP", hi: ["b8"], short: "The track's arpeggiator.", screen: "b ARP; w 400",
      long: "ARP opens the arpeggiator: MODE (OFF, UP, DOWN, U/D, RND, ORDR, the order you played them), SPD (1/32 to 1/4), RNG (one to four octaves) and GATE. Hold keys and it plays them in turn. GLO + ARP turns PADS on or off.", where: "K or F9" },
    { g: "Buttons", id: "b5", tag: "GLO", hi: ["b5"], short: "Tempo, the bus, the reverb, settings; held, FUNC.", screen: "b GLO; w 400",
      long: "GLO opens the pattern's and the instrument's pages: TIME (tempo, swing, length, accent), GLITCH, VERB, SET, MIDI and IN. Held, it is FUNC: REC copies and PLAY pastes the pattern (the track's steps in GRID), SEQ clears it, OCT− and OCT+ undo and redo, SAVE reloads, EDIT opens the library, HOME turns the keys' mode, ARP PADS; white 1–6 mute, 7 fills the next loop, 8 taps the tempo, 9–14 solo while held, 15 unmutes all; SELECT turns the tempo.", where: "G or F6" },
    { g: "Buttons", id: "b6", tag: "HOME", hi: ["b6"], short: "The pattern in six lanes; held, pick patterns.", screen: "b PLAY; w 1500",
      long: "HOME shows the pattern in six lanes, the step under way framed in each, and its four bars; KNOB 1–4 are the track's level and pitch, the tempo and the swing. Held: the white keys pick the bank's 16 patterns (playing: queued to the bar's end; the same key again: now), black 1–4 the bank, A to D; + SEQ clears the picked ones, + REC and + PLAY copy the open one into them.", where: "1 or F7" },
    { g: "Buttons", id: "b9", tag: "SEQ", hi: ["b9"], short: "GRID: the keys are the steps; held, the step tools.", screen: "b SEQ; w 400",
      long: "SEQ turns the keys into steps (GRID): the sixteen white keys are the page in view, OCT− and OCT+ turn the four pages. A key on an empty step places a trig with the last note played; a quick tap on a trig takes it away. Pressed again, the keys play notes. Held: white 1–8 are the step tools (clear, reverse, shift either way, random, ×2, cook, half), the knobs LEN, SPEED, SWING and TEMPO, ALGORITHM the accent.", where: "A or F10" },
    { g: "Buttons", id: "b10", tag: "PLAY", hi: ["b10"], short: "Start and stop.", screen: "b PLAY; w 1000",
      long: "PLAY starts and stops the pattern. With REC on and the sequencer stopped it is Ready: PLAY blinks and the first note you play starts the pattern. Held a second alone: its card.", where: "Space" },
    { g: "Buttons", id: "b11", tag: "REC", hi: ["b11"], short: "Record: live while playing, step entry while stopped.", screen: "b PLAY; w 300; b REC; w 600",
      long: "A tap arms recording: while playing, notes land on the nearest step (a hard key an accent) and knobs turned just after a trig lock their value to it; while stopped, step entry. Held, it is FILL. REC held + SEQ clears the track in view's steps, its sound kept.", where: "Enter" },
    { g: "Buttons", id: "b7", tag: "SAVE", hi: ["b7"], short: "Keep the pattern; held, the project.", screen: "b SAVE; w 300",
      long: "SAVE keeps the pattern as it is; what you play meanwhile is a working copy, and GLO + SAVE goes back to the last saved version. Held, it keeps the whole project. In the emulator everything is also kept in this browser as you go.", where: "\\ or F8" },
    { g: "Buttons", id: "bO", tag: "OCT− OCT+", hi: ["b12", "b13"], short: "The octave; in GRID the page; a held step's ratchet.", screen: "b OCTU; w 400",
      long: "OCT− and OCT+ move the track in view's octave (each track keeps its own, from its home note); in GRID they turn the page of steps; with a step held, its ratchet, one to four hits. In a question, OCT+ is YES and OCT− NO. Hold OCT− and turn a knob: that knob goes back to its INIT. GLO + OCT− undoes, GLO + OCT+ redoes.", where: "- =" },
    { g: "Keys and screen", id: "kw", tag: "White keys", hi: ALLW, short: "Notes, or the 16 steps in GRID.", screen: "b SEQ; w 2300; kd 4; w 600",
      long: "The sixteen white keys play the track in view (KEYS), or are the page's sixteen steps (GRID), or hit tracks 1–6 at their roots (PADS, white 1–6 and 9–14). Held with GLO: 1–6 mute, 7 fill, 8 tap tempo, 9–14 solo, 15 unmute all. With HOME: the bank's patterns. With SEQ: the eight step tools.", where: "Z X C V B N M, Q W E R T Y U I O" },
    { g: "Keys and screen", id: "kb", tag: "Black keys", hi: ALLB, short: "Notes; in GRID T1–T6, P1–P4, MUTE; held FX, PERFORM.", screen: "b PLAY; w 600; B FX; w 900",
      long: "The eleven black keys play notes in KEYS. In GRID and PADS, T1–T6 pick the track, F#5 mutes it, and in GRID P1–P4 show a page of steps (held, the pattern ends there). Held FX: PERFORM's eleven effects. Held HOME: black 1–4 the bank. A step held in GRID: black 1–5 its trig type, 6–11 its condition.", where: "S D F H J, 2 3 4 6 7 9" },
    { g: "Keys and screen", id: "lcd", tag: "The screen", hi: ["lcd"], short: "One grid of five bands on every page.", screen: "b PLAY; w 1200",
      long: "Five bands on every page: the header (the page and its pages as squares, the track and its machine; GRID, PADS, FILL, recording and the transport on the right), the four knobs as they sit on the panel (a locked value inverted), the stage (a value in big type, the machine's card or a held step on the left, the page's picture on the right), the track's sixteen steps, and the footer (the pattern's bars, its name, the tempo). A message takes the header for a moment, inverted.", where: "GLO > SET, SCRN: light or dark" },
    { g: "Pages", id: "pHOME", tag: "HOME", hi: ["b6"], short: "Six lanes, the bars, level, pitch, tempo, swing.", screen: "b PLAY; w 1500",
      long: "HOME: the whole pattern at once, six lanes of steps with the playing step framed, the four bars under them. Its knobs: the track's LEVL and PTCH, the pattern's BPM and SWNG.", where: "HOME" },
    { g: "Pages", id: "pSYN", tag: "SYN", hi: ["b4", "e1"], short: "The machine: its card and eight knobs.", screen: "b EDIT; w 400",
      long: "SYN: the machine's card (its family, its mark, how much PUNCH, METAL and PLASTIC it has) and its eight knobs, A–D then E–H; ALGORITHM changes the machine. The third page is SND, the sound library.", where: "EDIT" },
    { g: "Pages", id: "pSND", tag: "SND", hi: ["b4"], short: "The sound library: 128 sounds.", screen: "b EDIT; w 400; b EDIT; w 400; b EDIT; w 500",
      long: "SND: 128 sounds kept apart from the patterns, each a machine and every setting, a category and the note it was made at. SLOT picks one, LOAD puts it on the track, SAVE keeps the track's sound there (over a sound, it asks), CAT shows one category. GLO + EDIT opens it from anywhere.", where: "EDIT, EDIT, EDIT" },
    { g: "Pages", id: "pAMP", tag: "AMP", hi: ["b2"], short: "ATCK HOLD DEC REL | DIST VOL PAN PORT.", screen: "b ENV; w 400",
      long: "AMP: the amp envelope, drawn as it shapes the note: ATCK to full level (20 ms at 16, 0.4 s at 64, 10 s at 127), HOLD in beats, DEC and REL to silence (0.8 s at 64, 20 s at 127; DEC 127 holds). Then DIST into the machine's own clip, VOL into the chain, PAN, and PORT's glide.", where: "ENV" },
    { g: "Pages", id: "pFILT", tag: "FILT", hi: ["b1"], short: "BASE WDTH HPQ LPQ | the filter's envelope.", screen: "b SEL; w 400",
      long: "FILT: a high-pass and a low-pass as a band that follows the note, drawn as its response. BASE is the bottom, WDTH the width, HPQ and LPQ the resonance; the second page's envelope moves the base and the width.", where: "SEL" },
    { g: "Pages", id: "pFX", tag: "FX", hi: ["b0", "e1"], short: "FX, DELAY, DSTR, MOD.", screen: "b FX; w 400; b FX; w 500",
      long: "FX: EQF and EQG, one band of EQ; SRR, the sample rate reduced; DTIM. DELAY: the track's own echo, DSND its send (straight in level), DFB, DBAS, DWID. DSTR: BITS, FOLD, XOR and VERB, the send to the bus's reverb. MOD: a chorus, flanger or phaser across the stereo.", where: "FX, again for the next; or ALGORITHM" },
    { g: "Pages", id: "pLFO", tag: "LFO", hi: ["b3", "e1"], short: "1A to 3B: where, wave, trig | rate, phase, depth.", screen: "b LFO; w 400",
      long: "LFO: three per track. TRCK and DEST pick what it moves (any track's parameter), WAVE its shape, TRIG what a trig does (FREE, TRIG, HOLD, ONE, HALF); MULT and SPD its rate in time with the tempo (SPD 60 is a bar, 84 a quarter note), PHAS where it starts, DPTH how far either way.", where: "LFO, again for the next; or ALGORITHM" },
    { g: "Pages", id: "pARP", tag: "ARP", hi: ["b8"], short: "MODE SPD RNG GATE.", screen: "b ARP; w 400",
      long: "ARP: the track's arpeggiator. Hold keys and it plays them in turn, in MODE's order, at SPD, over RNG's octaves, each note GATE long.", where: "ARP" },
    { g: "Pages", id: "pGLO", tag: "GLO pages", hi: ["b5", "e1"], short: "TIME GLITCH VERB SET MIDI IN.", screen: "b GLO; w 100; e 1 1; e 1 1; w 500",
      long: "GLO: TIME (BPM, SWNG, LEN, ACNT), GLITCH (TAPE wow, ADPC crunch, GLCH from a ghost to wild, the repeat's SIZE), VERB (DCAY 0.3 to 24 s, DAMP, HISS and dust, DRIV into the soft clip), SET (the screen, projects, WIPE), MIDI (channel, sync in and out, notes out) and IN (where notes come from, THRU). The bus's settings belong to the pattern.", where: "GLO, again for the next; or ALGORITHM" },
    { g: "Held", id: "hGLO", tag: "GLO held", hi: ["b5", "b11", "b10", "b9", "b12", "b13", "b7"], short: "FUNC: copy, paste, clear, undo; mutes and solos.", screen: "B GLO; w 900",
      long: "Hold GLO a moment and the screen lists what it does with each button. REC copies, PLAY pastes, SEQ clears the pattern's steps, its sounds kept (in GRID, the track's; held a second, INIT; again, undone). OCT− undoes, OCT+ redoes, SAVE reloads, EDIT opens the library, HOME turns the keys' mode, ARP PADS. White 1–6 mute, 7 fills the next loop, 8 taps the tempo, 9–14 solo while held, 15 unmutes all.", where: "Hold GLO" },
    { g: "Held", id: "hHOME", tag: "HOME held", hi: ["b6", ...ALLW, "k1", "k3", "k5", "k8"], short: "Pick patterns and banks.", screen: "B HOME; w 900",
      long: "Hold HOME: the bank's sixteen slots on the white keys, the one picked last named over them, the open one inverted, INIT ones dotted. A white key opens its pattern (playing: queued to the bar's end; the same key again, now); black 1–4 (F#3 G#3 A#3 C#4) pick the bank. + SEQ clears the picked patterns; + REC copies the open one, + PLAY pastes it into those picked.", where: "Hold HOME" },
    { g: "Held", id: "hEDIT", tag: "EDIT held", hi: ["b4", "b9", "b7", "b11", "b10"], short: "The track's sound: INIT, keep, copy, paste.", screen: "B EDIT; w 900",
      long: "Hold EDIT: + SEQ starts the track's sound afresh at its machine's INIT (again: undone), + SAVE keeps it in the library's first INIT slot, in the category it sounds like, + REC copies it and + PLAY pastes it onto the track in view.", where: "Hold EDIT" },
    { g: "Held", id: "hSEQ", tag: "SEQ held", hi: ["b9", ...WHITE.slice(0, 8).map((k) => "k" + k), "e3", "e4", "e5", "e6", "e1"], short: "The step tools; LEN SPEED SWING TEMPO.", screen: "B SEQ; w 900",
      long: "Hold SEQ: white 1–8 are tools for the track in view over the pattern's length, its locks moving with its steps: CLEAR, REVERSE, SHIFT earlier and later, RANDOM, LEN ×2, COOK and HALF. KNOB 1–4 are LEN, the track's own SPEED (1/8× to 2×), SWING and TEMPO; ALGORITHM the accent. + REC and + PLAY copy and paste the track's steps. GLO + OCT− undoes a tool.", where: "Hold SEQ" },
    { g: "Held", id: "hPERF", tag: "PERFORM", hi: ["b0", ...ALLB, "e3", "e4", "e5", "e6"], short: "FX held: the bus's effects on the black keys.", screen: "b PLAY; w 600; B FX; w 900",
      long: "Hold FX: the black keys are the bus's effects while you hold them, the white keys still play the track. 1–4 repeat a quarter, an eighth, a sixteenth or a thirty-second, on the grid; 5 reverses the last beat; 6 a tape stop; 7 a stutter; 8 crush; 9 wow; 10 drive, held to its level; 11 kill. The knobs are GLCH, TAPE, ADPC and DRIV, and what you turn stays. Every effect fades in and out, so nothing clicks.", where: "Hold FX; double-tap to latch" },
    { g: "Held", id: "hLFO", tag: "LFO held", hi: ["b3", "e3", "e4", "e5", "e6", "b13"], short: "Knobs that come back when let go.", screen: "B LFO; w 900",
      long: "Hold LFO and turn the knobs: what they move comes back when LFO is let go, a sweep for a bar or a filter opened for a fill. OCT+ while it is held keeps the change.", where: "Hold LFO" },
    { g: "Held", id: "hLATCH", tag: "Latched", hi: ["b5", "b9", "b6", "b0"], short: "Double-tap GLO, SEQ, HOME or FX: the layer stays.", screen: "b GLO; w 80; b GLO; w 500",
      long: "Tap GLO, SEQ, HOME or FX twice, quickly: the layer stays as if you held it, its button blinks and its map says LATCHED. The keys and the knobs are the layer's; the button combinations still want the button held. Tap it again to let go; any other page button lets go too.", where: "Double-tap" },
    { g: "Held", id: "hCARD", tag: "Cards", hi: ["b1", "b2", "b8", "b10", "b12", "b13"], short: "Hold a button a second alone: what it does.", screen: "B SEL; w 1300",
      long: "Hold SEL, ENV, ARP, PLAY, OCT− or OCT+ a second with nothing else touched and the screen shows its card: what it does alone and with the others. Letting go then does nothing more. GLO, HOME, EDIT, SEQ, FX and LFO show their maps when held a moment.", where: "Hold a second" },
    { g: "The keys' modes", id: "mKEYS", tag: "KEYS", hi: [...ALLW, ...ALLB], short: "The 27 keys play the track in view.", screen: "w 300",
      long: "KEYS: all 27 keys play the track in view, from its home note's octave (a kick at C2 plays as a kick). OCT− and OCT+ move it. With REC on they record; stopped, step entry. GLO + HOME turns the keys to the next mode: KEYS, PADS, GRID.", where: "GLO + HOME, a tap each" },
    { g: "The keys' modes", id: "mGRID", tag: "GRID", hi: ["b9", ...ALLW, ...ALLB], short: "The white keys are the page's 16 steps.", screen: "b SEQ; w 400",
      long: "GRID: the white keys are the sixteen steps of the page in view (four pages, 64 steps), lit where a trig is, the playhead too. The black keys: T1–T6 the track, P1–P4 the page (held a moment, the pattern ends there: the new bar a copy of the bars before), F#5 mute.", where: "SEQ, or GLO + HOME" },
    { g: "The keys' modes", id: "mPADS", tag: "PADS", hi: ["b5", "b6", "b8", ...ALLW], short: "White 1–6 and 9–14 hit tracks 1–6.", screen: "B GLO; w 100; b HOME; u GLO; w 400",
      long: "PADS: white 1–6 (and 9–14 for the other hand) hit tracks 1–6, each at its root, the note its trigs sit on most, so a drum stays in its key. With PLAY and REC each hit is written to the nearest step of its track.", where: "GLO + HOME, or GLO + ARP" },
    { g: "The keys' modes", id: "mSTEP", tag: "A step held", hi: ["e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7", "b12", "b13", ...ALLB], short: "Its note, condition, ratchet, trig type, locks.", screen: "b SEQ; w 2300; kd 0; w 700",
      long: "Hold a step in GRID: SELECT its note, ALGORITHM accent and slide, PRESETS its condition, OCT− OCT+ its ratchet, MASTER its trig type, KNOB 1–4 lock the page's parameters to it (OCT− held and a knob takes a lock away); black 1–5 set its trig type and 6–11 its condition at once; REC, PLAY and SEQ copy it, paste onto it, clear its locks. Hold several and the edit goes to all.", where: "GRID, hold a white key" },
    { g: "Recording", id: "rLIVE", tag: "REC, live", hi: ["b10", "b11"], short: "PLAY, then REC: notes land on the nearest step.", screen: "b PLAY; w 300; b REC; w 600",
      long: "With the keys playing notes, press PLAY and tap REC: each note lands on the nearest step, a hard key an accent; a knob turned just after a trig locks its value to that step. Tap REC to stop. A take you don't want: hold REC and tap SEQ (SEQ again brings it back).", where: "PLAY, REC" },
    { g: "Recording", id: "rENTRY", tag: "Step entry", hi: ["b11", "b4", "e2"], short: "Stopped, REC: each note the next step.", screen: "b REC; w 400",
      long: "With the sequencer stopped, tap REC: STEP ENTRY. Each note you play is written to the next step and the cursor moves on; EDIT leaves a rest, PRESETS moves the cursor. Tap REC to leave.", where: "REC, stopped" },
    { g: "Recording", id: "rREADY", tag: "Ready", hi: ["b11", "b10"], short: "REC, then PLAY: the first note starts it.", screen: "b REC; w 100; b PLAY; w 400",
      long: "With REC on and the sequencer stopped, tap PLAY: READY in the header, PLAY blinks and nothing moves yet. The first note you play (a key, a pad or MIDI) starts the pattern from its first step and is recorded there. PLAY again starts it at once; REC off stops waiting.", where: "REC, PLAY, then play" },
    { g: "Recording", id: "rFILL", tag: "FILL", hi: ["b11", "b5"], short: "REC held while playing.", screen: "b PLAY; w 500; B REC; w 600",
      long: "Hold REC while the pattern plays for FILL: steps on the FILL condition play, those on !FIL rest. GLO + white 7 gives a fill to the next loop alone.", where: "Hold REC; GLO + white 7" },
    { g: "Recording", id: "rASK", tag: "Questions", hi: ["b13", "b12"], short: "OCT+ YES, OCT− NO.", screen: "b GLO; w 400; e 1 1; e 1 1; e 1 1; w 400; e 6 1; w 500",
      long: "Only what writes over something asks: a sound saved over another, every pattern wiped, the demos back, a project loaded. OCT+ is YES, OCT− NO, anything else NO.", where: "OCT+ / OCT−" },
  ];
  /* a key on its own, every mode at once (the guide's drawing, where no state is in view) */
  function key(k) {
    const w = WHITE.indexOf(k), b = BLACK.indexOf(k);
    if (w >= 0) {
      const pads = w < 6 ? `hits T${w + 1}` : w >= 8 && w < 14 ? `hits T${w - 7}` : "nothing";
      return { tag: `White ${w + 1} · ${note(53 + k)}`, where: KW[w],
        long: `KEYS: plays ${note(53 + k)} at the track's home octave (OCT− OCT+ move it). GRID: step ${w + 1} of the page in view. PADS: ${pads}.` +
          (GLO_W[w] ? ` GLO held: ${GLO_W[w]}.` : "") + ` HOME held: pattern ${w + 1} of the bank.` + (w < 8 ? ` SEQ held: ${SEQ_W[w]}.` : "") };
    }
    const grid = b < 6 ? `T${b + 1}, the track in view` : b < 10 ? `P${b - 5}, a page of steps (held: the pattern ends there)` : "mutes the track in view";
    return { tag: `Black ${b + 1} · ${note(53 + k)} · ${BROLE[b]}`, where: KB[b],
      long: `KEYS: plays ${note(53 + k)}. GRID: ${grid}${b < 6 || b === 10 ? " (PADS too)" : ""}. FX held: ${PERF[b]}.` + (b < 4 ? ` HOME held: bank ${"ABCD"[b]}.` : "") +
        ` A step held in GRID: ${b < 5 ? "its trig type, " + TTYPE[b] : "its condition, " + COND[b - 5]}.` };
  }
  return { line, key, G, note, WHITE, BLACK, isBlack, BNAME, ENAME, PAGE };
})();
