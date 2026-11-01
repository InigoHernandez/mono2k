/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the 128 x 64 1-bit screen (move/screen/DESIGN.md).
 *
 * m2k_screen.c is part of the core's unity build: include it after firmware/src/m2k_core.c (as move/m2k_move.c
 * includes the core), so it reads the core's own state and names: the pattern, the kits, the machines' pages and
 * value formats (m2k_ui.c's pinfo and fmt), the sequencer, the monitor. The controller fills an m2k_scr_t and asks
 * for a frame; nothing else is kept between frames but the meters' fall.
 *
 * The frame: 128 x 64, a row 16 bytes, MSB first, 1 = a lit pixel (the FM-1's framebuffer's order). Integer only,
 * nothing allocated. It reads mon.peak and clears it, as m2k_frame does: call one or the other, not both. */
#ifndef M2K_SCREEN_H
#define M2K_SCREEN_H

#include <stdint.h>

#define MS_W 128
#define MS_H 64
#define MS_BYTES (MS_W * MS_H / 8)

/* the pages. The first twelve are pages of eight knobs (m2k_screen_param), turned by the jog; the rest are views */
enum {
    MS_HOME,                    /* the pattern: the eight lanes; LEVL PTCH BPM SWNG TLEN SPD PROB LANE */
    MS_SYN,                     /* the machine's A..H */
    MS_AMP,                     /* ATK HOLD DEC REL DIST VOL PAN PORT: the envelope, one live curve */
    MS_FILT,                    /* BASE WDTH HPQ LPQ FATK FDEC BOF WOF: the response, one live curve */
    MS_EFX,                     /* EQF EQG SRR DTIM DSND DFB DBAS DWID */
    MS_DSTR,                    /* BITS FOLD XOR VERB, then the modulation effect: TYPE RATE DPTH MIX */
    MS_ARP,                     /* PTCH LEVL MODE SPD RNG GATE */
    MS_LFO1, MS_LFO2, MS_LFO3,  /* TRCK DEST WAVE TRIG MULT SPD PHAS DPTH */
    MS_GLO,                     /* the pattern's time and bus: BPM SWNG LEN ACNT DCAY DAMP ADPC DRIV */
    MS_GRID,                    /* Grids: X Y CHAOS RES BD SD HH, the track's LANE */
    MS_SET,                     /* the instrument's: CHAN SYIN SYOU NOTE NTIN THRU MUTE MOTN */
    MS_PAGES,                   /* (the pages of knobs: this many) */
    MS_CAT = MS_PAGES,          /* the sound catalogue: a list that previews as it scrolls */
    MS_PERF,                    /* PERFORM: the pads' effects, the bus on the knobs */
    MS_LIST,                    /* the page list (a jog click) */
    MS_COUNT
};

/* what the controller says about the moment. Everything else (the pattern, the kits, the locks, the playhead, the
 * tempo, the mutes, the arps, PERFORM's effects on, the library) is read from the core */
typedef struct {
    uint8_t page;               /* MS_* */
    uint8_t track;              /* the track in view, 0..7 */
    int8_t touch;               /* the knob touched (0..7): its value large on the stage; -1 none */
    uint8_t held;               /* a step held (0..63): the knobs show its locks; 0xFF none */
    uint8_t spage;              /* the 16 step keys' page (0..3) */
    uint8_t list;               /* MS_LIST: the page under the cursor */
    uint8_t cat_sel;            /* MS_CAT: the slot shown (0..127) */
    uint8_t cat_cat;            /* MS_CAT: the category shown (CAT_*, 0 all) */
    uint8_t sync;               /* the tempo follows the Move's (SYNC); 0 MONO2K's own */
    uint8_t shift;              /* Shift held: the track buttons reach tracks 5-8 */
    uint32_t ms;                /* the time in ms: the blinks, the meters' fall */
} m2k_scr_t;

/* draws the frame (MS_BYTES) */
const uint8_t *m2k_screen(const m2k_scr_t *s);
/* knob k (0..7) of a page of knobs on track t: the core's parameter (0..P_COUNT-1), a pattern global (0x80 | G_*), a
 * setting (0xC0 | S_*), or 0xFF for a free knob. The controller turns what this names */
int m2k_screen_param(int page, int t, int k);
/* a page's name, as the header and the page list write it */
const char *m2k_screen_page_name(int page);

#endif
