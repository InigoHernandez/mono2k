/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's factory sound library (SYN > SND): 96 sounds, twelve a category, S001..S096 in the categories' order (KICK
 * SNARE HATS PERC BASS LEAD PAD FX), S097..S128 INIT. Within a category each is a character of its own, not a
 * variation of one (the audition, m2k_host DIR --library, measures how far apart they are). Each is a machine on its
 * blank kit (kit_default on format 16's laws: kit_init without the machine's own INIT sound) and what it changes,
 * made at its note on T1. A sound is its own track: no reverb send, nothing from the bus's TAPE, HISS or GLCH; its
 * space, where it has one, is its own delay. The cheaper way to a character first: the dear stages (SWAV-ENS's side,
 * MOD, the EQ, unison) only where they are the sound. */

#define LIB_N 96
#define LIB_HASH 0xe0b24f46u              /* the library's bytes (FNV-1a over S001..S096, each its category and note), held by
                                         * the self-test: a library changed is a new LIB_REV, its sounds of before into LIB_OLD */
typedef char lib_fits[LIB_N <= M2K_SOUNDS && LIB_REV <= FACTORY_REV ? 1 : -1];

/* constants for the tables (the factory's enc and its helpers are functions: not for a static table) */
#define ENC(i, n) ((uint8_t)(((i) * 128 + 64) / (n)))
#define R24(i) ENC(i, 24)              /* FM+'s 24 ratios: 15 x1, 17 x1.5, 19 x2, 20 x2.5, 21 x3, 22 x3.5, 23 x4 */
#define SIDW(i) ENC(i, 5)              /* SID-6581's WAVE: 0 TRI 1 SAW 2 PULS 3 MIX 4 NOIS */
#define SIDMOD(i) ENC(i, 4)            /* its MOD: 0 OFF 1 RING 2 SYNC 3 R+S */
#define MODTY(i) ENC(i, 3)             /* MOD's type: 0 CHOR 1 FLNG 2 PHAS */
/* an LFO: its wave (TRI SIN SQR SAW EXP RMP RND), trig mode (FREE TRIG HOLD ONE HALF) and multiplier as values */
enum { LW_TRI = 9, LW_SIN = 27, LW_SQR = 45, LW_SAW = 64, LW_EXP = 82, LW_RMP = 100, LW_RND = 118 };
enum { LT_FREE = 12, LT_TRIG = 38, LT_HOLD = 64, LT_ONE = 89, LT_HALF = 115 };
enum { LX1 = 8, LX2 = 24, LX4 = 40, LX8 = 56 };
#define LFO(l, dest, w, trig, spd, dpth) \
    l + L_DEST, (dest) + 1, l + L_WAVE, w, l + L_TRIG, trig, l + L_SPD, spd, l + L_DPTH, dpth
/* SWAV-ENS's intervals, equal tempered (T_ENS_ET's middle of each group, as the factory's PCH) */
enum { I_M12 = 0, I_M5 = 31, I_P2 = 69, I_P3 = 73, I_P4 = 85, I_P5 = 93, I_P7 = 100, I_P9 = 112, I_P10 = 116,
       I_P11 = 120, I_P12 = 125 };

typedef struct {
    char name[8];
    uint8_t cat, m, root;              /* its category, machine, the note it was made at */
    const uint8_t *kv;                 /* (param, value) pairs over the blank kit, KV_END last */
#ifndef M2K_FW
    const char *what;                  /* its character in a few words (the audition's index; not on the FM-1) */
#endif
} lib_t;
#ifdef M2K_FW
#define S(name, cat, m, root, what, ...) {name, cat, m, root, KV(__VA_ARGS__)}
#else
#define S(name, cat, m, root, what, ...) {name, cat, m, root, KV(__VA_ARGS__), what}
#endif

static const lib_t LIB[LIB_N] = {
    /* KICK: on the beat and on their notes (A1 33, C2 36 ...); the sweeps fast, so a kick and not a laser */
    S("BOOM808", CAT_KICK, M_DKIK, 33, "long 808 boom, round sub tail",
      SA, 16, SB, 20, SC, 44, SD, 24, SG, 0, SH, 14, P_HOLD, 6, P_DEC, 74, P_REL, 50, P_DIST, 70, P_VOL, 90,
      P_LEVL, 100),
    S("PUNCH", CAT_KICK, M_DKIK, 36, "tight punchy body, mid knock",
      SA, 16, SB, 40, SC, 24, SD, 36, SG, 50, SH, 36, P_HOLD, 2, P_DEC, 40, P_REL, 16, P_DIST, 76, P_VOL, 96,
      P_LEVL, 106),
    S("CLIPPED", CAT_KICK, M_DKIK, 36, "hard clipped kick, square body",
      SA, 16, SB, 44, SC, 30, SD, 28, SG, 30, SH, 40, P_HOLD, 4, P_DEC, 50, P_REL, 24, P_DIST, 112, P_WDTH, 58,
      P_VOL, 80, P_LEVL, 112),
    S("FMKICK", CAT_KICK, M_FMD, 36, "tonal FM kick, a dub boing",
      SA, 64, SC, 36, SD, 112, SE, 0, SF, 64, SG, 0, LFO(L1, P_PTCH, LW_EXP, LT_ONE, 104, 88), P_HOLD, 2, P_DEC, 50,
      P_REL, 20, P_VOL, 110, P_LEVL, 112),
    S("SINEKIK", CAT_KICK, M_GSIN, 33, "round sine sweep, 2nd harmonic",
      SD, 40, SE, 76, SF, 50, P_HOLD, 4, P_DEC, 54, P_REL, 24, SA, 20, SG, 30, P_VOL, 87, P_LEVL, 117),
    S("CLICKY", CAT_KICK, M_DKIK, 40, "thin click-forward techno kick",
      SA, 48, SB, 56, SC, 14, SD, 40, SG, 110, SH, 127, P_HOLD, 1, P_DEC, 34, P_REL, 12, P_BASE, 16, P_DIST, 90,
      P_VOL, 82, P_LEVL, 127),
    S("CRUSHED", CAT_KICK, M_DKIK, 33, "bit-crushed kick, aliased grit",
      SA, 16, SB, 36, SC, 32, SD, 30, SG, 20, SH, 70, P_HOLD, 4, P_DEC, 50, P_REL, 20, P_SRR, 84, P_BITS, 100,
      P_DIST, 78, P_VOL, 96, P_LEVL, 103),
    S("TOMKIK", CAT_KICK, M_DKIK, 45, "tuned tom-like kick, woody",
      SA, 48, SB, 18, SC, 56, SD, 50, SE, 12, SF, 76, SH, 20, P_HOLD, 2, P_DEC, 58, P_REL, 24, P_VOL, 95, P_LEVL, 113),
    S("THUD", CAT_KICK, M_DKIK, 28, "deep muffled thud, no click",
      SA, 16, SB, 10, SC, 56, SD, 20, SG, 0, SH, 0, P_DEC, 62, P_REL, 24, P_WDTH, 20, P_LPQ, 16, P_DIST, 70,
      P_VOL, 106, P_LEVL, 115),
    S("GABBER", CAT_KICK, M_DKIK, 36, "distorted gabber, ringing tail",
      SA, 16, SB, 64, SC, 40, SD, 30, SG, 60, SH, 70, P_HOLD, 12, P_DEC, 66, P_REL, 30, P_DIST, 127, P_BASE, 4,
      P_WDTH, 60, P_VOL, 80, P_LEVL, 88),
    S("DUSTY", CAT_KICK, M_DKIK, 36, "short dusty boom-bap, crunch",
      SA, 16, SB, 22, SC, 36, SD, 24, SH, 50, P_HOLD, 2, P_DEC, 40, P_REL, 18, P_WDTH, 44, P_SRR, 64, P_BITS, 80,
      P_DIST, 80, P_VOL, 100, P_LEVL, 108),
    S("RESOKIK", CAT_KICK, M_DKIK, 33, "resonant filter knock, short",
      SA, 16, SB, 28, SC, 28, SD, 30, SH, 20, P_HOLD, 2, P_DEC, 54, P_REL, 20, P_WDTH, 30, P_LPQ, 110, P_FDEC, 20,
      P_WOF, 100, P_VOL, 76, P_LEVL, 105),

    /* SNARE: on 2 and 4 */
    S("CRACK", CAT_SNARE, M_DSNR, 55, "bright noisy crack",
      SA, 48, SB, 20, SC, 24, SD, 120, SE, 92, SF, 80, P_DEC, 40, P_REL, 20, P_BASE, 12, P_VOL, 95, P_LEVL, 127),
    S("TIGHT", CAT_SNARE, M_DSNR, 62, "tight rim-like snap, dry",
      SA, 16, SB, 14, SC, 10, SD, 110, SE, 70, SF, 96, P_DEC, 28, P_REL, 10, P_BASE, 22, P_DIST, 96, P_VOL, 83,
      P_LEVL, 127),
    S("CLAP", CAT_SNARE, M_DSNR, 60, "three-hit hand clap, papery",
      SA, 16, SB, 0, SC, 0, SD, 104, SE, 127, SF, 84, SG, 24, SH, 6, P_DEC, 42, P_REL, 24, P_BASE, 26, P_WDTH, 40,
      P_HPQ, 16, P_VOL, 107, P_LEVL, 126),
    S("FMSNARE", CAT_SNARE, M_FMS, 57, "metallic FM snare, ringing",
      SA, R24(21), SB, 64, SC, 96, SD, 127, SE, R24(17), SF, 90, SG, 100, P_DEC, 38, P_REL, 20, P_BASE, 14, P_VOL, 110,
      P_LEVL, 127),
    S("ROOMSNR", CAT_SNARE, M_DSNR, 57, "snare in a dark echo room",
      SA, 48, SB, 16, SC, 20, SD, 100, SE, 84, SF, 70, P_DEC, 40, P_REL, 20, P_DTIM, 6, P_DSND, 100, P_DFB, 52,
      P_DBAS, 30, P_DWID, 50, P_VOL, 97, P_LEVL, 126),
    S("LOFISNR", CAT_SNARE, M_DSNR, 56, "crunchy sampler snare, 7-bit",
      SA, 16, SB, 10, SC, 16, SD, 80, SE, 70, SF, 60, P_DEC, 38, P_REL, 20, P_SRR, 76, P_BITS, 88, P_DIST, 80,
      P_WDTH, 52, P_VOL, 108, P_LEVL, 127),
    S("WHITE", CAT_SNARE, M_GNOI, 60, "gated white-noise snare, 80s",
      P_HOLD, 4, P_DEC, 20, P_REL, 12, P_BASE, 34, P_WDTH, 40, P_HPQ, 20, P_VOL, 95, P_LEVL, 93),
    S("TRASH", CAT_SNARE, M_GPRC, 60, "trashy ringing metal snare",
      SA, 64, SC, 100, SD, 90, SG, 80, P_DEC, 50, P_REL, 20, P_BASE, 8, P_LPQ, 60, P_DIST, 110, P_WDTH, 40, P_VOL, 83,
      P_LEVL, 107),
    S("BRUSH", CAT_SNARE, M_DSNR, 60, "soft brushed swish",
      SA, 16, SD, 72, SE, 120, SF, 36, P_ATK, 10, P_DEC, 46, P_REL, 30, P_BASE, 20, P_WDTH, 40, P_VOL, 106,
      P_LEVL, 113),
    S("FAT808", CAT_SNARE, M_DSNR, 50, "deep fat 808 snare",
      SA, 16, SB, 12, SC, 44, SD, 90, SE, 44, SF, 70, P_HOLD, 2, P_DEC, 50, P_REL, 30, P_DIST, 72, P_WDTH, 46,
      P_VOL, 100, P_LEVL, 100),
    S("DIRTY", CAT_SNARE, M_DSNR, 58, "overdriven industrial snare",
      SA, 112, SB, 30, SC, 20, SD, 127, SE, 80, SF, 72, P_DEC, 42, P_REL, 20, P_DIST, 122, P_WDTH, 56, P_VOL, 73,
      P_LEVL, 127),
    S("FLAM", CAT_SNARE, M_DSNR, 57, "flammed snare, two quick hits",
      SA, 48, SB, 16, SC, 20, SD, 100, SE, 80, SF, 76, SG, 16, SH, 26, P_DEC, 46, P_REL, 22, P_BASE, 10, P_DIST, 86,
      P_VOL, 86, P_LEVL, 127),

    /* HATS: eighths (the open ones off the beat, the crash twice) */
    S("CLOSED", CAT_HATS, M_DHAT, 60, "crisp closed hat",
      SA, 120, SB, 76, SC, 40, SD, 92, SE, 110, SF, 91, SG, 6, P_DEC, 60, P_REL, 20, P_BASE, 50, P_WDTH, 40, P_HPQ, 20,
      P_EQF, 112, P_EQG, 80, P_VOL, 123, P_LEVL, 89),
    S("OPENHAT", CAT_HATS, M_DHAT, 60, "open hat, airy tail",
      SA, 120, SB, 76, SC, 40, SD, 92, SE, 110, SF, 91, SG, 6, SH, 24, P_DEC, 70, P_REL, 30, P_BASE, 50, P_WDTH, 40,
      P_EQF, 112, P_EQG, 80, P_VOL, 123, P_LEVL, 79),
    S("FMHAT", CAT_HATS, M_FMP, 72, "glassy FM bell tick",
      SA, R24(22), SB, 110, SC, R24(20), SD, 104, SE, R24(16), SF, 100, SG, 127, P_DEC, 30, P_REL, 14, P_BASE, 30,
      P_VOL, 110, P_LEVL, 93),
    S("NOISHAT", CAT_HATS, M_GNOI, 60, "tiny white-noise tick",
      P_DEC, 14, P_REL, 8, P_BASE, 62, P_WDTH, 24, P_HPQ, 30, P_EQF, 116, P_EQG, 80, P_VOL, 127, P_LEVL, 127),
    S("SIZZLE", CAT_HATS, M_DCYM, 60, "chattering sizzle, half open",
      SA, 100, SB, 92, SC, 90, SD, 120, SE, 120, SF, 117, SG, 4, SH, 40, P_DEC, 70, P_REL, 30, P_BASE, 36, P_WDTH, 40,
      P_EQF, 108, P_EQG, 84, LFO(L1, P_LEVL, LW_SQR, LT_TRIG, 120, 110), P_VOL, 78, P_LEVL, 73),
    S("CRUSHHT", CAT_HATS, M_DHAT, 60, "bit-crushed hat, aliased fizz",
      SA, 110, SB, 70, SC, 50, SD, 84, SE, 100, SF, 117, SG, 10, P_DEC, 60, P_REL, 20, P_BASE, 40, P_SRR, 76,
      P_BITS, 80, P_DIST, 64, P_EQF, 112, P_EQG, 88, P_VOL, 127, P_LEVL, 127),
    S("RIDE", CAT_HATS, M_DCYM, 60, "pingy ride, bell over wash",
      SA, 88, SB, 75, SC, 40, SD, 110, SE, 20, SF, 13, SG, 0, SH, 56, P_DEC, 80, P_REL, 40, P_BASE, 40, P_WDTH, 40,
      P_EQF, 92, P_EQG, 76, P_VOL, 92, P_LEVL, 96),
    S("CRASH", CAT_HATS, M_DCYM, 60, "wide crash, long wash",
      SA, 94, SB, 83, SC, 90, SD, 119, SE, 110, SF, 117, SG, 4, SH, 90, P_DEC, 96, P_REL, 60, P_BASE, 38, P_WDTH, 30,
      P_VOL, 105, P_LEVL, 127),
    S("PEDAL", CAT_HATS, M_DHAT, 55, "dark soft pedal chick",
      SA, 100, SB, 60, SC, 30, SD, 70, SE, 60, SF, 39, SG, 2, P_ATK, 3, P_DEC, 50, P_REL, 14, P_BASE, 28, P_WDTH, 30,
      P_LPQ, 20, P_EQF, 96, P_EQG, 92, P_DIST, 80, P_VOL, 116, P_LEVL, 123),
    S("808HAT", CAT_HATS, M_GPRC, 60, "808 six-square metal hat",
      SA, 64, SC, 127, SD, 30, P_DEC, 18, P_REL, 14, P_BASE, 46, P_WDTH, 30, P_HPQ, 20, P_VOL, 127, P_LEVL, 127),
    S("CHIPHAT", CAT_HATS, M_SID, 72, "grainy chip-noise hat",
      SD, SIDW(4), P_DEC, 18, P_REL, 10, P_BASE, 16, P_WDTH, 36, P_VOL, 87, P_LEVL, 121),
    S("LOOSE", CAT_HATS, M_DHAT, 62, "loose noisy half-open hat",
      SA, 116, SB, 80, SC, 60, SD, 100, SE, 127, SF, 117, SG, 16, P_DEC, 60, P_REL, 24, P_BASE, 40, P_WDTH, 30,
      P_EQF, 110, P_EQG, 80, P_VOL, 108, P_LEVL, 109),

    /* PERC: a syncopated bar on their notes */
    S("RIM", CAT_PERC, M_GSIN, 76, "noisy woody rimshot",
      SA, 40, SC, 50, SD, 8, SE, 24, SF, 120, SG, 30, P_DEC, 18, P_REL, 8, P_BASE, 20, P_HPQ, 20, P_DIST, 86,
      P_VOL, 83, P_LEVL, 127),
    S("TOM", CAT_PERC, M_DKIK, 48, "round electronic tom",
      SA, 16, SB, 18, SC, 48, SD, 30, SH, 24, P_HOLD, 2, P_DEC, 52, P_REL, 24, P_VOL, 98, P_LEVL, 75),
    S("COWBELL", CAT_PERC, M_FMP, 77, "FM cowbell, hollow clank",
      SA, R24(17), SB, 46, SC, R24(20), SD, 92, SE, R24(15), SF, 0, SG, 80, P_DEC, 32, P_REL, 16, P_BASE, 18,
      P_WDTH, 22, P_LPQ, 30, P_VOL, 95, P_LEVL, 107),
    S("CLAVE", CAT_PERC, M_GSIN, 91, "ringing pure clave",
      SG, 10, P_DEC, 22, P_REL, 8, P_VOL, 107, P_LEVL, 101),
    S("BLIP", CAT_PERC, M_SID, 72, "thin chip pulse blip",
      SA, 16, SD, SIDW(2), P_DEC, 28, P_REL, 12, P_WDTH, 36, P_VOL, 87, P_LEVL, 126),
    S("ZAP", CAT_PERC, M_FMD, 72, "quick falling FM zap",
      SA, 96, SB, 80, SC, 50, SD, 90, SE, 90, SF, 40, SG, 0, LFO(L1, P_PTCH, LW_EXP, LT_ONE, 110, 100), P_DEC, 26,
      P_REL, 14, P_VOL, 111, P_LEVL, 126),
    S("SHAKER", CAT_PERC, M_GNOI, 60, "soft-attack shaker",
      P_ATK, 12, P_DEC, 22, P_REL, 10, P_BASE, 48, P_WDTH, 16, P_HPQ, 20, P_LPQ, 20, P_VOL, 104, P_LEVL, 121),
    S("CONGA", CAT_PERC, M_FMD, 57, "conga-like pitched tap",
      SA, 64, SC, 24, SD, 100, SE, 64, SF, 64, SG, 0, LFO(L1, P_PTCH, LW_EXP, LT_ONE, 108, 80), P_DEC, 34, P_REL, 16,
      P_VOL, 110, P_LEVL, 105),
    S("WOODBLK", CAT_PERC, M_FMS, 69, "hollow resonant woodblock",
      SA, R24(21), SB, 64, SC, 100, SD, 0, SE, R24(19), SF, 10, SG, 90, P_DEC, 24, P_REL, 10, P_BASE, 20, P_WDTH, 20,
      P_LPQ, 40, P_VOL, 114, P_LEVL, 126),
    S("TING", CAT_PERC, M_FMP, 91, "bright ringing triangle ting",
      SA, R24(23), SB, 30, SC, R24(22), SD, 88, SE, R24(19), SF, 20, SG, 110, P_DEC, 70, P_REL, 50, P_BASE, 32,
      P_VOL, 100, P_LEVL, 109),
    S("METAL", CAT_PERC, M_GPRC, 60, "resonant metal clank",
      SA, 64, SC, 127, SD, 100, SE, 10, SF, 40, P_DEC, 40, P_REL, 20, P_BASE, 20, P_WDTH, 24, P_LPQ, 70, P_VOL, 78,
      P_LEVL, 93),
    S("KALIMBA", CAT_PERC, M_WAV, 72, "soft kalimba tine",
      P_WBNK, 127, SA, OS(14), SB, 63, SC, 64, P_DEC, 54, P_REL, 30, P_WDTH, 32, P_VOL, 110, P_LEVL, 92),

    /* BASS: a riff on A1 / A2 */
    S("ACID", CAT_BASS, M_SAW, 33, "squelchy resonant acid line",
      SA, 0, P_HOLD, 127, P_DEC, 127, P_REL, 18, P_GATE, 22, P_WDTH, 22, P_LPQ, 104, P_FDEC, 26, P_WOF, 104,
      P_DIST, 84, P_VOL, 116, P_LEVL, 127),
    S("CHIPSQR", CAT_BASS, M_SID, 33, "raw gated chip pulse",
      SA, 16, SD, SIDW(2), P_HOLD, 127, P_DEC, 127, P_REL, 14, P_GATE, 20, P_VOL, 63, P_LEVL, 116),
    S("DXBASS", CAT_BASS, M_FMS, 33, "punchy FM slap bass",
      SA, R24(15), SB, 64, SC, 98, SD, 24, SE, R24(19), SF, 24, SG, 80, P_HOLD, 4, P_DEC, 58, P_REL, 26, P_VOL, 100,
      P_LEVL, 127),
    S("SUB", CAT_BASS, M_GSIN, 33, "clean sine sub, felt",
      SG, 8, P_HOLD, 127, P_DEC, 127, P_REL, 20, P_GATE, 24, P_ATK, 2, P_VOL, 84, P_LEVL, 94),
    S("PWMBASS", CAT_BASS, M_PUL, 33, "warm wobbling PWM bass",
      SA, 70, SB, 40, SC, 60, SE, 64, SF, 40, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 20, P_WDTH, 24, P_LPQ, 20,
      P_FDEC, 30, P_WOF, 92, P_VOL, 102, P_LEVL, 127),
    S("GROWL", CAT_BASS, M_FMD, 33, "growling FM bass, phased",
      SA, 64, SC, 60, SE, 90, SF, 40, SG, 70, LFO(L1, SC, LW_SIN, LT_TRIG, 96, 100), P_HOLD, 127, P_DEC, 127,
      P_REL, 20, P_GATE, 26, P_DIST, 100, P_WDTH, 40, P_LPQ, 40, P_MODT, MODTY(2), P_MODR, 70, P_MODD, 90, P_MODM, 70,
      P_VOL, 63, P_LEVL, 99),
    S("PLUCKBS", CAT_BASS, M_WAV, 45, "plucky wavetable bass",
      P_WBNK, 127, SA, OS(41), SB, 64, SF, 64, SG, 0, P_DEC, 46, P_REL, 20, P_WDTH, 26, P_LPQ, 30, P_FDEC, 22,
      P_WOF, 100, P_DIST, 80, P_VOL, 127, P_LEVL, 127),
    S("REESE", CAT_BASS, M_SAW, 33, "detuned reese, beating saws",
      SA, 127, SB, 46, SC, 0, SD, 20, SF, 40, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 56, P_WDTH, 28, P_LPQ, 24,
      P_ATK, 1, P_VOL, 114, P_LEVL, 127),
    S("FOLDBS", CAT_BASS, M_GSIN, 33, "wavefolded pluck, west coast",
      SA, 60, SG, 20, LFO(L1, SA, LW_EXP, LT_ONE, 96, 112), P_HOLD, 127, P_DEC, 127, P_REL, 22, P_GATE, 24, P_VOL, 75,
      P_LEVL, 97),
    S("DUBTRI", CAT_BASS, M_SID, 40, "deep round dub triangle",
      SA, 0, SC, 127, SD, SIDW(0), P_ATK, 4, P_HOLD, 127, P_DEC, 127, P_REL, 50, P_GATE, 30, P_WDTH, 24, P_VOL, 110,
      P_LEVL, 83),
    S("SYNCBS", CAT_BASS, M_SID, 33, "tearing sync bass",
      SD, SIDW(1), SE, SIDMOD(2), SF, 0, SG, 30, LFO(L1, SG, LW_EXP, LT_ONE, 96, 100), P_HOLD, 127, P_DEC, 127,
      P_REL, 20, P_GATE, 24, P_WDTH, 40, P_DIST, 80, P_VOL, 90, P_LEVL, 92),
    S("MOOG", CAT_BASS, M_SAW, 33, "fat saw and sub, filter punch",
      SA, 0, SE, 70, SF, 90, P_DEC, 70, P_REL, 28, P_WDTH, 30, P_LPQ, 24, P_FDEC, 36, P_WOF, 92, P_DIST, 90, P_HOLD, 4,
      P_VOL, 127, P_LEVL, 127),

    /* LEAD: a phrase on A3 / A4 */
    S("SIDLEAD", CAT_LEAD, M_SID, 69, "C64 pulse lead, vibrato",
      SA, 20, SB, 30, SD, SIDW(2), LFO(L1, SH, LW_SIN, LT_FREE, 104, 82), P_ATK, 2, P_HOLD, 127, P_DEC, 127, P_REL, 30,
      P_GATE, 24, P_PORT, 16, P_WDTH, 44, P_VOL, 84, P_LEVL, 66),
    S("FMBELL", CAT_LEAD, M_FMS, 69, "clear FM bell, long ring",
      SA, R24(22), SB, 64, SC, 90, SD, 0, SE, R24(19), SF, 50, SG, 127, P_DEC, 86, P_REL, 70, P_VOL, 110, P_LEVL, 74),
    S("SQUARE", CAT_LEAD, M_PUL, 69, "dark hollow square, gliding",
      SE, 64, SC, 30, P_HOLD, 127, P_DEC, 127, P_REL, 28, P_GATE, 28, P_PORT, 30, P_BASE, 8, P_WDTH, 24, P_LPQ, 20,
      P_VOL, 127, P_LEVL, 107),
    S("VOXOOH", CAT_LEAD, M_VOX, 69, "sung ooh, vibrato",
      SA, 40, SB, 34, SC, 127, SD, 16, SE, 0, LFO(L1, SH, LW_SIN, LT_FREE, 104, 80), P_ATK, 10, P_HOLD, 127,
      P_DEC, 127, P_REL, 10, P_GATE, 44, P_VOL, 127, P_LEVL, 123),
    S("SUPERLD", CAT_LEAD, M_SAW, 69, "wide supersaw, ping-pong echo",
      SA, 90, SB, 40, SC, 40, SD, 10, P_HOLD, 127, P_DEC, 127, P_REL, 40, P_GATE, 28, P_BASE, 8, P_WDTH, 36, P_LPQ, 16,
      P_DTIM, 48, P_DSND, 90, P_DFB, 36, P_DBAS, 40, P_DWID, 36, P_VOL, 116, P_LEVL, 126),
    S("ARPLUCK", CAT_LEAD, M_WAV, 69, "bright snappy arp pluck",
      P_WBNK, 127, SA, OS(42), SB, 64, P_DEC, 40, P_REL, 24, P_WDTH, 34, P_LPQ, 20, P_FDEC, 20, P_WOF, 96, P_VOL, 100,
      P_LEVL, 127),
    S("SYNCLD", CAT_LEAD, M_SID, 57, "screaming swept sync",
      SD, SIDW(1), SE, SIDMOD(2), SG, 40, LFO(L1, SG, LW_EXP, LT_ONE, 84, 104), P_HOLD, 127, P_DEC, 127, P_REL, 30,
      P_GATE, 26, P_DIST, 90, P_VOL, 78, P_LEVL, 71),
    S("WHISTLE", CAT_LEAD, M_SID, 81, "soft gliding whistle, vibrato",
      SD, SIDW(0), LFO(L1, SH, LW_SIN, LT_FREE, 102, 82), P_ATK, 10, P_HOLD, 127, P_DEC, 127, P_REL, 36, P_GATE, 30,
      P_PORT, 40, P_BASE, 8, P_WDTH, 14, P_VOL, 100, P_LEVL, 76),
    S("ORGAN", CAT_LEAD, M_WAV, 57, "hollow lofi organ, turning",
      P_WBNK, 127, SA, OS(39), SF, 66, SG, 90, LFO(L1, P_PAN, LW_SIN, LT_FREE, 96, 96), P_HOLD, 127, P_DEC, 127,
      P_REL, 20, P_GATE, 30, P_WDTH, 44, P_VOL, 92, P_LEVL, 91),
    S("GLASSKY", CAT_LEAD, M_FMP, 69, "glassy FM mallet keys",
      SA, R24(19), SB, 40, SC, R24(23), SD, 96, SE, R24(15), SF, 20, SG, 127, P_DEC, 56, P_REL, 40, P_VOL, 110,
      P_LEVL, 90),
    S("EPIANO", CAT_LEAD, M_FMD, 60, "warm tine e-piano",
      SA, 64, SC, 24, SD, 96, SE, 127, SF, 100, SG, 0, LFO(L1, P_PAN, LW_TRI, LT_FREE, 92, 92), P_DEC, 74, P_REL, 60,
      P_VOL, 110, P_LEVL, 72),
    S("BRASS", CAT_LEAD, M_SAW, 57, "brassy saw, swelling filter",
      SA, 70, SB, 20, P_ATK, 30, P_HOLD, 127, P_DEC, 127, P_REL, 14, P_GATE, 20, P_WDTH, 18, P_LPQ, 20, P_FATK, 64,
      P_FDEC, 40, P_WOF, 92, P_DIST, 84, P_VOL, 127, P_LEVL, 116),

    /* PAD: two chords held (a mono machine: two notes) on A3 */
    S("WARMENS", CAT_PAD, M_ENS, 57, "warm chorused minor-7 chords",
      SA, I_P3, SB, I_P7, SC, I_P10, SD, 0, SF, 50, SG, 70, LFO(L1, P_WDTH, LW_SIN, LT_FREE, 48, 92), P_ATK, 40,
      P_HOLD, 127, P_DEC, 127, P_REL, 80, P_GATE, 127, P_BASE, 8, P_WDTH, 30, P_LPQ, 20, P_VOL, 110, P_LEVL, 94),
    S("FIFTHS", CAT_PAD, M_ENS, 57, "open fifths, glassy pulse",
      SA, I_P7, SB, I_P12, SC, I_M12, SD, 110, SE, 40, SF, 0, LFO(L1, SE, LW_TRI, LT_FREE, 60, 100), P_ATK, 60,
      P_HOLD, 127, P_DEC, 127, P_REL, 80, P_GATE, 127, P_BASE, 16, P_WDTH, 34, P_VOL, 110, P_LEVL, 114),
    S("ADD9PAD", CAT_PAD, M_ENS, 57, "dark add9 cluster, echoing",
      SA, I_P2, SB, I_P4, SC, I_P7, SD, 64, SE, 50, SF, 0, SG, 50, P_ATK, 50, P_HOLD, 127, P_DEC, 127, P_REL, 80,
      P_GATE, 127, P_BASE, 4, P_WDTH, 24, P_LPQ, 40, P_DTIM, 64, P_DSND, 88, P_DFB, 40, P_DWID, 40, P_VOL, 110,
      P_LEVL, 88),
    S("FMPAD", CAT_PAD, M_FMS, 57, "slow-breathing FM pad",
      SA, R24(19), SB, 66, SC, 30, SD, 30, SE, R24(21), SF, 40, SG, 110, LFO(L1, SC, LW_TRI, LT_FREE, 48, 96),
      P_ATK, 64, P_HOLD, 127, P_DEC, 127, P_REL, 80, P_GATE, 127, L1 + L_DPTH, 104, P_VOL, 109, P_LEVL, 71),
    S("CHOIR", CAT_PAD, M_VOX, 57, "airy aah choir, chorused",
      SA, 120, SB, 56, SC, 127, SD, 30, SE, 0, LFO(L1, SH, LW_SIN, LT_FREE, 96, 76), P_ATK, 56, P_HOLD, 127,
      P_DEC, 127, P_REL, 88, P_GATE, 127, P_MODT, MODTY(0), P_MODR, 30, P_MODD, 80, P_MODM, 70, P_VOL, 111,
      P_LEVL, 127),
    S("WINDPAD", CAT_PAD, M_GNOI, 57, "resonant wind, slow sweep",
      SB, 64, LFO(L1, P_BASE, LW_SIN, LT_FREE, 48, 84), P_ATK, 72, P_HOLD, 127, P_DEC, 127, P_REL, 90, P_GATE, 127,
      P_BASE, 28, P_WDTH, 14, P_HPQ, 40, P_LPQ, 70, P_VOL, 66, P_LEVL, 66),
    S("WAVEPAD", CAT_PAD, M_WAV, 57, "morphing wavetable pad",
      P_WBNK, 127, SA, OS(37), LFO(L1, SA, LW_TRI, LT_FREE, 48, 84), P_ATK, 56, P_HOLD, 127, P_DEC, 127, P_REL, 80,
      P_GATE, 127, P_WDTH, 36, P_VOL, 110, P_LEVL, 87),
    S("STRINGS", CAT_PAD, M_SAW, 57, "string machine ensemble",
      SA, 100, SB, 50, SC, 70, SD, 20, P_ATK, 72, P_HOLD, 127, P_DEC, 127, P_REL, 92, P_GATE, 127, P_BASE, 12,
      P_WDTH, 32, P_LPQ, 10, P_VOL, 127, P_LEVL, 127),
    S("PWMPAD", CAT_PAD, M_PUL, 57, "warm analog PWM pad",
      SA, 80, SB, 50, SC, 20, SE, 64, SF, 12, P_ATK, 60, P_HOLD, 127, P_DEC, 127, P_REL, 80, P_GATE, 127, P_BASE, 8,
      P_WDTH, 34, P_LPQ, 16, P_VOL, 127, P_LEVL, 104),
    S("GLASPAD", CAT_PAD, M_FMP, 69, "shimmering bell pad, echoes",
      SA, R24(19), SB, 30, SC, R24(21), SD, 20, SE, R24(23), SF, 10, SG, 100, P_ATK, 40, P_HOLD, 127, P_DEC, 127,
      P_REL, 80, P_GATE, 127, P_DTIM, 64, P_DSND, 92, P_DFB, 44, P_DWID, 60, P_VOL, 110, P_LEVL, 65),
    S("WARBLE", CAT_PAD, M_WAV, 57, "tape-warbled fifths, soft",
      P_WBNK, 127, SA, OS(19), SF, 120, SG, 100, LFO(L1, P_PTCH, LW_SIN, LT_FREE, 66, 69), P_ATK, 50, P_HOLD, 127,
      P_DEC, 127, P_REL, 80, P_GATE, 127, P_WDTH, 30, L1 + L_DPTH, 70, P_VOL, 110, P_LEVL, 80),
    S("HALO", CAT_PAD, M_GSIN, 69, "sine halo in long echoes",
      SA, 24, SG, 20, P_ATK, 50, P_HOLD, 127, P_DEC, 127, P_REL, 90, P_GATE, 127, P_DTIM, 96, P_DSND, 100, P_DFB, 50,
      P_DBAS, 50, P_DWID, 50, P_VOL, 100, P_LEVL, 51),

    /* FX: two gestures, or a few hits, or one */
    S("RISER", CAT_FX, M_SAW, 48, "rising detuned saw riser",
      SA, 90, SB, 60, LFO(L1, P_PTCH, LW_RMP, LT_ONE, 60, 94), LFO(L2, P_WDTH, LW_RMP, LT_ONE, 60, 100), P_ATK, 90,
      P_HOLD, 126, P_DEC, 127, P_REL, 30, P_GATE, 127, P_WDTH, 20, P_LPQ, 30, P_VOL, 100, P_LEVL, 127),
    S("WHOOSH", CAT_FX, M_GNOI, 60, "falling noise whoosh",
      SB, 0, LFO(L1, P_BASE, LW_RMP, LT_ONE, 64, 24), P_ATK, 30, P_HOLD, 100, P_DEC, 127, P_REL, 50, P_GATE, 127,
      P_BASE, 70, P_WDTH, 16, P_LPQ, 60, P_VOL, 69, P_LEVL, 59),
    S("RINGHIT", CAT_FX, M_SID, 60, "ring-mod metal strike",
      SD, SIDW(0), SE, SIDMOD(1), SF, 0, SG, 70, P_DEC, 78, P_REL, 60, P_DTIM, 32, P_DSND, 88, P_DFB, 44, P_VOL, 124,
      P_LEVL, 127),
    S("GLITCH", CAT_FX, M_SID, 72, "random chip blips, glitchy",
      SA, 30, SD, SIDW(2), LFO(L1, P_PTCH, LW_RND, LT_FREE, 112, 92), L1 + L_MULT, LX2,
      LFO(L2, SA, LW_RND, LT_FREE, 116, 100), P_DEC, 50, P_REL, 20, P_VOL, 75, P_LEVL, 127),
    S("DRONE", CAT_FX, M_FMD, 33, "dark evolving FM drone",
      SA, 64, SC, 40, SE, 64, SF, 30, SG, 30, LFO(L1, SC, LW_SIN, LT_FREE, 36, 100),
      LFO(L2, P_WDTH, LW_TRI, LT_FREE, 44, 96), P_ATK, 70, P_HOLD, 126, P_DEC, 127, P_REL, 90, P_GATE, 127, P_DIST, 80,
      P_WDTH, 40, P_LPQ, 40, P_VOL, 98, P_LEVL, 64),
    S("LASER", CAT_FX, M_GSIN, 72, "pew laser with echoes",
      SA, 20, SD, 127, SE, 86, P_DEC, 56, P_REL, 30, P_DTIM, 32, P_DSND, 96, P_DFB, 44, P_DWID, 60, P_VOL, 100,
      P_LEVL, 96),
    S("STATIC", CAT_FX, M_GPRC, 60, "crackling radio static",
      SA, 80, SB, 20, SG, 110, P_DIST, 110, LFO(L1, P_LEVL, LW_RND, LT_FREE, 120, 112), P_ATK, 10, P_HOLD, 126,
      P_DEC, 127, P_REL, 30, P_GATE, 127, P_BASE, 30, P_SRR, 50, L1 + L_DPTH, 84, P_VOL, 118, P_LEVL, 127),
    S("BUBBLES", CAT_FX, M_FMD, 72, "bubbling FM water drops",
      SA, 64, SC, 30, SD, 110, SE, 90, LFO(L1, P_PTCH, LW_RND, LT_FREE, 104, 88), L1 + L_MULT, LX2,
      LFO(L2, P_LEVL, LW_SAW, LT_FREE, 112, 110), P_HOLD, 126, P_DEC, 127, P_REL, 30, P_GATE, 127, P_WDTH, 30,
      P_LPQ, 60, P_VOL, 75, P_LEVL, 39),
    S("IMPACT", CAT_FX, M_DKIK, 28, "cinematic boom with echo",
      SA, 16, SB, 64, SC, 88, SD, 30, SH, 80, P_HOLD, 10, P_DEC, 92, P_REL, 60, P_DIST, 100, P_DTIM, 64, P_DSND, 92,
      P_DFB, 40, P_DBAS, 20, P_DWID, 30, P_VOL, 87, P_LEVL, 82),
    S("ROBOT", CAT_FX, M_VOX, 48, "talking robot vowels",
      SA, 80, SB, 60, SC, 127, SD, 0, LFO(L1, SA, LW_RND, LT_FREE, 96, 110), LFO(L2, SB, LW_RND, LT_FREE, 92, 110),
      P_HOLD, 126, P_DEC, 127, P_REL, 30, P_GATE, 127, P_BITS, 80, P_VOL, 110, P_LEVL, 98),
    S("DUBSTAB", CAT_FX, M_ENS, 57, "dub chord stab thrown into echoes",
      SA, I_P3, SB, I_P7, SC, I_P10, SD, 100, SE, 50, SF, 0, P_DEC, 40, P_REL, 16, P_BASE, 8, P_WDTH, 36, P_LPQ, 30,
      P_DTIM, 48, P_DSND, 127, P_DFB, 58, P_DBAS, 48, P_DWID, 40, P_HOLD, 2, P_DIST, 88, P_VOL, 83, P_LEVL, 127),
    S("SCRAPE", CAT_FX, M_GPRC, 60, "shifting metal scrape",
      SA, 64, SB, 60, SC, 127, SD, 60, LFO(L1, SD, LW_TRI, LT_FREE, 72, 110), LFO(L2, P_BASE, LW_SIN, LT_FREE, 66, 96),
      P_ATK, 40, P_HOLD, 126, P_DEC, 127, P_REL, 50, P_GATE, 127, P_BASE, 24, P_WDTH, 20, P_LPQ, 80, P_VOL, 65,
      P_LEVL, 68),


};
#undef S
#undef LFO
#undef ENC
#undef R24
#undef SIDW
#undef SIDMOD
#undef MODTY

/* sound i as the factory makes it: its machine's blank (track 1's chain, on format 16's laws), then its changes */
static void lib_sound(int i, m2k_sound_t *d)
{
    const lib_t *s = &LIB[i];
    uint8_t *k = pat_tmp.kit[0];
    kit_default(&pat_tmp, 0, s->m);
    feel_kit_v15(k);
    for (const uint8_t *kv = s->kv; kv[0] != KV_END; kv += 2) k[kv[0]] = kv[1];
    m_set(d, 0, sizeof *d);
    m_cpy(d->name, s->name, 8);
    d->machine = s->m;
    m_cpy(d->kit, k, P_COUNT);
}

static void lib_put(int i, int slot)
{
    lib_sound(i, &snd[slot]);
    snd_cat[slot] = LIB[i].cat;
    snd_root[slot] = LIB[i].root;
}

/* the factory's: S001..S096 the library, the rest INIT */
static void lib_factory(void)
{
    m_set(snd, 0, sizeof snd);
    m_set(snd_cat, 0, sizeof snd_cat);
    m_set(snd_root, 0, sizeof snd_root);
    for (int i = 0; i < LIB_N; i++) lib_put(i, i);
}

static uint32_t snd_fnv(const m2k_sound_t *s) { return fnv((const uint8_t *)s, sizeof *s); }

/* the factory's sounds before the library, as an older store holds them once read (each slot's name, machine and kit
 * after the store's conversions), by their FNV-1a: every commit's from the library's first (64 slots of bank A's
 * tracks, then 128 with 96, then 24) to the last before this one, sorted (tools/lib_old.py --c makes it) */
static const uint32_t LIB_OLD[] = {
    0x0270d512u, 0x028427d7u, 0x03b0dccdu, 0x04c79daeu, 0x05171fa4u, 0x06516caeu, 0x065bd784u, 0x0780da98u,
    0x07d482f6u, 0x08d51316u, 0x09f9f45du, 0x0a14c628u, 0x0a44e3efu, 0x0abaf916u, 0x0abf7417u, 0x0ad17634u,
    0x0af04f35u, 0x0b10862fu, 0x0b12f300u, 0x0c3ccc5du, 0x0c8de6d3u, 0x0ce03acfu, 0x0d147b46u, 0x0de5b9f4u,
    0x0e027139u, 0x0ef80e5eu, 0x0f2fdfe4u, 0x0f4086d8u, 0x106fa2c5u, 0x16768826u, 0x17a2a3cbu, 0x17d79068u,
    0x17e499acu, 0x18a528dau, 0x19608360u, 0x199314b5u, 0x1a2e0c62u, 0x1ac2316cu, 0x1b1f18aeu, 0x1c828304u,
    0x1cb5ba9eu, 0x1dcf3398u, 0x1eab56e2u, 0x1fc18137u, 0x2043c96bu, 0x2088b5efu, 0x21533de4u, 0x22481dc9u,
    0x2363e8b5u, 0x23bc0784u, 0x23c389f1u, 0x2613793cu, 0x273908f9u, 0x27c2f05eu, 0x28c7b63bu, 0x2a16b8c3u,
    0x2b7e8a93u, 0x2e7149f7u, 0x2f8d1e03u, 0x3049977bu, 0x30a2ce55u, 0x31401c7du, 0x3163ebecu, 0x31bb1610u,
    0x326f7923u, 0x33adbfe8u, 0x34c90bdau, 0x34d2f5a4u, 0x359351d9u, 0x36176afau, 0x36d3dda5u, 0x36d701b5u,
    0x36df29c2u, 0x3789973au, 0x38767e9fu, 0x38b73726u, 0x38c527f2u, 0x3906c27bu, 0x39a5267du, 0x3a60eefbu,
    0x3ad0801du, 0x3af9a6e0u, 0x3c7cfe9bu, 0x3dd89b14u, 0x3e00af8eu, 0x3eb03585u, 0x3f718d0au, 0x40a0dc2cu,
    0x40cf2affu, 0x4448a323u, 0x444f7b10u, 0x44b7be21u, 0x47f48515u, 0x487bed18u, 0x48ca3fa4u, 0x49cd5b3au,
    0x4a3e4571u, 0x4a478ff3u, 0x4b67180fu, 0x4d03a3d5u, 0x4dd8d8eeu, 0x4e7c364fu, 0x4f7bf9c7u, 0x5041928du,
    0x5092b495u, 0x51a14625u, 0x53f1964bu, 0x543cdfc4u, 0x548bc4e5u, 0x54de7a3du, 0x56229ca0u, 0x575f196cu,
    0x587c7972u, 0x59256a74u, 0x5a5d84bcu, 0x5af7faf3u, 0x5b6e3b33u, 0x5ccfb869u, 0x5d342ebdu, 0x5e02ff6du,
    0x5e06e53eu, 0x5e77ee17u, 0x5f5c480bu, 0x5f9e9b0eu, 0x5fe3c8a3u, 0x610bec30u, 0x612e5d1du, 0x634882fdu,
    0x63c8f74fu, 0x641dc995u, 0x6569fa82u, 0x65a829abu, 0x65ed292au, 0x6676c449u, 0x67172971u, 0x673eaefau,
    0x679b8825u, 0x68884eb3u, 0x68e5af2fu, 0x6acc71c0u, 0x6dc62175u, 0x6e5cb682u, 0x6eb82e51u, 0x6f98921cu,
    0x7128c380u, 0x729d96b1u, 0x73bbd2afu, 0x7457b2deu, 0x7476a507u, 0x74b876dfu, 0x74c6749bu, 0x761cfcabu,
    0x76df7616u, 0x7742925au, 0x782a10ddu, 0x78c4b552u, 0x78d023d4u, 0x7ae05f11u, 0x7af14127u, 0x7ba493bbu,
    0x7da12e54u, 0x7f54e90du, 0x7f8f0adbu, 0x81110acfu, 0x840300cau, 0x843118cau, 0x8503a4fau, 0x864b8843u,
    0x8707b6e9u, 0x875ea8b7u, 0x87e01d17u, 0x8888efcfu, 0x892a2e2eu, 0x897ca282u, 0x8a99623cu, 0x8ca38abbu,
    0x8d61f313u, 0x900ea10cu, 0x9156fdf0u, 0x93c02c6cu, 0x94259446u, 0x947634fdu, 0x95082655u, 0x959dbb82u,
    0x96105049u, 0x968b265du, 0x97136227u, 0x974a82f8u, 0x9955c791u, 0x99a1390bu, 0x9a139c9au, 0x9a4d0081u,
    0x9c0e9d06u, 0x9d0af674u, 0x9d557e39u, 0x9dad32c3u, 0x9e4f1f38u, 0x9e561748u, 0xa1b3e15eu, 0xa28ba4eau,
    0xa6a52643u, 0xa9b9b0acu, 0xaac74c34u, 0xaad5cdacu, 0xaba3fcd7u, 0xad3852c9u, 0xadfc369au, 0xaf6683dfu,
    0xb1b58f72u, 0xb1d79e2bu, 0xb20e5628u, 0xb211b05au, 0xb2e086cdu, 0xb32544c2u, 0xb4e23c85u, 0xb5124fc3u,
    0xb552a835u, 0xb5fe474eu, 0xb70011e4u, 0xb7fa3336u, 0xb89b1929u, 0xb8abf640u, 0xb9dc8b5au, 0xba07711du,
    0xbb706e0bu, 0xbd1b0612u, 0xbfa1a38au, 0xc0269e3au, 0xc100155du, 0xc22a0d4bu, 0xc2802571u, 0xc3d7d8cau,
    0xc70bced3u, 0xc75e379au, 0xc8603529u, 0xca620be8u, 0xcafcebb5u, 0xcd2c4864u, 0xcd48009du, 0xcee386efu,
    0xd1fa340bu, 0xd21bfe1du, 0xd272a8dfu, 0xd2786e6cu, 0xd2c89b5cu, 0xd3f17fb4u, 0xd42dc34au, 0xd4a6f95du,
    0xd502fdf6u, 0xd5190986u, 0xd57dbca9u, 0xd63cf55fu, 0xd65147f2u, 0xd6f3669cu, 0xd7a90369u, 0xd8ce0ad0u,
    0xd921d5dau, 0xdaa2eb2cu, 0xdba7b252u, 0xdc36fd28u, 0xdc602539u, 0xde4e2380u, 0xe090e1a1u, 0xe09916a2u,
    0xe09d457cu, 0xe4a17994u, 0xe79811a3u, 0xe8e537d5u, 0xe9338a75u, 0xea37607cu, 0xea9f443au, 0xed873c27u,
    0xee02ab3au, 0xeecc50afu, 0xf0e63315u, 0xf0f1d128u, 0xf20d61bfu, 0xf4de5149u, 0xf689353bu, 0xf6b9771cu,
    0xf71a292du, 0xf81efc60u, 0xfafde42au, 0xfb588f45u, 0xfb8bd1adu, 0xfc110054u, 0xfc86523au, 0xfc9e4938u,
    0xfd7068d8u, 0xfdb05a98u, 0xfe45d116u, 0xff5a15ceu,
};

static int lib_old(uint32_t h)
{
    int lo = 0, hi = (int)(sizeof LIB_OLD / sizeof LIB_OLD[0]) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (LIB_OLD[mid] == h) return 1;
        if (LIB_OLD[mid] < h) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

/* a store from before the library (its revision under LIB_REV), once its sounds are read: a slot holding nothing, a
 * factory sound of before (LIB_OLD) or one of the library's own (wherever it sits) is the factory's and takes the
 * library's sound for it; a slot holding the owner's keeps it, the library's sound for it going to the first free slot
 * from S097 (none free: not kept) */
static void lib_refresh(void)
{
    static uint32_t h[LIB_N];                  /* (not on the FM-1's stack) */
    uint8_t fac[M2K_SOUNDS];
    m2k_sound_t s;
    for (int i = 0; i < LIB_N; i++) { lib_sound(i, &s); h[i] = snd_fnv(&s); }
    for (int i = 0; i < M2K_SOUNDS; i++) {
        uint32_t x = snd_fnv(&snd[i]);
        int f = !snd[i].name[0] || lib_old(x);
        for (int j = 0; j < LIB_N && !f; j++) f = x == h[j];
        fac[i] = (uint8_t)f;
        if (f) { m_set(&snd[i], 0, sizeof snd[i]); snd_cat[i] = 0; snd_root[i] = 0; }
    }
    for (int i = 0; i < LIB_N; i++) {
        int at = fac[i] ? i : -1;
        for (int k = LIB_N; k < M2K_SOUNDS && at < 0; k++) if (fac[k]) at = k;
        if (at < 0) continue;
        lib_put(i, at);
        fac[at] = 0;
    }
}
