/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's screen and controls. One grid for every page, the focus cut at 104 (104 / 136: the picture is the larger):
 *   0..15    header: the page tag, the track and its machine badge, the pattern and the transport
 *   18..121  the focus: the last knob's value in big type (left, 104), the page's live picture (right, 136)
 *   124..146 the current track's 16 steps, the four pages under them
 *   148..239 the four knobs: label, value, a slab
 * Controls: page buttons (EDIT SYN, ENV AMP, SEL FILT, FX FX/DEL/DSTR, LFO 1-3, ARP, GLO, HOME), SEQ toggles the
 * keys between playing and the 16 steps (hold a step and turn a knob: a lock), SELECT the track, ALGORITHM the
 * machine, PRESETS the pattern, KNOB 1-4 the page's values, MASTER the volume. */

/* ------------------------------------------------------------------------------------------- parameters */

enum {
    K_NUM, K_BI, K_TIME, K_HOLD, K_DECT, K_ATIM, K_AHLD, K_ADEC, K_AREL, K_MODT, K_MHZ, K_HZ, K_PAN, K_DTIM, K_BITS, K_SEMI, K_RATIO, K_SEMI24, K_INT25, K_SIDW, K_SIDM,
    K_ALGO, K_VOIC, K_WAVE, K_VOF1, K_VOF2, K_CONS, K_TRK, K_DEST, K_LWAV, K_LTRG, K_MULT, K_AMOD, K_ASPD, K_ARNG, K_GATE,
    K_BPM, K_SWNG, K_LEN, K_GLCH, K_GSIZ, K_FINE, K_TUN2, K_SCRN, K_PROJ, K_LOAD, K_MCH, K_SSEL, K_SLOAD, K_SSAVE, K_SCAT, K_WIPE, K_DEMO, K_SPD,
    K_SYIN, K_NTIN, K_THRU,                                              /* GLO > MIDI's and IN's: where MIDI comes from */
    K_FOCT, K_FWID, K_FATK, K_FDEC, K_EQHZ, K_DBHZ, K_SRR, K_PCH, K_WBNK, K_FR24, K_FR64, K_FRSQ, K_ONOFF, K_MSRC, K_MFRQ,
    K_DWV4, K_DWV5, K_DRA, K_DRD, K_DOPN, K_DMUL, K_DNHZ, K_DCNT,         /* the DRM- drums' */
    K_LSPD, K_LDPT,                                                        /* the LFOs' speed and depth */
    K_DCAY, K_DAMP,                                                        /* the reverb's */
    K_PCT, K_GRES, K_LANE,                                                 /* GRIDS' */
    K_MUTQ, K_MOTN                                                         /* GLO > PLAY's, the screen's motion */
};
typedef struct { const char *s4, *name; uint8_t kind; } pi_t;

static const pi_t PI_COMMON[P_LFO - 8] = {
    {"ATCK", "ATTACK", K_ATIM}, {"HOLD", "HOLD", K_AHLD}, {"DEC", "DECAY", K_ADEC}, {"REL", "RELEASE", K_AREL},
    {"DIST", "DISTORTION", K_BI}, {"VOL", "VOLUME", K_NUM}, {"PAN", "PAN", K_PAN}, {"PORT", "PORTAMENTO", K_NUM},
    {"BASE", "FILTER BASE", K_FOCT}, {"WDTH", "FILTER WIDTH", K_FWID}, {"HPQ", "HP RESONANCE", K_NUM}, {"LPQ", "LP RESONANCE", K_NUM},
    {"ATCK", "FILTER ATTACK", K_FATK}, {"DEC", "FILTER DECAY", K_FDEC}, {"B-OF", "BASE BY ENV", K_BI}, {"W-OF", "WIDTH BY ENV", K_BI},
    {"EQF", "EQ FREQUENCY", K_EQHZ}, {"EQG", "EQ GAIN", K_BI}, {"SRR", "RATE REDUCTION", K_SRR}, {"DTIM", "DELAY TIME", K_DTIM},
    {"DSND", "DELAY SEND", K_BI}, {"DFB", "DELAY FEEDBACK", K_NUM}, {"DBAS", "DELAY BASE", K_DBHZ}, {"DWID", "DELAY WIDTH", K_NUM},
    {"BITS", "BIT DEPTH", K_BITS}, {"FOLD", "WAVEFOLD", K_NUM}, {"XOR", "XOR MANGLE", K_NUM}, {"VERB", "REVERB SEND", K_NUM},
    {"PTCH", "TRACK PITCH", K_SEMI}, {"LEVL", "TRACK LEVEL", K_NUM}, {"MODE", "ARP MODE", K_AMOD}, {"SPD", "ARP SPEED", K_ASPD},
    {"RNG", "ARP RANGE", K_ARNG}, {"GATE", "GATE LENGTH", K_GATE},
};
/* each LFO's own (its speed is shown with its MULT: which LFO, from where in here) */
#define PI_LFO1 \
    {"TRCK", "LFO TRACK", K_TRK}, {"DEST", "LFO DESTINATION", K_DEST}, {"WAVE", "LFO WAVE", K_LWAV}, {"TRIG", "LFO TRIG MODE", K_LTRG}, \
    {"MULT", "LFO MULTIPLIER", K_MULT}, {"SPD", "LFO CYCLE", K_LSPD}, {"PHAS", "START PHASE", K_NUM}, {"DPTH", "LFO DEPTH", K_LDPT}
static const pi_t PI_LFO[24] = {PI_LFO1, PI_LFO1, PI_LFO1};
static const pi_t PI_WBNK = {"BANK", "WAVE BANK", K_WBNK};
static const pi_t PI_MOD[4] = {
    {"TYPE", "MOD TYPE", K_MODT}, {"RATE", "MOD RATE", K_MHZ}, {"DPTH", "MOD DEPTH", K_NUM}, {"MIX", "MOD MIX", K_NUM},
};

static const pi_t PI_GLOB[G_COUNT] = {
    {"BPM", "TEMPO", K_BPM}, {"SWNG", "SWING", K_SWNG}, {"LEN", "PATTERN LENGTH", K_LEN}, {"ACNT", "ACCENT", K_NUM},
    {"TAPE", "TAPE WOW", K_NUM}, {"ADPC", "ADPCM CRUNCH", K_NUM}, {"GLCH", "GLITCH AMOUNT", K_GLCH}, {"SIZE", "REPEAT SIZE", K_GSIZ},
    {"DCAY", "REVERB DECAY", K_DCAY}, {"DAMP", "REVERB DAMPING", K_DAMP}, {"HISS", "HISS AND DUST", K_NUM}, {"DRIV", "BUS DRIVE", K_NUM},
    {"SPD", "T1 SPEED", K_SPD}, {"SPD", "T2 SPEED", K_SPD}, {"SPD", "T3 SPEED", K_SPD}, {"SPD", "T4 SPEED", K_SPD},
    {"SPD", "T5 SPEED", K_SPD}, {"SPD", "T6 SPEED", K_SPD},
    {"X", "MAP X", K_PCT}, {"Y", "MAP Y", K_PCT}, {"CHAOS", "CHAOS", K_PCT}, {"RES", "CYCLE", K_GRES},
    {"BD", "KICK DENSITY", K_PCT}, {"SD", "SNARE DENSITY", K_PCT}, {"HH", "HATS DENSITY", K_PCT},
    {"LANE", "T1 LANE", K_LANE}, {"LANE", "T2 LANE", K_LANE}, {"LANE", "T3 LANE", K_LANE}, {"LANE", "T4 LANE", K_LANE},
    {"LANE", "T5 LANE", K_LANE}, {"LANE", "T6 LANE", K_LANE},
};

static const pi_t PI_SET[17] = {
    {"SCRN", "SCREEN", K_SCRN},
    {"PROJ", "PROJECT", K_PROJ},
    {"LOAD", "LOAD PROJECT", K_LOAD},
    {"CHAN", "MIDI CHANNEL", K_MCH},   /* GLO > MIDI */
    {"SYIN", "SYNC IN", K_SYIN},
    {"SYOU", "SYNC OUT", K_ONOFF},
    {"NOTE", "NOTES OUT", K_ONOFF},
    {"SLOT", "SOUND SLOT", K_SSEL},    /* SYN > SND: the sound library */
    {"LOAD", "LOAD SOUND", K_SLOAD},
    {"SAVE", "SAVE SOUND", K_SSAVE},
    {"CAT", "CATEGORY", K_SCAT},
    {"WIPE", "EVERY PATTERN INIT", K_WIPE},   /* GLO > SET */
    {"DEMO", "BANK A'S DEMOS BACK", K_DEMO},
    {"NOTE", "NOTES IN", K_NTIN},      /* GLO > IN */
    {"THRU", "MIDI THRU", K_THRU},
    {"MUTE", "MUTES", K_MUTQ},         /* GLO > PLAY */
    {"MOTN", "MOTION", K_MOTN},        /* GLO > SET on the FM-1, PLAY with projects */
};

typedef struct {
    const char *name, *badge, *style, *cls;
    uint8_t stat[3];                   /* punch, metal, plastic: 0..5 */
    pi_t syn[8];
} mi_t;

/* the machines as the selector steps through them (patterns keep their numbers): the ten tonal ones first, the sine
 * and the SWAV and FM+ families together, then the drums in a kit's order, the noises, the FX (silent alone) */
static const uint8_t M_ORDER[M_COUNT] = {M_GSIN, M_SAW, M_PUL, M_ENS, M_FMS, M_FMP, M_FMD, M_VOX, M_WAV, M_SID,
                                          M_DKIK, M_DSNR, M_DHAT, M_DCYM, M_GPRC, M_GNOI, M_FXC, M_FXD, M_FXR};
static int m_place(int m) { for (int i = 0; i < M_COUNT; i++) if (M_ORDER[i] == m) return i; return 0; }

static const mi_t MI[M_COUNT] = {
    {"GND-SIN", "GND", "SINE", "PERC", {5, 1, 3},
     {{"FOLD", "SINE FOLD", K_NUM}, {"SUB", "SUB LEVEL", K_NUM}, {"NOIS", "NOISE", K_NUM}, {"PDEP", "PITCH DROP", K_NUM},
      {"PDEC", "DROP TIME", K_TIME}, {"CLIK", "CLICK", K_NUM}, {"HARM", "2ND HARMONIC", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"GND-PERC", "GND", "NOISE", "PERC", {3, 4, 1},
     {{"COLR", "COLOUR", K_BI}, {"S&H", "PITCHED HOLD", K_NUM}, {"META", "METAL MIX", K_NUM}, {"MSPR", "METAL SPREAD", K_NUM},
      {"PDEP", "PITCH DROP", K_NUM}, {"PDEC", "DROP TIME", K_TIME}, {"GRIT", "8-BIT GRIT", K_NUM}, {"-", "-", K_NUM}}},
    {"SID-6581", "SID", "CHIP", "SYNTH", {3, 2, 4},
     {{"PW", "PULSE WIDTH", K_NUM}, {"PWAD", "PW SWEEP", K_NUM}, {"PWRS", "SWEEP RESET", K_ONOFF}, {"WAVE", "WAVE", K_SIDW},
      {"MOD", "RING / SYNC", K_SIDM}, {"MSRC", "MOD SOURCE", K_MSRC}, {"MFRQ", "MOD FREQUENCY", K_MFRQ}, {"TUNE", "TUNE", K_TUN2}}},
    {"FM+STAT", "FM+", "STATIC", "SYNTH", {4, 5, 3},
     {{"1FRQ", "RATIO 1", K_FR24}, {"1FIN", "FINE 1", K_BI}, {"1ENV", "DEPTH / DECAY 1", K_BI}, {"1FB", "FEEDBACK 1", K_NUM},
      {"2FRQ", "RATIO 2", K_FR24}, {"2VOL", "DEPTH 2", K_NUM}, {"TONE", "TONE", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"FM+DYN", "FM+", "DYNAMIC", "SYNTH", {4, 4, 5},
     {{"1FRQ", "RATIO 1", K_FR64}, {"1FEN", "RATIO 1 BY ENV", K_BI}, {"1VOL", "DEPTH 1", K_NUM}, {"1VEN", "DEPTH 1 BY ENV", K_BI},
      {"2FRQ", "RATIO 2", K_FRSQ}, {"2ENV", "DEPTH / DECAY 2", K_BI}, {"2FB", "FEEDBACK 2", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"SWAV-SAW", "SWV", "SUPER", "SYNTH", {2, 1, 2},
     {{"UNIL", "UNISON LEVEL", K_NUM}, {"UNIW", "UNISON WIDTH", K_NUM}, {"UNIX", "EXTENDED UNISON", K_NUM}, {"DRFT", "DRIFT", K_NUM},
      {"SUBX", "SQUARE SUB", K_NUM}, {"SUB1", "SINE SUB -1", K_NUM}, {"SUB2", "SINE SUB -2", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"SWAV-ENS", "SWV", "CHORD", "SYNTH", {2, 1, 3},
     {{"PCH2", "PITCH 2", K_PCH}, {"PCH3", "PITCH 3", K_PCH}, {"PCH4", "PITCH 4", K_PCH}, {"WAVE", "SAW TO PULSE", K_NUM},
      {"PW", "PULSE WIDTH", K_NUM}, {"CHRL", "CHORUS LEVEL", K_NUM}, {"CHRW", "CHORUS WIDTH", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"DIGI-WAV", "DGW", "DIGITAL", "SYNTH", {3, 3, 4},
     {{"WAVE", "WAVE", K_WAVE}, {"SCAN", "SCAN BY ENV", K_BI}, {"STIM", "SCAN TIME", K_DECT}, {"SYNC", "HARD SYNC", K_NUM},
      {"BITS", "WAVE BITS", K_NUM}, {"DTUN", "OSC 2 DETUNE", K_BI}, {"MIX2", "OSC 2 LEVEL", K_NUM}, {"STEP", "PHASE STEPS", K_NUM}}},
    {"VOX-6", "VOX", "VOICE", "VOICE", {1, 2, 5},
     {{"VOC1", "VOWEL F1", K_VOF1}, {"VOC2", "VOWEL F2", K_VOF2}, {"V-SW", "VOWEL SWITCH", K_ONOFF}, {"VOIC", "WHISPER", K_NUM},
      {"CONS", "CONSONANT", K_CONS}, {"CLEN", "CONS LENGTH", K_NUM}, {"CVOL", "CONS VOLUME", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"FM+PAR", "FM+", "TRIPLE", "SYNTH", {4, 5, 3},
     {{"1FRQ", "RATIO 1", K_FR24}, {"1ENV", "DEPTH / DECAY 1", K_BI}, {"2FRQ", "RATIO 2", K_FR24}, {"2ENV", "DEPTH / DECAY 2", K_BI},
      {"3FRQ", "RATIO 3", K_FR24}, {"3ENV", "DEPTH / DECAY 3", K_BI}, {"TONE", "TONE", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"SWAV-PUL", "SWV", "PULSE", "SYNTH", {3, 1, 4},
     {{"UNIL", "UNISON LEVEL", K_NUM}, {"UNIW", "UNISON WIDTH", K_NUM}, {"SUB1", "SINE SUB -1", K_NUM}, {"SUB2", "SINE SUB -2", K_NUM},
      {"PW", "PULSE WIDTH", K_NUM}, {"PWAD", "PW SWEEP", K_NUM}, {"PWRS", "SWEEP RESET", K_ONOFF}, {"TUNE", "TUNE", K_TUN2}}},
    {"FX-CHORS", "FX", "CHORUS", "FX", {1, 2, 3},
     {{"DEL", "DELAY", K_NUM}, {"DEP", "DEPTH", K_NUM}, {"SPD", "SPEED", K_NUM}, {"MIX", "MIX", K_NUM},
      {"FB", "FEEDBACK", K_BI}, {"WID", "WIDTH", K_NUM}, {"LP", "LOWPASS", K_NUM}, {"INP", "INPUT", K_NUM}}},
    {"FX-RING", "FX", "RINGMOD", "FX", {2, 4, 2},
     {{"WAVE", "CARRIER WAVE", K_NUM}, {"EXT", "TRACK CARRIER", K_NUM}, {"-", "-", K_NUM}, {"MIX", "MIX", K_NUM},
      {"-", "-", K_NUM}, {"-", "-", K_NUM}, {"-", "-", K_NUM}, {"INP", "INPUT", K_NUM}}},
    {"FX-DYNMX", "FX", "DYNAMIX", "FX", {5, 1, 1},
     {{"ATK", "ATTACK", K_NUM}, {"REL", "RELEASE", K_NUM}, {"THRS", "THRESHOLD", K_NUM}, {"MIX", "MIX", K_NUM},
      {"RAT", "RATIO", K_NUM}, {"GAIN", "MAKE-UP GAIN", K_NUM}, {"RMS", "RMS / PEAK", K_NUM}, {"INP", "INPUT", K_NUM}}},
    {"GND-NOIS", "GND", "NOISE", "PERC", {2, 1, 1},
     {{"ST", "STEREO", K_NUM}, {"RED", "REDNESS", K_NUM}, {"STON", "STEREO ON", K_ONOFF}, {"-", "-", K_NUM},
      {"-", "-", K_NUM}, {"-", "-", K_NUM}, {"-", "-", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    /* the DRM- drums: voice design after the Erica Synths LXR drum voices (GPL-3.0-only) */
    {"DRM-KICK", "DRM", "KICK", "PERC", {5, 2, 3},
     {{"WAVE", "CARRIER WAVE", K_DWV4}, {"PDEP", "PITCH SWEEP", K_DMUL}, {"PDEC", "SWEEP TIME", K_DRA}, {"PCRV", "SWEEP CURVE", K_NUM},
      {"FM", "FM DEPTH", K_NUM}, {"FMFR", "FM PITCH", K_SEMI}, {"SNAP", "SNAP", K_NUM}, {"CLIK", "CLICK", K_NUM}}},
    {"DRM-SNAR", "DRM", "SNARE", "PERC", {4, 2, 4},
     {{"TONE", "TONE WAVE", K_DWV4}, {"PDEP", "PITCH SWEEP", K_DMUL}, {"PDEC", "SWEEP TIME", K_DRA}, {"NOIS", "NOISE RATE", K_DNHZ},
      {"MIX", "TONE TO NOISE", K_NUM}, {"NFLT", "NOISE FILTER", K_BI}, {"CLAP", "CLAP HITS", K_DCNT}, {"CSPC", "CLAP SPACING", K_DRA}}},
    {"DRM-HATS", "DRM", "HATS", "PERC", {3, 5, 2},
     {{"CARR", "CARRIER PITCH", K_SEMI}, {"M1FR", "MOD 1 PITCH", K_SEMI}, {"M1", "MOD 1 DEPTH", K_NUM}, {"M2FR", "NOISE PITCH", K_SEMI},
      {"M2", "NOISE DEPTH", K_NUM}, {"WAVE", "CARRIER WAVE", K_DWV5}, {"CDEC", "CLOSED DECAY", K_DRD}, {"OPEN", "OPEN DECAY", K_DOPN}}},
    {"DRM-CYMB", "DRM", "CYMBAL", "PERC", {2, 5, 2},
     {{"CARR", "CARRIER PITCH", K_SEMI}, {"M1FR", "MOD 1 PITCH", K_SEMI}, {"M1", "MOD 1 DEPTH", K_NUM}, {"M2FR", "NOISE PITCH", K_SEMI},
      {"M2", "NOISE DEPTH", K_NUM}, {"WAVE", "CARRIER WAVE", K_DWV5}, {"ATK", "ATTACK", K_DRA}, {"DEC", "DECAY", K_DRD}}},
};
static const char *const DWAV_N[5] = {"SIN", "TRI", "SAW", "SQR", "NOIS"};

static const char *const SIDW_N[5] = {"TRI", "SAW", "PULS", "MIX", "NOIS"};
static const char *const SIDM_N[4] = {"OFF", "RING", "SYNC", "R+S"};
static const char *const ALGO_N[4] = {"STCK", "PAR", "2CAR", "RING"};
static const char *const LWAV_N[7] = {"TRI", "SIN", "SQR", "SAW", "EXP", "RMP", "RND"};
static const char *const LTRG_N[5] = {"FREE", "TRIG", "HOLD", "ONE", "HALF"};
static const char *const AMOD_N[6] = {"OFF", "UP", "DOWN", "U/D", "RND", "ORDR"};
static const char *const ASPD_N[4] = {"1/32", "1/16", "1/8", "1/4"};
static const char *const GSIZ_N[5] = {"1/64", "1/32", "1/16", "1/8", "1/4"};
/* the trig types a held step steps through with VOL (the expert's order): what each masks, and its name */
static const uint8_t TTYPE[8] = {0, ST_NOFLT | ST_NOLFO, ST_NOAMP | ST_NOLFO, ST_NOAMP | ST_NOFLT, ST_NOLFO, ST_NOFLT,
                                 ST_NOAMP, ST_TTYPE};
static const char *const TTYPE_N[8] = {"ALL", "AMP", "FILTER", "LFO", "AMP+FLT", "AMP+LFO", "FLT+LFO", "NONE"};
static int ttype_of(uint8_t flags)
{
    for (int i = 0; i < 8; i++) if ((flags & ST_TTYPE) == TTYPE[i]) return i;
    return 0;
}

static const char *const COND_N[C_COUNT] = {"100%", "75%", "50%", "33%", "25%", "12%", "1:2", "2:2", "1:3", "2:3", "3:3",
                                           "1:4", "2:4", "3:4", "4:4", "1ST", "!1ST", "PRE", "!PRE", "FILL", "!FIL",
                                           "90%", "60%", "40%", "10%"};
/* the condition knob's order (PRESETS on a held step): the chances from most to least, then the rest as kept */
static const uint8_t COND_ORDER[C_COUNT] = {C_ALWAYS, C_P90, C_P75, C_P60, C_P50, C_P40, C_P33, C_P25, C_P12, C_P10,
                                            C_1_2, C_2_2, C_1_3, C_2_3, C_3_3, C_1_4, C_2_4, C_3_4, C_4_4,
                                            C_1ST, C_N1ST, C_PRE, C_NPRE, C_FILL, C_NFILL};
static int cond_place(int c) { for (int i = 0; i < C_COUNT; i++) if (COND_ORDER[i] == c) return i; return 0; }
#ifndef M2K_VER
#define M2K_VER ""                         /* the version the build is made as: tools/fm1/build.py, web/build.sh */
#endif
static const char *const NOTE_N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static const pi_t PI_NONE = {"-", "-", K_NUM};     /* a knob a page leaves free (NONE) */

static const pi_t *pinfo(int t, int id)
{
    if (id == 0xFF) return &PI_NONE;
    if (id >= 0xC0) { int s = id & 0x3F; return &PI_SET[s == 16 ? 1 : s == 17 ? 2 : s >= 18 && s <= 23 ? 7 + s - 18 : s >= S_MCH && s <= S_NOTO ? 3 + s - S_MCH
                                            : s == S_NTIN ? 13 : s == S_THRU ? 14 : s == S_MUTQ ? 15 : s == S_MOTN ? 16 : 0]; }
    if (id >= 0x80) return &PI_GLOB[id & 0x7F];
    if (id < 8) return &MI[PAT(cur_pat)->machine[t]].syn[id];
    if (id < P_LFO) return &PI_COMMON[id - 8];
    if (id == P_WBNK) return &PI_WBNK;
    if (id >= P_MODT) return &PI_MOD[(id - P_MODT) & 3];
    return &PI_LFO[(id - P_LFO) % 24];
}

static int kind_choices(int k);
/* a knob's choice from its value: FM+'s ratios on the sound's own steps (fr24), the rest even */
static int choice_of(int kind, int v)
{
    int n = kind_choices(kind);
    return kind == K_FR24 ? fr24(v) : kind == K_SYIN ? syin_choice(v) : n ? (v * n) >> 7 : 0;
}

static int kind_choices(int k)
{
    switch (k) {
    case K_RATIO: case K_WAVE: return 32;
    case K_FR24: return 24;
    case K_SIDW: return 5;
    case K_ONOFF: case K_MSRC: case K_THRU: case K_GRES: case K_MUTQ: case K_MOTN: return 2;
    case K_SIDM: case K_ALGO: case K_ASPD: case K_ARNG: case K_SYIN: case K_NTIN: case K_LANE: return 4;
    case K_SCRN: return 2;
    case K_MCH: return 16;
    case K_SSEL: return M2K_SOUNDS;
    case K_SCAT: return CAT_COUNT;
    case K_SPD: return 7;
    case K_MODT: return 3;
    case K_PROJ: return M2K_PROJECTS;
    case K_VOIC: case K_LWAV: return 7;
    case K_TRK: case K_AMOD: return 6;
    case K_LTRG: case K_GSIZ: return 5;
    case K_MULT: return 8;
    case K_DWV4: return 4;
    case K_DWV5: return 5;
    case K_DCNT: return 16;
    case K_INT25: return 25;
    case K_CONS: return 21;
    default: return 0;
    }
}

static int kind_max(int k) { return k == K_BPM ? 255 : k == K_LEN ? 64 : k == K_DEST ? P_COUNT : 127; }
static int kind_min(int k) { return k == K_LEN ? 1 : 0; }

/* ------------------------------------------------------------------------------------------------ strings */

static char *s_cpy(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static void s_cpyn(char *d, const char *s, int n) { while (*s && n-- > 1) *d++ = *s++; *d = 0; }   /* into n bytes */
static char *s_int(char *d, int v)
{
    char t[12];
    int n = 0;
    if (v < 0) { *d++ = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}
static char *s_dec1(char *d, int v10) { d = s_int(d, v10 / 10); *d++ = '.'; *d++ = (char)('0' + v10 % 10); *d = 0; return d; }
/* signed, one decimal: -2.0, +0.5 */
static char *s_sdec1(char *d, int v10) { *d++ = v10 < 0 ? '-' : '+'; return s_dec1(d, v10 < 0 ? -v10 : v10); }
static void s_hz(char *out, const char **unit, int hz)
{
    if (hz < 1000) { s_int(out, hz); *unit = "HZ"; }
    else { s_dec1(out, hz / 100); *unit = "KHZ"; }
}
/* an FM ratio in 4096ths as the Monomachine's screen writes it: its own fractions, else 3 decimals under 1, 2 over, cut */
static void s_ratio(char *d, int r, int sq)
{
    static const uint16_t FR[17] = {64, 128, 256, 384, 512, 576, 640, 768, 1024, 1280, 1536, 1792, 2048, 2304, 2560, 3072, 3584};
    static const char *const FN[17] = {"1/64", "1/32", "1/16", "3/32", "1/8", "9/64", "5/32", "3/16", "1/4", "5/16", "3/8",
                                       "7/16", "1/2", "9/16", "5/8", "3/4", "7/8"};
    for (int i = 0; i < 17; i++)
        if (r == FR[i] && (sq ? r <= 1024 || r == 2304 : r != 576)) { s_cpy(d, FN[i]); return; }
    if (r == 0) { s_cpy(d, "0.0"); return; }
    if (r < 4096) {
        int m = r * 1000 / 4096;
        d[0] = '.'; d[1] = (char)('0' + m / 100); d[2] = (char)('0' + m / 10 % 10); d[3] = (char)('0' + m % 10); d[4] = 0;
        return;
    }
    int c = r * 100 / 4096;
    d = s_int(d, c / 100);
    *d++ = '.';
    if (c % 100) { *d++ = (char)('0' + c / 10 % 10); *d++ = (char)('0' + c % 10); } else *d++ = '0';
    *d = 0;
}
/* a pattern's name on the panel: A01 .. A16 */
/* a pattern's name on the panel: the bank's letter and its place, A01 .. D16 */
static char *s_pat(char *d, int n)
{
    int k = n % 16 + 1;
    d[0] = (char)('A' + n / 16); d[1] = (char)('0' + k / 10); d[2] = (char)('0' + k % 10); d[3] = 0;
    return d + 3;
}
static char *s_note(char *d, int n) { d = s_cpy(d, NOTE_N[n % 12]); return s_int(d, n / 12 - 1); }

/* where a filter step sits on the screen's 20 Hz .. 20 kHz axis (0..1016): the filter follows the track's last note */
static int filt_q3(int t, int32_t steps_q8)
{
    int note = trk[t].note ? trk[t].note : 60;
    int32_t pos = (note * 8 * 256 + 12 * (steps_q8 - 16 * 256)) >> 8;
    return CLAMP((pos * 1087 >> 10) - 132, 0, 1016);
}

/* an LFO's cycle at SPD v and MULT 2^mu (format 16: 2^((84 - v) / 12 - mu) beats of the LFOs' clock, whatever the
 * tempo), as a player counts it: bars, a note (dotted, a triplet) where it is one (the clock's: 1.45 % long), else
 * beats; hertz past a 1/64 note (or past the LFO's fastest, lfo_inc's) */
static void s_lfo_cycle(char *out, const char **unit, int v, int mu)
{
    *unit = "";
    if (!v) { s_cpy(out, "STOP"); *unit = "STILL"; return; }
    uint32_t inc = lfo_inc(v * 256, mu, seq_bpm() * 10);
    int n = 84 - v, r = ((n % 12) + 12) % 12, k = (n - r) / 12 - mu;     /* 2^k beats, times 2^(r / 12) */
    if (inc >= LFO_MAX || k < -4 || (k == -4 && r)) {
        int h10 = (int)mulu_hi(inc, 27563, 32);                              /* inc x 44100 / 16 / 2^32, in 0.1 Hz */
        if (h10 < 1000) s_dec1(out, h10); else s_int(out, (h10 + 5) / 10);
        *unit = "HZ";
        return;
    }
    if (k >= 2 && (r == 0 || r == 7)) {                                      /* a bar or more: 1, 1.5, 2, 3, 4 .. 32 */
        int b2 = (r ? 3 : 2) << (k - 2);                                     /* half bars */
        if (b2 & 1) s_dec1(out, b2 * 5); else s_int(out, b2 / 2);
        *unit = b2 == 2 ? "BAR" : "BARS";
        return;
    }
    if (k < 2 && (r == 0 || r == 7 || (r == 5 && k <= 0))) {                 /* a note: 1/2 .. 1/64, dotted, triplets */
        char *e = s_cpy(out, "1/");
        e = s_int(e, r == 5 ? 2 << -k : k >= 0 ? 4 >> k : 4 << -k);
        if (r) s_cpy(e, r == 7 ? "." : "T");
        *unit = r == 7 ? "NOTE DOTTED" : r == 5 ? "NOTE TRIPLET" : "NOTE";
        return;
    }
    uint32_t c = T_LCYC[v] >> mu;                                            /* 1/1000 beat */
    if (c >= 4000) { s_dec1(out, (int)(c / 400)); *unit = "BARS"; }
    else if (c >= 1000) { s_dec1(out, (int)(c / 100)); *unit = "BEATS"; }
    else { char *e = s_cpy(out, "0."); e[0] = (char)('0' + c / 100); e[1] = (char)('0' + c / 10 % 10); e[2] = 0; *unit = "BEAT"; }
}

/* the value as the screen shows it, and its unit */
static void s_num3(char *o, int v) { o[0] = (char)('0' + v / 100); o[1] = (char)('0' + v / 10 % 10); o[2] = (char)('0' + v % 10); o[3] = 0; }
static int snd_sel_get(void);                        /* SYN > SND's slot (the screen's state, below) */

static void fmt(const pi_t *pi, int t, int v, char *out, const char **unit)
{
    *unit = "";
    int c = choice_of(pi->kind, v);
    switch (pi->kind) {
    case K_BI: s_int(out, v - 64); break;
    case K_FINE: s_int(out, ((v - 64) * 100) / 64); *unit = "CENT"; break;
    case K_TUN2: s_int(out, ((v - 64) * 25) / 8); *unit = "CENT"; break;     /* +-2 semitones, 3.125 c a step */
    case K_PCH: s_cpy(out, T_ENS_PNAME[T_ENS_PIDX[v & 127]]); break;            /* SWAV-ENS's intervals, the just as 5:4 */
    case K_WBNK: s_cpy(out, (v * 2) >> 7 ? "M64" : "M2K"); break;             /* MONO2K's 32, the owner's 64 */
    case K_TIME: case K_HOLD: case K_DECT: case K_ATIM: case K_AHLD: case K_ADEC: case K_AREL: {
        int k = pi->kind;
        if ((k == K_HOLD || k == K_DECT || k == K_AHLD || k == K_ADEC) && v == 127) { s_cpy(out, "INF"); break; }
        uint32_t ms10 = k == K_ATIM ? T_AATK_MS10[v] : k == K_AHLD ? (uint32_t)v * 18750u / seq_bpm()     /* v / 32 beat */
                        : (k == K_ADEC || k == K_AREL) ? T_ADEC_MS10[v] : T_ENV_MS10[v];
        if (ms10 < 100) { s_dec1(out, (int)ms10); *unit = "MS"; }
        else if (ms10 < 10000) { s_int(out, (int)(ms10 / 10)); *unit = "MS"; }
        else if (ms10 < 1000000) { s_dec1(out, (int)(ms10 / 1000)); *unit = "S"; }
        else { s_int(out, (int)(ms10 / 10000)); *unit = "S"; }
        break;
    }
    case K_DRA: case K_DRD: case K_DOPN: {          /* the DRM- drums' ramps, on the LXR's laws */
        if (pi->kind == K_DOPN && v == 0) { s_cpy(out, "OFF"); break; }
        if (pi->kind != K_DRA && v == 127) { s_cpy(out, "INF"); break; }
        uint32_t ms10 = pi->kind == K_DRA ? T_DRM_RA_MS10[v] : T_DRM_RD_MS10[v];
        if (ms10 < 100) { s_dec1(out, (int)ms10); *unit = "MS"; }
        else if (ms10 < 10000) { s_int(out, (int)(ms10 / 10)); *unit = "MS"; }
        else if (ms10 < 1000000) { s_dec1(out, (int)(ms10 / 1000)); *unit = "S"; }
        else { s_int(out, (int)(ms10 / 10000)); *unit = "S"; }
        break;
    }
    case K_DWV4: case K_DWV5: s_cpy(out, DWAV_N[c]); break;
    case K_LSPD: {                                  /* with its own LFO's MULT */
        int l = (int)(pi - PI_LFO) >> 3;
        s_lfo_cycle(out, unit, v, choice_of(K_MULT, PAT(cur_pat)->kit[t][P_LFO + 8 * l + L_MULT]));
        break;
    }
    case K_LDPT: {                                  /* its swing against full (format 16: squared about 64) */
        int d = T_LDPTH[v & 127], p10 = d < 0 ? -((-d * 1000 + 8192) >> 14) : (d * 1000 + 8064) / 16128;
        if (!p10) s_cpy(out, "0");
        else if (p10 > -100 && p10 < 100) s_sdec1(out, p10);
        else { char *e = s_cpy(out, p10 < 0 ? "-" : "+"); s_int(e, (p10 < 0 ? -p10 : p10) / 10); }
        *unit = "%";
        break;
    }
    case K_DMUL: s_dec1(out, (256 + T_DRM_PAMT[v]) * 10 / 256); *unit = "X PITCH"; break;   /* at the start: 1 + A */
    case K_DNHZ: s_hz(out, unit, (22050 * (v | 1)) / 127); break;
    case K_DCNT: if (!c) s_cpy(out, "OFF"); else { s_int(out, c); *unit = "HITS"; } break;
    case K_MODT: { static const char *const N[3] = {"CHOR", "FLNG", "PHAS"}; s_cpy(out, N[c < 3 ? c : 2]); break; }
    case K_MHZ: s_dec1(out, T_MOD_HZ10[v]); *unit = "HZ"; break;
    case K_HZ: s_hz(out, unit, T_HZ[v]); break;
    case K_EQHZ: s_hz(out, unit, T_EQHZ[v]); break;
    case K_DBHZ: s_hz(out, unit, T_DBHZ[v]); break;
    case K_SRR: if (!v) { s_cpy(out, "OFF"); break; } s_hz(out, unit, T_SRRHZ[v]); break;
    case K_FOCT: s_sdec1(out, ((v - 16) * 10) / 8); *unit = "OCT"; break;     /* the filter follows the note: 16 is on it */
    case K_FWID: s_sdec1(out, (v * 10) / 8); *unit = "OCT"; break;
    case K_FATK: case K_FDEC: {
        if (pi->kind == K_FDEC && v == 127) { s_cpy(out, "INF"); break; }
        uint32_t ms10 = pi->kind == K_FATK ? T_FATK_MS10[v] : T_FDEC_MS10[v];
        if (ms10 < 100) { s_dec1(out, (int)ms10); *unit = "MS"; }
        else if (ms10 < 10000) { s_int(out, (int)(ms10 / 10)); *unit = "MS"; }
        else { s_dec1(out, (int)(ms10 / 1000)); *unit = "S"; }
        break;
    }
    case K_PAN:
        if (v == 64) s_cpy(out, "C");
        else { char *e = s_cpy(out, v < 64 ? "L" : "R"); s_int(e, v < 64 ? 64 - v : v - 64); }
        break;
    case K_DTIM: {
        if (v == 0) { s_cpy(out, "COMB"); break; }
        /* in 256ths of a note, as the Monomachine counts: simplify */
        int num = v, den = 256;
        while (!(num & 1) && den > 1) { num >>= 1; den >>= 1; }
        char *e = s_int(out, num);
        if (den >= 128) { *unit = den == 256 ? "/256 NOTE" : "/128 NOTE"; break; }   /* (the cell's width) */
        if (den > 1) { *e++ = '/'; s_int(e, den); }
        *unit = "NOTE";
        break;
    }
    case K_BITS: s_int(out, 16 - (v * 14 >> 7)); *unit = "BIT"; break;
    case K_SEMI: s_int(out, v - 64); *unit = "SEMI"; break;
    case K_RATIO: s_cpy(out, T_RATIO_NAME[c]); *unit = "RATIO"; break;
    case K_FR24: s_cpy(out, T_FMR24_NAME[c]); *unit = "RATIO"; break;   /* FM+'s 24 steps */
    case K_FR64: s_ratio(out, v == 127 ? 8192 : v * 64, 0); *unit = "RATIO"; break;          /* FM+DYN's: v / 64, 2 at 127 */
    case K_FRSQ: s_ratio(out, v == 127 ? 16384 : v * v, 1); *unit = "RATIO"; break;          /* (v / 64)^2, 4 at 127 */
    case K_SEMI24: s_int(out, ((v - 64) * 24) / 64); *unit = "SEMI"; break;
    case K_INT25: { char *e = s_cpy(out, "+"); s_int(e, c); *unit = "SEMI"; break; }
    case K_SIDW: s_cpy(out, SIDW_N[c]); break;
    case K_SIDM: s_cpy(out, SIDM_N[c]); break;
    case K_ONOFF: s_cpy(out, c ? "ON" : "OFF"); break;
    case K_MSRC: s_cpy(out, c ? "PRCH" : "MFRQ"); *unit = c ? "PREV TRK" : ""; break;
    case K_MFRQ: s_note(out, 48 + (v * 3 + 4) / 8); *unit = "NOTE"; break;     /* C3 at 0, an octave every 32 */
    case K_ALGO: s_cpy(out, ALGO_N[c]); break;
    case K_VOIC: s_int(out, c + 1); *unit = "VOICES"; break;
    case K_WAVE: s_cpy(out, (PAT(cur_pat)->kit[t][P_WBNK] * 2) >> 7 ? T_WAVE64_NAME[v >> 1 > 63 ? 63 : v >> 1] : T_WAVE_NAME[c]); break;
    case K_VOF1: s_int(out, T_VO_F1HZ[v]); *unit = "HZ F1"; break;
    case K_VOF2: s_int(out, T_VO_F2HZ[v]); *unit = "HZ F2"; break;
    case K_CONS: s_cpy(out, T_VO_CONS[c]); break;
    case K_TRK: { char *e = s_cpy(out, "T"); s_int(e, c + 1); break; }
    case K_DEST:
        if (v == 0) s_cpy(out, "OFF");
        else s_cpy(out, pinfo(t, v - 1)->s4);
        break;
    case K_LWAV: s_cpy(out, LWAV_N[c]); break;
    case K_LTRG: s_cpy(out, LTRG_N[c]); break;
    case K_MULT: { char *e = s_cpy(out, "X"); s_int(e, 1 << c); break; }
    case K_AMOD: s_cpy(out, AMOD_N[c]); break;
    case K_ASPD: s_cpy(out, ASPD_N[c]); *unit = "NOTE"; break;
    case K_ARNG: s_int(out, c + 1); *unit = "OCT"; break;
    case K_GATE: { int v10 = (v * 10) / 16; s_dec1(out, v10); *unit = "STEPS"; break; }
    case K_BPM: s_int(out, 30 + v); *unit = "BPM"; break;
    case K_SWNG: s_int(out, 50 + (v * 25) / 127); *unit = "%"; break;
    case K_LEN: s_int(out, v); *unit = "STEPS"; break;
    case K_GLCH: {                     /* a sixteenth's chance of a repeat, its law's: 0.4 % .. 50 % */
        int c10 = (int)((T_GLCH_P[v & 127] * 1000u + 32768u) >> 16);
        if (c10 < 100) s_dec1(out, c10); else s_int(out, (c10 + 5) / 10);
        *unit = "% A STEP";
        break;
    }
    case K_DCAY: {                     /* the reverb's RT60, its law's: 0.3 s .. 24 s */
        uint32_t ms = T_RV_MS[v & 127];
        if (!v) s_cpy(out, "OFF");
        else if (ms < 1000) { s_int(out, (int)ms); *unit = "MS"; }
        else { s_dec1(out, (int)((ms + 50) / 100)); *unit = "S"; }
        break;
    }
    case K_DAMP: {                     /* the top's RT60 a share of the bottom's: 1/1 .. 1/30 */
        int n10 = (int)((10u * (256u + T_RV_DA[v & 127]) + 128u) >> 8);
        char *e = s_cpy(out, "1/");
        if (n10 < 100) s_dec1(e, n10); else s_int(e, (n10 + 5) / 10);
        *unit = "TOP'S TIME";
        break;
    }
    case K_GSIZ: s_cpy(out, GSIZ_N[(v * 5) >> 7]); *unit = "NOTE"; break;
    case K_SCRN: s_cpy(out, c ? "DARK" : "LIGHT"); *unit = "DISPLAY"; break;
    case K_MCH: { char *e = s_cpy(out, "CH"); s_int(e, c + 1); *unit = "T1; T2 NEXT"; break; }
    case K_SYIN: case K_NTIN: {        /* (BOTH: "USB+TRS" is wider than a knob) */
        static const char *const N[4] = {"OFF", "USB", "TRS", "BOTH"};
        int m = pi->kind == K_SYIN ? c : NTIN_FROM[c];   /* each as its sources (m2k_seq.c) */
        s_cpy(out, N[m]);
        *unit = m == 3 ? "USB + TRS" : "";
        break;
    }
    case K_THRU: s_cpy(out, c ? "ON" : "OFF"); *unit = "TRS TO USB"; break;
    case K_PCT: s_int(out, (v * 100 + 63) / 127); *unit = "%"; break;
    case K_GRES: s_cpy(out, c ? "32" : "16"); *unit = c ? "TWO BARS" : "A BAR"; break;   /* Grids' cycle */
    case K_LANE: { static const char *const N[4] = {"OFF", "BD", "SD", "HH"}, *const U[4] = {"", "KICK", "SNARE", "HATS"};
                   s_cpy(out, N[c]); *unit = U[c]; break; }
    case K_MUTQ: s_cpy(out, c ? "NOW" : "BAR"); *unit = c ? "AT ONCE" : "NEXT BAR"; break;
    case K_MOTN: s_cpy(out, c ? "GLTCH" : "CALM"); *unit = c ? "GLITCHES" : "CLEAN CUTS"; break;
    case K_SSEL: { int i = snd_sel_get(); out[0] = 'S'; s_num3(out + 1, i + 1);
                   *unit = snd[i].name[0] ? snd[i].name : "INIT"; break; }
    case K_SLOAD: s_cpy(out, "TURN"); *unit = "TO LOAD"; break;
    case K_SSAVE: s_cpy(out, "TURN"); *unit = "TO SAVE"; break;
    case K_SCAT: s_cpy(out, CAT_N[c]); *unit = "SHOWN"; break;
    case K_SPD: { static const char *const N[7] = {"1/8X", "1/4X", "1/2X", "3/4X", "1X", "3/2X", "2X"};
                  s_cpy(out, N[c]); *unit = "SPEED"; break; }
    case K_PROJ: { char *e = s_cpy(out, "P"); s_int(e, c + 1); *unit = m2k_project_name(c); break; }
    case K_LOAD: s_cpy(out, "TURN"); *unit = "TO LOAD"; break;
    case K_WIPE: s_cpy(out, "TURN"); *unit = "ALL INIT"; break;
    case K_DEMO: s_cpy(out, "TURN"); *unit = "DEMOS BACK"; break;
    default: s_int(out, v); break;
    }
}

/* -------------------------------------------------------------------------------------------- the pages */

enum { PG_HOME, PG_SYN, PG_AMP, PG_FILT, PG_FX, PG_LFO, PG_ARP, PG_GLO, PG_COUNT };
static const uint8_t NSUB[PG_COUNT] = {1, 3, 2, 2, 4, 6, 3, 7};   /* SYN: its two, then SND (BANK before it); ARP, GRIDS */
static int nsub(int page);                     /* SYN has a third page, the bank, where the machine plays waves */
#define SETTING(i) (0xC0 | (i))
#define GL(g) (0x80 | (g))
#define NONE 0xFF
static int view_trk(void);                     /* the track in view (LANE is its own) */

static int knob_param(int page, int sub, int k)
{
    switch (page) {
    case PG_HOME: { static const uint8_t m[4] = {P_LEVL, P_PTCH, GL(G_TMPO), GL(G_SWNG)}; return m[k]; }
    case PG_SYN: return sub < 2 ? P_SYN + sub * 4 + k : sub == nsub(PG_SYN) - 1 ? SETTING(18 + k)
                        : k == 0 ? P_WBNK : NONE;
    case PG_AMP: return P_ATK + sub * 4 + k;
    case PG_FILT: return P_BASE + sub * 4 + k;
    case PG_FX: return sub < 3 ? P_EQF + sub * 4 + k : P_MODT + k;
    case PG_LFO: return P_LFO + (sub >> 1) * 8 + (sub & 1) * 4 + k;
    case PG_ARP: return sub == 0 ? P_AMOD + k : sub == 1 ? GL(G_GRX + k) : k < 3 ? GL(G_GRBD + k) : GL(G_LANE1 + view_trk());
    case PG_GLO: return sub == 6 ? (k == 0 ? SETTING(S_MUTQ) : k == 1 && M2K_PROJECTS > 1 ? SETTING(S_MOTN) : NONE)   /* PLAY */
                        : sub == 5 ? (k == 0 ? SETTING(S_NTIN) : k == 1 ? SETTING(S_THRU) : NONE)   /* IN */
                        : sub < 3 ? GL(sub * 4 + k) : sub == 4 ? SETTING(S_MCH + k)      /* PROJ and LOAD only with */
                        : k == 0 ? SETTING(S_SCREEN) : M2K_PROJECTS < 2 ? (k == 1 ? SETTING(22) : k == 2 ? SETTING(23) : SETTING(S_MOTN))
                        : k == 1 ? SETTING(16) : k == 2 ? SETTING(17) : k == 3 ? SETTING(22) : NONE;   /* projects to choose */
    default: return NONE;
    }
}

static void page_label(int page, int sub, char *o)
{
    static const char *const N[PG_COUNT] = {"HOME", "SYN", "AMP", "FILT", "FX", "LFO", "ARP", "GLO"};
    switch (page) {
    case PG_FX: s_cpy(o, sub == 0 ? "FX" : sub == 1 ? "DELAY" : sub == 2 ? "DSTR" : "MOD"); break;
    case PG_LFO: { char *e = s_cpy(o, "LFO"); e = s_int(e, (sub >> 1) + 1); s_cpy(e, (sub & 1) ? "B" : "A"); break; }
    case PG_GLO: s_cpy(o, sub == 0 ? "TIME" : sub == 1 ? "GLITCH" : sub == 2 ? "VERB" : sub == 3 ? "SET" : sub == 4 ? "MIDI" : sub == 5 ? "IN" : "PLAY"); break;
    case PG_ARP: s_cpy(o, sub == 0 ? "ARP" : sub == 1 ? "GRIDS" : "LANES"); break;
    case PG_SYN: s_cpy(o, sub >= 2 && sub == nsub(PG_SYN) - 1 ? "SND" : sub == 2 ? "BANK" : "SYN"); break;
    default: s_cpy(o, N[page]); break;
    }
}

/* --------------------------------------------------------------------------------------------- UI state */

static struct {
    uint32_t now, boot_t0;
    uint8_t page, sub, trk, grid, spage, follow;
    int8_t oct[M2K_TRACKS];            /* each track's keyboard octave, from its home (OCT-/+: the track in view's) */
    uint8_t note[M2K_TRACKS];          /* the last note played on each track: new trigs take it */
    /* steps held in GRID */
    uint8_t held[8], nheld, held_added[8], held_edit;
    uint8_t btn[B_COUNT], glo_used, keys[M2K_KEYS];
    /* what each key's press did (KA_*, and its note, track, effect, page or step), so that its release undoes that
     * whatever is held by then; when it was pressed */
    uint8_t kact[M2K_KEYS], karg[M2K_KEYS];
    uint32_t kt0[M2K_KEYS];
    uint32_t glo_t0;                   /* GLO pressed: held a moment alone, its list shows */
    uint8_t pads;                      /* the keys as pads: white 1-6 and 9-14 hit T1-T6 */
    uint32_t clr_t;                    /* a first GLO + SEQ (clear): a second soon after makes it INIT */
    uint8_t clr_pat, clr_trk, clr_whole;
    /* a section button held (HOME, EDIT): its keys and buttons (held_combo) */
    uint32_t hold_t0;                  /* HOME or EDIT pressed */
    uint8_t hold_used, pbank, pick[M2K_PATTERNS / 8], hclr;   /* the bank shown, the patterns picked, a first clear */
    uint8_t pfb;                       /* PERFORM's bank on the black keys: 0 A, 1 B (FX held: OCT- A, OCT+ B) */
    uint32_t seq_t0;                   /* SEQ pressed: a tap is GRID, held it is the SEQ layer */
    uint8_t seq_used;
    uint32_t lay_t0[2];                /* FX, LFO pressed: a tap is the page, held the layer (PERFORM, momentary knobs) */
    uint8_t lay_used[2];
    uint32_t tap_t[4];                 /* tap tempo's last taps (GLO + white 8) */
    uint8_t ntap;
    uint8_t octd, octd_used;           /* OCT- held: a knob turned meanwhile takes its lock away (or its value home) */
    uint8_t latch;                     /* a layer double-tapped open (its button + 1): GLO, SEQ, HOME, FX */
    uint32_t arp_tap;                  /* ARP's last press: a second within 300 ms is the arp's HOLD */
    uint8_t arp_sub;                   /* (the page that press left, + 1, when it turned ARP's: HOLD puts it back) */
    uint8_t dt_b, dt_page, dt_sub, dt_grid, dt_pads;   /* the last clean tap (+ 1) and what it changed */
    uint32_t dt_t;
    uint8_t card, card_used;           /* a button held alone (+ 1) for its help card; something else touched */
    uint32_t card_t0;
    uint32_t rec_t0, save_t0;
    uint8_t rec_used;                  /* REC held and SEQ pressed: its release neither arms nor fills */
    uint8_t save_fired, proj_sel, snd_sel, snd_cat;   /* SYN > SND's slot, the category shown (0 all) */
    uint8_t pkey, pfired, cursor;      /* a page key held in GRID (+1), its hold done; step entry's step */
    uint8_t pkey_k;                    /* (that page key) */
    uint32_t pkey_t0;
    uint32_t edit_t0;                  /* EDIT pressed: a tap is SYN, held it is a modifier */
    uint8_t syn_n;                     /* SYN's page count ui.sub was set against (the machine's: BANK or not) */
    uint32_t load_t, snd_t;                  /* a first turn of LOAD (a project, a sound) or SAVE (a sound) */
    uint8_t snd_arm, snd_last;               /* which knob that turn was (LOAD or SAVE a sound), on which slot: the same
                                              * turn's next detents wait */
    /* the focus */
    uint8_t focus;
    uint32_t focus_t;
    char roll_old[12];
    uint32_t roll_t;
    int32_t ease[4];                   /* Q8 */
    uint16_t fdt;                      /* ms since the last frame (its motions go by the time) */
    /* animations */
    uint32_t trans_t, glitch_t, card_t, save_t, toast_t;
    uint16_t trans_len;                /* ms; the boot's is longer than a byte */
    uint8_t glitch_amt, in_trans, trans_down;
    uint32_t glitch_seed;
    char toast[24], toast2[24];
    uint8_t seen_trig[M2K_TRACKS];
    uint32_t flash_t[M2K_TRACKS];
    int32_t icon_ph, icon_spd;         /* Q8 frames, and frames per second Q8 */
    uint32_t icon_hit;                 /* when the track in view last trigged (its mark jumps) */
    uint8_t icon_amp;                  /* how hard: the track's meter then, 0..63 */
    int32_t mhist[16];                 /* the track in view's meter, a frame each (SWAV-SAW's teeth read it late) */
    uint8_t mh, saw_lv[3], saw_fall[3];
    int32_t meter[M2K_TRACKS];
    /* the splash: when it began (boot_t0) and when it ends (a key, a button or a knob skips to its last part; the
     * press is its alone, its release too) */
    uint32_t splash_end, skip_keys;
    uint16_t skip_btns;
    uint8_t splashed, trans_wipe;      /* the wipe into HOME begun; a transition that is the wipe */
    uint32_t pop_t;                    /* a step toggled (its box pops a moment): when, which */
    uint8_t pop_s, pop_trk;
    uint8_t leds[M2K_KEYS + B_COUNT];
    int8_t kw[M2K_KEYS], kb[M2K_KEYS];
} ui;

static void ui_forget(void);
static int view_trk(void) { return ui.trk; }

#define SPLASH_MS 2100                 /* the splash at power-on, then the wipe into HOME */
#define WIPE_MS 400

/* a track at home on note n: its new trigs take n, its keys the octave whose lowest F is 1 to 12 semitones under n (a
 * C on their first C), so a kick at C2 plays as a kick from the keys and a bass as a bass */
static void track_home(int t, int n)
{
    ui.note[t] = (uint8_t)n;
    ui.oct[t] = (int8_t)CLAMP((n + 66) / 12 - 10, -3, 3);      /* floor((n - 54) / 12), n from 0 */
}

/* every track of the open pattern at home: on its root (its trigs'), or its machine's home note without trigs (a
 * pattern opened, a project or a store loaded, the boot) */
static void keys_home(void)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    for (int t = 0; t < M2K_TRACKS; t++) {
        int r = pat_root(p, t);
        track_home(t, r >= 0 ? r : mach_home(p->machine[t]));
    }
}

/* a track's root, the note PADS plays it at and the library keeps a sound made at (pat_root); a track without trigs
 * plays the note new trigs take */
static int track_root(const m2k_pattern_t *p, int t) { int r = pat_root(p, t); return r < 0 ? ui.note[t] : r; }

static void ui_init(void)
{
    m_set(&ui, 0, sizeof ui);
    ui_forget();
    for (int k = 0, w = 0, b = 0; k < M2K_KEYS; k++) {
        int black = (0x54a >> ((k + 5) % 12)) & 1;
        ui.kw[k] = (int8_t)(black ? -1 : w++);
        ui.kb[k] = (int8_t)(black ? b++ : -1);
    }
    keys_home();
    ui.follow = 1;
    ui.page = PG_HOME;
    ui.icon_spd = 6 << 8;
    ui.boot_t0 = 0xFFFFFFFFu;
    ui.splash_end = SPLASH_MS;
}

static void start_glitch(int amt);

/* the screen's motion (SET > MOTN, S_MOTN): CALM, the default, cuts pages and values clean and flashes nothing on an
 * action (a player found every glitch a moment's refocusing, on a firmware this full of pages); GLITCH is the
 * first redesign's. The splash and its wipe are the same either way */
static int calm(void) { return store_.set[S_MOTN] < 64; }

static void start_trans_any(int len, int down)
{
    ui.trans_down = (uint8_t)down;
    ui.trans_wipe = 0;
    m_cpy(fb_prev, fb_out, sizeof fb_prev);
    ui.trans_t = ui.now;
    ui.trans_len = (uint16_t)len;
    ui.in_trans = 1;
}

static void start_trans_dir(int len, int down) { if (!calm()) start_trans_any(len, down); }
static void start_trans(int len) { start_trans_dir(len, 0); }
static void start_glitch(int amt)
{
    if (calm()) return;
    if (ui.glitch_t && ui.now - ui.glitch_t < 120 && amt <= ui.glitch_amt) return;
    ui.glitch_t = ui.now | 1u;
    ui.glitch_amt = (uint8_t)amt;
    ui.glitch_seed = rnd32();
}

static int nsub(int page) { return page == PG_SYN && PAT(cur_pat)->machine[ui.trk] == M_WAV ? 4 : NSUB[page]; }
static int snd_page(void) { return ui.page == PG_SYN && ui.sub >= 2 && ui.sub == nsub(PG_SYN) - 1; }

/* SYN's pages are the machine's (BANK only for the wave machines): when the track's machine changes under them (ALGO,
 * another track, a sound loaded or pasted, an undo, another pattern) the library stays the library and a BANK that
 * went is SYN's second page, never a page past the last */
static void syn_sub_check(void)
{
    int n = nsub(PG_SYN);
    if (n == ui.syn_n) return;
    if (ui.syn_n && ui.page == PG_SYN && ui.sub >= 2) ui.sub = (uint8_t)(ui.sub == ui.syn_n - 1 ? n - 1 : 1);   /* (0: not yet) */
    ui.syn_n = (uint8_t)n;
}

static void toast(const char *a, const char *b)
{
    s_cpyn(ui.toast, a, sizeof ui.toast);
    s_cpyn(ui.toast2, b ? b : "", sizeof ui.toast2);
    ui.toast_t = ui.now | 1u;
}

static void go(int page)
{
    if (page == ui.page) ui.sub = (uint8_t)((ui.sub + 1) % nsub(page));
    else { ui.page = (uint8_t)page; ui.sub = 0; }
    ui.focus_t = 0;
    start_trans(250);
}

/* the verbs' nouns and verbs (below), and the undo's copy, for the knobs' own takings-away */
enum { N_PAT, N_TRK, N_PICK, N_SND, N_STEP };
enum { V_COPY, V_PASTE, V_CLEAR };
static void undo_mark(int noun, int verb);

/* a question (only before a write that cannot be taken back: a sound over a sound, every pattern, the demos, a
 * project): OCT+ YES, OCT- NO, anything else NO */
enum { ASK_NONE, ASK_SND_OVER, ASK_WIPE, ASK_DEMO, ASK_PROJ, ASK_GRIDS };
/* what it asks about is taken as it is asked (the slot, its category, the track, the pattern, the project): a knob
 * turned or a pattern changed before the answer leaves the question as it reads */
static struct { uint8_t kind, slot, cat, trk, pn, proj; uint32_t t; char a[20], b[28]; } ask;

static void ask_open(int kind, const char *a, const char *b)
{
    ask.kind = (uint8_t)kind;
    ask.t = ui.now | 1u;
    ask.slot = ui.snd_sel; ask.cat = ui.snd_cat; ask.trk = ui.trk; ask.pn = (uint8_t)cur_pat; ask.proj = ui.proj_sel;
    s_cpyn(ask.a, a, sizeof ask.a);
    s_cpyn(ask.b, b, sizeof ask.b);
}

/* SYN > SND's list: the slots of the category shown, and the INIT ones (to save into) */
static int snd_shown(int i) { return !ui.snd_cat || !snd[i].name[0] || snd_cat[i] == ui.snd_cat; }
static int snd_list(int sel, int *n)                 /* sel's place among those shown; *n how many */
{
    int at = 0, c = 0;
    for (int i = 0; i < M2K_SOUNDS; i++) if (snd_shown(i)) { if (i < sel) at++; c++; }
    *n = c;
    return at;
}
static int snd_sel_get(void) { return ui.snd_sel; }
static int snd_nth(int k)
{
    for (int i = 0; i < M2K_SOUNDS; i++) if (snd_shown(i) && !k--) return i;
    return 0;
}

static void sound_load(m2k_pattern_t *p)
{
    m2k_sound_t *sd = &snd[ui.snd_sel];
    char o[16] = {'T', (char)('1' + ui.trk), ' ', 0};
    if (sd->name[0]) {                           /* its keys and new trigs on the note it was made at */
        p->machine[ui.trk] = sd->machine;
        m_cpy(p->kit[ui.trk], sd->kit, P_COUNT);
        track_home(ui.trk, snd_root[ui.snd_sel]);
        s_cpy(o + 3, sd->name);
        toast("SOUND LOADED", o);
    } else {                                     /* INIT: the track's machine as it starts */
        kit_init(p, ui.trk, p->machine[ui.trk]);
        track_home(ui.trk, mach_home(p->machine[ui.trk]));
        s_cpy(o + 3, MI[p->machine[ui.trk]].name);
        toast("SOUND INIT", o);
    }
    m_cpy(trk[ui.trk].eff, p->kit[ui.trk], P_COUNT);
    ui.card_t = ui.now | 1u;
    start_glitch(2);
}

/* the value a knob shows: a held step's lock, or the kit */
/* the knob k turns: the page's, or while SEQ is held the sequencer's (LEN, SWING, ACCENT, TEMPO) */
static int held(int b) { return ui.btn[b] || ui.latch == b + 1; }    /* (a latched layer is held) */
static int seq_layer(void) { return held(B_SEQ) && !held(B_GLO) && !held(B_HOME) && !ui.btn[B_EDIT]; }
static int perf_layer(void) { return held(B_FX) && !held(B_GLO) && !held(B_HOME) && !ui.btn[B_EDIT]; }
static int mom_layer(void) { return ui.btn[B_LFO] && !held(B_GLO) && !held(B_HOME) && !ui.btn[B_EDIT]; }
static int knob_at(int k)
{
    const uint8_t SEQ_K[4] = {GL(G_LEN), (uint8_t)GL(G_SPD1 + ui.trk), GL(G_SWNG), GL(G_TMPO)};   /* (the track's speed) */
    static const uint8_t PERF_K[4] = {GL(G_GLCH), GL(G_TAPE), GL(G_ADPC), GL(G_DRIV)};   /* the bus's, to stay */
    return seq_layer() ? SEQ_K[k] : perf_layer() ? PERF_K[k] : knob_param(ui.page, ui.sub, k);
}

/* LFO held: what the knobs move snaps back when it is let go (OCT+ while held keeps it), into the pattern it was
 * turned on (a queued one may have opened meanwhile) */
static struct { uint8_t n, keep, pn[16], trk[16], code[16], val[16]; } mom;
static void mom_note(int code)
{
    if (!mom_layer() || code >= 0xC0) return;
    for (int i = 0; i < mom.n; i++) if (mom.code[i] == code && mom.trk[i] == ui.trk && mom.pn[i] == cur_pat) return;
    if (mom.n >= 16) return;
    m2k_pattern_t *p = PAT(cur_pat);
    mom.pn[mom.n] = (uint8_t)cur_pat;
    mom.trk[mom.n] = ui.trk;
    mom.code[mom.n] = (uint8_t)code;
    mom.val[mom.n++] = code >= 0x80 ? p->glob[code & 0x7F] : p->kit[ui.trk][code];
}
static void mom_release(void)
{
    if (!mom.keep)
        for (int i = 0; i < mom.n; i++) {
            m2k_pattern_t *p = PAT(mom.pn[i]);
            if (mom.code[i] >= 0x80) p->glob[mom.code[i] & 0x7F] = mom.val[i];
            else {
                p->kit[mom.trk[i]][mom.code[i]] = mom.val[i];
                if (mom.pn[i] == cur_pat) trk[mom.trk[i]].eff[mom.code[i]] = mom.val[i];
            }
        }
    if (mom.n) toast(mom.keep ? "KEPT" : "BACK", mom.keep ? "THE KNOBS STAY" : "THE KNOBS AS THEY WERE");
    mom.n = 0;
    mom.keep = 0;
}

static int knob_value(int code)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    if (code == NONE) return 0;                       /* (not store_.set[63]) */
    if (code == SETTING(16)) return enc(ui.proj_sel, M2K_PROJECTS);
    if (code == SETTING(17)) return 0;
    if (code == SETTING(18)) { int n = 0, at = snd_list(ui.snd_sel, &n); return enc(at, n ? n : 1); }
    if (code == SETTING(19) || code == SETTING(20) || code == SETTING(22) || code == SETTING(23)) return 0;
    if (code == SETTING(21)) return enc(ui.snd_cat, CAT_COUNT);
    if (code >= 0xC0) return store_.set[code & 0x3F];
    if (code >= 0x80) return p->glob[code & 0x7F];
    if (ui.grid && ui.nheld) {
        int i = lock_find(ui.trk, ui.held[0], code);
        if (i >= 0) return p->lock[i].val;
    }
    return p->kit[ui.trk][code];
}

static int knob_locked(int code)
{
    return code < 0x80 && ui.grid && ui.nheld && lock_find(ui.trk, ui.held[0], code) >= 0;
}

static void edit(int k, int d)
{
    int code = knob_at(k);
    if (code == NONE) return;
    if (ui.octd && code < 0xC0) {                     /* OCT- held: the lock away (a step held), else the value home */
        ui.octd_used = 1;
        m2k_pattern_t *q = PAT(cur_pat);
        const pi_t *pi0 = pinfo(ui.trk, code);
        if (ui.grid && ui.nheld && code < 0x80) {
            undo_mark(N_STEP, 20);
            for (int i = 0; i < ui.nheld; i++) {
                int li = lock_find(ui.trk, ui.held[i], code);
                if (li >= 0) q->lock[li].trk = 0xFF;
            }
            lockmap_build();
            trk[ui.trk].eff[code] = q->kit[ui.trk][code];
            ui.held_edit = 1;
            toast(pi0->s4, "LOCK REMOVED");
        } else {
            undo_mark(N_PAT, 21);
            if (code >= 0x80) { pat_init(&pat_tmp, "X"); q->glob[code & 0x7F] = pat_tmp.glob[code & 0x7F]; }
            else { kit_init(&pat_tmp, ui.trk, q->machine[ui.trk]); q->kit[ui.trk][code] = pat_tmp.kit[ui.trk][code]; trk[ui.trk].eff[code] = q->kit[ui.trk][code]; }
            toast(pi0->s4, "BACK TO ITS START");
        }
        ui.focus = (uint8_t)k; ui.focus_t = ui.now | 1u;
        return;
    }
    if (seq_layer()) ui.seq_used = 1;
    if (perf_layer()) ui.lay_used[0] = 1;
    if (mom_layer()) { ui.lay_used[1] = 1; mom_note(code); }
    m2k_pattern_t *p = PAT(cur_pat);
    const pi_t *pi = pinfo(ui.trk, code);
    if (pi->s4[0] == '-') return;
    char old[12];
    const char *u;
    int v = knob_value(code);
    fmt(pi, ui.trk, v, old, &u);
    int n = kind_choices(pi->kind), nv;
    if (n) nv = enc(CLAMP(choice_of(pi->kind, v) + d, 0, n - 1), n);
    else nv = CLAMP(v + d, kind_min(pi->kind), kind_max(pi->kind));
    if (code == SETTING(16)) {
        ui.proj_sel = (uint8_t)((nv * M2K_PROJECTS) >> 7);
        ui.load_t = 0;
    } else if (code == SETTING(17)) {                             /* LOAD a project: asked */
        char o[16] = {'P', (char)('1' + ui.proj_sel), ' ', 0};
        s_cpy(o + 3, m2k_project_name(ui.proj_sel));
        ask_open(ASK_PROJ, "LOAD", o);
    } else if (code == SETTING(18)) {                             /* SLOT: through the category shown */
        int n = 0, at = snd_list(ui.snd_sel, &n);
        at = CLAMP(at + (d > 0 ? 1 : -1), 0, n - 1);
        ui.snd_sel = (uint8_t)snd_nth(at);
        ui.snd_t = 0;
    } else if (code == SETTING(21)) {                             /* CAT: the category shown; the slot follows into it */
        ui.snd_cat = (uint8_t)CLAMP(ui.snd_cat + (d > 0 ? 1 : -1), 0, CAT_COUNT - 1);
        int n = 0, at = snd_list(ui.snd_sel, &n);
        if (!snd_shown(ui.snd_sel)) ui.snd_sel = (uint8_t)snd_nth(at < n ? at : 0);
        ui.snd_t = 0;
    } else if (code == SETTING(22)) {                             /* WIPE: every pattern INIT, asked */
        ask_open(ASK_WIPE, "WIPE", "EVERY PATTERN INIT");
    } else if (code == SETTING(23)) {                             /* DEMO: bank A's demos back, asked */
        ask_open(ASK_DEMO, "DEMO", "BANK A'S DEMOS BACK");
    } else if ((code == SETTING(19) || code == SETTING(20)) && ui.snd_t && ui.snd_arm == code && ui.snd_last == ui.snd_sel &&
               ui.now - ui.snd_t < 400) {
        ui.snd_t = ui.now | 1u;                                    /* the same turn's next detents: done already */
    } else if (code == SETTING(19)) {                             /* LOAD: the slot's sound on the track; INIT starts it afresh */
        sound_load(p);
        ui.snd_t = ui.now | 1u; ui.snd_arm = (uint8_t)code; ui.snd_last = ui.snd_sel;
    } else if (code == SETTING(20)) {                             /* SAVE: at once into INIT; over a sound, asked */
        ui.snd_t = ui.now | 1u; ui.snd_arm = (uint8_t)code; ui.snd_last = ui.snd_sel;
        if (snd[ui.snd_sel].name[0]) {
            char o[16] = {'S', 0};
            s_num3(o + 1, ui.snd_sel + 1);
            o[4] = ' ';
            s_cpy(o + 5, snd[ui.snd_sel].name);
            ask_open(ASK_SND_OVER, "REPLACE", o);
        } else {
            char o[16] = {'S', 0};
            s_num3(o + 1, ui.snd_sel + 1);
            sound_save(ui.snd_sel, p, ui.trk, ui.snd_cat, track_root(p, ui.trk));
            o[4] = ' ';
            s_cpy(o + 5, CAT_N[snd_cat[ui.snd_sel]]);
            toast("SOUND SAVED", o);
            dirty = 1;
        }
    } else if (code >= 0xC0) {
        m2k_setting(code & 0x3F, nv);             /* (kept with the store: saved once it stands) */
    } else if (code >= 0x80) {
        p->glob[code & 0x7F] = (uint8_t)nv;
    } else if (ui.grid && ui.nheld) {
        for (int i = 0; i < ui.nheld; i++) lock_set(ui.trk, ui.held[i], code, nv);
        trk[ui.trk].eff[code] = (uint8_t)nv;
        ui.held_edit = 1;
    } else if (sq.rec && sq.playing) {
        int s = track_pos(ui.trk);                                 /* the track's own step (off 1x, not the pattern's) */
        if (p->step[ui.trk][s].flags & ST_TRIG) lock_set(ui.trk, s, code, nv);
        else p->kit[ui.trk][code] = (uint8_t)nv;
        trk[ui.trk].eff[code] = (uint8_t)nv;
    } else {
        p->kit[ui.trk][code] = (uint8_t)nv;
        trk[ui.trk].eff[code] = (uint8_t)nv;
    }
    if (nv != v) {
        if (ui.focus != k || ui.now - ui.roll_t > 90) s_cpy(ui.roll_old, old);
        ui.roll_t = ui.now;
        if (ui.page == PG_SYN && code < 8 && (!ui.icon_hit || ui.now - ui.icon_hit > 120)) { ui.icon_hit = ui.now | 1u; ui.icon_amp = 8; }   /* the mark answers */
    }
    ui.focus = (uint8_t)k;
    ui.focus_t = ui.now | 1u;
}

static int kit_same(const uint8_t *a, const uint8_t *b)
{
    for (int k = 0; k < P_COUNT; k++) if (a[k] != b[k]) return 0;
    return 1;
}

/* machine m on track t (ALGORITHM on EDIT, the Patch Editor's): a sound still its machine's INIT (or the INIT before
 * these, kit_default's) takes m's whole INIT; one the player shaped takes m's page alone, its chain kept. Its new trigs
 * and keys go to m's home note */
static void machine_to(int t, int m)
{
    m2k_pattern_t *p = PAT(cur_pat);
    kit_init(&pat_tmp, t, p->machine[t]);
    int fresh = kit_same(p->kit[t], pat_tmp.kit[t]);
    kit_default(&pat_tmp, t, p->machine[t]);
    feel_kit_v15(pat_tmp.kit[t]);                  /* (an old pattern's, its amp and LFOs since turned to format 16) */
    fresh |= kit_same(p->kit[t], pat_tmp.kit[t]);
    kit_init(&pat_tmp, t, m);
    int n = fresh ? P_COUNT : 8;
    m_cpy(p->kit[t], pat_tmp.kit[t], (uint32_t)n);
    p->machine[t] = (uint8_t)m;
    m_cpy(trk[t].eff, p->kit[t], (uint32_t)n);
    track_home(t, mach_home(m));
}

static void set_machine(int m)
{
    machine_to(ui.trk, m);
    start_glitch(4);
    ui.card_t = ui.now | 1u;
    ui.focus_t = 0;
    syn_sub_check();
}

/* the keys let go of every note they hold: the keys' track changes, or the keys become steps or pads (a note left
 * in sq.held drones, and the sequence skips its track) */
static void keys_off(void) { for (int i = 0; i < 8 && sq.nkeys; i++) live_off(sq.keys[0]); }   /* (a HOLD keeps its) */

/* the track in view (SELECT, a black key in GRID or PADS, the web's cells): the keys play it */
static void track_to(int t)
{
    if (t == ui.trk) return;
    keys_off();
    sq.nheld = 0;                      /* (a held arp stays with its track: it stops, and HOLD waits for new keys) */
    ui.trk = (uint8_t)t;
    sq.ktrk = ui.trk;
    start_trans_dir(190, 1);
    syn_sub_check();
}

static void held_remove(int s)
{
    int j = 0;
    for (int i = 0; i < ui.nheld; i++)
        if (ui.held[i] != s) { ui.held[j] = ui.held[i]; ui.held_added[j] = ui.held_added[i]; j++; }
    ui.nheld = (uint8_t)j;
}

/* a page key in GRID: the page, or why it is empty; tapped again, the view follows the playhead */
static void page_key(int pg)
{
    char a[16], o[24], *e;
    e = s_cpy(a, "PAGE "); s_int(e, pg + 1);
    if (pg * 16 >= seq_len()) {
        e = s_cpy(o, "PAST THE END, HOLD: ");
        s_int(e, (pg + 1) * 16);
        toast(a, o);
        return;
    }
    if (pg == ui.spage && !ui.follow) { ui.follow = 1; toast(a, "FOLLOWS THE PLAY"); return; }
    ui.spage = (uint8_t)pg;
    ui.follow = 0;
    e = s_cpy(o, "STEPS "); e = s_int(e, pg * 16 + 1); e = s_cpy(e, "-"); s_int(e, pg * 16 + 16);
    toast(a, o);
}

/* a page key held: the pattern's length ends with that page; new steps take the pattern so far, so it goes on.
 * Returns 1 when the locks' pool could not take every lock the new steps copy (the message says so) */
static int page_length(int pg)
{
    m2k_pattern_t *p = PAT(cur_pat);
    int old = seq_len(), len = (pg + 1) * 16, lost = 0;
    if (len > old) {
        for (int t = 0; t < M2K_TRACKS; t++)
            for (int s = old; s < len; s++) { p->step[t][s] = p->step[t][s % old]; lock_clear_step(t, s); }
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t l = p->lock[i];
            if (l.trk >= M2K_TRACKS || l.step >= old) continue;
            for (int s = l.step + old; s < len; s += old) lost |= !lock_set(l.trk, s, l.param, l.val);
        }
    }
    p->glob[G_LEN] = (uint8_t)len;
    ui.spage = (uint8_t)pg;
    ui.follow = 0;
    char o[16], *e = s_cpy(o, "LENGTH ");
    s_int(e, len);
    toast(o, lost ? "LOCKS FULL: SOME LOST" : len > old ? "THE STEPS GO ON" : "STEPS PAST IT KEPT");
    return lost;
}

/* step entry: REC armed while stopped, the keys playing: each note is the next step */
/* a trig taken away: its condition (chance, A:B...), ratchet and locks go with it, so a trig put there later
 * starts plain (the owner: "when I removed the note or trigger, the probability should be removed too") */
static void step_pop(int t, int s) { ui.pop_t = ui.now | 1u; ui.pop_s = (uint8_t)s; ui.pop_trk = (uint8_t)t; }
static void step_remove(int t, int s)
{
    step_pop(t, s);
    m2k_step_t *st = &PAT(cur_pat)->step[t][s];
    st->flags = 0;
    st->cond = 0;
    st->rtrg = 0;
    lock_clear_step(t, s);
}
static int step_entry(void) { return sq.rec && !sq.playing && !ui.grid && !sq.ready; }

static void entry_show(const char *what)
{
    char o[16], *e = s_cpy(o, "STEP ");
    s_int(e, ui.cursor + 1);
    toast(o, what);
    ui.spage = (uint8_t)(ui.cursor / 16);
    ui.follow = 0;
}

static void entry_write(int note)
{
    m2k_step_t *st = &PAT(cur_pat)->step[ui.trk][ui.cursor];
    step_pop(ui.trk, ui.cursor);
    st->flags = ST_TRIG;
    st->note = (uint8_t)note;
    st->cond = C_ALWAYS;
    st->rtrg = 0;
    lock_clear_step(ui.trk, ui.cursor);
    char n[8];
    s_note(n, note);
    entry_show(n);
    ui.cursor = (uint8_t)((ui.cursor + 1) % seq_len());
}

static void select_pattern(int n);

/* the SEQ layer's tools, on the track in view over the pattern's length: its locks move with its steps */
static struct { m2k_step_t st[M2K_STEPS]; uint16_t nlk; uint8_t ok; m2k_lock_t lk[M2K_LOCKS]; } seq_clip;   /* (every lock: 384) */
static const char *const SEQ_TOOL[8] = {"CLEAR", "REVERSE", "SHIFT <", "SHIFT >", "RANDOM", "LEN X2", "COOK", "HALF"};
static void tool_undo(int i);                       /* (below: the undo's copy, a run of one tool keeping the first) */

static void seq_tool(int i)
{
    tool_undo(i);
    m2k_pattern_t *p = PAT(cur_pat);
    int t = ui.trk, len = seq_len();
    m2k_step_t *st = p->step[t];
    char o[24] = {'T', (char)('1' + t), ' ', 0};
    switch (i) {
    case 0:                                            /* CLEAR: its steps and locks */
        for (int s = 0; s < M2K_STEPS; s++) { st[s].flags = 0; st[s].cond = 0; st[s].rtrg = 0; }
        for (int k = 0; k < M2K_LOCKS; k++) if (p->lock[k].trk == t) p->lock[k].trk = 0xFF;
        break;
    case 1:                                            /* REVERSE */
        for (int s = 0; s < len / 2; s++) { m2k_step_t x = st[s]; st[s] = st[len - 1 - s]; st[len - 1 - s] = x; }
        for (int k = 0; k < M2K_LOCKS; k++) if (p->lock[k].trk == t && p->lock[k].step < len) p->lock[k].step = (uint8_t)(len - 1 - p->lock[k].step);
        break;
    case 2: case 3: {                                  /* SHIFT: a step earlier or later, round the length */
        int d = i == 2 ? len - 1 : 1;
        m2k_step_t tmp[M2K_STEPS];
        for (int s = 0; s < len; s++) tmp[(s + d) % len] = st[s];
        for (int s = 0; s < len; s++) st[s] = tmp[s];
        for (int k = 0; k < M2K_LOCKS; k++) if (p->lock[k].trk == t && p->lock[k].step < len) p->lock[k].step = (uint8_t)((p->lock[k].step + d) % len);
        break;
    }
    case 4:                                            /* RANDOM: a quarter of the steps, at the track's note */
        for (int k = 0; k < M2K_LOCKS; k++) if (p->lock[k].trk == t) p->lock[k].trk = 0xFF;
        for (int s = 0; s < M2K_STEPS; s++) {
            st[s].flags = (uint8_t)(s < len && (rnd32() & 3) == 0 ? ST_TRIG : 0);
            st[s].note = ui.note[t]; st[s].cond = 0; st[s].rtrg = 0;
        }
        break;
    case 5:                                            /* LEN X2: the pattern twice as long, the new bars a copy */
        if (len > 32 || len % 16) { toast("LEN X2", "TO 64 STEPS AT MOST"); return; }
        if (!page_length(len * 2 / 16 - 1)) {        /* (else its message: the locks full) */
            s_int(s_cpy(o, "LENGTH "), len * 2);
            toast("SEQ", o);
        }
        start_glitch(2);
        return;
    case 6: {                                          /* COOK: a few of its notes moved a fourth, fifth, tone or third */
        static const int8_t D[8] = {-7, -5, -3, -2, 2, 3, 5, 7};
        for (int s = 0; s < len; s++)
            if ((st[s].flags & ST_TRIG) && (rnd32() & 3) == 0) st[s].note = (uint8_t)CLAMP(st[s].note + D[rnd32() & 7], 12, 120);
        break;
    }
    case 7:                                            /* HALF: the pattern half as long (the steps after it kept) */
        if (len < 2) return;
        p->glob[G_LEN] = (uint8_t)(len / 2);
        if (ui.spage * 16 >= len / 2) ui.spage = 0;
        s_int(s_cpy(o, "LENGTH "), len / 2);
        toast("SEQ", o);
        return;
    }
    lockmap_build();
    s_cpy(o + 3, SEQ_TOOL[i]);
    toast("SEQ", o);
}

/* a track muted or heard again (F#5 in GRID and PADS, the web's cells), and said so: at the next bar, when it waits */
static void mute_flip(int t)
{
    mute_to(t, !mute_will(t));
    char o[4] = {'T', (char)('1' + t), 0, 0};
    if ((sq.mq_m >> t) & 1) toast(mute_will(t) ? "MUTE AT THE BAR" : "UNMUTE AT THE BAR", o);
    else toast(mute_will(t) ? "MUTED" : "UNMUTED", o);
}

/* a key's press, for its release (ui.kact): its note, a solo, a PERFORM effect, a page key, a step held */
enum { KA_NONE, KA_NOTE, KA_SOLO, KA_PERF, KA_PKEY, KA_STEP };

/* a key let go: what its press did is undone, whatever layer is held by now (a note left on drones and silences its
 * track's sequence, a solo stays, a step stays held, a page key fires its length) */
static void key_release(int k)
{
    int a = ui.kact[k], arg = ui.karg[k];
    ui.kact[k] = KA_NONE;
    switch (a) {
    case KA_NOTE:                                  /* (unless SELECT, GRID or PADS let go of it already) */
        for (int i = 0; i < sq.nkeys; i++) if (sq.keys[i] == arg) { live_off(arg); break; }
        break;
    case KA_SOLO: solo_key(arg, 0); break;
    case KA_PERF: if (perf.on[arg]) bus_perform(arg, 0); break;   /* (FX let go first has stopped it) */
    case KA_PKEY: if (ui.pkey == arg + 1) ui.pkey = 0; break;
    case KA_STEP: {                                /* a quick tap on a trig removes it; held longer, it was looked at */
        int added = 1, was = 0;
        for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == arg) { added = ui.held_added[i]; was = 1; }
        held_remove(arg);
        if (was && !added && !ui.held_edit && ui.now - ui.kt0[k] < 300) step_remove(ui.trk, arg);
        break;
    }
    default: break;
    }
}

static void key_event(int k, int down)
{
    ui.keys[k] = (uint8_t)down;
    ui.card_used = 1;
    m2k_pattern_t *p = PAT(cur_pat);
    int w = ui.kw[k], b = ui.kb[k];
    if (!down) {
        if (held(B_GLO) && w >= 0) ui.glo_used = 1;           /* (the layers count a release as used, as before) */
        else if (perf_layer() && b >= 0) ui.lay_used[0] = 1;
        else if (seq_layer()) ui.seq_used = 1;
        key_release(k);
        return;
    }
    ui.kact[k] = KA_NONE;
    ui.kt0[k] = ui.now;
    if (held(B_GLO) && w >= 0) {       /* GLO + white: 1-6 mute, 7 fill next loop, 8 tap tempo, 9-14 solo, 15 unmute */
        ui.glo_used = 1;
        if (w >= 8 && w < 8 + M2K_TRACKS) {         /* solo while held, several together (with BAR, from the bar) */
            solo_key(w - 8, 1);
            ui.kact[k] = KA_SOLO; ui.karg[k] = (uint8_t)(w - 8);
            return;
        }
        if (w < M2K_TRACKS) mute_to(w, !mute_will(w));   /* (with BAR, while playing: at the next bar) */
        else if (w == 6) { sq.fill_next = 1; toast("FILL", "THE NEXT LOOP"); }
        else if (w == 7) {                           /* tap tempo: two taps or more, a second or two apart at most */
            if (ui.ntap && ui.now - ui.tap_t[(ui.ntap - 1) & 3] > 2000) ui.ntap = 0;
            ui.tap_t[ui.ntap & 3] = ui.now;
            ui.ntap++;
            if (ui.ntap >= 2) {
                int n = ui.ntap > 4 ? 3 : ui.ntap - 1;
                uint32_t span = ui.now - ui.tap_t[(ui.ntap - 1 - n) & 3];
                int bpm = span ? (int)(60000u * (uint32_t)n / span) : 120;
                p->glob[G_TMPO] = (uint8_t)CLAMP(bpm - 30, 0, 255);
                char o[12], *e = s_int(o, bpm);
                s_cpy(e, " BPM");
                toast("TAP", o);
            } else toast("TAP", "AGAIN, ON THE BEAT");
        } else if (w == 14) { mutes_all_on(); toast(mute_pending() ? "UNMUTE ALL" : "UNMUTED", mute_pending() ? "AT THE NEXT BAR" : "EVERY TRACK"); }
        return;
    }
    if (perf_layer() && b >= 0) {      /* FX held: the black keys are PERFORM's effects, while held */
        int e = b + ui.pfb * PF_BANK;  /* the bank shown; its release lets go of this one, whichever is shown then */
        if (b < PF_BANK && bus_perform(e, 1)) { ui.kact[k] = KA_PERF; ui.karg[k] = (uint8_t)e; }
        else if (b < PF_BANK) toast(PF_NAME[e], "NO DELAY FREE");
        ui.lay_used[0] = 1;
        return;
    }
    if (seq_layer()) {                 /* SEQ held: the white keys 1-8 are tools for the track's steps */
        if (w >= 0 && w < 8) seq_tool(w);
        ui.seq_used = 1;
        return;
    }
    if (held(B_HOME) && !held(B_GLO)) {       /* HOME held: the white keys the bank's 16 patterns, the black 1-4 the bank */
        ui.hold_used = 1;
        if (b >= 0 && b < M2K_BANKS) {
            ui.pbank = (uint8_t)b;
            char a[8] = {'B', 'A', 'N', 'K', ' ', (char)('A' + b), 0};
            toast(a, "WHITE KEYS PICK ITS 16");
            return;
        }
        if (w >= 0 && ui.pbank * 16 + w < M2K_PATTERNS) {
            int n = ui.pbank * 16 + w;
            if (sq.playing && sq.queued == n && n != cur_pat) {   /* again before the bar: now */
                sq.jump = 1;
                char a[16], *e = s_pat(a, n);
                s_cpy(e, " NOW");
                toast(a, 0);
                return;
            }
            ui.pick[n >> 3] |= (uint8_t)(1u << (n & 7));
            select_pattern(n);
        }
        return;
    }
    if (ui.pads && w >= 0) {           /* PADS: white 1-6 and 9-14 hit T1-T6, each at its root */
        int t = w < M2K_TRACKS ? w : w >= 8 && w < 8 + M2K_TRACKS ? w - 8 : -1;
        if (t >= 0) {
            pad_hit(t, track_root(p, t));
            ui.flash_t[t] = ui.now;
        }
        return;
    }
    if (ui.pads) {                     /* the black keys as in GRID: T1-T6 pick the track (the keys' too), F#5 mutes it */
        if (b < 0) return;
        if (b < M2K_TRACKS) track_to(b);
        else if (b == 10) mute_flip(ui.trk);
        return;
    }
    if (!ui.grid) {
        int note = 53 + k + 12 * ui.oct[ui.trk];   /* (its release lets go of this note, whatever the octave is by then) */
        sq.ktrk = ui.trk;
        live_on(note, 100);
        ui.kact[k] = KA_NOTE; ui.karg[k] = (uint8_t)note;
        ui.note[ui.trk] = (uint8_t)note;
        if (step_entry()) entry_write(note);
        return;
    }
    if (b >= 0 && ui.nheld) {         /* a step held: the black keys set its trig type (1-5) and its condition (6-11) */
        static const uint8_t QT[5] = {0, 1, 2, 3, 7};                       /* ALL AMP FILTER LFO NONE */
        static const uint8_t QC[6] = {C_ALWAYS, C_P50, C_1_2, C_1_4, C_FILL, C_NFILL};
        for (int i = 0; i < ui.nheld; i++) {
            m2k_step_t *q = &p->step[ui.trk][ui.held[i]];
            if (b < 5) q->flags = (uint8_t)((q->flags & ~ST_TTYPE) | TTYPE[QT[b]]);
            else if (b < 11) q->cond = QC[b - 5];
        }
        ui.held_edit = 1;
        if (b < 5) toast("TRIG", TTYPE_N[QT[b]]);
        else toast("CONDITION", COND_N[QC[b - 5]]);
        return;
    }
    if (b >= 0) {
        if (b >= 6 && b < 10) {        /* P1..P4: a tap shows the page, a hold makes the pattern end there */
            page_key(b - 6);
            ui.pkey = (uint8_t)(b - 5); ui.pkey_k = (uint8_t)k; ui.pkey_t0 = ui.now | 1u; ui.pfired = 0;
            ui.kact[k] = KA_PKEY; ui.karg[k] = (uint8_t)(b - 6);
            return;
        }
        if (b < M2K_TRACKS) track_to(b);
        else mute_flip(ui.trk);
        return;
    }
    int s = ui.spage * 16 + w;
    if (ui.nheld >= 8 || s >= seq_len()) return;      /* (past the end: no trig placed there unseen) */
    m2k_step_t *st = &p->step[ui.trk][s];
    int added = 0;
    if (!(st->flags & ST_TRIG)) {
        st->flags = ST_TRIG;
        st->note = ui.note[ui.trk];
        added = 1;
        step_pop(ui.trk, s);
    }
    if (!ui.nheld) ui.held_edit = 0;
    ui.held[ui.nheld] = (uint8_t)s;
    ui.held_added[ui.nheld] = (uint8_t)added;
    ui.nheld++;
    ui.focus_t = 0;
    ui.kact[k] = KA_STEP; ui.karg[k] = (uint8_t)s;
}

/* the splash, while it runs: a key, a button or a knob (not the master, a pot that drifts) ends it; the press is the
 * splash's alone and so is its release, so nothing plays or opens under it */
static int splash_on(void) { return ui.boot_t0 != 0xFFFFFFFFu && ui.now - ui.boot_t0 < ui.splash_end; }
static void splash_skip(void) { ui.splash_end = ui.now - ui.boot_t0; }

void m2k_key(int k, int down)
{
    if (k < 0 || k >= M2K_KEYS) return;
    if (down && splash_on()) { splash_skip(); ui.skip_keys |= 1u << k; return; }
    if (!down && ((ui.skip_keys >> k) & 1u)) { ui.skip_keys &= ~(1u << k); return; }
    syn_sub_check();
    key_event(k, down);
    syn_sub_check();
}

static void held_step_edit(int dcond, int drtrg)
{
    m2k_pattern_t *p = PAT(cur_pat);
    for (int i = 0; i < ui.nheld; i++) {
        m2k_step_t *st = &p->step[ui.trk][ui.held[i]];
        if (dcond) st->cond = COND_ORDER[CLAMP(cond_place(st->cond) + dcond, 0, C_COUNT - 1)];   /* (by the knob's order) */
        if (drtrg) st->rtrg = (uint8_t)CLAMP((st->rtrg ? st->rtrg : 1) + drtrg, 1, 4);
    }
    ui.held_edit = 1;
    ui.roll_t = ui.now;
}

static void held_note(int d)
{
    m2k_pattern_t *p = PAT(cur_pat);
    for (int i = 0; i < ui.nheld; i++) {
        m2k_step_t *st = &p->step[ui.trk][ui.held[i]];
        st->note = (uint8_t)CLAMP(st->note + d, 12, 120);
        ui.note[ui.trk] = st->note;
    }
    ui.held_edit = 1;
    /* audition */
    m_cpy(trk[ui.trk].eff, p->kit[ui.trk], P_COUNT);
    if (ui.nheld) { apply_locks(ui.trk, ui.held[0]); voice_on(ui.trk, p->step[ui.trk][ui.held[0]].note, 100, 0); trk[ui.trk].gate_left = gate_blocks(ui.trk); }
}

/* ---- the verbs. REC copies, PLAY pastes, SEQ clears, always (the Monomachine's grammar); what they act on is what is
 * held: GLO, the track's steps in GRID or the open pattern; HOME, the patterns picked; EDIT, the track's sound; SEQ,
 * the track's steps; a step held in GRID, that step. Repeating a paste or a clear within 3 s undoes it; holding the
 * clear a second makes it INIT (a ring fills). GLO + OCT- undoes, GLO + OCT+ redoes */
static m2k_pattern_t clip_pat POOL;                 /* the clipboards: a pattern, a sound, a track's steps, steps */
static struct { uint8_t kind, machine, kit[P_COUNT]; } clipb;
static struct { uint8_t n, ok, off[8]; m2k_step_t st[8]; uint16_t nlk; m2k_lock_t lk[M2K_LOCKS]; } step_clip;

/* undo: one level, the open pattern as it was before the last undoable action; a run of the same keeps the first.
 * The same is the same verb on the same thing: the pattern, and the track (a track's steps, its sound) and the first
 * step held (steps) it acted on; again on another track or step is a new action, not an undo */
static m2k_pattern_t undo_pat;                    /* (in RAM: the FM-1's pool is full with the second core's stacks) */
static struct { uint8_t ok, redo, which, noun, verb, trk, step; uint32_t t; } undo;

static int held_first(void)                         /* the first of the steps held in GRID, 0xFF none */
{
    int f = 0xFF;
    for (int i = 0; ui.grid && i < ui.nheld; i++) if (ui.held[i] < f) f = ui.held[i];
    return f;
}

static int undo_repeat(int noun, int verb)
{
    return undo.ok && !undo.redo && undo.which == cur_pat && undo.noun == noun && undo.verb == verb && undo.t &&
           ui.now - undo.t < 3000u && (noun == N_PAT || noun == N_PICK || undo.trk == ui.trk) &&
           (noun != N_STEP || undo.step == held_first());
}

static void undo_mark(int noun, int verb)
{
    if (undo_repeat(noun, verb) && verb != V_PASTE && verb != V_CLEAR) { undo.t = ui.now; return; }
    m_cpy(&undo_pat, PAT(cur_pat), sizeof undo_pat);
    undo.ok = 1; undo.redo = 0; undo.which = (uint8_t)cur_pat; undo.noun = (uint8_t)noun; undo.verb = (uint8_t)verb;
    undo.trk = ui.trk; undo.step = (uint8_t)held_first();
    undo.t = ui.now | 1u;
}
static void undo_forget(void) { undo.ok = 0; }     /* another project, or the store loaded: nothing of it to undo */

static void tool_undo(int i) { undo_mark(N_TRK, 10 + i); }
static int first_pick(void) { for (int i = 0; i < M2K_PATTERNS; i++) if (ui.pick[i >> 3] >> (i & 7) & 1) return i; return cur_pat; }

static int undo_swap(void)                          /* UNDO and REDO alike: the open pattern and the copy change places */
{
    if (!undo.ok || undo.which != cur_pat) return 0;
    m_cpy(&pat_tmp, PAT(cur_pat), sizeof pat_tmp);
    m_cpy(PAT(cur_pat), &undo_pat, sizeof undo_pat);
    m_cpy(&undo_pat, &pat_tmp, sizeof undo_pat);
    undo.redo ^= 1;
    undo.t = 0;
    work_reset_current();
    return 1;
}

static void undo_do(int redo)
{
    if (redo ? undo.ok && undo.redo && undo_swap() : undo.ok && !undo.redo && undo_swap()) {
        char a[8];
        s_pat(a, cur_pat);
        toast(redo ? "REDO" : "UNDO", a);
        start_glitch(2);
    } else toast(redo ? "NOTHING TO REDO" : "NOTHING TO UNDO", 0);
}

/* a clear held on: after a second it becomes INIT (ring_noun's), unless the buttons were let go, or the pattern (or
 * the track, for its steps) is no longer the one cleared (a queued pattern opened meanwhile) */
static struct { uint32_t t0; uint8_t noun, done, pn, trk; } ring;
static void ring_start(int noun) { ring.t0 = ui.now | 1u; ring.noun = (uint8_t)noun; ring.done = 0; ring.pn = (uint8_t)cur_pat; ring.trk = ui.trk; }

static void steps_clear(m2k_pattern_t *q, int t)    /* t < 0: every track's */
{
    for (int k = 0; k < M2K_TRACKS; k++) {
        if (t >= 0 && k != t) continue;
        for (int st = 0; st < M2K_STEPS; st++) { q->step[k][st].flags = 0; q->step[k][st].cond = 0; q->step[k][st].rtrg = 0; }
    }
    for (int k = 0; k < M2K_LOCKS; k++) if (t < 0 || q->lock[k].trk == t) q->lock[k].trk = 0xFF;
}

static void track_copy(void)
{
    m2k_pattern_t *p = PAT(cur_pat);
    m_cpy(seq_clip.st, p->step[ui.trk], sizeof seq_clip.st);
    seq_clip.nlk = 0;
    for (int k = 0; k < M2K_LOCKS; k++) if (p->lock[k].trk == ui.trk) seq_clip.lk[seq_clip.nlk++] = p->lock[k];
    seq_clip.ok = 1;
}

/* 0 nothing to paste, 1 pasted, 2 pasted but the locks' pool (shared by the tracks) took only some of its locks */
static int track_paste(void)
{
    m2k_pattern_t *p = PAT(cur_pat);
    if (!seq_clip.ok) return 0;
    m_cpy(p->step[ui.trk], seq_clip.st, sizeof seq_clip.st);
    for (int k = 0; k < M2K_LOCKS; k++) if (p->lock[k].trk == ui.trk) p->lock[k].trk = 0xFF;
    int lost = 0;
    for (int k = 0; k < seq_clip.nlk; k++) lost |= !lock_set(ui.trk, seq_clip.lk[k].step, seq_clip.lk[k].param, seq_clip.lk[k].val);
    return 1 + lost;
}

static int picked(int i) { return ui.pick[i >> 3] >> (i & 7) & 1; }
static int pick_count(void) { int n = 0; for (int i = 0; i < M2K_PATTERNS; i++) n += picked(i); return n; }
static int pick_target(int i) { return pick_count() ? picked(i) : i == cur_pat; }

static void verb(int noun, int v)
{
    m2k_pattern_t *p = PAT(cur_pat);
    char a[24], o[28], t2[4] = {'T', (char)('1' + ui.trk), 0, 0};
    s_pat(a, cur_pat);
    const char *undo_line = "GLO+OCT- UNDO";
    if ((v == V_PASTE || v == V_CLEAR) && noun != N_PICK && undo_repeat(noun, v)) {   /* again: undone */
        undo_do(0);
        ring.t0 = 0;
        return;
    }
    switch (noun) {
    case N_PAT:
        if (v == V_COPY) { m_cpy(&clip_pat, p, sizeof clip_pat); clipb.kind = 1; toast("COPY", a); break; }
        if (v == V_PASTE) {
            if (clipb.kind != 1) { toast("NOTHING", "COPY A PATTERN FIRST"); break; }
            undo_mark(noun, v);
            m_cpy(p, &clip_pat, sizeof *p);
            work_reset_current();
            s_cpy(s_cpy(o, "PASTE "), a);
            toast(o, undo_line);
            start_glitch(2);
            break;
        }
        undo_mark(noun, v);
        steps_clear(p, -1);
        s_cpy(s_cpy(o, a), " CLEARED");
        toast(o, "HOLD: INIT. AGAIN: UNDO");
        ring_start(N_PAT);
        break;
    case N_TRK:
        if (v == V_COPY) { track_copy(); s_cpy(s_cpy(o, t2), " STEPS"); toast("COPY", o); break; }
        if (v == V_PASTE) {
            if (!seq_clip.ok) { toast("NOTHING", "SEQ + REC COPIES ONE"); break; }
            undo_mark(noun, v);
            int lost = track_paste() == 2;
            s_cpy(s_cpy(o, "PASTE "), t2);
            toast(lost ? "LOCKS FULL" : o, lost ? "PASTED, SOME LOCKS LOST" : undo_line);
            break;
        }
        undo_mark(noun, v);
        steps_clear(p, ui.trk);
        s_cpy(s_cpy(o, t2), " STEPS CLEARED");
        toast(o, "HOLD: SOUND INIT TOO");
        ring_start(N_TRK);
        break;
    case N_PICK: {
        int n = pick_count() ? pick_count() : 1, one = n == 1 && pick_target(cur_pat);
        if (v == V_COPY) { m_cpy(&clip_pat, p, sizeof clip_pat); clipb.kind = 1; toast("COPY", a); break; }
        if (one && (v == V_PASTE || v == V_CLEAR) && undo_repeat(noun, v)) { undo_do(0); ring.t0 = 0; break; }
        if (v == V_PASTE && clipb.kind != 1) { toast("NOTHING", "HOME + REC COPIES ONE"); break; }
        if (one) undo_mark(noun, v);
        int size = v == V_PASTE ? pat_pack_img(&clip_pat) : 0, done = 0, full = 0;
        for (int i = 0; i < M2K_PATTERNS; i++) {
            if (!pick_target(i)) continue;
            if (v == V_PASTE) {
                if (i != cur_pat && !pat_room(size)) { full = 1; break; }
                m_cpy(PAT(i), &clip_pat, sizeof clip_pat);
            } else steps_clear(PAT(i), -1);
            done++;
        }
        work_reset_current();
        char *e = done == 1 ? s_pat(o, pick_count() ? first_pick() : cur_pat) : s_int(o, done);
        s_cpy(e, v == V_PASTE ? (done == 1 ? " PASTED" : " PASTED INTO") : done == 1 ? " CLEARED" : " PATTERNS CLEARED");
        toast(full ? "PATTERNS FULL" : o, full ? o : v == V_CLEAR ? "HOLD: INIT" : one ? undo_line : "NO UNDO FOR SEVERAL");
        if (v == V_CLEAR) ring_start(N_PICK);
        start_glitch(2);
        break;
    }
    case N_SND:
        if (v == V_COPY) {
            clipb.kind = 2;
            clipb.machine = p->machine[ui.trk];
            m_cpy(clipb.kit, p->kit[ui.trk], P_COUNT);
            s_cpy(s_cpy(o, t2), " SOUND");
            toast("COPY", o);
            break;
        }
        if (v == V_PASTE && clipb.kind != 2) { toast("NOTHING", "EDIT + REC COPIES ONE"); break; }
        undo_mark(noun, v);
        if (v == V_PASTE) { p->machine[ui.trk] = clipb.machine; m_cpy(p->kit[ui.trk], clipb.kit, P_COUNT); }
        else {                                         /* a sound cleared is INIT: its machine as it starts, at home */
            kit_init(p, ui.trk, p->machine[ui.trk]);
            track_home(ui.trk, mach_home(p->machine[ui.trk]));
        }
        m_cpy(trk[ui.trk].eff, p->kit[ui.trk], P_COUNT);
        s_cpy(s_cpy(o, t2), v == V_PASTE ? " SOUND PASTED" : " SOUND INIT");
        toast(o, undo_line);
        ui.card_t = ui.now | 1u;
        start_glitch(2);
        break;
    case N_STEP: {                                     /* the steps held: copy them, paste them, clear their locks */
        if (v == V_COPY) {
            step_clip.n = 0; step_clip.nlk = 0;
            int first = ui.held[0];
            for (int i = 0; i < ui.nheld; i++) if (ui.held[i] < first) first = ui.held[i];
            for (int i = 0; i < ui.nheld && i < 8; i++) {
                int s = ui.held[i];
                step_clip.off[step_clip.n] = (uint8_t)(s - first);
                step_clip.st[step_clip.n++] = p->step[ui.trk][s];
                for (int k = 0; k < M2K_LOCKS && step_clip.nlk < M2K_LOCKS; k++)
                    if (p->lock[k].trk == ui.trk && p->lock[k].step == s) { step_clip.lk[step_clip.nlk] = p->lock[k]; step_clip.lk[step_clip.nlk++].step = (uint8_t)(s - first); }
            }
            step_clip.ok = 1;
            ui.held_edit = 1;
            s_int(s_cpy(o, ""), step_clip.n);
            s_cpy(o + (step_clip.n > 9 ? 2 : 1), step_clip.n == 1 ? " STEP" : " STEPS");
            toast("COPY", o);
            break;
        }
        undo_mark(noun, v);
        ui.held_edit = 1;
        if (v == V_CLEAR) {
            for (int i = 0; i < ui.nheld; i++) lock_clear_step(ui.trk, ui.held[i]);
            toast("LOCKS CLEARED", undo_line);
            break;
        }
        if (!step_clip.ok) { toast("NOTHING", "A STEP + REC COPIES IT"); break; }
        int first = ui.held[0];
        for (int i = 0; i < ui.nheld; i++) if (ui.held[i] < first) first = ui.held[i];
        int len = seq_len(), lost = 0;
        for (int h = 0; h < (step_clip.n == 1 ? ui.nheld : 1); h++) {   /* one step onto each held; several from the first */
            int base = step_clip.n == 1 ? ui.held[h] : first;
            for (int i = 0; i < step_clip.n; i++) {
                int s = (base + step_clip.off[i]) % len;
                p->step[ui.trk][s] = step_clip.st[i];
                lock_clear_step(ui.trk, s);
            }
            for (int k = 0; k < step_clip.nlk; k++)
                lost |= !lock_set(ui.trk, (base + step_clip.lk[k].step) % len, step_clip.lk[k].param, step_clip.lk[k].val);
        }
        toast(lost ? "LOCKS FULL" : "PASTE", lost ? "PASTED, SOME LOCKS LOST" : undo_line);
        break;
    }
    }
    lockmap_build();
}

/* GRIDS + OCT+, stopped: the lanes written into the steps of the tracks that follow them, as many as the pattern's
 * length, as they play from its start (the chaos of a PLAY's first cycles), at the track's note (lane_note), the
 * accents its accents; their locks there cleared and their following off, so the steps can be edited and locked */
static int grids_followers(const m2k_pattern_t *p)
{
    int m = 0;
    for (int t = 0; t < M2K_TRACKS; t++) if (lane_of(p, t)) m |= 1 << t;
    return m;
}

static void grids_write(m2k_pattern_t *p)
{
    const int len = CLAMP(p->glob[G_LEN], 1, 64);
    for (int t = 0; t < M2K_TRACKS; t++) {
        int lane = lane_of(p, t);
        if (!lane) continue;
        int note = lane_note(p, t);
        for (int st = 0; st < len; st++) {
            int hit = grids_hit(p, lane - 1, (uint32_t)st, 0);
            p->step[t][st] = (m2k_step_t){(uint8_t)note, (uint8_t)(hit ? ST_TRIG | (hit == 2 ? ST_ACC : 0) : 0), C_ALWAYS, 0};
        }
        for (int i = 0; i < M2K_LOCKS; i++) if (p->lock[i].trk == t && p->lock[i].step < len) p->lock[i].trk = 0xFF;
        p->glob[G_LANE1 + t] = 0;
    }
    lockmap_build();
}

static char *s_tracks(char *e, int m)                /* " T2 T4" */
{
    for (int t = 0; t < M2K_TRACKS; t++) if (m >> t & 1) { *e++ = ' '; *e++ = 'T'; *e++ = (char)('1' + t); }
    *e = 0;
    return e;
}

static void grids_written(int m)
{
    char o[28];
    s_cpy(s_tracks(o, m), ": STEPS");
    toast("LANES WRITTEN", o + 1);
    start_glitch(3);
}

static void grids_snapshot(void)
{
    m2k_pattern_t *p = PAT(cur_pat);
    int m = grids_followers(p), over = 0, len = seq_len();
    if (!m) { toast("NO LANE FOLLOWED", "LANES: A TRACK'S LANE"); return; }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int st = 0; st < len && (m >> t & 1); st++) if (p->step[t][st].flags & ST_TRIG) over |= 1 << t;
    if (over) {                        /* over trigs: asked */
        char o[28];
        s_cpy(s_tracks(s_cpy(o, "OVER"), over), " TRIGS");
        ask_open(ASK_GRIDS, "WRITE STEPS", o);
        return;
    }
    undo_mark(N_PAT, 22);
    grids_write(p);
    grids_written(m);
}

/* the ring's second: INIT what was cleared */
static void ring_tick(void)
{
    if (!ring.t0 || ring.done) return;
    int still = ui.btn[B_SEQ] && (ring.noun == N_PICK ? ui.btn[B_HOME] : ui.btn[B_GLO]);
    if (!still || cur_pat != ring.pn || (ring.noun == N_TRK && ui.trk != ring.trk)) { ring.t0 = 0; return; }
    if (ui.now - (ring.t0 & ~1u) < 1000) return;
    ring.done = 1;
    m2k_pattern_t *p = PAT(cur_pat);
    char o[28];
    int home = 1;                                      /* the open pattern INIT: its tracks' keys at their homes */
    if (ring.noun == N_PAT) {
        pattern_init_blank(p);
        s_cpy(s_pat(o, cur_pat), " INIT");
        toast(o, "BLANK. GLO+OCT- UNDO");
    } else if (ring.noun == N_TRK) {
        kit_init(p, ui.trk, p->machine[ui.trk]);
        track_home(ui.trk, mach_home(p->machine[ui.trk]));
        home = 0;
        char t2[24] = {'T', (char)('1' + ui.trk), 0};
        s_cpy(t2 + 2, " STEPS AND SOUND INIT");
        toast(t2, "GLO+OCT- UNDO");
    } else {
        int n = 0;
        home = pick_target(cur_pat);
        for (int i = 0; i < M2K_PATTERNS; i++) if (pick_target(i)) { pattern_init_blank(PAT(i)); n++; }
        s_cpy(s_int(o, n), n == 1 ? " PATTERN INIT" : " PATTERNS INIT");
        toast(o, "BLANK, AS INIT BANKS");
    }
    work_reset_current();
    if (home) keys_home();
    start_glitch(4);
}


static void ask_answer(int yes)
{
    int k = ask.kind;
    ask.kind = ASK_NONE;
    if (!yes) { toast("NO", "NOTHING CHANGED"); return; }
    char o[24];
    switch (k) {
    case ASK_SND_OVER:                                 /* what was asked: that slot, that track's sound */
        sound_save(ask.slot, PAT(ask.pn), ask.trk, ask.cat, track_root(PAT(ask.pn), ask.trk));
        o[0] = 'S'; s_num3(o + 1, ask.slot + 1); o[4] = ' '; s_cpy(o + 5, CAT_N[snd_cat[ask.slot]]);
        toast("SOUND SAVED", o);
        dirty = 1;
        break;
    case ASK_WIPE:
        for (int i = 0; i < M2K_PATTERNS; i++) pattern_init_blank(PAT(i));
        work_reset_current();
        keys_home();
        toast("ALL PATTERNS INIT", "THE SOUND LIBRARY KEPT");
        start_glitch(5);
        dirty = 1;
        undo.ok = 0;
        break;
    case ASK_DEMO:
        factory();                                /* A01-A04, the slots with a demo (FACTORY_DEMOS) */
        work_reset_current();
        if (cur_pat < FACTORY_DEMOS) keys_home();
        toast("DEMOS BACK", "A01-A04, THE REST KEPT");
        start_glitch(5);
        dirty = 1;
        undo.ok = 0;
        break;
    case ASK_GRIDS:                                    /* the pattern asked about, its followers as they are */
        if (ask.pn == cur_pat) {
            int m = grids_followers(PAT(cur_pat));
            undo_mark(N_PAT, 22);
            grids_write(PAT(cur_pat));
            grids_written(m);
        } else toast("NO", "ANOTHER PATTERN NOW");
        break;
    case ASK_PROJ:
        project_load(ask.proj);
        o[0] = 'P'; o[1] = (char)('1' + ask.proj); o[2] = ' '; s_cpyn(o + 3, m2k_project_name(ask.proj), 9);
        toast("LOADED", o);
        start_glitch(4);
        dirty = 1;
        undo.ok = 0;
        break;
    default: break;
    }
}

/* a fresh start (m2k_init: the boot, the web's reset): no undo of the patterns before it, no question, ring or knobs
 * held for LFO left over */
static void ui_forget(void)
{
    undo.ok = 0;
    ask.kind = ASK_NONE;
    ring.t0 = 0;
    mom.n = 0; mom.keep = 0;
}

/* HOME held: + SAVE is nothing; EDIT held: + SAVE keeps the track's sound in the library's first INIT slot */
static void sound_keep(void)
{
    m2k_pattern_t *p = PAT(cur_pat);
    char o[24];
    int slot = -1;
    for (int i = 0; i < M2K_SOUNDS && slot < 0; i++) if (!snd[i].name[0]) slot = i;
    if (slot < 0) { toast("LIBRARY FULL", "SYN > SND: REPLACE ONE"); return; }
    sound_save(slot, p, ui.trk, 0, track_root(p, ui.trk));
    o[0] = 'S'; s_num3(o + 1, slot + 1); o[4] = ' '; s_cpy(o + 5, CAT_N[snd_cat[slot]]);
    toast("SOUND SAVED", o);
    ui.snd_sel = (uint8_t)slot;
    dirty = 1;
}

static const char *const BTN_N[B_COUNT] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                            "PLAY", "REC", "OCT-", "OCT+"};

/* double-tap GLO, SEQ, HOME or FX: its keys and knobs stay as if it were held, over the page the first tap left (the
 * button combos still want it held: PLAY, REC, SAVE and the octaves are themselves meanwhile); its button again lets
 * it go, and so does any other page button */
static void latch_off(void)
{
    if (!ui.latch) return;
    int b = ui.latch - 1;
    ui.latch = 0;
    if (b == B_FX) for (int k = 0; k < PF_COUNT; k++) if (perf.on[k]) bus_perform(k, 0);
    char a[12];
    s_cpy(s_cpy(a, BTN_N[b]), " LET GO");
    toast(a, 0);
}
/* a press of the four: 1 when the latch took it (opened, or let go), and it does nothing else */
static int latch_press(int b)
{
    if (ui.latch == b + 1) { latch_off(); return 1; }
    if (ui.dt_b == b + 1 && ui.now - ui.dt_t < 250) {   /* the second tap: the first undone, the layer kept */
        ui.dt_b = 0;
        ui.page = ui.dt_page;
        ui.sub = ui.dt_sub;
        if (ui.grid != ui.dt_grid || ui.pads != ui.dt_pads) keys_off();
        ui.grid = ui.dt_grid;
        ui.pads = ui.dt_pads;
        ui.nheld = 0;
        latch_off();
        ui.latch = (uint8_t)(b + 1);
        char a[16], o[20];
        s_cpy(s_cpy(a, BTN_N[b]), " LATCHED");
        s_cpy(s_cpy(s_cpy(o, "TAP "), BTN_N[b]), ": LET GO");
        toast(a, o);
        start_trans(150);
        return 1;
    }
    ui.dt_b = 0;
    ui.dt_page = ui.page;
    ui.dt_sub = ui.sub;
    ui.dt_grid = ui.grid;
    ui.dt_pads = ui.pads;
    return 0;
}
static void latch_tapped(int b, uint32_t hd) { if (hd < 250) { ui.dt_b = (uint8_t)(b + 1); ui.dt_t = ui.now; } }
static int latch_frees(int b)
{
    int l = ui.latch - 1;
    return ui.latch && b != l && b != B_SAVE && b != B_PLAY && b != B_REC && b != B_OCTD && b != B_OCTU;
}

/* SEL ENV ARP PLAY OCT- OCT+ held a second with nothing else touched: a card of what they do; the release then
 * does nothing */
static int card_of(int b) { return b == B_SEL || b == B_ENV || b == B_ARP || b == B_PLAY || b == B_OCTD || b == B_OCTU; }
static int card_up(void)
{
    return ui.card && !ui.card_used && ui.btn[ui.card - 1] && ui.now - ui.card_t0 >= 1000;
}
static void card_track(int b, int down)
{
    if (!down) return;
    int other = 0;
    for (int i = 0; i < B_COUNT; i++) if (i != b && ui.btn[i]) other = 1;
    for (int k = 0; k < M2K_KEYS; k++) if (ui.keys[k]) other = 1;
    ui.card_used = 1;
    if (!other && !ui.latch && !ask.kind && card_of(b)) { ui.card = (uint8_t)(b + 1); ui.card_t0 = ui.now; ui.card_used = 0; }
}

static void button_event(int b, int down)
{
    int was = ui.btn[b];
    if (!down && ui.card == b + 1) {               /* its card was read: nothing more */
        int shown = card_up();
        ui.card = 0;
        if (shown) { ui.btn[b] = 0; if (b == B_OCTD) ui.octd = 0; return; }
    }
    card_track(b, down);
    ui.btn[b] = (uint8_t)down;
    if (ask.kind && down) {                        /* a question open: OCT+ YES, OCT- NO, anything else NO */
        ask_answer(b == B_OCTU);
        ui.btn[b] = 0;
        if (b == B_OCTU || b == B_OCTD) return;
        ui.btn[b] = 1;
    }
    if (down && latch_frees(b)) latch_off();
    /* GLO held (FUNC): the verbs on the pattern (the track's steps in GRID); OCT- UNDO, OCT+ REDO; SAVE reload; EDIT
     * the sound library; HOME the keys' mode in turn (KEYS, PADS, GRID); ARP pads */
    if (b != B_GLO && ui.btn[B_GLO] &&          /* (the combos want GLO itself held, latched or not) */
        (b == B_REC || b == B_PLAY || b == B_SEQ || b == B_SAVE || b == B_EDIT || b == B_HOME || b == B_ARP ||
         b == B_OCTD || b == B_OCTU)) {
        if (!down) return;
        ui.glo_used = 1;
        int noun = ui.grid ? N_TRK : N_PAT;
        switch (b) {
        case B_REC: verb(noun, V_COPY); break;
        case B_PLAY: verb(noun, V_PASTE); break;
        case B_SEQ: verb(noun, V_CLEAR); break;
        case B_SAVE: undo_mark(N_PAT, 9); if (pattern_reload()) { char a[8]; s_pat(a, cur_pat); toast("RELOAD", a); } start_glitch(3); break;
        case B_EDIT: ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1); start_trans(200); break;
        case B_HOME:                       /* the keys' mode, a tap each: KEYS, PADS, GRID, KEYS (the owner's combo) */
            keys_off();
            ui.hold_t0 = 0;                /* (HOME's own release: no page, no latch) */
            if (ui.grid) ui.grid = 0;
            else if (ui.pads) {
                ui.pads = 0;
                ui.grid = 1;
                ui.nheld = 0;
                if (ui.spage * 16 >= seq_len()) ui.spage = 0;
            } else ui.pads = 1;
            if (ui.grid) toast("GRID: OCT-/+ PAGES", "HOLD G#4-D#5: LENGTH");
            else toast(ui.pads ? "PADS" : "KEYS", ui.pads ? "WHITE 1-6, 9-14: T1-T6" : "PLAY THE TRACK");
            start_trans(250);
            break;
        case B_ARP:
            keys_off();                    /* (the keys become pads, or keys again) */
            ui.pads = (uint8_t)!ui.pads;
            if (ui.pads) ui.grid = 0;
            toast(ui.pads ? "PADS" : "KEYS", ui.pads ? "WHITE 1-6, 9-14: T1-T6" : "PLAY THE TRACK");
            break;
        case B_OCTD: undo_do(0); ui.octd = 0; ui.octd_used = 1; break;   /* (its release, GLO let go first: nothing) */
        case B_OCTU: undo_do(1); break;
        }
        return;
    }
    /* HOME held: the verbs on the patterns picked (the white keys pick them, the black 1-4 the bank); EDIT held: on the
     * track's sound, SAVE keeps it in the library */
    if (!ui.btn[B_GLO] && (ui.btn[B_HOME] || ui.btn[B_EDIT]) && b != B_HOME && b != B_EDIT &&
        (b == B_SEQ || b == B_REC || b == B_PLAY || b == B_SAVE)) {
        if (!down) return;
        ui.hold_used = 1;
        int noun = ui.btn[B_HOME] ? N_PICK : N_SND;
        if (b == B_SAVE) { if (noun == N_SND) sound_keep(); return; }
        verb(noun, b == B_REC ? V_COPY : b == B_PLAY ? V_PASTE : V_CLEAR);
        return;
    }
    /* SEQ held: REC / PLAY copy and paste the track's steps */
    if (ui.btn[B_SEQ] && seq_layer() && b != B_SEQ && (b == B_REC || b == B_PLAY)) {
        if (!down) return;
        ui.seq_used = 1;
        verb(N_TRK, b == B_REC ? V_COPY : V_PASTE);
        return;
    }
    /* a step held in GRID: REC copies it, PLAY pastes onto it, SEQ clears its locks */
    if (ui.grid && ui.nheld && (b == B_REC || b == B_PLAY || b == B_SEQ)) {
        if (!down) return;
        verb(N_STEP, b == B_REC ? V_COPY : b == B_PLAY ? V_PASTE : V_CLEAR);
        return;
    }
    /* REC held: SEQ clears the track in view's steps, in KEYS as in GRID, its sound always kept (no INIT however long
     * it is held; again, undone): what was just recorded, gone to record again */
    if (b == B_SEQ && ui.btn[B_REC] && !ui.btn[B_GLO] && !ui.btn[B_HOME] && !ui.btn[B_EDIT]) {
        if (!down) return;
        ui.rec_used = 1;
        sq.fill = 0;
        ui.seq_t0 = 0;
        ui.seq_used = 1;
        verb(N_TRK, V_CLEAR);
        if (ring.t0) {                 /* a clear (not an undo): no ring, the sound stays */
            ring.t0 = 0;
            char o[24] = {'T', (char)('1' + ui.trk), 0};
            s_cpy(o + 2, " STEPS CLEARED");
            toast(o, "SOUND KEPT. AGAIN: UNDO");
        }
        return;
    }
    if ((b == B_OCTU || b == B_OCTD) && perf_layer()) {   /* PERFORM's banks: OCT- A, OCT+ B (held effects go on) */
        if (down) { ui.pfb = b == B_OCTU; ui.lay_used[0] = 1; }
        if (b == B_OCTD) { ui.octd = 0; ui.octd_used = 1; }   /* (its release, FX let go first: nothing) */
        return;
    }
    if (b == B_OCTU && down && mom_layer()) { mom.keep = 1; ui.lay_used[1] = 1; toast("KEEP", "LET GO OF LFO"); return; }
    if (b == B_OCTD) {                 /* OCT- on its release, unless a knob turned while it was held (that knob's home) */
        if (down) { ui.octd = 1; ui.octd_used = 0; return; }
        int used = ui.octd_used;
        ui.octd = 0;
        if (used || !was) return;
        down = 1;                      /* (the release does what the press did) */
    }
    if (b != B_GLO && !down && !was) return;
    if (b == B_FX || b == B_LFO) {    /* a tap: the page; held: PERFORM (FX), the momentary knobs (LFO) */
        int i = b == B_LFO;
        if (down) {
            if (!i && latch_press(b)) { ui.lay_t0[0] = 0; ui.lay_used[0] = 1; return; }
            ui.lay_t0[i] = ui.now | 1u; ui.lay_used[i] = 0;
            return;
        }
        uint32_t hd = ui.lay_t0[i] ? ui.now - (ui.lay_t0[i] & ~1u) : 0;
        int used = ui.lay_used[i] || !ui.lay_t0[i];
        ui.lay_t0[i] = 0;
        if (b == B_FX) { if (ui.latch != B_FX + 1) for (int k = 0; k < PF_COUNT; k++) if (perf.on[k]) bus_perform(k, 0); }
        else mom_release();
        if (used || hd >= 400) return;
        go(b == B_FX ? PG_FX : PG_LFO);
        if (!i) latch_tapped(b, hd);
        return;
    }
    if (b == B_SEQ) {                  /* a tap: GRID (or out of it); held: the SEQ layer (its keys and knobs) */
        if (down) {
            if (latch_press(b)) { ui.seq_t0 = 0; ui.seq_used = 1; return; }
            ui.seq_t0 = ui.now | 1u; ui.seq_used = 0;
            return;
        }
        uint32_t hd = ui.seq_t0 ? ui.now - (ui.seq_t0 & ~1u) : 0;
        int was_used = ui.seq_used || !ui.seq_t0;
        ui.seq_t0 = 0;
        if (was_used || hd >= 400) return;
        keys_off();                        /* (the keys become steps, or keys again) */
        ui.grid ^= 1;
        ui.pads = 0;
        ui.nheld = 0;
        if (ui.spage * 16 >= seq_len()) ui.spage = 0;
        toast(ui.grid ? "GRID: OCT-/+ PAGES" : "KEYS", ui.grid ? "HOLD G#4-D#5: LENGTH" : "PLAY THE TRACK");
        start_trans(250);
        latch_tapped(b, hd);
        return;
    }
    if (b == B_SAVE) {
        if (down) { ui.save_t0 = ui.now | 1u; ui.save_fired = 0; return; }
        if (!ui.save_t0) return;
        ui.save_t0 = 0;
        if (ui.save_fired) return;            /* the hold already saved the project */
        char a[24], *e = s_pat(a, cur_pat);
        if (pattern_save()) { *e = 0; toast("SAVED", a); dirty = 1; ui.save_t = ui.now | 1u; }
        else toast("PROJECT FULL", "PATTERN NOT SAVED");
        return;
    }
    if (b == B_REC) {                  /* a tap arms recording; held, it is FILL */
        if (down) { ui.rec_t0 = ui.now | 1u; ui.rec_used = 0; return; }
        if (!ui.rec_used && ui.now - ui.rec_t0 < 300) {
            sq.rec ^= 1;
            if (!sq.rec) sq.ready = 0;     /* (REC off: no longer waiting for a note) */
            if (sq.rec && !sq.playing) {   /* stopped: step entry from the page in view */
                ui.cursor = (uint8_t)(ui.spage * 16 < seq_len() ? ui.spage * 16 : 0);
                if (ui.grid) toast("REC", "SEQ: KEYS WRITE STEPS");
                else toast("STEP ENTRY", "KEYS WRITE, EDIT RESTS");
            }
        }
        ui.rec_t0 = 0;
        sq.fill = 0;
        return;
    }
    if (b == B_GLO) {
        if (down) {
            if (latch_press(b)) { ui.glo_used = 1; ui.glo_t0 = 0; return; }
            ui.glo_used = 0; ui.glo_t0 = ui.now | 1u;
        } else if (!ui.glo_used && ui.glo_t0 && ui.now - (ui.glo_t0 & ~1u) < 400) {   /* a tap (held: its map was read) */
            go(PG_GLO);
            latch_tapped(b, ui.now - (ui.glo_t0 & ~1u));
        }
        return;
    }
    if (b == B_HOME && !down) {
        if (!ui.hold_used && ui.hold_t0) latch_tapped(b, ui.now - (ui.hold_t0 & ~1u));
        return;
    }
    if (b == B_EDIT && !down) {        /* a tap: SYN (its pages in turn); held, the modifier (+ SEQ REC PLAY SAVE) */
        uint32_t t0 = ui.edit_t0;
        ui.edit_t0 = 0;
        if (t0 && !ui.hold_used && ui.now - (t0 & ~1u) < 400) go(PG_SYN);
        return;
    }
    if (!down) return;
    switch (b) {
    case B_SEL: go(PG_FILT); break;
    case B_ENV: go(PG_AMP); break;
    case B_EDIT:
        if (step_entry()) {                            /* step entry: a rest, and on */
            step_remove(ui.trk, ui.cursor);
            ui.cursor = (uint8_t)((ui.cursor + 1) % seq_len());
            entry_show("REST");
            ui.edit_t0 = 0;
            break;
        }
        ui.edit_t0 = ui.hold_t0 = ui.now | 1u; ui.hold_used = 0;   /* (SYN on the release: a modifier flips no page) */
        break;
    case B_HOME:
        if (latch_press(b)) { ui.hold_t0 = ui.now | 1u; ui.hold_used = 1; break; }
        go(PG_HOME);
        ui.hold_t0 = ui.now | 1u; ui.hold_used = 0;
        ui.pbank = (uint8_t)(cur_pat / 16);
        m_set(ui.pick, 0, sizeof ui.pick);
        break;
    case B_ARP:                        /* twice: the arp's HOLD, the notes outlast the keys (the page the first turned back) */
        if (ui.arp_tap && ui.now - ui.arp_tap < 300) {
            ui.arp_tap = 0;
            if (ui.arp_sub && ui.page == PG_ARP) ui.sub = (uint8_t)(ui.arp_sub - 1);
            arp_hold_set(!sq.arp_hold);
            toast(sq.arp_hold ? "ARP HOLD" : "ARP HOLD OFF", sq.arp_hold ? "ARP TWICE: LET GO" : 0);
            break;
        }
        ui.arp_tap = ui.now | 1u;
        ui.arp_sub = (uint8_t)(ui.page == PG_ARP ? ui.sub + 1 : 0);
        go(PG_ARP);                    /* again, a moment later: its next page (GRIDS, LANES) */
        break;

    case B_PLAY:
        if (!sq.playing && sq.rec && !ui.grid && !sq.ready && !clk_from()) {   /* Ready (Demake's): the first */
            sq.ready = 1;                                                        /* note starts it, recorded */
            toast("READY", "THE FIRST NOTE STARTS IT");
        } else seq_play(!sq.playing);  /* (Ready: PLAY again starts it now) */
        break;
    case B_OCTD: case B_OCTU: {
        int d = b == B_OCTU ? 1 : -1;
        if (d > 0 && ui.page == PG_ARP && ui.sub && !sq.playing && !(ui.grid && ui.nheld)) { grids_snapshot(); break; }
        if (ui.grid && ui.nheld) held_step_edit(0, d);
        else if (ui.grid) { ui.spage = (uint8_t)CLAMP(ui.spage + d, 0, (seq_len() - 1) / 16); ui.follow = 0; }   /* (the pattern's pages) */
        else {                                       /* the track in view's octave (each track keeps its own) */
            int8_t *oc = &ui.oct[ui.trk];
            *oc = (int8_t)CLAMP(*oc + d, -3, 3);
            char o[8]; char *e = s_cpy(o, "OCT "); if (*oc > 0) *e++ = '+'; s_int(e, *oc); toast(o, 0);
        }
        break;
    }
    default: break;
    }
}

void m2k_button(int b, int down)
{
    if (b < 0 || b >= B_COUNT) return;
    if (down && splash_on()) { splash_skip(); ui.skip_btns |= (uint16_t)(1u << b); return; }
    if (!down && ((ui.skip_btns >> b) & 1u)) { ui.skip_btns &= (uint16_t)~(1u << b); return; }
    syn_sub_check();
    button_event(b, down);
    syn_sub_check();
}

/* a pattern: at once when stopped, at the end of the bar when playing */
static void select_pattern(int n)
{
    n = (n % M2K_PATTERNS + M2K_PATTERNS) % M2K_PATTERNS;
    char a[24], *e = s_pat(a, n);
    if (!pat_can_open(n)) { toast("PATTERNS FULL", "CLEAR OR INIT ONE FIRST"); return; }   /* (the FM-1's memory) */
    if (sq.playing) {
        sq.queued = (uint8_t)n;
        *e++ = ' ';
        s_cpy(e, PAT(n)->name);
        toast(a, n == cur_pat ? "" : "QUEUED");
        return;
    }
    int other = n != cur_pat;
    cur_pat = n;
    sq.queued = (uint8_t)n;
    if (ui.spage * 16 >= seq_len()) ui.spage = 0;     /* a page this pattern does not reach shows nothing: its first */
    ui.follow = 1;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, PAT(n)->kit[t], P_COUNT);
    if (other) keys_home();
    toast(a, PAT(n)->name);
    start_glitch(4);
}

void m2k_pattern(int n) { select_pattern(n); syn_sub_check(); }

/* the web page's own controls: what the panel does, without the panel's ways */
void m2k_track(int t)
{
    if (t < 0 || t >= M2K_TRACKS) return;
    track_to(t);
}

void m2k_mute(int t)
{
    if (t < 0 || t >= M2K_TRACKS) return;
    mute_flip(t);
}

void m2k_tempo(int d)
{
    const pi_t *pi = pinfo(ui.trk, GL(G_TMPO));
    uint8_t *g = &PAT(cur_pat)->glob[G_TMPO];
    *g = (uint8_t)CLAMP(*g + d, kind_min(pi->kind), kind_max(pi->kind));
    char o[12];
    s_cpy(s_int(o, (int)seq_bpm()), " BPM");
    toast("TEMPO", o);
}

void m2k_encoder(int e, int d)
{
    if (!d) return;
    if (e != E_MASTER && splash_on()) { splash_skip(); return; }
    syn_sub_check();
    if (e != E_MASTER) ui.card_used = 1;          /* (the master is a pot: it can drift) */
    m2k_pattern_t *p = PAT(cur_pat);
    switch (e) {
    case E_SELECT:
        if (ui.grid && ui.nheld) { held_note(d); break; }
        if (held(B_GLO)) {                             /* GLO + SELECT: the tempo */
            ui.glo_used = 1;
            p->glob[G_TMPO] = (uint8_t)CLAMP(p->glob[G_TMPO] + d, 0, 255);
            char o[12], *e = s_int(o, (int)seq_bpm());
            s_cpy(e, " BPM");
            toast("TEMPO", o);
            break;
        }
        track_to((ui.trk + M2K_TRACKS + (d > 0 ? 1 : -1)) % M2K_TRACKS);
        break;
    case E_ALGO:
        if (seq_layer()) {                             /* SEQ held: the accent's amount */
            ui.seq_used = 1;
            p->glob[G_ACNT] = (uint8_t)CLAMP(p->glob[G_ACNT] + d, 0, 127);
            char o[12];
            s_int(o, p->glob[G_ACNT]);
            toast("ACCENT", o);
            break;
        }
        if (ui.grid && ui.nheld) {
            for (int i = 0; i < ui.nheld; i++) {
                m2k_step_t *st = &p->step[ui.trk][ui.held[i]];
                int f = ((st->flags >> 1) + (d > 0 ? 1 : 3)) & 3;
                st->flags = (uint8_t)((st->flags & ~(ST_ACC | ST_SLD)) | (f << 1));   /* its trig type stays */
            }
            ui.held_edit = 1;
            break;
        }
        if (ui.page != PG_SYN) {                       /* ALGORITHM turns the page's pages (LFO 1A..3B, FX, GLO...), */
            int n = nsub(ui.page);                     /* to its ends; only EDIT's (SYN) changes the machine (the owner's) */
            int s = CLAMP(ui.sub + (d > 0 ? 1 : -1), 0, n - 1);
            if (s != ui.sub) { ui.sub = (uint8_t)s; ui.focus_t = 0; start_trans(150); }
            break;
        }
        set_machine(M_ORDER[(m_place(p->machine[ui.trk]) + M_COUNT + (d > 0 ? 1 : -1)) % M_COUNT]);
        break;
    case E_PRESET: {
        if (ui.grid && ui.nheld) { held_step_edit(d > 0 ? 1 : -1, 0); break; }
        if (step_entry()) {                            /* step entry: the cursor */
            ui.cursor = (uint8_t)((ui.cursor + seq_len() + (d > 0 ? 1 : -1)) % seq_len());
            entry_show(d > 0 ? "NEXT" : "BACK");
            break;
        }
        select_pattern(((sq.playing ? sq.queued : cur_pat) + M2K_PATTERNS + (d > 0 ? 1 : -1)) % M2K_PATTERNS);
        break;
    }
    case E_MASTER: {
        if (ui.grid && ui.nheld) {            /* a held step: which of its trig tracks it trigs (ALL ... NONE) */
            int k = (ttype_of(p->step[ui.trk][ui.held[0]].flags) + (d > 0 ? 1 : 7)) & 7;
            for (int i = 0; i < ui.nheld; i++) {
                m2k_step_t *st = &p->step[ui.trk][ui.held[i]];
                st->flags = (uint8_t)((st->flags & ~ST_TTYPE) | TTYPE[k]);
            }
            ui.held_edit = 1;
            toast("TRIG", TTYPE_N[k]);
            break;
        }
        master_vol = (uint8_t)CLAMP(master_vol + d, 0, 127);
        char o[12];
        s_int(o, master_vol);
        toast("VOLUME", o);
        break;
    }
    default:
        if (e >= E_K1 && e <= E_K4) edit(e - E_K1, d);
        break;
    }
    syn_sub_check();
}

/* ---------------------------------------------------------------------------------------------- drawing */

/* the grid (docs/screen-grid.md), every text in MONO2K's Orbitron (F_S 12 px at 800 the labels, values and lines, 9
 * rows of capitals; F_M and F_L the stage's big type; F_B, the first redesign's 11 px, the bars). The header an ink
 * bar (rows 0-15: the pattern, the modes, REC, the tempo, the transport); the knobs 16-60 (four cells of 60 under the
 * FM-1's four knobs, solid rules at x 60, 120, 180, each its label, its value and a bar, 5 px in; a solid rule
 * on 60); the stage 61-201 (the value's column [4, 101), a solid rule on 104, the picture [108, 237)); the steps 202-223 (a dotted rule, 16
 * boxes, a beat to a knob's cell); the footer an ink bar (224-239: the bars, the breadcrumb, the page). A cell's
 * content begins 4 px past its left edge and ends 3 clear before its right one (a knob's 5 and 4): the audit fails a
 * text that leaves its cell or comes near a rule */
#define HEAD_H 16                       /* the header's bar */
#define KNOB_Y 16
#define KNOB_R 60                       /* the knobs' rule */
#define STAGE_Y 61
#define STAGE_H 141                     /* 61..201 */
#define STEPS_R 202                     /* the steps' rule, dotted */
#define STRIP_Y 203
#define FOOT_Y 224                      /* the footer's bar: rows 224..239 */
#define GX 104                          /* where the stage's two cells meet */
#define VX 4                            /* the value's column: [VX, VX + VW) */
#define VW 97
#define PX0 108                         /* the picture: [PX0, PX0 + PIC_W) x [PY0, PY0 + PIC_H) */
#define PIC_W 129
#define PY0 64
#define PIC_H 134
#define TY 1                            /* a tag's type below its box's top */
/* the stage's lines of small type (their tops: the capitals are the 9 rows from there) */
#define L_TOP 64                        /* the label (a tag's box from 63) */
#define BIG_TOP 80                      /* the big name or value's top */
#define L_B0 188                        /* the stage's last line: a picture's names */
#define STEP_Y 205                      /* the steps' boxes, 11 x 11 */

static int tag_w(const char *s) { return text_w(&F_S, s, 0) + 4; }

/* a line in at most w px: in f, then the alternative s2, then each size under f to the small type. The font picked
 * (0: nothing fits), *s the text, *tr its tracking (0: every size keeps its own, a pixel of the design) */
static const m2k_font_t *const SIZES[3] = {&F_S, &F_M, &F_L};
static const m2k_font_t *fit_font(const m2k_font_t *f, const char **s, const char *s2, int w, int *tr)
{
    int k = f == &F_L ? 2 : f == &F_M ? 1 : 0;
    for (; k >= 0; k--)
        for (int a = 0; a < 2; a++) {
            const char *c = a ? s2 : *s;
            if (!c) continue;
            if (text_w(SIZES[k], c, 0) <= w) { *s = c; *tr = 0; return SIZES[k]; }
        }
    *tr = 0;
    return 0;
}

/* a line of the small type in at most w, else the alternative (s2, may be 0), else cut (the audit fails it: a
 * shorter form is wanted). Returns the width drawn */
static int text_fit(int x, int y, const char *s, const char *s2, int w, int ink)
{
    int tr;
    const char *c = s;
    if (fit_font(&F_S, &c, s2, w, &tr)) return text(&F_S, x, y, c, ink, tr);
    clip(x, y, w, F_S.h);
    int r = text(&F_S, x, y, s, ink, 0);
    noclip();
    return r < w ? r : w;
}

/* a line of f in at most w, cut if it must be (the audit fails it) */
static int text_fit_f(const m2k_font_t *f, int x, int y, const char *s, int w, int ink)
{
    if (text_w(f, s, 0) <= w) return text(f, x, y, s, ink, 0);
    clip(x, y, w, f->h);
    int r = text(f, x, y, s, ink, 0);
    noclip();
    return r < w ? r : w;
}

/* the big name or value, from the stage's BIG_TOP in the size asked (a size down where too wide); as it changes two
 * bands of it slip sideways a moment (the glitch in motion, still type at rest). Its last row */
static int big_value(int x, int y, int w, const char *s, const m2k_font_t *f, uint32_t roll)
{
    int tr;
    const char *c = s;
    const m2k_font_t *g = fit_font(f, &c, 0, w, &tr);
    if (!g) g = &F_S;                                   /* (cut: the audit says so) */
    text(g, x, y, c, TX_INK, tr);
    if (roll < 90 && !calm()) {
        row_slip(x, y + g->base / 3, w, 2 * (g->base / 7), roll < 45 ? 4 : 2);
        row_slip(x, y + (2 * g->base) / 3, w, g->base / 7, roll < 45 ? -3 : -1);
    }
    return y + g->base - 1;
}

/* ---- the header, an ink bar (the first redesign's, filled), in the bars' type: the pattern (its slot a paper tag,
 * its name), the keys' and the sequencer's modes, REC, the tempo, the transport. What does not fit goes, the name
 * first (whole or not at all), then the modes from the last */
static const uint8_t REC_ON[8] = {0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C};
static const uint8_t REC_OFF[8] = {0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C};
#define BAR_TY 4                        /* the bars' type: its capitals 4 px into the bar, 4 clear under them */

/* a tag on a bar: a paper box, its type ink (on); else a paper frame round paper type. 12 rows from the bar's third */
static int bar_tag(int x, int y, const char *s, int on)
{
    int w = text_w(&F_B, s, 0) + 6;
    if (on) rect(x, y + BAR_TY - 2, w, F_B.base + 4, 0);
    else frame(x, y + BAR_TY - 2, w, F_B.base + 4, 0);
    text(&F_B, x + 3, y + BAR_TY, s, !!on, 0);
    return w;
}

static void header(void)
{
    rect(0, 0, 240, HEAD_H, 1);
    CELL(0, 0, 240, HEAD_H, 2, 1, 238, HEAD_H - 1);
    const int y = BAR_TY;                               /* the capitals' rows 4..11 */
    int xr = 236;                                       /* the free space's last column */
    if (sq.playing) for (int j = 0; j < 8; j++) hline(xr - 4, y + j, j < 4 ? j + 2 : 9 - j, 0);
    else rect(xr - 7, y, 8, 8, 0);
    xr -= 8 + 6;
    char t[8];
    s_int(t, (int)seq_bpm());
    xr -= text_r(&F_B, xr + 1, y, t, 0, 0) + 6;
    if (sq.rec) { blit((ui.now >> 8) & 1 ? REC_ON : REC_OFF, 8, 8, xr - 7, y, 4); xr -= 8 + 6; }   /* (paper) */
    char a[8];
    s_pat(a, cur_pat);
    int pw = 2 + bar_tag(2, 0, a, 1);
    /* (HOLD: the arp's; MUTE: a mute waiting for the bar, its box blinking) */
    static const char *const MODE[6] = {"GRID", "PADS", "HOLD", "MUTE", "READY", "FILL"};
    const int on[6] = {ui.grid, ui.pads, sq.arp_hold, mute_pending(), sq.ready, sq.fill};
    for (int i = 0; i < 6; i++) {
        if (!on[i]) continue;
        int w = text_w(&F_B, MODE[i], 0) + 6;
        if (xr - w + 1 < pw + 6) continue;
        bar_tag(xr - w + 1, 0, MODE[i], i == 3 ? (int)((ui.now >> 7) & 1) : i >= 2);
        xr -= w + 4;
    }
    const char *nm = pat_name(cur_pat);
    int room = xr - (pw + 6) + 1;                       /* (tracked a pixel more where it fits, as the first redesign) */
    if (text_w(&F_B, nm, 0) <= room) text(&F_B, pw + 6, y, nm, 0, text_w(&F_B, nm, 1) <= room);
}

/* ---- the knobs, along the top as on the panel: each cell its label (inverted while turned), its value under it (in
 * F_M where the row's every value fits, else F_S; locked: inverted), and a bar under that, easing to where
 * the value sits in its range: a dotted track and a solid part (from the middle for a two-sided one), segments for a
 * few choices, nothing for an action. Everything 5 px in from the cell's rules */
static int bipolar(int kind) { return kind == K_BI || kind == K_PAN || kind == K_SEMI || kind == K_FINE || kind == K_TUN2 || kind == K_SEMI24; }
static int action(int kind) { return kind == K_LOAD || kind == K_SLOAD || kind == K_SSAVE || kind == K_WIPE || kind == K_DEMO; }

/* a level (a picture's amount): a bar to w8 (Q8 of w) on a dotted track, from the middle when two-sided; 3 rows */
static void level(int x, int y, int w, int n, int c, int w8, int bip)
{
    (void)n; (void)c;
    int f = CLAMP(w8 >> 8, 0, w);
    dots_h(x, y + 2, w);
    if (bip) {
        int mid = x + w / 2;
        vline(mid, y - 2, 5, 1);
        if (x + f > mid) rect(mid, y, x + f - mid, 3, 1);
        else rect(x + f, y, mid - x - f, 3, 1);
    } else rect(x, y, f, 3, 1);
}

#define KX 5                            /* a knob cell's content: [60 k + KX, 60 k + KX + KW) */
#define KW 51
#define K_LBL (KNOB_Y + 5)              /* the label's capitals: rows 21..29 */
#define K_BASE (KNOB_Y + 31)            /* the value's last row, 47, whatever its size */
#define KB_H 3                          /* the bar's rows: 2 read thin under the bold values */
#define K_BAR (KNOB_Y + 36)             /* the bar: rows 52..54 */

/* a knob's bar, KB_H rows from y, w wide: n choices (n <= 8) a segment each, the chosen solid and the rest a line on
 * the last row; else a dotted track on the last row and the part up to q (0..256) solid, from the middle when
 * two-sided (a tick there). pop: turning, the chosen segment or the solid part's end stands up 3 rows */
static void knob_bar(int x, int y, int w, int n, int c, int q, int bip, int pop)
{
    const int h = KB_H;
    if (n) {
        for (int i = 0; i < n; i++) {
            int x0 = x + i * (w + 2) / n, x1 = x + (i + 1) * (w + 2) / n - 2;
            if (i == c) rect(x0, y - (pop ? 3 : 0), x1 - x0, h + (pop ? 3 : 0), 1);
            else hline(x0, y + h - 1, x1 - x0, 1);
        }
        return;
    }
    int f = (CLAMP(q, 0, 256) * w + 128) >> 8;          /* (Q8 of the width, to the nearest pixel) */
    dots_h(x, y + h - 1, w);
    int e;                                              /* the solid part's moving end */
    if (bip) {
        int mid = x + w / 2;
        vline(mid, y - 2, h + 2, 1);
        if (x + f > mid) rect(mid, y, x + f - mid, h, 1); else rect(x + f, y, mid - x - f, h, 1);
        e = x + f > mid ? x + f - 2 : x + f;
    } else { rect(x, y, f, h, 1); e = x + f - 2; }
    if (pop) rect(CLAMP(e, x, x + w - 2), y - 3, 2, h + 3, 1);
}

/* the value's size a knob's parameter allows: F_M if its every value fits the cell, else F_S. Kept per knob while its
 * parameter stays */
static const m2k_font_t *knob_font(int k, const pi_t *pi, int code)
{
    static int key[4] = {-1, -1, -1, -1};
    static const m2k_font_t *f[4];
    int id = code | ui.trk << 8 | PAT(cur_pat)->machine[ui.trk] << 12;
    if (id == key[k]) return f[k];
    key[k] = id;
    int n = kind_choices(pi->kind), mn = kind_min(pi->kind), mx = kind_max(pi->kind), cnt = n ? n : mx - mn + 1, w = 0;
    for (int i = 0; i < cnt && i < 128 && w <= KW; i++) {   /* (every value: a page turned costs ~0.1 M once) */
        char o[16];
        const char *u;
        fmt(pi, ui.trk, n ? enc(i, n) : mn + i, o, &u);
        int ow = text_w(&F_M, o, 0);
        w = ow > w ? ow : w;
    }
    f[k] = w <= KW ? &F_M : &F_S;
    return f[k];
}

static void knobs(void)
{
    /* the row's values in one size: F_M where all four allow it, else F_S (turning one never changes it) */
    const m2k_font_t *rf = &F_M;
    for (int k = 0; k < 4; k++) {
        int code = knob_at(k);
        if (code == NONE) continue;
        const pi_t *pi = pinfo(ui.trk, code);
        if (pi->s4[0] != '-' && knob_font(k, pi, code) != &F_M) rf = &F_S;
    }
    for (int k = 0; k < 4; k++) {
        int x = k * 60, cx = x + KX, code = knob_at(k);
        CELL(x, KNOB_Y, x + 60, KNOB_R, cx, KNOB_Y + 1, cx + KW, KNOB_R - 1);
        if (k) rule_v(x, KNOB_Y, KNOB_R - KNOB_Y, 0);
        if (code == NONE) continue;
        const pi_t *pi = pinfo(ui.trk, code);
        if (pi->s4[0] == '-') { text(&F_S, cx, K_LBL, "-", 1, 0); continue; }
        int v = knob_value(code), lk = knob_locked(code);
        int focus = ui.focus_t && ui.focus == k && ui.now - ui.focus_t < 2500;
        if (focus) rect(x + 3, K_LBL - 1, 55, F_S.base + 2, 1);   /* (2 px clear of the rules) */
        text(&F_S, cx, K_LBL, pi->s4, !focus, 0);
        char o[16];
        const char *u;
        fmt(pi, ui.trk, v, o, &u);
        int tr;
        const char *c = o;
        const m2k_font_t *vf = fit_font(rf, &c, 0, KW, &tr), *g = vf ? vf : &F_S;   /* (a size down where one is wider) */
        int vy = K_BASE - (g->base - 1);
        if (lk) rect(x + 3, vy - 1, (vf ? text_w(g, c, tr) : KW) + 4, g->base + 2, 1);
        if (vf) text(vf, cx, vy, c, !lk, tr);
        else text_fit(cx, vy, o, 0, KW, !lk);           /* (too wide even small: cut, the audit says so) */
        if (action(pi->kind)) continue;
        int n = kind_choices(pi->kind), mx = kind_max(pi->kind), mn = kind_min(pi->kind);
        int target = CLAMP(((v - mn) * 256) / (mx - mn ? mx - mn : 1), 0, 256);
        int32_t ef = ui.fdt >= 99 ? 256 : (int32_t)ui.fdt * 256 / 99;   /* a third of the way each 33 ms, whatever */
        ui.ease[k] += ((target * 256 - ui.ease[k]) * ef) >> 8;            /* the frame rate (Q8: the bar eases to it) */
        knob_bar(cx, K_BAR, KW, n && n <= 8 ? n : 0, choice_of(pi->kind, v), ui.ease[k] >> 8, bipolar(pi->kind),
                 focus && ui.now - ui.focus_t < 200);
    }
    rule_h(0, KNOB_R, 240, 0);
}

/* ---- the steps */
static int play_step(void) { int s = (int)sq.step - 1; return s < 0 ? seq_len() - 1 : s; }

/* a step's box (a ring pops round it as it is toggled): empty its outline; a trig solid; with locks a checker in its
 * outline; on a condition (a chance, A:B, PRE, FILL ...) framed, its block inset; on some of its trig tracks a third
 * filled for each (AMP, FILTER, LFO, left to right), NONE a dot; accent a notch, slide a dotted edge, ratchets dots
 * along the foot; past the pattern's end a dot at its centre. The playing step framed */
static void step_cell(int x, int y, int w, int h, int t, int s)
{
    m2k_pattern_t *p = PAT(cur_pat);
    m2k_step_t st = p->step[t][s];
    int playing = sq.playing && track_pos(t) == s;
    int held = 0;
    if (ui.grid && t == ui.trk) for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == s) held = 1;
    if (s >= seq_len()) { px(x + w / 2, y + h / 2, 1); return; }
    if (lane_of(p, t)) {               /* following a lane: its trigs in this loop (stopped, a PLAY's first), dotted round */
        uint32_t len = (uint32_t)seq_len(), n = !sq.playing ? 0 : spd_of(t) == 4 ? sq.n1x : sq.tk[t];
        int hit = grids_hit(p, lane_of(p, t) - 1, (n ? (n - 1) / len * len : 0) + (uint32_t)s, 0);
        if (hit) { rect(x, y, w, h, 1); if (hit == 2) rect(x + 2, y + 2, 2, 2, 0); }
        else dots_frame(x, y, w, h);
        if (playing) frame(x - 2, y - 2, w + 4, h + 4, 1);
        if (held && ((ui.now >> 7) & 1)) rect(x, y, w, h, 2);
        return;
    }
    if (st.flags & ST_TRIG) {
        int c = st.cond, lk = has_locks(t, s), in = c == C_ALWAYS ? 0 : 2;
        unsigned sk = st.flags & ST_TTYPE;
        if (in || sk || lk) frame(x, y, w, h, 1);
        if (sk) {
            for (int k = 0; k < 3; k++) {
                if (sk & (ST_NOAMP << k)) continue;
                int x0 = x + 1 + k * (w - 2) / 3, x1 = x + 1 + (k + 1) * (w - 2) / 3;
                rect(x0 + (in ? 1 : 0), y + 1 + in / 2, x1 - x0 - (in ? 1 : 0), h - 2 - in, 1);
            }
            if (sk == ST_TTYPE) rect(x + w / 2 - 1, y + h / 2 - 1, 3, 3, 1);
        } else if (lk && !in) {
            for (int j = 2; j < h - 2; j++) for (int i = 2; i < w - 2; i++) if (!((x + i + y + j) & 1)) px(x + i, y + j, 1);
        } else rect(x + in, y + in, w - 2 * in, h - 2 * in, 1);
        if (st.flags & ST_ACC) rect(x + 2, y + 2, 2, 2, 0);
        if (st.flags & ST_SLD) for (int j = 0; j < h; j += 2) px(x + w - 2, y + j, 0);
        for (int i = 1; i < st.rtrg && i < 4; i++) rect(x + i * 3 - 1, y + h - 3, 2, 2, 2);
    } else frame(x, y, w, h, 1);
    if (playing) frame(x - 2, y - 2, w + 4, h + 4, 1);
    if (step_entry() && t == ui.trk && s == ui.cursor && ((ui.now >> 8) & 1)) frame(x - 2, y - 2, w + 4, h + 4, 1);
    if (ui.pop_t && t == ui.pop_trk && s == ui.pop_s && ui.now - ui.pop_t < 160) rect(x - 1, y - 1, w + 2, h + 2, 2);   /* toggled */
    if (held && ((ui.now >> 7) & 1)) rect(x, y, w, h, 2);
}

/* the steps of the page in view, the track's: 16 boxes, a beat's four in each knob's cell, a mark under each beat */
static int step_x(int i) { return (i >> 2) * 60 + 4 + (i & 3) * 14; }

static void strip(void)
{
    rule_h(0, STEPS_R, 240, 1);
    for (int i = 0; i < 16; i++) {
        step_cell(step_x(i), STEP_Y, 11, 11, ui.trk, ui.spage * 16 + i);
        if (!(i & 3)) rect(step_x(i) + 4, STEP_Y + 14, 3, 2, 1);   /* (rows 219..220) */
    }
}

/* ---- the footer, an ink bar (the first redesign's, filled), in the bars' type: the pattern's four bars (the one in
 * view solid, those past its end dotted, the playing one marked), the breadcrumb (the track, its machine, the page; on
 * GLO the section) and the page among its section's */
static int crumb_w(const char *const *it, int n)
{
    int w = 0;
    for (int i = 0; i < n; i++) w += text_w(&F_B, it[i], 0) + (i ? 11 : 0);
    return w;
}

static void foot(void)
{
    const int y = FOOT_Y + BAR_TY;                      /* the capitals' rows 228..235 */
    rect(0, FOOT_Y, 240, 240 - FOOT_Y, 1);
    CELL(0, FOOT_Y, 240, 240, 2, FOOT_Y + 1, 238, 239);
    for (int pg = 0; pg < 4; pg++) {
        int x = 4 + pg * 11, used = pg * 16 < seq_len(), on = pg == ui.spage;
        if (!used) {                                    /* past the end: dotted, in step with the checker */
            for (int i = 0; i < 9; i++)
                for (int j = 0; j < 8; j++)
                    if ((i == 0 || i == 8 || j == 0 || j == 7) && !((x + i + y + j) & 1)) px(x + i, y + j, 0);
            continue;
        }
        if (on) rect(x, y, 9, 8, 0); else frame(x, y, 9, 8, 0);
        if (sq.playing && play_step() / 16 == pg) rect(x + 3, y + 3, 3, 2, on);
    }
    char pl[12], cnt[8];
    page_label(ui.page, ui.sub, pl);
    int n = nsub(ui.page), xr = 236;
    if (n > 1) {
        char *e = s_int(cnt, ui.sub + 1);
        *e++ = '/';
        s_int(e, n);
        xr -= text_r(&F_B, 237, y, cnt, 0, 0) + 8;
    }
    const mi_t *mi = &MI[PAT(cur_pat)->machine[ui.trk]];
    char tn[3] = {'T', (char)('1' + ui.trk), 0};
    const char *it[3];
    int k = 0;
    if (ui.page == PG_GLO) it[k++] = "GLO";
    else { it[k++] = tn; it[k++] = mi->name; }
    it[k++] = pl;
    const int x0 = 52, room = xr - x0 + 1;
    if (k == 3 && crumb_w(it, k) > room) it[1] = mi->badge;       /* the machine's badge, else none */
    if (k == 3 && crumb_w(it, k) > room) { it[1] = it[2]; k = 2; }
    for (int i = 0, x = x0; i < k; i++) {
        if (i) { for (int j = 0; j < 5; j++) hline(x + 4, y + 2 + j, j < 3 ? j + 1 : 5 - j, 0); x += 11; }
        x += text(&F_B, x, y, it[i], 0, 0);
    }
}

/* ---- the value's column (left of the stage) */

/* a parameter's values in one size: the largest its widest value fits the column in (each choice measured; a number
 * at its ends and middle), so turning it never changes the size. Kept while the same parameter is in focus */
static const m2k_font_t *value_font(const pi_t *pi, int code)
{
    static int key = -1;
    static const m2k_font_t *f;
    int k = code | ui.trk << 8 | PAT(cur_pat)->machine[ui.trk] << 12;
    if (k == key) return f;
    key = k;
    int n = kind_choices(pi->kind), cnt = n ? n : 3, w = 0;
    for (int i = 0; i < cnt && i < 64; i++) {
        int mn = kind_min(pi->kind), mx = kind_max(pi->kind);
        int v = n ? enc(i, n) : i == 0 ? mn : i == 1 ? mx : (mn + mx) / 2;
        char o[16];
        const char *u;
        fmt(pi, ui.trk, v, o, &u);
        int ow = text_w(&F_L, o, 0);
        w = ow > w ? ow : w;
    }
    f = w <= VW ? &F_L : &F_M;
    return f;
}

static void value_cell(void) { CELL(0, STAGE_Y, GX, STEPS_R, VX, STAGE_Y + 2, VX + VW, STEPS_R - 1); }

/* a knob in focus: its name above, its value in big type, its unit under it (and LOCK or REC) */
static void focus_left(int code, int k)
{
    char v[16], o[32];
    const char *u;
    const pi_t *pi = pinfo(ui.trk, code);
    int val = knob_value(code);
    fmt(pi, ui.trk, val, v, &u);
    char *e = o;
    if (code < 8) { *e++ = (char)('A' + code); *e++ = ' '; *e++ = '/'; *e++ = ' '; }
    s_cpy(e, pi->name);
    text_fit(VX, L_TOP, text_w(&F_S, o, 0) <= VW ? o : pi->name, pi->s4, VW, 1);
    int y = big_value(VX, BIG_TOP, VW, v, value_font(pi, code), ui.now - ui.roll_t) + 8;
    int x = VX, mark = knob_locked(code) ? 1 : sq.rec && sq.playing && code < 0x80 ? 2 : 0;
    if (*u) x += text_fit(VX, y, u, 0, VW, 1) + 8;
    if (mark && x + tag_w("LOCK") > VX + VW) { x = VX; y += 15; }   /* (beside the unit, else under it) */
    if (mark == 1) tag(x - 2, y - TY, "LOCK", 1);
    else if (mark == 2) tag(x - 2, y - TY, "REC", (ui.now >> 8) & 1);
    (void)k;
}

/* the machine card, after Model:Cycles' engine screens: its class a tag and its style; the name after its family in
 * big type; the family's badge and which of the machines; punch, metal and plastic, five segments each, dealt in */
static void machine_card(void)
{
    const int m = PAT(cur_pat)->machine[ui.trk];
    const mi_t *mi = &MI[m];
    if (tag_w(mi->cls) + 6 + text_w(&F_S, mi->style, 0) <= VW + 2) {   /* its class and style, else its style alone */
        int tw = tag(VX - 2, L_TOP - TY, mi->cls, 1);
        text(&F_S, VX - 2 + tw + 6, L_TOP, mi->style, 1, 0);
    } else tag(VX - 2, L_TOP - TY, mi->style, 1);
    int n = 0;
    while (mi->name[n] && mi->name[n] != '-' && mi->name[n] != '+' && n < 6) n++;
    const char *model = mi->name[n] ? mi->name + n + 1 : mi->name;
    int y = big_value(VX, BIG_TOP, VW, model, &F_L, 1000) + 8;
    char o[16], *e = s_cpy(o, mi->badge);
    int i = m_place(m) + 1;
    *e++ = ' '; *e++ = '#'; *e++ = (char)('0' + i / 10); *e++ = (char)('0' + i % 10); *e++ = '/';
    s_int(e, M_COUNT);
    text_fit(VX, y, o, mi->badge, VW, 1);
    static const char *const ST[3] = {"PNCH", "METL", "PLST"};
    uint32_t age = ui.card_t ? ui.now - ui.card_t : 1000;
    for (int r = 0; r < 3; r++) {
        int ry = y + 16 + r * 14;
        text(&F_S, VX, ry, ST[r], 1, 0);
        for (int sg = 0; sg < 5; sg++) {
            if (age <= (uint32_t)(60 + (r * 5 + sg) * 25)) continue;
            int sx = VX + 46 + sg * 10;
            if (sg < mi->stat[r]) rect(sx, ry + 1, 8, 8, 1);
            else frame(sx, ry + 1, 8, 8, 1);
        }
    }
}

/* a held step: its note in big type, what turns it, its condition, ratchet, trig type, accent and slide */
static void step_card(void)
{
    m2k_step_t st = PAT(cur_pat)->step[ui.trk][ui.held[0]];
    char o[24], *e = s_cpy(o, "STEP ");
    e = s_int(e, ui.held[0] + 1);
    if (ui.nheld > 1) { e = s_cpy(e, " +"); s_int(e, ui.nheld - 1); }
    text_fit(VX, L_TOP, o, 0, VW, 1);
    s_note(o, st.note);
    int y = big_value(VX, BIG_TOP, VW, o, &F_L, 1000) + 8;
    text_fit(VX, y, "TRACK: NOTE", "TRK: NOTE", VW, 1);  /* the TRACK knob turns the note */
    int x = VX - 2;
    y += 15;
    const char *tg[5] = {COND_N[st.cond], 0, (st.flags & ST_TTYPE) ? TTYPE_N[ttype_of(st.flags)] : 0,
                         (st.flags & ST_ACC) ? "ACC" : 0, (st.flags & ST_SLD) ? "SLD" : 0};
    char r[4] = {'X', (char)('0' + (st.rtrg ? st.rtrg : 1)), 0, 0};
    tg[1] = r;
    const int on[5] = {st.cond != C_ALWAYS, st.rtrg > 1, 1, 1, 1};
    for (int i = 0; i < 5; i++) {                 /* in a row, on to the next line when the column is full */
        if (!tg[i]) continue;
        if (x + tag_w(tg[i]) > VX + VW + 2) { x = VX - 2; y += 14; }
        x += tag(x, y - TY, tg[i], on[i]) + 4;
    }
}

/* the page at rest: its name in big type */
static void page_rest(void)
{
    char o[24];
    page_label(ui.page, ui.sub, o);
    text(&F_S, VX, L_TOP, "PAGE", 1, 0);
    big_value(VX, BIG_TOP, VW, o, &F_L, 1000);         /* every page's title in one size; the picture says the rest */
}

/* the mark, after the machine's 3D logo: it jumps off its dithered shadow on each of the track's hits (higher for a
 * louder one) and falls back, the shadow narrower as it rises */
static void mark_3d(int x, int y, int m)
{
    int f = (ui.icon_ph >> 8) % ICON_FRAMES;
    int lift = 0;
    uint32_t dt = ui.icon_hit ? ui.now - ui.icon_hit : 1000u;
    if (dt < 180) { int top = 2 + ui.icon_amp / 16; lift = top - (int)(((uint32_t)top * dt * dt) / (180u * 180u)); }
    for (int j = 0; j < 5; j++) {
        int w = 44 - j * 6 - lift * 4;
        if (w < 4) continue;
        rect_grad(x + 32 - w / 2, y + ICON_N + 1 + j, w / 2, 1, 4, 28 - j * 5, 0);
        rect_grad(x + 32, y + ICON_N + 1 + j, w / 2, 1, 28 - j * 5, 4, 0);
    }
    if (m == M_SAW && sq.playing) {
        /* SWAV-SAW while it plays: each tooth as tall as the track's level, each a little behind the one before and
         * falling at its own pace, stacked back to front, a front tooth cutting out what it covers */
        for (int k = 0; k < 3; k++) {
            if (k) blit(SAW_MASK[k][ui.saw_lv[k]], ICON_N, ICON_N, x, y - lift, 4);
            blit(SAW_TEETH[k][ui.saw_lv[k]], ICON_N, ICON_N, x, y - lift, 1);
        }
        return;
    }
    blit(ICON_BITS[m][f], ICON_N, ICON_N, x, y - lift, 1);
}

/* one mark a machine, the current one a solid bar; 5 px apart (nineteen: 93 px of the picture's 129) */
static void machine_dots(int cx, int y, int m)
{
    int x0 = cx - ((M_COUNT - 1) * 5) / 2;
    for (int i = 0; i < M_COUNT; i++) {
        int x = x0 + i * 5;
        if (M_ORDER[i] == m) rect(x - 1, y - 2, 3, 5, 1);
        else px(x, y, 1);
    }
}

/* the filter's response as the track chain builds it (m2k_dsp.c track_block), on the screen's axis (q: 0..1016 for
 * 20 Hz .. 20 kHz, 101.9 an octave): the two-pole high-pass and low-pass at their cutoffs and resonance offsets (T_HPC
 * T_LPC, k = 1 / Q: T_HPK T_LPK), the one-pole high-pass a 1.75th above, the two one-pole low-passes 1.55 times up.
 * Each stage's gain in dB, Q8, at a distance d (q) from its cutoff; a high-pass is its low-pass mirrored */
static int32_t db_of(uint64_t den_q32)               /* -10 log10(den / 2^32), Q8 dB */
{
    int n = 0;
    while (den_q32 >> 32) { den_q32 >>= 1; n++; }
    int32_t l2 = ilog2_q8((uint32_t)den_q32 | 1u) + n * 256 - 32 * 256;
    return -(l2 * 771) / 256;                        /* 10 log10 2 = 3.0103 dB an octave of power */
}

static int32_t lp1_db(int32_t d)
{
    if (d < -510) return 0;
    if (d > 510) return -d * 15 + 1541;              /* -6 dB an octave, from the corner's -3 */
    uint32_t u2 = iexp2_q16((d * 1286) >> 8);        /* (f / fc)^2, Q16 */
    return db_of((uint64_t)(65536u + u2) << 16);
}

static int32_t lp2_db(int32_t d, uint32_t k_q24)
{
    if (d < -510) return 0;
    if (d > 510) return -d * 30;                     /* -12 dB an octave */
    uint32_t u2 = iexp2_q16((d * 1286) >> 8);
    int32_t a = 65536 - (int32_t)u2, k16 = (int32_t)(k_q24 >> 8);   /* u2 under 2^27 (d within 510), k16 2^17 */
    uint64_t den = (uint64_t)mul_sh(a, a, 0) + (uint64_t)mul_sh((int32_t)mul_sh(k16, k16, 16), (int32_t)u2, 0);
    return db_of(den);
}

/* the whole response at q, Q8 dB, for cutoffs hp and lp (q) and the resonance values hq lq (0..127) */
static int32_t filt_db(int q, int hp, int lp, int hq, int lq)
{
    int hc = hp + (int)((T_HPC[hq] * 1087) >> 18), lc = lp + (int)((T_LPC[lq] * 1087) >> 18);
    return lp2_db(hc - q, T_HPK[hq]) + lp1_db(hp + 83 - q) + lp2_db(q - lc, T_LPK[lq]) + 2 * lp1_db(q - (lp + 65));
}

#define FDB_TOP (18 * 256)                           /* the picture's range: +18 dB .. -42 dB */
#define FQ_MAX 1220                                  /* the axis: 20 Hz to two octaves past 20 kHz (1016), so a
                                                        high LP's slope shows falling, not cut at the edge */
#define FDB_SPAN (60 * 256)

static int filt_row(int32_t db, int top, int yb)     /* a gain to its row; past the top it rides the top */
{
    int32_t r = (FDB_TOP - db) * (yb - top) / FDB_SPAN;
    return top + CLAMP(r, 0, yb - top);
}

/* the response drawn: hatched under it (dense: a pattern, PT_*), a two-dot line on it (three heavy), 0 dB dotted, the
 * decades ticked;
 * returns the rows at the two cutoffs for the handles */
static void filt_curve(int X, int W, int top, int yb, int hp, int lp, int hq, int lq, int dense, int heavy, int *yh, int *yl)
{
    int prev = yb, y0 = filt_row(0, top, yb);
    for (int i = 0; i < W; i++) {
        int q = i * FQ_MAX / (W - 1);
        int y = filt_row(filt_db(q, hp, lp, hq, lq), top, yb);
        for (int j = y + 1; j <= yb; j++) if (pat_ink(dense, X + i, j)) px(X + i, j, 1);
        if (i) { line(X + i - 1, prev, X + i, y, 1); line(X + i - 1, prev - 1, X + i, y - 1, 1); }
        if (heavy && i) line(X + i - 1, prev - 2, X + i, y - 2, 1);
        prev = y;
    }
    dots_h(X, y0, W);
    static const int16_t DEC[3] = {236, 575, 913};   /* 100 Hz, 1 kHz, 10 kHz on the axis */
    for (int d = 0; d < 3; d++) vline(X + DEC[d] * (W - 1) / FQ_MAX, yb + 1, 3, 1);
    hline(X, yb + 1, W, 1);
    *yh = filt_row(filt_db(hp, hp, lp, hq, lq), top, yb);
    *yl = filt_row(filt_db(lp, hp, lp, hq, lq), top, yb);
}

/* the amp's level at a point of its picture (q 0..256 along its segment): Q8, 0..256, on the sound's own curves
 * (env_block): the attack the square of its time (ATK 0 straight), the hold, the decay straight in dB to where the key
 * is let go (a quarter: -12 dB), the release on down 48 dB (-60 under full) */
static int env_level(int seg, int q, int dec_inf, int atk0)
{
    if (seg == 0) return atk0 ? q : (q * q) >> 8;
    if (seg == 1) return 256;
    if (seg == 2) return dec_inf ? 256 : (int)(iexp2_q16(-(q * 2)) >> 8);                      /* two halvings */
    int from = dec_inf ? 256 : 64;
    return (int)(((uint32_t)from * (iexp2_q16(-(q * 8)) >> 8)) >> 8);                          /* eight */
}

/* ---- the pictures (right of the stage): what the page is about, live. Each keeps to [PX0, PX0 + PIC_W) x [PY0, PY0 +
 * PIC_H): its plot above, its names on the stage's last line (L_B0); curves two dots thick (three for the part being
 * turned), axes solid, references dotted on the checker, areas hatched (the part being turned solid), handles 5 x 5
 * (7 x 7 solid while turned), a tag inverted for what turns */
static void lfo_scopes(const uint8_t *k);

/* GRIDS' pictures. A lane's K playing now: the track in view's when it follows the lane, else the first that does, else
 * the pattern's (-1 stopped: a PLAY's first cycle is shown) */
static int lane_now(const m2k_pattern_t *p, int part)
{
    if (!sq.playing) return -1;
    int t = lane_of(p, ui.trk) == part + 1 ? ui.trk : -1;
    for (int i = 0; t < 0 && i < M2K_TRACKS; i++) if (lane_of(p, i) == part + 1) t = i;
    uint32_t n = t < 0 || spd_of(t) == 4 ? sq.n1x : sq.tk[t];
    return n ? (int)(n - 1) : -1;
}

/* a lane's cycle as it plays: a bar a step (16 or 32), as tall as its level (CHAOS's raise in it), solid where it trigs,
 * framed under its density (the dotted line), an accent a notch at its top; the step playing marked under it */
static void grids_lane(int x, int y, int h, int part)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int cyc = grids_cyc(p), kn = lane_now(p, part), base = kn < 0 ? 0 : kn - kn % cyc, bw = PIC_W / cyc;
    const int th = 255 - gr8(p->glob[G_GRBD + part]);
    for (int i = 0; i < cyc; i++) {
        int lv, hit = grids_hit(p, part, (uint32_t)(base + i), &lv), bh = (lv * h + 254) / 255, bx = x + i * bw;
        if (hit) rect(bx, y + h - bh, bw - 1, bh, 1);
        else if (bh > 1) frame(bx, y + h - bh, bw - 1, bh, 1);
        else if (bh) hline(bx, y + h - 1, bw - 1, 1);
        if (hit == 2 && bw > 4) rect(bx + 1, y + h - bh + 1, bw - 3, 2, 0);
        else if (hit == 2) px(bx + 1, y + h - bh + 1, 0);
        if (base + i == kn) rect(bx, y + h + 2, bw - 1, 2, 1);
    }
    dots_h(x, y + h - (th * h + 127) / 255, cyc * bw - 1);
}

/* GRIDS: the map's 25 nodes (dotted between), the four X and Y read solid, the place X and Y read a dot on its
 * cross; the three lanes under it. LANES: the three lanes, each its name (inverted for the track in view's) and the
 * tracks that follow it */
static void grids_picture(void)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int X = PX0, Y0 = PY0;
    if (ui.sub == 2) {
        static const char *const N[3] = {"BD", "SD", "HH"};
        for (int l = 0; l < 3; l++) {
            int y = Y0 + l * 44, m = 0;
            for (int t = 0; t < M2K_TRACKS; t++) if (lane_of(p, t) == l + 1) m |= 1 << t;
            int tw = tag(X, y - TY, N[l], lane_of(p, ui.trk) == l + 1);
            char o[24];
            if (m) { s_tracks(o, m); text(&F_S, X + tw + 6, y, o + 1, 1, 0); }
            grids_lane(X, y + 14, 23, l);
        }
        return;
    }
    const int x8 = gr8(p->glob[G_GRX]), y8 = gr8(p->glob[G_GRY]), i0 = x8 >> 6, j0 = y8 >> 6;
    for (int j = 0; j < 5; j++) dots_h(X + 4, Y0 + 4 + 21 * j, 121);
    for (int i = 0; i < 5; i++) dots_v(X + 4 + 30 * i, Y0 + 4, 85);
    const int px0 = X + 4 + x8 * 30 / 64, py0 = Y0 + 4 + y8 * 21 / 64;
    hline(X + 4, py0, 121, 1);
    vline(px0, Y0 + 4, 85, 1);
    for (int j = 0; j < 5; j++)
        for (int i = 0; i < 5; i++) {
            int cx = X + 4 + 30 * i, cy = Y0 + 4 + 21 * j;
            if ((i == i0 || i == i0 + 1) && (j == j0 || j == j0 + 1)) rect(cx - 2, cy - 2, 5, 5, 1);
            else { rect(cx - 2, cy - 2, 5, 5, 0); frame(cx - 2, cy - 2, 5, 5, 1); }
        }
    rect(px0 - 4, py0 - 4, 9, 9, 0);
    rect(px0 - 3, py0 - 3, 7, 7, 1);
    for (int l = 0; l < 3; l++) grids_lane(X, Y0 + 98 + 12 * l, 7, l);
}

/* GLO > PLAY: the six tracks heard (solid) or muted (framed), one waiting for the bar blinking to what it will be; the
 * bar in play, its steps gone solid; when a mute comes */
static void play_picture(void)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int X = PX0, Y0 = PY0, fast = (ui.now >> 7) & 1;
    for (int i = 0; i < M2K_TRACKS; i++) {
        int x = X + (i % 3) * 44, y = Y0 + (i / 3) * 30, w = 41, b = 1 << i;
        int muted = p->mute & b, solo = sq.solo;
        if (fast && (sq.mq_m & b)) muted = sq.mq_v & b;
        if (fast && (sq.sq_m & b)) solo = (solo & ~b) | (sq.sq_v & b);
        if (fast && (sq.solo_rel & b)) solo |= b;
        int heard = !muted && (!solo || (solo & b));
        if (heard) rect(x, y, w, 26, 1); else if (muted) frame(x, y, w, 26, 1); else dots_frame(x, y, w, 26);
        text(&F_S, x + 3, y + 3, (const char[]){'T', (char)('1' + i), 0}, !heard, 0);
    }
    const int len = seq_len(), ps = sq.playing ? play_step() : -1, bar = ps < 0 ? 0 : ps / 16, bl = CLAMP(len - bar * 16, 1, 16);
    for (int i = 0; i < 16; i++) {
        int x = X + i * 8, y = Y0 + 64;
        if (i >= bl) px(x + 3, y + 5, 1);
        else if (ps >= 0 && i <= ps % 16) rect(x, y, 7, 11, 1);
        else frame(x, y, 7, 11, 1);
    }
    if (mute_pending() && sq.playing) {
        int nx = sq.step >= len ? 0 : sq.step, left = !(nx & 15) ? 0 : CLAMP(16 - (nx & 15), 0, len - nx);
        char o[20];
        if (left) s_cpy(s_int(s_cpy(o, "IN "), left), left == 1 ? " STEP" : " STEPS"); else s_cpy(o, "AT THE NEXT STEP");
        tag(X, Y0 + 84 - TY, "WAITS", fast);
        text(&F_S, X, Y0 + 99, o, 1, 0);
    }
    text(&F_S, X, L_B0, set_on(S_MUTQ) ? "AT ONCE" : "AT THE BAR", 1, 0);
}

static void picture(void)
{
    const int X = PX0, W = PIC_W, Y0 = PY0;
    trk_t *t = &trk[ui.trk];
    m2k_pattern_t *p = PAT(cur_pat);
    const uint8_t *k = p->kit[ui.trk];
    int m = p->machine[ui.trk];
    int fk = ui.focus_t && ui.now - ui.focus_t < 2500 ? ui.focus : -1;   /* the knob being turned */
    CELL(GX, STAGE_Y, 240, STEPS_R, PX0, STAGE_Y + 2, PX0 + PIC_W, STEPS_R - 1);
    switch (ui.page) {
    case PG_SYN:
    case PG_HOME: {
        if (snd_page()) {              /* the library: eight of the slots shown, the one picked inverted, each its category */
            int n = 0, at = snd_list(ui.snd_sel, &n), first = CLAMP(at - 4, 0, n > 9 ? n - 9 : 0);
            for (int r = 0; r < 9 && first + r < n; r++) {
                int i = snd_nth(first + r), y = Y0 + r * 14, on = i == ui.snd_sel;
                char o[4];
                s_num3(o, i + 1);
                if (on) rect(X, y - 1, W, F_S.base + 2, 1);
                text(&F_S, X + 2, y, o, !on, 0);
                int nx = X + 2 + text_w(&F_S, "000", 0) + 6;
                if (snd[i].name[0]) {
                    const char *c = CAT_S[snd_cat[i]];
                    int cw = text_w(&F_S, c, 0);
                    int e = nx + text_fit(nx, y, snd[i].name, 0, X + W - 2 - nx, !on);
                    if (e + 4 + cw <= X + W - 2) text(&F_S, X + W - 2 - cw, y, c, !on, 0);
                } else {
                    int iw = text(&F_S, nx, y, "INIT", !on, 0);
                    if (!on) dots_h(nx + iw + 4, y + 8, X + W - 2 - (nx + iw + 4));
                }
            }
            break;
        }
        if (ui.page == PG_SYN) {         /* the mark, one dot a machine (its place: the card's #12/19) */
            mark_3d(X + (W - ICON_N) / 2, Y0 + 26, m);
            machine_dots(X + W / 2, L_B0 + 4, m);
            break;
        }
        /* HOME, a step held: the track in the corner, its mark, the machine's name, the six tracks' meters */
        char tn[3] = {'T', (char)('1' + ui.trk), 0};
        tag(X, Y0 - TY, tn, 1);
        mark_3d(X + (W - ICON_N) / 2, Y0 + 2, m);
        const char *nm = MI[m].name;
        text_c(&F_S, X + W / 2, Y0 + 76, nm, 1, 0);
        for (int i = 0; i < M2K_TRACKS; i++) {           /* the six tracks' meters: a frame, the level solid in it */
            int x = X + i * 22, h = (ui.meter[i] * 40) >> 15;
            frame(x, Y0 + 90, 19, 44, 1);
            rect(x + 2, Y0 + 132 - h, 15, h, 1);
        }
        break;
    }
    case PG_AMP: {
        if (ui.sub == 1) {
            /* distortion as a transfer curve, the volume as a meter, the pan on a line, the glide as a ramp between two
             * notes; the part being turned is drawn heavy and its tag inverted */
            int32_t dh = (int32_t)T_DIST_H[k[P_DIST]];
            int32_t dc = T_DIST_C[k[P_DIST]], vol = T_VOL[k[P_VOL]];
            const int bs = 52, bx = X, by = Y0, mid = by + bs / 2;
            frame(bx, by, bs, bs, 1);
            for (int j = 4; j < bs; j += 4) px(bx + j, by + bs - 1 - j, 1);           /* the clean line */
            int prev = mid;
            for (int i = 0; i < bs - 2; i++) {
                int32_t in = (i - (bs - 2) / 2) * 65535 / (bs - 2);
                int64_t q = mul_sh(in, dh, 24);
                int32_t c = (int32_t)CLAMP(q, -dc, dc);                                /* the headroom and the clip */
                int32_t o = ((c * 128) / (T_DIST_C[64] >> 8)) * (vol >> 3) >> 12;   /* Q15, 32-bit division only */
                int y = mid - (int)((o * (bs / 2 - 2)) >> 15);
                if (i) { line(bx + i, prev, bx + 1 + i, y, 1); if (fk == 0) line(bx + i, prev + 1, bx + 1 + i, y + 1, 1); }
                prev = y;
            }
            int vx = X + bs + 8, vw = X + W - vx, vh = (bs - 4) * vol >> 15;
            frame(vx, by, vw, bs, 1);
            rect(vx + 2, by + bs - 2 - vh, vw - 4, vh, 1);
            tag(bx, by + bs + 4, "DS", fk == 0);
            tag(vx, by + bs + 4, "VL", fk == 1);
            int lx = X + 30, lw = W - 30, pan = k[P_PAN];
            int py = by + bs + 34;                                   /* (the pan's tag on it, the glide's under) */
            tag(X, py - 6, "PN", fk == 2);
            hline(lx, py, lw, 1);
            vline(lx + lw / 2, py - 3, 7, 1);
            vline(lx, py - 2, 5, 1); vline(lx + lw - 1, py - 2, 5, 1);
            int mx = lx + 2 + pan * (lw - 5) / 127;
            if (fk == 2) rect(mx - 2, py - 5, 5, 11, 1); else { rect(mx - 2, py - 5, 5, 11, 0); frame(mx - 2, py - 5, 5, 11, 1); }
            int gy = py + 34, gl = 2 + k[P_PORT] * (lw - 10) / 127;
            tag(X, gy - 11, "GL", fk == 3);
            hline(lx, gy, 4, 1);
            line(lx + 4, gy, lx + 4 + gl, gy - 9, 1);
            if (fk == 3) line(lx + 5, gy, lx + 5 + gl, gy - 9, 1);
            hline(lx + 4 + gl, gy - 9, lw - 4 - gl, 1);
            break;
        }
        /* attack, hold, decay, release as lengths on a log scale (a long decay does not crowd the attack out), the level
         * on the voice's own curves (env_level): the segment being turned solid and heavy, the others hatched; dotted
         * lines and handles where each ends, the 50 % level dotted, the letters under their segments, and the live
         * voice running on it, where its level is on the curve */
        int dec_inf = k[P_DEC] == 127;
        int wa = 6 + k[P_ATK] * 24 / 127, wh = k[P_HOLD] == 127 ? 18 : 2 + k[P_HOLD] * 18 / 127;
        int wd = dec_inf ? 18 : 12 + k[P_DEC] * 30 / 127, wr = 8 + k[P_REL] * 24 / 127;
        int tot = wa + wh + wd + wr;
        const int PW = W - 6;
        wa = wa * PW / tot; wh = wh * PW / tot; wd = wd * PW / tot; wr = PW - wa - wh - wd;
        int x0 = X + 3, yb = Y0 + 112, top = Y0 + 8, hgt = yb - top;
        int xs[5] = {x0, x0 + wa, x0 + wa + wh, x0 + wa + wh + wd, x0 + wa + wh + wd + wr};
        int ys[5] = {yb, yb, yb, yb, yb}, py = yb;
        dots_h(X, top + hgt / 2, W);                                /* half way up */
        for (int x = xs[0]; x <= xs[4] && x < X + W; x++) {
            int seg = x < xs[1] ? 0 : x < xs[2] ? 1 : x < xs[3] ? 2 : 3;
            int len = xs[seg + 1] - xs[seg];
            int q = (x - xs[seg]) * 256 / (len ? len : 1);
            int y = yb - hgt * env_level(seg, q, dec_inf, !k[P_ATK]) / 256;
            int tone = seg == fk ? PT_SOLID : PT_HATCH;
            for (int j = y + 1; j <= yb; j++) if (pat_ink(tone, x, j)) px(x, j, 1);
            line(x - 1, py, x, y, 1);
            line(x - 1, py - 1, x, y - 1, 1);                   /* two dots thick */
            if (seg == fk) line(x - 1, py - 2, x, y - 2, 1);    /* three for the one being turned */
            for (int b = 1; b < 5; b++) if (x == xs[b]) ys[b] = y;
            py = y;
        }
        ys[4] = yb;
        for (int b = 1; b < 4; b++) dots_v(xs[b], top - 4, ys[b] - top + 4);
        for (int b = 1; b < 5; b++) handle(xs[b], ys[b], b - 1 == fk);
        hline(X, yb + 1, W, 1);
        static const char *const L[4] = {"A", "H", "D", "R"};
        int lx = -100;
        for (int i = 0; i < 4; i++) {          /* under its segment; a letter that would touch the last is left out */
            int c = (xs[i] + xs[i + 1]) / 2, w = text_w(&F_S, L[i], 0), tw = tag_w(L[i]);
            if (i == fk) { int tx = CLAMP(c - tw / 2, X, X + W - tw); tag(tx, L_B0 - TY, L[i], 1); lx = tx + tw; }
            else if (c - w / 2 > lx + 3 && (fk < 0 || fk != i + 1 || c + w / 2 + 6 < (xs[fk] + xs[fk + 1]) / 2 - tw / 2)) {
                text(&F_S, c - w / 2, L_B0, L[i], 1, 0);
                lx = c + w / 2;
            }
        }
        int st = mon.astage[ui.trk], lv = mon.aenv[ui.trk];
        if (st) {
            int y = yb - (hgt * lv >> 15);
            /* along its segment as its level says: the attack's square root (ATK 0: straight), the decay's and the
             * release's halvings (two to a quarter, eight on) */
            int dn = lv ? (ilog2_q8(32768) - ilog2_q8((uint32_t)lv)) : 8 * 256;          /* halvings under full, Q8 */
            int ra = !k[P_ATK] ? lv : (int)(atk_from((uint32_t)lv << 15) >> 15);
            int x = st == 1 ? xs[0] + wa * ra / 32768 : st == 2 ? xs[1] + wh / 2
                  : st == 3 ? xs[2] + wd * CLAMP(dn, 0, 512) / 512
                  : xs[3] + wr * CLAMP(dn - (dec_inf ? 0 : 512), 0, 2048) / 2048;
            vline(x, top - 4, yb - top + 4, 2);
            disc(x, y, 3, 1);
        }
        break;
    }
    case PG_FILT: {
        const int FW = W - 9;          /* (the curve and its handles clear of the screen's edge, which the bezel hides) */
        int db = k[P_BOF] - 64, dw = k[P_WOF] - 64, ob = 7 * db * (db < 0 ? -db : db), ow = 7 * dw * (dw < 0 ? -dw : dw);
        int hp0 = filt_q3(ui.trk, k[P_BASE] << 8), lp0 = filt_q3(ui.trk, (k[P_BASE] + k[P_WDTH]) << 8);
        int hq = k[P_HPQ], lq = k[P_LPQ];
        /* one curve, never two: the band the knobs set, and while the track sounds where its envelope (and LFOs)
         * have it now, so it moves as it plays */
        int live = t->sounding && (mon.cut_hp[ui.trk] != hp0 || mon.cut_lp[ui.trk] != lp0);
        int hp = live ? mon.cut_hp[ui.trk] : hp0, lp = live ? mon.cut_lp[ui.trk] : lp0, yh, yl;
        if (ui.sub == 1) {
            /* the filter envelope, two blocks on equal gaps: the curve (while B-OF or W-OF turns, where the envelope
             * takes it), the envelope's reach a dotted line at each cutoff, BO and WO under them; then the envelope,
             * a straight attack up and a straight decay down (as the chain's), where it is now marked, ATK and DEC
             * under their segments */
            int hp1 = filt_q3(ui.trk, (k[P_BASE] << 8) + ob);
            int lp1 = filt_q3(ui.trk, ((k[P_BASE] + k[P_WDTH]) << 8) + ob + ow);
            if (fk == 2 || fk == 3) { hp = hp1; lp = lp1; }
            const int top = Y0 + 2, yb = Y0 + 48, ty = yb + 8;   /* the curve 66..112, its names 120..130 */
            filt_curve(X, FW, top, yb, hp, lp, hq, lq, fk >= 2 ? PT_VLINES : PT_HATCH, fk >= 2, &yh, &yl);
            int xh1 = X + hp1 * (FW - 1) / FQ_MAX, xl1 = X + lp1 * (FW - 1) / FQ_MAX;
            dots_v(xh1, top, yb - top + 1);
            dots_v(xl1, top, yb - top + 1);
            handle(X + hp * (FW - 1) / FQ_MAX, yh, fk == 2);
            handle(X + lp * (FW - 1) / FQ_MAX, yl, fk == 3);
            int wB = tag_w("BO"), wW = tag_w("WO");
            int tb = CLAMP(xh1 - wB / 2, X, X + W - wB - wW - 3), tw = CLAMP(xl1 - wW / 2, tb + wB + 3, X + W - wW);
            tag(tb, ty, "BO", fk == 2);
            tag(tw, ty, "WO", fk == 3);
            const int ex = X, ew = FW, et = ty + 21, eb = L_B0 - TY - 6;   /* the envelope 141..181, its line 182 */
            int a = 2 + k[P_FATK] * (ew / 3 - 2) / 127, d = 3 + k[P_FDEC] * (ew - a - 4) / 127, pe = eb;
            for (int x = 0; x <= a + d; x++) {
                int y = x <= a ? eb - (eb - et) * x / (a ? a : 1) : eb - (eb - et) * (a + d - x) / (d ? d : 1);
                int seg = x <= a ? 0 : 1;
                for (int j = y + 1; j <= eb; j++) if (pat_ink(seg == fk ? PT_SOLID : PT_HATCH, ex + x, j)) px(ex + x, j, 1);
                if (x) { line(ex + x - 1, pe, ex + x, y, 1); line(ex + x - 1, pe - 1, ex + x, y - 1, 1); }
                pe = y;
            }
            hline(ex, eb + 1, ew, 1);
            uint32_t fe = t->fenv >> 22;                /* (Q8 of full) */
            if (t->sounding && (t->fstage == 1 || fe)) {
                int x = ex + (t->fstage == 1 ? (int)(a * fe >> 8) : a + (int)(d * (256 - fe) >> 8));
                vline(x, et - 4, eb - et + 4, 2);
                disc(x, eb - (int)((eb - et) * fe >> 8), 3, 1);
            }
            int wA = tag_w("ATK"), wD = tag_w("DEC");
            int ta = CLAMP(ex + a / 2 - wA / 2, X, X + W - wA - wD - 3), td = CLAMP(ex + a + d / 2 - wD / 2, ta + wA + 3, X + W - wD);
            tag(ta, L_B0 - TY, "ATK", fk == 0);
            tag(td, L_B0 - TY, "DEC", fk == 1);
            break;
        }
        /* the band: the chain's response, its resonances as they peak; handles on each cutoff, the width a bracket
         * under the line; what is being turned is heavy */
        int top = Y0 + 10, yb = Y0 + 100;
        filt_curve(X, FW, top, yb, hp, lp, hq, lq, fk >= 0 ? PT_VLINES : PT_HATCH, fk >= 0, &yh, &yl);
        int xh = X + hp * (FW - 1) / FQ_MAX, xl = X + lp * (FW - 1) / FQ_MAX;
        dots_v(xh, top - 6, yb - top + 6);
        dots_v(xl, top - 6, yb - top + 6);
        int hot_h = fk == 0 || fk == 2, hot_l = fk == 3;
        handle(xh, yh, hot_h);
        handle(xl, yl, hot_l);
        /* the width */
        int wy = yb + 6;
        vline(xh, wy, 5, 1); vline(xl, wy, 5, 1);
        if (fk == 1) rect(xh, wy + 1, xl - xh + 1, 3, 1); else dots_h(xh, wy + 2, xl - xh + 1);
        /* the names, under their cutoffs and inside the picture, apart */
        int wH = tag_w("HP"), wL = tag_w("LP");
        int th = CLAMP(xh - wH / 2, X, X + W - wH - wL - 3), tl = CLAMP(xl - wL / 2, th + wH + 3, X + W - wL);
        tag(th, L_B0 - TY, "HP", hot_h);
        tag(tl, L_B0 - TY, "LP", hot_l);
        break;
    }
    case PG_FX: {
        if (ui.sub == 1) {             /* echoes: the width is half a bar; each lights when it sounds */
            int dt = k[P_DTIM] ? k[P_DTIM] : 1;
            int fb_ = k[P_DFB];                          /* 64 is unity */
            int lvl = 256;
            uint32_t ms_step = 15000u / seq_bpm();       /* a sixteenth in ms */
            uint32_t since = ui.now - ui.flash_t[ui.trk];
            const int base = Y0 + 112;
            for (int e = 0; e < 12; e++) {
                int x = X + 8 + (dt * (e + 1) * (W - 12)) / 128;
                if (x + 4 > X + W) break;
                int h = ((base - Y0 - 4) * lvl) >> 8;
                uint32_t at = (uint32_t)dt * (uint32_t)(e + 1) * ms_step / 16u;
                int lit = since >= at && since < at + 90;
                if (lit) rect(x, base - h, 4, h, 1); else frame(x, base - h, 4, h, 1);
                lvl = CLAMP((lvl * fb_) >> 6, 0, 256);
                if (lvl < 8) break;
            }
            rect(X, Y0 + 4, 4, base - Y0 - 4, 1);        /* the sound itself */
            hline(X, base + 1, W, 1);
            static const char *const DN[2] = {"DRY", "ECHOES"};
            text(&F_S, X, L_B0, DN[0], 1, 0);
            text_r(&F_S, X + W, L_B0, DN[1], 1, 0);
        } else if (ui.sub == 2) {      /* a sine through this track's destruction */
            int bits = 16 - (k[P_BITS] * 14 >> 7), fold = k[P_FOLD], xm = (k[P_XOR] * k[P_XOR] * 2) & 0x7FFF;
            int srr = k[P_SRR], step = (128 - srr) * (128 - srr) * 4, acc = 0, hold = 0, prev = 0;
            uint32_t ph = (uint32_t)ui.now * 2000000u;
            const int mid = Y0 + 56;
            for (int i = 0; i < W; i++) {
                int32_t s = sinp(ph + (uint32_t)i * (0xFFFFFFFFu / W) * 2) * 7 / 8;
                if (fold) s = fold16((s * (256 + fold * 6)) >> 8);
                if (xm) s = (int16_t)((uint16_t)s ^ (uint16_t)xm);
                if (bits < 16) s = (s >> (16 - bits)) * (1 << (16 - bits));   /* a multiply: no left shift of a negative */
                acc += step;
                if (acc >= 65536 || i == 0) { acc -= 65536; hold = s; }
                if (srr) s = hold;
                int y = mid - (s * 50 >> 15);
                if (i) { line(X + i - 1, prev, X + i, y, 1); line(X + i - 1, prev + 1, X + i, y + 1, 1); }
                prev = y;
            }
            dots_h(X, mid, W);
            static const char *const DN[4] = {"BITS", "FOLD", "XOR", "VERB"};
            if (fk >= 0) tag(X, L_B0 - TY, DN[fk], 1);
        } else if (ui.sub == 3) {      /* the modulation: its sweep at its rate, as deep as DPTH, and how much of it */
            int type = (k[P_MODT] * 3) >> 7, dep = k[P_MODD], mix = k[P_MODM];
            uint32_t ph = (uint32_t)ui.now * (T_MOD_INC[k[P_MODR]] * 44u);
            int mid = Y0 + 56, amp = 3 + dep * 46 / 127, prev = mid;
            for (int i = 0; i < W; i++) {
                int32_t tri = (int32_t)((ph + (uint32_t)i * (0xFFFFFFFFu / W)) >> 16);
                tri = tri < 32768 ? tri : 65535 - tri;
                int y = mid + amp - (tri * amp * 2 >> 15);
                for (int j = y; j <= mid + amp; j++) if (pat_ink(PT_HATCH, X + i, j)) px(X + i, j, 1);
                if (i) { line(X + i - 1, prev, X + i, y, 1); line(X + i - 1, prev - 1, X + i, y - 1, 1); }
                prev = y;
            }
            hline(X, mid + amp + 1, W, 1);
            static const char *const N[3] = {"CHOR", "FLNG", "PHAS"};
            int bx = X + tag(X, L_B0 - TY, N[type], 1) + 6, bw = X + W - bx;
            level(bx, L_B0 + 4, bw, 0, 0, mix * 256 * bw / 127, 0);
        } else {                       /* EQ: the band and its gain; the rate reduction as steps under it */
            int f = k[P_EQF] * (W - 1) / 127, g = k[P_EQG] - 64, prev = 0;
            const int mid = Y0 + 48;
            for (int i = 0; i < W; i++) {
                int d = i - f;
                int bw = W / 4, bump = d > -bw && d < bw ? (g * (bw - (d < 0 ? -d : d))) / bw : 0;
                int y = mid - bump * 42 / 64;
                for (int j = (y < mid ? y : mid); j <= (y < mid ? mid : y); j++) if (pat_ink(PT_HATCH, X + i, j)) px(X + i, j, 1);
                if (i) { line(X + i - 1, prev, X + i, y, 1); line(X + i - 1, prev + (y < mid ? -1 : 1), X + i, y + (y < mid ? -1 : 1), 1); }
                prev = y;
            }
            dots_h(X, mid, W);
            int n = 4 + (127 - k[P_SRR]) * 40 / 127, sy = Y0 + 104, pq = -1, pyy = sy;
            for (int i = 0; i < W; i++) {
                int q = (i * n / W) * W / n;
                int y = sy - (sinp((uint32_t)q * (0xFFFFFFFFu / W)) * 8 >> 15);
                if (q != pq && i) vline(X + i, y < pyy ? y : pyy, (y < pyy ? pyy - y : y - pyy) + 1, 1);
                px(X + i, y, 1);
                pq = q; pyy = y;
            }
            text(&F_S, X, L_B0, "EQ", 1, 0);
            text_r(&F_S, X + W, L_B0, "RATE", 1, 0);
        }
        break;
    }
    case PG_LFO: lfo_scopes(k); break;
    case PG_ARP: {
        if (ui.sub) { grids_picture(); break; }
        int n = sq.nheld, rng = 1 + ((k[P_ARNG] * 4) >> 7), tot = n * rng, cw = (W - 7 * 3) / 8;
        for (int i = 0; i < tot && i < 24; i++) {
            int x = X + (i % 8) * (cw + 3), y = Y0 + 98 - (i / 8) * 30 - (i % 8) * 2;
            int on = (int)(sq.arp_i ? (sq.arp_i - 1) % (tot ? tot : 1) : -1) == i;
            if (on) rect(x, y, cw, 9, 1); else frame(x, y, cw, 9, 1);
        }
        if (sq.arp_hold) tag(X, Y0 - TY, "HOLD", 1);       /* the arp's HOLD: the notes outlast the keys */
        if (!n) { text_c(&F_S, X + W / 2, Y0 + 44, sq.arp_hold ? "PLAY" : "HOLD", 1, 0); text_c(&F_S, X + W / 2, Y0 + 58, "SOME KEYS", 1, 0); }
        break;
    }
    case PG_GLO: {
        if (ui.sub == 3) {             /* the project picked, the open one; the display disc */
            char o[16] = {'P', (char)('1' + ui.proj_sel), 0};
            text(&F_L, X, Y0, o, 1, 0);
            text_fit(X, Y0 + 28, m2k_project_name(ui.proj_sel), 0, W, 1);
            if (ui.proj_sel == cur_proj) tag(X - 2, Y0 + 44 - TY, "OPEN", 1);
            int cx = X + W - 16, cy = Y0 + 110, r = 12;
            for (int j = -r; j <= r; j++)
                for (int i = -r; i <= r; i++) {
                    int d = i * i + j * j;
                    if (d > r * r) continue;
                    if (d > (r - 2) * (r - 2) || i >= 0) px(cx + i, cy + j, 1);
                    else if (T_BAYER[(cy + j) & 7][(cx + i) & 7] < 8) px(cx + i, cy + j, 1);
                }
        } else if (ui.sub == 1) {      /* the master, and the repeat when it bites */
            int prev = 0;
            const int mid = Y0 + 56;
            for (int i = 0; i < W; i++) {
                int v = mon.scope[(uint8_t)(mon.scope_w - W * 2 + i * 2)];
                int y = mid - (v * 50 >> 15);
                if (i) line(X + i - 1, prev, X + i, y, 1);
                prev = y;
            }
            dots_h(X, mid, W);
            static const char *const GM_N[GM_COUNT] = {"REPEAT", "REVERSE", "GATED", "OCTAVE UP", "OCTAVE DOWN", "ROLL",
                                                       "SLOWING", "STOP", "FREEZE", "OCTAVE UP", "OCTAVE DOWN", "SCATTER",
                                                       "ROLL", "SPIN"};
            tag(X, L_B0 - TY, GM_N[mon.gmode < GM_COUNT ? mon.gmode : 0], mon.glitch);
        } else if (ui.sub == 2) {      /* the decay: its level over time (0.1 .. 30 s on a log scale), 60 dB down the
                                        * height; the bottom's solid, the top's dotted (DAMP's share of it) */
            int v = p->glob[G_VERB];
            const int top = Y0 + 6, ty = Y0 + 112, dh = ty - top;
            uint32_t t60 = T_RV_MS[v], th = (t60 * 256u) / (256u + T_RV_DA[p->glob[G_DAMP]]);
            hline(X, ty, W, 1);
            static const char *const TN[3] = {"0.1", "1 S", "10 S"};
            for (int s = 0; s < 3; s++) {
                int tx = X + s * 4037 * (W - 1) / 10000;
                vline(tx, ty, 4, 1);
                int w = text_w(&F_S, TN[s], 0);
                text(&F_S, CLAMP(tx - w / 2, X, X + W - w), L_B0, TN[s], 1, 0);
            }
            int lp = -1;
            for (int i = 0; v && i < W; i++) {
                /* the time at column i: 100 ms 2^(i / (W - 1) log2 300), in ms */
                uint32_t e = (uint32_t)i * 2105u / (uint32_t)(W - 1), ms = (100u * T_EXP2F[e & 255]) >> (16 - (e >> 8));
                int yl = (int)(ms * (uint32_t)dh / (t60 ? t60 : 1)), yh = (int)(ms * (uint32_t)dh / (th ? th : 1));
                if (yl <= dh) {
                    int y = top + yl;
                    if (lp >= 0) { line(X + i - 1, lp, X + i, y, 1); line(X + i - 1, lp + 1, X + i, y + 1, 1); } else px(X + i, y, 1);
                    lp = y;
                }
                if (yh <= dh && !((X + i + top + yh) & 1)) px(X + i, top + yh, 1);
            }
            if (!v) text_c(&F_S, X + W / 2, Y0 + 44, "OFF", 1, 0);
        } else if (ui.sub == 4) {      /* MIDI: the six tracks on their channels, the clock in and out, the beat */
            int ch = (store_.set[S_MCH] * 16) >> 7;
            for (int i = 0; i < M2K_TRACKS; i++) {
                int x = X + (i % 3) * 44, y = Y0 + (i / 3) * 30, w = 41;
                char o[4];
                s_int(o, (ch + i) % 16 + 1);
                if (i == ui.trk) rect(x, y, w, 26, 1); else frame(x, y, w, 26, 1);
                text(&F_S, x + 3, y + 3, (const char[]){'T', (char)('1' + i), 0}, i != ui.trk, 0);
                text_r(&F_S, x + w - 3, y + 13, o, i != ui.trk, 0);
            }
            int beat = sq.playing && (sq.step & 3) == 1;
            const int on_in = clk_from() != 0, on_out = store_.set[S_SYNO] >= 64;
            tag(X - 2, Y0 + 64 - TY, "CLOCK IN", on_in);
            tag(X - 2, Y0 + 79 - TY, "CLOCK OUT", on_out);
            tag(X - 2, Y0 + 94 - TY, "NOTES OUT", store_.set[S_NOTO] >= 64);
            int cx = X + W - 12, cy = Y0 + 82;
            if (beat) rect(cx - 10, cy - 10, 21, 21, 1); else frame(cx - 10, cy - 10, 21, 21, 1);
            char o[12];
            s_int(o, (int)seq_bpm());
            text_c(&F_M, X + W / 2, Y0 + 110, o, 1, 0);
        } else if (ui.sub == 6) {      /* PLAY: the mutes and when they come */
            play_picture();
        } else if (ui.sub == 5) {      /* IN: what each port feeds, its square lit while it brings something */
            const int clk = clk_from(), nts = notes_from();
            for (int s = 0; s < 2; s++) {
                int x = X + s * 66, b = 1 << s, w = text(&F_S, x, Y0 + 4, s ? "TRS" : "USB", 1, 0);
                if ((mx.in_seen & b) && mx.now - mx.in_at[s] < M2K_SR / 8u) rect(x + w + 6, Y0 + 5, 8, 8, 1);
                else frame(x + w + 6, Y0 + 5, 8, 8, 1);
                tag(x - 2, Y0 + 24 - TY, "CLOCK", clk & b);
                tag(x - 2, Y0 + 39 - TY, "NOTES", nts & b);
                if (s) tag(x - 2, Y0 + 54 - TY, "THRU", set_on(S_THRU));
            }
        } else {                       /* four beats, the tempo */
            int beat = sq.playing ? (sq.step ? sq.step - 1 : 0) / 4 % 4 : -1, bw = (W - 3 * 3) / 4;
            for (int i = 0; i < 4; i++) {
                int x = X + i * (bw + 3);
                if (i == beat) rect(x, Y0 + 4, bw, 48, 1);
                else frame(x, Y0 + 4, bw, 48, 1);
            }
            char o[12];
            s_int(o, (int)seq_bpm());
            text_c(&F_L, X + W / 2, Y0 + 66, o, 1, 0);
            text_c(&F_S, X + W / 2, L_B0, "BPM", 1, 0);
        }
        break;
    }
    default:
        break;
    }
}

/* the LFO pages: the three LFOs stacked, each its number (inverted for the one the knobs turn), where it goes and how
 * fast; its wave over a cycle as deep as DPTH swings it (heavy for the one the knobs turn, flat and dotted when it goes
 * nowhere), and live: a cursor where its phase is, a block on the value it sends now. (A scope of its past would
 * alias: a 1/16 LFO turns faster than the FM-1 draws) */
static void lfo_scopes(const uint8_t *k)
{
    const int X = PX0, W = PIC_W, el = ui.sub >> 1;
    const trk_t *tk = &trk[ui.trk];
    for (int l = 0; l < 3; l++) {
        const uint8_t *b = &k[P_LFO + 8 * l];
        int y = PY0 + l * 44, on = l == el;
        char n[2] = {(char)('1' + l), 0}, d[12], r[20];
        int tw = tag(X, y - TY, n, on);
        const char *u;
        fmt(&PI_LFO[8 * l + L_DEST], ui.trk, b[L_DEST], d, &u);
        s_lfo_cycle(r, &u, b[L_SPD], choice_of(K_MULT, b[L_MULT]));
        if (*u && u[0] != 'N' && u[0] != 'S') {          /* bars, beats and hertz say so; a note is its fraction */
            char *e = r;
            while (*e) e++;
            *e++ = ' ';
            s_cpyn(e, u, (int)(r + sizeof r - e));
        }
        int rw = text_w(&F_S, r, 0);
        if (rw > W - tw - 54) { s_lfo_cycle(r, &u, b[L_SPD], choice_of(K_MULT, b[L_MULT])); rw = text_w(&F_S, r, 0); }
        text(&F_S, X + W - rw, y, r, 1, 0);
        text_fit(X + tw + 6, y, d, 0, W - tw - 6 - rw - 8, 1);
        const int mid = y + 26, A = 11;
        if (!b[L_DEST]) { dots_h(X, mid, W); continue; }
        int wave = (b[L_WAVE] * 7) >> 7;
        int32_t dp = T_LDPTH[b[L_DPTH]], rr = 0, prev = mid;
        for (int i = 0; i < W; i++) {
            int32_t v = wave == 6 ? (int32_t)(hash32((uint32_t)(i / 16) + 77u) >> 16) - 32768
                                  : lfo_shape(wave, (uint32_t)i * (0xFFFFFFFFu / (uint32_t)(W - 1)), &rr);
            int yy = mid - (int)((((v * dp) >> 14) * A) >> 15);
            for (int j = yy < mid ? yy + 1 : mid + 1; j < (yy < mid ? mid : yy); j++) if (!((X + i + j) & 1)) px(X + i, j, 1);
            if (i) { line(X + i - 1, prev, X + i, yy, 1); if (on) line(X + i - 1, prev + 1, X + i, yy + 1, 1); }
            prev = yy;
        }
        int cx = X + (int)(((tk->lph[l] >> 16) * (uint32_t)(W - 1)) >> 16);
        int cy = mid - (int)((((tk->lval[l] * dp) >> 14) * A) >> 15);
        dots_v(cx, mid - A - 2, 2 * A + 5);
        hline(X, mid, W, 1);
        rect(cx - 2, cy - 2, 5, 5, 1);
    }
}

/* ---- the pages */

/* the windows' system (docs/screen-grid.md, Windows): x 3-235 (its shadow to 237), from y 64 (its shadow no lower than
 * 201); paper, a 1 px frame, a 2 px shadow; a title bar inside the frame, ink, 14 rows, its title in the bars' type
 * (paper) 5 px in, a mark (LATCHED) at its right; the content 5 px in from the frame (4 blank), its first row 5 under
 * the bar, rows W_ROW apart (a tag's 11 and 4); pairs a true table (pairs()); nothing within 3 px of the frame */
#define W_X 3
#define W_W 233
#define W_IN 5
#define W_BAR 14
#define W_ROW 15
#define W_TOP (1 + W_BAR + 5)
#define W_H(rows) (W_TOP + (rows) * W_ROW + 2)   /* (the last row's 11, 5 blank, the frame) */

/* a window and its title bar; the first row's y */
static int window(int x, int y, int w, int h, const char *title, const char *mark)
{
    win(x, y, w, h);
    rect(x + 1, y + 1, w - 2, W_BAR, 1);
    int mw = mark ? text_w(&F_B, mark, 0) + 8 : 0;
    text_fit_f(&F_B, x + W_IN, y + 4, title, w - 2 * W_IN - mw, 0);
    if (mark) text_r(&F_B, x + w - W_IN, y + 4, mark, 0, 0);
    return y + W_TOP;
}

static const char *latch_title(int b) { return ui.latch == b + 1 ? "LATCHED" : 0; }   /* (a title bar's mark) */

/* a table of n key -> action pairs (kv: key, action, ...) in cols columns, filled row by row, from (x, y), w wide:
 * the keys' tags one width (the widest's and 2 each side), their type centred, the actions 4 after and aligned; a
 * column's gutter 7. Its rows */
static int pairs(int x, int y, int w, const char *const *kv, int n, int cols)
{
    int kt = 0;
    for (int i = 0; i < n; i++) { int t = text_w(&F_S, kv[2 * i], 0) + 4; if (t > kt) kt = t; }
    const int cw = cols > 1 ? (w - 7 * (cols - 1)) / cols : w;
    for (int i = 0; i < n; i++) {
        int cx = x + (i % cols) * (cw + 7), cy = y + (i / cols) * W_ROW;
        rect(cx, cy, kt, F_S.base + 2, 1);
        text(&F_S, cx + (kt - text_w(&F_S, kv[2 * i], 0)) / 2, cy + TY, kv[2 * i], 0, 0);
        text_fit(cx + kt + 4, cy + TY, kv[2 * i + 1], 0, cw - kt - 4, 1);
    }
    return (n + cols - 1) / cols;
}

/* a knob turned on HOME: its value in big type in a window over the lanes */
static void focus_popup(int code)
{
    const pi_t *pi = pinfo(ui.trk, code);
    char v[16];
    const char *u;
    fmt(pi, ui.trk, knob_value(code), v, &u);
    const int x = 30, y = STAGE_Y + 14, w = 180, h = 88;
    int r = window(x, y, w, h, pi->name, pi->s4);
    int b = big_value(x + W_IN, r, w - 2 * W_IN, v, value_font(pi, code), ui.now - ui.roll_t);
    if (*u) text_fit(x + W_IN, b + 8, u, 0, w - 2 * W_IN, 1);
}

static void home(void)
{
    m2k_pattern_t *p = PAT(cur_pat);
    int fcode = knob_at(ui.focus), fresh = ui.focus_t && ui.now - ui.focus_t < 1800 && fcode != NONE;   /* (a layer let go) */
    if (ui.grid && ui.nheld && !fresh) {         /* a held step: its card and the track's picture, as on a page */
        value_cell();
        rule_v(GX, STAGE_Y, STEPS_R - STAGE_Y, 0);
        step_card();
        picture();
    } else {
        /* the six lanes, the page in view, grouped by the beat as the steps below are */
        CELL(0, STAGE_Y, 240, STEPS_R, 4, STAGE_Y + 2, 237, STEPS_R - 1);
        for (int t = 0; t < M2K_TRACKS; t++) {
            int y = PY0 + t * 22;
            int flash = ui.now - ui.flash_t[t] < 90, inv = t == ui.trk || flash;
            if ((((sq.mq_m | sq.sq_m | sq.solo_rel) >> t) & 1) && ((ui.now >> 7) & 1)) inv = !inv;   /* waits for the bar */
            char n[2] = {(char)('1' + t), 0};
            if (inv) rect(0, y, 14, 16, 1);
            text_c(&F_S, 8, y + 3, n, !inv, 0);
            int muted = (p->mute >> t) & 1;
            for (int i = 0; i < 16; i++) {
                int s = ui.spage * 16 + i, x = 18 + (i >> 2) * 56 + (i & 3) * 13;
                if (muted) {
                    if (p->step[t][s].flags & ST_TRIG) dots_frame(x, y, 11, 16); else px(x + 5, y + 8, 1);
                    continue;
                }
                step_cell(x, y, 11, 16, t, s);
            }
            if (muted) dots_h(18, y + 8, 218);
        }
        if (fresh) focus_popup(fcode);
    }
    /* the four bars, wide, a beat to 12 px: the one in view solid, its beats and the playhead in paper */
    rule_h(0, STEPS_R, 240, 1);
    for (int pg = 0; pg < 4; pg++) {
        int x = pg * 60 + 6, w = 48, y = STEP_Y;
        int used = pg * 16 < seq_len();
        if (!used) { dots_frame(x, y, w, 11); continue; }
        int on = pg == ui.spage;
        if (on) rect(x, y, w, 11, 1); else frame(x, y, w, 11, 1);
        for (int i = 1; i < 4; i++) vline(x + i * 12, y + 7, 3, !on);
        if (sq.playing && play_step() / 16 == pg) rect(x + (play_step() % 16) * 3, y, 3, 11, !on);
        rect(x + 22, STEP_Y + 14, 3, 2, 1);
    }
    knobs();
    foot();
}

static void param_page(void)
{
    /* the knob turned last, while its value shows; one a layer gave (PERFORM's, SEQ's) and the page has not is none */
    int fcode = knob_at(ui.focus), fresh = ui.focus_t && ui.now - ui.focus_t < 2500 && fcode != NONE;
    value_cell();
    rule_v(GX, STAGE_Y, STEPS_R - STAGE_Y, 0);          /* the stage's two cells apart, from the knobs' rule to the steps' */
    if (ui.grid && ui.nheld && !fresh) step_card();
    else if (ui.page == PG_SYN && (!fresh || (ui.card_t && ui.now - ui.card_t < 1500))) machine_card();
    else if (fresh) focus_left(fcode, ui.focus);
    else page_rest();
    picture();
    strip();
    knobs();
    foot();
}

/* ---- the splash (2.1 s, then the wipe into HOME, 0.4 s; a key, a button or a knob skips to the wipe). One surface of
 * MONO2K's own shapes in dithered 3D on ink, morphing the whole time, its height eased from each shape into the next
 * (no front, no cut): the wordmark's letters extruded over it, GND-SIN's dome, the wavefolds (DSTR's, folded more to
 * the right), the wave bank as a waterfall (a wave a row, rolling), and back to the letters' relief as the camera,
 * swinging the while, rises over it and the slab narrows to the logotype's band; the light turns with it. The relief
 * resolves into the wordmark, and the type comes in. Drawn front to back with a floating horizon (each pixel once), each strip lit by its slope, the light through the
 * Bayer matrix a byte at a time (T_BMASK). Integer only; about the heaviest page's cost a frame (host/m2k_draw.c) */
static inline int logo_bit(const uint8_t *b, int x, int y) { return (b[y * ((LOGO_W + 7) >> 3) + (x >> 3)] >> (7 - (x & 7))) & 1; }

/* t through [t0, t1] eased (3s^2 - 2s^3), Q8 */
static int ease8(int t, int t0, int t1)
{
    int s = t <= t0 ? 0 : t >= t1 ? 256 : (t - t0) * 256 / (t1 - t0);
    return (s * s * (768 - 2 * s)) >> 16;
}

/* the slab's shapes at (u, q) (Q8 px about its middle, q away from the viewer), Q8 px: 0 GND-SIN's dome (breathing),
 * 1 the wavefolds, 2 the wave bank, 3 the wordmark's relief, 4 the wordmark stretched three times over the slab's depth
 * (the opening). What the frame fixes (the breath, the travel) is in sp, with the shape going (a), the one coming (b)
 * and how far the surface has morphed from one to the other (w, 0..256, eased: everywhere at once) */
typedef struct { uint8_t a, b; int32_t w, breath; uint32_t fph; int32_t wrow; } slab_t;

static inline __attribute__((always_inline)) int32_t slab_shape(int k, int32_t u, int32_t q, const slab_t *sp)
{
    switch (k) {
    case 0: {
        int32_t a = 6400 - ((u >> 8) * (u >> 8) + (q >> 8) * (q >> 8));    /* (80 px) */
        if (a <= 0) return 0;
        int32_t b = (a * 41) >> 11;                                       /* (a / 50) */
        return ((b * b) >> 7) * sp->breath;
    }
    case 1: {                                      /* a triangle sweeping across, folded more to the right */
        uint32_t ph = (uint32_t)(u * 4 + q) * 140000u + sp->fph;
        int32_t s = (int32_t)(ph >> 17);
        s = 2 * ((s < 16384 ? s : 32767 - s) - 8192);                       /* +-16384 */
        int32_t g = 300 + ((u + 30000) >> 7);      /* x1.2 at the left .. x2.0 at the right */
        return fold16((s * g) >> 7) * 11 >> 7;
    }
    case 2: {                                      /* a wave every 8 px of depth, rolling in, each blended to the next */
        int32_t r = (q >> 3) + sp->wrow;           /* Q8 rows of 8 px */
        int w0 = (r >> 8) & 63, f = r & 255, i = ((((u >> 8) + 116) * 9) >> 3) & 255;
        int32_t a = T_WAVES64[w0 * 256 + i], b = T_WAVES64[((w0 + 1) & 63) * 256 + i];
        return (a + (((b - a) * f) >> 8)) * 16 >> 7;
    }
    default: {
        int lx = (u >> 8) + LOGO_W / 2, ly = LOGO_H / 2 - 1 - (k == 4 ? (q >> 8) / 3 : q >> 8);
        return lx >= 0 && lx < LOGO_W && ly >= 0 && ly < LOGO_H && logo_bit(LOGO_BITS, lx, ly) ? (k == 4 ? 18 : 14) * 256 : 0;
    }
    }
}

/* the height there: the two shapes' heights blended by the morph (one worked out where it is all of it) */
static inline __attribute__((always_inline)) int32_t slab_h(int32_t u, int32_t q, const slab_t *sp)
{
    if (!sp->w) return slab_shape(sp->a, u, q, sp);
    int32_t hb = slab_shape(sp->b, u, q, sp);
    if (sp->w == 256) return hb;
    int32_t ha = slab_shape(sp->a, u, q, sp);
    return ha + (((hb - ha) * sp->w) >> 8);
}

/* rows y0..y1 of the column pair at x (even), lit l (0..64): paper where the Bayer matrix is under l, on the ink the
 * splash is drawn on (each pixel is drawn once: only the paper is set; under 4 the strip is ink, nothing to do); its
 * rows to y0 + n0 - 1 lit l0 instead (the waterfall's line on its strip, one pass) */
static inline void slab_run(int x, int y0, int y1, int l, int n0, int l0)
{
    if (y1 > M2K_H - 1) y1 = M2K_H - 1;
    if (y0 < 0) { n0 += y0; y0 = 0; }
    if (y0 > y1) return;
    const uint8_t pm = (uint8_t)(0xC0u >> (x & 7));
    uint8_t *b = &fb[y0 * ROWB + (x >> 3)], *e = b + (y1 - y0 + 1) * ROWB;
    int k = y0 & 7;
    if (n0 > 0) {
        const uint8_t *m = T_BMASK[CLAMP(l0, 0, 64)];
        for (uint8_t *e0 = n0 < y1 - y0 + 1 ? b + n0 * ROWB : e; b < e0; b += ROWB, k = (k + 1) & 7) *b &= (uint8_t)~(m[k] & pm);
    }
    if (l < 4) return;
    if (l >= 64) { for (; b < e; b += ROWB) *b &= (uint8_t)~pm; return; }
    const uint8_t *m = T_BMASK[l];
    for (; b < e; b += ROWB, k = (k + 1) & 7) *b &= (uint8_t)~(m[k] & pm);
}

/* the columns c where a + c d lies within +-lim (a row's u or q, Q8): [*lo, *hi] narrowed to them, exactly (the
 * samples are not tested again) */
static void slab_span(int32_t a, int32_t d, int32_t lim, int *lo, int *hi)
{
    if (!d) { if (a < -lim || a > lim) *hi = -1; return; }
    if (d < 0) { a = -a; d = -d; }                     /* (the same columns: -lim <= -a - c d <= lim) */
    int32_t n0 = -lim - a, n1 = lim - a;               /* d c in [n0, n1] */
    int32_t c0 = n0 >= 0 ? (n0 + d - 1) / d : -((-n0) / d), c1 = n1 >= 0 ? n1 / d : -((-n1 + d - 1) / d);
    if (c0 > *lo) *lo = (int)c0;
    if (c1 < *hi) *hi = (int)c1;
}

#ifndef MORPH_CUT
#define MORPH_CUT 64                    /* (of 256: the rows a morph's middle gives up) */
#endif
static void slab(int t, int fade)
{
    /* the turn and the camera's elevation (Q14 sine and cosine): one swing, 30 degrees to -16 and home to 0 as it rises
     * over the relief; the slab's half depth; the light's direction, turning with them */
    int fold = ease8(t, 1150, 1700);
    int yaw = (30 * 256 - 46 * ease8(t, 0, 1350)) * (256 - fold) >> 8;          /* Q8 degrees */
    int elev = 38 * 256 + 6 * ease8(t, 0, 1150) + 46 * fold;
    int la = ease8(t, 0, 1700);                                                 /* the light from the left, to the front */
    const int LX = 20 - (8 * la >> 8), LZ = 8 + (10 * la >> 8);
    uint32_t ay = (uint32_t)((int32_t)yaw * 46603), ae = (uint32_t)((int32_t)elev * 46603);   /* (2^32 / 360 / 256) */
    int32_t sy = sinp(ay) >> 1, cy = sinp(ay + 0x40000000u) >> 1, se = sinp(ae) >> 1, ce = sinp(ae + 0x40000000u) >> 1;
    const int32_t PU = 116 << 8, PQ = (116 << 8) - (98 << 8) * fold / 256;
    slab_t sp;
    /* the morphs: the wordmark, the waves, the dome, the folds, the wordmark again, each into the next from MS[k] to
     * ME[k] ms (eased), a short hold between (the shapes keep moving: the waves' roll, the breath, the folds' travel) */
    static const int16_t MS[4] = {120, 540, 920, 1300}, ME[4] = {440, 840, 1220, 1700};
    static const uint8_t SHAPE[5] = {4, 2, 0, 1, 3};   /* (the waves and the folds never blended: the dearest pair) */
    int k = 0;
    while (k < 3 && t >= MS[k + 1]) k++;
    sp.a = SHAPE[k];
    sp.b = SHAPE[k + 1];
    sp.w = ease8(t, MS[k], ME[k]);
    if (sp.w < 12) sp.w = 0;                            /* (a morph's first and last 5 %: one shape, a sample cheaper) */
    if (sp.w > 244) { sp.a = sp.b; sp.w = 0; }
    const int ww = (sp.a == 2 ? 256 - sp.w : 0) + (sp.b == 2 && sp.a != 2 ? sp.w : 0);   /* the wave bank's share */
    const int wv = ww > 128 ? (ww - 128) * 2 : 0;      /* its lines: only while it is most of the surface (they are dear
                                                        * drawn down a tall dome) */
    sp.breath = 72 + (sinp((uint32_t)t * 2900000u) >> 11);
    sp.fph = 0u - (uint32_t)t * 7000000u;                               /* (unsigned: the phase wraps) */
    sp.wrow = t << 2;                                                    /* (16 waves a second) */
    /* the rows, front first: fewer as the slab narrows into the logotype's band (a pixel apart at the end), and a
     * quarter fewer while a morph works out two shapes a sample (the FM-1's budget), eased in over its first 6 % */
    const int act = sp.w ? CLAMP(12 * (sp.w < 128 ? sp.w : 256 - sp.w), 0, 256) : 0, nr0 = 24 + 14 * (PQ - (18 << 8)) / (98 << 8);   /* (act 0..256) */
    const int NR = nr0 - ((nr0 * act * MORPH_CUT) >> 16), T = 12 * ce >> 14, yc = 128 - 8 * fold / 256;
    uint8_t hz[M2K_W / 2];
    int16_t hp[M2K_W / 2];
    m_set(hz, 0xFF, sizeof hz);
    for (int r = 0; r < NR; r++) {
        int32_t z = -PQ + 2 * PQ * r / (NR - 1);                       /* Q8 */
        int32_t k12 = (360 << 12) / (360 + ((z * ce) >> 22));          /* the perspective: 360 px off */
        int32_t dx = (1 << 20) / k12;                                  /* Q8 plane px a screen px */
        int32_t x0 = -120 * dx;
        int32_t u0 = (x0 * cy - z * sy) >> 14, q0 = (x0 * sy + z * cy) >> 14;
        int32_t du = (2 * dx * cy) >> 14, dq = (2 * dx * sy) >> 14;
        int32_t zs = (z * se) >> 14;
        int32_t rx = (1 << 16) / (2 * dx), rz = (1 << 16) / (2 * PQ / (NR - 1) + 1);
        int lo = 0, hi = M2K_W / 2 - 1;
        slab_span(u0, du, PU, &lo, &hi);
        slab_span(q0, dq, PQ, &lo, &hi);
        int32_t hl = 0, u = u0 + lo * du, q = q0 + lo * dq;
        int in = 0, yl = 0x7FFF;
        for (int c = lo; c <= hi; c++, u += du, q += dq) {
            int32_t h = slab_h(u, q, &sp), hpc = hp[c], hlc = hl;
            int y = yc - (int)(((zs + ((h * ce) >> 14)) * k12) >> 20), x = c * 2, first = hz[c] == 0xFF, was = in;
            hl = h;
            in = 1;
            hp[c] = (int16_t)h;                                         /* (+-60 px, Q8: a halfword) */
            int ly = yl == 0x7FFF ? y : yl;                             /* (the crest a column before) */
            yl = y;
            if (first) {                                                /* the slab's edge: its side, down T */
                slab_run(x, y, y + T, (8 * fade) >> 8, 0, 0);
                hz[c] = (uint8_t)CLAMP(y, 0, 239);
                continue;
            }
            if (y >= hz[c]) continue;                                   /* behind what is drawn */
            /* the slope across (rising to the right faces the light, from the left) and away (facing the viewer),
             * through a soft knee (tanh: T_SAT); the waterfall's strips dark under their lit crests */
            int32_t gx = was ? ((h - hlc) * rx) >> 8 : 0, gz = ((h - hpc) * rz) >> 8;
            int32_t lt = (T_SAT[CLAMP(((gz * LZ + gx * LX) >> 7) + 128, 0, 256)] * 42) >> 15;   /* 42 tanh(x / 4096) */
            int l = ((14 + lt) * (256 - ww * 7 / 8) >> 8) * fade >> 8, wl = (wv * fade) >> 10;   /* (the lines: 64 at full) */
            if (wl) {                                                   /* the waterfall's lines, joined where seen */
                if (ly < y) slab_run(x, ly, y - 1, wl, 0, 0);
                slab_run(x, y, hz[c] - 1, l, (ly > y ? ly - y : 0) + 1, wl);
            } else slab_run(x, y, hz[c] - 1, l, 0, 0);
            hz[c] = (uint8_t)(y < 0 ? 0 : y);
        }
    }
}

/* the boot logotype resolved, white on ink: its outline paper, inside a Bayer dither from sparse to solid left to
 * right, drifting slowly (sines out of step); through the matrix at level `in` (another phase of it) over what is
 * there. A screen byte at a time: the logotype's 232 columns from x 4, half a byte off */
static void wordmark(int t, int in)
{
    const int y0 = 120 - LOGO_H / 2, BPR = (LOGO_W + 7) >> 3;
    uint8_t dm[ROWB];
    for (int i = 0; i < ROWB; i++) {                                    /* the density about each byte's middle */
        int x = CLAMP(8 * i, 0, LOGO_W - 1);
        int32_t a = sinp((uint32_t)x * 41700000u + (uint32_t)t * 321000u) >> 8;
        int32_t b2 = sinp((uint32_t)x * 11600000u - (uint32_t)t * 212000u) >> 8;
        dm[i] = (uint8_t)CLAMP(26 + LOGO_DENS[x] * 5 / 8 + ((a * b2 * 7) >> 14), 0, 64);   /* (fuller, on ink) */
    }
    for (int y = 0; y < LOGO_H; y++) {
        int sy = y0 + y;
        const uint8_t *lb = LOGO_BITS + y * BPR, *le = LOGO_EDGE + y * BPR;
        uint8_t *o = &fb[sy * ROWB];
        const uint8_t inm = T_BMASK[in][(sy + 3) & 7];
        for (int i = 0; i < ROWB; i++) {
            unsigned bits = (((i ? lb[i - 1] : 0u) << 4) | (i < BPR ? lb[i] >> 4 : 0u)) & 0xFFu;
            unsigned edge = (((i ? le[i - 1] : 0u) << 4) | (i < BPR ? le[i] >> 4 : 0u)) & 0xFFu;
            uint8_t lit = (uint8_t)(bits & (edge | T_BMASK[dm[i]][sy & 7]));
            o[i] = (uint8_t)((o[i] & ~inm) | (~lit & inm));
        }
    }
}

static void splash(int t)
{
    m_set(fb, 0xFF, sizeof fb);                                         /* ink */
    int fade = ease8(t, 0, 320) * (256 - ease8(t, 1820, 1960)) >> 8;   /* (the relief fades as the wordmark comes) */
    if (t < 1960) slab(t, fade);
    if (t >= 1640) wordmark(t, ease8(t, 1640, 1880) >> 2);    /* (the relief dissolves into it: no cut) */
    /* the type comes in */
    if (t >= 1760) {
        static const char sub[] = "SIX MONO MACHINES";
        int n = CLAMP((t - 1760) / 14, 0, (int)sizeof sub - 1);
        char o[24];
        m_cpy(o, sub, (uint32_t)n);
        o[n] = 0;
        text(&F_S, 4, 120 + LOGO_H / 2 + 12, o, TX_PAPER, 2);
    }
    if (t >= 1880) {
        text(&F_S, 4, 6, "FM-1", TX_PAPER, 0);
        if (M2K_VER[0]) text_r(&F_S, 236, 6, "V" M2K_VER, TX_PAPER, 0);
        text(&F_S, 4, 222, "LOFI 2000", TX_PAPER, 0);
        text_r(&F_S, 236, 222, "6 X 64", TX_PAPER, 0);
    }
}

/* out of the splash into HOME: a dithered front sweeps down and across (the old screen through the Bayer matrix as
 * it passes), the rows at the front slipping a byte; p 0..256 */
static void wipe(int p)
{
    int front = -60 + p * 380 / 256;                                    /* along (x + 2 y) / 3 */
    for (int y = 0; y < M2K_H; y++) {
        const uint8_t *a = &fb_prev[y * ROWB], *b = &fb[y * ROWB];
        uint8_t *o = &fb_out[y * ROWB];
        int d = front - (2 * y) / 3, sl = 0;
        if (d > -8 && d < 40 && (hash32((uint32_t)(y >> 2) * 7919u + (uint32_t)(p >> 5)) & 3u) == 0) sl = (y >> 2) & 1 ? 1 : -1;
        for (int i = 0; i < ROWB; i++) {
            int l = CLAMP((d - (8 * i + 4) / 3) * 2 + 32, 0, 64);
            int j = CLAMP(i - sl, 0, ROWB - 1);
            uint8_t m = T_BMASK[l][y & 7];
            o[i] = (uint8_t)((b[i] & m) | (a[j] & ~m));
        }
    }
}

/* ---- the windows over the stage (window(), pairs() above): from y 64 */
#define WY 64

/* GLO's white keys drawn, 15 of 12 x 14 a 14 px pitch from x (208 px), y the labels' first row: FILL and ALL over their
 * keys, MUTE, TAP and SOLO under theirs (no label crosses another). The mute keys show the tracks (heard solid, muted
 * framed, soloed away a dot; one waiting for the bar blinks between), the solo keys solid while held, FILL solid
 * while one is queued. Its height, 38 */
static int white_keys(int x, int y)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int fast = (ui.now >> 7) & 1, ky = y + 12;
    for (int i = 0; i < 15; i++) {
        int bx = x + i * 14, on = 0, dot = 0;
        if (i < M2K_TRACKS) {
            const int b = 1 << i;
            int muted = p->mute & b, solo = sq.solo;
            if (fast && (sq.mq_m & b)) muted = sq.mq_v & b;
            if (fast && (sq.sq_m & b)) solo = (solo & ~b) | (sq.sq_v & b);
            if (fast && (sq.solo_rel & b)) solo |= b;
            on = !muted && (!solo || (solo & b));
            dot = !muted && !on;
        } else if (i >= 8 && i < 8 + M2K_TRACKS) on = (sq.solo >> (i - 8)) & 1;
        else if (i == 6) on = sq.fill_next;
        if (on) rect(bx, ky, 12, 14, 1); else frame(bx, ky, 12, 14, 1);
        if (dot) rect(bx + 5, ky + 6, 2, 2, 1);
    }
    text_c(&F_S, x + 6 * 14 + 6, y, "FILL", 1, 0);
    text_r(&F_S, x + 14 * 14 + 12, y, "ALL", 1, 0);
    text_c(&F_S, x + (5 * 14 + 12) / 2, ky + 17, "MUTE", 1, 0);
    text_c(&F_S, x + 7 * 14 + 6, ky + 17, "TAP", 1, 0);
    text_c(&F_S, x + 8 * 14 + (5 * 14 + 12) / 2, ky + 17, "SOLO", 1, 0);
    return 38;
}

/* GLO held a moment alone: what it does with each, in a window over the page */
static void glo_help(void)
{
    if (ui.latch != B_GLO + 1 && (!ui.btn[B_GLO] || !ui.glo_t0 || (!ui.glo_used && ui.now - (ui.glo_t0 & ~1u) < 350))) return;
    if (ring.t0 && !ring.done) return;                 /* the ring has the stage */
    const int x = W_X, y = WY, w = W_W, lat = ui.latch == B_GLO + 1;
    if (lat) {                                         /* latched: its keys (the combos want it held) */
        int r = window(x, y, w, W_TOP + 38 + 5 + 9 + 7, "GLO KEYS", "LATCHED");
        r += white_keys(x + 12, r) + 5;
        text(&F_S, x + W_IN, r, "TAP GLO TO LET GO", 1, 0);
        return;
    }
    static const char *const L[16] = {"REC", "COPY", "PLAY", "PASTE", "SEQ", "CLEAR", "SAVE", "RELOAD",
                                      "OCT-", "UNDO", "OCT+", "REDO", "EDIT", "SOUNDS", "HOME", "KEYS"};
    int r = window(x, y, w, W_TOP + 4 * W_ROW + 2 + 38 + 7, "GLO HELD", "SELECT: BPM");
    r += pairs(x + W_IN, r, w - 2 * W_IN, L, 8, 2) * W_ROW + 2;
    white_keys(x + 12, r);
}

/* HOME held a moment: the bank's 16 patterns as the white keys pick them, a cell each (the open one inverted, those
 * picked a square at its end, INIT ones dotted), the one picked last (or the open one) named over them; EDIT held: what it
 * does */
static void hold_help(void)
{
    const int lat = ui.latch == B_HOME + 1;
    if (held(B_GLO) || (!lat && (!ui.hold_t0 || ui.now - (ui.hold_t0 & ~1u) < 350))) return;
    const int x = W_X, y = WY, w = W_W;
    if (held(B_HOME)) {
        int f = sq.playing ? sq.queued : cur_pat;      /* (a pick opens or queues its pattern) */
        char a[24], *e = s_pat(a, f);
        *e++ = ' ';
        s_cpy(e, pat_name(f));
        static const char *const P[6] = {"SEQ", "CLEAR", "REC", "COPY", "PLAY", "PASTE"};
        int r = window(x, y, w, W_TOP + 4 * W_ROW + 1 + (lat ? 13 + 9 + 6 : W_H(2) - W_TOP), a,
                       lat ? "LATCHED" : "BLACK 1-4: BANKS");
        for (int i = 0; i < 16; i++) {                 /* a cell a key: its slot, an INIT one dotted */
            int n = ui.pbank * 16 + i, cw = 52, ch = 13, cx = x + W_IN + (i & 3) * (cw + 5), cy = r + (i >> 2) * W_ROW;
            if (n >= M2K_PATTERNS) continue;
            int on = n == cur_pat, picked = ui.pick[n >> 3] >> (n & 7) & 1, blank = name_is(pat_name(n), "INIT");
            if (on) rect(cx, cy, cw, ch, 1);
            else if (blank && !picked) dots_frame(cx, cy, cw, ch);
            else frame(cx, cy, cw, ch, 1);
            if (picked) rect(cx + cw - 10, cy + 4, 6, 6, !on);
            char num[8];
            s_pat(num, n);
            text(&F_S, cx + 4, cy + 2, num, !on, 0);
        }
        r += 4 * W_ROW + 1;
        if (lat) { text(&F_S, x + W_IN, r, "AGAIN: AT ONCE", 1, 0); text(&F_S, x + W_IN, r + 13, "TAP HOME TO LET GO", 1, 0); }
        else pairs(x + W_IN, r, w - 2 * W_IN, P, 3, 2);
        return;
    }
    if (ui.btn[B_EDIT] && !ui.hold_used) {
        static const char *const L[8] = {"SEQ", "INIT THE SOUND", "SAVE", "INTO THE LIBRARY", "REC", "COPY THE SOUND",
                                         "PLAY", "PASTE IT"};
        int r = window(x, y, w, W_H(4), "EDIT HELD", 0);
        pairs(x + W_IN, r, w - 2 * W_IN, L, 4, 1);
    }
}

/* SEQ held a moment: its tools on the white keys (the knobs above show LEN, SPEED, SWING, TEMPO meanwhile) */
static void seq_help(void)
{
    if (!seq_layer() || (ui.latch != B_SEQ + 1 && (!ui.seq_t0 || ui.now - (ui.seq_t0 & ~1u) < 350))) return;
    const int x = W_X, y = WY, w = W_W, lat = ui.latch == B_SEQ + 1;
    char o[16] = {'S', 'E', 'Q', ' ', 'T', (char)('1' + ui.trk), 0};
    s_cpy(o + 6, " STEPS");
    static const char *const P[4] = {"REC", "COPY", "PLAY", "PASTE"};
    int r = window(x, y, w, lat ? W_TOP + 4 * W_ROW + 9 + 6 : W_H(5), o, latch_title(B_SEQ));
    const char *kv[16];
    static const char N[8][2] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    for (int i = 0; i < 8; i++) { kv[2 * i] = N[(i & 1) * 4 + (i >> 1)]; kv[2 * i + 1] = SEQ_TOOL[(i & 1) * 4 + (i >> 1)]; }
    r += pairs(x + W_IN, r, w - 2 * W_IN, kv, 8, 2) * W_ROW;   /* (1-4 down the left, 5-8 the right) */
    if (lat) text(&F_S, x + W_IN, r, "TAP SEQ TO LET GO", 1, 0);
    else pairs(x + W_IN, r, w - 2 * W_IN, P, 2, 2);
}

/* FX held a moment: PERFORM's effects on the black keys, the bus's knobs; LFO held: the knobs snap back */
static void lay_help(void)
{
    int i = perf_layer() ? 0 : mom_layer() ? 1 : -1;
    if (i < 0 || ((i || ui.latch != B_FX + 1) && (!ui.lay_t0[i] || (!ui.lay_used[i] && ui.now - (ui.lay_t0[i] & ~1u) < 350)))) return;
    const int y = WY;
    if (i) {
        static const char *const P[2] = {"OCT+", "KEEPS THEM"};
        int r = window(W_X, y, W_W, W_H(2), "LFO HELD", 0);
        text(&F_S, W_X + W_IN, r + TY, "THE KNOBS SNAP BACK", 1, 0);
        pairs(W_X + W_IN, r + W_ROW, W_W - 2 * W_IN, P, 1, 1);
        return;
    }
    /* the bank shown, its eleven black keys a table (1-6 down the left, 7-11 the right), an effect on in inverse;
     * under it both banks on OCT- and OCT+, the one shown in inverse */
    const int x = W_X + W_IN, w = W_W - 2 * W_IN, cw = (w - 7) / 2;
    int r = window(W_X, y, W_W, W_H(7), ui.pfb ? "PERFORM: BANK B" : "PERFORM: BANK A", latch_title(B_FX));
    static const char N[PF_BANK][3] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"};
    const char *kv[2 * PF_BANK];                       /* (filled row by row: item j is key j / 2 or 7 + j / 2) */
    int kt = 0;
    for (int j = 0; j < PF_BANK; j++) {
        int s = (j & 1) ? 6 + (j >> 1) : j >> 1, k = s + ui.pfb * PF_BANK;
        kv[2 * j] = N[s];
        kv[2 * j + 1] = perf.on[k] ? "" : PF_NAME[k];
        if (text_w(&F_S, N[s], 0) + 4 > kt) kt = text_w(&F_S, N[s], 0) + 4;   /* (the table's tag width) */
    }
    pairs(x, r, w, kv, PF_BANK, 2);
    for (int j = 0; j < PF_BANK; j++) {                /* the effects on: their names in inverse */
        int s = (j & 1) ? 6 + (j >> 1) : j >> 1, k = s + ui.pfb * PF_BANK;
        if (!perf.on[k]) continue;
        int ax = x + (j & 1) * (cw + 7) + kt + 4, cy = r + (j >> 1) * W_ROW;
        rect(ax - 2, cy, text_w(&F_S, PF_NAME[k], 0) + 4, F_S.base + 2, 1);
        text(&F_S, ax, cy + TY, PF_NAME[k], 0, 0);
    }
    static const char *const B[4] = {"OCT-", "BANK A", "OCT+", "BANK B"};
    int by = r + 6 * W_ROW, bt = text_w(&F_S, "OCT-", 0) > text_w(&F_S, "OCT+", 0) ? text_w(&F_S, "OCT-", 0) + 4 : text_w(&F_S, "OCT+", 0) + 4;
    pairs(x, by, w, B, 2, 2);
    int ax = x + ui.pfb * (cw + 7) + bt + 4;
    rect(ax - 2, by, text_w(&F_S, B[2 * ui.pfb + 1], 0) + 4, F_S.base + 2, 1);
    text(&F_S, ax, by + TY, B[2 * ui.pfb + 1], 0, 0);
}

/* a button's card (card_up): its tap, then what it does with the others, a row each */
static void card_draw(void)
{
    if (!card_up()) return;
    static const char *const C_SEL[] = {"THE FILTER", "TAP", "ITS PAGES IN TURN", "STEP +", "A KNOB LOCKS IT",
                                        "OCT- +", "A KNOB TO DEFAULT", 0};
    static const char *const C_ENV[] = {"THE AMP", "TAP", "ITS PAGES IN TURN", "STEP +", "A KNOB LOCKS IT",
                                        "OCT- +", "A KNOB TO DEFAULT", 0};
    static const char *const C_ARP[] = {"AND GRIDS", "TAP", "ARP, GRIDS, LANES", "TWICE", "HOLD: NOTES STAY", "GLO +",
                                        "PADS: KEYS T1-T6", "OCT+", "LANES TO STEPS", 0};
    static const char *const C_PLAY[] = {"AND PASTE", "TAP", "PLAY, STOP", "GLO +", "THE PATTERN",
                                         "HOME +", "ON THE PICKED", "EDIT +", "THE SOUND", "SEQ +", "THE STEPS",
                                         "STEP +", "ONTO IT", 0};
    static const char *const C_OCTD[] = {"AND NO", "TAP", "OCTAVE, GRID PAGE", "+ KNOB", "ITS DEFAULT",
                                         "STEP +", "KNOB: LOCK OFF", "GLO +", "UNDO", "ASKED", "NO", 0};
    static const char *const C_OCTU[] = {"AND YES", "TAP", "OCTAVE, GRID PAGE", "GLO +", "REDO", "LFO +",
                                         "KEEP THE KNOBS", "ASKED", "YES", 0};
    int b = ui.card - 1;
    const char *const *c = b == B_SEL ? C_SEL : b == B_ENV ? C_ENV : b == B_ARP ? C_ARP : b == B_PLAY ? C_PLAY
                         : b == B_OCTD ? C_OCTD : C_OCTU;
    int rows = 0;
    while (c[1 + 2 * rows]) rows++;
    char t[24];
    s_cpy(s_cpy(s_cpy(t, BTN_N[b]), " "), c[0]);
    int r = window(W_X, WY, W_W, W_H(rows), t, 0);    /* over the stage, whatever the page has there */
    pairs(W_X + W_IN, r, W_W - 2 * W_IN, c + 1, rows, 1);
}

/* a clear held on: a ring of twelve fills in a second, then INIT */
static void ring_draw(void)
{
    if (!ring.t0 || ring.done) return;
    uint32_t a = ui.now - (ring.t0 & ~1u);
    if (a < 150) return;
    const int w = 120, h = 104, x = 120 - w / 2, y = STAGE_Y + 14, cx = 120, cy = y + W_TOP + 34;
    window(x, y, w, h, "HOLD: INIT", 0);
    static const int8_t RX[12] = {0, 14, 24, 28, 24, 14, 0, -14, -24, -28, -24, -14};
    static const int8_t RY[12] = {-28, -24, -14, 0, 14, 24, 28, 24, 14, 0, -14, -24};
    int n = (int)(a > 1000 ? 12 : a * 12 / 1000);
    for (int i = 0; i < 12; i++) {
        int px0 = cx + RX[i] - 4, py0 = cy + RY[i] - 4;
        if (i < n) rect(px0, py0, 8, 8, 1); else frame(px0, py0, 8, 8, 1);
    }
}

static void ask_draw(void)
{
    if (!ask.kind) return;
    if (ui.now - (ask.t & ~1u) > 8000) { ask.kind = ASK_NONE; return; }
    const int x = 20, y = STAGE_Y + 24, w = 200;
    char t[16];
    s_cpy(s_cpy(t, ask.a), " ?");
    int r = window(x, y, w, W_H(2), t, 0);
    text_fit(x + W_IN, r + TY, ask.b, 0, w - 2 * W_IN, 1);
    static const char *const P[4] = {"OCT-", "NO", "OCT+", "YES"};
    pairs(x + W_IN, r + W_ROW, w - 2 * W_IN, P, 2, 2);
}

/* a message takes the header for a moment: inverted, its text, a gradient tail (a long one the line under it too) */
static void overlays(void)
{
    if (!ui.toast_t) return;
    uint32_t a = ui.now - ui.toast_t;
    /* on the title's line; what does not fit there goes on a second, and stays a little longer to be read */
    int two = ui.toast2[0] && 4 + text_w(&F_S, ui.toast, 0) + 8 + text_w(&F_S, ui.toast2, 0) > 236, hgt = two ? 31 : HEAD_H;
    if (a >= (two ? 2200u : 1200u)) return;
    int w = a < 120 ? (int)a * 240 / 120 : 240;      /* it sweeps in */
    rect(0, 0, w, hgt, 0);
    CELL(0, 0, 240, hgt, 3, 0, 237, hgt - 1);
    clip(0, 0, w, hgt);
    int x = 4 + text(&F_S, 4, 3, ui.toast, 1, 0) + 8;   /* (the capitals 3..11, a comma's foot 3 clear of the rule) */
    if (two) text(&F_S, 4, 18, ui.toast2, 1, 0);
    else if (ui.toast2[0]) x += text(&F_S, x, 3, ui.toast2, 1, 0) + 8;
    noclip();
    if (w == 240 && x < 236) rect_grad(x, 0, 240 - x, HEAD_H, 0, 64, 0);
    hline(0, hgt, w, 1);
}

static uint8_t fb_show[M2K_FB_BYTES];

static const uint8_t *present(void)
{
    if (store_.set[S_SCREEN] < 64) return fb_out;
    for (int i = 0; i < M2K_FB_BYTES; i++) fb_show[i] = (uint8_t)~fb_out[i];
    return fb_show;
}

void m2k_render_ui_tick(void);

const uint8_t *m2k_frame(uint32_t ms)
{
    uint32_t dt = ui.now ? ms - ui.now : 16;
    ui.fdt = (uint16_t)(dt > 1000u ? 1000u : dt);   /* (the motions by the time gone, not the frames drawn) */
    if (ui.boot_t0 == 0xFFFFFFFFu) ui.boot_t0 = ms;
    ui.now = ms;
    if (sq.full) { sq.full = 0; toast("PATTERNS FULL", "THE NEXT DID NOT OPEN"); }
    /* what the audio did */
    int boost = 0;
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (mon.trig[t] != ui.seen_trig[t]) {
            ui.seen_trig[t] = mon.trig[t];
            ui.flash_t[t] = ms;
            if (t == ui.trk) boost = 1;
        }
        int32_t pk = mon.peak[t] * 2;
        mon.peak[t] = 0;
        ui.meter[t] = pk > ui.meter[t] ? CLAMP(pk, 0, 32767)
                    : ui.meter[t] - ui.meter[t] * (ui.fdt > 132 ? 132 : ui.fdt) / 264;   /* an eighth a 33 ms (2^15 by 132: a word) */
    }
    /* the mark: in time with the music while it plays (a loop a bar), kicked on by each of the track's trigs; slow
     * when stopped */
    if (boost) { ui.icon_spd = 30 << 8; ui.icon_hit = ms | 1u; ui.icon_amp = (uint8_t)(ui.meter[ui.trk] >> 9); }
    int32_t ibase = sq.playing ? (int32_t)(((int32_t)seq_bpm() << 8) / 20) : 6 << 8;
    /* SWAV-SAW's teeth: the level on a log scale (-36 dB the lowest of 8), each tooth three frames behind the one
     * before; up at once, down a level every frame, two, three: the back falls first, the front lingers */
    ui.mh = (uint8_t)((ui.mh + 1) & 15);
    ui.mhist[ui.mh] = ui.meter[ui.trk];
    for (int k = 0; k < 3; k++) {
        int32_t mv = ui.mhist[(ui.mh - 3 * k) & 15];
        int l = CLAMP(((ilog2_q8((uint32_t)mv | 1u) - 9 * 256) * 8) / (6 * 256), 0, 7);
        if (l >= ui.saw_lv[k]) { ui.saw_lv[k] = (uint8_t)l; ui.saw_fall[k] = 0; }
        else if (++ui.saw_fall[k] > k) { ui.saw_fall[k] = 0; ui.saw_lv[k]--; }
    }
    ui.icon_spd += (ibase - ui.icon_spd) >> 3;
    ui.icon_ph += (int32_t)((ui.icon_spd * (int32_t)dt) / 1000);
    mon.lfo_view = ui.trk;
    syn_sub_check();                    /* (a queued pattern opened: the track's machine may be another) */
    if (sq.playing && ui.follow && !ui.nheld) ui.spage = (uint8_t)(track_pos(ui.trk) / 16);   /* (its own step, off 1x) */
    if (ui.rec_t0 && ui.btn[B_REC] && !ui.rec_used && ms - ui.rec_t0 >= 300) sq.fill = 1;
    if (ui.pkey && !ui.pfired && ms - ui.pkey_t0 >= 600 && ui.grid && ui.keys[ui.pkey_k]) { ui.pfired = 1; page_length(ui.pkey - 1); }
    if (ui.save_t0 && ui.btn[B_SAVE] && !ui.save_fired && ms - ui.save_t0 >= 700) {
        int slot = ui.page == PG_GLO && ui.sub == 3 ? ui.proj_sel : cur_proj;
        ui.save_fired = 1;
        char o[16] = {'P', (char)('1' + slot), ' ', 0};
        s_cpy(o + 3, "SAVED");
        if (project_save(slot)) { toast("PROJECT", o); dirty = 1; ui.save_t = ms | 1u; }
        else toast("PROJECT FULL", "NOT SAVED");
    }

    uint32_t bt = ms - ui.boot_t0;
    if (bt < ui.splash_end) {
        splash((int)bt);
        m_cpy(fb_out, fb, sizeof fb_out);
        return present();
    }
    if (!ui.splashed) { ui.splashed = 1; start_trans_any(WIPE_MS, 0); ui.trans_wipe = 1; }   /* out of the splash */

    cls();
    if (ui.page == PG_HOME) { header(); home(); }
    else { header(); param_page(); }
    glo_help();
    if (held(B_HOME) || ui.btn[B_EDIT]) hold_help();
    seq_help();
    lay_help();
    card_draw();
    ring_tick();
    ring_draw();
    ask_draw();
    overlays();

    if (ui.in_trans) {
        uint32_t a = ms - ui.trans_t;
        if (a >= ui.trans_len) { ui.in_trans = 0; ui.trans_wipe = 0; m_cpy(fb_out, fb, sizeof fb_out); }
        else if (ui.trans_wipe) wipe((int)(a * 256 / ui.trans_len));
        else cyber((int)(a * 256 / ui.trans_len), ui.glitch_seed ^ ui.trans_t, ui.trans_down);
    } else {
        m_cpy(fb_out, fb, sizeof fb_out);
    }
    if (ui.glitch_t && ms - ui.glitch_t < 120) glitch(fb_out, ms - ui.glitch_t, ui.glitch_seed, ui.glitch_amt);
    return present();
}

const uint8_t *m2k_leds(void)
{
    m2k_pattern_t *p = PAT(cur_pat);
    uint8_t *L = ui.leds;
    m_set(L, 0, sizeof ui.leds);
    int ps = sq.playing ? play_step() : -1, tp = sq.playing ? track_pos(ui.trk) : -1;   /* the pattern's, the track's */
    int blink = (ui.now >> 8) & 1, fast = (ui.now >> 7) & 1;
    /* a layer's keys answer (on the FM-1 a key is lit from 64): GLO the mutes (lit heard), fill, the beat, the solos;
     * PERFORM the effects on; SEQ its eight tools; HOME the bank's patterns (lit used, the open one winking, the
     * queued blinking) and the banks */
    int lay = held(B_GLO) ? 1 : perf_layer() ? 2 : seq_layer() ? 3 : held(B_HOME) ? 4 : 0;
    for (int k = 0; lay && k < M2K_KEYS; k++) {
        int w = ui.kw[k], b = ui.kb[k], v = 0;
        if (lay == 1 && w >= 0) {
            if (w < M2K_TRACKS) {          /* (a mute waiting for the bar: blinking) */
                int on = !((p->mute >> w) & 1), heard = on && (!sq.solo || ((sq.solo >> w) & 1));
                v = (sq.mq_m >> w) & 1 ? (fast ? 255 : 0) : !on ? 0 : !heard ? 40 : ui.now - ui.flash_t[w] < 80 ? 255 : 200;
            } else if (w == 6) v = sq.fill_next ? (fast ? 255 : 40) : 16;
            else if (w == 7) v = ps >= 0 ? ((ps & 3) == 0 ? 255 : 16) : 16;
            else if (w >= 8 && w < 8 + M2K_TRACKS)
                v = ((sq.sq_m | sq.solo_rel) >> (w - 8)) & 1 ? (fast ? 255 : 0) : (sq.solo >> (w - 8)) & 1 ? 255 : 16;
            else if (w == 14) v = mute_pending() ? (fast ? 255 : 16) : p->mute || sq.solo ? 90 : 16;
        } else if (lay == 2) {
            if (b >= 0 && b < PF_BANK) v = perf.on[b + ui.pfb * PF_BANK] ? 255 : ui.kact[k] == KA_PERF ? 90 : 24;   /* (90: the other bank's, held) */
            else if (w >= 0) { int note = 53 + k + 12 * ui.oct[ui.trk]; v = ui.keys[k] ? 255 : trk[ui.trk].sounding && trk[ui.trk].note == note ? 120 : 0; }
        } else if (lay == 3) {
            if (w >= 0 && w < 8) v = 200;
        } else if (lay == 4) {
            if (w >= 0 && ui.pbank * 16 + w < M2K_PATTERNS) {
                int n = ui.pbank * 16 + w;
                if (n == cur_pat) v = (ui.now & 1023) < 900 ? 255 : 0;
                else if (sq.playing && sq.queued == n) v = blink ? 255 : 0;
                else if (ui.pick[n >> 3] >> (n & 7) & 1) v = 200;
                else v = name_is(pat_name(n), "INIT") ? 0 : 90;
            } else if (b >= 0 && b < M2K_BANKS) v = b == ui.pbank ? 255 : b == cur_pat / 16 ? 90 : 16;
        }
        L[k] = (uint8_t)v;
    }
    for (int k = 0; !lay && k < M2K_KEYS; k++) {
        int w = ui.kw[k], b = ui.kb[k], v = 0;
        if (ui.grid) {
            if (w >= 0) {
                int s = ui.spage * 16 + w;
                if (s < seq_len()) {
                    if (!(w & 3)) v = 14;      /* the beats' landmarks, faint (off on the FM-1) */
                    if (p->step[ui.trk][s].flags & ST_TRIG) v = 110;
                    if (s == tp) v = 255;
                    for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == s) v = ((ui.now >> 7) & 1) ? 255 : 40;
                }
            } else if (b < M2K_TRACKS) {
                v = b == ui.trk ? 255 : (ui.now - ui.flash_t[b] < 80) ? 160 : 24;
            } else if (b < 10) {
                v = b - 6 == ui.spage ? 255 : (tp >= 0 && tp / 16 == b - 6) ? 90 : 16;
            } else {
                v = (sq.mq_m >> ui.trk) & 1 ? (((ui.now >> 7) & 1) ? 255 : 16) : (p->mute >> ui.trk) & 1 ? 255 : 16;
            }
        } else {
            int note = 53 + k + 12 * ui.oct[ui.trk];
            if (ui.keys[k]) v = 255;
            else if (trk[ui.trk].sounding && trk[ui.trk].note == note) v = 120;
        }
        L[k] = (uint8_t)v;
    }
    uint8_t *B = L + M2K_KEYS;
    B[B_FX] = ui.page == PG_FX ? 255 : 0;
    B[B_SEL] = ui.page == PG_FILT ? 255 : 0;
    B[B_ENV] = ui.page == PG_AMP ? 255 : 0;
    B[B_LFO] = ui.page == PG_LFO ? 255 : 0;
    B[B_EDIT] = ui.page == PG_SYN ? 255 : 0;
    B[B_GLO] = ui.page == PG_GLO ? 255 : 0;
    B[B_HOME] = ui.page == PG_HOME ? 255 : 0;
    B[B_ARP] = sq.arp_hold ? (blink ? 255 : 90) : ui.page == PG_ARP ? 255 : 0;   /* HOLD: it pulses */
    B[B_SAVE] = ui.save_t && ui.now - ui.save_t < 300 ? 255 : 0;
    B[B_SEQ] = ui.grid ? 255 : 0;
    B[B_PLAY] = sq.playing ? ((play_step() & 3) == 0 ? 255 : 70) : sq.ready && blink ? 255 : 0;   /* Ready: blinks */
    B[B_REC] = sq.rec ? 255 : 0;
    B[B_OCTD] = lay == 2 ? (ui.pfb ? 24 : 255) : ui.oct[ui.trk] < 0 ? 200 : 0;   /* PERFORM: the bank shown lit */
    B[B_OCTU] = lay == 2 ? (ui.pfb ? 255 : 24) : ui.oct[ui.trk] > 0 ? 200 : 0;
    if (ui.latch) B[ui.latch - 1] = blink ? 255 : 0;   /* a latched layer's button blinks */
    return L;
}
