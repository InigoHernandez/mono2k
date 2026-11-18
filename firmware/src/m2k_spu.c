/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move alone (M2K_SPU: the FM-1's builds and the six-track browser and host compile none of it; the owner's,
 * 2026-11-17: too heavy for the FM-1). Two sounds of the PS1-era console's sound chip, after AFFINE's PS1-era bus
 * (github.com/InigoHernandez/affine, app/dsp/psx.c, MPL-2.0: its laws and numbers read, its float code written again
 * here in integers), on psx-spx's documentation of the chip (psx-spx.consoledev.net: the reverb's register sets, its
 * 39-tap resampler, the ADPCM filters and the 4-point gaussian, firmware/gen/m2k_spu_tables.h from tools/gen_spu.py):
 *   GRIT: the bus through the chip's sample codec (SPU-ADPCM, 28-sample units, five filters, an encoder CRUNCH can
 *     spoil) and its gaussian playback, at 8 to 22 kHz: a colour PERFORM plays, in and out over 10 ms, idle while off;
 *   the chip's reverb: its nine register sets at 22.05 kHz in 16-bit cells (64 KB), between the 39-tap half-band
 *     resampler's two passes, in place of the FDN when a pattern picks a type; asleep (nothing run) once it is quiet.
 * Phase 1 (docs/spu-move-plan.md): the two as modules with their own tests (host/m2k_spu_test.c); nothing calls them
 * yet. Included after m2k_dsp.c and before m2k_fx.c (the bus that will call it). */
#include "m2k_spu_tables.h"

static inline int32_t spu_sat(int32_t x) { return x > 32767 ? 32767 : x < -32768 ? -32768 : x; }   /* the chip's 16 bits */

/* ======================================================================================================= GRIT ---- */

#define GRIT_LEAD 32                     /* the playback behind the clock, codec samples: a unit (28), the gaussian's reach
                                        * past it (2) and 2 to spare */
#define GRIT_RING 64u                    /* decoded samples by codec index (a power of two dividing 65536: the clock's wrap) */
#define GRIT_WARM (GRIT_LEAD + 2)          /* codec samples taken, unheard, before it fades in: the playback reads a whole unit */
#define GRIT_FADE 37                     /* in and out, Q14 a sample: 443 samples, 10 ms */
typedef struct { int32_t x1, x2, y1, y2, r; } grit_bq_t;   /* a biquad's past, and its remainder */
typedef struct {
    grit_bq_t aa[2];                     /* the anti-alias low-pass: two biquads (a 4th-order Butterworth) */
    int32_t prev;                      /* the filtered sample before (a codec sample falls between it and this one) */
    int32_t hist[2];                   /* the decoder's history: old, older */
    int16_t blk[28];                   /* the unit being gathered */
    int16_t dec[GRIT_RING];              /* decoded */
} grit_ch_t;
static struct {
    grit_ch_t ch[2];
    uint32_t acc;                      /* the clock: codec samples, Q16 (wraps; the ring follows its low bits) */
    uint32_t made;                     /* codec samples taken since it started (its warmth) */
    uint32_t rnd;                      /* CRUNCH's dice (its own: the bus's side) */
    int32_t fade;                      /* Q14: from the bus as it came (0) to the codec's (16384) */
    uint8_t run, chr, nb;              /* running; the character running; samples gathered into the unit */
} spu_g;

/* a biquad low-pass, T_GRIT_AA's b0 a1 a2 (Q13), the remainder carried (no DC held, no limit cycle): y = b0 (x0 + 2 x1 + x2)
 * - a1 y1 - a2 y2. In 16 bits, out a little past them on a square's ringing (|a1| under 1.5, so the sum stays under
 * ~2^30 on the 11 and 8 kHz filters, less at 22: a word) */
static inline int32_t grit_bq(grit_bq_t *f, const int16_t *c, int32_t x)
{
    int32_t a = c[0] * (x + 2 * f->x1 + f->x2) - c[1] * f->y1 - c[2] * f->y2 + f->r;
    int32_t y = a >> 13;
    f->r = a & 8191;
    f->x2 = f->x1; f->x1 = x;
    f->y2 = f->y1; f->y1 = y;
    return y;
}

/* the encoder's choices for a unit; at rest (CRUNCH 0) the clean encoder: every filter ranked, the best two simulated,
 * the finest shift that holds the residual's peak, rounding */
typedef struct { uint8_t nf, sim, bias, shmax, trunc; } grit_enc_t;

static inline uint32_t grit_r8(void)
{
    spu_g.rnd ^= spu_g.rnd << 13; spu_g.rnd ^= spu_g.rnd >> 17; spu_g.rnd ^= spu_g.rnd << 5;
    return spu_g.rnd >> 24;
}

/* CRUNCH c (Q8, 0..256) into one unit's choices, each a step at a time with a fraction its chance (dice a unit: the
 * sound thickens smoothly with c): filters 5 -> 2 (the strongest, the low end's, go first), the simulation gone by
 * half, up to 2 shifts coarser, the finest shift 12 -> 4 (quiet tails turn to grain), the residual truncated toward 0
 * (a cheap encoder's: small residuals lost). AFFINE's law (crunch_enc) */
static void grit_crunch(grit_enc_t *e, int32_t c)
{
    int32_t v = (5 * 256 - 3 * c + (int32_t)grit_r8()) >> 8;
    e->nf = (uint8_t)(v < 2 ? 2 : v > 5 ? 5 : v);
    e->sim = (int32_t)grit_r8() >= 2 * c;
    e->bias = (uint8_t)((2 * c + (int32_t)grit_r8()) >> 8);
    v = (12 * 256 - 8 * c + (int32_t)grit_r8()) >> 8;
    e->shmax = (uint8_t)(v < 0 ? 0 : v > 12 ? 12 : v);
    e->trunc = (int32_t)grit_r8() < c;
}

/* the shift for a residual's peak: the finest (12: a step of one) whose largest nibble (7 steps) reaches it, CRUNCH's
 * bias coarser, no finer than its shmax */
static int grit_shift(int32_t peak, const grit_enc_t *e)
{
    int s = 12;
    while (s > 0 && (7 << (12 - s)) < peak) s--;
    s -= e->bias;
    return s < 0 ? 0 : s > e->shmax ? e->shmax : s;
}

/* one unit through filter f at shift sh: the decoder's own sample from each nibble, as the console hears it
 * (s = t 2^(12 - sh) + (old f0 + older f1 + 32) >> 6, clamped), into out; the error's magnitude back. h: the decoder's
 * history before the unit */
static uint32_t grit_try(const int16_t *b, const int32_t *h, int f, int sh, int trunc, int16_t *out)
{
    const int32_t f0 = T_SPU_PRED[f][0], f1 = T_SPU_PRED[f][1], k = 12 - sh;
    int32_t hp = k ? 1 << (k - 1) : 0, hn = hp, o = h[0], oo = h[1];   /* the nearest nibble: half a step, floored */
    uint32_t err = 0;
    if (trunc) { hp = 0; hn = (1 << k) - 1; }                          /* (toward 0) */
    for (int j = 0; j < 28; j++) {
        int32_t p = (o * f0 + oo * f1 + 32) >> 6, q = b[j] - p;     /* (|o f0 + oo f1| < 2^23; |q| < 2^17) */
        q = (q + (q < 0 ? hn : hp)) >> k;
        q = q < -8 ? -8 : q > 7 ? 7 : q;
        int32_t s = spu_sat(q * (1 << k) + p), d = s - b[j];
        err += (uint32_t)(d < 0 ? -d : d);
        oo = o;
        o = s;
        out[j] = (int16_t)s;
    }
    return err;
}

/* a unit encoded and decoded: the filters allowed ranked by their residual on the input (its size and its peak), the
 * best two simulated on the decoder's history at their shifts, the closer kept; h carried on */
static void grit_unit(const int16_t *b, int32_t *h, const grit_enc_t *e, int16_t *out)
{
    int16_t tmp[28];
    uint32_t e0 = ~0u, e1 = ~0u;
    int32_t pk[5] = {0, 0, 0, 0, 0};
    int c0 = 0, c1 = 0;
    for (int f = 0; f < e->nf; f++) {
        const int32_t f0 = T_SPU_PRED[f][0], f1 = T_SPU_PRED[f][1];
        int32_t o = h[0], oo = h[1], p = 0;
        uint32_t en = 0;
        for (int j = 0; j < 28; j++) {
            int32_t r = b[j] - ((o * f0 + oo * f1 + 32) >> 6);
            r = r < 0 ? -r : r;
            en += (uint32_t)r;                                       /* (28 x 2^17: a word) */
            if (r > p) p = r;
            oo = o;
            o = b[j];
        }
        pk[f] = p;
        if (en < e0) { e1 = e0; c1 = c0; e0 = en; c0 = f; }
        else if (en < e1) { e1 = en; c1 = f; }
    }
    if (e1 == ~0u) c1 = c0;
    uint32_t ea = grit_try(b, h, c0, grit_shift(pk[c0], e), e->trunc, out);
    if (e->sim && c1 != c0 && grit_try(b, h, c1, grit_shift(pk[c1], e), e->trunc, tmp) < ea)
        for (int j = 0; j < 28; j++) out[j] = tmp[j];
    h[0] = out[27];
    h[1] = out[26];
}

/* the playback at codec index i and fr / 256 past it: SMOOTH 2 the chip's gaussian (its oldest, older, old and new
 * samples, each product SAR 15, as psx-spx gives it), 1 linear, 0 the nearest sample */
static inline int32_t grit_play(const int16_t *d, uint32_t i, uint32_t fr, int sm)
{
    int32_t b = d[i & (GRIT_RING - 1)], c = d[(i + 1) & (GRIT_RING - 1)];
    if (sm == 2) {
        int32_t a = d[(i - 1) & (GRIT_RING - 1)], e = d[(i + 2) & (GRIT_RING - 1)];
        return ((T_SPU_GAUSS[0xFF - fr] * a) >> 15) + ((T_SPU_GAUSS[0x1FF - fr] * b) >> 15) +
               ((T_SPU_GAUSS[0x100 + fr] * c) >> 15) + ((T_SPU_GAUSS[fr] * e) >> 15);   /* (sums under 8000h: no clip) */
    }
    if (sm == 1) return b + (((c - b) * (int32_t)fr) >> 8);
    return fr < 128 ? b : c;
}

/* from rest, on the block's first frame: everything cleared, the filters and the decoder's history set to where the
 * sound is (no step into them), the clock at 0 */
static void grit_start(const int16_t *io, int chr)
{
    m_set(&spu_g, 0, sizeof spu_g);
    spu_g.chr = (uint8_t)chr;
    spu_g.rnd = 0x2545F491u;
    spu_g.run = 1;
    for (int c = 0; c < 2; c++) {
        grit_ch_t *h = &spu_g.ch[c];
        for (int s = 0; s < 2; s++) h->aa[s].x1 = h->aa[s].x2 = h->aa[s].y1 = h->aa[s].y2 = io[c];   /* (at rest there) */
        h->prev = h->hist[0] = h->hist[1] = io[c];
    }
}

/* GRIT over a block of the bus's output in place (n frames, interleaved, after the limiter: the codec's own 16 bits):
 * on while its key holds it, in character chr (T_GRIT_*: 0 PERFORM's); it starts unheard, fades in once its playback
 * reads what it encoded (GRIT_WARM), and out over 10 ms when let go, then idles. Its latency (the playback GRIT_LEAD codec
 * samples behind, the filter's) is crossfaded over, from the sound as it came: never a jump */
static void spu_grit_block(int16_t *io, int n, int on, int chr)
{
    if (!on && !spu_g.run) return;
    if (!spu_g.run) grit_start(io, chr);
    const int c = spu_g.chr, raw = T_GRIT_RAW[c], sm = T_GRIT_SMOOTH[c];
    const uint32_t inc = T_GRIT_INC[c], iinc = T_GRIT_IINC[c];
    const int32_t crunch = T_GRIT_CRUNCH[c];
    uint32_t acc = spu_g.acc, nb = spu_g.nb;
    int32_t fade = spu_g.fade;
    for (int i = 0; i < n; i++) {
        int32_t x[2] = {io[2 * i], io[2 * i + 1]}, y[2];
        for (int ch = 0; ch < 2; ch++) {
            y[ch] = x[ch];
            if (!raw) y[ch] = spu_sat(grit_bq(&spu_g.ch[ch].aa[1], T_GRIT_AA[c][1], grit_bq(&spu_g.ch[ch].aa[0], T_GRIT_AA[c][0], x[ch])));
        }
        uint32_t d = (0u - acc) & 0xFFFFu;               /* the next codec sample, Q16 of one past the sample before */
        while (d < inc) {
            int32_t ph = (int32_t)((d * iinc) >> 16);    /* its place between the two, Q14 (d iinc < 2^30) */
            for (int ch = 0; ch < 2; ch++) {
                grit_ch_t *h = &spu_g.ch[ch];
                h->blk[nb] = (int16_t)(h->prev + (((y[ch] - h->prev) * ph) >> 14));   /* (|y - prev| < 2^16) */
            }
            spu_g.made++;
            if (++nb == 28) {                            /* a unit: through the round trip, into the ring */
                grit_enc_t e = {5, 1, 0, 12, 0};
                int16_t out[28];
                uint32_t base = ((acc + d) >> 16) - 27u;
                if (crunch) grit_crunch(&e, crunch);
                for (int ch = 0; ch < 2; ch++) {
                    grit_unit(spu_g.ch[ch].blk, spu_g.ch[ch].hist, &e, out);
                    for (int j = 0; j < 28; j++) spu_g.ch[ch].dec[(base + (uint32_t)j) & (GRIT_RING - 1)] = out[j];
                }
                nb = 0;
            }
            d += 0x10000u;
        }
        spu_g.ch[0].prev = y[0];
        spu_g.ch[1].prev = y[1];
        const uint32_t pi = (acc >> 16) - GRIT_LEAD, fr = (acc >> 8) & 0xFFu;
        acc += inc;
        if (on) { if (spu_g.made >= GRIT_WARM) fade = fade + GRIT_FADE > 16384 ? 16384 : fade + GRIT_FADE; }
        else fade = fade > GRIT_FADE ? fade - GRIT_FADE : 0;
        if (!fade) continue;                             /* (warming, or faded out: the sound as it came) */
        for (int ch = 0; ch < 2; ch++) {
            int32_t w = spu_sat(grit_play(spu_g.ch[ch].dec, pi, fr, sm));
            io[2 * i + ch] = (int16_t)(fade == 16384 ? w : x[ch] + (((w - x[ch]) * fade) >> 14));   /* (< 2^16 x 2^14) */
        }
    }
    spu_g.acc = acc;
    spu_g.nb = (uint8_t)nb;
    spu_g.fade = fade;
    if (!on && !fade) spu_g.run = 0;
}

/* ================================================================================================= the reverb ---- */

/* the chip's reverb, psx-spx's formula: per 22.05 kHz tick, both sides at the same buffer address,
 *   same side:  [mLSAME] = (Lin + [dLSAME] vWALL - [mLSAME - 1]) vIIR + [mLSAME - 1]   (and R)
 *   other side: [mLDIFF] = (Lin + [dRDIFF] vWALL - [mLDIFF - 1]) vIIR + [mLDIFF - 1]   (and R from dLDIFF)
 *   early echo: Lout = vCOMB1 [mLCOMB1] + .. + vCOMB4 [mLCOMB4]
 *   late, two all-passes: Lout -= vAPF1 [mLAPF1 - dAPF1], [mLAPF1] = Lout, Lout = Lout vAPF1 + [mLAPF1 - dAPF1]; APF2
 * every product / 8000h, every value written saturated to 16 bits, the addresses relative to the buffer address (the
 * registers' 8-byte units, 4 cells each), which moves one cell a tick. "/ 8000h" truncates toward 0 here, as C divides
 * (srv_mz), so a loop under unity gain falls to 0 whatever its length (rounded, HALF ECHO's 100-tick loop at -0.96 held
 * a 12-step limit cycle for ever; floored, DC crept in); but the reflections' one-pole rounds (srv_mul), or a step
 * under 1 / vIIR would never move it and its last step would hold */
#define SRV_CELLS 32768u                /* the work area: 16-bit cells, 64 KB (every set fits: tools/gen_spu.py holds it) */
#define SRV_HI 38                       /* the send's history at 44.1 kHz: the decimator's 39 taps less the newest */
#define SRV_HO 19                       /* the reverb's ticks kept a side: the interpolator's 20 less the newest */
#define SRV_QUIET 4                     /* within -4..3 for a lap, with nothing sent: it empties and sleeps (rounding's
                                        * last steps; a window of 8 so the heads' (s + 4) can be or-ed and masked) */
#define SRV_FSTEP 2048                  /* a type's way in and out, Q15 a block: 16 blocks, 5.8 ms (the FDN's off) */
static int16_t srv_wk[SRV_CELLS] POOL;
static struct {
    uint32_t cur;                      /* the buffer address, cells */
    uint32_t a[32];                    /* the type's addresses, cells (its registers' x 4) */
    int16_t v[32];                     /* its volumes, signed 16-bit */
    uint32_t quiet;                    /* ticks with nothing sent and every head and output within SRV_QUIET */
    int32_t wall, ap1, ap2, iir, gout; /* now: they glide to DCAY's and DAMP's (vWALL, vAPF1, vAPF2, vIIR; the level
                                        * out, Q12) */
    int32_t fade;                      /* Q15: a type's way in and out */
    uint8_t type, sleep;               /* sounding: 1..SRV_N, 0 at rest; asleep (emptied, nothing run until a send) */
    int16_t ih[SRV_HI + M2K_SUB];       /* the send at 44.1 kHz: the last SRV_HI, then the block's */
    int16_t oh[2][SRV_HO + M2K_SUB / 2];/* the ticks a side: the last SRV_HO, then the block's */
} spu_r;

static void srv_clear(void)
{
    m_set(srv_wk, 0, sizeof srv_wk);
    m_set(spu_r.ih, 0, sizeof spu_r.ih);
    m_set(spu_r.oh, 0, sizeof spu_r.oh);
    spu_r.quiet = 0;
}

/* a type from rest (the area empty): its addresses and volumes, DCAY's wall, DAMP's colour and its level at once */
static void srv_start(int t, int dcay, int damp)
{
    const uint16_t *r = T_SRV_REG[t - 1];
    for (int k = 0; k < 32; k++) { spu_r.a[k] = (uint32_t)r[k] * 4u; spu_r.v[k] = (int16_t)r[k]; }
    spu_r.type = (uint8_t)t;
    spu_r.wall = T_SRV_WALL[t - 1][dcay];
    spu_r.ap1 = T_SRV_APF1[t - 1][dcay];
    spu_r.ap2 = T_SRV_APF2[t - 1][dcay];
    spu_r.iir = T_SRV_IIR[t - 1][damp];
    spu_r.gout = T_SRV_GOUT[t - 1][dcay];
    spu_r.fade = 0;
    spu_r.quiet = 0;
}

/* a coefficient an eighth of the way to where it goes, a block (a step at least) */
static inline int32_t srv_glide(int32_t now, int32_t to)
{
    int32_t d = to - now;
    return d > 8 || d < -8 ? now + d / 8 : to;
}

static inline int32_t srv_mul(int32_t a, int32_t v) { return (a * v + 16384) >> 15; }   /* (|a|, |v| <= 2^15) */
static inline int32_t srv_mz(int32_t a, int32_t v) { int32_t p = a * v; return (p + ((p >> 31) & 32767)) >> 15; }

/* a block (M2K_SUB samples) of the reverb, its wet added into L, R (the bus's sums): type 1..SRV_N (0 none), DCAY and DAMP
 * on their laws (T_SRV_WALL, T_SRV_APF1/2, T_SRV_IIR), rs the tracks' sends (mono, as the FDN hears them). DCAY 0 or no type: the wet
 * fades out over 5.8 ms and the area empties; another type: the old one so, then the new one from silence. Asleep
 * nothing runs until a send comes */
static void spu_rv_block(int type, int dcay, int damp, const int32_t *rs, int32_t *L, int32_t *R)
{
    const int want = dcay && type > 0 && type <= SRV_N ? type : 0;
    if (!spu_r.type && !want) return;
    int32_t in = 0;
    for (int i = 0; i < M2K_SUB; i++) in |= rs[i];
    if (spu_r.sleep && !in && want == spu_r.type) return;
    if (spu_r.sleep && want != spu_r.type) spu_r.type = 0;        /* (asleep the area is empty: another type starts at once) */
    spu_r.sleep = 0;
    if (!spu_r.type) {
        if (!want) return;
        srv_start(want, dcay, damp);
    }
    const int32_t f0 = spu_r.fade;                          /* (the block's ramp: from f0 to spu_r.fade) */
    if (want != spu_r.type) {                               /* out, the old type playing on to 0 (then emptied, below) */
        spu_r.fade = spu_r.fade > SRV_FSTEP ? spu_r.fade - SRV_FSTEP : 0;
    } else {
        spu_r.fade = spu_r.fade + SRV_FSTEP > 32767 ? 32767 : spu_r.fade + SRV_FSTEP;
        spu_r.wall = srv_glide(spu_r.wall, T_SRV_WALL[want - 1][dcay]);
        spu_r.ap1 = srv_glide(spu_r.ap1, T_SRV_APF1[want - 1][dcay]);
        spu_r.ap2 = srv_glide(spu_r.ap2, T_SRV_APF2[want - 1][dcay]);
        spu_r.iir = srv_glide(spu_r.iir, T_SRV_IIR[want - 1][damp]);
        spu_r.gout = srv_glide(spu_r.gout, T_SRV_GOUT[want - 1][dcay]);
    }
    /* the sends into the cells (x SRV_IN, saturated as the chip's input would be) */
    int16_t *ih = spu_r.ih;
    for (int i = 0; i < M2K_SUB; i++) ih[SRV_HI + i] = (int16_t)spu_sat(rs[i] * SRV_IN);   /* (eight sends: |rs| < 2^19) */
    int16_t *w = srv_wk;
    const uint32_t M = SRV_CELLS - 1;
    const uint32_t *a = spu_r.a;
    const int16_t *v = spu_r.v;
    const int32_t wall = spu_r.wall, iir = spu_r.iir, vlin = v[30], vrin = v[31];
    const int32_t vc1 = v[3], vc2 = v[4], vc3 = v[5], vc4 = v[6], vap1 = spu_r.ap1, vap2 = spu_r.ap2;
    const uint32_t mls = a[10], mrs = a[11], dls = a[16], drs = a[17], mld = a[18], mrd = a[19], dld = a[24], drd = a[25];
    const uint32_t lap1 = a[26], rap1 = a[27], lap2 = a[28], rap2 = a[29], lap1d = lap1 - a[0], rap1d = rap1 - a[0],
                   lap2d = lap2 - a[1], rap2d = rap2 - a[1];
    uint32_t c = spu_r.cur;
    int32_t big = 0;
#define AT(o) w[(c + (o)) & M]
    for (int t = 0; t < M2K_SUB / 2; t++, c++) {
        /* down to 22.05 kHz: the 39 taps over the send's last 39 samples (h[0] the oldest), the even ones in mirrored
         * pairs and the centre (each sum under 2^31) */
        const int16_t *h = &ih[2 * t + 1];
        int32_t acc = 0x4000 * h[19];
        for (int k = 0; k < 10; k++) acc += T_SPU_RS[k] * (h[2 * k] + h[38 - 2 * k]);
        const int32_t x = spu_sat((acc + 16384) >> 15);
        const int32_t l = srv_mz(x, vlin), r = srv_mz(x, vrin);
        int32_t p, s;
        p = AT(mls - 1); s = spu_sat(srv_mul(spu_sat(l + srv_mz(AT(dls), wall) - p), iir) + p); AT(mls) = (int16_t)s; big |= s + SRV_QUIET;
        p = AT(mrs - 1); s = spu_sat(srv_mul(spu_sat(r + srv_mz(AT(drs), wall) - p), iir) + p); AT(mrs) = (int16_t)s; big |= s + SRV_QUIET;
        p = AT(mld - 1); s = spu_sat(srv_mul(spu_sat(l + srv_mz(AT(drd), wall) - p), iir) + p); AT(mld) = (int16_t)s; big |= s + SRV_QUIET;
        p = AT(mrd - 1); s = spu_sat(srv_mul(spu_sat(r + srv_mz(AT(dld), wall) - p), iir) + p); AT(mrd) = (int16_t)s; big |= s + SRV_QUIET;
        int32_t lo = spu_sat(srv_mz(AT(a[12]), vc1) + srv_mz(AT(a[14]), vc2) + srv_mz(AT(a[20]), vc3) + srv_mz(AT(a[22]), vc4));
        int32_t ro = spu_sat(srv_mz(AT(a[13]), vc1) + srv_mz(AT(a[15]), vc2) + srv_mz(AT(a[21]), vc3) + srv_mz(AT(a[23]), vc4));
        int32_t q;
        q = AT(lap1d); lo = spu_sat(lo - srv_mz(q, vap1)); AT(lap1) = (int16_t)lo; lo = spu_sat(srv_mz(lo, vap1) + q);
        q = AT(rap1d); ro = spu_sat(ro - srv_mz(q, vap1)); AT(rap1) = (int16_t)ro; ro = spu_sat(srv_mz(ro, vap1) + q);
        q = AT(lap2d); lo = spu_sat(lo - srv_mz(q, vap2)); AT(lap2) = (int16_t)lo; lo = spu_sat(srv_mz(lo, vap2) + q);
        q = AT(rap2d); ro = spu_sat(ro - srv_mz(q, vap2)); AT(rap2) = (int16_t)ro; ro = spu_sat(srv_mz(ro, vap2) + q);
        spu_r.oh[0][SRV_HO + t] = (int16_t)lo;
        spu_r.oh[1][SRV_HO + t] = (int16_t)ro;
        big |= (lo + SRV_QUIET) | (ro + SRV_QUIET);
    }
#undef AT
    spu_r.cur = c & M;
    /* up to 44.1 kHz: the even taps on the tick's own sample (x2: the zeros between), the centre alone (2 x 4000h /
     * 8000h: the tick 9 back) on the one after; the level out and the way in or out ramped across the block */
    const int32_t g1 = (spu_r.gout * spu_r.fade) >> 15, g0 = (spu_r.gout * f0) >> 15;   /* (both under 2^15: a word) */
    for (int t = 0; t < M2K_SUB / 2; t++) {
        int32_t y[2][2];
        for (int sd = 0; sd < 2; sd++) {
            const int16_t *o = &spu_r.oh[sd][t + 1];        /* o[0] the oldest .. o[19] this tick */
            int32_t acc = 0;
            for (int k = 0; k < 10; k++) acc += T_SPU_RS[k] * (o[k] + o[19 - k]);
            y[sd][0] = spu_sat((acc + 8192) >> 14);
            y[sd][1] = o[10];
        }
        for (int j = 0; j < 2; j++) {
            const int i = 2 * t + j;
            const int32_t g = g0 + (((g1 - g0) * (i + 1)) >> 4);   /* (Q12, < 2^15; |g1 - g0| x 16 < 2^20) */
            L[i] += (y[0][j] * g) >> 12;
            R[i] += (y[1][j] * g) >> 12;
        }
    }
    m_cpy(spu_r.ih, spu_r.ih + M2K_SUB, SRV_HI * sizeof spu_r.ih[0]);
    for (int sd = 0; sd < 2; sd++)
        for (int k = 0; k < SRV_HO; k++) spu_r.oh[sd][k] = spu_r.oh[sd][k + M2K_SUB / 2];
    if (want != spu_r.type) {                               /* faded out: emptied; another type from silence next block */
        if (!spu_r.fade) {
            srv_clear();
            spu_r.type = 0;
            if (want) srv_start(want, dcay, damp);
        }
        return;
    }
    /* quiet: nothing sent, every head and output within -4..3 for a lap of the farthest head and the resamplers: what
     * is left is the rounding's last steps; emptied, it sleeps (a step of 4 at most in the cells, under -78 dBFS) */
    spu_r.quiet = in || ((uint32_t)big & ~(uint32_t)(2 * SRV_QUIET - 1)) ? 0 : spu_r.quiet + M2K_SUB / 2;
    if (spu_r.quiet > (uint32_t)T_SRV_SPAN[spu_r.type - 1] + SRV_HI && spu_r.fade == 32767) { srv_clear(); spu_r.sleep = 1; }
}

/* both silent and at rest, every state and the work area cleared: m2k_init's to call (phase 2), so no tail or warm codec
 * survives an init */
static void spu_init(void)
{
    m_set(&spu_g, 0, sizeof spu_g);
    m_set(&spu_r, 0, sizeof spu_r);
    m_set(srv_wk, 0, sizeof srv_wk);
}
