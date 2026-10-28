/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's effects. Each track has its own delay, as on the Monomachine (time in 1/64 notes, feedback, and a
 * base / width band in the loop); here it runs at half rate in 8-bit mu-law, which is the memory budget and the
 * colour. The bus is the lofi part: a reverb at 22 kHz (a feedback delay network, 0.3 to 24 s), a glitch that grows
 * from rare quiet repeats to wild, tape wow and flutter, a 4-bit ADPCM crunch, hiss and crackle, and a drive into the
 * soft clip; and PERFORM's effects over them while FX is held. */

static uint8_t mulaw_enc(int32_t x)
{
    int sign = 0;
    if (x < 0) { sign = 0x80; x = -x; }
    if (x > 32635) x = 32635;
    x += 0x84;                         /* 0x84 .. 0x7FFF: the segment is the top bit's place over bit 7 */
    int exp = x >= 0x800 ? 4 : 0;
    if (x >= (0x200 << exp)) exp += 2;
    if (x >= (0x100 << exp)) exp += 1;
    int man = (x >> (exp + 3)) & 0x0F;
    return (uint8_t)~(sign | (exp << 4) | man);
}

static uint32_t sps_q8 = 1411200;      /* samples per sixteenth, Q8; the sequencer sets it */
static uint8_t tape_rolling;           /* the sequencer runs or a voice sounds: the hiss and dust follow it */

/* DUB (PERFORM): the bus thrown into a borrowed track's delay (m2k_fx.c's dub_line), as a delay's own values: a
 * dotted eighth (3/16, the line's clock slowing past its length, darker), feedback 0.75, the band 24 .. 84, ping-pong
 * from the left, at LEVL 110; the send a block late, the echoes out apart (they must not feed the send back) */
#define DUB_TIM 48
#define DUB_FB 48
#define DUB_BAS 24
#define DUB_WID 60
#define DUB_PAN 16
#define DUB_LEV 110
static int32_t (*dub_io(int ti))[M2K_SUB];   /* track ti's DUB send, echoes left and right if the line is DUB's */
static int dub_on(void);
static void dub_end(void);

/* one line's sample at a fractional position, Q8 of a line sample (unsigned: it wraps, after minutes) */
static inline int32_t dl_read(const uint8_t *b, uint32_t mask, uint32_t pos)
{
    uint32_t j = pos >> 8;
    int32_t f = (int32_t)(pos & 255);
    return (T_MULAW[b[j & mask]] * (256 - f) + T_MULAW[b[(j + 1) & mask]] * f) >> 8;
}

/* a line's sample, encoded. Under GATE (a feedback under unity's dead band: rounded to the nearest mu-law step, an
 * echo DFB / 64 as loud as the last can round back to it, a pass after a pass, for ever) it takes the step below
 * it instead, so a tail ends in silence */
static inline uint8_t mulaw_line(int32_t x, int32_t gate)
{
    uint8_t q = mulaw_enc(x);
    int32_t a = x < 0 ? -x : x, d = T_MULAW[q];
    if (a < gate && q != 0xFF && q != 0x7F && (d < 0 ? -d : d) >= a) q++;   /* (a code on is a step nearer silence) */
    return q;
}

/* the track's delay, after the Monomachine's (measured): DTIM in 256ths of a note plus 7.6 ms; the send |d| / 64
 * (format 17: the Monomachine's square, (d / 64)^2, left a third of the knob near silent), negative keeping the
 * source's side, positive ping-pong; feedback DFB / 64, unity at 64 and saturating above; the
 * echoes through a one-pole HP at DBAS and a one-pole LP at DBAS + DWID. The line runs at half rate in 8-bit mu-law;
 * a time longer than the line slows its clock instead, the echoes darker, as a tape or a BBD would. Ping-pong
 * keeps a sum and a difference line, each half the memory */
static void delay_block(int ti, const int32_t *restrict in, int32_t *restrict L, int32_t *restrict R)
{
    trk_t *t = &trk[ti];
    int sent = t->dsent;
    t->dsent = 0;
    /* DUB's line: the bus's send with the track's own, DUB's values, its echoes apart (its own sum) */
    int32_t (*dio)[M2K_SUB] = dub_io(ti), din[M2K_SUB];
    if (dio) {
        for (int i = 0; i < M2K_SUB; i++) din[i] = in[i] + dio[0][i];
        in = din;
        sent |= dub_on();
        L = dio[1];
        R = dio[2];
    }
    if (t->dsleep && !sent) {          /* quiet last block and nothing sent since: it would be again, as it stands */
        if (dio && !dub_on()) dub_end();              /* (DUB's echoes over: the line the track's again) */
        return;
    }
    /* the layout from the kit's DSND, not its locks' or LFOs' (each crossing of 64 wiped the lines), and only past 64
     * by a step (a kit turned to 64 keeps the echoes it has) */
    int kd = PAT(cur_pat)->kit[ti][P_DSND], dual = dio ? 1 : t->dmode ? kd >= 64 : kd > 64;
    if (dual != t->dmode) {            /* the lines change their layout: start them empty */
        m_set(t->dl, 0x7F, DL_LEN);
        t->dmode = (uint8_t)dual;
        t->dacc[0] = t->dacc[1] = t->dn = 0;
        t->dhp[0] = t->dhp[1] = t->dlp[0] = t->dlp[1] = 0;
        t->dhr[0] = t->dhr[1] = t->dlr[0] = t->dlr[1] = 0;
        t->dquiet = DL_LEN;
    }
    uint32_t n = dual ? DL_LEN / 2 : DL_LEN, mask = n - 1;
    /* nothing sent, every entry of the line written silent since, the filters settled: nothing to hear, nothing to
     * work out (most tracks, most of the time; the FM-1's time) */
    int quiet = t->dquiet >= n;
    for (int i = 0; quiet && i < M2K_SUB; i++) quiet = in[i] == 0;
    if (quiet) {
        int32_t s = 0;
        for (int c = 0; c < 2; c++) {
            int32_t v[4] = {t->dhp[c], t->dlp[c], t->dprev[c], t->dacc[c]};
            for (int k = 0; k < 4; k++) s |= v[k] > 3 || v[k] < -3;
        }
        if (!s) {
            t->dhp[0] = t->dhp[1] = t->dlp[0] = t->dlp[1] = t->dprev[0] = t->dprev[1] = t->dacc[0] = t->dacc[1] = 0;
            t->dhr[0] = t->dhr[1] = t->dlr[0] = t->dlr[1] = 0;
            mon.dly[ti] = 0;
            if (dio && !dub_on()) dub_end();
            t->dsleep = 1;
            return;
        }
    }
    t->dsleep = 0;
    /* the time, half-rate samples Q8: whole samples once it settles (a read between two samples dulls the top) */
    int32_t target = dio ? (int32_t)(((uint32_t)DUB_TIM * (sps_q8 >> 5) + 128) & ~255u)
                         : ((int32_t)((uint32_t)P7(t, P_DTIM) * (sps_q8 >> 5)) + (167 << 8) + 128) & ~255;
    int32_t dd = target - t->dt_q8;
    t->dt_q8 = dd > -64 && dd < 64 ? target : t->dt_q8 + (dd >> 5);
    uint32_t th = (uint32_t)t->dt_q8 >> 8, room = n - 4;
    uint32_t rate = th <= room ? 65536u : (room << 16) / th;                  /* the line's clock, Q16 */
    int32_t back = th <= room ? t->dt_q8 : (int32_t)(room << 8);               /* the delay in line samples, Q8 */
    int dfb = dio ? DUB_FB : P7(t, P_DFB);
    /* mulaw_line's gate: the dead band of mu-law's first segment is 256 / (64 - DFB), the next ones' to 4 x that (DFB 62
     * and 63, a band wide open, can still hold an echo a little over it) */
    int32_t fb = dfb * 512, gate = dfb < 64 ? 1024 / (64 - dfb) : 0;
    int dbas = dio ? DUB_BAS : P7(t, P_DBAS);
    int32_t ahp = T_DLY_HP[dbas], alp = T_DLY_LP[CLAMP(dbas + (dio ? DUB_WID : P7(t, P_DWID)), 0, 127)];
    int pan = dio ? DUB_PAN : P7(t, P_PAN);
    int32_t gl = T_PAN[pan], gr = T_PAN[127 - pan], lev = T_VOL[dio ? DUB_LEV : P7(t, P_LEVL)];
    uint8_t *b = t->dl;
    int32_t pk = 0;
    for (int i = 0; i < M2K_SUB; i += 2) {
        int32_t x = (in[i] + in[i + 1]) >> 1;
        uint32_t pos = (t->dw << 8) + (t->dph >> 8) - (uint32_t)back;
        int32_t e[2];
        for (int c = 0; c <= dual; c++) {
            /* the one-poles carry their remainders (as zop_lp does): without, they stalled up to 252 off zero and the
             * delay never went quiet */
            int32_t y = dl_read(b + c * n, mask, pos);
            int32_t q = (y - t->dhp[c]) * ahp + t->dhr[c];
            t->dhp[c] += q >> 15;
            t->dhr[c] = q & 32767;
            q = (y - t->dhp[c] - t->dlp[c]) * alp + t->dlr[c];
            t->dlp[c] += q >> 15;
            t->dlr[c] = q & 32767;
            e[c] = t->dlp[c];
        }
        int32_t l, r;
        if (dual) {                    /* sum and difference: the echo crosses sides on every pass */
            /* the echo comes off its filters past 16 bits and DFB runs to 1.75: its product takes 64 bits */
            uint64_t ef = mul2_q15(fb, e[0], e[1]);
            t->dacc[0] += ((x * (gl + gr)) >> 15) + (int32_t)ef;
            t->dacc[1] += ((x * (gl - gr)) >> 15) - (int32_t)(ef >> 32);
            l = (e[0] - e[1]) >> 1;
            r = (e[0] + e[1]) >> 1;
        } else {
            t->dacc[0] += x + (int32_t)mul_sh(e[0], fb, 15);
            l = (e[0] * gl) >> 15;
            r = (e[0] * gr) >> 15;
        }
        t->dn++;
        t->dph += rate;
        if (t->dph >= 65536u) {        /* the clock ticks: the inputs since the last tick, averaged, go in */
            t->dph -= 65536u;
            int loud = 0;
            for (int c = 0; c <= dual; c++) {
                uint8_t q = mulaw_line(t->dacc[c] / t->dn, gate);
                b[c * n + (t->dw & mask)] = q;
                loud |= q != 0xFF && q != 0x7F;           /* mu-law's two silences */
            }
            t->dquiet = loud ? 0 : t->dquiet < 0x40000000u ? t->dquiet + 1 : t->dquiet;
            t->dw++;
            t->dacc[0] = t->dacc[1] = t->dn = 0;
        }
        l = (l * lev) >> 15;
        r = (r * lev) >> 15;
        L[i] += (t->dprev[0] + l) >> 1;
        R[i] += (t->dprev[1] + r) >> 1;
        L[i + 1] += l;
        R[i + 1] += r;
        t->dprev[0] = l;
        t->dprev[1] = r;
        int32_t a = (l < 0 ? -l : l) + (r < 0 ? -r : r);
        if (a > pk) pk = a;
    }
    if (!dio) mon.dly[ti] = pk;
}

/* ------------------------------------------------------------------------------------------------ the bus */

#define GL_LEN 16384u
static int16_t gl_buf[GL_LEN][2] POOL;
static int16_t tp_buf[2048][2] POOL;
/* the reverb's four lines at their whole size (22 kHz: 31 .. 47 ms, primes) and its two input diffusers, all in one
 * ring with one write place: each sits its longest (and the sample read past it) behind the next, so none reaches
 * another's, and none needs a wrap of its own */
static const uint16_t RV_LEN[4] = {683, 797, 907, 1031}, RD_LEN[2] = {149, 211};
#define RV_N 4096u
static const uint16_t RV_AT[6] = {685, 685 + 799, 685 + 799 + 909, 685 + 799 + 909 + 1033,
                                  685 + 799 + 909 + 1033 + 151, 685 + 799 + 909 + 1033 + 151 + 213};   /* (3790) */
static int16_t rv_buf[RV_N] POOL;
static const int8_t ADPCM_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static struct {
    /* reverb */
    uint32_t rw;                       /* the ring's write place */
    int32_t rlp[4], rlr[4];            /* the lines' damping and its remainders */
    int32_t rq[4], rdq[2], rrq[2];     /* the lines', the diffusers' and the swing's remainders */
    int32_t rbw, rbr;                  /* the input's top, and its remainder */
    int32_t rgo;                       /* its level out, Q12: T_RV_GO's, glided */
    /* DUB: the track whose line it has + 1 (0 none), its send now (Q15), the mix it sends for the next block and its
     * echoes this one */
    uint8_t dub_t;
    int32_t dub_snd, dub_io[3][M2K_SUB];
    /* SHIMMER: the wash on (held, then until it is quiet), the bus's send into it (Q15), its octave's grain phase,
     * the blocks it has been quiet */
    uint8_t sh_on;
    int32_t sh_snd;
    uint32_t sh_ph, sh_q;
    int32_t sh_hp;                     /* its high-pass's low part */
    int32_t rv_l, rv_r;                /* the last output, for the half-rate's in-between */
    uint32_t rsz;                      /* the room's size now, Q12 (it glides to DCAY's) */
    uint32_t rmph;                     /* the swing's phase */
    uint8_t rv_v, rv_d, rsleep;        /* DCAY and DAMP the coefficients were worked out for, rv_s the room */
    uint32_t rv_s;
    int32_t rgh, ra;                   /* the lines' gain / 2 and damping, Q15 */
    int32_t rfade;                     /* turned off: the tail's way out, Q15 */
    uint32_t rquiet;                   /* half-rate samples with nothing in and the lines within a step */
    /* glitch: the ring (half rate) and the one event it plays, a GLCH repeat or a PERFORM key's */
    uint32_t gw;                       /* the ring's write place */
    uint32_t g_start, g_len;           /* the slice (or, stopping, the read's start) and its length */
    uint32_t g_pos, g_rate;            /* the read: Q12 into the slice; its step Q24 (1 << 24 at pitch) */
    uint32_t g_left, g_dec;            /* half-rate samples before it fades out (G_HELD: until let go); a stop's slowing */
    int32_t g_env, g_wet, g_dry;       /* its level, Q10, fading in and out; the wet's and the dry's under it, Q15 */
    uint8_t g_on, g_mode, g_phase, g_pk, g_next, g_spin;   /* g_pk, g_next: the PERFORM key holding it, waiting (+1) */
    int32_t gacc_l, gacc_r, gout_l, gout_r, gprev_l, gprev_r;
    uint32_t g_grid[3];                /* the ring's place at the last sixteenth, eighth and beat */
    uint32_t grng;                     /* the glitch's own dice: the trigs' chances keep theirs */
    uint16_t g_fi, g_fo;               /* the event's fade in and out, Q10 a sample */
    int32_t g_dip;                     /* a stop let go near rest: its dip before the landing, Q15 */
    /* FREEZE and the octaves: three Hann grains a third apart, g_gl long (g_wst the window's angle a sample); a
     * freeze's grains each start somewhere in its slice */
    uint32_t g_gl, g_g3, g_gph, g_wst, g_gp[3];
    uint8_t g_sc[2], g_rl;             /* SCATTER's slice now and before (0-3, 4 reversed, 8 its first half twice); ROLL's laps */
    int32_t g_srt;                     /* SPIN: its speed, signed, Q24; its ticks, the pull's and the spin's */
    uint32_t g_st, g_t1, g_t2;
    int32_t gtd;                       /* GATE: its depth, Q15; its place in the sixteenth with no grid */
    uint32_t gt_ph;
    int32_t g_tgt, g_lev, g_agc, g_agt;   /* a freeze's level: wanted, followed; its gain (Q15) and the gain it glides to */
    uint8_t g_nb;                      /* (blocks, for its gain) */
    int32_t olev;                      /* the output's level, followed over ~0.4 s (every 8th sample's, Q4) */
    /* LPF and HPF (PERFORM): each a ZDF two-pole a side, its place (Q8 of T_FG's 1/96 octaves) and its share */
    zsvf_t fl[2][2];
    int32_t fpos[2], fmix[2], fgain[2];
    uint8_t fnew;                      /* the low-pass starting: its state set to the first sample (no step in) */
    /* tape */
    uint32_t tw, wow, flut;
    int32_t tlp_l, tlp_r;
    int32_t tq;                        /* the depth now (it glides to the control's or WOW's) */
    int32_t tp_mix;                    /* the tape's share, Q15: all of it while TAPE is up, WOW's way in and out */
    uint32_t tp_fill;                  /* samples into the line since it started (WOW from TAPE 0) */
    /* adpcm */
    int32_t ad_pred[2], ad_idx[2], ad_hold[2];
    uint16_t ad_acc;                   /* Q8 samples since the 4-bit sample was taken */
    int32_t ad_mixq;                   /* the mix now, Q8 of the 0..127 one (around CRUSH it glides) */
    uint8_t ad_perf, ad_warm;          /* CRUSH held (until its mix is back); blocks of its coder running unheard */
    /* drive: PERFORM's, glided, its level held to the dry's */
    int32_t dv, dv_e0, dv_e1, dv_g, dv_gp;   /* the gain now, Q8; the two levels; the hold, Q15, and where it ramped to */
    /* hiss */
    uint32_t hs;
    int32_t hlp, crackle, hgain;       /* hgain: Q15, follows tape_rolling over ~0.3 s */
} bus;

static inline int32_t adpcm_step(int c, int32_t x)
{
    int32_t step = T_ADPCM_STEP[bus.ad_idx[c]], diff = x - bus.ad_pred[c];
    int code = 0;
    if (diff < 0) { code = 8; diff = -diff; }
    int32_t delta = step >> 3;
    if (diff >= step) { code |= 4; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step) { code |= 2; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step) { code |= 1; delta += step; }
    bus.ad_pred[c] = sat16(bus.ad_pred[c] + ((code & 8) ? -delta : delta));
    bus.ad_idx[c] = CLAMP(bus.ad_idx[c] + ADPCM_IDX[code & 7], 0, 88);
    return bus.ad_pred[c];
}

/* ------------------------------------------------------------------------------ the glitch and PERFORM */

/* the glitch plays one event at a time from the ring: a GLCH repeat (bus_step) or a PERFORM key's (bus_perform). Each
 * fades in and out over GL_XF, and a slice's seam (its end into its start) crossfades over as much, so nothing cuts.
 * The modes: a slice repeated, reversed, gated, an octave up or down, halved each time round (a roll), slowing (a
 * tape stop on the slice); and PERFORM's stop, the live sound slowed to a halt as a tape */
enum { GM_RPT, GM_REV, GM_GATE, GM_OCTU, GM_OCTD, GM_ROLL, GM_TSTOP, GM_STOP,      /* (GLCH's dice pick 1..6) */
       GM_FRZ, GM_PSU, GM_PSD, GM_SCAT, GM_PROLL, GM_SPIN, GM_COUNT };             /* PERFORM's bank B */
#define GL_XF 64u                      /* 2.9 ms */
#define GL_MAX (GL_LEN - 4 * GL_XF - 512)   /* the longest slice: a quarter from 85 BPM (slower, an eighth) */
#define G_HELD 0x7FFFFFFFu
#define G_UNITY (1u << 24)

/* PERFORM (FX held): a black key's effect while it is held. Repeats of 1/4 .. 1/32 note, the last beat reversed, a
 * tape stop, a gated stutter (1/32 hits of the last sixteenth): each locked to the sequencer's grid (a repeat's slice
 * the one that began on the last beat, eighth or sixteenth, its first time round the sound coming in; the reverse the
 * beat that ended there), played from where that puts it now, so the repeats fall on the beat however late the key;
 * the crush, the wow and the drive at their most, each gliding in and out, the drive's level held to the dry's; a
 * kill (the bus silent). Let go, each fades out (the stop spins back up first) */
/* Bank B (FX held, OCT+): a freeze (the last eighth held as a sustain, three Hann grains wandering in it), a scatter,
 * a roll, the bus an octave down or up (three Hann grains through the ring), a low-pass and a high-pass swept over a
 * bar while held (gliding open again let go), a gate, a dub throw, a shimmer and a vinyl spin-back */
enum { PF_R4, PF_R8, PF_R16, PF_R32, PF_REV, PF_STOP, PF_STUT, PF_CRSH, PF_WOW, PF_DRIV, PF_KILL,
       PF_FRZ, PF_SCAT, PF_ROLL, PF_OCTD, PF_OCTU, PF_LPF, PF_HPF, PF_GATE, PF_DUB, PF_SHIM, PF_SPIN, PF_COUNT };
#define PF_BANK 11                     /* the black keys: bank A's 0..10, bank B's 11..21 */
static const char *const PF_NAME[PF_COUNT] = {"RPT 1/4", "RPT 1/8", "RPT 1/16", "RPT 1/32", "REVERSE", "STOP", "STUTTER",
                                              "CRUSH", "WOW", "DRIVE", "KILL", "FREEZE", "SCATTER", "ROLL", "OCT DN",
                                              "OCT UP", "LPF", "HPF", "GATE", "DUB", "SHIMMER", "SPIN"};
static struct { uint8_t on[PF_COUNT]; int32_t kill; } perf = {{0}, 32767};
/* the effects that play from the glitch ring, one at a time */
static int pf_ring(int k)
{
    return k <= PF_STUT || k == PF_FRZ || k == PF_SCAT || k == PF_ROLL || k == PF_OCTD || k == PF_OCTU || k == PF_SPIN;
}

static int seq_running(void);

static uint32_t gl_rnd(void) { uint32_t x = bus.grng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return bus.grng = x; }

static void gl_start(uint32_t start, uint32_t len, uint32_t pos, int mode, uint32_t left, int32_t wet, int32_t dry)
{
    bus.g_start = start;
    bus.g_len = len;
    bus.g_pos = pos << 12;
    bus.g_rate = G_UNITY;
    bus.g_left = left;
    bus.g_mode = (uint8_t)mode;
    bus.g_wet = wet;
    bus.g_dry = dry;
    bus.g_spin = 0;
    bus.g_fi = bus.g_fo = 8;           /* 2.9 ms */
    bus.g_on = 1;
}

/* FREEZE and the octaves' grains, g long: their window's step and their thirds */
static void gl_grains_init(uint32_t g)
{
    bus.g_gl = g;
    bus.g_g3 = g / 3;
    bus.g_gph = 0;
    bus.g_wst = 0x80000000u / g;       /* (half a turn over a grain: sin^2 is the Hann window) */
}

#define FRZ_MIN 2048u                  /* FREEZE's slice, half-rate samples: an eighth, within 93 .. 279 ms */
#define FRZ_MAX 6144u
#define PS_UP 1536u                    /* the octaves' grains: up 70 ms (the read 70 ms back at most), down 93 ms (46) */
#define PS_DN 2048u
#define PS_DMIN 2u                     /* (the read a sample and its neighbour behind the write) */
static void gl_perform(int k)
{
    uint32_t six = sps_q8 >> 9;                           /* a sixteenth in half-rate samples */
    if (k == PF_FRZ) {                                    /* the last eighth, its grains a half of it each */
        uint32_t r = CLAMP(six * 2, FRZ_MIN, FRZ_MAX);
        gl_start(bus.gw - r - 4, r, 0, GM_FRZ, G_HELD, 32768, 0);
        gl_grains_init(r / 2);
        /* held at the last beat's level, not the eighth's (a kick or a chord's attack held as long as the key would
         * be the loudest of the bar): their mean magnitudes, x 1/4 .. 1.4 */
        uint32_t bt = six * 4 > GL_MAX ? GL_MAX : six * 4, sb = 0, sr = 0;
        for (uint32_t i = 0; i < bt; i++) {               /* (|x| < 2^15, bt < 2^14: the sums a word) */
            const int16_t *x = gl_buf[(bus.gw - 4 - bt + i) & (GL_LEN - 1)];
            uint32_t a = (uint32_t)(x[0] < 0 ? -x[0] : x[0]) + (uint32_t)(x[1] < 0 ? -x[1] : x[1]);
            sb += a >> 1;
            if (i >= bt - r) sr += a >> 1;
        }
        uint32_t mb = sb / bt, mr = sr / r + 1;
        /* that, then held at the output to its level before (~0.4 s): the grains agree more or less as the sound
         * is held or moves (a held bass up to 3 dB), and the drive's knee takes a sustain harder than a beat */
        bus.g_agc = bus.g_agt = (int32_t)CLAMP((mb << 15) / mr, 8192u, 40000u);
        bus.g_tgt = bus.olev;
        bus.g_lev = bus.olev;
        for (int i = 0; i < 3; i++) bus.g_gp[i] = bus.g_start + gl_rnd() % (r - bus.g_gl + 1);
        bus.g_fi = 2;                                     /* in over 12 ms, out over 23 */
        bus.g_fo = 1;
        bus.g_pk = (uint8_t)(k + 1);
        return;
    }
    if (k == PF_OCTU || k == PF_OCTD) {                   /* the live sound through the grains: no slice */
        gl_start(bus.gw, 0, 0, k == PF_OCTU ? GM_PSU : GM_PSD, G_HELD, k == PF_OCTU ? 32768 : 29205, 0);   /* (down: its taps nearer, the bass 1 dB up) */
        gl_grains_init(k == PF_OCTU ? PS_UP : PS_DN);
        bus.g_fi = bus.g_fo = 4;
        bus.g_pk = (uint8_t)(k + 1);
        return;
    }
    if (k == PF_SPIN) {                                   /* the live sound from here, pulled back and spun backwards */
        uint32_t t1 = CLAMP(six >> 1, 256u, 1024u), t2 = six * 4;
        if (t1 + t2 > 12000u) t2 = 12000u - t1;           /* (what it reads back, about t1 + t2: within the ring) */
        gl_start(bus.gw - 2, t1 + t2 + 64, 0, GM_SPIN, G_HELD, 32768, 0);
        bus.g_srt = (int32_t)G_UNITY;
        bus.g_st = 0;
        bus.g_t1 = t1;
        bus.g_t2 = t2;
        bus.g_pk = (uint8_t)(k + 1);
        return;
    }
    if (k > PF_STUT && k != PF_SCAT && k != PF_ROLL) return;   /* (the rest of bank B: no ring) */
    int mk = k == PF_SCAT ? GM_SCAT : k == PF_ROLL ? GM_PROLL : -1;
    if (k == PF_SCAT) k = PF_REV;                         /* (the scatter's slices: the beat the reverse plays) */
    if (k == PF_ROLL) k = PF_R4;                          /* (the roll's: the quarter's, halved a beat at a time) */
    if (k == PF_STOP) {                                   /* the live sound from here, slowing over a beat */
        gl_start(bus.gw - 2, GL_MAX, 0, GM_STOP, G_HELD, 32768, 0);
        bus.g_dec = G_UNITY / (six * 4 > 64 ? six * 4 : 64);
        bus.g_fo = 2;                                     /* (its landing: 12 ms) */
        bus.g_dip = 32767;
        bus.g_pk = (uint8_t)(k + 1);
        return;
    }
    static const uint8_t EIGHTHS[7] = {32, 16, 8, 4, 32, 0, 4};   /* the slice in eighths of a sixteenth */
    static const uint8_t GRID[7] = {2, 1, 0, 0, 2, 0, 0};         /* locked to the last beat, eighth, sixteenth */
    uint32_t len = (six * EIGHTHS[k]) >> 3;
    int res = GRID[k];
    while (len > GL_MAX) { len >>= 1; if (res) res--; }   /* (slower than 85 BPM a quarter is too long: an eighth) */
    if (len < 32) len = 32;
    uint32_t space = six << res, e = bus.gw - bus.g_grid[res];
    if (!seq_running() || e >= space + (six >> 1)) e = 0, bus.g_grid[res] = bus.gw;   /* no grid: from now */
    /* a repeat's slice begins on the grid (its first time round the live sound coming in, so it starts on the
     * attack); the reverse plays the beat that ended there */
    uint32_t at = bus.g_grid[res] - (k == PF_REV ? len : 0);
    gl_start(at, len, e % len, mk >= 0 ? mk : k == PF_REV ? GM_REV : k == PF_STUT ? GM_GATE : GM_RPT, G_HELD, 32768, 0);
    bus.g_sc[0] = bus.g_sc[1] = (uint8_t)((e % len) / (len >> 2));   /* (a scatter starts where it is, a beat back) */
    bus.g_rl = 0;
    bus.g_pk = (uint8_t)((mk == GM_SCAT ? PF_SCAT : mk == GM_PROLL ? PF_ROLL : k) + 1);
}

static int32_t (*dub_io(int ti))[M2K_SUB] { return bus.dub_t == ti + 1 ? bus.dub_io : 0; }
static int dub_on(void) { return perf.on[PF_DUB] || bus.dub_snd > 0; }
static void dub_end(void) { bus.dub_t = 0; }

/* DUB borrows a track's delay line, one the pattern leaves free: its kit's send 0 (DSND 64), no lock on it, no LFO
 * on it, the line asleep. The track number + 1, 0 when none is */
static int dub_line(void)
{
    if (bus.dub_t) return bus.dub_t;                      /* (still ringing: the same line) */
    const m2k_pattern_t *p = PAT(cur_pat);
    for (int t = M2K_TRACKS - 1; t >= 0; t--) {
        int ok = p->kit[t][P_DSND] == 64 && trk[t].dsleep;
        for (int i = 0; ok && i < M2K_LOCKS; i++) ok = !(p->lock[i].param == P_DSND && p->lock[i].trk == t);
        for (int u = 0; ok && u < M2K_TRACKS; u++)
            for (int l = 0; l < 3; l++) {
                const uint8_t *b = &p->kit[u][P_LFO + 8 * l];
                if (b[L_DEST] == P_DSND + 1 && ((b[L_TRCK] * M2K_TRACKS) >> 7) == t) ok = 0;
            }
        if (ok) return t + 1;
    }
    return 0;
}

/* a key's effect on (down) or off; 0 when it cannot play (the key then holds nothing) */
static int bus_perform(int k, int down)
{
    if (k < 0 || k >= PF_COUNT) return 0;
    if (k == PF_DUB && down) {
        int t = dub_line();
        if (!t) return 0;
        if (!bus.dub_t) {                                 /* the line's from rest: at DUB's time at once */
            trk_t *d = &trk[t - 1];
            d->dt_q8 = (int32_t)(((uint32_t)DUB_TIM * (sps_q8 >> 5) + 128) & ~255u);
        }
        bus.dub_t = (uint8_t)t;
    }
    perf.on[k] = (uint8_t)down;
    if (!pf_ring(k)) return 1;
    if (!down) {                                          /* let go: its own effect ends (a stop spins up first) */
        if (bus.g_next == k + 1) bus.g_next = 0;
        if (bus.g_on && bus.g_pk == k + 1) {
            bus.g_pk = 0;
            /* a stop spins up: from where it is while it still moves, else (under a quarter of the speed) it dips out
             * and lands (gl_stop_land) */
            if (bus.g_mode == GM_STOP) bus.g_spin = bus.g_rate < G_UNITY / 4 ? 2 : 1;
            else bus.g_left = 0;
        }
        return 1;
    }
    if (bus.g_on) {                                       /* the one sounding fades out first, then this begins */
        bus.g_next = (uint8_t)(k + 1);
        bus.g_pk = 0;
        bus.g_spin = 0;
        bus.g_left = 0;
        return 1;
    }
    gl_perform(k);
    return 1;
}

/* the sequencer calls this on each of the pattern's steps: the grid's places, then maybe a repeat. GLCH's laws
 * (tools/gen_tables.py): low, a rare short repeat of a short slice under the dry, quietly; up the knob more often,
 * louder, longer, the slice up to SIZE's, the dry giving way and the modes coming in, to wild at the top */
static void bus_step(int s)
{
    bus.g_grid[0] = bus.gw;
    if (!(s & 1)) bus.g_grid[1] = bus.gw;
    if (!(s & 3)) bus.g_grid[2] = bus.gw;
    const uint8_t *g = PAT(cur_pat)->glob;
    int v = g[G_GLCH];
    if (!v || bus.g_on) return;
    if ((gl_rnd() & 0xFFFF) >= T_GLCH_P[v]) return;
    int n = (g[G_GSIZ] * 5) >> 7;                         /* 1/64 .. 1/4, at most the law's */
    if (n > T_GL_CAP[v]) n = T_GL_CAP[v];
    uint32_t six = sps_q8 >> 9, len = (six << n) >> 2;
    len = CLAMP(len, 32u, GL_MAX);
    uint32_t r = gl_rnd(), reps = T_GL_REP[v];            /* Q4 */
    if (v > 50) reps = (reps * (12 + (r & 7))) >> 4;      /* the upper half, a length of its own each */
    uint32_t dur = (len * reps) >> 4;
    dur = CLAMP(dur, len, GL_LEN - len - 4 * GL_XF);
    int mode = (int)((r >> 8) & 255) < T_GL_ART[v] ? 1 + (int)((r >> 16) % T_GL_NM[v]) : GM_RPT;
    gl_start(bus.gw - len, len, 0, mode, dur, T_GL_WET[v], T_GL_DRY[v]);
    if (mode == GM_OCTU) bus.g_rate = G_UNITY * 2;
    if (mode == GM_OCTD) bus.g_rate = G_UNITY / 2;
    if (mode == GM_TSTOP) bus.g_dec = G_UNITY / dur;
}

/* FREEZE and the octaves, a half-rate sample: three Hann grains a third of one apart (their windows' squares sum to
 * a constant, so the sum holds its level whether the grains agree or not). A freeze's grain reads its own place in
 * the slice at pitch, a new place each time round; up, a grain reads at twice the speed from a grain and a bit back
 * to just behind the write ([1 2 1] first, so the top folds back 6 dB down and more); down, at half from just behind
 * to half a grain back */
#define GR_GAIN 30894                  /* 1 / sqrt(9 / 8): the three windows' power */
static void gl_grains(int32_t *pl, int32_t *pr)
{
    const uint32_t M = GL_LEN - 1, G = bus.g_gl, w = bus.gw - 1;     /* (w: the newest sample written) */
    int mode = bus.g_mode;
    int32_t al = 0, ar = 0;
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t ph = bus.g_gph + i * bus.g_g3;
        if (ph >= G) ph -= G;
        int32_t s = sinp(ph * bus.g_wst), win = (s * s) >> 15;       /* (ph g_wst < 2^31; |s| < 2^15) */
        int32_t xl, xr;
        if (mode == GM_FRZ) {
            if (!ph) bus.g_gp[i] = bus.g_start + gl_rnd() % (bus.g_len - G + 1);
            uint32_t k = bus.g_gp[i] + ph;
            xl = gl_buf[k & M][0];
            xr = gl_buf[k & M][1];
        } else if (mode == GM_PSU) {
            uint32_t k = w - PS_DMIN - G + ph;
            xl = (gl_buf[(k - 1) & M][0] + 2 * gl_buf[k & M][0] + gl_buf[(k + 1) & M][0]) >> 2;
            xr = (gl_buf[(k - 1) & M][1] + 2 * gl_buf[k & M][1] + gl_buf[(k + 1) & M][1]) >> 2;
        } else {
            uint32_t k = w - PS_DMIN - ((ph + 1) >> 1);
            xl = gl_buf[k & M][0];
            xr = gl_buf[k & M][1];
            if (ph & 1) { xl = (xl + gl_buf[(k + 1) & M][0]) >> 1; xr = (xr + gl_buf[(k + 1) & M][1]) >> 1; }
        }
        al += (xl * win) >> 15;
        ar += (xr * win) >> 15;
    }
    bus.g_gph = bus.g_gph + 1 >= G ? 0 : bus.g_gph + 1;
    al = (al * GR_GAIN) >> 15;                            /* (|al| < 1.5 x 2^15) */
    ar = (ar * GR_GAIN) >> 15;
    if (mode == GM_FRZ) {                                 /* (its gain: bus_block's, by the output's level, glided here) */
        bus.g_agc += (bus.g_agt - bus.g_agc) >> 8;
        al = (al * bus.g_agc) >> 15;                      /* (|al| < 2^16, the gain < 2^15.3) */
        ar = (ar * bus.g_agc) >> 15;
        al = CLAMP(al, -32767, 32767);
        ar = CLAMP(ar, -32767, 32767);
    }
    *pl = al;
    *pr = ar;
}

/* a stop let go near rest, dipped out: its read moved (unheard) to where an eighth's spin-up from rest (which reads
 * half an eighth) ends a beat behind the write (half one, slower than 85 BPM), so it lands in time, on the beat as
 * the sound plays it, and fades into it over 12 ms */
static void gl_stop_land(void)
{
    uint32_t t = (sps_q8 >> 8) > 64 ? (sps_q8 >> 8) : 64, back = (sps_q8 >> 9) * 4;
    while (back > GL_MAX - t) back >>= 1;
    bus.g_start = bus.gw - (back - t / 2);
    bus.g_pos = 0;
    bus.g_rate = 0;
    bus.g_spin = 1;
}

/* SCATTER's slice d (0-3 of the beat, 4 reversed, 8 its first half twice), its sample o (past the slice: where it
 * goes on, for the crossfade into the next) */
static inline uint32_t sc_at(uint32_t d, uint32_t o, uint32_t sl)
{
    uint32_t pos = d & 4 ? sl - 1 - o : d & 8 ? (o < sl / 2 ? o : o - sl / 2) : o;
    return bus.g_start + (d & 3) * sl + pos;
}

/* one half-rate sample of the event: the wet's next (gout), its read moved on */
static void gl_tick(void)
{
    const uint32_t M = GL_LEN - 1;
    uint32_t p = bus.g_pos >> 12, len = bus.g_len, mode = bus.g_mode;
    int32_t fr = (int32_t)(bus.g_pos & 4095), l, r;
    if (mode == GM_SPIN) {                                /* SPIN: the speed and the read, then out */
        uint32_t st = bus.g_st++, t1 = bus.g_t1, t2 = bus.g_t2;
        if (st < t1) bus.g_srt = (int32_t)G_UNITY - (int32_t)(((st << 12) / t1) << 14);      /* pulled: 1 to -3 */
        else if (st < t1 + t2) {                          /* let spin: -3 to rest, slowing as (1 - u)^2 */
            int32_t v = 4096 - (int32_t)(((st - t1) << 12) / t2);
            bus.g_srt = -(((v * v) >> 12) * 12288);
        } else bus.g_srt = 0;
        uint32_t k = bus.g_start + (bus.g_pos >> 12);
        int32_t a = bus.g_srt < 0 ? -bus.g_srt : bus.g_srt;
        if (a > (int32_t)G_UNITY) {                       /* past its speed: [1 2 1] against the fold */
            l = (gl_buf[(k - 1) & M][0] + 2 * gl_buf[k & M][0] + gl_buf[(k + 1) & M][0]) >> 2;
            r = (gl_buf[(k - 1) & M][1] + 2 * gl_buf[k & M][1] + gl_buf[(k + 1) & M][1]) >> 2;
            a = (int32_t)G_UNITY;
        } else {
            l = gl_buf[k & M][0] + (((gl_buf[(k + 1) & M][0] - gl_buf[k & M][0]) * fr) >> 12);
            r = gl_buf[k & M][1] + (((gl_buf[(k + 1) & M][1] - gl_buf[k & M][1]) * fr) >> 12);
        }
        int32_t sp = a >> 9, g = (sp * (65536 - sp)) >> 15;          /* (as the stop's: s (2 - s)) */
        bus.gprev_l = bus.gout_l;
        bus.gprev_r = bus.gout_r;
        bus.gout_l = (l * g) >> 15;
        bus.gout_r = (r * g) >> 15;
        bus.g_pos += (uint32_t)(bus.g_srt >> 12);
        if (bus.g_left != G_HELD && bus.g_left) bus.g_left--;
        return;
    }
    if (mode == GM_SCAT) {                                /* SCATTER: a quarter of the beat at a time, from anywhere in it */
        uint32_t sl = len >> 2, o = p % sl, i = p / sl;
        if (!o) {
            uint32_t q = gl_rnd();
            uint8_t d = (q & 7) < 3 ? (uint8_t)i : (uint8_t)((q >> 3) & 3);   /* (3 in 8 where it was) */
            if (d != i && !((q >> 5) & 3)) d |= 4;        /* reversed */
            else if (d != i && !((q >> 7) & 7)) d |= 8;   /* its first half twice */
            bus.g_sc[1] = bus.g_sc[0];
            bus.g_sc[0] = d;
        }
        uint32_t k = sc_at(bus.g_sc[0], o, sl);
        l = gl_buf[k & M][0];
        r = gl_buf[k & M][1];
        if (o < GL_XF) {                                  /* the slice before, on past its end, crossfaded out */
            uint32_t j = sc_at(bus.g_sc[1], o + sl, sl);
            int32_t w = (int32_t)(o << 12) / (int32_t)GL_XF;
            l = gl_buf[j & M][0] + (((l - gl_buf[j & M][0]) * w) >> 12);
            r = gl_buf[j & M][1] + (((r - gl_buf[j & M][1]) * w) >> 12);
        }
        bus.gprev_l = bus.gout_l;
        bus.gprev_r = bus.gout_r;
        bus.gout_l = l;
        bus.gout_r = r;
        bus.g_pos += 1u << 12;
        if ((bus.g_pos >> 12) >= len) bus.g_pos -= len << 12;
        if (bus.g_left != G_HELD && bus.g_left) bus.g_left--;
        return;
    }
    if (mode == GM_FRZ || mode == GM_PSU || mode == GM_PSD) {
        gl_grains(&l, &r);
        bus.gprev_l = bus.gout_l;
        bus.gprev_r = bus.gout_r;
        bus.gout_l = l;
        bus.gout_r = r;
        if (bus.g_left != G_HELD && bus.g_left) bus.g_left--;
        return;
    }
    if (mode == GM_STOP) {
        uint32_t k = bus.g_start + p;
        l = gl_buf[k & M][0] + (((gl_buf[(k + 1) & M][0] - gl_buf[k & M][0]) * fr) >> 12);
        r = gl_buf[k & M][1] + (((gl_buf[(k + 1) & M][1] - gl_buf[k & M][1]) * fr) >> 12);
        /* a tape's head gives less as it slows: s (2 - s), nearly its square root, so the fall is heard to the end */
        int32_t sp = (int32_t)(bus.g_rate >> 9), a = (sp * (65536 - sp)) >> 15;        /* Q15 */
        if (bus.g_spin == 2) {                            /* let go near rest: out over 1.5 ms, then the landing */
            bus.g_dip = bus.g_dip > 1024 ? bus.g_dip - 1024 : 0;
            a = (a * bus.g_dip) >> 15;
            if (!bus.g_dip) { gl_stop_land(); bus.g_dip = 32767; }
        }
        l = (l * a) >> 15;
        r = (r * a) >> 15;
        if (bus.g_spin == 1) {                              /* let go: back up to speed over an eighth, then out */
            uint32_t up = G_UNITY / ((sps_q8 >> 8) > 64 ? (sps_q8 >> 8) : 64);
            bus.g_rate = bus.g_rate + up >= G_UNITY ? G_UNITY : bus.g_rate + up;
            if (bus.g_rate == G_UNITY) { bus.g_spin = 3; bus.g_left = 0; }   /* (3: landed, at speed) */
        } else if (!bus.g_spin) bus.g_rate = bus.g_rate > bus.g_dec ? bus.g_rate - bus.g_dec : 0;   /* slowing in a straight line */
    } else {
        int rev = mode == GM_REV;
        uint32_t k = rev ? bus.g_start + len - 1 - p : bus.g_start + p, k1 = rev ? k - 1 : k + 1;
        l = gl_buf[k & M][0];
        r = gl_buf[k & M][1];
        if (fr) {                                         /* (only off pitch: a repeat reads whole samples) */
            l += ((gl_buf[k1 & M][0] - l) * fr) >> 12;
            r += ((gl_buf[k1 & M][1] - r) * fr) >> 12;
        }
        uint32_t xf = len >> 2 < GL_XF ? len >> 2 : GL_XF;
        if (p + xf >= len) {                              /* the seam: into what leads to the slice's start */
            uint32_t j = rev ? bus.g_start + 2 * len - 1 - p : bus.g_start + p - len;
            int32_t w = (int32_t)(((p + xf - len) << 12) + (uint32_t)fr) / (int32_t)xf;   /* Q12 */
            l += ((gl_buf[j & M][0] - l) * w) >> 12;
            r += ((gl_buf[j & M][1] - r) * w) >> 12;
        }
        if (mode == GM_GATE) {                            /* open the first half of each time round, edges faded */
            uint32_t on = len >> 1, e = len >> 3 < GL_XF / 2 ? len >> 3 : GL_XF / 2;
            int32_t gt = p < e ? (int32_t)((p << 12) / e) : p < on - e ? 4096 : p < on ? (int32_t)(((on - p) << 12) / e) : 0;
            l = (l * gt) >> 12;
            r = (r * gt) >> 12;
        } else if (mode == GM_TSTOP) {
            int32_t sp = (int32_t)(bus.g_rate >> 9), a = (sp * (65536 - sp)) >> 15;
            l = (l * a) >> 15;
            r = (r * a) >> 15;
            bus.g_rate = bus.g_rate > bus.g_dec ? bus.g_rate - bus.g_dec : 0;
        }
    }
    bus.gprev_l = bus.gout_l;
    bus.gprev_r = bus.gout_r;
    bus.gout_l = l;
    bus.gout_r = r;
    bus.g_pos += bus.g_rate >> 12;
    if (mode != GM_STOP && (bus.g_pos >> 12) >= len) {
        bus.g_pos -= len << 12;
        if (mode == GM_ROLL && len >= 128) bus.g_len = len >> 1;
        /* ROLL: halved once its laps make a beat (a quarter, two eighths, four sixteenths), to a 1/32 */
        if (mode == GM_PROLL && ++bus.g_rl >= (sps_q8 >> 9) * 4 / len && len > (sps_q8 >> 10)) { bus.g_len = len >> 1; bus.g_rl = 0; }
    }
    if (bus.g_left != G_HELD && bus.g_left) bus.g_left--;
}

/* the ring's place w is one the event still reads: its slice and seams a lap on (its first time round they are
 * the live sound coming in), or what a stop will read spinning up */
static inline int gl_kept(uint32_t w)
{
    if (bus.g_mode == GM_PSU || bus.g_mode == GM_PSD) return 0;   /* (a grain and a bit behind the write) */
    /* a stop reads behind the write by at most half its beat while it slows, nothing at rest, and lands from fresh
     * sound (gl_stop_land): it keeps nothing (it kept what was ahead of its read from the press, so it slowed down
     * the sound of a ring's lap before, 0.74 s) */
    if (bus.g_mode == GM_STOP) return 0;
    if (bus.g_mode == GM_SPIN) {                          /* what it spins back over, behind where it began */
        uint32_t d = w - (bus.g_start - bus.g_len - GL_XF);
        return d >= GL_LEN && (d & (GL_LEN - 1)) < bus.g_len + 2 * GL_XF;
    }
    uint32_t d = w - (bus.g_start - GL_XF);
    return d >= GL_LEN && (d & (GL_LEN - 1)) < bus.g_len + 2 * GL_XF;
}

static uint32_t isqrt32(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x) b >>= 2;
    while (b) { if (x >= r + b) { x -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return r;
}

/* 2^(-e / 65536), Q16: T_EXP2F's 1/256 octaves read between (a long tail's gain a line a pass wants the finer steps) */
static uint32_t exp2neg(uint32_t e)
{
    uint32_t n = e >> 16, g = 65536u - (e & 0xFFFFu);           /* 2^-e = 2^(g / 65536) / 2^(n + 1) */
    if (n > 15) return 0;
    if (g == 65536u) return 65536u >> n;
    uint32_t i = g >> 8, r = g & 255u, a = T_EXP2F[i], b = i < 255 ? T_EXP2F[i + 1] : 131072u;
    return (a + (((b - a) * r) >> 8)) >> (n + 1);
}

/* the reverb's coefficients for DCAY v, DAMP d, the room as it stands, at the lines' mean length (the matrix shares
 * every pass out among them): their gain g for the RT60, and their damping, a one-pole whose loss at the top gives the
 * top DAMP's share a of the bottom's RT60 (there the loop wants g^(1/a): the pole's r = g^(1/a - 1) = (1 - b) /
 * (1 + b)) */
#define RV_MEAN 855                    /* (683 + 797 + 907 + 1031) / 4 */
static void rv_coef(int v, int d)
{
    int32_t len = (int32_t)((RV_MEAN * bus.rsz) >> 4);                                /* Q8 samples */
    uint32_t e = (uint32_t)mul_sh((int32_t)T_RV_K[v], len, 16);                       /* Q16 octaves, < 2^16 */
    bus.rgh = (int32_t)(exp2neg(e) >> 2);
    uint32_t r = exp2neg((e * T_RV_DA[d]) >> 8);                                      /* 1/a - 1 Q8: < 2^28 */
    uint32_t bq = ((65536u - r) << 15) / (65536u + r);
    bus.ra = 32767 - (int32_t)(bq > 31000u ? 31000u : bq);
    bus.rv_v = (uint8_t)v;
    bus.rv_d = (uint8_t)d;
    bus.rv_s = bus.rsz;
}

static void rv_clear(void)
{
    m_set(rv_buf, 0, sizeof rv_buf);
    m_set(bus.rlp, 0, sizeof bus.rlp);
    m_set(bus.rlr, 0, sizeof bus.rlr);
    m_set(bus.rq, 0, sizeof bus.rq);
    bus.rdq[0] = bus.rdq[1] = bus.rrq[0] = bus.rrq[1] = 0;
    bus.rbw = bus.rbr = bus.rv_l = bus.rv_r = 0;
    bus.rquiet = 0;
}

/* a block of the reverb in three passes (each with few enough values live to stay in registers): the input through
 * its top and the diffusers; the lines; out into the mix. Specialised for a room settled (each line's read whole
 * samples behind its write: a read between two dulls the top every pass) or gliding (between them) */
/* SHIMMER (PERFORM): the bus thrown into the reverb at DCAY 92 (7 s) and DAMP 24 (its top a little sooner), the
 * longest line read back an octave up (three Hann grains of 44 ms at twice the speed, its last 1033 samples) into
 * every line's input: each pass round an octave more, as it dies */
#define SH_G 960u
#define SH_V 92
#define SH_D 24
#define SH_FB 3277                     /* the octave's share back in, Q15 (0.1: 0.35 ran away, the loop over unity) */
static inline __attribute__((always_inline)) void rv_run(const int32_t *rsend, int32_t *L, int32_t *R, int v,
                                                         int damp, int32_t fade, int32_t rs_, int32_t rc_, int glide,
                                                         int32_t shim)
{
    int32_t xin[M2K_SUB / 2], ol[M2K_SUB / 2], orr[M2K_SUB / 2];
    int16_t *b = rv_buf;
    const uint32_t M = RV_N - 1;
    uint32_t w0 = bus.rw;
    {
        int32_t gi = T_RV_GI[v], bw = T_RV_BW[damp], rbw = bus.rbw, rbr = bus.rbr, dq0 = bus.rdq[0], dq1 = bus.rdq[1];
        uint32_t w = w0;
        for (int k = 0; k < M2K_SUB / 2; k++, w++) {
            int32_t x = (((rsend[2 * k] + rsend[2 * k + 1]) >> 3) * gi) >> 15;   /* (six tracks' sends at most: < 2^16) */
            if (bw < 32767) {                             /* the input's top, its remainder carried */
                int32_t q = (x - rbw) * bw + rbr;
                rbw += q >> 15;
                rbr = q & 32767;
                x = rbw;
            }
            /* two allpasses smear the attack */
            int32_t d = b[(RV_AT[4] - RD_LEN[0] + w) & M], p = d * 20480 + dq0, u = sat16(x + (p >> 15));
            dq0 = p & 32767;
            b[(RV_AT[4] + w) & M] = (int16_t)u;
            x = d - ((u * 20480 + 16384) >> 15);
            d = b[(RV_AT[5] - RD_LEN[1] + w) & M]; p = d * 20480 + dq1; u = sat16(x + (p >> 15));
            dq1 = p & 32767;
            b[(RV_AT[5] + w) & M] = (int16_t)u;
            xin[k] = d - ((u * 20480 + 16384) >> 15);
        }
        bus.rbw = rbw; bus.rbr = rbr; bus.rdq[0] = dq0; bus.rdq[1] = dq1;
    }
    {
        uint32_t rd[4];
        int32_t fr[4];
        for (int c = 0; c < 4; c++) {
            int32_t d = (int32_t)((RV_LEN[c] * bus.rsz) >> 4);                   /* Q8 */
            if (!glide) d = (d + 128) & ~255;
            rd[c] = RV_AT[c] - (uint32_t)(d >> 8);
            fr[c] = d & 255;
        }
        int32_t a = bus.ra, h = bus.rgh;
        int32_t lp0 = bus.rlp[0], lp1 = bus.rlp[1], lp2 = bus.rlp[2], lp3 = bus.rlp[3];
        int32_t lr0 = bus.rlr[0], lr1 = bus.rlr[1], lr2 = bus.rlr[2], lr3 = bus.rlr[3];
        int32_t q0 = bus.rq[0], q1 = bus.rq[1], q2 = bus.rq[2], q3 = bus.rq[3], rr0 = bus.rrq[0], rr1 = bus.rrq[1];
        uint32_t sph = bus.sh_ph;
        int32_t shp = bus.sh_hp;
        uint32_t w = w0;
#define RV_RD(c) (glide ? b[(rd[c] + w) & M] + (((b[(rd[c] + w - 1) & M] - b[(rd[c] + w) & M]) * fr[c] + 128) >> 8) \
                        : b[(rd[c] + w) & M])
#define RV_LP(s, lp, lr) do { int32_t q_ = ((s) - lp) * a + lr; lp += q_ >> 15; lr = q_ & 32767; } while (0)
        for (int k = 0; k < M2K_SUB / 2; k++, w++) {
            int32_t s0 = RV_RD(0), s1 = RV_RD(1), s2 = RV_RD(2), s3 = RV_RD(3);
            RV_LP(s0, lp0, lr0);                          /* (|s - lp| < 2^16) */
            RV_LP(s1, lp1, lr1);
            RV_LP(s2, lp2, lr2);
            RV_LP(s3, lp3, lr3);
            /* (the angle under 0.2 rad: each sum under 2^31) */
            int32_t p1 = rc_ * lp1 - rs_ * lp3 + rr0, p3 = rs_ * lp1 + rc_ * lp3 + rr1;
            rr0 = p1 & 32767;
            rr1 = p3 & 32767;
            int32_t y1 = p1 >> 15, y3 = p3 >> 15;
            int32_t s01 = lp0 + y1, d01 = lp0 - y1, s23 = lp2 + y3, d23 = lp2 - y3;
            /* the matrix's rows (< 2^17) by the gain / 2 (under 2^14), each with its remainder */
            int32_t f0 = (s01 + s23) * h + q0, f1 = (d01 + d23) * h + q1, f2 = (s01 - s23) * h + q2, f3 = (d01 - d23) * h + q3;
            q0 = f0 & 32767; q1 = f1 & 32767; q2 = f2 & 32767; q3 = f3 & 32767;
            int32_t x = xin[k];
            if (shim) {                                   /* SHIMMER: the longest line an octave up, back in */
                int32_t s = 0;
                for (uint32_t t = 0; t < 3; t++) {
                    uint32_t ph = sph + t * (SH_G / 3), j = RV_AT[3] + w - 1 - 3 - SH_G + ph;
                    if (ph >= SH_G) ph -= SH_G, j -= SH_G;
                    int32_t sn = sinp(ph * (0x80000000u / SH_G)), wn = (sn * sn) >> 15;
                    s += (((b[(j - 1) & M] + 2 * b[j & M] + b[(j + 1) & M]) >> 2) * wn) >> 15;   /* ([1 2 1]: no fold) */
                }
                sph = sph + 1 >= SH_G ? 0 : sph + 1;
                /* a high-pass at 240 Hz first: an octave of the bass's, the taps agreeing, would feed the lines back
                 * more than they lose a pass (DC's stays DC) and grow without end */
                shp += ((s - shp) * 2190) >> 15;
                x += ((s - shp) * shim) >> 15;            /* (|s| < 1.5 x 2^15) */
            }
            b[(RV_AT[0] + w) & M] = (int16_t)sat16(x + (f0 >> 15));
            b[(RV_AT[1] + w) & M] = (int16_t)sat16((f1 >> 15) - x);
            b[(RV_AT[2] + w) & M] = (int16_t)sat16(x + (f2 >> 15));
            b[(RV_AT[3] + w) & M] = (int16_t)sat16((f3 >> 15) - x);
            /* out before the damping: each pass darker than the last, the first as it came in */
            ol[k] = s0 + s2;
            orr[k] = -s1 - s3;
        }
#undef RV_RD
#undef RV_LP
        bus.rw = w;
        bus.rlp[0] = lp0; bus.rlp[1] = lp1; bus.rlp[2] = lp2; bus.rlp[3] = lp3;
        bus.rlr[0] = lr0; bus.rlr[1] = lr1; bus.rlr[2] = lr2; bus.rlr[3] = lr3;
        bus.rq[0] = q0; bus.rq[1] = q1; bus.rq[2] = q2; bus.rq[3] = q3;
        bus.rrq[0] = rr0; bus.rrq[1] = rr1;
        bus.sh_ph = sph;
        bus.sh_hp = shp;
    }
    {
        int32_t go = bus.rgo, pl = bus.rv_l, pr = bus.rv_r;     /* (T_RV_GO[v], glided) */
        if (fade < 32767) go = (go * fade) >> 15;         /* (go < 2^15) */
        for (int k = 0; k < M2K_SUB / 2; k++) {
            int32_t l = (ol[k] * go) >> 12, r = (orr[k] * go) >> 12;           /* (|ol| < 2^16) */
            L[2 * k] += (pl + l) >> 1; R[2 * k] += (pr + r) >> 1;            /* the half rate's in-between */
            L[2 * k + 1] += l; R[2 * k + 1] += r;
            pl = l;
            pr = r;
        }
        bus.rv_l = pl; bus.rv_r = pr;
    }
}

/* the reverb at half rate: four lines through a Hadamard matrix (each feeds all: dense soon, smooth long), damped,
 * two of them turned into each other by a swaying angle. Every recursive product carries its remainder (a tail
 * decays at its rate to the last step, not faster, and holds no DC); once the input and the lines stay within a step
 * for a whole pass it empties and sleeps. Turned to 0 its tail fades in 6 ms */
static void reverb_block(int verb, int damp, const int32_t *rsend, int32_t *L, int32_t *R, int32_t shim)
{
    int v = verb ? verb : bus.rv_v;
    if (!v) return;
    int in = 0;
    for (int i = 0; i < M2K_SUB; i++) in |= rsend[i];
    if (bus.rsleep && !in && verb == bus.rv_v) return;
    bus.rsleep = 0;
    uint32_t tgt = (uint32_t)T_RV_SZ[v] << 4;            /* the room, Q12: it glides a 4096th a block (0.3 .. 1 in 1 s) */
    int from_off = !bus.rv_v;
    if (from_off) bus.rsz = tgt;
    else if (verb && bus.rsz != tgt) bus.rsz += bus.rsz < tgt ? 1 : (uint32_t)-1;
    if (v != bus.rv_v || damp != bus.rv_d || bus.rsz != bus.rv_s) rv_coef(v, damp);
    /* its level out glides an eighth of the way a block to DCAY's (SHIMMER's DCAY and back: no jump in a tail) */
    {
        int32_t got = T_RV_GO[v], d = got - bus.rgo;
        bus.rgo = from_off || (d > -16 && d < 16) ? got : bus.rgo + d / 8;
    }
    int32_t fade = verb ? 32767 : bus.rfade;
    if (!verb) {
        bus.rfade -= 2048;
        if (bus.rfade <= 0) { rv_clear(); bus.rv_v = 0; return; }   /* the lines start empty next time */
    } else bus.rfade = 32767;
    /* the swing: lines 2 and 4 turned into each other by a small angle swaying at 0.7 Hz (energy kept, no line's
     * gain moves: a long tail's modes smeared instead of ringing) */
    bus.rmph += 1090862u;
    uint32_t ang = (uint32_t)(sinp(bus.rmph) * (int32_t)T_RV_MD[v]);
    if (shim) rv_run(rsend, L, R, v, damp, fade, sinp(ang), sinp(ang + 0x40000000u), 1, shim);   /* (gliding: whole reads too) */
    else if (bus.rsz == tgt) rv_run(rsend, L, R, v, damp, fade, sinp(ang), sinp(ang + 0x40000000u), 0, 0);
    else rv_run(rsend, L, R, v, damp, fade, sinp(ang), sinp(ang + 0x40000000u), 1, 0);
    int32_t big = 0;
    for (int c = 0; c < 4; c++) big |= bus.rlp[c] > 1 || bus.rlp[c] < -1;
    /* quiet: nothing in, the lines within a step, a whole pass of the longest */
    bus.rquiet = in || big ? 0 : bus.rquiet + M2K_SUB / 2;   /* (the lines looked at once a block) */
    if (bus.rquiet > RV_LEN[3] + 2u && verb) { rv_clear(); bus.rsleep = 1; }
}

/* LPF and HPF, a block: held, the cutoff moves from open to its far end over a bar (an octave in the same time
 * all the way), the resonance rising with it (Q 0.71 to 2.2); let go, back open in 150 ms; mixed in and out over
 * 5 ms at the open end (the two-poles start from rest unheard). Their bits in the return: the ones to run */
#define FL_LP_OPEN (F_TOP << 8)        /* 18 kHz .. 200 Hz */
#define FL_LP_FAR (443 << 8)
#define FL_HP_OPEN (124 << 8)          /* 20 Hz .. 2 kHz */
#define FL_HP_FAR (762 << 8)
#define FL_K_OPEN 23726566             /* 1 / Q, Q24: 1.414 .. 0.45 */
#define FL_K_FAR 7549747
static int bus_filters(void)
{
    int run = 0;
    for (int f = 0; f < 2; f++) {
        int held = perf.on[f ? PF_HPF : PF_LPF];
        if (!held && !bus.fmix[f]) continue;
        int32_t open = f ? FL_HP_OPEN : FL_LP_OPEN, far = f ? FL_HP_FAR : FL_LP_FAR, span = far > open ? far - open : open - far;
        if (held && !bus.fmix[f]) {                       /* from rest (the low-pass at its first sample's level) */
            m_set(bus.fl[f], 0, sizeof bus.fl[f]);
            bus.fpos[f] = open;
            if (!f) bus.fnew = 1;
        }
        int32_t d = bus.fpos[f] > open ? bus.fpos[f] - open : open - bus.fpos[f];
        if (held) {
            int32_t bar = (int32_t)((sps_q8 >> 8) * 16u / M2K_SUB), st = span / (bar > 0 ? bar : 1) + 1;   /* blocks a bar */
            d = d + st > span ? span : d + st;
        } else {
            int32_t st = span / 413 + 1;                  /* (150 ms of blocks) */
            d = d > st ? d - st : 0;
        }
        bus.fpos[f] = far > open ? open + d : open - d;
        if (held || d) bus.fmix[f] = bus.fmix[f] + 2400 > 32767 ? 32767 : bus.fmix[f] + 2400;
        else bus.fmix[f] = bus.fmix[f] > 2400 ? bus.fmix[f] - 2400 : 0;
        int32_t u = (d << 7) / (span >> 8);              /* the way there, Q15 (d << 7 < 2^25) */
        int32_t k = FL_K_OPEN - (int32_t)mul_sh(FL_K_OPEN - FL_K_FAR, u, 15);
        bus.fgain[f] = f ? 32767 : 32767 - ((u * 9830) >> 15);   /* the low-pass's peak paid back: -3 dB at its end */
        zsvf_set(&bus.fl[f][0], fg_at(T_FG, bus.fpos[f]), (uint32_t)k);
        bus.fl[f][1].a1 = bus.fl[f][0].a1;
        bus.fl[f][1].a2 = bus.fl[f][0].a2;
        bus.fl[f][1].a3 = bus.fl[f][0].a3;
        bus.fl[f][1].k = bus.fl[f][0].k;
        if (bus.fmix[f]) run |= 1 << f;
    }
    return run;
}

static void bus_block(int32_t *L, int32_t *R, const int32_t *rsend, int16_t *out)
{
    const uint8_t *g = PAT(cur_pat)->glob;
    /* DUB: its echoes into the mix; the mix, the send gliding in and out over 12 ms, to its line next block */
    if (bus.dub_t) {
        int32_t s0 = bus.dub_snd;
        bus.dub_snd = perf.on[PF_DUB] ? (s0 + 1024 > 32767 ? 32767 : s0 + 1024) : (s0 > 1024 ? s0 - 1024 : 0);
        for (int i = 0; i < M2K_SUB; i++) {               /* ((L + R) / 4 under 2^16, x s / 2) */
            int32_t s = s0 + (((bus.dub_snd - s0) * (i + 1)) >> 4);
            bus.dub_io[0][i] = (((L[i] + R[i]) >> 2) * (s >> 1)) >> 13;
            L[i] += bus.dub_io[1][i];
            R[i] += bus.dub_io[2][i];
            bus.dub_io[1][i] = bus.dub_io[2][i] = 0;
        }
    }
    /* SHIMMER: held, the bus into the reverb (gliding in and out over 12 ms) at its decay and colour; let go, the
     * wash rings on 4 s, the octaves still climbing, then the pattern's reverb again (its lines are the pattern's too:
     * no level tells the wash from them) */
    int verb = g[G_VERB], damp = g[G_DAMP];
    int32_t shim = 0, rs2[M2K_SUB];
    const int32_t *rs = rsend;
    if (perf.on[PF_SHIM]) bus.sh_on = 1;
    if (bus.sh_on) {
        int32_t s0 = bus.sh_snd;
        bus.sh_snd = perf.on[PF_SHIM] ? (s0 + 1024 > 32767 ? 32767 : s0 + 1024) : (s0 > 1024 ? s0 - 1024 : 0);
        for (int i = 0; i < M2K_SUB; i++) {               /* (a quarter of the mix: (L + R) / 4 under 2^16, x s / 2) */
            int32_t s = s0 + (((bus.sh_snd - s0) * (i + 1)) >> 4);
            rs2[i] = rsend[i] + ((((L[i] + R[i]) >> 2) * (s >> 1)) >> 15);
        }
        rs = rs2;
        if (verb < SH_V) verb = SH_V;
        damp = SH_D;
        shim = SH_FB;
    }
    reverb_block(verb, damp, rs, L, R, shim);
    if (bus.sh_on && perf.on[PF_SHIM]) bus.sh_q = 0;
    else if (bus.sh_on && !bus.sh_snd && ++bus.sh_q > 4u * M2K_SR / M2K_SUB) bus.sh_on = 0, bus.sh_q = 0;   /* (4 s on) */
    /* the controls on their laws (tools/gen_tables.py, the bus's controls). The tape's depth glides to its control's
     * or WOW's (no jump in the line's read); TAPE up, the tape is all there is, as it was; WOW from TAPE 0 fills the
     * line, then fades the tape in, and out again once its depth is back down */
    int32_t tq_t = T_TAPE_Q8[perf.on[PF_WOW] ? 127 : g[G_TAPE]];
    int tape_on = g[G_TAPE] || perf.on[PF_WOW] || bus.tp_mix;
    int32_t tq0 = bus.tq, tqd = 0;                  /* (gliding, the depth moves sample by sample over the block) */
    if (!tape_on) bus.tq = 0;
    else if (!bus.tq && g[G_TAPE] && !perf.on[PF_WOW]) bus.tq = tq_t;          /* TAPE turned up: at once */
    else { int32_t d = tq_t - bus.tq; bus.tq = d > -256 && d < 256 ? tq_t : bus.tq + (d >> 8); tqd = bus.tq - tq0; }
    if (g[G_TAPE]) bus.tp_mix = 32767;
    else if (tape_on) {
        if (!bus.tp_fill && !bus.tp_mix) bus.tlp_l = bus.tlp_r = 0;   /* (from off: the head loss from rest) */
        bus.tp_fill += M2K_SUB;
        int32_t want = perf.on[PF_WOW] || bus.tq > 64 ? 32767 : 0;
        if (bus.tp_fill > 1024 && want != bus.tp_mix)     /* (the line read 420 +- 285 samples back: filled) */
            bus.tp_mix = want > bus.tp_mix ? (bus.tp_mix + 1024 > 32767 ? 32767 : bus.tp_mix + 1024)
                                           : (bus.tp_mix > 1024 ? bus.tp_mix - 1024 : 0);
        if (!bus.tp_mix && !perf.on[PF_WOW]) bus.tp_fill = 0;
    } else bus.tp_fill = 0;
    int32_t tq = bus.tq, tpm = bus.tp_mix;
    /* the crush: its control's at once; CRUSH glides in and out over 10 ms, the coder (idle under ADPC 0) first taking
     * up the sound it meets for 2 ms, unheard, so it comes in tracking it */
    int adpc = perf.on[PF_CRSH] ? 127 : g[G_ADPC];
    int32_t ad_t = (adpc * 2 > 127 ? 127 : adpc * 2) << 8;
    if (perf.on[PF_CRSH] && !bus.ad_perf) {
        bus.ad_perf = 1;
        if (!bus.ad_mixq) {
            bus.ad_pred[0] = sat16(L[0]); bus.ad_pred[1] = sat16(R[0]); bus.ad_idx[0] = bus.ad_idx[1] = 40;
            bus.ad_warm = 6;
        }
    }
    if (!bus.ad_perf) bus.ad_mixq = ad_t;
    else if (bus.ad_warm) bus.ad_warm--;
    else {
        int32_t d = ad_t - bus.ad_mixq;
        bus.ad_mixq += d > 1200 ? 1200 : d < -1200 ? -1200 : d;
        if (!perf.on[PF_CRSH] && bus.ad_mixq == ad_t) bus.ad_perf = 0;
    }
    int32_t ad_mix = bus.ad_mixq >> 8, ad_div = T_ADPC_DIV[adpc], ad_run = ad_mix || bus.ad_warm;
    /* the drive: its control's; DRIVE glides up and down over 20 ms, its output held to the level the control's would
     * give: while it rises, by the gain's own ratio (a small sound's level, a loud one's a little under), then by the
     * two outputs measured (one-poles on both, ~23 ms), rising to it, and so back down; never louder, never at full
     * scale */
    int32_t drv0 = T_DRIV[g[G_DRIV]], drv_t = perf.on[PF_DRIV] ? T_DRIV[127] : drv0;
    if (!bus.dv) bus.dv = drv0, bus.dv_g = bus.dv_gp = 32767;   /* (from init: the control's, nothing held) */
    int32_t dvp = bus.dv;                                  /* (the gain and the hold ramp across the block: stepped */
    { int32_t d = drv_t - bus.dv; bus.dv += d > 24 ? 24 : d < -24 ? -24 : d; }   /*  a block at a time: a +24 dB click) */
    int32_t drv = bus.dv, pdrv = drv != drv0 || dvp != drv0 || bus.dv_g < 32767 || bus.dv_gp < 32767;
    if (!pdrv) bus.dv_e0 = bus.dv_e1 = 0;                 /* (both from 0: their ratio right from the start) */
    int32_t dvg = bus.dv_g, dgp = bus.dv_gp;
    bus.dv_gp = dvg;
    int fl_run = bus_filters();
    /* GATE: its depth in and out over 6 ms; its place in the sixteenth the sequencer's (a free one without it) */
    int32_t gtd0 = bus.gtd;                                /* (ramped across the block) */
    if (perf.on[PF_GATE]) bus.gtd = bus.gtd + 2048 > 32767 ? 32767 : bus.gtd + 2048;
    else bus.gtd = bus.gtd > 2048 ? bus.gtd - 2048 : 0;
    int gate = bus.gtd > 0 || gtd0 > 0;
    uint32_t gsix = (sps_q8 >> 8) > 512 ? (sps_q8 >> 8) : 512, gph = 0;
    if (gate) {
        gph = seq_running() ? (bus.gw - bus.g_grid[0]) * 2u + bus.g_phase : bus.gt_ph;
        gph %= gsix;
        bus.gt_ph = gph + M2K_SUB;
    }
    int32_t mvol = T_MVOL[master_vol];
    uint32_t wow_inc = 54000, flut_inc = 620000;    /* 0.55 Hz, 6.3 Hz */
    uint8_t g_phase = bus.g_phase;                  /* (a block's in locals: no byte stored a sample) */
    int32_t hgain = bus.hgain, htgt = tape_rolling ? 32767 : 0, hiss = g[G_HISS];
    uint32_t e0 = 0, e1 = 0;
    int32_t olv = 0;
    for (int i = 0; i < M2K_SUB; i++) {
        int32_t l = L[i], r = R[i];
        /* glitch: the ring written at half rate (never under the event's read); the event mixed in over the dry */
        if (!(g_phase ^= 1)) {
            if (!bus.g_on || !gl_kept(bus.gw)) {
                gl_buf[bus.gw & (GL_LEN - 1)][0] = (int16_t)sat16((bus.gacc_l + l) >> 1);
                gl_buf[bus.gw & (GL_LEN - 1)][1] = (int16_t)sat16((bus.gacc_r + r) >> 1);
            }
            bus.gw++;
            if (bus.g_on) gl_tick();
        } else {
            bus.gacc_l = l;
            bus.gacc_r = r;
        }
        if (bus.g_on) {
            if (bus.g_left) { if (bus.g_env < 1024) bus.g_env = bus.g_env + bus.g_fi > 1024 ? 1024 : bus.g_env + bus.g_fi; }
            else if ((bus.g_env -= bus.g_fo) <= 0) {      /* faded out: over, or the next key's begins */
                bus.g_env = 0;
                bus.g_on = 0;
                if (bus.g_next) { int k = bus.g_next - 1; bus.g_next = 0; gl_perform(k); }
            }
            /* the wet between its half-rate samples; l + env (wet W - l (1 - D)) (|l| < 2^18, the products < 2^31) */
            int32_t wl = g_phase ? bus.gout_l : (bus.gprev_l + bus.gout_l) >> 1, wr = g_phase ? bus.gout_r : (bus.gprev_r + bus.gout_r) >> 1;
            int32_t dm = (32768 - bus.g_dry) >> 3, env = bus.g_env;          /* (both whole: exactly the wet) */
            l += ((((wl * bus.g_wet) >> 3) - l * dm) >> 12) * env >> 10;
            r += ((((wr * bus.g_wet) >> 3) - r * dm) >> 12) * env >> 10;
        }
        /* tape: wow and flutter on a short line, head loss, a little compression */
        if (tape_on) {
            tp_buf[bus.tw & 2047][0] = (int16_t)sat16(l);
            tp_buf[bus.tw & 2047][1] = (int16_t)sat16(r);
            bus.wow += wow_inc;
            bus.flut += flut_inc;
            /* the pitch a swing of the line makes grows with its rate: the flutter (6.3 Hz, 11x the wow's) takes 1/48
             * of the wow's swing, so it wobbles about a quarter as far in pitch (at TAPE 127: wow ~+-40 c, flutter ~+-10 c) */
            int32_t m = (sinp(bus.wow) * 48 + sinp(bus.flut)) >> 6;           /* +-24600 */
            if (tqd) tq = tq0 + ((tqd * (i + 1)) >> 4);
            int32_t d = (420 << 8) + (int32_t)mul_sh(m, tq * 3, 15);         /* Q8 samples */
            uint32_t pos = (bus.tw << 8) - (uint32_t)d;                       /* unsigned: it wraps after 3 minutes */
            uint32_t j = pos >> 8;
            int32_t f = (int32_t)(pos & 255);
            int32_t tl = (tp_buf[j & 2047][0] * (256 - f) + tp_buf[(j + 1) & 2047][0] * f) >> 8;
            int32_t tr = (tp_buf[j & 2047][1] * (256 - f) + tp_buf[(j + 1) & 2047][1] * f) >> 8;
            bus.tw++;
            int32_t li = 127 * 1024 - tq, lj = li >> 10;                     /* the head loss: 127 - old / 4, Q10 */
            int32_t a = T_OP[lj] + (((T_OP[lj < 127 ? lj + 1 : 127] - T_OP[lj]) * (li & 1023)) >> 10);
            bus.tlp_l += ((tl - bus.tlp_l) * a) >> 15;
            bus.tlp_r += ((tr - bus.tlp_r) * a) >> 15;
            if (tpm >= 32767) { l = bus.tlp_l; r = bus.tlp_r; }
            else { l += (int32_t)mul_sh(bus.tlp_l - l, tpm, 15); r += (int32_t)mul_sh(bus.tlp_r - r, tpm, 15); }
        }
        /* LPF and HPF: the two-poles at 16 x the level (their remainders the finer), mixed in by their shares */
        for (int f = 0; fl_run >> f; f++) {
            if (!((fl_run >> f) & 1)) continue;
            int32_t yl, yr;
            if (!f && bus.fnew) { bus.fnew = 0; bus.fl[0][0].s2 = l * 16; bus.fl[0][1].s2 = r * 16; }
            zsvf_tick(&bus.fl[f][0], l * 16, f ? 0 : &yl, 0, f ? &yl : 0);
            zsvf_tick(&bus.fl[f][1], r * 16, f ? 0 : &yr, 0, f ? &yr : 0);
            yl >>= 4;
            yr >>= 4;
            if (bus.fgain[f] < 32767) { yl = (int32_t)mul_sh(yl, bus.fgain[f], 15); yr = (int32_t)mul_sh(yr, bus.fgain[f], 15); }
            if (bus.fmix[f] >= 32767) { l = yl; r = yr; }
            else { l += (int32_t)mul_sh(yl - l, bus.fmix[f], 15); r += (int32_t)mul_sh(yr - r, bus.fmix[f], 15); }
        }
        /* the 4-bit crunch, its rate falling to a third at the top */
        if (ad_run) {
            bus.ad_acc += 256;
            if (bus.ad_acc >= ad_div) {
                bus.ad_acc -= (uint16_t)ad_div;
                bus.ad_hold[0] = adpcm_step(0, sat16(l));
                bus.ad_hold[1] = adpcm_step(1, sat16(r));
            }
            l += ((bus.ad_hold[0] - l) * ad_mix) >> 7;
            r += ((bus.ad_hold[1] - r) * ad_mix) >> 7;
        }
        /* hiss and dust: only while the tape rolls, fading in and out with it */
        hgain += (htgt - hgain) >> 13;
        if (hiss && hgain > 64) {
            int32_t n = (int32_t)noise_step(&bus.hs) >> 16;
            bus.hlp += ((n - bus.hlp) * 9000) >> 15;
            int32_t h = (bus.hlp * T_HISS_G[hiss]) >> 15;       /* the hiss and the dust on their own laws */
            /* a crackle's chance: the noise's low 18 bits against the old value, three more bits its fraction (so a
             * whole old value picks its clicks, and their levels, as it did) */
            uint32_t cr = ((bus.hs & 0x3FFFF) << 3) | ((bus.hs >> 18) & 7);
            if (cr < T_CRK_P[hiss] && !bus.crackle) bus.crackle = (int32_t)(1500 + (bus.hs >> 21)) * T_CRK_A[hiss] >> 10;
            if (bus.crackle) { h += (bus.hs & 0x100000) ? bus.crackle : -bus.crackle; bus.crackle = (bus.crackle * 5) >> 3; if (bus.crackle < 100) bus.crackle = 0; }
            h = (h * hgain) >> 15;
            l += h;
            r += h;
        }
        if (pdrv) {                                       /* DRIVE: the control's output beside it, for the level */
            uint64_t lr0 = mul2_q15(mvol, (l * drv0) >> 8, (r * drv0) >> 8);
            int32_t l0 = lim_soft((int32_t)lr0) >> 4, r0 = lim_soft((int32_t)(lr0 >> 32)) >> 4;
            e0 += (uint32_t)(l0 * l0 + r0 * r0) >> 5;
        }
        int32_t dri = pdrv ? dvp + (((drv - dvp) * (i + 1)) >> 4) : drv;
        uint64_t lr = mul2_q15(mvol, (l * dri) >> 8, (r * dri) >> 8);
        l = lim_soft((int32_t)lr);
        r = lim_soft((int32_t)(lr >> 32));
        if (pdrv) {
            int32_t dgi = dgp + (((dvg - dgp) * (i + 1)) >> 4);
            e1 += (uint32_t)((l >> 4) * (l >> 4) + (r >> 4) * (r >> 4)) >> 5;
            l = (l * dgi) >> 15;
            r = (r * dgi) >> 15;
        }
        /* KILL: down and back over 6 ms on an S (a straight line's corners were a click's worth of top: +8 dB) */
        if (perf.kill != (perf.on[PF_KILL] ? 0 : 32767)) {
            perf.kill += perf.on[PF_KILL] ? -124 : 124;
            perf.kill = CLAMP(perf.kill, 0, 32767);
        }
        if (perf.kill < 32767) {
            int32_t s = (perf.kill * perf.kill) >> 15, kg = (s * ((98304 - 2 * perf.kill) >> 1)) >> 14;   /* u^2 (3 - 2u) */
            l = (l * kg) >> 15;
            r = (r * kg) >> 15;
        }
        if (gate) {                                       /* GATE: open the first half of each sixteenth */
            uint32_t q = gph + (uint32_t)i;
            while (q >= gsix) q -= gsix;
            int32_t o;
            if (q < 64) { int32_t u = (int32_t)q << 9; o = (((u * u) >> 15) * ((98304 - 2 * u) >> 1)) >> 14; }   /* 1.5 ms on an S */
            else if (q < gsix / 2) o = 32767;
            else if (q < gsix / 2 + 256) o = (int32_t)(gsix / 2 + 256 - q) << 7;
            else o = 0;
            int32_t gd = gtd0 + (((bus.gtd - gtd0) * (i + 1)) >> 4), gg = 32767 - (((32767 - o) * gd) >> 15);
            l = (l * gg) >> 15;
            r = (r * gg) >> 15;
        }
        out[2 * i] = (int16_t)l;
        out[2 * i + 1] = (int16_t)r;
        if ((i & 7) == 0) {
            int32_t m = (l + r) >> 1;
            mon.scope[mon.scope_w++] = (int16_t)m;
            olv += m < 0 ? -m : m;
        }
    }
    /* the output's level (two samples a block, Q4): over ~0.4 s, and FREEZE's over ~90 ms, its gain toward the
     * level before it every 16 blocks (a quarter of the way: no pumping on a sustain) */
    bus.olev += (olv * 8 - bus.olev) >> 10;
    if (bus.g_on && bus.g_mode == GM_FRZ && bus.g_env >= 1024) {
        bus.g_lev += (olv * 8 - bus.g_lev) >> 8;
        if (!(++bus.g_nb & 15)) {
            uint32_t q = ((uint32_t)bus.g_tgt << 4) / ((uint32_t)bus.g_lev >> 8 | 1u);   /* Q12 (the levels < 2^19) */
            int32_t want = (int32_t)CLAMP((bus.g_agc * (int32_t)(q > 16384u ? 16384u : q)) >> 12, 8192, 40000);
            bus.g_agt += (want - bus.g_agt) >> 2;
        }
    }
    if (pdrv) {                                           /* the level DRIVE may have: the control's, under 31/32 of full */
        bus.dv_e0 += ((int32_t)e0 - bus.dv_e0) >> 6;
        bus.dv_e1 += ((int32_t)e1 - bus.dv_e1) >> 6;
        int32_t want = 32767;
        if (drv != drv0) {                                /* (at least the small signal's: drv0 / drv) */
            uint32_t w = bus.dv_e1 > 0 ? isqrt32(((uint32_t)bus.dv_e0 << 9) / ((uint32_t)bus.dv_e1 >> 11 | 1u)) << 5 : 32767;
            int32_t lo = (drv0 << 15) / drv;
            want = drv < drv_t ? lo : CLAMP((int32_t)w, lo, 31743);   /* (rising, at the gain's ratio) */
        }
        if (want < bus.dv_g) bus.dv_g = want;             /* down at once, up gently */
        else bus.dv_g += (want - bus.dv_g) >> 4;
        if (want == 32767 && bus.dv_g > 32767 - 16) bus.dv_g = 32767;
    }
    bus.g_phase = g_phase;
    bus.hgain = hgain;
    mon.glitch = bus.g_on;
    mon.gmode = bus.g_mode;
}

/* ------------------------------------------------------------------------------------------- the render */

static void seq_block(void);
static int seq_running(void);
static void mx_gates_out(void);

/* the second core's share of a block: tracks par_from..5, into their own sums (par_L ...); it touches no MIDI (a
 * gate's end only marked, its note-off sent by the first core after both) */
int (*m2k_par_begin)(void (*job)(void));
void (*m2k_par_end)(void);
static int par_from = M2K_TRACKS;
static int32_t par_L[M2K_SUB], par_R[M2K_SUB], par_rs[M2K_SUB], par_ds[M2K_SUB];

static void par_job(void)
{
    m_set(par_L, 0, sizeof par_L);
    m_set(par_R, 0, sizeof par_R);
    m_set(par_rs, 0, sizeof par_rs);
    for (int t = par_from; t < M2K_TRACKS; t++) {
        m_set(par_ds, 0, sizeof par_ds);
        track_block(t, par_L, par_R, par_ds, par_rs);
        delay_block(t, par_ds, par_L, par_R);
    }
}

/* where the tracks split: an FX machine stays with the track it hears (the one before it, FX-RING's EXT the one before
 * that), and the work is shared out as each track's last block weighed it (T_MCOST: its machine, chain and stages, its
 * delay while it rings), the first core keeping the sequencer, the LFOs and the bus as their controls stand. In Mac
 * instructions a sample, measured as T_MCOST is */
#define C_CORE1 140                    /* the sequencer, the mix, the bus dry */
#define C_LFO 8                        /* an LFO that moves something */
#define C_VERB 85                      /* (the feedback delay network; the combs were 72) */
#define C_TAPE 97
#define C_ADPC 129
#define C_HISS 34
#define C_IDLE 12                      /* a track that does not sound */
#define C_DLY 92                       /* a delay that rings: one line, ping-pong */
#define C_DLY2 116
#define C_PAR 10                       /* the second core's sums */
#define C_PF_RING 75                   /* PERFORM: a repeat, reverse, stop, stutter, scatter, roll, spin */
#define C_PF_GR 140                    /* FREEZE or an octave: three grains */
#define C_PF_FLT 155                   /* LPF or HPF */
#define C_PF_SHIM 110                  /* SHIMMER's octave and send (the reverb's own C_VERB) */
#define C_PF_GATE 37
static int par_split(void)
{
    const m2k_pattern_t *pp = PAT(cur_pat);
    const uint8_t *mc = pp->machine, *g = pp->glob;
    int32_t c[M2K_TRACKS], a = C_CORE1 + modpn * C_LFO + (g[G_VERB] || bus.sh_on ? C_VERB : 0) + (g[G_TAPE] || perf.on[PF_WOW] ? C_TAPE : 0)
                                  + (g[G_ADPC] || perf.on[PF_CRSH] ? C_ADPC : 0) + (g[G_HISS] ? C_HISS : 0), b = C_PAR;
    /* PERFORM's on the first core (measured as T_MCOST, a demo with the key held against without: DUB's delay is its
     * track's above) */
    if (bus.g_on) a += bus.g_mode == GM_FRZ || bus.g_mode == GM_PSU || bus.g_mode == GM_PSD ? C_PF_GR : C_PF_RING;
    if (bus.fmix[0]) a += C_PF_FLT;
    if (bus.fmix[1]) a += C_PF_FLT;
    if (bus.sh_on) a += C_PF_SHIM;
    if (bus.gtd) a += C_PF_GATE;
    for (int t = 0; t < M2K_TRACKS; t++) {
        c[t] = (trk[t].sounding ? trk[t].ccost : C_IDLE) + (trk[t].dsleep ? 0 : trk[t].dmode ? C_DLY2 : C_DLY);
        b += c[t];
    }
    int best = M2K_TRACKS;
    int32_t cost = INT32_MAX;
    for (int s = 1; s < M2K_TRACKS; s++) {
        a += c[s - 1];
        b -= c[s - 1];
        if (mc[s] == M_FXC || mc[s] == M_FXR || mc[s] == M_FXD || (s + 1 < M2K_TRACKS && mc[s + 1] == M_FXR)) continue;
        int32_t w = a > b ? a : b;
        if (w < cost) { cost = w; best = s; }
    }
    return best;
}

void m2k_render(int16_t *lr, int frames)
{
    static int32_t L[M2K_SUB], R[M2K_SUB], rs[M2K_SUB], ds[M2K_SUB];
    for (int f = 0; f + M2K_SUB <= frames; f += M2K_SUB) {
        PROF_START();
        seq_block();
        tape_rolling = seq_running();
        for (int t = 0; t < M2K_TRACKS; t++) if (trk[t].sounding) tape_rolling = 1;
        for (int t = 0; t < M2K_TRACKS; t++) lfo_block(t);
        mod_apply();
        m_set(L, 0, sizeof L);
        m_set(R, 0, sizeof R);
        m_set(rs, 0, sizeof rs);
        hold_bpm = ext_bpm ? ext_bpm : 30u + PAT(cur_pat)->glob[G_TMPO];
        PROF(0);
        int split = M2K_TRACKS;
        if (m2k_par_begin) {
            par_from = par_split();
            if (par_from < M2K_TRACKS && m2k_par_begin(par_job)) split = par_from;
        }
        for (int t = 0; t < split; t++) {
            m_set(ds, 0, sizeof ds);
            track_block(t, L, R, ds, rs);
            delay_block(t, ds, L, R);
            PROF(3);
        }
        if (split < M2K_TRACKS) {
            m2k_par_end();
            for (int i = 0; i < M2K_SUB; i++) { L[i] += par_L[i]; R[i] += par_R[i]; rs[i] += par_rs[i]; }
        }
        mx_gates_out();                /* the gates ended on either core: their notes out, in track order */
        bus_block(L, R, rs, lr + 2 * f);
        PROF(4);
    }
}
