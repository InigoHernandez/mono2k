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
    "taps the tempo", "solos T1", "solos T2", "solos T3", "solos T4", "solos T5", "solos T6", "unmutes all", ""];
  const BROLE = ["T1", "T2", "T3", "T4", "T5", "T6", "P1", "P2", "P3", "P4", "MUTE"];
  const PAGE = ["HOME", "SYN", "AMP", "FILT", "FX", "LFO", "ARP", "GLO"];

  /* the status line: one short line, what the control under the pointer does now (ctx: the core's state, app.js's
   * w_info and the knobs' names on this page) */
  const PERF_S = ["REPEAT 1/4", "REPEAT 1/8", "REPEAT 1/16", "REPEAT 1/32", "REVERSE", "tape STOP", "STUTTER", "CRUSH", "WOW", "DRIVE", "KILL"];
  function line(id, c) {
    if (!id) return "";
    const kind = id[0], i = +id.slice(1);
    const mode = c.grid ? "GRID" : c.pads ? "PADS" : "KEYS";
    if (id === "lcd") return "The screen: the page, its knobs, the stage, the steps, the bars.";
    if (kind === "e") {
      const k = `${ENAME[i]} (${EK[i]})`;
      switch (i) {
        case 0: return `${k}: the track, now T${c.trk + 1}. GLO held: the tempo.`;
        case 1: return c.page === 1 ? `${k}: the machine, now ${c.mach}.` : `${k}: this page's pages, now ${c.label}.`;
        case 2: return `${k}: the pattern, now ${c.pat}; queued while playing.`;
        case 7: return `${k}: the volume; a held step's trig type.`;
        default: {
          const kn = c.knobs && c.knobs[i - 3];
          return kn && kn.s4 ? `${k}: ${kn.s4}, ${kn.name.toLowerCase()}. Hold a step: a lock.` : `${k}: nothing on ${c.label}.`;
        }
      }
    }
    if (kind === "b") {
      const k = `${BNAME[i]} (${BK[i]})`;
      switch (i) {
        case 0: return `${k}: FX, four pages. Held: PERFORM on the black keys.`;
        case 1: return `${k}: FILT, the filters; again, their envelope.`;
        case 2: return `${k}: AMP, the envelope; again, DIST VOL PAN PORT.`;
        case 3: return `${k}: three LFOs. Held: the knobs spring back.`;
        case 4: return `${k}: SYN, the machine's knobs. Held: its sound's tools.`;
        case 5: return `${k}: tempo, bus, settings. Held: copy, paste, mutes.`;
        case 6: return `${k}: six lanes; again, the track's TLEN SPD PROB LANE. Held: pick patterns.`;
        case 7: return `${k}: keeps the pattern; held, the project.`;
        case 8: return `${k}: the arpeggiator. Twice: HOLD. GLO + ARP: PADS.`;
        case 9: return `${k}: ${c.grid ? "back to notes" : "GRID, the keys become steps"}. Held: step tools.`;
        case 10: return `${k}: ${c.playing ? "stops" : c.rec && !c.grid && !c.ready ? "Ready: the first note starts it" : "starts the pattern"}.`;
        case 11: return `${k}: ${c.rec ? "recording off" : c.playing ? "records live" : "step entry"}. Held: FILL.`;
        default: {
          const up = i === 13;
          return `${k}: ${c.grid ? "the page of steps" : `T${c.trk + 1}'s octave`} ${up ? "up; in a question, YES" : "down; in a question, NO"}.`;
        }
      }
    }
    if (kind === "k") {
      const w = WHITE.indexOf(i), b = BLACK.indexOf(i), n = 53 + i + 12 * (c.oct || 0);
      if (w >= 0) {
        const now = mode === "GRID" ? `step ${c.spage * 16 + w + 1}` : mode === "PADS" ? (w < 6 ? `hits T${w + 1}` : w >= 8 && w < 14 ? `hits T${w - 7}` : "nothing") : `plays ${note(n)}`;
        return `White ${w + 1} (${KW[w]}): ${now}.${GLO_W[w] ? ` GLO held: ${GLO_W[w]}.` : ""}`;
      }
      const now = mode === "KEYS" ? `plays ${note(n)}` : b < 6 ? `T${b + 1}` : b < 10 ? (mode === "GRID" ? `page P${b - 5}` : "nothing") : "mutes the track";
      return `Black ${b + 1} (${KB[b]}): ${now}. FX held: ${PERF_S[b]}.`;
    }
    return "";
  }

  /* the guide: every control and every page, layer and mode, a line and at most two short sentences each. hi: the
   * controls lit on the drawing (e knobs, b buttons, k keys); screen: what the FM-1 shows for it, played on a core of
   * its own from the factory's A01 (screens.js: "b X" a tap, "B X" held, "u X" let go, "w ms", "e knob d", "kd i" a
   * white key held, "k i" / "K i" a white / black key tapped) */
  const ALLW = WHITE.map((k) => "k" + k), ALLB = BLACK.map((k) => "k" + k);
  const G = [
    { g: "Knobs", id: "e0", tag: "SELECT", hi: ["e0"], short: "The track, T1 to T6.", screen: "e 0 1; w 400",
      long: "Picks the track the keys play. With GLO held, the tempo; with a step held, its note; in the catalogue, the sound.", where: "; '" },
    { g: "Knobs", id: "e1", tag: "ALGORITHM", hi: ["e1"], short: "The track's machine, on any page.", screen: "b EDIT; w 300; e 1 1; w 500",
      long: "Changes the track's machine, on any page (on SYN its card shows). With a step held, accent and slide.", where: ", ." },
    { g: "Knobs", id: "e2", tag: "PRESETS", hi: ["e2"], short: "The pattern: queued to the bar's end while playing.", screen: "e 2 1; w 400",
      long: "Picks the pattern, at once when stopped, at the bar's end while playing. With a step held, its condition.", where: "[ ]" },
    { g: "Knobs", id: "eK", tag: "KNOB 1–4", hi: ["e3", "e4", "e5", "e6"], short: "The four parameters of the page in view.", screen: "b ENV; w 200; e 3 4; w 300",
      long: "Turn the four values the screen names. Hold a step and turn one: a lock on that step.", where: "Tab picks, ↑ ↓ turn" },
    { g: "Knobs", id: "e7", tag: "MASTER", hi: ["e7"], short: "The volume; a held step's trig type.", screen: "e 7 -4; w 300",
      long: "The volume. With a step held, its trig type: ALL, AMP, FILTER, LFO, a pair or NONE.", where: "/" },
    { g: "Buttons", id: "b4", tag: "EDIT", hi: ["b4"], short: "SYN: the machine's knobs A–H, then the sound library.", screen: "b EDIT; w 400",
      long: "SYN: the machine's card, A–D, then E–H, then SND. Held: + SEQ INIT, + SAVE keep, + REC / PLAY copy, paste.", where: "5 or F5" },
    { g: "Buttons", id: "b2", tag: "ENV", hi: ["b2"], short: "AMP: the envelope, distortion, volume, pan.", screen: "b ENV; w 400",
      long: "AMP: ATCK, HOLD, DEC, REL, each shown as the time you hear. Again: DIST, VOL, PAN, PORT.", where: "8 or F3" },
    { g: "Buttons", id: "b1", tag: "SEL", hi: ["b1"], short: "FILT: the filters as a band.", screen: "b SEL; w 400",
      long: "FILT: a high-pass and a low-pass as one band that follows the note. Again: its envelope.", where: "0 or F2" },
    { g: "Buttons", id: "b0", tag: "FX", hi: ["b0"], short: "FX's four pages; held, PERFORM.", screen: "b FX; w 400",
      long: "FX, DELAY, DSTR and MOD: EQ, rate, the track's echo, bits, fold, XOR, chorus. Held: PERFORM.", where: "P or F1" },
    { g: "Buttons", id: "b3", tag: "LFO", hi: ["b3"], short: "Three LFOs, two pages each.", screen: "b LFO; w 400",
      long: "Three LFOs a track, each moving any track's parameter. Held: what you turn comes back.", where: "L or F4" },
    { g: "Buttons", id: "b8", tag: "ARP", hi: ["b8"], short: "The track's arpeggiator.", screen: "b ARP; w 400",
      long: "Hold keys and it plays them in turn. Tap twice: HOLD, the notes keep going.", where: "K or F9" },
    { g: "Buttons", id: "b5", tag: "GLO", hi: ["b5"], short: "Tempo, the bus, settings; held, FUNC.", screen: "b GLO; w 400",
      long: "TIME, BUS, SET, MIDI, IN, PLAY (the mutes, and the FM-1's CPU meter). Held: copy, paste, clear, undo; the white keys mute, fill, tap, solo.", where: "G or F6" },
    { g: "Buttons", id: "b6", tag: "HOME", hi: ["b6"], short: "The pattern in six lanes; held, pick patterns.", screen: "b PLAY; w 1500",
      long: "Six lanes, the playing step framed; level, pitch, tempo, swing. Again: HOME 2, the track's own length, speed, chance (PROB) and Grids lane. Held: the keys pick patterns and banks.", where: "1 or F7" },
    { g: "Buttons", id: "b9", tag: "SEQ", hi: ["b9"], short: "GRID: the keys are the steps; held, the step tools.", screen: "b SEQ; w 400",
      long: "GRID: the white keys are sixteen steps; tap one for a trig. Held: eight step tools on the white keys.", where: "A or F10" },
    { g: "Buttons", id: "b10", tag: "PLAY", hi: ["b10"], short: "Start and stop.", screen: "b PLAY; w 1000",
      long: "Starts and stops. With REC on and stopped: Ready, the first note starts it.", where: "Space" },
    { g: "Buttons", id: "b11", tag: "REC", hi: ["b11"], short: "Record: live while playing, step entry while stopped.", screen: "b PLAY; w 300; b REC; w 600",
      long: "Playing: notes land on the nearest step. Stopped: step entry. Held: FILL; + SEQ clears the take.", where: "Enter" },
    { g: "Buttons", id: "b7", tag: "SAVE", hi: ["b7"], short: "Keep the pattern; held, the project.", screen: "b SAVE; w 300",
      long: "Keeps the pattern; GLO + SAVE goes back to it. Held: the whole project.", where: "\\ or F8" },
    { g: "Buttons", id: "bO", tag: "OCT− OCT+", hi: ["b12", "b13"], short: "The octave; in GRID the page; a held step's ratchet.", screen: "b OCTU; w 400",
      long: "The track's octave; in GRID, the page; a held step's ratchet. In a question, NO and YES.", where: "- =" },
    { g: "Keys", id: "kw", tag: "White", hi: ALLW, short: "Notes, or the 16 steps in GRID.", screen: "b SEQ; w 2300; kd 4; w 600",
      long: "Notes in KEYS, steps in GRID, tracks 1–6 in PADS. With GLO: mute, fill, tap, solo.", where: "Z X C V B N M, Q W E R T Y U I O" },
    { g: "Keys", id: "kb", tag: "Black", hi: ALLB, short: "Notes; in GRID T1–T6, P1–P4, MUTE; held FX, PERFORM.", screen: "b PLAY; w 600; B FX; w 900",
      long: "Notes in KEYS; in GRID the track, the page and mute. With FX held: eleven effects.", where: "S D F H J, 2 3 4 6 7 9" },
    { g: "Keys", id: "lcd", tag: "Screen", hi: ["lcd"], short: "One grid of five bands on every page.", screen: "b PLAY; w 1200",
      long: "The page and track, the four knobs, the stage, the steps, the bars. Light or dark: GLO > SET.", where: "GLO > SET, SCRN" },
    { g: "Held", id: "hGLO", tag: "GLO held", hi: ["b5", "b11", "b10", "b9", "b12", "b13", "b7"], short: "FUNC: copy, paste, clear, undo; mutes and solos.", screen: "B GLO; w 900",
      long: "REC copies, PLAY pastes, SEQ clears, OCT− OCT+ undo and redo. White 1–6 mute, 9–14 solo.", where: "Hold GLO" },
    { g: "Held", id: "hHOME", tag: "HOME held", hi: ["b6", ...ALLW, "k1", "k3", "k5", "k8"], short: "Pick patterns and banks.", screen: "B HOME; w 900",
      long: "White keys open the bank's patterns, queued while playing. Black 1–4 pick the bank.", where: "Hold HOME" },
    { g: "Held", id: "hEDIT", tag: "EDIT held", hi: ["b4", "e3", "e0", "e4", "b13", "b12", "b9", "b7", "b11", "b10"], short: "The sound catalogue: KNOB 1 or SELECT plays each sound.", screen: "B EDIT; w 600; e 3 2; w 400",
      long: "Held a moment, the library opens: KNOB 1 or SELECT puts each sound on the track and plays it, KNOB 2 the category. OCT+ keeps it, OCT− gives the track its own back. + SEQ, SAVE, REC, PLAY: INIT, keep, copy, paste the sound.", where: "Hold EDIT" },
    { g: "Held", id: "hSEQ", tag: "SEQ held", hi: ["b9", ...WHITE.slice(0, 8).map((k) => "k" + k), "e3", "e4", "e5", "e6", "e1"], short: "The step tools; LEN, the track's LEN and SPEED, SWING.", screen: "B SEQ; w 900",
      long: "White 1–8: clear, reverse, shift, random, ×2, cook, half. The knobs: LEN, the track's own length (PAT: the pattern's) and speed, SWING.", where: "Hold SEQ" },
    { g: "Held", id: "hPERF", tag: "PERFORM", hi: ["b0", ...ALLB, "e3", "e4", "e5", "e6"], short: "FX held: the bus's effects on the black keys.", screen: "b PLAY; w 600; B FX; w 900",
      long: "Repeats, reverse, tape stop, stutter, crush, wow, drive, kill, all on the beat. The knobs: DCAY DAMP ADPC DRIV.", where: "Hold FX; double-tap to latch" },
    { g: "Held", id: "hLFO", tag: "LFO held", hi: ["b3", "e3", "e4", "e5", "e6", "b13"], short: "Knobs that come back when let go.", screen: "B LFO; w 900",
      long: "Sweep a knob for a fill; it comes back when you let go. OCT+ keeps it.", where: "Hold LFO" },
    { g: "Held", id: "hLATCH", tag: "Latched", hi: ["b5", "b9", "b6", "b0"], short: "Double-tap GLO, SEQ, HOME or FX: the layer stays.", screen: "b GLO; w 80; b GLO; w 500",
      long: "The layer stays as if held, its button blinking. Tap it again to let go.", where: "Double-tap" },
    { g: "Held", id: "hCARD", tag: "Cards", hi: ["b1", "b2", "b8", "b10", "b12", "b13"], short: "Hold a button a second alone: what it does.", screen: "B SEL; w 1300",
      long: "SEL, ENV, ARP, PLAY or OCT held a second: the screen shows what it does.", where: "Hold a second" },
    { g: "Modes", id: "mKEYS", tag: "KEYS", hi: [...ALLW, ...ALLB], short: "The 27 keys play the track in view.", screen: "w 300",
      long: "All 27 keys play the track, from its home octave. GLO + HOME: the next mode.", where: "GLO + HOME" },
    { g: "Modes", id: "mGRID", tag: "GRID", hi: ["b9", ...ALLW, ...ALLB], short: "The white keys are the page's 16 steps.", screen: "b SEQ; w 400",
      long: "Sixteen steps a page, four pages. Black: the track, the page (held: the length), mute.", where: "SEQ" },
    { g: "Modes", id: "mPADS", tag: "PADS", hi: ["b5", "b6", "b8", ...ALLW], short: "White 1–6 and 9–14 hit tracks 1–6.", screen: "B GLO; w 100; b HOME; u GLO; w 400",
      long: "Each track at its root, so a drum stays in key. With REC, hits are written.", where: "GLO + HOME, or GLO + ARP" },
    { g: "Modes", id: "mSTEP", tag: "Held step", hi: ["e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7", "b12", "b13", ...ALLB], short: "Its note, condition, ratchet, trig type, locks.", screen: "b SEQ; w 2300; kd 0; w 700",
      long: "Every knob edits it: note, accent, condition, ratchet, trig type, locks. Hold several to edit all.", where: "GRID, hold a white key" },
    { g: "Recording", id: "rLIVE", tag: "REC live", hi: ["b10", "b11"], short: "PLAY, then REC: notes land on the nearest step.", screen: "b PLAY; w 300; b REC; w 600",
      long: "Notes land on the nearest step; knobs turned after a trig lock to it. REC held + SEQ: undo the take.", where: "PLAY, REC" },
    { g: "Recording", id: "rENTRY", tag: "Step entry", hi: ["b11", "b4", "e2"], short: "Stopped, REC: each note the next step.", screen: "b REC; w 400",
      long: "Each note is the next step. EDIT leaves a rest; PRESETS moves the cursor.", where: "REC, stopped" },
    { g: "Recording", id: "rREADY", tag: "Ready", hi: ["b11", "b10"], short: "REC, then PLAY: the first note starts it.", screen: "b REC; w 100; b PLAY; w 400",
      long: "Nothing moves until you play: the first note starts the pattern and is recorded.", where: "REC, PLAY" },
    { g: "Recording", id: "rFILL", tag: "FILL", hi: ["b11", "b5"], short: "REC held while playing.", screen: "b PLAY; w 500; B REC; w 600",
      long: "FILL steps play, !FIL steps rest. GLO + white 7: a fill on the next loop.", where: "Hold REC" },
    { g: "Recording", id: "rASK", tag: "Questions", hi: ["b13", "b12"], short: "OCT+ YES, OCT− NO.", screen: "b GLO; w 400; b GLO; w 400; b GLO; w 400; e 6 1; w 500",
      long: "Only what overwrites asks first. OCT+ is YES, anything else NO.", where: "OCT+ / OCT−" },
  ];
  /* a key on its own, every mode at once (the guide's drawing, where no state is in view): two short sentences */
  function key(k) {
    const w = WHITE.indexOf(k), b = BLACK.indexOf(k);
    if (w >= 0) {
      const pads = w < 6 ? `, PADS hits T${w + 1}` : w >= 8 && w < 14 ? `, PADS hits T${w - 7}` : "";
      return { tag: `White ${w + 1} · ${note(53 + k)}`, where: KW[w],
        long: `Plays ${note(53 + k)}; GRID step ${w + 1}${pads}.` + (GLO_W[w] ? ` GLO held: ${GLO_W[w]}.` : ` HOME held: pattern ${w + 1}.`) };
    }
    const grid = b < 6 ? `T${b + 1}` : b < 10 ? `page P${b - 5}` : "mute";
    return { tag: `Black ${b + 1} · ${note(53 + k)} · ${BROLE[b]}`, where: KB[b],
      long: `Plays ${note(53 + k)}; in GRID, ${grid}. FX held: ${PERF_S[b]}.` };
  }
  return { line, key, G, note, WHITE, BLACK, isBlack, BNAME, ENAME, PAGE };
})();
