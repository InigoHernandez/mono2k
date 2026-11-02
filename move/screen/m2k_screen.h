/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the 128 x 64 1-bit screen (move/screen/DESIGN.md), and the pages the pads lead to.
 *
 * m2k_screen.c is part of the core's unity build: include it after firmware/src/m2k_core.c (as move/m2k_move.c
 * includes the core). It works in two halves, so the drawing can run off the audio thread:
 *   m2k_screen_gather   reads the core (the pattern, the kits, the machines' pages and value formats, the sequencer,
 *                       the monitor) and the controller's moment (m2k_scr_t) into a plain view (ms_view_t): numbers
 *                       and strings, nothing that points into the core. The audio thread's, between blocks.
 *   m2k_screen_draw     draws the frame from a view alone (and const tables and pure functions): any thread.
 * The frame: 128 x 64, a row 16 bytes, MSB first, 1 = a lit pixel. Integer only, nothing allocated. The gather reads
 * mon.peak and clears it, as m2k_frame does: a build calls one or the other. */
#ifndef M2K_SCREEN_H
#define M2K_SCREEN_H

#include <stdint.h>

#define MS_W 128
#define MS_H 64
#define MS_BYTES (MS_W * MS_H / 8)

/* the pages. Every one has its own pad (MS_PAD_PAGE); the first MS_PAGES are pages of eight knobs */
enum {
    MS_HOME,                    /* the pattern: eight lanes; LEVL PTCH BPM SWNG TLEN SPD PROB LANE (no pictures) */
    MS_SYN,                     /* the machine's A..H */
    MS_AMP,                     /* ATCK HOLD DEC REL DIST VOL PAN PORT */
    MS_FILT,                    /* BASE WDTH HPQ LPQ ATK DEC B-OF W-OF */
    MS_EFX,                     /* EQF EQG SRR DTIM DSND DFB DBAS DWID */
    MS_DSTR,                    /* BITS FOLD XOR VERB, then the modulation effect: TYPE RATE DPTH MIX */
    MS_LFO1, MS_LFO2, MS_LFO3,  /* TRCK DEST WAVE TRIG MULT SPD PHAS DPTH */
    MS_TRK,                     /* the track and its arp: PTCH LEVL MODE SPD RNG GATE, BANK (DIGI-WAV's) */
    MS_PAT,                     /* the pattern's time and the track's: BPM SWNG LEN ACNT TLEN SPD PROB LANE */
    MS_GRID,                    /* Grids: X Y CHAOS RES BD SD HH, the track's LANE */
    MS_MIX,                     /* the eight tracks' LEVL, a knob a track (a proposal) */
    MS_GLO,                     /* the bus and the instrument: DCAY DAMP ADPC DRIV, MUTES AT BAR */
    MS_PERF,                    /* PERFORM: the bus's four; the keyboard plays the effects */
    MS_PAGES,                   /* (the pages of knobs: this many) */
    MS_CAT = MS_PAGES,          /* SOUNDS: the library, a list that previews as it scrolls */
    MS_PATS,                    /* the patterns, a list the jog scrolls (no pad: the jog's) */
    MS_COUNT
};

/* the page pads: the Move's top two rows, pads 16..31 (pad 0 bottom left, rows of eight). The top row is the sound of
 * the track in view, the second the rest; each row a colour family on the LEDs, and the screen's header draws the pad
 * leading to the page in view */
#define MS_PAD0 16
static const uint8_t MS_PAD_PAGE[16] = {
    MS_HOME, MS_TRK, MS_PAT, MS_GRID, MS_MIX, MS_GLO, MS_CAT, MS_PERF,      /* pads 16..23: the second row */
    MS_SYN, MS_AMP, MS_FILT, MS_EFX, MS_DSTR, MS_LFO1, MS_LFO2, MS_LFO3,   /* pads 24..31: the top row */
};
enum { MS_FAM_SOUND, MS_FAM_SEQ, MS_FAM_FN, MS_FAM_NONE };   /* the sound, the sequence and mix, the two functions */
static inline int ms_pad_of(int page) { for (int i = 0; i < 16; i++) if (MS_PAD_PAGE[i] == page) return i; return -1; }
static inline int ms_family(int page)
{
    const int p = ms_pad_of(page);
    return p < 0 ? MS_FAM_NONE : p >= 8 ? MS_FAM_SOUND : p >= 6 ? MS_FAM_FN : MS_FAM_SEQ;
}

/* what the controller says about the moment. Everything else is read from the core */
typedef struct {
    uint8_t page;               /* MS_* in view */
    uint8_t track;              /* the track in view, 0..7 */
    int8_t touch;               /* the knob shown large (touched, or turned a moment ago), -1 none */
    uint8_t tempo;              /* what is shown large is the tempo, whatever the knob is on the page (Shift + knob 1) */
    uint8_t held;               /* the first step held (0..63): the knobs show its locks; 0xFF none */
    uint8_t spage;              /* the bar the 16 steps show (0..3) */
    uint8_t cat_sel, cat_cat;   /* MS_CAT: the slot shown (0..127), the category (CAT_*, 0 all) */
    uint8_t pat_sel;            /* MS_PATS: the pattern under the cursor */
    uint8_t sync;               /* the tempo is the Move's (SYNC); 0 MONO2K's own (INT: the header says so) */
    uint16_t bpm10;             /* the tempo heard, tenths (0: the pattern's) */
    uint8_t shift, mute;        /* Shift, Mute held */
    uint8_t bank;               /* the bank Shift + steps pick from */
    uint8_t perf, perf_pg;      /* PERFORM's keys on the keyboard; their page (0 each bank's 1-8, 1 its 9-11) */
    const char *toast, *toast2; /* a message a moment after an action (0 or "" none) */
    uint32_t ms;                /* the time in ms: the meters' fall, the modulation's sweep */
} m2k_scr_t;

/* ---------------------------------------------------------------------------------------- the view, plain */

#define MS_NROW 5                         /* a list's rows */
typedef struct {
    uint8_t pic;                          /* the picture (m2k_screen.c MP_*), 0xFF a free knob (nothing drawn) */
    uint8_t sub;                          /* its variant: a segment's shape, a filter knob's part, a square wave... */
    uint8_t n, c;                         /* a choice's count, the one chosen */
    uint8_t lock;                         /* the value is a held step's lock */
    uint16_t q;                           /* where the value sits, 0..256 */
    char val[8];                          /* the value as its column writes it (13 px; a name 14) */
    int8_t wy[13];                        /* a wave's rows about its middle, -6..6, a column of the box each */
} ms_knob_t;

typedef struct {
    int8_t k;                             /* the knob shown large, -1 none */
    uint8_t lock, bip, ruler, step;       /* a lock; bipolar; a ruler under it; the held step + 1 (0 none) */
    uint16_t q;
    char name[24], s4[8], val[16], unit[12];
} ms_touch_t;

typedef struct {
    uint8_t idx, icon, flags;             /* a slot or a pattern; its machine's icon; MS_ROW_* */
    char name[9];
} ms_row_t;
#define MS_ROW_EMPTY 1u
#define MS_ROW_OPEN  2u                   /* the pattern open (playing) */
#define MS_ROW_NEXT  4u                   /* the pattern queued */

typedef union {
    struct { uint16_t trig[8], hollow[8]; uint8_t len[8]; int8_t pos[8]; } home;
    struct { char cls[12], style[12], model[10]; } syn;
    struct { uint8_t a, h, d, r, st; int32_t lv; } amp;
    struct { int16_t hp, lp, hp1, lp1; uint8_t hq, lq; } filt;
    struct { uint8_t f, g, dt, snd, fb, wid; } efx;
    struct { uint8_t bits, fold, xr, type, dep, mix; uint32_t inc; } dstr;
    struct { uint8_t mode, n, rng, hold; int16_t on; } arp;
    struct { char dest[8], cyc[16]; uint8_t wave, on; int16_t dp; uint16_t ph; int32_t val; } lfo;
    struct { uint16_t bpm10; uint8_t sync, len, tlen; int8_t beat, ppos, tpos; } tm;
    struct { uint8_t x8, y8, lane, cyc; int8_t kn; uint8_t lv[3][32]; uint32_t hit[3]; } grid;
    struct { uint8_t meter[8]; } mix;
    struct { uint8_t verb, damp, adpc, driv; } glo;
    struct { uint32_t on; uint8_t pg; } perf;
    struct { uint8_t n, first, at; ms_row_t row[MS_NROW]; char cat[8]; } list;
} ms_stage_t;

typedef struct {
    uint8_t page, pad, fam, perf, track, ntrk;   /* the pad leading here (0..15, 0xFF none), its family */
    uint8_t icon, muted, hold, mutes;            /* the track's machine icon, muted, its arp holding; every mute */
    uint8_t playing, rec, shift, mute, bank, spage, npage;
    uint8_t clk_int;                             /* MONO2K on its own tempo (INT), not Move's */
    char slot[4], next[4], pname[9];
    char toast[24], toast2[28];
    uint32_t ms;
    ms_knob_t knob[8];
    ms_touch_t touch;
    ms_stage_t st;
} ms_view_t;

/* once, before the first gather (off the audio thread: it works out which values each kind writes) */
void m2k_screen_init(void);
/* the moment into a view: reads the core (the audio thread's, between blocks) */
void m2k_screen_gather(ms_view_t *v, const m2k_scr_t *s);
/* a view into a frame (MS_BYTES): touches nothing else */
void m2k_screen_draw(const ms_view_t *v, uint8_t *fb);
/* both, into the renderer's own frame (the sheets; a build drawing in its render) */
const uint8_t *m2k_screen(const m2k_scr_t *s);
/* knob k (0..7) of a page of knobs on track t: the core's parameter (0..P_COUNT-1), a pattern global (0x80 | G_*), a
 * setting (0xC0 | S_*), or 0xFF for a free knob; and the track it is of (MIX's knob k: track k) */
int m2k_screen_param(int page, int t, int k);
int m2k_screen_knob_track(int page, int t, int k);
/* a page's name, as the header writes it */
const char *m2k_screen_page_name(int page);

#endif
