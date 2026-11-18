// SPDX-License-Identifier: GPL-3.0-only
// The Move version's change log (move/manual.html#changelog, drawn by ../manual.js as the FM-1's): one entry per
// update, newest first, each line under one of the kinds. The FM-1's is ../changelog.js; what both get goes in both.
window.M2K_KINDS = window.M2K_KINDS || {
  machines: "Machines", sound: "Sound", sequencer: "Sequencer", workflow: "Workflow",
  patterns: "Patterns", screen: "Screen", web: "Website", fixes: "Fixes",
};
window.M2K_CHANGES = [
  {
    date: "2026-11-18", title: "MONO2K for the Move 0.7: four new performance effects",
    items: [
      ["workflow", "In PERFORM, Up now swaps both rows of keys: page 2 has three new colours on the top row and the rest of the moves with SKIP on the bottom. Down: page 1. What you hold or left on keeps playing across the swap."],
      ["sound", "SKIP (the 7th move on page 2): a CD that can't read. At every sixteenth, nine times in ten, the sixteenth just played repeats once to three times, its pitch sagging, then the music goes on."],
      ["sound", "CONSOLE (page 2's first colour): a TV speaker. The mix darkens to 8 kHz, folds to mono and rounds off softly, never louder."],
      ["sound", "DUCK (the second): the mix dips at the start of every beat and swells back over it, like a sidechain without a kick."],
      ["sound", "LO-FI (the third): an unsteady converter. The mix held at 11 kHz by a clock that wanders, in 10 bits."],
      ["patterns", "Your patterns and sounds play exactly as before."],
      ["web", "The site redrawn on the device screen's 4 px grid: one set of buttons, cards and tables, sliding tabs, folding sections, copy buttons that confirm, the screens arriving as a pixel mosaic and a beam around Download."],
    ],
  },
  {
    date: "2026-11-17", title: "MONO2K for the Move 0.6: POLY, the machines' marks, effects that stay on",
    items: [
      ["sequencer", "POLY plays chords on one track. On TRK, turn MODE to POLY: the track becomes a lead and the tracks after it its voices, as many as VOIC (knob 4) says, up to eight. T1 on four plays T1 to T4; the rest play on as always."],
      ["sound", "Every voice plays the lead's machine and its whole sound: every knob, its locks, its LFOs and its effects. The voices' own sounds wait unheard and come back when POLY is off."],
      ["workflow", "Hold a chord on the keys or play it over MIDI on the lead's channel: each note takes a free voice; past VOIC the oldest gives its voice up. Loop holds the chord until the next one."],
      ["sequencer", "Rec while playing, or Capture: a chord's lowest note goes on the lead's step and the others on its voices' steps. The lead's step decides the chord: its condition, chance, trig type, accent and locks."],
      ["screen", "TRK on POLY shows the voices and their notes; HOME brackets the voices to their lead, and a voice's track button shows the lead."],
      ["patterns", "Your patterns and sounds load and play exactly as before."],
      ["workflow", "An effect that stays on: in PERFORM, tap a key twice and its effect plays after you let go, lit white. Tap it again to let go. Colours stack; a move gives way to the next move. The PERFORM pad again lets go of them all."],
      ["sound", "A repeat keeps landing on the beat however long it plays: its loop is now the step's exact length (it drifted up to 5 ms a minute)."],
      ["screen", "SYN shows the machine's mark, the FM-1's 3D picture of it, beside its name: it sways in time with the music and hops on the track's hits, higher for a louder one. SWAV-SAW's three teeth rise and fall with its level while it plays."],
    ],
  },
  {
    date: "2026-11-16", title: "MONO2K for the Move 0.5: regular ADSR envelopes",
    items: [
      ["sound", "The amp is a regular ADSR: AMP's knobs 1-4 are ATCK DEC SUS REL. SUS holds a level while the note lasts and REL takes it down after; at 0 the envelope is as it was, so your patterns play exactly as before."],
      ["sound", "The filter's envelope is an ADSR too: FILT's knobs 5-8 are ATCK DEC SUS REL."],
      ["workflow", "AMP's pad again: HOLD. FILT's pad again: B-OF and W-OF, how far the envelope moves the band."],
    ],
  },
  {
    date: "2026-11-16", title: "The manual, in the order you make music",
    items: [
      ["web", "The Move's manual in the order you make music: play the demos, build a beat, shape a sound, add effects, add movement, make it vary, then patterns into a song, play it live, in time with Move. Each page starts with a few steps to do; every control and page is under Reference."],
    ],
  },
  {
    date: "2026-11-07", title: "MONO2K for the Move 0.4.1: a step's chance with one hand",
    items: [
      ["workflow", "Hold a step and click the jog: the jog now sets the step's chance instead of its note, so one hand holds and the other turns. It stays on the chance for the next step you hold; click again for the note. The screen tags JOG under what it sets. Shift + jog still works."],
    ],
  },
  {
    date: "2026-11-07", title: "MONO2K for the Move 0.4: a held step says what it holds, and a note can last",
    items: [
      ["screen", "Hold a step and the screen shows its condition, note and ratchet, each over what turns it: Shift + jog, the jog, Up and Down. They were being set unseen. A tap stays quiet, and a knob touched keeps the screen."],
      ["sequencer", "A note's length: hold a step a moment and tap a later empty step, and the note lasts to it, up to 8 steps, even on a sound that would have decayed sooner. The same step again gives the sound's own length back."],
    ],
  },
  {
    date: "2026-11-02", title: "MONO2K for the Move 0.3: PADS, Capture, a cleaner knob row",
    items: [
      ["workflow", "PADS on the Sample button: the bottom row plays the eight tracks at their own notes, the row above mutes them. Sample again for the piano. REC records each hit into its track."],
      ["sequencer", "Capture: MONO2K always listens to the keys. Press Capture and the last whole bars you played go into the steps, where you played them (stopped: from bar 1). It adds, never more than the track's length; one Undo takes it back. In PADS every track gets its own hits."],
      ["screen", "The knob row: every knob in the same box, apart from its neighbours, solid shapes instead of dots, every line picture from the box's top to its foot. Resonance shows as a solid corner."],
      ["web", "The playable Move on the site has Sample and Capture."],
    ],
  },
  {
    date: "2026-11-01", title: "MONO2K for the Move 0.2: the first release",
    items: [
      ["machines", "The FM-1's nineteen machines, its sequencer, PERFORM and the bus, on eight tracks. The same core as the FM-1's, so the same sound, sample for sample."],
      ["workflow", "Every page a pad: the sound's eight on the top row; HOME TRK PAT GRID MIX GLO, SOUNDS and PERFORM on the second. The keyboard a two-row piano on the bottom two; the steps the sequencer on every page."],
      ["workflow", "MIX: the eight tracks' levels on the eight knobs."],
      ["sequencer", "In time with the Move: one Play starts both, at Move's tempo, locked to its bar. Shift + knob 1 sets the tempo on any page, Move's too with Link on."],
      ["workflow", "PERFORM on the keys: eight colours that stack over eight moves that take turns; Up for six more moves."],
      ["screen", "A screen of its own for the Move's 128 × 64: every page one live picture, the knobs as pictures, a knob's name and value large while touched."],
      ["patterns", "Four banks of 16 patterns, bank A the FM-1's demos; saved with Shift + Rec and on closing, kept across updates."],
      ["web", "Its own pages: home, manual and install, a switch away from the FM-1's."],
    ],
  },
];
