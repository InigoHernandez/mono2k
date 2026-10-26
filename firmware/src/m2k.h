/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K: six mono machines, a 64-step sequencer with parameter locks, and a lofi bus, for the M-VAVE FM-1.
 * This core has no platform code: the FM-1 build calls it from the audio ISR and the main loop, the browser
 * build from an AudioWorklet, the host build from tests. Integer only (the AC791N has no FPU and no libgcc). */
#ifndef M2K_INCLUDED
#define M2K_INCLUDED

#include <stdint.h>

#define M2K_SR        44100
#define M2K_SUB       16          /* control-rate block: envelopes, LFOs, params, triggers */
#define M2K_TRACKS    6
#define M2K_STEPS     64
#ifndef M2K_PATTERNS
#define M2K_PATTERNS  64          /* per project: 4 banks of 16 (the FM-1's first build: 16, bank A, for its RAM) */
#endif
#define M2K_BANKS     (M2K_PATTERNS / 16)
#define M2K_SOUNDS    128         /* the sound library: S001..S128 */
#ifndef M2K_PROJECTS
#define M2K_PROJECTS  4           /* (the FM-1's first build: 1) */
#endif
#define M2K_LOCKS     384         /* parameter locks per pattern (track, step, param, value) */
#define M2K_W         240
#define M2K_H         240
#define M2K_FB_BYTES  (M2K_W * M2K_H / 8)

/* big buffers go to the FM-1's pool RAM (Felucca's linker script); elsewhere they are plain statics (a host test of
 * the FM-1's build sets it empty) */
#ifndef POOL
#ifdef M2K_FW
#define POOL __attribute__((section(".pool")))
#else
#define POOL
#endif
#endif

/* the FM-1's controls, in the order of the platform's input layer */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTD, B_OCTU, B_COUNT };
enum { E_SELECT, E_ALGO, E_PRESET, E_K1, E_K2, E_K3, E_K4, E_MASTER, E_COUNT };
#define M2K_KEYS 27               /* F3..G5: 16 lower ("white") and 11 upper ("black") keys */

/* track parameters, 0..127 each (bipolar ones centre on 64) */
enum {
    P_SYN = 0,                    /* 0..7: the machine's A..H */
    P_ATK = 8, P_HOLD, P_DEC, P_REL, P_DIST, P_VOL, P_PAN, P_PORT,
    P_BASE = 16, P_WDTH, P_HPQ, P_LPQ, P_FATK, P_FDEC, P_BOF, P_WOF,
    P_EQF = 24, P_EQG, P_SRR, P_DTIM, P_DSND, P_DFB, P_DBAS, P_DWID,
    P_BITS = 32, P_FOLD, P_XOR, P_SPU,
    P_PTCH = 36, P_LEVL, P_AMOD, P_ASPD, P_ARNG, P_GATE,
    P_LFO = 42,                   /* 3 x 8: TRCK DEST WAVE TRIG MULT SPD PHAS DPTH */
    P_MODT = 66, P_MODR, P_MODD, P_MODM,   /* the modulation effect: CHOR FLNG PHAS, rate, depth, mix */
    P_WBNK = 70,                  /* the wave machines' bank: MONO2K's 32, the owner's 64 (format 8) */
    P_COUNT = 71
};
enum { L_TRCK, L_DEST, L_WAVE, L_TRIG, L_MULT, L_SPD, L_PHAS, L_DPTH };

/* global (pattern) parameters */
enum {
    G_TMPO, G_SWNG, G_LEN, G_ACNT,          /* tempo is 30 + v BPM (seq_bpm); accent amount */
    G_TAPE, G_ADPC, G_GLCH, G_GSIZ,          /* the lofi bus */
    G_VERB, G_DAMP, G_HISS, G_DRIV,
    G_SPD1,                                  /* + t: track t's speed, 1/8x .. 2x (the pattern format's 15th) */
    G_COUNT = G_SPD1 + 6
};
#define G_COUNT_V14 G_SPD1                   /* the globals before the speeds */
#define SPD_1X 82                            /* a speed's value at 1x (the fifth of seven) */

/* machines */
/* new ones go last: patterns keep the number (GND-PERC, MONO2K's own noise, was GND-NOIS until the Monomachine's took
 * the name) */
enum { M_GSIN, M_GPRC, M_SID, M_FMS, M_FMD, M_SAW, M_ENS, M_WAV, M_VOX, M_FMP, M_PUL, M_FXC, M_FXR, M_FXD, M_GNOI,
       M_DKIK, M_DSNR, M_DHAT, M_DCYM,       /* the DRM- drums, after the LXR's voices */
       M_COUNT };

typedef struct {
    uint8_t note;                 /* MIDI note */
    uint8_t flags;                /* bit0 trig, bit1 accent, bit2 slide */
    uint8_t cond;                 /* trig condition: C_* */
    uint8_t rtrg;                 /* ratchet: hits in the step, 0 or 1 = one */
} m2k_step_t;

/* trig conditions, after the modern Elektron sequencers: chances, every A of B loops, first, previous, fill */
enum {
    C_ALWAYS, C_P75, C_P50, C_P33, C_P25, C_P12,
    C_1_2, C_2_2, C_1_3, C_2_3, C_3_3, C_1_4, C_2_4, C_3_4, C_4_4,
    C_1ST, C_N1ST, C_PRE, C_NPRE, C_FILL, C_NFILL, C_COUNT
};
#define ST_TRIG 1u
#define ST_ACC  2u
#define ST_SLD  4u
/* the trig tracks (the Monomachine's AMP, FILTER and LFO, manual p.58): a trig restarts the ones it does not mask; all
 * three masked is a trigless trig, its note and its locks alone. Clear in every older pattern, so they play ALL */
#define ST_NOAMP 8u
#define ST_NOFLT 16u
#define ST_NOLFO 32u
#define ST_TTYPE (ST_NOAMP | ST_NOFLT | ST_NOLFO)

typedef struct { uint8_t trk, step, param, val; } m2k_lock_t;   /* trk 0xFF: free */

typedef struct {
    char name[8];
    uint8_t machine[M2K_TRACKS];
    uint8_t kit[M2K_TRACKS][P_COUNT];
    uint8_t glob[G_COUNT];
    uint8_t mute;                 /* bit per track */
    uint8_t pad[3];
    m2k_step_t step[M2K_TRACKS][M2K_STEPS];
    m2k_lock_t lock[M2K_LOCKS];
} m2k_pattern_t;

/* settings: the instrument's, not a pattern's; saved with the store. 0..127 */
/* S_SCREEN: below 64 light (ink on paper), from 64 dark (the negative). GLO > MIDI: S_MCH track 1's channel (the
 * others on the ones after it; (v * 16) >> 7), S_SYNI where the clock in comes from (OFF USB TRS BOTH, m2k_seq.c's
 * syin_choice: an old OFF stays OFF, an old ON is BOTH), S_SYNO the clock out, S_NOTO the notes out (from 64 on).
 * GLO > IN: S_NTIN where notes and CCs come from (BOTH USB TRS OFF, (v * 4) >> 7: an old store's 0 is BOTH), S_THRU
 * the TRS jack's MIDI out over USB (from 64 on) */
enum { S_SCREEN, S_MCH, S_SYNI, S_SYNO, S_NOTO, S_NTIN, S_THRU, S_COUNT = 16 };
/* where a message came in (m2k_midi_from): the FM-1's USB or its TRS jack */
enum { M2K_USB, M2K_TRS };

/* the API */
void m2k_init(uint32_t seed);
void m2k_render(int16_t *lr, int frames);           /* interleaved stereo, frames a multiple of M2K_SUB */
void m2k_key(int k, int down);
void m2k_button(int b, int down);
void m2k_encoder(int e, int delta);
void m2k_midi(int status, int d1, int d2);          /* as from USB (the browser's, the host's) */
void m2k_midi_from(int src, int status, int d1, int d2);   /* src M2K_USB or M2K_TRS: GLO > MIDI and IN pick by it */
const uint8_t *m2k_frame(uint32_t ms);              /* draws the screen: 240 x 240, 1 bit, MSB first, 1 = ink */
const uint8_t *m2k_leds(void);                      /* 27 key LEDs then 14 button LEDs, 0..255 */
/* the store: everything the instrument keeps (settings, working patterns, projects) as one blob */
uint8_t *m2k_store_buf(int *max);                   /* the buffer the blob is packed into, or written to before unpack */
int m2k_store_pack(void);                           /* packs it; returns its length */
int m2k_store_unpack(int len);                      /* reads a blob written into the buffer; 0: refused whole, state kept */
int m2k_store_pack_as(int patterns, int projects);  /* packs it for an instrument keeping fewer (the FM-1: 16, 1) */
int m2k_proj_export(int slot);                      /* a project as a file, into the buffer; returns its length */
int m2k_proj_import(int slot, int len);             /* a project file from the buffer into a slot */
/* the FM-1 keeps the store in its flash, not in a buffer: the platform's stream, written and read front to back from
 * offset 0 (m2k_store_pack and m2k_store_unpack use it in place of the buffer when M2K_FW is set) */
int m2k_fw_store_put(uint32_t off, const uint8_t *p, uint32_t n);   /* 0: written */
int m2k_fw_store_get(uint32_t off, uint8_t *p, uint32_t n);         /* 0: read */
void m2k_project(int slot, int save);               /* load (0) or save (1) a project */
const char *m2k_project_name(int slot);
int m2k_project_current(void);
int m2k_dirty(void);                                /* 1 when SAVE asked for the store to be written */
/* MIDI out (GLO > MIDI: the clock, start and stop; each track's notes): the next message due once the sound has
 * been played up to sample PLAYED (m2k_samples counts the samples rendered since init): its length, 0 for none */
int m2k_midi_out(uint8_t msg[3], uint32_t played);
uint32_t m2k_samples(void);
int m2k_playing(void);
/* a second core (the FM-1's): the platform sets these and the render runs some tracks there, block by block.
 * m2k_par_begin(job) starts job on it (0: it could not, the core does it in place), m2k_par_end waits for it */
extern int (*m2k_par_begin)(void (*job)(void));
extern void (*m2k_par_end)(void);                              /* 1 while the sequencer plays (the FM-1 writes its flash when it stops) */
void m2k_setting(int i, int v);                     /* e.g. the web page's theme sets S_SCREEN */
void m2k_pattern(int n);                            /* select a pattern, as PRESETS does */
void m2k_track(int t);                              /* select a track, as SELECT does (the web's track cells) */
void m2k_mute(int t);                               /* mute or unmute a track of the pattern */
void m2k_tempo(int d);                              /* the tempo up or down by d BPM */

#endif
