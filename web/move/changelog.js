// SPDX-License-Identifier: GPL-3.0-only
// The Move version's change log (move/manual.html#changelog, drawn by ../manual.js as the FM-1's): one entry per
// update, newest first, each line under one of the kinds. The FM-1's is ../changelog.js; what both get goes in both.
window.M2K_KINDS = window.M2K_KINDS || {
  machines: "Machines", sound: "Sound", sequencer: "Sequencer", workflow: "Workflow",
  patterns: "Patterns", screen: "Screen", web: "Website", fixes: "Fixes",
};
window.M2K_CHANGES = [
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
