/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the page state. The controller (m2k_move_ui.c, mv_view_build) fills one on the audio thread and
 * publishes it; the drawer reads a copy on its own thread and draws the 128 x 64 screen from its screen part alone
 * (scr: move/screen/m2k_screen.h's plain view, m2k_screen_draw). Plain data: nothing in it points into the core. The
 * LEDs ride along: ui.js sets them as they are here; the rest is the controller's summary, for the tests. */
#ifndef M2K_MOVE_VIEW_H
#define M2K_MOVE_VIEW_H

#include <stdint.h>
#include "screen/m2k_screen.h"

/* what is over the page: nothing, the sound catalogue (SOUNDS), the patterns' list (the jog), Shift's layer (patterns,
 * bank, machine), Mute's layer */
enum { MV_OV_NONE, MV_OV_PATS, MV_OV_CAT, MV_OV_SHIFT, MV_OV_MUTE };

/* the tempo's source: Move's clock (followed, or waited for), or MONO2K's own (Move sent none when PLAY was pressed) */
enum { MV_SYNC_INT, MV_SYNC_MOVE };

/* a knob */
#define MV_K_FREE   1u                 /* nothing on it */
#define MV_K_LOCK   2u                 /* the value is a held step's lock */
#define MV_K_BIP    4u                 /* bipolar: 64 the middle */
#define MV_K_CHOICE 8u                 /* a list of choices, not a range */
#define MV_K_MOVE   16u                /* the tempo, Move's and MONO2K's: turning it asks Move for it too */
typedef struct {
    uint16_t raw;                      /* 0..127 (the tempo 0..255) */
    uint8_t flags;                     /* MV_K_* */
} mv_knob_t;

/* how ui.js turns each knob's detents into steps (its curve, ui.js): a range, a short list's options (slower), the
 * tempo (tenths of BPM, whole ones fast), nothing on it. Sent in the frame as '0'.. '3' */
enum { MV_KK_RANGE, MV_KK_CHOICE, MV_KK_TEMPO, MV_KK_NONE };
#define MV_KK_LIST 32                  /* a list this long or shorter turns as options; longer ones as a range */

/* the tempo asked of Move (m2k_move.c tq): none, waiting for Move to follow (MONO2K at it already), MONO2K alone at
 * it (Move did not follow: no Link, or Move->Schwung off) */
enum { MV_TQ_NONE, MV_TQ_WAIT, MV_TQ_ALONE };

/* a step's cell on the page shown */
#define MV_ST_TRIG 1u
#define MV_ST_LOCK 2u                  /* the trig has parameter locks */
#define MV_ST_PLAY 4u                  /* the playhead */
#define MV_ST_PAST 8u                  /* past the track's length: no step there */
#define MV_ST_HELD 16u                 /* held (its locks on the knobs) */
#define MV_ST_ACC  32u                 /* accent */
#define MV_ST_COND 64u                 /* a condition or a ratchet */
#define MV_ST_BEAT 128u                /* a beat's first sixteenth */

/* the LEDs as palette indices (pads, steps, track buttons, Rec, Sample: RGB) or brightness 0..127 (the white buttons,
 * MV_BTN_CC). Sample and Capture last, so the slots before them stay where they were */
#define MV_NBTN 13
static const uint8_t MV_BTN_CC[MV_NBTN] = {86, 88, 56, 60, 119, 62, 63, 55, 54, 50, 58, 118, 52};   /* Rec Mute Undo Copy
                                                                         * Delete Left Right Up Down Menu Loop Sample Capture */
typedef struct {
    uint8_t step[16], pad[32], trk[4], btn[MV_NBTN];
} mv_leds_t;

typedef struct {
    uint32_t seq;                      /* bumps with every publish */
    uint8_t page;                      /* MS_* in view */
    uint8_t trk;                       /* the track in view from 0 */
    char machine[10];                  /* "SID-6581" */
    char slot[4], next[4];             /* the pattern, the one queued for the bar's end ("" none) */
    uint16_t bpm10;                    /* the tempo heard (Move's while following), tenths */
    uint8_t sync;                      /* MV_SYNC_* */
    uint8_t following;                 /* MONO2K plays to Move's clock now */
    uint8_t playing, rec, shift;
    mv_knob_t knob[8];
    int8_t focus;                      /* the knob shown large (touched, or turned a moment ago), -1 none */
    uint8_t tempo_focus;               /* what is shown large is the tempo (Shift + knob 1) */
    uint8_t kkind[8];                  /* MV_KK_*: how ui.js turns each knob */
    uint8_t tq_state, tq_seq;          /* MV_TQ_*; the tempo asked's number (bumps with each change; 0 never) */
    uint16_t tq_b10;                   /* the tempo asked, tenths: ui.js writes it for Move (desired-tempo) */
    uint8_t step[16];                  /* MV_ST_* for the 16 shown */
    uint8_t spage, npage, follow;      /* the bar shown, how many the track reaches, whether it follows the playhead */
    uint8_t held;                      /* steps held */
    uint8_t mutes;                     /* a bit a track, as it will be (a mute waiting for the bar shown as done) */
    int8_t oct;                        /* the track's keyboard octave, -3..3 */
    uint8_t bank;                      /* the bank Shift + steps pick from */
    uint8_t overlay;                   /* MV_OV_* */
    uint8_t pat_sel;                   /* the patterns' list's cursor */
    uint8_t perf, perf_pg;             /* PERFORM's keys on the keyboard, their page */
    uint8_t kpads;                     /* the keyboard rows are PADS (Sample): the tracks, their mutes over them */
    uint16_t capn;                     /* notes played since the last Capture (what it would write from) */
    char cat_slot[6], cat_name[9];     /* the catalogue: "S012", its name ("INIT" empty) */
    uint32_t perf_on;                  /* PERFORM's effects on, a bit each (22) */
    char toast[24], toast2[28];        /* a message, a moment after an action ("" none) */
    mv_leds_t led;
    ms_view_t scr;                     /* the screen's view: all the drawer reads */
} mv_view_t;


#endif
