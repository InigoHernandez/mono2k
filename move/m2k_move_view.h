/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the page state. The controller (m2k_move_ui.c, mv_view_build) fills one on the audio thread and
 * publishes it; a renderer reads a copy on its own thread and draws the 128 x 64 screen from it alone (m2k_move_draw.c,
 * the plain one; the final design replaces it). Plain data: every string NUL-ended, in capitals, as the FM-1 writes
 * them; nothing in it points into the core. The LEDs ride along: ui.js sets them as they are here. */
#ifndef M2K_MOVE_VIEW_H
#define M2K_MOVE_VIEW_H

#include <stdint.h>

/* the pages the jog turns, in its order (m2k_move_ui.c MV_PAGE) */
enum { MV_PG_SYN, MV_PG_AMP, MV_PG_FILT, MV_PG_EFX, MV_PG_DSTR, MV_PG_TRK, MV_PG_LFO1, MV_PG_LFO2, MV_PG_LFO3,
       MV_PG_SEQ, MV_PG_GRID, MV_PG_PAT, MV_PG_COUNT };

/* what is over the page: nothing, the page list (jog click), the sound catalogue (Menu held), Shift's layer (pattern,
 * bank, PERFORM, machine), Mute's layer */
enum { MV_OV_NONE, MV_OV_PAGES, MV_OV_CAT, MV_OV_SHIFT, MV_OV_MUTE };

/* the tempo's source: Move's clock (followed, or waited for), or MONO2K's own (Move sent none when PLAY was pressed) */
enum { MV_SYNC_INT, MV_SYNC_MOVE };

/* a knob */
#define MV_K_FREE   1u                 /* nothing on it ("-") */
#define MV_K_LOCK   2u                 /* the value is a held step's lock */
#define MV_K_BIP    4u                 /* bipolar: 64 the middle */
#define MV_K_CHOICE 8u                 /* a list of choices, not a range */
#define MV_K_MOVE   16u                /* Move sets it (the tempo, followed): turning it does nothing */
typedef struct {
    char label[8];                     /* the FM-1's four letters: "ATCK" */
    char value[12];                    /* "64", "1/16", "INF" */
    char unit[16];                     /* "MS", "SEMI", "" */
    char name[20];                     /* the long name: "FILTER ATTACK" */
    uint16_t raw;                      /* 0..127 (the tempo 0..255) */
    uint8_t flags;                     /* MV_K_* */
} mv_knob_t;

/* a step's cell on the page shown */
#define MV_ST_TRIG 1u
#define MV_ST_LOCK 2u                  /* the trig has parameter locks */
#define MV_ST_PLAY 4u                  /* the playhead */
#define MV_ST_PAST 8u                  /* past the track's length: no step there */
#define MV_ST_HELD 16u                 /* held (its locks on the knobs) */
#define MV_ST_ACC  32u                 /* accent */
#define MV_ST_COND 64u                 /* a condition or a ratchet */
#define MV_ST_BEAT 128u                /* a beat's first sixteenth */

/* the LEDs as palette indices (pads, steps, track buttons, Rec) or brightness 0..127 (the white buttons, MV_BTN_CC) */
#define MV_NBTN 11
static const uint8_t MV_BTN_CC[MV_NBTN] = {86, 88, 56, 60, 119, 62, 63, 55, 54, 50, 58};   /* Rec Mute Undo Copy Delete
                                                                                            * Left Right Up Down Menu Loop */
typedef struct {
    uint8_t step[16], pad[32], trk[4], btn[MV_NBTN];
} mv_leds_t;

typedef struct {
    uint32_t seq;                      /* bumps with every publish */
    /* the header */
    uint8_t page;                      /* MV_PG_* */
    char page_name[8];                 /* "AMP" */
    uint8_t trk, ntrk;                 /* the track in view from 0, how many */
    char machine[10];                  /* "SID-6581" */
    char slot[4], name[9];             /* the pattern: "A01", "CIRCUIT" */
    char next[4];                      /* the pattern queued for the bar's end ("" none) */
    uint16_t bpm10;                    /* the tempo heard (Move's while following), tenths */
    uint8_t sync;                      /* MV_SYNC_* */
    uint8_t following;                 /* MONO2K plays to Move's clock now */
    uint8_t playing, rec, shift;
    /* the knobs */
    mv_knob_t knob[8];
    int8_t focus;                      /* the knob shown large (touched, or turned a moment ago), -1 none */
    /* the steps */
    uint8_t step[16];                  /* MV_ST_* for the 16 shown */
    uint8_t spage, npage, follow;      /* the bar shown, how many the track reaches, whether it follows the playhead */
    int16_t play;                      /* the track's step playing, from 0 (-1 stopped) */
    uint8_t len;                       /* the track's length */
    uint8_t held;                      /* steps held */
    char held_note[6];                 /* the first held step's note: "C4" */
    uint8_t mutes;                     /* a bit a track, as it will be (a mute waiting for the bar shown as done) */
    int8_t oct;                        /* the track's keyboard octave, -3..3 */
    uint8_t bank;                      /* the bank Shift + steps pick from */
    /* what is over the page */
    uint8_t overlay;                   /* MV_OV_* */
    uint8_t list_sel;                  /* the page list's choice */
    char cat_slot[6], cat_name[9], cat_cat[8];   /* the catalogue: "S012", its name ("INIT" empty), its category shown */
    uint32_t perf;                     /* PERFORM's effects on, a bit each (22) */
    char toast[24], toast2[28];        /* a message, a moment after an action ("" none) */
    mv_leds_t led;
} mv_view_t;

#endif
