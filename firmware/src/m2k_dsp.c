/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's tracks: a machine, then the Monomachine's chain: HP and LP filters (base / width), amp envelope with
 * distortion, a parametric EQ band, sample rate reduction, the destroy stage (bits, fold, xor), pan, and sends to
 * the track's delay and the bus reverb. Machines are clean-room designs after what is published about the
 * Monomachine's machine families, not ports of its code. Control rate: every M2K_SUB samples. */

typedef struct { int32_t s1, s2, g, k, d; } svf_t;

static void svf_set(svf_t *f, int32_t q3, int32_t r12)
{
    q3 = CLAMP(q3, 0, 1016);
    int32_t g = T_CUT_G[q3], k = 2 * r12;
    int32_t den = 4096 + ((k * g) >> 12) + ((g * g) >> 12);
    f->g = g;
    f->k = k;
    f->d = (int32_t)((1u << 28) / (uint32_t)den);
}

/* one ZDF state-variable step; returns lp, writes hp and bp */
static inline int32_t svf_tick(svf_t *f, int32_t x, int32_t *hp_out, int32_t *bp_out)
{
    int32_t hp = (int32_t)(((int64_t)(x - (int32_t)(((int64_t)(f->k + f->g) * f->s1) >> 12) - f->s2) * f->d) >> 16);
    int32_t v1 = (int32_t)(((int64_t)f->g * hp) >> 12);
    int32_t bp = v1 + f->s1;
    f->s1 = CLAMP(v1 + bp, -(1 << 21), 1 << 21);
    int32_t v2 = (int32_t)(((int64_t)f->g * bp) >> 12);
    int32_t lp = v2 + f->s2;
    f->s2 = CLAMP(v2 + lp, -(1 << 21), 1 << 21);
    if (hp_out) *hp_out = hp;
    if (bp_out) *bp_out = bp;
    return lp;
}

/* the track filter's parts, after the Monomachine's (measured, README: Calibration): a ZDF two-pole and a ZDF one-pole
 * on Q24 coefficients and 64-bit products, their states held in range. Each update carries its remainder to the next
 * sample, so a low cutoff settles exactly instead of sticking a few hundred steps off zero */
#define ZS_MAX (1 << 28)
/* the remainders are under 2^30 (2^24): a word each, the FM-1's adds and stores 32-bit */
typedef struct { int32_t s1, s2, a1, a2, a3, k; uint32_t r1, r2; } zsvf_t;  /* a1 a2 a3 Q30, k Q24; r: remainders */
typedef struct { int32_t s, G; uint32_t r; } zop_t;                         /* G Q24 */

/* g, k Q24 (g = tan(pi fc / fs), k = 1 / Q); the one division a block is 32-bit. 1 / den from it alone holds 14
 * bits, and a low cutoff's damping is g k ~1e-5 of den: at 131 Hz HPQ 127 rounded to no damping at all. One Newton
 * step (multiplies only) takes it to Q30. Every value a word: g under 2^26 and k under 2^29 (T_FG, T_EQG; the EQ's
 * deepest cut), den under 2^31, a1 at most 2^30, the step's error e under 2^20 */
static void zsvf_set(zsvf_t *f, uint32_t g, uint32_t k)
{
    int32_t den = (1 << 24) + (int32_t)mul_sh((int32_t)g, (int32_t)(g + k), 24);   /* Q24, 1 .. ~110 */
    int32_t a1 = (int32_t)((0x80000000u / (uint32_t)(den >> 10)) << 13);          /* Q30, ~14 bits */
    int32_t e = (1 << 30) + (int32_t)mul_sh(-den, a1, 24);                          /* Q30: 1 - den a1 */
    a1 += (int32_t)mul_sh(a1, e, 30);
    f->a1 = a1;
    f->a2 = (int32_t)mul_sh((int32_t)g, a1, 24);
    f->a3 = (int32_t)mul_sh((int32_t)g, f->a2, 24);
    f->k = (int32_t)k;
}

static inline void zsvf_tick(zsvf_t *f, int32_t v0, int32_t *lp, int32_t *bp, int32_t *hp)
{
    int32_t v3 = v0 - f->s2;
    int64_t p1 = dot(f->a1, f->s1, f->a2, v3) + f->r1;
    int64_t p2 = dot(f->a2, f->s1, f->a3, v3) + f->r2;
    int32_t v1 = (int32_t)(p1 >> 30), d2 = (int32_t)(p2 >> 30);
    f->r1 = (uint32_t)p1 & ((1u << 30) - 1);                   /* what the shift dropped: p - (p >> 30) * 2^30, the same */
    f->r2 = (uint32_t)p2 & ((1u << 30) - 1);                   /* bits for less work (two's complement) */
    int32_t v2 = f->s2 + d2;
    f->s1 = CLAMP(2 * v1 - f->s1, -ZS_MAX, ZS_MAX);
    f->s2 = CLAMP(2 * v2 - f->s2, -ZS_MAX, ZS_MAX);
    if (lp) *lp = v2;
    if (bp) *bp = v1;
    if (hp) *hp = v0 - (int32_t)mul_sh(f->k, v1, 24) - v2;
}

static inline int32_t zop_lp(zop_t *f, int32_t x)
{
    int64_t p = mul_sh(x - f->s, f->G, 0) + f->r;
    int32_t v = (int32_t)(p >> 24);
    f->r = (uint32_t)p & ((1u << 24) - 1);
    int32_t y = v + f->s;
    f->s = CLAMP(y + v, -ZS_MAX, ZS_MAX);
    return y;
}

/* a plain one-pole, y += G (x - y), G Q24 = 1 - e^(-2 pi fc / fs): GND-NOIS's colour, as fitted (tools/ref/gnoi_fit.py) */
static inline int32_t op_lp(zop_t *f, int32_t x)
{
    int64_t p = mul_sh(x - f->s, f->G, 0) + f->r;
    int32_t v = (int32_t)(p >> 24);
    f->r = (uint32_t)p & ((1u << 24) - 1);
    f->s += v;
    return f->s;
}

/* a Q24 coefficient at a filter position (1/96 octave over MIDI note 0, Q8), between table entries (T_FG or T_FG1:
 * rising, steps under 2^21, so the step's share is a word) */
static uint32_t fg_at(const uint32_t *T, int32_t pos)
{
    if (pos <= 0) return T[0];
    int32_t i = pos >> 8;
    if (i >= F_TOP) return T[F_TOP];
    return T[i] + (((T[i + 1] - T[i]) * (uint32_t)(pos & 255)) >> 8);
}

#define DL_LEN 16384u                  /* bytes per track: 0.74 s at half rate, mu-law; two lines of half that for ping-pong */

typedef struct {
    uint8_t note, gate, vel, sounding, slide, let_go;   /* let_go: a key or a MIDI note-off, not the sequencer */
    int32_t pitch, ptarget;            /* 1/64 semitones */
    uint8_t astage, fstage;
    uint32_t aenv, fenv;               /* Q30 */
    uint32_t aph;                      /* the attack's way through, Q30, straight in time: aenv its square */
    int32_t hold;                      /* control blocks left in hold */
    int32_t amp0, amp1;                /* Q15, this block's ramp */
    int32_t gate_left;                 /* control blocks until the sequencer's note-off; <0: none */
    /* machine state */
    uint32_t ph[8];
    int32_t ms[6];
    uint32_t menv[3];                  /* Q30 envelopes owned by the machine */
    uint32_t lfsr, lfsr2;              /* the noise; GND-NOIS's second (ST) */
    zop_t gn[2][4];                    /* GND-NOIS: each noise's pink one-poles and the brown's top */
    zsvf_t vo[4];                      /* VO-6's formants, F1 .. F4 in series */
    zop_t vohp;                        /* and its source's high-pass */
    int64_t vo_pw;                     /* and its compressor: the cascade's mean square, followed */
    int32_t vo_g;                      /* its gain (Q12 log2) */
    /* the chain: EQ, the filter (LP1 before the clip, the HP's two-pole, its one-pole and LP2 after), SRR */
    zsvf_t eq, lp1, hp2;
    zop_t hp1, lp2a, lp2b;
    int32_t eqc;                       /* the EQ band's mix, Q24 */
    /* what the chain's coefficients were worked out from (k_ok: they were): the same, they are the same, and the
     * division and the products a block are skipped (a still knob, most tracks, most of the time) */
    int32_t k_lpc, k_hpc, k_pl, k_ph, k_eqv, k_eqf;
    uint32_t k_lpk, k_hpk;
    uint8_t k_ok, k_side, k_eq;
    uint32_t srr_acc;
    int32_t srr_hold, srr_prev;
    /* SWAV-ENS's side (its chorus across the stereo) through the same chain, linear: the clip's ceiling holds it, the
     * fold and the XOR (MONO2K's own) leave it */
    zsvf_t eqs, lp1s, hp2s;
    zop_t hp1s, lp2as, lp2bs;
    int32_t srr_holds, srr_prevs;
    /* delay: a sum line, and a difference line while the send is positive (ping-pong) */
    uint8_t *dl;
    uint32_t dw, dph;                  /* write index; the line's clock, Q16 of a line sample */
    int32_t dt_q8;                     /* smoothed delay time in half-rate samples, Q8 */
    int32_t dacc[2], dn, dprev[2], dcur[2], dhp[2], dlp[2];
    int32_t dhr[2], dlr[2];            /* its one-poles' remainders */
    uint32_t dquiet;                   /* the delay's line writes in a row that were silence (all of it: it is skipped) */
    uint8_t dmode;                     /* 0: one line, the echoes keep the source's side; 1: ping-pong */
    uint8_t dsleep, dsent;             /* the delay was quiet last block; the track sent to it this one */
    /* LFOs */
    uint32_t lph[3];
    int32_t lval[3], lhold[3], lrnd[3];
    uint8_t ldone[3];
    uint32_t linc[3], lkey[3], lkb[3];    /* each one's rate, and the speed, MULT and tempo it came from (lkb 0: none) */
    /* params: the kit with this trig's locks, and the modulation in 1/256 steps */
    uint8_t eff[P_COUNT];
    int32_t mod[P_COUNT];              /* the LFOs' modulation, set a block at a time (modq: only what they touch) */
    int32_t peak;
    /* the modulation effect; its side (chorus and flanger across the stereo) high-passed and resampled as the mid */
    uint32_t mph;
    uint16_t mw;
    int32_t map[4], mfb;
    zop_t mhp1, mhp2;
    int32_t msrr_hold, msrr_prev;
    uint8_t mach;                      /* the machine its state is for */
    uint16_t ccost;                    /* its machine and chain's cost, the last block it sounded (T_MCOST) */
    uint32_t drng;                     /* SWAV-SAW's drift: its own generator (either core, in any order) */
} trk_t;

static trk_t trk[M2K_TRACKS];
static uint8_t dl_mem[M2K_TRACKS][DL_LEN] POOL;
#define MOD_LEN 1024u                  /* 23 ms: room for the chorus' sweep */
static int16_t mod_mem[M2K_TRACKS][MOD_LEN] POOL;
#define ENS_LEN 1024u                  /* SWAV-ENS's chorus line: 46 ms at half rate */
#define ENS_GAIN 7119                  /* a voice against the others' levels (calibrated: chain_ens20) */
static int16_t ens_mem[M2K_TRACKS][ENS_LEN] POOL;
static int16_t blk[M2K_TRACKS][M2K_SUB];      /* machine output of this block */
static int16_t tout[M2K_TRACKS][M2K_SUB];     /* each track's output, after its amp, before its VOL: an FX machine's input */
static int16_t tvol[M2K_TRACKS][M2K_SUB];     /* and after its VOL, its clip and its LEV, as the Monomachine's FX hear it (FX-RING) */
static int16_t eside[M2K_TRACKS][M2K_SUB];    /* SWAV-ENS's and GND-NOIS's side out of the machine, before the chain */
static int16_t bside[M2K_TRACKS][M2K_SUB];    /* FX-CHORUS's side (its width), added after the pan */

/* a parameter with its modulation, 0..127*256 */
static inline int32_t P8(const trk_t *t, int id)
{
    int32_t v = (int32_t)t->eff[id] * 256 + t->mod[id];
    return CLAMP(v, 0, 127 * 256);
}
static inline int32_t P7(const trk_t *t, int id) { return P8(t, id) >> 8; }
/* bipolar, -64..63 (Q8) */
static inline int32_t PB8(const trk_t *t, int id) { return P8(t, id) - 64 * 256; }
/* a choice of n */
static inline int32_t PN(const trk_t *t, int id, int n) { return (P7(t, id) * n) >> 7; }
/* a table between its entries at a parameter's Q8 (its LFO's share in it): an LFO glides through a law, never steps;
 * a whole value reads the entry itself, as the knob always has */
static uint32_t tq_u32_mid(const uint32_t *T, int32_t q8)       /* (out of line: a modulated value's, rare) */
{
    uint32_t a = T[q8 >> 8], b = T[(q8 >> 8) + 1], f = (uint32_t)q8 & 255u;
    return b >= a ? a + mulu_hi(b - a, f, 8) : a - mulu_hi(a - b, f, 8);
}
static inline uint32_t tq_u32(const uint32_t *T, int32_t q8) { return q8 & 255 ? tq_u32_mid(T, q8) : T[q8 >> 8]; }
static inline int32_t tq_u16(const uint16_t *T, int32_t q8)    /* (16-bit steps by a byte's share: a word) */
{
    int32_t a = T[q8 >> 8], f = q8 & 255;
    return f ? a + ((((int32_t)T[(q8 >> 8) + 1] - a) * f) >> 8) : a;
}
static inline int32_t tq_i16(const int16_t *T, int32_t q8)
{
    int32_t a = T[q8 >> 8], f = q8 & 255;
    return f ? a + ((((int32_t)T[(q8 >> 8) + 1] - a) * f) >> 8) : a;
}
/* a parameter times k, its share of a step too (P7 x k on a whole value; P8 x k under 2^31: k under 66000) */
static inline int32_t PK(const trk_t *t, int id, int32_t k) { return (P8(t, id) * k) >> 8; }

/* ------------------------------------------------------------------------------------------------ machines */

/* PolyBLEP saw, -32768..32767; inv = 0xFFFFFFFF / inc */
static inline int32_t blep_saw(uint32_t ph, uint32_t inc, uint32_t inv)
{
    int32_t s = (int32_t)(ph >> 16) - 32768;
    if (ph < inc) {
        uint32_t t = (ph * inv) >> 16;
        int32_t p = 2 * (int32_t)t - (int32_t)((t * t) >> 16) - 65536;
        s -= p >> 1;
    } else if (~ph < inc) {
        uint32_t u = (~ph * inv) >> 16, w = 65536 - u;
        s -= (int32_t)((w * w) >> 16) >> 1;
    }
    return s;
}

static inline int32_t fold16(int32_t y)
{
    int32_t p = (y + 32768) & 0x1FFFF;
    if (p > 65535) p = 131071 - p;
    return p - 32768;
}

static inline uint32_t noise_step(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x; }

#define GN_A 23460                     /* GND-NOIS's white: 0.716 of full scale (the Monomachine's level) */
#define GN_HR 2                        /* and out of the machine 12 dB down, the chain's input 12 dB up */
#define HOLD_K (M2K_SR * 60u * 8u / M2K_SUB)   /* HOLD's control blocks a step at 1 BPM, Q8 (a step is 1 / 32 beat) */
#define VO_INC_Q16 6382652533ULL      /* VO-6: a phase increment a hertz, Q16 (2^48 / 44100) */
#define VO_R 10                        /* its compressor: the ratio */
#define VO_T 3143                      /* its level where the gain is unity (Q8 log2 of the cascade's rms; tuned) */
#define VO_UP 57                       /* its gain up at most 0.23 dB a ms: Q12 log2 a block */
#define VO_DET 475                     /* its detector, a one-pole on the mean square a block, Q15 (25 ms) */
#define VO_G0 (-16806)                 /* its gain at a trig, Q12 log2 (-24.7 dB: 34.8 under its target at C3 on VOC 64 / 64) */
#define VO_NZ 1023                     /* its noise against its voice, Q12 (tuned: alike at VOC 64 / 64) */
#define RG_HR 1                        /* FX-RING out of the machine 6 dB down (its input reaches 1.94 x full scale) */
#define RG_CEIL 65500                  /* FX-RING's input clip, 2.03 x a track's ceiling (T_CLIP) at LEV 100: clean to INP ~82 */
#define PUL_A 3430                     /* SWAVE-PULS's pulse: 0.105 of GND-SIN's sine */
#define PUL_S1 32767                   /* its sine subs, as measured: an octave down at full scale, two at 0.96 */
#define PUL_S2 31500
#define SID_A 24950                    /* SID-6581's peak: a square 2.3 dB under full scale (measured) */
#define SID_MIX0 1024                  /* the triangle AND the saw: its mean over a cycle, of 4096 */
#define SID_MIXA (SID_A * 15 / 32)   /* MIX 9 dB under the triangle */
static const int8_t SAW_SPREAD[7] = {0, -16, 16, -9, 9, -4, 4};
static const uint16_t METAL_HZ[6] = {205, 304, 370, 523, 540, 800};

/* FM+DYN's depth at a point of its law (Q8 steps, 0..256), between table entries (rising, steps under 2^13) */
static inline uint32_t fmd_at(int32_t v8)
{
    int32_t i = v8 >> 8;
    if (i >= 256) return T_FMDV[256];
    return T_FMDV[i] + (((T_FMDV[i + 1] - T_FMDV[i]) * (uint32_t)(v8 & 255)) >> 8);
}

/* log2 in Q8 of x (x >= 1), and 2^(y / 256) in Q16 (y may be negative) */
static int32_t ilog2_q8(uint32_t x)
{
    if (!x) return 0;
    int p = 31;
    while (!(x >> p)) p--;
    uint32_t m = p >= 8 ? (x >> (p - 8)) & 255 : (x << (8 - p)) & 255;
    return p * 256 + T_LOG2M[m];
}
static uint32_t iexp2_q16(int32_t y)
{
    int32_t i = y >> 8;
    uint32_t f = T_EXP2F[y & 255];
    if (i >= 15) return 0x7FFFFFFFu;
    if (i < -16) return 0;
    return i >= 0 ? f << i : f >> -i;
}

/* FM+'s 24 ratio steps from a knob's 0..127, as the Monomachine's sound steps them (tools/ref/chain_fmstep43.spec): a
 * step begins at 5, 11, 16, 21, 27 .. 117, 123, rounded, where its screen (and the expert's sheet read off it) begins
 * eight of them a value later, at 6, 22, 38 .. 118 */
static inline int fr24(int v) { return (v * 24 + 8) >> 7; }

/* the DRM- drums: voice design after the Erica Synths LXR drum voices (GPL-3.0-only), MONO2K's own fixed point. Their
 * ramps fall a block at the LXR's time laws (T_DRM_RA, T_DRM_RD: its 64-sample steps as four of ours), shaped by its
 * curve: out(1/2) = c / 127, c x / ((127 - c) + (2c - 127) x), x and out Q15, one division a block */
static inline int32_t drm_curve(int32_t x, int32_t c)
{
    if (x <= 0) return 0;
    if (x >= 32768) return 32768;
    uint32_t num = (uint32_t)(c * x) << 8;                                   /* c x < 2^22 */
    uint32_t den = (uint32_t)((127 - c) * 32768 + (2 * c - 127) * x) >> 7;    /* 256 at least */
    return (int32_t)(num / den);
}
static inline int32_t drm_c(int v) { return 1 + ((v * 252 + 127) >> 8); }      /* a knob to c 1..126 */
static inline uint32_t drm_fall(uint32_t r, uint32_t d) { return r > d ? r - d : 0; }
#define DRM_K 18454938u                /* the snare's noise filter: Q 0.91 (k Q24) */
/* a carrier: SIN TRI SAW SQR, or NOIS (a new value each cycle of its pitch, held) */
static inline int32_t drm_wave(int w, uint32_t ph, uint32_t inc, uint32_t inv, trk_t *t, uint32_t prev)
{
    switch (w) {
    case 0: return sinp(ph);
    case 1: { int32_t s = (int32_t)(ph >> 15) - 65536; return (s < 0 ? -s : s) - 32768; }
    case 2: return -blep_saw(ph, inc, inv);                                  /* falling, as the LXR's */
    case 3: return (blep_saw(ph, inc, inv) - blep_saw(ph + 0x80000000u, inc, inv)) >> 1;
    default:
        if (ph < prev) t->ms[2] = (int32_t)noise_step(&t->lfsr) >> 16;
        return t->ms[2];
    }
}

static void mach_trig(trk_t *t, int m)
{
    t->menv[0] = t->menv[1] = t->menv[2] = 1u << 30;
    switch (m) {
    case M_GSIN: case M_FMS: case M_FMD: case M_FMP:
        t->ph[0] = t->ph[1] = t->ph[2] = t->ph[3] = 0;
        t->ms[0] = t->ms[1] = t->ms[2] = t->ms[3] = t->ms[4] = t->ms[5] = 0;
        /* FM+'s carrier starts half a cycle on from its modulators (its harmonics' phases: a ratio-1 modulator reads
         * turned over, a ratio-2 one not) */
        if (m != M_GSIN) t->ph[0] = 1u << 31;
        break;
    case M_WAV:
        t->ph[0] = t->ph[1] = 0;
        break;
    case M_SID:                        /* PWRS ON: the PW sweep starts again */
        if (t->eff[P_SYN + 2] >= 64) t->ms[4] = 0;
        break;
    case M_FXD:
        t->ms[2] = t->ms[3] = 0;
        break;
    case M_PUL:                        /* the voices start together (the unison's beating says so) */
        t->ph[0] = t->ph[1] = t->ph[2] = 0;
        if (t->eff[P_SYN + 6] >= 64) t->ms[4] = 0;
        break;
    case M_ENS:                        /* the voices start together on a trig (chain_ens18: two trigs alike) */
        t->ph[0] = t->ph[1] = t->ph[2] = t->ph[3] = 0;
        break;
    case M_VOX:                        /* the compressor's gain from near silence at each trig: the fade-in */
        t->vo_g = VO_G0;
        break;
    case M_DKIK:                       /* the phases from the sine's rising zero crossing; the snap and the click */
        t->ph[0] = t->ph[1] = 0;
        t->ms[2] = 0;
        t->ms[0] = 32767;
        t->ms[1] = P7(t, P_SYN + 7) * 258;
        break;
    case M_DSNR:                       /* the tone from its start; the clap's repeats */
        t->ph[0] = 0;
        t->ms[5] = 0;
        t->ms[3] = PN(t, P_SYN + 6, 16);
        break;
    case M_DHAT: case M_DCYM:          /* the three from their start; the amp from where it is (a hit chokes the last) */
        t->ph[0] = t->ph[1] = t->ph[2] = 0;
        t->ms[5] = m == M_DCYM && P7(t, P_SYN + 6);
        if (t->ms[5]) t->menv[1] = 0;
        break;
    default:
        break;
    }
}

/* another machine on a sounding track (SET MACHINE, a sound loaded, a pattern change): the state is the last one's,
 * which read as this one's can be anything (a full-scale block, seconds of silence, overflows). It starts from rest,
 * as a trig starts it */
static void machine_reset(int ti, int m)
{
    trk_t *t = &trk[ti];
    m_set(t->ph, 0, sizeof t->ph);
    m_set(t->ms, 0, sizeof t->ms);
    m_set(t->gn, 0, sizeof t->gn);
    m_set(t->vo, 0, sizeof t->vo);
    m_set(&t->vohp, 0, sizeof t->vohp);
    t->vo_pw = 0;
    t->vo_g = 0;
    if (m == M_ENS || m == M_FXC) m_set(ens_mem[ti], 0, sizeof ens_mem[ti]);   /* their line: not the last one's */
    mach_trig(t, m);
    t->mach = (uint8_t)m;
}

/* GND-PERC's COLR (-64..63) to its one-pole's coefficient, on its low-pass side or its high-pass side */
static inline int32_t gprc_colr(int32_t c, int low)
{
    return low ? T_OP[CLAMP(127 + c * 3 / 2, 20, 127)] : T_OP[CLAMP(c * 8 / 5, 0, 102)];
}

/* the machines: fill blk with 16 samples */
static void mach_block(int ti, int m, int32_t pitch)
{
    trk_t *t = &trk[ti];
    int16_t *o = blk[ti];
    const uint8_t *e = t->eff;
    (void)e;
    switch (m) {
    case M_GSIN: {                     /* FOLD SUB NOIS PDEP PDEC CLIK HARM TUNE: the Monomachine's page is TUNE alone, on
                                        * H (CC 55); A..G are MONO2K's own, nothing at init (a sine, as its) */
        int32_t env = (int32_t)(t->menv[0] >> 15);
        int32_t pq = pitch + (PB8(t, P_SYN + 7) >> 7) + ((PK(t, P_SYN + 3, 24) * env) >> 15);   /* TUNE: +-2 semitones */
        uint32_t inc = inc_of(pq);
        t->menv[0] = mulu_hi(t->menv[0], tq_u32(T_ENV_COEF, P8(t, P_SYN + 4)), 31);
        int32_t fold = P8(t, P_SYN + 0), sub = tq_u16(T_VOL, P8(t, P_SYN + 1));
        int32_t noi = tq_u16(T_VOL, P8(t, P_SYN + 2)) >> 1;
        int32_t harm = tq_u16(T_VOL, P8(t, P_SYN + 6));
        int32_t clk = ((int32_t)(t->menv[1] >> 15) * P8(t, P_SYN + 5)) >> 15;  /* 2^15 by 2^15 */
        t->menv[1] = mulu_hi(t->menv[1], 1700000000u, 31);
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = sinp(t->ph[0]);
            if (harm) s += (sinp(t->ph[0] * 2) * harm) >> 16;
            if (fold) s = fold16((s * (256 + (fold >> 4))) >> 8);
            if (sub) s += (sinp(t->ph[1]) * sub) >> 15;
            uint32_t n = noise_step(&t->lfsr);
            if (noi) s += (((int32_t)n >> 16) * noi) >> 15;
            if (clk) s += (((int32_t)n >> 16) * clk) >> 15;
            t->ph[0] += inc;
            t->ph[1] += inc >> 1;
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_GPRC: {                     /* COLR S&H META MSPR PDEP PDEC GRIT - */
        int32_t env = (int32_t)(t->menv[0] >> 15);
        int32_t pq = pitch + ((PK(t, P_SYN + 4, 24) * env) >> 15);
        t->menv[0] = mulu_hi(t->menv[0], tq_u32(T_ENV_COEF, P8(t, P_SYN + 5)), 31);
        /* COLR -64..63: darker (a lowpass falling to ~300 Hz) or thinner (a highpass rising to ~5 kHz); between steps too */
        int32_t c8 = PB8(t, P_SYN + 0), colr = c8 >> 8, cf = c8 & 255;
        int32_t a = gprc_colr(colr, c8 < 0);
        if (cf) a += ((gprc_colr(colr + 1, c8 < 0) - a) * cf) >> 8;
        int32_t sh = P8(t, P_SYN + 1);
        uint32_t shinc = sh ? inc_of(pq - 24 * 64 + (sh >> 3)) : 0;
        int32_t meta = tq_u16(T_VOL, P8(t, P_SYN + 2));
        int32_t spread = 256 + PK(t, P_SYN + 3, 6);
        int32_t grit = P8(t, P_SYN + 6) >> 1;                               /* Q7 of a step */
        uint32_t minc[6];
        if (meta) {
            uint32_t base = inc_of(pq) >> 2;                     /* 1 at C4-ish: scale the 808's set by the note */
            for (int k = 0; k < 6; k++) minc[k] = mulu_hi(base, (uint32_t)METAL_HZ[k] * (uint32_t)spread, 13);
        }
        if (!t->ms[2]) t->ms[2] = 1;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t n;
            int clock = 1;
            if (sh) {
                uint32_t p = t->ph[0];
                t->ph[0] += shinc;
                clock = t->ph[0] < p;
            }
            if (clock) {
                int32_t w = (int32_t)noise_step(&t->lfsr) >> 16;
                if (grit) {            /* GRIT: towards the 93-step loop of an 8-bit console's short noise */
                    uint32_t x = (uint32_t)t->ms[2];
                    uint32_t bit = (x ^ (x >> 6)) & 1u;
                    x = (x >> 1) | (bit << 14);
                    t->ms[2] = (int32_t)x;
                    int32_t nes = (x & 1u) ? 24000 : -24000;
                    w += ((nes - w) * grit) >> 14;                          /* 2^17 by 2^14 */
                }
                t->ms[0] = w;
            }
            n = t->ms[0];
            /* colour: one-pole low (below 64) or the rest of it (above) */
            t->ms[1] += ((n - t->ms[1]) * a) >> 15;
            int32_t s = c8 < 0 ? t->ms[1] : c8 > 0 ? n - t->ms[1] : n;
            if (meta) {
                int32_t m6 = 0;
                for (int k = 0; k < 6; k++) { t->ph[2 + k] += minc[k]; m6 += (t->ph[2 + k] >> 31) ? 5400 : -5400; }
                s = s + (int32_t)mul_sh(m6 - s, meta, 15);                 /* COLR's thin noise and m6 apart: past 2^16 */
            }
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_GNOI: {                     /* ST RED STON - - - - TUNE */
        /* after the Monomachine's GND-NOIS, measured on its OS (tools/ref/chain_gnoi42.spec): white noise, the same at
         * every note (the filter follows the note and TUNE, +-2 semitones). RED colours it: white at 0, pink at 64, red at
         * 127 (white, pink and brown from the same noise, weights fitted, T_RED_W). ST mixes in a second noise coloured
         * alike, ST / 127 of it: in the middle (3 dB louder at 127), or with STON across the stereo, the left hearing
         * the first plus the second and the right the first less it (L/R 0.88 0.60 0.29 0.01 at ST 32 64 96 127) */
        int32_t r8 = P8(t, P_SYN + 1), rf = r8 & 255, rw[3];
        for (int k = 0; k < 3; k++) {                                      /* RED's weights, between steps too */
            const int16_t *w = &T_RED_W[3 * (r8 >> 8) + k];
            rw[k] = rf ? w[0] + (((w[3] - w[0]) * rf) >> 8) : w[0];
        }
        int32_t st = PK(t, P_SYN + 0, 258), on = P7(t, P_SYN + 2) >= 64;      /* st Q15 */
        int16_t *es = eside[ti];
        if (!st && !rw[1] && !rw[2]) {     /* white, the second noise unheard: the first's white alone, the second's
                                            * generator kept in step; the colours' filters stand (ms[0] says so) */
            for (int i = 0; i < M2K_SUB; i++) {
                int32_t w = (((int32_t)noise_step(&t->lfsr) >> 16) * GN_A) >> 15;
                (void)noise_step(&t->lfsr2);
                o[i] = (int16_t)sat16(((w * rw[0]) >> 14) >> GN_HR);          /* w under 23460, the weight 2^14 */
                es[i] = 0;
            }
            t->ms[0] = 1;
            break;
        }
        if (t->ms[0]) { m_set(t->gn, 0, sizeof t->gn); t->ms[0] = 0; }      /* turned up: the colours from rest */
        for (int k = 0; k < 2; k++) {
            t->gn[k][0].G = GN_G0; t->gn[k][1].G = GN_G1; t->gn[k][2].G = GN_G2; t->gn[k][3].G = GN_GT;
        }
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t c[2];
            for (int k = 0; k < 2; k++) {
                int32_t w = (((int32_t)noise_step(k ? &t->lfsr2 : &t->lfsr) >> 16) * GN_A) >> 15;
                int32_t z0 = op_lp(&t->gn[k][0], w), z1 = op_lp(&t->gn[k][1], w), z2 = op_lp(&t->gn[k][2], w);
                /* the one-poles stay within the white's 23460 and the pink's weights sum to 48475: 32 bits */
                int32_t pk = (w * GN_KW + z0 * GN_K0 + z1 * GN_K1 + z2 * GN_K2) >> 12;
                int32_t br = (int32_t)mul_sh(op_lp(&t->gn[k][3], z0), GN_KB, 12);
                c[k] = (int32_t)((dot(pk, rw[1], br, rw[2]) + w * rw[0]) >> 14);
            }
            int32_t n2 = (int32_t)mul_sh(c[1], st, 15);
            /* the headroom: pink and red peak at 2.3 x full scale (3.3 x with ST); the chain takes it back (GN_HR) */
            o[i] = (int16_t)sat16((on ? c[0] : c[0] + n2) >> GN_HR);
            es[i] = (int16_t)sat16(on ? n2 >> GN_HR : 0);
        }
        break;
    }
    case M_SID: {                      /* PW PWAD PWRS WAVE MOD MSRC MFRQ TUNE */
        /* after the Monomachine's SID-6581, measured on its OS (tools/ref/chain_sid36.spec): an oscillator on the note,
         * TRI SAW PULS MIX NOIS at one peak level, band-limited. The pulse's duty is |PW - 64| / 128 (a square at 0
         * and 127, silent at 64); PWAD sweeps PW on from the trig, 2.70 steps a second a step, round and round, PWRS
         * ON starting it again at each trig. A second oscillator at MFRQ (C3 at 0, an octave every 32 steps, the same
         * at every note) or on the previous track's note (MSRC): SYNC plays it in the wave, hard-synced to the note;
         * RING multiplies the two triangles, unipolar, whatever the wave; R+S rings with the second synced */
        uint32_t inc1 = inc_of(pitch + (PB8(t, P_SYN + 7) >> 7));
        int mod = P7(t, P_SYN + 4) >> 5, wave = (P7(t, P_SYN + 3) * 5) >> 7;
        uint32_t inc2 = P7(t, P_SYN + 5) >= 64 ? inc_of(trk[(ti + M2K_TRACKS - 1) % M2K_TRACKS].pitch)
                                                : inc_of(48 * 64 + PK(t, P_SYN + 6, 24));
        t->ms[4] = (int32_t)(((uint32_t)t->ms[4] + (uint32_t)PK(t, P_SYN + 1, 64)) & 0x7FFFFFu);   /* the sweep, Q16 */
        int32_t pw = ((P8(t, P_SYN + 0) << 8) + t->ms[4]) & 0x7FFFFF;                             /* PW, Q16, 0..128 */
        pw -= 64 << 16;
        uint32_t duty = (uint32_t)(pw < 0 ? -pw : pw) << 9;                  /* of a cycle (2^32): |PW - 64| / 128 */
        int sync = mod >= 2, ring = mod & 1, two = mod == 2;                 /* SYNC: the second is what's heard */
        uint32_t inc = two ? inc2 : inc1, iv = 0xFFFFFFFFu / (inc | 1), iv1 = 0xFFFFFFFFu / (inc1 | 1);
        for (int i = 0; i < M2K_SUB; i++) {
            uint32_t p1 = t->ph[0];
            t->ph[0] += inc1;
            t->ph[1] += inc2;
            if (sync && t->ph[0] < p1)  /* the second restarts half a cycle on with the note's cycle (its partials
                                         * say so), as far into it as the note is */
                t->ph[1] = (1u << 31) + mulu_hi(t->ph[0] * iv1, inc2, 32);
            uint32_t ph = two ? t->ph[1] : t->ph[0];
            int32_t s;
            if (ring) {                /* two unipolar triangles multiplied, its mean taken off (the chain takes the rest) */
                int32_t u1 = (int32_t)((t->ph[0] ^ (uint32_t)((int32_t)t->ph[0] >> 31)) >> 16);    /* 0..32767 */
                int32_t u2 = (int32_t)((t->ph[1] ^ (uint32_t)((int32_t)t->ph[1] >> 31)) >> 16);
                s = (((u1 * u2) >> 14) - 16384) * SID_A >> 16;
            } else if (wave == 0) {
                s = ((int32_t)((ph ^ (uint32_t)((int32_t)ph >> 31)) >> 15) - 32768) * SID_A >> 15;
            } else if (wave == 1) {
                s = blep_saw(ph, inc, iv) * SID_A >> 15;
            } else if (wave == 2) {    /* two saws a duty apart: a pulse between +-SID_A, its mean left (the chain's) */
                s = (blep_saw(ph, inc, iv) - blep_saw(ph - duty, inc, iv) + 32768 - (int32_t)(duty >> 16)) * SID_A >> 15;
            } else if (wave == 3) {    /* MIX: the triangle AND the saw, as the chip combines them, its mean taken off */
                uint32_t tri = (ph ^ (uint32_t)((int32_t)ph >> 31)) << 1;
                s = ((int32_t)((tri & ph) >> 20) - SID_MIX0) * SID_MIXA >> 11;
            } else {                   /* NOIS: the chip's 23-bit register, clocked by bit 19 of the cycle's 24, its 8 */
                if (((ph ^ (ph - inc)) & (1u << 27)) && (ph & (1u << 27))) {     /* output bits */
                    uint32_t x = t->lfsr & 0x7FFFFFu;
                    if (!x) x = 0x7FFFF8u;
                    x = ((x << 1) | (((x >> 22) ^ (x >> 17)) & 1u)) & 0x7FFFFFu;
                    t->lfsr = x;
                    t->ms[0] = (int32_t)(((x >> 13) & 0x80) | ((x >> 12) & 0x40) | ((x >> 9) & 0x20) | ((x >> 7) & 0x10) |
                                         ((x >> 6) & 0x08) | ((x >> 3) & 0x04) | ((x >> 1) & 0x02) | (x & 0x01));
                }
                s = (t->ms[0] * 2 - 255) * (SID_A * 33 / 32) >> 8;    /* 0.3 dB over: as measured */
            }
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_FMS: {                      /* 1FRQ 1FIN 1ENV 1FB 2FRQ 2VOL TONE TUNE */
        /* after the Monomachine's FM+STAT, measured on its OS (tools/ref/chain_fm25.spec): a sine carrier on the note,
         * half GND-SIN's level, phase-modulated by two sine operators side by side. Each operator's ratio on one 24-step
         * table (1/64 .. 4); the first's fine-tuned by 0.75 .. 1.25 and fed back on itself; 1ENV its depth up to 64
         * (index 0 .. 1.56), above 64 that depth decaying from the trig (10 s .. 5 ms); 2VOL the second's depth, the same
         * law to 64 and on to index 14 at 127; both through a lowpass, TONE its cutoff (30 Hz at 0, 1.37 kHz at 64, open at
         * 127), so a high modulator goes in shallower; TUNE +-2 semitones */
        uint32_t inc = inc_of(pitch + (PB8(t, P_SYN + 7) >> 7));
        uint32_t r1 = mulu_hi(T_FMR24[fr24(P7(t, P_SYN + 0))], 192 * 256 + (uint32_t)P8(t, P_SYN + 1), 16);
        uint32_t r2 = T_FMR24[fr24(P7(t, P_SYN + 4))];
        uint32_t i1 = mulu_hi(inc, r1, 16), i2 = mulu_hi(inc, r2, 16);
        int32_t env = P8(t, P_SYN + 2);
        uint32_t k1 = tq_u32(T_FMB, env < 64 * 256 ? env : 64 * 256);
        if (env >= 65 * 256) {         /* the depth from the trig, decaying (T_FMB[64] 32837: a word) */
            k1 = (T_FMB[64] * (t->menv[0] >> 15)) >> 15;
            t->menv[0] = mulu_hi(t->menv[0], tq_u32(T_FMEC, env - 64 * 256), 31);
        }
        uint32_t k2 = tq_u32(T_FMB, P8(t, P_SYN + 5));
        int32_t lp = tq_u16(T_FMLP, P8(t, P_SYN + 6));
        uint32_t kf = tq_u32(T_FMFB, P8(t, P_SYN + 3));
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t m1 = sinp(t->ph[1] + (uint32_t)((t->ms[0] + t->ms[1]) >> 1) * kf);   /* the low word alone */
            t->ms[1] = t->ms[0];
            t->ms[0] = m1;
            /* each modulator through two one-poles at TONE's cutoff on its way to the carrier (at 64, 1.37 kHz: its
             * depth measured 0.88 at 523 Hz, 0.30 at 2 kHz, 0.10 at 4 kHz; the feedback taken before), states Q23 */
            uint64_t d = mul2_q15(lp, m1 * 256 - t->ms[2], sinp(t->ph[2]) * 256 - t->ms[4]);   /* the two firsts */
            t->ms[2] += (int32_t)d;
            t->ms[4] += (int32_t)(d >> 32);
            d = mul2_q15(lp, t->ms[2] - t->ms[3], t->ms[4] - t->ms[5]);                       /* and seconds */
            t->ms[3] += (int32_t)d;
            t->ms[5] += (int32_t)(d >> 32);
            uint32_t pm = (uint32_t)mul_sh(t->ms[3], (int32_t)k1, 8) + (uint32_t)mul_sh(t->ms[5], (int32_t)k2, 8);
            o[i] = (int16_t)(sinp(t->ph[0] + pm) >> 1);
            t->ph[0] += inc;
            t->ph[1] += i1;
            t->ph[2] += i2;
        }
        break;
    }
    case M_FMD: {                      /* 1FRQ 1FEN 1VOL 1VEN 2FRQ 2ENV 2FB TUNE */
        /* after the Monomachine's FM+DYN, measured on its OS (tools/ref/chain_fm26.spec): FM+STAT's carrier and two
         * operators side by side. The first's ratio 1FRQ / 64 (2 at 127), its frequency pushed from the trig by 1FEN (a
         * note-independent amount added before the ratio, up over 64, a quarter of it down under) and falling back; 1VOL
         * its depth, on FM+STAT's law; 1VEN over 64 pushes that law up from the trig by up to 128 of its steps, falling
         * back on 1FEN's time (under 64's depth the push adds twice as fast, and stops there). The second's ratio
         * (2FRQ / 64)^2 (4 at 127), 2ENV its depth or its decay as FM+STAT's 1ENV, 2FB its feedback as FM+STAT's 1FB;
         * TUNE +-2 semitones */
        uint32_t inc = inc_of(pitch + (PB8(t, P_SYN + 7) >> 7));
        int32_t f1 = P8(t, P_SYN + 0), f2 = P8(t, P_SYN + 4);              /* Q8: between steps too */
        int32_t r1 = (f1 << 2) + (f1 > 126 * 256 ? (f1 - 126 * 256) << 2 : 0);   /* Q16, v / 64, 2 at 127 */
        uint32_t r2 = f2 <= 126 * 256 ? ((uint32_t)f2 * (uint32_t)f2) >> 12          /* (v / 64)^2, 4 at 127 */
                                      : (126u * 126u << 4) + ((508u << 4) * (uint32_t)(f2 - 126 * 256) >> 8);   /* to 128^2 */
        int32_t fen = P8(t, P_SYN + 1), ven = P8(t, P_SYN + 3), env = P8(t, P_SYN + 5);
        int64_t b1 = inc;
        if (fen != 64 * 256) {
            int32_t fe = T_FMFEN[fen >> 8];
            if (fen & 255) fe += (int32_t)mul_sh(T_FMFEN[(fen >> 8) + 1] - fe, fen & 255, 8);
            b1 += mul_sh(fe, (int32_t)(t->menv[1] >> 15), 15);
            t->menv[1] = mulu_hi(t->menv[1], tq_u32(T_FMDEC, fen > 64 * 256 ? fen - 64 * 256 : 64 * 256 - fen), 31);
        }
        uint32_t i1 = (uint32_t)(int32_t)mul64_sh(b1, r1, 16), i2 = mulu_hi(inc, r2, 16);
        int32_t vol8 = P8(t, P_SYN + 2);
        int32_t k1 = (int32_t)fmd_at(vol8);
        /* the envelope's 2^15 by a step count, a knob or 64's depth (T_FMDV[64] 32691) fits a word; the turn's not */
        if (ven > 64 * 256) {          /* a push from the trig: 4 steps of the law a step, to 128 */
            int32_t e8 = ((ven < 96 * 256 ? 4 * (ven - 64 * 256) : 128 * 256) * (int32_t)(t->menv[0] >> 15)) >> 15;
            t->menv[0] = mulu_hi(t->menv[0], tq_u32(T_FMDEC, ven - 64 * 256), 31);
            if (vol8 + e8 >= 64 * 256) {
                k1 = (int32_t)fmd_at(vol8 + e8);
            } else {                   /* under 64's depth it adds linearly, twice as fast, and stops there */
                k1 += (int32_t)((T_FMDV[64] * (uint32_t)e8) >> 13);
                if (k1 > (int32_t)T_FMDV[64]) k1 = (int32_t)T_FMDV[64];
            }
        } else if (ven < 64 * 256) {   /* a cut from the trig, 1/32 of 1VOL a step; on 64's depth, past half of it the
                                        * operator turns over (to its whole depth, inverted, at 0) */
            int32_t c = ((64 * 256 - ven) * (int32_t)(t->menv[0] >> 15)) >> 13;                /* Q15 of 1VOL */
            t->menv[0] = mulu_hi(t->menv[0], tq_u32(T_FMDEC, 64 * 256 - ven), 31);
            int32_t f = 32768 - c;                                               /* Q15, -1 .. 1 */
            int32_t v8 = ((f < 0 && vol8 > 64 * 256 ? 64 * 256 : vol8) * f) >> 15;
            if (v8 >= 64 * 256) {
                k1 = (int32_t)fmd_at(v8);
            } else if (vol8 >= 64 * 256) {
                /* from over 64 the turn comes lower: at 32 steps from 64, at 0 from 127 */
                int32_t h = vol8 - 64 * 256;
                int32_t z = 32 * 256 - (int32_t)mul_sh(h * h, 2113, 26);
                k1 = ((int32_t)T_FMDV[64] * (v8 - z)) >> 13;
                if (k1 > (int32_t)T_FMDV[64]) k1 = (int32_t)T_FMDV[64];
            } else {
                k1 = (int32_t)mul_sh(k1, 2 * f - 32768, 15);
            }
        }
        uint32_t k2 = fmd_at(env < 64 * 256 ? env : 64 * 256);
        if (env >= 65 * 256) {
            k2 = (T_FMDV[64] * (t->menv[2] >> 15)) >> 15;
            t->menv[2] = mulu_hi(t->menv[2], tq_u32(T_FMEC, env - 64 * 256), 31);
        }
        uint32_t kf = tq_u32(T_FMFB, P8(t, P_SYN + 6));
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t m2 = sinp(t->ph[2] + (uint32_t)((t->ms[0] + t->ms[1]) >> 1) * kf);
            t->ms[1] = t->ms[0];
            t->ms[0] = m2;
            uint32_t pm = (uint32_t)sinp(t->ph[1]) * (uint32_t)k1 + (uint32_t)m2 * k2;   /* the low words alone */
            o[i] = (int16_t)(sinp(t->ph[0] + pm) >> 1);
            t->ph[0] += inc;
            t->ph[1] += i1;
            t->ph[2] += i2;
        }
        break;
    }
    case M_FMP: {                      /* 1FRQ 1ENV 2FRQ 2ENV 3FRQ 3ENV TONE TUNE */
        /* after the Monomachine's FM+PAR, measured on its OS (tools/ref/chain_fm26.spec): FM+STAT's carrier, lowpass and
         * laws with three operators side by side, each on the 24 steps; an ENV up to 64 its depth, over 64 the depth of
         * FM+STAT's 2VOL at that value, falling from the trig on 1ENV's time; TONE as FM+STAT's; TUNE +-2 semitones */
        uint32_t inc = inc_of(pitch + (PB8(t, P_SYN + 7) >> 7));
        uint32_t ii[3], kk[3];
        for (int j = 0; j < 3; j++) {
            ii[j] = mulu_hi(inc, T_FMR24[fr24(P7(t, P_SYN + 2 * j))], 16);
            int32_t env = P8(t, P_SYN + 2 * j + 1);
            uint32_t k = tq_u32(T_FMB, env);
            if (env >= 65 * 256) {
                k = mulu_hi(k, t->menv[j] >> 15, 15);
                t->menv[j] = mulu_hi(t->menv[j], tq_u32(T_FMEC, env - 64 * 256), 31);
            }
            kk[j] = k;
        }
        int32_t lp = tq_u16(T_FMLP, P8(t, P_SYN + 6));
        for (int i = 0; i < M2K_SUB; i++) {
            uint32_t pm = 0;
            for (int j = 0; j < 3; j++) {
                int32_t *a = &t->ms[2 * j];
                a[0] += (int32_t)mul_sh(sinp(t->ph[1 + j]) * 256 - a[0], lp, 15);
                a[1] += (int32_t)mul_sh(a[0] - a[1], lp, 15);
                pm += (uint32_t)mul_sh(a[1], (int32_t)kk[j], 8);
                t->ph[1 + j] += ii[j];
            }
            o[i] = (int16_t)(sinp(t->ph[0] + pm) >> 1);
            t->ph[0] += inc;
        }
        break;
    }
    case M_SAW: {                      /* UNIL UNIW UNIX DRFT SUBX SUB1 SUB2 TUNE */
        /* after the Monomachine's SWAVE-SAW, measured on its OS (tools/ref/swave_saw*.spec): a rising saw, free
         * running; the unison pair at -0.42 / +0.43 c a step of UNIW, each as loud as UNIL says; the extended pair at
         * half that distance, UNIL x UNIX; a square an octave down on its own curve; all through one pole of highpass
         * that follows the note (cutoff 1.0 .. 2.6 x f0: the saw's hollow fundamental); then sines an octave and two
         * down. Against GND-SIN's sine the saw stands at 0.158, SUB1 at 0.217, SUB2 at 0.082. DRFT is MONO2K's own. */
        int32_t unil = P8(t, P_SYN + 0), uniw = P8(t, P_SYN + 1), drift = P8(t, P_SYN + 3);   /* Q8 */
        int32_t pq = pitch + (PB8(t, P_SYN + 7) >> 7);                  /* TUNE: +-2 semitones, 3.125 c a step */
        if (drift) {
            if (!t->drng) t->drng = 0x6C8E9CF5u ^ ((uint32_t)ti + 1u) * 0x9E3779B9u;   /* its own: either core, any order */
            t->ms[4] += (((int32_t)(noise_step(&t->drng) >> 20) - 2048) * drift) >> 17;   /* 2^11 by 2^15 */
            t->ms[4] = CLAMP(t->ms[4], -(drift >> 5), drift >> 5);
            pq += t->ms[4] >> 6;
        }
        uint32_t inc[5], inv[5];
        inc[0] = inc_of(pq);
        inc[1] = mulu_hi(inc[0], tq_u32(T_SW1LO, uniw), 24);
        inc[2] = mulu_hi(inc[0], tq_u32(T_SW1HI, uniw), 24);
        inc[3] = mulu_hi(inc[0], tq_u32(T_SW2LO, uniw), 24);
        inc[4] = mulu_hi(inc[0], tq_u32(T_SW2HI, uniw), 24);
        int nv = unil ? 5 : 1;
        for (int k = 0; k < nv; k++) inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
        int32_t g1 = 5166 * unil / (127 * 256), g2 = (g1 * P8(t, P_SYN + 2)) / (127 * 256);   /* (2^13 by 2^15) */
        int32_t gx = (3369 * tq_u16(T_SWSUBX, P8(t, P_SYN + 4))) >> 15;
        int32_t gs1 = 7097 * P8(t, P_SYN + 5) / (127 * 256), gs2 = 2701 * P8(t, P_SYN + 6) / (127 * 256);
        uint32_t ih = inc[0] >> 1, invh = 0xFFFFFFFFu / (ih | 1);
        int32_t a = T_SWHP[CLAMP(pq >> 6, 0, 127)];
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = blep_saw(t->ph[0], inc[0], inv[0]) * 5166;
            if (unil) {
                s += (blep_saw(t->ph[1], inc[1], inv[1]) + blep_saw(t->ph[2], inc[2], inv[2])) * g1;
                if (g2) s += (blep_saw(t->ph[3], inc[3], inv[3]) + blep_saw(t->ph[4], inc[4], inv[4])) * g2;
            }
            s >>= 15;                                                     /* rising, a drop at the reset, as the original */
            if (gx) s += ((blep_saw(t->ph[5], ih, invh) - blep_saw(t->ph[5] + 0x80000000u, ih, invh)) * gx) >> 15;
            for (int k = 0; k < nv; k++) t->ph[k] += inc[k];
            /* the highpass: y = a (y + x - x') */
            t->ms[0] = (int32_t)mul_sh(a, t->ms[0] + s - t->ms[1], 15);
            t->ms[1] = s;
            int32_t y = t->ms[0];
            if (gs1) y += (sinp(t->ph[5]) * gs1) >> 15;
            if (gs2) y += (sinp(t->ph[6]) * gs2) >> 15;
            t->ph[5] += ih;
            t->ph[6] += inc[0] >> 2;
            o[i] = (int16_t)sat16(y);
        }
        break;
    }
    case M_PUL: {                      /* UNIL UNIW SUB1 SUB2 PW PWAD PWRS TUNE */
        /* after the Monomachine's SWAVE-PULS, measured on its OS (tools/ref/chain_pul38.spec): a pulse, its voices
         * started together, its duty PW / 128 (a square at 64, silent at 0 and 127), against GND-SIN's sine at 0.105;
         * PWAD sweeps PW on, 5.38 steps a second a step, round and round, PWRS ON starting it again at each trig; a
         * unison pair -+0.211 c a step of UNIW off, each as loud as UNIL says; sines an octave and two down */
        int32_t unil = P8(t, P_SYN + 0), uniw = P8(t, P_SYN + 1);         /* Q8 */
        uint32_t inc[3], inv[3];
        inc[0] = inc_of(pitch + (PB8(t, P_SYN + 7) >> 7));
        inc[1] = mulu_hi(inc[0], tq_u32(T_SW2LO, uniw), 24);            /* SWAV-SAW's extended pair's spread */
        inc[2] = mulu_hi(inc[0], tq_u32(T_SW2HI, uniw), 24);
        int nv = unil ? 3 : 1;
        for (int k = 0; k < nv; k++) inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
        t->ms[4] = (int32_t)(((uint32_t)t->ms[4] + (uint32_t)PK(t, P_SYN + 5, 128)) & 0x7FFFFFu);  /* the sweep, Q16 */
        uint32_t duty = (uint32_t)(((P8(t, P_SYN + 4) << 8) + t->ms[4]) & 0x7FFFFF) << 9;
        if (P7(t, P_SYN + 5)) {        /* swept: back and forth between 4 % and 96 % (the owner: wrapping round, it
                                        * passed through the silent ends, a dropout every sweep "like looping") */
            uint32_t x = (uint32_t)(((P8(t, P_SYN + 4) << 8) + t->ms[4]) & 0x7FFFFF);
            uint32_t tri = x < 0x400000u ? x << 1 : (0x800000u - x) << 1;   /* 0..2^23, no jump where x wraps */
            duty = 0x0A3D70A4u + mulu_hi(tri, 0xEB851EB8u, 23);             /* 4 % + 92 % of the triangle: to 96 % */
        }
        int32_t g1 = PUL_A * unil / (127 * 256);
        int32_t gs1 = PUL_S1 * P8(t, P_SYN + 2) / (127 * 256);              /* (2^15 by 2^15) */
        int32_t gs2 = PUL_S2 * P8(t, P_SYN + 3) / (127 * 256);
        int32_t dc = 32768 - (int32_t)(duty >> 16);
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = (blep_saw(t->ph[0], inc[0], inv[0]) - blep_saw(t->ph[0] - duty, inc[0], inv[0]) + dc) * PUL_A;
            if (unil)
                for (int k = 1; k < 3; k++)
                    s += (blep_saw(t->ph[k], inc[k], inv[k]) - blep_saw(t->ph[k] - duty, inc[k], inv[k]) + dc) * g1;
            s >>= 15;
            for (int k = 0; k < nv; k++) t->ph[k] += inc[k];
            if (gs1) s += (sinp(t->ph[5]) * gs1) >> 15;
            if (gs2) s += (sinp(t->ph[6]) * gs2) >> 15;
            t->ph[5] += inc[0] >> 1;
            t->ph[6] += inc[0] >> 2;
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_FXC: case M_FXR: case M_FXD: {
        /* the FX machines take the previous track's output (after its amp, before its VOL; the first track has none),
         * INP its level (100 as it is). MONO2K's own designs on the Monomachine's pages: the emulator it is measured on
         * passes no neighbour into its FX machines */
        int32_t x[M2K_SUB];
        int32_t inp = PK(t, P_SYN + 7, 328);                                         /* Q15, 100: unity */
        for (int i = 0; i < M2K_SUB; i++) x[i] = ti ? (tout[ti - 1][i] * inp) >> 15 : 0;
        int32_t mix = PK(t, P_SYN + 3, 258);                                         /* Q15 */
        if (m == M_FXC) {          /* DEL DEP SPD MIX FB WID LP INP: two taps swept in opposite ways off one line, their
                                    * mean the chorus, their difference its width, fed back through the first */
            int16_t *ln = ens_mem[ti];
            /* (DEL, DEP and LP between their steps too: an LFO glides them) */
            int32_t b8 = (22 << 8) + P8(t, P_SYN + 0) * 4, bm = b8 < 400 << 8 ? b8 >> 8 : 400;   /* to 12 ms, +-9 */
            int32_t d8 = P8(t, P_SYN + 1), dv = d8 >> 8, dep = ((dv * bm) >> 7) << 4;   /* Q4 samples */
            if (d8 & 255) dep += ((((dv + 1) * bm >> 7) - (dv * bm >> 7)) * (d8 & 255)) >> 4;
            uint32_t linc = tq_u32(T_CHO_INC, P8(t, P_SYN + 2));
            int32_t fb = (PB8(t, P_SYN + 4) * 58) >> 5;                              /* Q15 of +-0.9, from Q8 steps */
            int32_t l8 = P8(t, P_SYN + 6), wid = PK(t, P_SYN + 5, 258), lp = T_FMLP[64 + ((l8 >> 8) >> 1)];
            if (l8 & 255) lp += ((T_FMLP[64 + (((l8 >> 8) + 1) >> 1)] - lp) * (l8 & 255)) >> 8;
            for (int i = 0; i < M2K_SUB; i++) {
                uint32_t p = t->ph[6] += linc;
                int32_t tri = (int32_t)((p ^ (uint32_t)((int32_t)p >> 31)) >> 15) - 32768;   /* -32768 .. 32767 */
                int32_t da = b8 + ((dep * tri) >> 11), db = b8 - ((dep * tri) >> 11);     /* Q8 samples (2^13 by 2^15) */
                uint32_t w = t->ph[5];
                uint32_t ja = w - (uint32_t)(da >> 8), jb = w - (uint32_t)(db >> 8);
                int32_t fa = da & 255, fb2 = db & 255;
                int32_t a = (ln[ja & (ENS_LEN - 1)] * (256 - fa) + ln[(ja - 1) & (ENS_LEN - 1)] * fa) >> 8;
                int32_t b = (ln[jb & (ENS_LEN - 1)] * (256 - fb2) + ln[(jb - 1) & (ENS_LEN - 1)] * fb2) >> 8;
                ln[w & (ENS_LEN - 1)] = (int16_t)sat16(x[i] + ((a * fb) >> 15));
                t->ph[5] = w + 1;
                /* the wet's lowpass: 16-bit values apart by a Q15 coefficient under one, 32 bits */
                t->ms[2] += ((((a + b) >> 1) - t->ms[2]) * lp) >> 15;
                t->ms[3] += ((((a - b) >> 1) - t->ms[3]) * lp) >> 15;
                o[i] = (int16_t)sat16(x[i] + (int32_t)mul_sh(t->ms[2] - x[i], mix, 15));
                bside[ti][i] = (int16_t)sat16((((t->ms[3] * wid) >> 15) * mix) >> 15);
            }
        } else if (m == M_FXR) {   /* WAVE EXT - MIX - - - INP: after the Monomachine's RINGMOD, measured on its OS
                                    * (scratch ring/FINDINGS.md): the track before after its VOL, its clip and its LEV,
                                    * INP a square-law gain into a clip of its own (unity at 45, 7.9 x at 127), times a
                                    * carrier on the track's note: WAVE's measured cycle, sine to triangle (64) to a
                                    * square 10.4 dB down (127), odd harmonics only (T_RINGW). MIX crossfades dry and
                                    * wet. EXT fades the carrier to the track before the input (the Monomachine fades
                                    * it to its audio input, which the FM-1 has not); out RG_HR down, taken back */
            uint32_t inc = inc_of(pitch);
            int32_t wv = P8(t, P_SYN + 0), ext = PK(t, P_SYN + 1, 258), iv = P8(t, P_SYN + 7);   /* Q8 */
            int wi = wv >> 11;
            int32_t wf = wi < 15 ? (wv & 2047) << 4 : ((wv - 120 * 256) << 7) / 7;
            const int16_t *ca = T_RINGW[wi], *cb = T_RINGW[wi + 1];
            int32_t g = (iv * iv) >> 15;                                           /* Q12, (INP / 45.25)^2 */
            for (int i = 0; i < M2K_SUB; i++) {
                int32_t xi = ti ? CLAMP((tvol[ti - 1][i] * g) >> 12, -RG_CEIL, RG_CEIL) : 0;
                uint32_t ph = t->ph[0] += inc;
                int j = (int)(ph >> 24), j1 = (j + 1) & 255, fr = (int)((ph >> 9) & 0x7FFF);
                int32_t a = ca[j] + (((ca[j1] - ca[j]) * fr) >> 15), b = cb[j] + (((cb[j1] - cb[j]) * fr) >> 15);
                int32_t c = a + (((b - a) * wf) >> 15);
                /* 32 bits: c and the track 16-bit, EXT under 2^15; xi under 65500 by c */
                if (ext) c += (((ti > 1 ? tout[ti - 2][i] : 0) - c) * ext) >> 15;
                int32_t y = (xi * c) >> 15;
                o[i] = (int16_t)sat16((xi + (int32_t)mul_sh(y - xi, mix, 15)) >> RG_HR);
            }
        } else {                   /* ATK REL THRS MIX RAT GAIN RMS INP: a compressor on the block's peak and its RMS
                                    * (RMS's blend), followed in log2, over THRS's -60 .. 0 dB by RAT, GAIN made up */
            uint32_t pk = 1;
            uint64_t ms = 0;
            for (int i = 0; i < M2K_SUB; i++) {
                uint32_t a = (uint32_t)(x[i] < 0 ? -x[i] : x[i]);
                if (a > pk) pk = a;
                ms += (uint32_t)(x[i] * x[i]);                                  /* x under 41656: a word */
            }
            int32_t lp = ilog2_q8(pk), lr = ilog2_q8((uint32_t)(ms >> 4) | 1) >> 1;        /* log2 of the peak, the RMS */
            int32_t rmsv = P8(t, P_SYN + 6);
            int32_t lv = lp + (((lr - lp) * rmsv) >> 15);                                  /* 2^13 by 2^15 */
            int32_t *env = &t->ms[2];
            if (*env == 0) *env = lv;
            int32_t ar = lv > *env ? tq_u16(T_DYN_A, P8(t, P_SYN + 0)) : tq_u16(T_DYN_R, P8(t, P_SYN + 1));
            *env += ((lv - *env) * ar) >> 15;                                                /* lv under 2^12 */
            int32_t thr = 15 * 256 - ((127 * 256 - P8(t, P_SYN + 2)) * 2549) / (127 * 256);   /* -60 .. 0 dB, log2 Q8 */
            int32_t over = *env - thr;
            int32_t gl = over > 0 ? -((over * tq_u16(T_DYN_K, P8(t, P_SYN + 4))) >> 8) : 0;
            gl += (P8(t, P_SYN + 5) * 1020) / (127 * 256);                                 /* GAIN: up to +24 dB */
            uint32_t g1 = iexp2_q16(gl), g0 = (uint32_t)t->ms[3] ? (uint32_t)t->ms[3] : g1;
            if (g1 < g0) g0 = g1;          /* down at once, ramped up: a peak in this block is not let through at the
                                            * quieter block's gain (a note's first peak passed ~10 dB over) */
            for (int i = 0; i < M2K_SUB; i++) {
                uint32_t g = g0 + (uint32_t)mul_sh((int32_t)(g1 - g0), i, 4);       /* both under 2^31 */
                int32_t y = (int32_t)mul_sh(x[i], (int32_t)g, 16);
                o[i] = (int16_t)sat16(x[i] + (int32_t)mul_sh(y - x[i], mix, 15));
            }
            t->ms[3] = (int32_t)g1;
        }
        break;
    }
    case M_ENS: {                      /* PCH2 PCH3 PCH4 WAVE PW CHRL CHRW TUNE */
        /* after the Monomachine's SWAVE-ENS, measured on its OS (tools/ref/chain_ens15..20): the root and up to three
         * voices off its interval table (equal tempered, and the just 3/2 4/3 5/4 6/5 beside them), no level shared
         * between them. A voice is two saws an octave under its note, the second PW / 128 of a period on; WAVE fades
         * their sum out and their difference in at 0.63 (weights 1 - 0.37 x and 1 - 1.63 x, x = WAVE / 127): PW 64
         * one saw at the note, WAVE 127 a pulse an octave under. The chorus (tools/ref/chain_ens41.spec): two copies
         * off a half-rate line, each delayed by a sine at 0.2 Hz swinging +-16.8 c, the second a quarter turn on;
         * CHRL their mix (dry 1 - 0.83 x, wet 0.85 x). CHRW sets them apart across the stereo: the left hears
         * (1 + w) / 2 of the first and (1 - w) / 2 of the second, the right the other way round (w = 0.355
         * (CHRW / 127)^1.15, T_ENS_W), so at 0 both hear their mean (mono); out as the mid here and the side (eside) */
        int32_t base = pitch + (PB8(t, P_SYN + 7) >> 7) - 12 * 64;
        int32_t w8 = P8(t, P_SYN + 3);
        int32_t wa = 32768 - (w8 * 12124) / (127 * 256);                   /* Q15 (2^15 by 2^16) */
        int32_t wb = 32768 - (w8 * 53412) / (127 * 256);
        uint32_t off = (uint32_t)P8(t, P_SYN + 4) << 17;
        uint32_t inc[4], inv[4];
        for (int k = 0; k < 4; k++) {
            int32_t iv = k ? T_ENS_PCH[P7(t, P_SYN + k - 1)] : 0;
            inc[k] = iv == ENS_OFF ? 0 : inc_of(base + iv);
            inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
        }
        int32_t chrl = P8(t, P_SYN + 5), chrw = P8(t, P_SYN + 6);                  /* Q8 */
        int32_t dry = 32768 - (chrl * 27197) / (127 * 256), wet = (chrl * 27853) / (127 * 256);    /* Q15 */
        wet = (wet * tq_u16(T_ENS_G, chrw)) >> 15;                                 /* each channel at one copy's level */
        int32_t wside = (wet * tq_u16(T_ENS_W, chrw)) >> 15;                        /* the side's share, Q15 */
        int16_t *cm = ens_mem[ti];
        int16_t *es = eside[ti];
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = 0;
            for (int k = 0; k < 4; k++) {
                if (!inc[k]) continue;
                s += (blep_saw(t->ph[k], inc[k], inv[k]) * wa + blep_saw(t->ph[k] + off, inc[k], inv[k]) * wb) >> 15;
                t->ph[k] += inc[k];
            }
            s = (s * ENS_GAIN) >> 15;
            /* the line runs at half rate, always written (a CHRL turned up finds it full): a pair's mean */
            t->ms[0] += s;
            if (i & 1) { cm[t->ph[5]++ & (ENS_LEN - 1)] = (int16_t)sat16(t->ms[0] >> 1); t->ms[0] = 0; }
            t->ph[4] += ENS_CINC;
            es[i] = 0;
            if (chrl) {
                int32_t y[2];
                for (int c = 0; c < 2; c++) {                                       /* the two taps */
                    int32_t d8 = (2 << 8) + ENS_CDEP + ((ENS_CDEP * sinp(t->ph[4] + ((uint32_t)c << 30))) >> 15);   /* 1.4e9 */
                    uint32_t pos = (t->ph[5] << 8) + ((uint32_t)(i & 1) << 7) - (uint32_t)d8;   /* unsigned: it wraps */
                    uint32_t j = pos >> 8;
                    int32_t f = (int32_t)(pos & 255);
                    y[c] = (cm[j & (ENS_LEN - 1)] * (256 - f) + cm[(j + 1) & (ENS_LEN - 1)] * f) >> 8;
                }
                /* 32-bit products, their sum in 64 bits: the taps 16-bit, wside under 2^14, s under 2^16 by dry's 2^15,
                 * wet under 2^16 */
                es[i] = (int16_t)sat16(((y[0] - y[1]) * wside) >> 16);
                s = (int32_t)(((int64_t)(s * dry) + ((y[0] + y[1]) >> 1) * wet) >> 15);
            }
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_WAV: {                      /* WAVE SCAN STIM SYNC BITS DTUN MIX2 STEP */
        int32_t env = (int32_t)(t->menv[0] >> 15);
        if (P8(t, P_SYN + 2) < 127 * 256) t->menv[0] = mulu_hi(t->menv[0], tq_u32(T_ENV_COEF, P8(t, P_SYN + 2)), 31);
        /* the bank: MONO2K's 32 across WAVE's range (as it always was), or the owner's 64 two values a wave (an even
         * WAVE is one exactly, an odd one between two) */
        int big = PN(t, P_WBNK, 2), last = big ? 63 : 31;
        const int16_t *bank = big ? T_WAVES64 : T_WAVES;
        int32_t scan = (PB8(t, P_SYN + 1) * 2 * env) >> 15;
        int32_t pos = big ? (P8(t, P_SYN + 0) >> 1) + scan * 63 / 128
                          : (P8(t, P_SYN + 0) * 31) / 127 + scan * 31 / 128;      /* Q8 wave index */
        pos = CLAMP(pos, 0, last * 256);
        const int16_t *wa = &bank[(pos >> 8) * 256], *wb = &bank[CLAMP((pos >> 8) + 1, 0, last) * 256];
        /* a cycle's mean, out at playback for the owner's 64 (up to -29 % of the peak: a step at every trig, 13-17 dB
         * more under 60 Hz there, measured), and the gain that keeps a cycle so in 16 bits (four waves, to 32000); the
         * waves keep theirs, MONO2K's 32 play as they always have */
        int ia = pos >> 8, ib = CLAMP(ia + 1, 0, last);
        int32_t dca = big ? T_WAVES64_DC[ia] : 0, dcb = big ? T_WAVES64_DC[ib] : 0;
        int32_t ga = big ? T_WAVES64_G[ia] : 16384, gb = big ? T_WAVES64_G[ib] : 16384;
        int32_t dc = dca + (((dcb - dca) * (pos & 255)) >> 8), g = ga + (((gb - ga) * (pos & 255)) >> 8);
        int32_t wf = pos & 255;
        uint32_t inc = inc_of(pitch);
        uint32_t sinc = mulu_hi(inc, 256 + (uint32_t)PK(t, P_SYN + 3, 14), 8);
        uint32_t inc2 = inc_of(pitch + (PB8(t, P_SYN + 5) >> 5));
        int bits = 16 - (P7(t, P_SYN + 4) * 14 >> 7);                      /* (bits and STEP are steps) */
        int32_t mix2 = tq_u16(T_VOL, P8(t, P_SYN + 6));
        int stp = P7(t, P_SYN + 7) * 6 >> 7;   /* 0: smooth .. 5: 8 steps per cycle */
        uint32_t qmask = stp ? ~((1u << (24 + stp)) - 1) : 0xFFFFFFFFu;
        for (int i = 0; i < M2K_SUB; i++) {
            uint32_t pm = t->ph[2];
            t->ph[2] += inc;
            if (t->ph[2] < pm) t->ph[0] = 0;           /* hard sync to the master */
            uint32_t p = t->ph[0] & qmask;
            int32_t j = (int32_t)(p >> 24), f = stp ? 0 : (int32_t)((p >> 16) & 255);
            int32_t a = wa[j] + (((wa[(j + 1) & 255] - wa[j]) * f) >> 8);
            int32_t b = wb[j] + (((wb[(j + 1) & 255] - wb[j]) * f) >> 8);
            int32_t s = a + (((b - a) * wf) >> 8);
            s = ((s - dc) * g) >> 14;  /* the owner's waves play without their DC: no thump at a trig (wave_check) */
            if (mix2) {
                uint32_t q = t->ph[1] & qmask;
                int32_t j2 = (int32_t)(q >> 24);
                s += ((((wa[j2] - dca) * ga) >> 14) * mix2) >> 15;
                s = (s * 3) >> 2;
            }
            t->ph[0] += sinc;
            t->ph[1] += inc2;
            if (bits < 16) s = (s >> (16 - bits)) * (1 << (16 - bits));     /* a multiply: no left shift of a negative */
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_DKIK: {                     /* WAVE PDEP PDEC PCRV FM FMFR SNAP CLIK */
        /* the drum voice: a carrier swept f0 (1 + A P) in hertz (A = 32 (PDEP / 127)^2, x33 at most), P a ramp falling
         * at PDEC's time on PCRV's curve; FM by a sine at FMFR semitones from the note, its depth riding P (a clang in
         * the attack, pure in the tail); the snap a blip of pitch, s^2 x 24 x SNAP over 7 ms; a 1 ms click on top */
        int w = PN(t, P_SYN + 0, 4);
        if (!(t->ms[2]++ & 3)) {       /* the sweep and the snap a step every 64 samples, as the LXR's (its attack) */
            int32_t c8 = P8(t, P_SYN + 3), e = (int32_t)(t->menv[0] >> 15);
            int32_t pc = drm_curve(e, drm_c(c8 >> 8));
            if (c8 & 255) pc += ((drm_curve(e, drm_c((c8 >> 8) + 1)) - pc) * (c8 & 255)) >> 8;   /* PCRV between steps */
            t->menv[0] = drm_fall(t->menv[0], 4 * tq_u32(T_DRM_RA, P8(t, P_SYN + 2)));
            int32_t sn = t->ms[0];
            t->menv[2] = 65536u + (uint32_t)((tq_u16(T_DRM_PAMT, P8(t, P_SYN + 1)) * pc) >> 7)
                         + (uint32_t)((((sn * sn) >> 15) * PK(t, P_SYN + 6, 48)) >> 7);
            t->ms[0] = sn > 6552 ? sn - 6552 : 0;
            t->ms[3] = pc;
        }
        int32_t pc = t->ms[3];
        uint32_t mul = t->menv[2];
        int64_t ic = mul64_sh(inc_of(pitch), (int32_t)mul, 16);            /* (an increment up to 2^32, mul 2^30) */
        uint32_t inc = ic > 0x7FFF0000u ? 0x7FFF0000u : (uint32_t)ic, inv = 0xFFFFFFFFu / (inc | 1u);
        uint32_t minc = inc_of(pitch + (PB8(t, P_SYN + 5) >> 2));
        int32_t amt = PK(t, P_SYN + 4, 258);
        int32_t fmd = (amt * pc) >> 15;                                     /* both Q15 */
        int sh = w ? 4 : 2;                                /* a cycle of phase at full on the sine, four on the others */
        int32_t clk = t->ms[1];
        for (int i = 0; i < M2K_SUB; i++) {
            uint32_t off = 0;
            if (fmd) off = (uint32_t)(((sinp(t->ph[1]) * amt) >> 15) * fmd) << sh;
            int32_t y = (drm_wave(w, t->ph[0] + off, inc, inv, t, 0) * 29491) >> 15;   /* -0.9 dB: room for the click */
            if (clk) { y += clk; clk -= (clk * 734 + 32767) >> 15; }
            t->ph[0] += inc;
            t->ph[1] += minc;
            o[i] = (int16_t)sat16(y);
        }
        t->ms[1] = clk;
        break;
    }
    case M_DSNR: {                     /* TONE PDEP PDEC NOIS MIX NFLT CLAP CSPC */
        /* the snare: a tone swept as the drum's (its curve fixed, fast with a long tail) and a sample-and-hold noise at
         * NOIS (22 kHz at 127, not on the note), crossfaded by MIX; NFLT filters the noise alone (below 64 a lowpass down
         * from 18 kHz, above a highpass up from 100 Hz); CLAP repeats the hit: so many quick falls at CSPC's spacing,
         * then the track's AMP from the top */
        int w = PN(t, P_SYN + 0, 4);
        if (!(t->ms[5]++ & 3)) {       /* the sweep a step every 64 samples, as the drum's */
            int32_t pc = drm_curve((int32_t)(t->menv[0] >> 15), 15);
            t->menv[0] = drm_fall(t->menv[0], 4 * tq_u32(T_DRM_RA, P8(t, P_SYN + 2)));
            t->menv[2] = 65536u + (uint32_t)((tq_u16(T_DRM_PAMT, P8(t, P_SYN + 1)) * pc) >> 7);
        }
        int64_t ic = mul64_sh(inc_of(pitch), (int32_t)t->menv[2], 16);
        uint32_t inc = ic > 0x7FFF0000u ? 0x7FFF0000u : (uint32_t)ic, inv = 0xFFFFFFFFu / (inc | 1u);
        int32_t n8 = P8(t, P_SYN + 3);
        uint32_t ninc = (uint32_t)((n8 >> 8) | 1) * 16870877u;                /* 22050 v / 127 Hz */
        if (n8 & 255) ninc += mulu_hi((uint32_t)(((n8 >> 8) + 1) | 1) * 16870877u - ninc, (uint32_t)(n8 & 255), 8);
        int32_t mix = PK(t, P_SYN + 4, 258);
        int32_t nf = PB8(t, P_SYN + 5);                                     /* Q8 */
        if (nf) zsvf_set(&t->vo[0], fg_at(T_FG, nf < 0 ? DRM_LP0 + nf * 9 : DRM_HP0 + nf * 10), DRM_K);
        int32_t g0 = t->ms[4], g1 = 32768;
        if (t->ms[3] > 0) {
            t->menv[1] = drm_fall(t->menv[1], tq_u32(T_DRM_RA, P8(t, P_SYN + 7)));
            g1 = drm_curve((int32_t)(t->menv[1] >> 15), 20);
            if (!t->menv[1] && --t->ms[3] > 0) t->menv[1] = 1u << 30;
        }
        t->ms[4] = g1;
        for (int i = 0; i < M2K_SUB; i++) {
            uint32_t p = t->ph[1];
            t->ph[1] += ninc;
            if (t->ph[1] < p) t->ms[2] = (((int32_t)noise_step(&t->lfsr) >> 16) * 29491) >> 15;
            int32_t n = t->ms[2];
            if (nf) { int32_t lp, hp; zsvf_tick(&t->vo[0], n * 256, &lp, 0, &hp); n = (nf < 0 ? lp : hp) / 256; }
            int32_t y = drm_wave(w, t->ph[0], inc, inv, t, 0);
            t->ph[0] += inc;
            y = (int32_t)(dot(y, 32768 - mix, n, mix) >> 15);
            int32_t g = g0 + (((g1 - g0) * i) >> 4);
            o[i] = (int16_t)sat16((int32_t)mul_sh(y, g, 15));               /* NFLT's resonance takes y past 2^16 */
        }
        break;
    }
    case M_DHAT: case M_DCYM: {        /* CARR M1FR M1 M2FR M2 WAVE CDEC OPEN / CARR M1FR M1 M2FR M2 WAVE ATK DEC */
        /* the hats and the cymbal: three oscillators on the note, each a semitone offset, as the LXR's: the carrier
         * phase-modulated by a sine (M1) and a sample-and-hold noise (M2) at their pitches, their sum saturated; their own
         * amp on its decay law (the track's AMP left open), a hit taking it from where it is. The hats: CDEC, or OPEN's
         * when not 0 (lock it on the open steps), the carrier at half; the cymbal: a rise at ATK's, then DEC's */
        int cym = m == M_DCYM, w = PN(t, P_SYN + 5, 5), slp = cym ? 2 : 6;   /* their curves: the LXR's inits' */
        uint32_t inc = inc_of(pitch + (PB8(t, P_SYN + 0) >> 2)), inv = 0xFFFFFFFFu / (inc | 1u);
        uint32_t i1 = inc_of(pitch + (PB8(t, P_SYN + 1) >> 2)), i2 = inc_of(pitch + (PB8(t, P_SYN + 3) >> 2));
        int32_t g1 = PK(t, P_SYN + 2, 258), g2 = PK(t, P_SYN + 4, 258);
        int32_t a0 = t->ms[4], a1;
        if (t->ms[5]) {
            uint32_t r = t->menv[1] + tq_u32(T_DRM_RA, P8(t, P_SYN + 6));
            t->menv[1] = r >= 1u << 30 ? 1u << 30 : r;
            if (t->menv[1] >= 1u << 30) t->ms[5] = 0;
            a1 = 32768 - drm_curve(32768 - (int32_t)(t->menv[1] >> 15), 127 - slp);       /* the decay's mirror */
        } else {
            int32_t dv = cym || P7(t, P_SYN + 7) ? P8(t, P_SYN + 7) : P8(t, P_SYN + 6);
            t->menv[1] = drm_fall(t->menv[1], tq_u32(T_DRM_RD, dv));
            a1 = drm_curve((int32_t)(t->menv[1] >> 15), slp);
        }
        t->ms[4] = a1;
        int32_t lvl = cym ? 32767 : 16384;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t m1 = (sinp(t->ph[1]) * g1) >> 15;
            uint32_t p2 = t->ph[2];
            t->ph[2] += i2;
            if (t->ph[2] < p2) t->ms[3] = (int32_t)noise_step(&t->lfsr2) >> 16;
            int32_t mm = sat16(m1 + ((t->ms[3] * g2) >> 15));
            uint32_t pp = t->ph[0];
            int32_t y = w < 4 ? drm_wave(w, pp + ((uint32_t)mm << (w ? 19 : 17)), inc, inv, t, 0)
                              : drm_wave(4, pp, inc, inv, t, pp - inc);
            t->ph[0] += inc;
            t->ph[1] += i1;
            int32_t a = a0 + (((a1 - a0) * i) >> 4);
            o[i] = (int16_t)sat16((((y * lvl) >> 15) * a) >> 15);          /* y 16-bit, lvl and a Q15 */
        }
        break;
    }
    case M_VOX: {                      /* VOC1 VOC2 V-SW VOIC CONS CLEN CVOL TUNE */
        /* after the Monomachine's VO-6, measured on its OS (gen_tables VO_*): a cascade formant voice. An impulse a
         * period (the first difference of a band-limited saw: a flat spectrum, its level the same at any pitch) and
         * white noise, crossfaded by VOIC; a 188 Hz high-pass; four resonators in series, two-pole low-passes unity at
         * DC: F1 on VOC1, F2 on VOC2, F3 2495 Hz, F4 3527 Hz. Its pitch in whole hertz, rounded down, as the
         * Monomachine's (TUNE +-2 semitones). Then a compressor, as the Monomachine's level says (its loudness over 117
         * vowels is the cascade's at a tenth of the dB, 10:1, residual 0.9 dB): the cascade's mean square followed
         * (25 ms), the gain down at once and up at most 0.23 dB a ms, from the same low gain at each trig (the
         * Monomachine's fade-in, straight in dB, 35 dB at C3 and longer where its gain is higher). CONS CLEN CVOL and V-SW's switch: still to come (their knobs kept) */
        uint32_t hz = mulu_hi(inc_of(pitch + (PB8(t, P_SYN + 7) >> 7)), M2K_SR, 32);
        if (hz < 8) hz = 8;
        uint32_t inc = (uint32_t)mul64_sh((int64_t)VO_INC_Q16, (int32_t)hz, 16), inv = 0xFFFFFFFFu / inc;
        int32_t sg = (int32_t)iexp2_q16((ilog2_q8(130) - ilog2_q8(hz)) >> 1);      /* Q16: sqrt(130 / hz) */
        int32_t v1 = P8(t, P_SYN + 0), v2 = P8(t, P_SYN + 1), vc = P8(t, P_SYN + 3);   /* Q8: the vowels glide */
        zsvf_set(&t->vo[0], tq_u32(T_VO_G1, v1), tq_u32(T_VO_K1, v1));
        zsvf_set(&t->vo[1], tq_u32(T_VO_G2, v2), tq_u32(T_VO_K2, v2));
        zsvf_set(&t->vo[2], T_VO_GK34[0], T_VO_GK34[1]);
        zsvf_set(&t->vo[3], T_VO_GK34[2], T_VO_GK34[3]);
        t->vohp.G = VO_HP_G;
        int32_t gv = (int32_t)mul_sh(tq_u16(T_VO_GV, vc), sg, 16), gn = (tq_u16(T_VO_GN, vc) * VO_NZ) >> 12;
        /* the compressor: its target from the cascade's level so far, the gain to it at once down, slewed up */
        int32_t lv = (ilog2_q8((uint32_t)((t->vo_pw >> 16) | 1)) + 16 * 256) >> 1;  /* the rms, Q8 log2 */
        int32_t gt = ((VO_T - lv) * (VO_R - 1) * 16) / VO_R;                       /* Q12, 10:1 over everything */
        t->vo_g = gt < t->vo_g ? gt : t->vo_g + VO_UP < gt ? t->vo_g + VO_UP : gt;
        uint32_t cg = iexp2_q16(t->vo_g >> 4);                                     /* Q16 */
        int64_t pw = 0;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t sw = blep_saw(t->ph[0], inc, inv);
            t->ph[0] += inc;
            int32_t im = sw - t->ms[0];                /* an impulse a period, band-limited */
            t->ms[0] = sw;
            int32_t nz = (int32_t)noise_step(&t->lfsr) >> 16;
            int32_t x = (int32_t)mul_sh(im, gv, 15) + ((nz * gn) >> 15);   /* the noise 16-bit, gn under 2^13 */
            x -= zop_lp(&t->vohp, x);
            int32_t y;
            zsvf_tick(&t->vo[0], x, &y, 0, 0);
            zsvf_tick(&t->vo[1], y, &y, 0, 0);
            zsvf_tick(&t->vo[2], y, &y, 0, 0);
            zsvf_tick(&t->vo[3], y, &y, 0, 0);
            pw += mul_sh(y, y, 0);
            o[i] = (int16_t)sat16((int32_t)mul_sh(y, (int32_t)cg, 16));
        }
        t->vo_pw = t->vo_pw ? t->vo_pw + mul64_sh((pw >> 4) - t->vo_pw, VO_DET, 15) : (pw >> 4) | 1;   /* followed */
        break;
    }
    default:
        m_set(o, 0, sizeof(int16_t) * M2K_SUB);
        break;
    }
}

/* -------------------------------------------------------------------------------------------- envelopes */

static uint32_t hold_bpm = 120;       /* the pattern's tempo, for HOLD (set by each track's block) */
static uint32_t ext_bpm;               /* the clock in's tempo (GLO > MIDI SYNC IN), 0 when the pattern's rules */
static void mx_gate_end(int ti);       /* the sequencer's: a track's note ends (NOTES out) */

/* the amp's attack from a level (a trig over a sounding note): its phase where its square is that level, so the rise
 * goes on from there, no step */
static uint32_t atk_from(uint32_t lv)
{
    uint32_t r = 0, b = 1u << 14;                   /* the square root, Q15, of a Q30 level (2^30 at most) */
    for (; b; b >>= 1) if ((r + b) * (r + b) <= lv) r += b;
    return lv >= (1u << 30) ? 1u << 30 : r << 15;
}

static void env_block(trk_t *t)
{
    /* amp (format 16, gen_tables.py): the attack a square of its time to full (an even swell to the ear), hold (127:
     * while the gate is open), decay (127: none) and release on note-off straight in dB to -60 at their times */
    switch (t->astage) {
    case 1:
        t->aph += T_AATK_INC[t->eff[P_ATK]];        /* (2^30 at most, twice: a word) */
        t->aenv = t->eff[P_ATK] ? mulu_hi(t->aph, t->aph, 30) : t->aph;   /* 0: the Monomachine's straight 1 ms */
        if (t->aph >= (1u << 30)) {
            t->aenv = t->aph = 1u << 30;
            t->astage = 2;
            int h = t->eff[P_HOLD];                         /* v / 32 of a beat at the pattern's tempo, as the
                                                             * Monomachine's (measured at 60, 120 and 240 BPM) */
            t->hold = h == 127 ? -1 : (int32_t)((((uint32_t)h * HOLD_K) / hold_bpm + 128) >> 8);
        }
        break;
    case 2:
        if (t->hold < 0) { if (!t->gate) t->astage = 4; }
        else if (t->let_go) t->astage = 4;                  /* a key let go ends the hold, as on the Monomachine */
        else if (t->hold-- <= 0) t->astage = 3;
        break;
    case 3: {
        if (t->let_go) { t->astage = 4; break; }            /* and the decay */
        int32_t d8 = P8(t, P_DEC);
        if (d8 < 127 * 256) t->aenv = mulu_hi(t->aenv, tq_u32(T_ADEC_COEF, d8), 31);
        else if (!t->gate) t->astage = 4;
        if (t->aenv < (1u << 15)) { t->aenv = 0; t->astage = 0; }
        break;
    }
    case 4:
        t->aenv = mulu_hi(t->aenv, tq_u32(T_ADEC_COEF, P8(t, P_REL)), 31);
        if (t->aenv < (1u << 15)) { t->aenv = 0; t->astage = 0; }
        break;
    default:
        t->aenv = 0;
        break;
    }
    if (t->astage == 0) t->sounding = 0;
    /* filter: a straight attack, a straight decay to zero (127: stays), timed as the Monomachine's (measured) */
    if (t->fstage == 1) {
        t->fenv += T_FATK_INC[t->eff[P_FATK]];
        if (t->fenv >= (1u << 30)) { t->fenv = 1u << 30; t->fstage = 2; }
    } else if (t->fstage == 2) {
        uint32_t d = tq_u32(T_FDEC_DEC, P8(t, P_FDEC));
        t->fenv = t->fenv > d ? t->fenv - d : 0;
    }
}

/* ---------------------------------------------------------------------------------------------- the LFOs */

static int32_t lfo_shape(int w, uint32_t ph, int32_t *rnd)
{
    int32_t x = (int32_t)(ph >> 16);
    switch (w) {
    case 0: return x < 32768 ? x * 2 - 32768 : (65535 - x) * 2 - 32768;
    case 1: return sinp(ph);
    case 2: return ph < 0x80000000u ? 32767 : -32767;
    case 3: return 32767 - x;                                   /* saw, falling */
    case 4: {                                                   /* exp, unipolar (y * y to 2^32: unsigned) */
        uint32_t y = 65535u - (uint32_t)x;
        y = (y * y) >> 16;
        y = (y * y) >> 16;
        return (int32_t)(y >> 1);
    }
    case 5: return x >> 1;                                      /* ramp, unipolar */
    default: return *rnd;
    }
}

static uint32_t bpm10;                 /* tempo in 1/10 bpm, set by the sequencer */

/* an LFO's phase a control block (format 16, T_LSPD): SPD (Q8) a cycle in 2^((84 - SPD) / 12) beats of its clock (format
 * 15's, 1.45 % slow: saved LFOs keep their speed), 0 still, MULT (0..7) doubling it; at most 1/8 of a cycle a block
 * (345 Hz, about format 15's fastest): past it, a shape no more */
#define LFO_MAX (1u << 29)
static uint32_t lfo_inc(int32_t spd8, int mult, uint32_t b10)
{
    uint32_t base = mulu_hi(tq_u32(T_LSPD, spd8), b10, 8);
    return base > (LFO_MAX >> mult) ? LFO_MAX : base << mult;
}

/* the LFOs' modulation: at most three a track, so only what they touch is set, and cleared the block after */
typedef struct { uint8_t t, p; int32_t v; } modq_t;
static modq_t modq[M2K_TRACKS * 3], modp[M2K_TRACKS * 3];
static int modn, modpn;

static void mod_add(int t, int p, int32_t v)
{
    for (int i = 0; i < modn; i++) if (modq[i].t == t && modq[i].p == p) { modq[i].v += v; return; }
    modq[modn++] = (modq_t){(uint8_t)t, (uint8_t)p, v};
}

static void mod_apply(void)
{
    for (int i = 0; i < modpn; i++) trk[modp[i].t].mod[modp[i].p] = 0;
    for (int i = 0; i < modn; i++) { trk[modq[i].t].mod[modq[i].p] = modq[i].v; modp[i] = modq[i]; }
    modpn = modn;
    modn = 0;
}

static void lfo_block(int ti)
{
    trk_t *t = &trk[ti];
    for (int l = 0; l < 3; l++) {
        const int b = P_LFO + 8 * l;
        int dest = P7(t, b + L_DEST);
        if (dest == 0 || dest > P_COUNT) { t->lval[l] = 0; mon.lfo[ti][l] = 0; continue; }
        int wave = PN(t, b + L_WAVE, 7), trig = PN(t, b + L_TRIG, 5);
        int32_t spd8 = P8(t, b + L_SPD);
        int mult = PN(t, b + L_MULT, 8);
        uint32_t key = (uint32_t)spd8 << 3 | (uint32_t)mult;                /* (the same, the same rate) */
        if (key != t->lkey[l] || bpm10 != t->lkb[l] || !t->lkb[l]) {
            t->linc[l] = lfo_inc(spd8, mult, bpm10);
            t->lkey[l] = key;
            t->lkb[l] = bpm10;
        }
        uint32_t inc = t->linc[l];
        uint32_t p = t->lph[l];
        if (!t->ldone[l]) {
            t->lph[l] += inc;
            if (t->lph[l] < p) t->lrnd[l] = (int32_t)(rnd32() >> 16) - 32768;
            if (trig >= 3) {               /* ONE, HALF: a cycle or half of one from the trig's phase (PHAS), then held */
                uint32_t s = (uint32_t)t->eff[b + L_PHAS] << 25, a = p - s, z = t->lph[l] - s;
                if (trig == 3 && z < a) { t->ldone[l] = 1; t->lph[l] = s - 1u; }
                if (trig == 4 && z >= 0x80000000u && a < 0x80000000u) { t->ldone[l] = 1; t->lph[l] = s + 0x80000000u; }
            }
        }
        /* the depth (format 16): DPTH's distance from 64 squared, fine near 0, full at the ends (T_LDPTH: the old
         * -64..63 x 256), so a step of DPTH near the middle swings a fraction of a step, not two */
        int32_t depth = tq_i16(T_LDPTH, P8(t, b + L_DPTH));
        if (!depth && ti != mon.lfo_view) { t->lval[l] = 0; mon.lfo[ti][l] = 0; continue; }   /* its phase runs on; no
                                                                                             * shape for nothing */
        int32_t v = lfo_shape(wave, t->lph[l], &t->lrnd[l]);
        if (trig == 2) v = t->lhold[l];
        t->lval[l] = v;
        mon.lfo[ti][l] = v;
        if (depth) mod_add(PN(t, b + L_TRCK, M2K_TRACKS), dest - 1, (v * depth) >> 14);   /* 2^15 by 2^14: a word */
    }
}

static void lfo_trig(trk_t *t)
{
    for (int l = 0; l < 3; l++) {
        const int b = P_LFO + 8 * l;
        int trig = (t->eff[b + L_TRIG] * 5) >> 7;
        if (trig == 0) continue;
        if (trig == 2) {
            t->lhold[l] = lfo_shape((t->eff[b + L_WAVE] * 7) >> 7, t->lph[l], &t->lrnd[l]);
            continue;
        }
        t->lph[l] = (uint32_t)t->eff[b + L_PHAS] << 25;
        t->ldone[l] = 0;
    }
}

/* ---------------------------------------------------------------------------------------------- voices */

/* the store: the working patterns, then the settings. PAT(i) is pattern i. Where RAM is short (the FM-1:
 * M2K_PAT_SLOTS 2) two patterns are unpacked at a time, the rest packed in the working image (m2k_proj.c, pat_load);
 * elsewhere every pattern is (and pat[i] is pattern i too) */
#ifndef M2K_PAT_SLOTS
#define M2K_PAT_SLOTS M2K_PATTERNS
#endif
static struct {
    m2k_pattern_t p[M2K_PAT_SLOTS];
    uint8_t set[S_COUNT];
} store_ POOL;
static int cur_pat;
#if M2K_PAT_SLOTS < M2K_PATTERNS
#if M2K_PAT_SLOTS != 2
#error "the pattern cache keeps two patterns unpacked"
#endif
#define PAT_CACHE 1
static int16_t pat_idx[2] = {-1, -1};           /* the pattern each slot holds, -1 none */
static m2k_pattern_t *pat_load(int i);
static inline m2k_pattern_t *PAT(int i)
{
    return pat_idx[0] == i ? &store_.p[0] : pat_idx[1] == i ? &store_.p[1] : pat_load(i);
}
#else
#define PAT_CACHE 0
#define PAT(i) (&store_.p[(i)])
#define pat (store_.p)
#endif
static int pat_can_open(int j);                  /* (m2k_proj.c) may pattern j become the playing one */
static uint8_t master_vol = 100;

/* a trig: the note always (a trigless trig moves the pitch too), then the amp (and the machine with it), the filter's
 * envelope and the LFOs, each unless SKIP masks it (ST_NOAMP, ST_NOFLT, ST_NOLFO) */
static void voice_trig(int ti, int note, int vel, int slide, unsigned skip)
{
    trk_t *t = &trk[ti];
    t->note = (uint8_t)note;
    t->ptarget = note * 64;
    t->vel = (uint8_t)vel;
    if (slide && t->sounding) {
        t->slide = 1;
        t->gate = 1;
        return;
    }
    t->slide = 0;
    if (!t->sounding || t->eff[P_PORT] == 0) t->pitch = t->ptarget;
    if (!(skip & ST_NOAMP)) {
        t->gate = 1;
        t->let_go = 0;
        t->sounding = 1;
        t->astage = 1;
        /* ATK 0: percussive, from zero (its 1 ms rise the hit's edge, as bank A's drums have it); a slower attack goes
         * on from where the last note is, no step */
        t->aph = t->eff[P_ATK] ? atk_from(t->aenv) : 0;
        mach_trig(t, PAT(cur_pat)->machine[ti]);
    }
    if (!(skip & ST_NOFLT)) {
        t->fstage = 1;
        t->fenv = 0;
    }
    if (!(skip & ST_NOLFO)) lfo_trig(t);
    mon.trig[ti]++;
}

static void voice_on(int ti, int note, int vel, int slide) { voice_trig(ti, note, vel, slide, 0); }

static void mx_note_off(int t);        /* (the sequencer's: NOTES out) */

static void voice_off(int ti)
{
    trk_t *t = &trk[ti];
    t->gate = 0;
    t->gate_left = -1;
    t->let_go = 1;
    mx_note_off(ti);                   /* its note out ends with it (no gate end comes to send it) */
}

/* the modulation effect over a block: a chorus (7..17 ms, swept), a flanger (0.5..5 ms, fed back) or a phaser (four
 * allpasses swept from 0.1 to 0.9, fed back), all on one triangle LFO; mix 0..254 of the wet. The chorus and the
 * flanger are stereo (MONO2K's own): two taps swept in opposite ways, the left hearing one and the right the other;
 * the mid (their mean) goes back into x, the side (half their difference) into side; the phaser stays in the middle.
 * Its state in locals through the block (the line's 16-bit writes may alias its write place: through t it was read
 * again after each), one type a loop, a sample at a time as before */
static void mod_block(trk_t *t, int16_t *buf, int32_t *x, int32_t *side, int type, uint32_t inc, int32_t dep, int32_t mix)
{
    uint32_t mph = t->mph;
    if (type == 2) {
        int32_t m0 = t->map[0], m1 = t->map[1], m2 = t->map[2], m3 = t->map[3], mfb = t->mfb;
        for (int i = 0; i < M2K_SUB; i++) {
            mph += inc;
            int32_t tri = (int32_t)(mph >> 16);
            tri = tri < 32768 ? tri : 65535 - tri;
            int32_t a = 3277 + ((tri * dep) >> 15), v = x[i] + (mfb >> 1), u;
            u = v - ((a * m0) >> 15); v = (int32_t)mul_sh(a, u, 15) + m0; m0 = CLAMP(u, -65536, 65536);
            u = v - ((a * m1) >> 15); v = (int32_t)mul_sh(a, u, 15) + m1; m1 = CLAMP(u, -65536, 65536);
            u = v - ((a * m2) >> 15); v = (int32_t)mul_sh(a, u, 15) + m2; m2 = CLAMP(u, -65536, 65536);
            u = v - ((a * m3) >> 15); v = (int32_t)mul_sh(a, u, 15) + m3; m3 = CLAMP(u, -65536, 65536);
            mfb = CLAMP(v, -65536, 65536);
            side[i] = 0;
            x[i] = x[i] + (((v - x[i]) * mix) >> 8);
        }
        t->map[0] = m0; t->map[1] = m1; t->map[2] = m2; t->map[3] = m3; t->mfb = mfb;
    } else {
        uint32_t mw = t->mw;
        const int32_t base = type == 0 ? 309 << 8 : 20 << 8;
        for (int i = 0; i < M2K_SUB; i++) {
            mph += inc;
            int32_t tri = (int32_t)(mph >> 16);
            tri = tri < 32768 ? tri : 65535 - tri;
            int32_t ys[2];
            for (int c = 0; c < 2; c++) {
                int32_t tr = c ? 32767 - tri : tri;
                int32_t pos = ((int32_t)(uint16_t)mw << 8) - (base + ((tr * dep) >> 14));
                uint32_t j = (uint32_t)(pos >> 8);
                int32_t f = pos & 255;
                ys[c] = (buf[j & (MOD_LEN - 1)] * (256 - f) + buf[(j + 1) & (MOD_LEN - 1)] * f) >> 8;
            }
            int32_t xi = x[i], w = type == 1 ? xi + ((ys[0] * 18000) >> 15) : xi;
            buf[mw & (MOD_LEN - 1)] = (int16_t)sat16(w);
            mw++;
            int32_t y = (ys[0] + ys[1]) >> 1;
            side[i] = (((ys[0] - ys[1]) >> 1) * mix) >> 8;
            x[i] = xi + (((y - xi) * mix) >> 8);
        }
        t->mw = (uint16_t)mw;
    }
    t->mph = mph;
}

#define EQ_K 7626007                   /* 1 / 2.2, Q24: the EQ band's damping */
#define T_CLIP 32244                   /* a track's ceiling, per channel after the pan: just under full at LEV 127 */

/* b g >> sh for a 16-bit sample b, DIST's gain g (0 .. 2^31) and sh 16 .. 31, in words: b by each half of g fits one,
 * and (b gh 2^16 + b gl) >> sh is (b gh + (b gl >> 16)) >> (sh - 16) exactly */
static inline int32_t dist_in(int32_t b, int32_t g, int sh)
{
    return (b * (g >> 16) + ((b * (g & 0xFFFF)) >> 16)) >> (sh - 16);
}

/* the chain's filter over a block, a stage at a time (each filter's state in registers through its 16 samples: a
 * filter hears only its own input, so the order of the stages within a block changes nothing): EQ, LP1, the clip, the
 * HP's two-pole (its resonance held by the ceiling too), its one-pole, LP2 */
static void chain_filter(int32_t *x, int eq_on, int32_t eqc, int32_t ceil, zsvf_t *eq, zsvf_t *lp1, zsvf_t *hp2,
                         zop_t *hp1, zop_t *lp2a, zop_t *lp2b)
{
    if (eq_on) {
        zsvf_t f = *eq;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t b;
            zsvf_tick(&f, x[i], 0, &b, 0);
            int32_t y = x[i] + (int32_t)mul_sh(b, eqc, 24);
            x[i] = CLAMP(y, -(1 << 26), 1 << 26);
        }
        *eq = f;
    }
    {
        zsvf_t f = *lp1;
        for (int i = 0; i < M2K_SUB; i++) { int32_t v; zsvf_tick(&f, x[i], &v, 0, 0); x[i] = CLAMP(v, -ceil, ceil); }
        *lp1 = f;
    }
    {
        zsvf_t f = *hp2;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t u;
            zsvf_tick(&f, x[i], 0, 0, &u);
            f.s1 = CLAMP(f.s1, -ceil, ceil);
            f.s2 = CLAMP(f.s2, -ceil, ceil);
            x[i] = u;
        }
        *hp2 = f;
    }
    zop_t h = *hp1, la = *lp2a, lb = *lp2b;
    for (int i = 0; i < M2K_SUB; i++) { int32_t u = x[i]; u -= zop_lp(&h, u); x[i] = zop_lp(&lb, zop_lp(&la, u)); }
    *hp1 = h; *lp2a = la; *lp2b = lb;
}

/* what a sounding track costs, Mac instructions a sample counted as the FM-1's budget is (CLAUDE.md): its machine and
 * the chain on the kit's defaults (one track held, less the idle render), then the machine's dearer settings and each
 * stage it has on; the two cores' split weighs them (m2k_fx.c, par_split) */
static const uint16_t T_MCOST[M_COUNT] = {
    /* GSIN GPRC SID  FMS  FMD  SAW  ENS  WAV  VOX  FMP  PUL  FXC  FXR  FXD  GNOI DKIK DSNR DHAT DCYM */
       285, 275, 275, 310, 290, 310, 320, 290, 450, 325, 315, 350, 280, 290, 260, 290, 300, 320, 285
};
#define C_UNI_SAW 140                  /* SWAV-SAW's unison */
#define C_UNI_PUL 90                   /* SWAV-PULS's */
#define C_ENS_V 23                     /* SWAV-ENS: a voice past the root */
#define C_ENS_CH 100                   /* its chorus taps */
#define C_GN_COL 140                   /* GND-NOIS coloured, or its second noise */
#define C_EQ 40                        /* the EQ band */
#define C_MOD 135                      /* MOD's chorus or flanger, and its side */
#define C_PHAS 75                      /* MOD's phaser */
#define C_CRUSH 40                     /* SRR or BITS */
#define C_FOLD 15                      /* FOLD or XOR */
#define C_SIDE 140                     /* SWAV-ENS's or GND-NOIS's side through the chain */

/* one control block of one track: modulation, envelopes, the machine, then the chain, summed into lr */
static void track_block(int ti, int32_t *restrict L, int32_t *restrict R, int32_t *restrict dsend, int32_t *restrict rsend)
{
    trk_t *t = &trk[ti];
    const m2k_pattern_t *pp = PAT(cur_pat);
    int m = pp->machine[ti];
    if (t->gate_left > 0 && --t->gate_left == 0) { t->gate = 0; t->gate_left = -1; mx_gate_end(ti); }   /* the step's gate:
                                                          * hold and decay go on; its note out marked, sent after */
    env_block(t);
    /* an FX machine hears this track (the next one; FX-RING two on, its EXT): only then its tout and tvol are kept (no
     * one else reads them; the FX machine is with it on its core, after it) */
    const uint8_t *mc = pp->machine;
    const int heard = (ti + 1 < M2K_TRACKS && mc[ti + 1] >= M_FXC && mc[ti + 1] <= M_FXD) ||
                      (ti + 2 < M2K_TRACKS && mc[ti + 2] == M_FXR);
    if (heard) {
        m_set(tout[ti], 0, sizeof tout[ti]);
        m_set(tvol[ti], 0, sizeof tvol[ti]);
    }
    if (!t->sounding && t->astage == 0) {
        t->amp0 = t->amp1 = 0;
        t->peak = 0;
        mon.aenv[ti] = 0;
        mon.astage[ti] = 0;
        PROF(2);
        return;
    }
    if (t->mach != m) machine_reset(ti, m);
    /* pitch: the note, its glide, the track's PTCH. A step's size rounded away from zero both ways (the shift alone
     * rounds down: a glide up stopped short of its note, or never left at a slow PORT) */
    int32_t g = t->slide && t->eff[P_PORT] == 0 ? T_GLIDE[40] : tq_u16(T_GLIDE, P8(t, P_PORT));
    int32_t gd = t->ptarget - t->pitch;
    int32_t gs = gd * g;                                                   /* notes 0..255 apart: under 2^31 */
    t->pitch += gd > 0 ? (gs + 65535) >> 16 : gs >> 16;
    int32_t pitch = t->pitch + (PB8(t, P_PTCH) >> 2);
    PROF(2);
    mach_block(ti, m, pitch);
    PROF_M(m);

    /* the filter follows the note (and the tuned pitch): a cutoff 2^((step - 16) / 8) x f0, HP at BASE, LP at BASE + WDTH; the envelope adds
     * 7 d|d| / 256 steps at full (d: BOFS / WOFS from their centre). Positions in 1/96 octave over note 0, Q8 */
    int32_t fe = (int32_t)(t->fenv >> 15);
    int32_t db = PB8(t, P_BOF), dw = PB8(t, P_WOF);
    int32_t ob = (((7 * db * (db < 0 ? -db : db)) >> 16) * fe) >> 15;     /* 7 x 2^28 at most, then 2^15 by 2^15 */
    int32_t ow = (((7 * dw * (dw < 0 ? -dw : dw)) >> 16) * fe) >> 15;
    int32_t sb = P8(t, P_BASE) + ob, sl = sb + P8(t, P_WDTH) + ow;          /* steps, Q8 */
    int32_t key = t->note * 8 * 256;
    if (m == M_GSIN || m == M_GNOI) key += (PB8(t, P_SYN + 7) >> 7) * 32;   /* and their TUNE (chain_tune13, gnoi42) */
    int32_t ph = key + 12 * (sb - 16 * 256), pl = key + 12 * (sl - 16 * 256);
    int32_t lq = P8(t, P_LPQ), hq = P8(t, P_HPQ);                        /* (an LFO glides the resonances too) */
    int32_t lpc = pl + tq_i16(T_LPC, lq), hpc = ph + tq_i16(T_HPC, hq);
    uint32_t lpk = tq_u32(T_LPK, lq), hpk = tq_u32(T_HPK, hq);
    int side = (m == M_ENS && P8(t, P_SYN + 5) && P8(t, P_SYN + 6))       /* SWAV-ENS's chorus across the stereo, */
               || (m == M_GNOI && P7(t, P_SYN + 2) >= 64 && P8(t, P_SYN + 0));    /* GND-NOIS's second noise */
    int moved = !t->k_ok || lpc != t->k_lpc || hpc != t->k_hpc || pl != t->k_pl || ph != t->k_ph || lpk != t->k_lpk ||
                hpk != t->k_hpk;
    if (moved) {
        zsvf_set(&t->lp1, fg_at(T_FG, lpc), lpk);
        zsvf_set(&t->hp2, fg_at(T_FG, hpc), hpk);
        t->lp2a.G = t->lp2b.G = (int32_t)fg_at(T_FG1, pl + F_LP2 * 256);
        t->hp1.G = (int32_t)fg_at(T_FG1, ph + F_HP1 * 256);
        t->k_lpc = lpc; t->k_hpc = hpc; t->k_pl = pl; t->k_ph = ph; t->k_lpk = lpk; t->k_hpk = hpk;
        t->k_ok = 1;
        t->k_side &= (uint8_t)~1u;
    }
    if (side && !(t->k_side & 1)) {    /* (the side's the same, set when it is on and they moved) */
        t->lp1s.a1 = t->lp1.a1; t->lp1s.a2 = t->lp1.a2; t->lp1s.a3 = t->lp1.a3; t->lp1s.k = t->lp1.k;
        t->hp2s.a1 = t->hp2.a1; t->hp2s.a2 = t->hp2.a2; t->hp2s.a3 = t->hp2.a3; t->hp2s.k = t->hp2.k;
        t->lp2as.G = t->lp2bs.G = t->lp2a.G;
        t->hp1s.G = t->hp1.G;
        t->k_side |= 1;
    }
    mon.cut_hp[ti] = CLAMP(((ph >> 8) * 1087 >> 10) - 132, 0, 1016);       /* on the screen's 20 Hz .. 20 kHz */
    mon.cut_lp[ti] = CLAMP(((pl >> 8) * 1087 >> 10) - 132, 0, 1016);
    /* DIST: headroom below 64, a gain into the clip above 72; the clip's ceiling falls from +5.8 dB to -3.2 dB of a
     * full-scale GND-SIN between 64 and 80, and each machine has its own, as the Monomachine's do (SWAV-SAW's 10.5 dB
     * under GND-SIN's, whatever goes in: chain_dist21). Between steps, interpolated */
    int32_t d8 = P8(t, P_DIST), di = d8 >> 8, df = d8 & 255, dn = di < 127 ? di + 1 : 127;
    int32_t dh = (int32_t)(T_DIST_H[di] + mulu_hi(T_DIST_H[dn] - T_DIST_H[di], (uint32_t)df, 8));   /* rising, under 2^30 */
    int32_t ceil = ((T_DIST_C[di] + (((T_DIST_C[dn] - T_DIST_C[di]) * df) >> 8)) * T_DIST_M[m]) >> 12;   /* 2^17 by 2^13 */
    /* EQ: a band added to the signal, Q 2.2, +36 / -31 dB; a cut is the inverse of a boost as deep (its band widened
     * by the gain) */
    int32_t eqv = P8(t, P_EQG);
    int eq_on = eqv != 64 * 256;
    if (eq_on) {
        int32_t eqf = P8(t, P_EQF);
        if (!t->k_eq || eqv != t->k_eqv || eqf != t->k_eqf) {
            uint32_t gb = tq_u32(T_EQGAIN, eqv), ef = tq_u32(T_EQG, eqf);
            uint32_t k = eqv > 64 * 256 ? EQ_K : mulu_hi(EQ_K, gb, 16);
            zsvf_set(&t->eq, ef, k);
            int32_t c = (int32_t)mul_sh((int32_t)(gb - 65536), EQ_K, 16);
            t->eqc = eqv > 64 * 256 ? c : -c;
            t->k_eqv = eqv; t->k_eqf = eqf; t->k_eq = 1;
            t->k_side &= (uint8_t)~2u;
        }
        if (side && !(t->k_side & 2)) {
            t->eqs.a1 = t->eq.a1; t->eqs.a2 = t->eq.a2; t->eqs.a3 = t->eq.a3; t->eqs.k = t->eq.k;
            t->k_side |= 2;
        }
    }
    /* amp */
    t->amp0 = t->amp1;
    t->amp1 = (int32_t)(t->aenv >> 15);
    mon.aenv[ti] = t->amp1;
    mon.astage[ti] = t->astage;
    /* the rest between their table's steps too (an LFO on VOL, PAN, a send: no zipper); BITS and XOR are steps */
    int32_t vol = tq_u16(T_VOLK, P8(t, P_VOL));
    int32_t acc = 256 + (t->vel > 100 ? (pp->glob[G_ACNT] * 2) : 0);
    vol = (vol * acc) >> 8;
    int32_t lev = tq_u16(T_VOL, P8(t, P_LEVL));
    int32_t srr = P8(t, P_SRR);
    uint32_t sinc = tq_u32(T_SRR_INC, srr);
    int bits = 16 - (P7(t, P_BITS) * 14 >> 7);
    int32_t fold = PK(t, P_FOLD, 6), xr = P7(t, P_XOR), xm = (xr * xr * 2) & 0x7FFF;
    int32_t pan = P8(t, P_PAN);
    int32_t gl = tq_u16(T_PAN, pan), gr = tq_u16(T_PAN, 127 * 256 - pan);
    int32_t sd = P8(t, P_DSND) - 64 * 256, ds = (sd < 0 ? -sd : sd) * 2;   /* the send: |d| / 64, Q15 (format 17) */
    int32_t rs = tq_u16(T_SEND, P8(t, P_SPU));
    int mtype = PN(t, P_MODT, 3);
    int32_t mmix = PK(t, P_MODM, 2), mdep = PK(t, P_MODD, mtype == 2 ? 206 : mtype ? 200 : 441);
    uint32_t minc = tq_u32(T_MOD_INC, P8(t, P_MODR));
    int32_t pk = 0;
    int ish = m == M_GNOI ? 24 - GN_HR : m == M_FXR ? 24 - RG_HR : 24;       /* GND-NOIS's, FX-RING's headroom, taken back */
    t->dsent = ds != 0;
    {                                  /* what this block cost, for the next one's split */
        int32_t c = T_MCOST[m];
        if (m == M_SAW || m == M_PUL) c += P7(t, P_SYN) ? (m == M_SAW ? C_UNI_SAW : C_UNI_PUL) : 0;
        else if (m == M_ENS) {
            for (int k = 0; k < 3; k++) c += T_ENS_PCH[P7(t, P_SYN + k)] == ENS_OFF ? 0 : C_ENS_V;
            c += P7(t, P_SYN + 5) ? C_ENS_CH : 0;
        }
        else if (m == M_GNOI) c += t->ms[0] ? 0 : C_GN_COL;             /* (ms[0]: it took the white path) */
        c += (eq_on ? C_EQ : 0) + (mmix ? (mtype == 2 ? C_PHAS : C_MOD) : 0) + (srr || bits < 16 ? C_CRUSH : 0)
             + (fold || xr ? C_FOLD : 0) + (side ? C_SIDE : 0);
        t->ccost = (uint16_t)c;
    }
    /* the chain a stage at a time over the block: the filter (and the side's), MONO2K's own breakage and the amp, the
     * MOD effect, then VOL, SRR, BITS, the sides, the pan and the sends */
    int32_t x[M2K_SUB], xs[M2K_SUB], msd[M2K_SUB];
    for (int i = 0; i < M2K_SUB; i++) x[i] = CLAMP(dist_in(blk[ti][i], dh, ish), -(1 << 24), 1 << 24);
    chain_filter(x, eq_on, t->eqc, ceil, &t->eq, &t->lp1, &t->hp2, &t->hp1, &t->lp2a, &t->lp2b);
    if (side) {                        /* the side through the same filters, amp, VOL and SRR, added after the pan */
        for (int i = 0; i < M2K_SUB; i++) xs[i] = CLAMP(dist_in(eside[ti][i], dh, ish), -(1 << 24), 1 << 24);
        chain_filter(xs, eq_on, t->eqc, ceil, &t->eqs, &t->lp1s, &t->hp2s, &t->hp1s, &t->lp2as, &t->lp2bs);
    }
    int32_t a0 = t->amp0, da = t->amp1 - t->amp0;
    for (int i = 0; i < M2K_SUB; i++) {               /* MONO2K's own breakage, then the amp */
        int32_t s = x[i];
        if (fold) s = fold16((sat16(s) * (256 + fold)) >> 8);
        if (xr) s = (int32_t)(int16_t)((uint16_t)sat16(s) ^ (uint16_t)xm);
        x[i] = (int32_t)mul_sh(s, a0 + ((da * i) >> 4), 15);
    }
    if (mmix) {
        int16_t *mb = mod_mem[ti];
        mod_block(t, mb, x, msd, mtype, minc, mdep, mmix);
        t->mhp1.G = t->mhp2.G = MOD_SIDE_HP;
    }
    int16_t *to = tout[ti], *tv = tvol[ti];
    if (!srr && bits >= 16 && !side && !(mmix && mtype != 2) && m != M_FXC) {
        if (!ds && !rs && !heard) {
            /* and nothing sent, no FX machine hearing it (its tout and tvol stand cleared, its sends at 0) */
            for (int i = 0; i < M2K_SUB; i++) {
                int32_t s = (int32_t)mul_sh(x[i], vol, 15);
                uint64_t lr = mul2_q15(s, gl, gr);
                int32_t l = CLAMP((int32_t)lr, -T_CLIP, T_CLIP), r = CLAMP((int32_t)(lr >> 32), -T_CLIP, T_CLIP);
                int32_t as = l < 0 ? -l : l, ar = r < 0 ? -r : r;
                if (ar > as) as = ar;
                if (as > pk) pk = as;
                L[i] += (l * lev) >> 15;
                R[i] += (r * lev) >> 15;
            }
            t->peak = pk;
            if (pk > mon.peak[ti]) mon.peak[ti] = pk;
            PROF(2);
            return;
        }
        for (int i = 0; i < M2K_SUB; i++) {           /* the common case: no SRR, BITS or side, the same steps as below */
            int32_t s = x[i];
            if (heard) to[i] = (int16_t)sat16(s);
            s = (int32_t)mul_sh(s, vol, 15);
            uint64_t lr = mul2_q15(s, gl, gr);
            int32_t l = CLAMP((int32_t)lr, -T_CLIP, T_CLIP), r = CLAMP((int32_t)(lr >> 32), -T_CLIP, T_CLIP);
            int32_t as = l < 0 ? -l : l, ar = r < 0 ? -r : r;
            if (ar > as) as = ar;
            if (as > pk) pk = as;
            L[i] += (l * lev) >> 15;
            R[i] += (r * lev) >> 15;
            int32_t mm = CLAMP(s, -T_CLIP, T_CLIP);
            if (heard) tv[i] = (int16_t)((mm * lev) >> 15);
            dsend[i] = (mm * ds) >> 15;
            rsend[i] += (((mm * rs) >> 15) * lev) >> 15;
        }
        t->peak = pk;
        if (pk > mon.peak[ti]) mon.peak[ti] = pk;
        PROF(2);
        return;
    }
    if (!side && m != M_FXC) {         /* (no side of the machine's: the same steps as below, their state in locals and
                                        * the branches the block's: the dense patterns' case, SRR, BITS and MOD on) */
        const int msd_on = mmix && mtype != 2, bs = bits < 16 ? 16 - bits : 0;
        uint32_t sacc = t->srr_acc;
        int32_t sh = t->srr_hold, sp = t->srr_prev, mh = t->msrr_hold, mp = t->msrr_prev;
        zop_t h1 = t->mhp1, h2 = t->mhp2;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = x[i];
            if (heard) to[i] = (int16_t)sat16(s);
            s = (int32_t)mul_sh(s, vol, 15);
            if (srr) {
                sacc += sinc;
                if (sacc >= 65536u) { sacc -= 65536u; sp = sh; sh = s; }
                s = sp + (int32_t)mul_sh(sh - sp, (int32_t)sacc, 16);
            }
            if (bs) s = (sat16(s) >> bs) * (1 << bs);
            int32_t sd = 0;
            if (msd_on) {
                int32_t ms = (int32_t)mul_sh(msd[i], vol, 15);
                ms -= zop_lp(&h1, ms);
                ms -= zop_lp(&h2, ms);
                if (srr) {
                    if (sacc < sinc) { mp = mh; mh = ms; }
                    ms = mp + (int32_t)mul_sh(mh - mp, (int32_t)sacc, 16);
                }
                if (bs) ms = (sat16(ms) >> bs) * (1 << bs);
                sd = (int32_t)mul_sh(ms, T_PAN[64], 15);
            }
            uint64_t lr = mul2_q15(s, gl, gr);
            int32_t l = (int32_t)CLAMP((int64_t)(int32_t)lr + sd, -T_CLIP, T_CLIP);
            int32_t r = (int32_t)CLAMP((int64_t)(int32_t)(lr >> 32) - sd, -T_CLIP, T_CLIP);
            int32_t as = l < 0 ? -l : l, ar = r < 0 ? -r : r;
            if (ar > as) as = ar;
            if (as > pk) pk = as;
            L[i] += (l * lev) >> 15;
            R[i] += (r * lev) >> 15;
            int32_t mm = CLAMP(s, -T_CLIP, T_CLIP);
            if (heard) tv[i] = (int16_t)((mm * lev) >> 15);
            dsend[i] = (mm * ds) >> 15;
            rsend[i] += (((mm * rs) >> 15) * lev) >> 15;
        }
        t->srr_acc = sacc; t->srr_hold = sh; t->srr_prev = sp; t->msrr_hold = mh; t->msrr_prev = mp;
        t->mhp1 = h1; t->mhp2 = h2;
        t->peak = pk;
        if (pk > mon.peak[ti]) mon.peak[ti] = pk;
        PROF(2);
        return;
    }
    for (int i = 0; i < M2K_SUB; i++) {
        int32_t s = x[i];
        if (heard) to[i] = (int16_t)sat16(s);                             /* an FX machine's input: before VOL, so VOL 0
                                                                           * keeps a track out of the mix and in the FX */
        s = (int32_t)mul_sh(s, vol, 15);
        /* the track clips here, a channel at a time after the pan: one ceiling for both while the pan holds the
         * louder side at or under it */
        if (srr) {                     /* resampled, and joined by straight lines as the Monomachine does (its images
                                        * fall twice as far in dB as a plain hold's) */
            t->srr_acc += sinc;
            if (t->srr_acc >= 65536u) { t->srr_acc -= 65536u; t->srr_prev = t->srr_hold; t->srr_hold = s; }
            s = t->srr_prev + (int32_t)mul_sh(t->srr_hold - t->srr_prev, (int32_t)t->srr_acc, 16);
        }
        if (bits < 16) s = (sat16(s) >> (16 - bits)) * (1 << (16 - bits));
        int32_t sd = 0;
        if (m == M_FXC) sd = (int32_t)mul_sh((bside[ti][i] * (a0 + ((da * i) >> 4))) >> 15, vol, 15);   /* 2^15 by 2^15 */
        if (side) {
            int32_t ss = (int32_t)mul_sh(xs[i], a0 + ((da * i) >> 4), 15);
            ss = (int32_t)mul_sh(ss, vol, 15);
            if (srr) {
                if (t->srr_acc < sinc) { t->srr_prevs = t->srr_holds; t->srr_holds = ss; }   /* the mid's clock just ticked */
                ss = t->srr_prevs + (int32_t)mul_sh(t->srr_holds - t->srr_prevs, (int32_t)t->srr_acc, 16);
            }
            if (bits < 16) ss = (sat16(ss) >> (16 - bits)) * (1 << (16 - bits));
            sd = (int32_t)mul_sh(ss, T_PAN[64], 15);
        }
        if (mmix && mtype != 2) {      /* the MOD effect's side: VOL, two high-passes (the low end in the middle), SRR */
            int32_t ms = (int32_t)mul_sh(msd[i], vol, 15);
            ms -= zop_lp(&t->mhp1, ms);
            ms -= zop_lp(&t->mhp2, ms);
            if (srr) {
                if (t->srr_acc < sinc) { t->msrr_prev = t->msrr_hold; t->msrr_hold = ms; }
                ms = t->msrr_prev + (int32_t)mul_sh(t->msrr_hold - t->msrr_prev, (int32_t)t->srr_acc, 16);
            }
            if (bits < 16) ms = (sat16(ms) >> (16 - bits)) * (1 << (16 - bits));
            sd += (int32_t)mul_sh(ms, T_PAN[64], 15);
        }
        uint64_t lr = mul2_q15(s, gl, gr);
        int32_t l = (int32_t)CLAMP((int64_t)(int32_t)lr + sd, -T_CLIP, T_CLIP);
        int32_t r = (int32_t)CLAMP((int64_t)(int32_t)(lr >> 32) - sd, -T_CLIP, T_CLIP);
        int32_t as = l < 0 ? -l : l, ar = r < 0 ? -r : r;
        if (ar > as) as = ar;
        if (as > pk) pk = as;
        L[i] += (l * lev) >> 15;
        R[i] += (r * lev) >> 15;
        int32_t mm = CLAMP(s, -T_CLIP, T_CLIP);
        if (heard) tv[i] = (int16_t)((mm * lev) >> 15);
        dsend[i] = (mm * ds) >> 15;
        rsend[i] += (((mm * rs) >> 15) * lev) >> 15;
    }
    t->peak = pk;
    if (pk > mon.peak[ti]) mon.peak[ti] = pk;
    PROF(2);
}
