/* SPDX-License-Identifier: GPL-3.0-only */
/* The bank A that shipped until factory revision 9 (MAMMOTH .. SPLICE), kept for the tests and never built into the
 * instrument: real patterns written on the old pages (GND-SIN's TUNE first, FM+ and SID on MONO2K's old pages, HOLD
 * in 14.6 ms steps, VOX-6's old page) and turned at build, so the format's conversions are held on parts as they
 * were, and a store saved by an older revision is rebuilt as it shipped. factory_rev9() writes them over bank A. */

/* the old pages' choices */
#define SIDW(i) enc(i, 8)              /* 0 TRI 1 SAW 2 PUL 3 NOI 4 T+S 5 T+P 6 S+P 7 ALL */
#define MODE4(i) enc(i, 4)             /* SID: OFF SYNC RING S+R; FM+STAT: STCK PAR 2CAR RING */
#define RATIO(i) enc(i, 32)            /* 3: 1.0, 6: 2.0, 8: 2.76, 9: 3.0, 10: 3.5, 11: 4.0, 13: 5.0, 15: 6.0, 17: 7.07, 26: 15.7 */
#define SEMI(n) enc(n, 25)             /* ENS intervals, 0..24 semitones */
#define VOWEL(i) ((uint8_t)((i) * 127 / 7))   /* A AE E I Y O U ER */
#define VOICES(n) enc((n) - 1, 7)

/* a demo written on today's chain, the Monomachine's (version 5): DIST 64 clean, the filter's steps over the note
 * (16 on it, 8 an octave), DTIM in 256ths of a note, DSND from 64 (below keeps the side, above ping-pongs) */
static m2k_pattern_t *pat_v5(int i, const char *name, int bpm, const uint8_t *machines)
{
    m2k_pattern_t *p = PAT(i);
    pat_init(p, name);
    for (int t = 0; t < M2K_TRACKS; t++) {
        kit_default(p, t, machines[t]);
        if (machines[t] == M_GSIN) m_cpy(&p->kit[t][P_SYN], GSIN_V11, 8);  /* the parts are written on that page */
    }
    p->glob[G_TMPO] = (uint8_t)(bpm - 30);
    p->glob[G_VERB] = 50;
    return p;
}

/* A01 MAMMOTH's parts (direction v5, the owner's picks round by round, 2026-10-19..20): the drums A (an 808 with its
 * sub tail) and B (a curved drop) by bars, the DROP bass after Gabor Lazar's "pathway" (a pitch drop that is the bass,
 * its saturation CRUSH, FOLD and both hit to hit), a held Gm(add9) pad moving one voice at a time over eight bars (trig
 * conditions 1:2 and 2:2), and the Monomachine's metal: FM+PAR into FX-CHORUS on the next track, fed back hard, its
 * comb ringing an elephant's calls and the snare's clang. Written on GND-SIN's old page, turned at build as the rest */

static void mm_cyc(m2k_pattern_t *p, int t, int prm, const uint8_t *v, int n)   /* each trig of the track the next value */
{
    for (int st = 0, k = 0; st < M2K_STEPS; st++)
        if (p->step[t][st].flags & ST_TRIG) lk(p, t, st, prm, v[k++ % n]);
}

#define MM_CYC(p, t, prm, ...) mm_cyc(p, t, prm, NOTES(__VA_ARGS__))

static void mm_chorus4(m2k_pattern_t *p, int t, int rate, int dep, int mix)
{
    SET(p, t, P_MODT, enc(0, 3), P_MODR, rate, P_MODD, dep, P_MODM, mix);
}

static void mm_bass4_drop(m2k_pattern_t *p)
{
    /* "pathway": GND-SIN (old page) folded, dropping 110 -> 50 Hz over 150 ms, bright at the hit and pure on the body (a
     * filter envelope over a low WDTH); the hits between the kicks, three steps on (three against four) */
    SET(p, 3, SA, 64, SB, 30, SC, 0, SD, 0, SE, 90, SF, 84, SG, 60, SH, 0, P_ATK, 0, P_HOLD, 10, P_DEC, 37, P_REL, 0,
        P_DIST, 72, P_VOL, 127, P_BASE, 0, P_WDTH, 24, P_FATK, 0, P_FDEC, 28, P_WOF, 90, P_GATE, 127);
    BEAT(p, 3, "...X.....X.....x|......X.x.....X.|...X.....X.....x|.....X....X.....",
         31, 31, 31, 31, 34, 31, 31, 31, 29, 31, 27, 29);
    MM_CYC(p, 3, SE, 90, 90, 110, 84, 90, 100);                               /* the drop's depth, hit to hit */
    MM_CYC(p, 3, SB, 30, 42, 30, 22, 56);                                      /* the fold */
    lfo(p, 3, 0, P_WOF, 1, 0, 30, 16, 0);                                   /* the hit's brightness, ~0.32 Hz */
    lfo(p, 3, 1, SH, 0, 0, 20, 16, 0);                                      /* the 2nd harmonic, ~0.21 Hz */
    lfo(p, 3, 2, P_FDEC, 1, 0, 44, 20, 0);                                  /* how long the hit stays bright, ~0.47 Hz */
    mm_chorus4(p, 3, 40, 40, 110);
}

static void mm_drop5(m2k_pattern_t *p)
{
    mm_bass4_drop(p);
    SET(p, 3, P_HOLD, 18, P_DEC, 42);                                       /* 263 ms, then -20 dB in 136 ms */
    MM_CYC(p, 3, P_HOLD, 18, 24, 14, 30, 18, 20, 26);                          /* 204..438 ms, hit to hit */
}

static void mm_drop6(m2k_pattern_t *p)
{
    mm_drop5(p);
    SET(p, 3, P_DEC, 50, P_REL, 40);
}

#define MM_SNARE_GRID "........X.......|........X......?|........X.......|........X.....x."

static void mm_hats7(m2k_pattern_t *p, int red, int bits, int dec)
{
    SET(p, 1, SA, 127, SB, (uint8_t)red, SC, 127, SH, 64, P_ATK, 0, P_HOLD, 0, P_DEC, (uint8_t)dec, P_REL, 2, P_BASE, 48,
        P_DIST, 80, P_BITS, (uint8_t)bits, P_VOL, 92, P_DTIM, 16, P_DSND, 104, P_DFB, 40, P_DBAS, 90);
    BEAT(p, 1, "x.xxX.x2x.xxX.x3|x.xxX3x.x.xxX2x4|x.xxX.x2x.xxX.x3|x4xxX2x3x.x4X4x4", 60);
    MM_CYC(p, 1, SB, (uint8_t)red, (uint8_t)(red + 30), (uint8_t)red, (uint8_t)(red + 60));       /* bright, darker */
    MM_CYC(p, 1, SA, 127, 100, 127, 80, 127);                                   /* how wide, hit to hit */
    lfo(p, 1, 0, P_DEC, 6, 3, 56, 60, 0);
    lfo(p, 1, 1, P_BASE, 1, 0, 30, 30, 0);
    lfo(p, 1, 2, P_SRR, 6, 3, 44, 50, 0);
}

static void mm_snare7(m2k_pattern_t *p, int dec, int dist, int grit)
{
    /* GND-PERC: noise thinned, the six-square metal at a spread, a short drop for the crack */
    SET(p, 2, SA, 84, SB, 0, SC, 104, SD, 84, SE, 70, SF, 18, SG, (uint8_t)grit, P_ATK, 0, P_HOLD, 0, P_DEC, (uint8_t)dec,
        P_REL, 20, P_BASE, 20, P_DIST, (uint8_t)dist, P_VOL, 112, P_PAN, 64, P_DTIM, 24, P_DSND, 96, P_DFB, 50,
        P_DBAS, 70, P_MODT, enc(0, 3), P_MODR, 50, P_MODD, 40, P_MODM, 110);
    BEAT(p, 2, MM_SNARE_GRID, 60);
    lfo(p, 2, 0, SD, 6, 3, 40, 30, 0);                                      /* the metal's spread, hit to hit */
    lfo(p, 2, 1, P_DEC, 6, 3, 44, 20, 0);
    lfo(p, 2, 2, SA, 1, 0, 20, 20, 0);
}

static void mm_trig(m2k_pattern_t *p, int t, int st, int note, int acc)
{
    m2k_step_t *x = &p->step[t][st];
    x->flags = (uint8_t)(ST_TRIG | (acc ? ST_ACC : 0)); x->note = (uint8_t)note; x->cond = C_ALWAYS; x->rtrg = 0;
}

static void mm_lks(m2k_pattern_t *p, int t, int st, const uint8_t *pv, int n)
{
    for (int i = 0; i + 1 < n; i += 2) lk(p, t, st, pv[i], pv[i + 1]);
}

#define MM_LKS(p, t, st, ...) do { const uint8_t _v[] = {__VA_ARGS__}; mm_lks(p, t, st, _v, sizeof _v); } while (0)

static void mm_combo9(m2k_pattern_t *p)
{
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 0; p->glob[G_GLCH] = 22; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 50;
    p->glob[G_TAPE] = 6; p->glob[G_HISS] = 4; p->glob[G_VERB] = 30; p->glob[G_DAMP] = 50; p->glob[G_ACNT] = 80;
    p->glob[G_DRIV] = BANK_DRIVE;
    static const char *const KA = "X.........x.....|X.....x.........|X.........x.....|X.....x.....x...";
    static const char *const KB = "X.....x.....x...|X..x......x.x...|X.....x.....x...|X..x....x...x.4.";
    static const char *const BL = "...x..x.....x..x|.x......x..x....|...x..x.....x..x|.x....x.x..3..x.";
    static const uint8_t BN[6] = {79, 74, 82, 77, 74, 86};
    /* the kick: B's curved drop in the kit, A's 808 by locks on bars 1 and 3 */
    SET(p, 0, SA, 64, SB, 24, SC, 0, SD, 0, SE, 127, SF, 84, SG, 70, SH, 10, P_ATK, 0, P_HOLD, 6, P_DEC, 44, P_REL, 6,
        P_DIST, 92, P_VOL, 124);
    for (int st = 0; st < M2K_STEPS; st++) {
        int bbar = (st / 16) & 1, c = (bbar ? KB : KA)[st + st / 16];
        if (c == '.') continue;
        mm_trig(p, 0, st, bbar ? 34 : 31, c == 'X');
        if (c == '4') p->step[0][st].rtrg = 4;
        if (!bbar) MM_LKS(p, 0, st, SB, 12, SE, 118, SF, 60, SG, 40, P_HOLD, 20, P_DEC, 50, P_REL, 10, P_DIST, 84);
    }
    lfo(p, 0, 0, SE, 6, 3, 40, 24, 0);
    lfo(p, 0, 1, P_SRR, 6, 3, 36, 50, 0);
    lfo(p, 0, 2, SB, 1, 0, 20, 20, 0);
    mm_hats7(p, 0, 34, 4);
    mm_snare7(p, 55, 100, 10);
    for (int st = 0; st < M2K_STEPS; st++)
        if ((p->step[2][st].flags & ST_TRIG) && ((st / 16) & 1)) MM_LKS(p, 2, st, P_DEC, 36, P_DIST, 104, SG, 20);
    /* FM+PAR: B's blips (bars 2 and 4) in the kit, A's partials on A's snares by locks */
    SET(p, 4, SA, FR24(19), SB, 90, SC, FR24(15), SD, 80, SE, FR24(21), SF, 70, SG, 110, SH, 64, P_ATK, 0, P_DEC, 22,
        P_REL, 6, P_DIST, 90, P_VOL, 80, P_DTIM, 32, P_DSND, 110, P_DFB, 50, P_SPU, 24, P_PAN, 64, P_MODT, enc(0, 3),
        P_MODR, 60, P_MODD, 40, P_MODM, 127);
    for (int st = 16, n = 0; st < M2K_STEPS; st++) {
        int c = BL[st + st / 16];
        if (c == '.' || !((st / 16) & 1)) continue;
        mm_trig(p, 4, st, BN[n++ % 6], 0);
        if (c == '3') p->step[4][st].rtrg = 3;
    }
    for (int k = 0; k < 2; k++) {
        int st = k ? 40 : 8;
        mm_trig(p, 4, st, 67, 1);
        MM_LKS(p, 4, st, SA, FR24(22), SB, 100, SC, FR24(18), SD, 96, SE, FR24(20), SF, 92, SG, 120, P_DEC, 52, P_REL, 20,
            P_DIST, 96, P_VOL, 84, P_DTIM, 48, P_DSND, (uint8_t)(k ? 124 : 100), P_DFB, 64, P_SPU, 30, P_MODR, 40, P_MODD, 50);
    }
    lfo(p, 4, 0, P_PTCH, 0, 4, 64, 3, 0);
    lfo(p, 4, 1, P_PAN, 6, 3, 40, 100, 0);
    lfo(p, 4, 2, SG, 1, 0, 24, 30, 0);
    mm_drop6(p);
    MM_CYC(p, 3, P_BITS, 100, 0, 90);
    MM_CYC(p, 3, P_SRR, 50, 0, 44);
    MM_CYC(p, 3, P_FOLD, 0, 70, 56);
    MM_CYC(p, 3, P_XOR, 0, 40, 30);
    MM_CYC(p, 3, P_DIST, 88, 92, 90);
}

static const uint8_t MM_SUS[4][3] = {{69, 73, 100}, {73, 100, 116}, {73, 100, 109}, {69, 93, 100}};

typedef struct { uint8_t st, note, flags; } mm_ln_t;

static void mm_pad12t(m2k_pattern_t *p, int t)                        /* pad12 B on track t */
{
    p->glob[G_VERB] = 64; p->glob[G_DAMP] = 64;
    SET(p, t, SA, 69, SB, 73, SC, 100, SD, 10, SE, 64, SF, 110, SG, 127, SH, 64, P_ATK, 90, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_PORT, 0, P_WDTH, 52, P_LPQ, 16, P_DIST, 64, P_VOL, 84, P_DTIM, 96, P_DSND, 100,
        P_DFB, 70, P_DBAS, 80, P_DWID, 40, P_SPU, 116, P_PAN, 64, P_MODT, enc(0, 3), P_MODR, 24, P_MODD, 30, P_MODM, 90);
    for (int st = 0; st < M2K_STEPS; st += 4)
        for (int k = 0; k < 2; k++) {
            const uint8_t *c = MM_SUS[2 * k + st / 32];
            mm_trig(p, t, st + k, 55, 0); p->step[t][st + k].flags |= ST_SLD;
            p->step[t][st + k].cond = (uint8_t)(k ? C_2_2 : C_1_2);
            MM_LKS(p, t, st + k, SA, c[0], SB, c[1], SC, c[2]);
        }
    lfo(p, t, 0, P_WDTH, 0, 0, 64, 70, 0);
    lfo(p, t, 1, P_PAN, 1, 0, 32, 80, 0);
    lfo(p, t, 2, P_VOL, 0, 1, 64, 66, 0);
}

static const mm_ln_t MM_ELE2[] = {
    {2, 55, ST_ACC}, {4, 55, 0},
    {10, 74, ST_ACC}, {12, 79, ST_SLD}, {15, 81, ST_SLD}, {18, 79, ST_SLD}, {20, 74, ST_SLD},
    {34, 55, ST_ACC}, {36, 57, 0},
    {42, 77, ST_ACC}, {44, 84, ST_SLD}, {47, 82, ST_SLD}, {50, 79, ST_SLD}, {52, 74, ST_SLD},
    {60, 55, 0}};

static void mm_metal11(m2k_pattern_t *p, int del)
{
    mm_combo9(p);
    for (int t = 2; t < 6; t++) if (t != 3)                                  /* the steps blank, their notes too */
        for (int st = 0; st < M2K_STEPS; st++) { m2k_step_t *x = &p->step[t][st]; x->flags = 0; x->note = 60; x->cond = C_ALWAYS; x->rtrg = 0; }
    for (int k = 0; k < M2K_LOCKS; k++)                                       /* combo9's snare and blips: gone */
        if (p->lock[k].trk == 2 || p->lock[k].trk == 4) p->lock[k].trk = 0xFF;
    m_set(&p->kit[4][P_LFO], 0, 24);
    for (int l = 0; l < 3; l++) p->kit[4][P_LFO + 8 * l + L_DPTH] = 64;
    mm_pad12t(p, 2);
    /* the snare's noise on the hats track */
    for (int st = 8; st < M2K_STEPS; st += 16) { mm_trig(p, 1, st, 60, 1); MM_LKS(p, 1, st, SB, 0, SA, 127, P_DEC, 30, P_BASE, 22, P_VOL, 112); }
    /* T5: the FM, plucked; the elephant's calls between the snares, the snare's clang on 8 24 40 56 */
    SET(p, 4, SA, FR24(22), SB, 112, SC, FR24(18), SD, 96, SE, FR24(15), SF, 88, SG, 120, SH, 64, P_ATK, 0, P_HOLD, 6,
        P_DEC, 40, P_REL, 20, P_PORT, 96, P_GATE, 127, P_DIST, 72, P_VOL, 24, P_WDTH, 127, P_DSND, 64, P_SPU, 20,
        P_PAN, 64, P_MODM, 0);
    for (unsigned i = 0; i < sizeof MM_ELE2 / sizeof MM_ELE2[0]; i++) {
        mm_trig(p, 4, MM_ELE2[i].st, MM_ELE2[i].note, MM_ELE2[i].flags & ST_ACC);
        p->step[4][MM_ELE2[i].st].flags |= MM_ELE2[i].flags & ST_SLD;
        if (MM_ELE2[i].st == 4 || MM_ELE2[i].st == 36 || MM_ELE2[i].st == 60) p->step[4][MM_ELE2[i].st].cond = C_P50;
    }
    for (int st = 8; st < M2K_STEPS; st += 16) {
        int b = (st / 16) & 1;                                            /* bars 2 and 4: B's crack */
        mm_trig(p, 4, st, 62, 1);
        MM_LKS(p, 4, st, SA, FR24(22), SB, 120, SC, FR24(20), SD, 108, SE, FR24(23), SF, 100, SG, 127, P_DEC, (uint8_t)(b ? 24 : 34),
            P_PORT, 0);
    }
    lfo(p, 4, 0, SH, 0, 3, 70, 69, 0);                                      /* the elephant's vibrato */
    lfo(p, 4, 1, SB, 2, 2, 64, 54, 0);                                      /* the tutorial's inverted square */
    lfo(p, 4, 2, P_PAN, 1, 0, 32, 82, 0);
    /* T6: FX-CHORUS, the comb, opened by every T5 trig; its gate short so the hits breathe */
    SET(p, 5, SA, (uint8_t)del, SB, 12, SC, 10, SD, 127, SE, 120, SF, 110, SG, 116, SH, 100, P_ATK, 0, P_HOLD, 10,
        P_DEC, 50, P_REL, 20, P_GATE, 127, P_VOL, 108, P_DIST, 64, P_PAN, 64, P_SPU, 40, P_DSND, 96, P_DTIM, 48, P_DFB, 40);
    for (int st = 0; st < M2K_STEPS; st++) if (p->step[4][st].flags & ST_TRIG) mm_trig(p, 5, st, 60, 0);
    for (int st = 8; st < M2K_STEPS; st += 16) lk(p, 5, st, P_DEC, (uint8_t)(((st / 16) & 1) ? 44 : 58));
}

static void factory_rev9(void)
{
    m2k_pattern_t *p;

    /* the parts in the order written; each one's slot is pat_v5's first argument. The bank's order mixes the energies,
     * each row of four opening on a hard part (MAMMOTH, CAROM, SPLINTR, RIBBON) */

    /* A03 HAZE, after Boards of Canada: D minor at 88, a one-chord vamp, Dm7 opening to Dsus2 (no third) in the
     * second half: a dark pad, chorused and wobbling on the tape (the detune is the point: TAPE 80, about +-10 c,
     * slow); a dusty beat with room in it and a sub that walks under it carry it. Over it a fragment of four long notes,
     * A G, then E D, low on one of the owner's dark waves, low-passed, bit-worn and lightly driven into a dark echo */
    p = pat_v5(2, "HAZE", 88, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_GSIN, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 18; p->glob[G_TAPE] = 80; p->glob[G_HISS] = 56; p->glob[G_ADPC] = 16;
    p->glob[G_VERB] = 74; p->glob[G_DAMP] = 80; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 56, SF, 28, SG, 16, SC, 30, P_DEC, 34, P_REL, 10, P_DIST, 66, P_VOL, 120);
    BEAT(p, 0, "x.........x.....|x.......x.......|x.........x.....|x.......x..h....", 38);
    SET(p, 1, SA, 62, SC, 20, SD, 40, SE, 20, SF, 20, SG, 16, P_DEC, 30, P_REL, 14, P_BASE, 18, P_SRR, 30, P_BITS, 20,
        P_SPU, 50, P_VOL, 95);
    BEAT(p, 1, "....x.......x...|....x.......x...|....x.......x...|....x.......x.q.", 60);
    SET(p, 2, SA, 100, P_ATK, 10, P_DEC, 14, P_REL, 8, P_BASE, 36, P_SRR, 40, P_VOL, 60, P_PAN, 58);
    BEAT(p, 2, "..x...x...x...x.|..x...x?..x...x.|..x...x...x...x.|..x...x...x?..x.", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 30, SF, 70, SG, 30, P_ATK, 80, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 80, P_VOL, 95);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 50);
    chords(p, 3, 32, 64, 2, 7, 12);                                         /* Dsus2 */
    lfo(p, 3, 0, P_WDTH, 1, 0, 16, 70, 0);
    SET(p, 4, SE, 0, SC, 40, SG, 0, SH, 6, P_HOLD, 127, P_DEC, 127, P_REL, 40, P_GATE, 60, P_WDTH, 24, P_VOL, 65);
    BEAT(p, 4, "x...............|x.........x.....|x...............|x.........x.....", 38, 38, 36, 38, 34, 33);
    SET(p, 5, SA, OS(30), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 20, P_HOLD, 127,
        P_DEC, 127, P_REL, 60, P_GATE, 56, P_WDTH, 24, P_DIST, 66, P_SRR, 30, P_DTIM, 64, P_DSND, 30, P_DFB, 50,
        P_DBAS, 10, P_DWID, 30, P_SPU, 70, P_VOL, 51, P_PAN, 70);
    BEAT(p, 5, "................|..x.......x.....|................|..x.......h.H...", 57, 55, 52, 50, 45);
    trim(p, 93);

    /* A16 SPLICE, after Autechre (the mid-90s): E phrygian at 104 in 14 beats (56 steps), so the rhythm turns against
     * the bar. The idea is the metal: a low FM clank whose decay is new every few steps and whose ratio is locked to a
     * cleaner tick here and there, ratcheting, on trig conditions, with a kick off the grid and hats crushed to a
     * console's noise. Under it an open chord with no third (E B D E) that turns to Fmaj7 every second loop, a sub,
     * and four long notes, B A G, then E, low on one of the owner's buzzing waves, closed down and crushed */
    p = pat_v5(15, "SPLICE", 104, MACHINES(M_GSIN, M_FMD, M_GPRC, M_WAV, M_ENS, M_GSIN));
    p->glob[G_LEN] = 56; p->glob[G_SWNG] = 4; p->glob[G_GLCH] = 24; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 24;
    p->glob[G_DRIV] = 32; p->glob[G_VERB] = 40; p->glob[G_DAMP] = 60;
    SET(p, 0, SE, 50, SF, 18, SG, 50, SC, 10, P_DEC, 22, P_REL, 6, P_DIST, 74, P_VOL, 127);
    BEAT(p, 0, "x.....x...h.....|x..?....x..H..x.|x.....x...k.....|x..q..x.", 40);
    SET(p, 1, SA, RATIO(17), SB, 70, SC, 22, SD, RATIO(26), SE, 36, SF, 12, SG, 30, SH, 100, P_DEC, 20, P_REL, 6,
        P_GATE, 4, P_WDTH, 56, P_DIST, 72, P_DTIM, 24, P_DSND, 100, P_DFB, 36, P_DWID, 40, P_SPU, 30, P_VOL, 56,
        P_PAN, 44);
    BEAT(p, 1, "..x.x..2...x.x..|.x...x.3..x..x.x|..x.x..2...x.x..|.x..4.x.", 52);
    for (int st = 11; st < 56; st += 16) lk(p, 1, st, SA, RATIO(13));      /* a cleaner tick: 5.0 */
    lk(p, 1, 20, SB, 96);
    lfo(p, 1, 0, SC, 6, 2, 48, 80, 0);                                      /* its decay new every few steps */
    SET(p, 2, SA, 112, SG, 70, P_DEC, 6, P_REL, 4, P_BASE, 40, P_VOL, 62);
    BEAT(p, 2, "x?x.x?xex?x.x?x2|x?x.x?xex?xqx?x.|x?x.x?xex?x.x?x2|x?x.3?x.", 60);
    lfo(p, 2, 0, P_PAN, 6, 2, 64, 100, 0);
    SET(p, 3, SA, OS(42), SB, 64, SC, 127, SE, 40, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 30, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 48, P_WDTH, 22, P_DIST, 69, P_SRR, 40, P_DTIM, 72, P_DSND, 34, P_DFB, 52,
        P_DBAS, 20, P_DWID, 30, P_SPU, 50, P_VOL, 85, P_PAN, 80);
    BEAT(p, 3, "........x.......|....x...x.......|............x...|........", 59, 57, 55, 52);
    SET(p, 4, SA, PCH(7), SB, PCH(10), SC, PCH(12), SD, 20, P_ATK, 90, P_HOLD, 127, P_DEC, 127, P_REL, 96,
        P_GATE, 127, P_WDTH, 24, P_LPQ, 0, P_SPU, 70, P_VOL, 110);
    BEAT(p, 4, "x...............|............H...|x...............|........", 52, 53, 52);
    chord(p, 4, 28, 4, 7, 11);                                              /* Fmaj7, every second loop */
    SET(p, 5, SE, 0, SC, 30, SG, 0, SH, 0, P_HOLD, 127, P_DEC, 127, P_REL, 40, P_GATE, 60, P_WDTH, 24, P_VOL, 67);
    BEAT(p, 5, "x...............|............H...|x...............|........", 28, 29, 28);
    trim(p, 87);

    /* A13 RIBBON, after Aphex Twin (drill'n'bass): F minor at 168. Fast programmed breaks, kick and snare ratcheting
     * and rolling, over a pedal F in the sub; a dark pad, Fm7 then Db over F; and a slow melancholic line in its low
     * octave on one of the owner's dark waves, its second oscillator 12.5 c sharp against the first (the detune is
     * the point: the line beats), low-passed, driven and worn. Every second loop the drums drop out of the last bar
     * and the line and the pad are left alone */
    p = pat_v5(12, "RIBBON", 168, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_WAV, M_ENS, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 0; p->glob[G_VERB] = 72; p->glob[G_DAMP] = 70; p->glob[G_ADPC] = 20;
    p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_DRIV] = 28; p->glob[G_HISS] = 32;
    SET(p, 0, SE, 66, SF, 18, SG, 40, SC, 16, P_DEC, 22, P_REL, 6, P_DIST, 72, P_VOL, 127);
    BEAT(p, 0, "x..x..2...x..x..|x.x...x...3...x.|x..x..2...x..x..|h..h..h.....h...", 41);
    SET(p, 1, SA, 70, SC, 40, SD, 56, SE, 36, SF, 18, SG, 50, P_DEC, 26, P_REL, 10, P_BASE, 20, P_EQG, 70, P_SRR, 16,
        P_SPU, 30, P_VOL, 104);
    BEAT(p, 1, "....x..q.2..x..3|..q.x...x.3.x.4.|....x..q.2..x..3|....h.....h.h.h.", 60);
    SET(p, 2, SA, 110, SG, 30, P_DEC, 8, P_REL, 4, P_BASE, 42, P_VOL, 73, P_PAN, 74);
    BEAT(p, 2, "xxqxx2qxxxqxx3qx|xxqxxxqx2xqxxxqx|xxqxx2qxxxqxx3qx|h.h.h.h.h.h.h.h.", 60);
    SET(p, 3, SA, OS(47), SB, 64, SC, 127, SE, 20, SF, 65, SG, 70, SH, 0, P_WBNK, 127, P_ATK, 30, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 64, P_WDTH, 28, P_DIST, 66, P_SRR, 20, P_DTIM, 64, P_DSND, 34, P_DFB, 46,
        P_DBAS, 10, P_DWID, 40, P_SPU, 70, P_VOL, 58, P_PAN, 56);   /* osc 2 one step (12.5 c) sharp */
    BEAT(p, 3, "x.......x...x...|....x...........|x...........x...|............h...|", 60, 58, 56, 53, 56, 53, 51);
    SET(p, 4, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 40, P_ATK, 92, P_HOLD, 127, P_DEC, 127, P_REL, 92,
        P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 90, P_VOL, 72);
    BEAT(p, 4, "x.......x.......|x.......x.......|x.......x.......|x.......x.......", 53);
    chords(p, 4, 32, 64, 3, 8, 12);                                         /* Db over F */
    SET(p, 5, SE, 0, SC, 40, SG, 0, SH, 10, P_HOLD, 127, P_DEC, 127, P_REL, 20, P_GATE, 60, P_WDTH, 24, P_VOL, 67);
    BEAT(p, 5, "x.........x.....|x.........x.....|x.........x.....|x.........x.....", 41);
    trim(p, 77);

    /* A06 TENFOUR, flavour 2, after Radiohead: the opening progression of "Everything in Its Right Place" (2000),
     * from the owner's MIDI; the rest is MONO2K's own. Around C at 108, four bars. The harmony: C, Dbmaj7 and Eb6, each
     * with C on top, in the opening's proportions over the four bars (6, 4 and 4 beats, then 2 with the Eb ringing),
     * swelling on a chorused pad, driven, crushed and worn by the tape, its filter breathing. Under it a driven sine
     * bass on the roots, its sub on C1, Db1 and Eb1, walking G Bb back to C. The drums never quite repeat: a hard kick,
     * a crushed snare whose length and crack change, ghosts round it, its rate drifting; short hats in swung sixteenths
     * whose decay wanders, with metal ticks and pitched blips locked among them. Over it a four-bar line from the first
     * step, one figure (fifth, third, fifth, the top, a step down) through each chord, held on the changes and sliding
     * into some, on one of the owner's waves, crushed, its filter moving */
    p = pat_v5(5, "TENFOUR", 108, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_GSIN, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 22; p->glob[G_TAPE] = 70; p->glob[G_HISS] = 60; p->glob[G_ADPC] = 40;
    p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_VERB] = 56; p->glob[G_DAMP] = 76; p->glob[G_ACNT] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 60, SF, 22, SG, 40, SC, 40, P_DEC, 34, P_REL, 6, P_DIST, 82, P_SRR, 12, P_VOL, 127);
    BEAT(p, 0, "X.........x.....|x.......X.......|x.......X.x.....|x.........x...x.", 36);
    SET(p, 1, SA, 96, SC, 30, SD, 50, SG, 50, P_DEC, 20, P_REL, 6, P_BASE, 26, P_DIST, 78, P_BITS, 36, P_SRR, 30,
        P_SPU, 40, P_VOL, 98);
    BEAT(p, 1, "....X..x....X...|....X.......X..x|....X..x....X...|....X..x....X.x.", 60);
    static const uint8_t ghost4[] = {7, 31, 39, 55, 62};
    for (unsigned i = 0; i < sizeof ghost4; i++) lk(p, 1, ghost4[i], P_VOL, 58);     /* the ghosts, 9 dB down */
    lk(p, 1, 28, P_DEC, 32); lk(p, 1, 60, P_DEC, 32);                       /* a longer one closing bars 2 and 4 */
    lk(p, 1, 44, SC, 100); lk(p, 1, 44, SD, 90);                            /* a metal crack */
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);                                   /* its rate drifting, hit to hit */
    SET(p, 2, SA, 116, SG, 80, P_ATK, 0, P_DEC, 4, P_REL, 3, P_BASE, 44, P_SRR, 44, P_BITS, 44, P_VOL, 61, P_PAN, 60);
    BEAT(p, 2, "x?X.x?X.x?X.x?X2|x?X.x?X.x?X3x?X.|x?X.x?X.x?X.x?X2|x?X.x?X.x2X.x?X4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);                                   /* their length, never the same twice */
    lfo(p, 2, 1, P_PAN, 0, 1, 20, 84, 0);
    for (int st = 9; st < 64; st += 18) { lk(p, 2, st, SC, 110); lk(p, 2, st, SD, 90); }   /* metal ticks */
    lk(p, 2, 19, SB, 70); lk(p, 2, 51, SB, 76);                             /* pitched blips */
    lk(p, 2, 30, P_DEC, 24); lk(p, 2, 62, P_DEC, 24);
    SET(p, 3, SA, PCH(4), SB, PCH(7), SC, PCH(12), SD, 40, SF, 50, SG, 30, P_ATK, 60, P_HOLD, 127, P_DEC, 127,
        P_REL, 60, P_GATE, 127, P_WDTH, 30, P_LPQ, 0, P_DIST, 74, P_SRR, 20, P_BITS, 16, P_SPU, 60, P_VOL, 102);
    BEAT(p, 3, "x...............|........x.......|........x.......|................", 48, 49, 51);
    chord(p, 3, 24, 4, 7, 11);                                              /* Dbmaj7 */
    chord(p, 3, 40, 4, 7, 9);                                               /* Eb6 */
    lfo(p, 3, 0, P_WDTH, 1, 0, 24, 74, 0);                                  /* its filter, breathing */
    SET(p, 4, SE, 0, SC, 64, SG, 0, SH, 30, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 50, P_WDTH, 28, P_DIST, 80,
        P_VOL, 85);
    BEAT(p, 4, "x.........x.....|x.......x.......|x.......x.......|x.........x...x.", 36, 36, 36, 37, 37, 39, 39, 43, 46);
    SET(p, 5, SA, OS(16), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 16, P_HOLD, 127,
        P_DEC, 127, P_REL, 60, P_GATE, 50, P_WDTH, 30, P_DIST, 68, P_SRR, 36, P_BITS, 30, P_DTIM, 64, P_DSND, 30,
        P_DFB, 44, P_DBAS, 10, P_DWID, 30, P_SPU, 56, P_VOL, 66, P_PAN, 64);
    BEAT(p, 5, "x..x..x.s...x.x.|x..x..x.s..x..x.|s...x.x.x..x..x.|s...x.x.x...x...",
         55, 52, 55, 60, 58, 55, 52, 55, 60, 56, 53, 56, 60, 56, 58, 55, 51, 55, 60, 58, 55, 58, 55);
    for (int st = 0; st < 64; st += 8) lk(p, 5, st, P_GATE, 100);           /* held on the beats and the changes */
    lfo(p, 5, 0, P_WDTH, 1, 1, 20, 76, 0);
    trim(p, 70);

    /* A10 SMOKE, dub techno after Basic Channel: C minor at 120, one chord. The idea is a Cm7 stab, short and dark,
     * on the offbeat sixteenths, thrown into a long, filtered ping-pong echo whose feedback swells and falls; its own
     * filter breathing over the four bars. A deep four-to-the-floor, offbeat hats with shuffled ghosts, a rim far in the
     * echo, a sub rolling on C; hiss and a touch of tape under it all */
    p = pat_v5(9, "SMOKE", 120, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_GSIN, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 14; p->glob[G_TAPE] = 32; p->glob[G_HISS] = 40; p->glob[G_ADPC] = 16;
    p->glob[G_VERB] = 66; p->glob[G_DAMP] = 72; p->glob[G_DRIV] = 28;
    SET(p, 0, SE, 50, SF, 30, SG, 10, SC, 30, P_DEC, 36, P_REL, 10, P_DIST, 66, P_VOL, 127);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x..q", 36);
    SET(p, 1, SA, 104, P_DEC, 12, P_REL, 6, P_BASE, 40, P_SRR, 20, P_VOL, 65, P_PAN, 60);
    BEAT(p, 1, "..x...x...x...x.|..x..qx...x..qx.|..x...x...x...x.|..x..qx...x.?.x.", 60);
    SET(p, 2, SA, 90, SC, 60, SD, 70, SF, 10, P_DEC, 10, P_REL, 6, P_BASE, 30, P_DTIM, 48, P_DSND, 40, P_DFB, 50,
        P_DWID, 40, P_SPU, 50, P_VOL, 77, P_PAN, 70);
    BEAT(p, 2, "......x.........|..........x.....|......x.........|..........x...q.", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 30, P_DEC, 44, P_REL, 30, P_GATE, 6, P_WDTH, 30, P_LPQ, 0,
        P_WOF, 76, P_FDEC, 40, P_DTIM, 48, P_DSND, 100, P_DFB, 58, P_DBAS, 30, P_DWID, 28, P_SPU, 70, P_VOL, 127, P_LEVL, 122);
    BEAT(p, 3, "...x.......x....|...x.......x..h.|...x.......x....|...x...q...x..H.", 48);
    lfo(p, 3, 0, P_WDTH, 1, 0, 16, 74, 0);                                  /* its filter, over the four bars */
    lfo(p, 3, 1, P_DFB, 0, 0, 8, 67, 0);                                    /* the echo's feedback, swelling */
    SET(p, 4, SE, 0, SC, 40, SG, 0, SH, 0, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 30, P_WDTH, 24, P_VOL, 72);
    BEAT(p, 4, "x.....x...x.....|x.....x...x.....|x.....x...x.....|x.....x...x.....",
         36, 36, 31, 36, 36, 39, 36, 36, 31, 36, 43, 39);
    trim(p, 85);

    /* A05 CAROM, flavour 1, after Aphex Twin (the late 90s, "Bucephalus Bouncing Ball"): A minor at 160. The drums lead.
     * The idea is a ball: a metal FM hit bouncing through each bar, the bounces closing in (eight steps, then six,
     * four, two, then ratchets), each shorter and brighter, a new run every bar, settling at the end of the loop.
     * Around it a dense industrial beat: the kick folded and driven, ratcheting into the turn; a crushed snare with
     * ghosts round every hit, rolls into the last bar, its crack and rate changing; hats in thirty-second rolls whose
     * decay wanders; a metal rim thrown into an echo. Under it a wave-folded sine bass that boings down onto each note,
     * its fold new every hit, closed down to its growl, jumping octaves, locked with the kick, A1 to A2; no melody */
    p = pat_v5(4, "CAROM", 160, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_FMS, M_GSIN, M_GPRC));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 8; p->glob[G_GLCH] = 24; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 32;
    p->glob[G_TAPE] = 40; p->glob[G_HISS] = 40; p->glob[G_VERB] = 30; p->glob[G_DAMP] = 60; p->glob[G_ACNT] = 60;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SB, 20, SE, 64, SF, 18, SG, 50, SC, 40, P_DEC, 26, P_REL, 6, P_DIST, 84, P_SRR, 12, P_VOL, 127);
    BEAT(p, 0, "X..x..x.X..x.x..|X..x..x.X.x.x.2.|X..x..x.X..x.x..|X.x.x2x.X3..x4..", 45);
    SET(p, 1, SA, 96, SC, 40, SD, 60, SG, 60, P_DEC, 16, P_REL, 6, P_BASE, 26, P_DIST, 80, P_BITS, 40, P_SRR, 32,
        P_SPU, 30, P_VOL, 98);
    BEAT(p, 1, "....X..x....X.x.|....X..x.3..X..x|....X..x....X.x.|..x.X..x.2..X433", 60);
    static const uint8_t ghost6[] = {7, 14, 23, 31, 39, 46, 50, 55};
    for (unsigned i = 0; i < sizeof ghost6; i++) lk(p, 1, ghost6[i], P_VOL, 56);     /* the ghosts, 9 dB down */
    lk(p, 1, 28, P_DEC, 28); lk(p, 1, 44, SC, 100); lk(p, 1, 44, SD, 90);
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);
    SET(p, 2, SA, 116, SG, 80, P_ATK, 0, P_DEC, 4, P_REL, 3, P_BASE, 44, P_SRR, 44, P_BITS, 44, P_VOL, 58, P_PAN, 60);
    BEAT(p, 2, "x2xXx2xXx2xXx2x3|x2xXx3xXx2xXx4xX|x2xXx2xXx2xXx2x3|x3xXx2xXx4xXx2x4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);
    lfo(p, 2, 1, P_PAN, 0, 1, 20, 84, 0);
    for (int st = 13; st < 64; st += 16) { lk(p, 2, st, SC, 110); lk(p, 2, st, SD, 90); }
    SET(p, 3, SA, RATIO(13), SB, 70, SC, 40, SD, RATIO(17), SE, 50, SF, 16, SG, 30, SH, MODE4(0), P_DEC, 24, P_REL, 6,
        P_GATE, 2, P_DIST, 76, P_SPU, 30, P_VOL, 66, P_PAN, 50);
    BEAT(p, 3, "x.......x...x.x.|x.....x...x.x.xx|x...x..x.x.x2x3.|x.x.x2x3x4......", 57);
    static const uint8_t ball[] = {0, 8, 12, 14, 16, 22, 26, 28, 30, 31, 32, 36, 39, 41, 43, 44, 45, 46,
                                   48, 50, 52, 53, 54, 55, 56, 57};
    for (unsigned i = 0, k = 0; i < sizeof ball; i++, k++) {                /* each bounce shorter and brighter */
        if (i && ball[i] / 16 != ball[i - 1] / 16) k = 0;                   /* a new run every bar */
        lk(p, 3, ball[i], SC, (uint8_t)(k < 7 ? 40 - k * 5 : 6));
        lk(p, 3, ball[i], SB, (uint8_t)(k < 11 ? 70 + k * 5 : 127));
    }
    SET(p, 4, SB, 40, SC, 0, SE, 40, SF, 30, SG, 0, SH, 20, P_HOLD, 20, P_DEC, 50, P_REL, 10, P_GATE, 8, P_WDTH, 24,
        P_DIST, 76, P_VOL, 118);
    BEAT(p, 4, "x..x..x.x..x.x..|x..x..x.x.x.x.2.|x..x..x.x..x.x..|x.x.x2x.x3..x4..",
         33, 33, 45, 33, 36, 38, 33, 33, 45, 40, 38, 36, 31, 33, 33, 45, 33, 36, 38, 33, 45, 33, 33, 36, 38, 40, 43, 45);
    lfo(p, 4, 0, SB, 6, 3, 48, 90, 0);                                      /* its fold, new every hit */
    SET(p, 5, SA, 90, SC, 100, SD, 90, SF, 10, P_DEC, 10, P_REL, 4, P_BASE, 32, P_DIST, 76, P_BITS, 40, P_SRR, 40,
        P_DTIM, 48, P_DSND, 40, P_DFB, 40, P_DWID, 30, P_SPU, 30, P_VOL, 72, P_PAN, 40);
    BEAT(p, 5, "...x..x...x..x..|.x....x..x...x.x|...x..x...x..x..|.x..x.x.x.x.x.x.", 72);
    trim(p, 55);

    /* A07 VESPER, flavour 2, after Plaid (the late 90s): F# minor at 152, the beat in half time. The idea is an
     * intricate line: four bars from the first step, one six-note figure turned through F#m7, Dmaj7, Bm9 and C#7sus4,
     * on one of the owner's waves, crushed, its filter moving, held on the bars' first notes; the chords rootless,
     * swelling on a chorused pad, driven and worn by the tape. Under it an FM bass, its ratio falling into each note,
     * fed back, on the roots and their fifths, C#1 to C#2, locked with the kick; busy dusty drums under the half-time
     * snare: ghost rolls, hats in sixteenths whose decay wanders, rolling and ratcheting, metal ticks among them */
    p = pat_v5(6, "VESPER", 152, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_FMD, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 16; p->glob[G_TAPE] = 66; p->glob[G_HISS] = 50; p->glob[G_ADPC] = 30;
    p->glob[G_GLCH] = 10; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_VERB] = 56; p->glob[G_DAMP] = 76; p->glob[G_ACNT] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 60, SF, 22, SG, 36, SC, 40, P_DEC, 32, P_REL, 6, P_DIST, 80, P_SRR, 12, P_VOL, 127);
    BEAT(p, 0, "x.....x...x.....|x..x......x..x..|x.....x...x.....|x..x....x.x...x.", 42);
    SET(p, 1, SA, 92, SC, 30, SD, 50, SG, 50, P_DEC, 20, P_REL, 6, P_BASE, 26, P_DIST, 78, P_BITS, 34, P_SRR, 30,
        P_SPU, 40, P_VOL, 94);
    BEAT(p, 1, "........X..x...x|...x....X..x.x..|........X..x...x|...x....X..x.3.4", 60);
    static const uint8_t ghost7[] = {11, 15, 19, 27, 29, 43, 47, 51, 59};
    for (unsigned i = 0; i < sizeof ghost7; i++) lk(p, 1, ghost7[i], P_VOL, 56);
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);
    SET(p, 2, SA, 116, SG, 80, P_ATK, 0, P_DEC, 4, P_REL, 3, P_BASE, 44, P_SRR, 44, P_BITS, 40, P_VOL, 65, P_PAN, 60);
    BEAT(p, 2, "x?xXx?x2x?xXx?x3|x?xXx2x.x3xXx?x2|x?xXx?x2x?xXx?x3|x2xXx3x.x?xXx4x4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);
    lfo(p, 2, 1, P_PAN, 0, 1, 20, 84, 0);
    for (int st = 9; st < 64; st += 18) { lk(p, 2, st, SC, 110); lk(p, 2, st, SD, 90); }
    SET(p, 3, SA, PCH(4), SB, PCH(7), SC, PCH(12), SD, 40, SF, 50, SG, 30, P_ATK, 60, P_HOLD, 127, P_DEC, 127,
        P_REL, 70, P_GATE, 127, P_WDTH, 30, P_LPQ, 0, P_DIST, 72, P_SRR, 16, P_BITS, 12, P_SPU, 60, P_VOL, 109);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 45, 42, 50, 44);
    chord(p, 3, 16, 3, 7, 12);                                              /* Dmaj7, rootless: F# A C# */
    chord(p, 3, 32, 4, 7, 11);                                              /* Bm9, rootless: D F# A C# */
    chord(p, 3, 48, 3, 5, 10);                                              /* C#7sus4: G# B C# F# */
    lfo(p, 3, 0, P_WDTH, 1, 0, 24, 74, 0);
    SET(p, 4, SA, RATIO(3), SB, 60, SC, 40, SD, RATIO(6), SE, 30, SF, 30, SG, 30, SH, 100, P_HOLD, 127, P_DEC, 127,
        P_REL, 30, P_GATE, 40, P_WDTH, 34, P_DIST, 78, P_BITS, 16, P_VOL, 80);
    BEAT(p, 4, "x.....x...x.....|x..x......x..x..|x.....x...x.....|x..x....x.x...x.",
         30, 30, 37, 26, 26, 33, 26, 35, 35, 30, 25, 25, 32, 37, 32);
    lfo(p, 4, 0, SB, 1, 1, 16, 80, 0);                                      /* its depth, breathing */
    SET(p, 5, SA, OS(40), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 10, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 40, P_WDTH, 30, P_DIST, 68, P_SRR, 30, P_BITS, 24, P_DTIM, 64, P_DSND, 32,
        P_DFB, 44, P_DBAS, 10, P_DWID, 30, P_SPU, 56, P_VOL, 84, P_PAN, 64);
    BEAT(p, 5, "x..x..x.x..x..x.|x..x..x.x..x..x.|x..x..x.x..x..x.|x..x..x.x..x..x.",
         54, 57, 61, 59, 57, 52, 57, 54, 61, 57, 54, 52, 61, 57, 54, 52, 54, 57, 56, 59, 61, 59, 56, 61);
    for (int st = 0; st < 64; st += 16) lk(p, 5, st, P_GATE, 100);
    lfo(p, 5, 0, P_WDTH, 1, 1, 20, 76, 0);
    trim(p, 67);

    /* A14 MOTEL, lo-fi house after Anthony Naples: B dorian at 122, loose and swung. A few things: a kick tuned to B, a
     * clap in a small room, open hats sampled down; a muffled, crushed stab, Bm7 then E7sus4, off the beat into a dull
     * echo; and the idea, a bass that walks the chords; tape, hiss and the bus's crunch for the dust */
    p = pat_v5(13, "MOTEL", 122, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_GSIN, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 22; p->glob[G_TAPE] = 24; p->glob[G_HISS] = 44; p->glob[G_ADPC] = 24;
    p->glob[G_DRIV] = 28; p->glob[G_VERB] = 50; p->glob[G_DAMP] = 78;
    SET(p, 0, SE, 56, SF, 26, SG, 20, SC, 30, P_DEC, 34, P_REL, 10, P_DIST, 70, P_SRR, 10, P_VOL, 127);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x.q.", 35);
    SET(p, 1, SA, 72, SC, 30, SD, 70, SG, 20, P_DEC, 32, P_REL, 14, P_BASE, 24, P_BITS, 24, P_SPU, 70, P_VOL, 106);
    BEAT(p, 1, "....x.......x...|....x.......x...|....x.......x...|....x.......x..q", 60);
    SET(p, 2, SA, 106, SG, 30, P_DEC, 22, P_REL, 10, P_BASE, 38, P_SRR, 30, P_VOL, 73);
    BEAT(p, 2, "..x...x...x...x.|..x...x...x...x.|..x...x...x...x.|..x...x...x.?.x.", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 30, P_DEC, 50, P_REL, 30, P_GATE, 10, P_WDTH, 26, P_LPQ, 0,
        P_BITS, 30, P_SRR, 30, P_DTIM, 64, P_DSND, 34, P_DFB, 50, P_DWID, 30, P_SPU, 50, P_VOL, 124, P_LEVL, 110);
    BEAT(p, 3, ".......x......x.|...x......x.....|.......x......x.|...x......x.....", 47, 47, 47, 47, 52, 52, 52, 52);
    chords(p, 3, 32, 64, 5, 7, 10);                                         /* E7sus4 */
    SET(p, 4, SE, 0, SC, 40, SG, 0, SH, 16, P_HOLD, 127, P_DEC, 127, P_REL, 20, P_GATE, 16, P_WDTH, 32, P_DIST, 70,
        P_VOL, 70);
    BEAT(p, 4, "x..x..x...x..x..|x..x..x...x..x..|x..x..x...x..x..|x..x..x...x..x..",
         35, 35, 42, 35, 38, 35, 35, 42, 40, 38, 40, 40, 47, 40, 45, 40, 40, 47, 45, 42);
    trim(p, 75);

    /* A01 MAMMOTH, direction v5 (Lazar, Fell, saintmatan, the Monomachine's metal demos): G minor at 164. Kick, hats
     * (and the snare's noise), the pad, the DROP bass, the FM (the snare's clang and an elephant's calls), FX-CHORUS */
    p = pat_v5(0, "MAMMOTH", 164, MACHINES(M_GSIN, M_GNOI, M_ENS, M_GSIN, M_FMP, M_FXC));
    mm_metal11(p, 62);                                                       /* the comb at 270 samples, ~163 Hz */
    trim(p, 45);
    p->kit[3][P_LEVL] = (uint8_t)CLAMP(p->kit[3][P_LEVL] * 110 / 100, 0, 127);
    p->kit[2][P_LEVL] = (uint8_t)CLAMP(p->kit[2][P_LEVL] * 248 / 100, 0, 127);
    p->kit[5][P_LEVL] = (uint8_t)CLAMP(p->kit[5][P_LEVL] * 140 / 100, 0, 127);

    /* A11 RIVETS, flavour 1, after Autechre: C# minor at 118, swung. The drums lead, a broken funk with space in it: a
     * hard kick on one and the off-beats, ratcheting into the turn; a short crushed snare on the backbeat with ghost
     * notes round it and a drag before the turn; hats in swung sixteenths, accented on the off-beat eighths, opening
     * every second bar; an FM clank answering the snare, its ratio locked to a brighter strike; a metal rim thrown
     * into a dotted echo. Under it a punchy SWAV-SAW bass in 3 3 4 3 3, on the kick and between, jumping octaves,
     * played an octave over its root so its sub sits on C#1 to C#2 (35-70 Hz); no melody */
    p = pat_v5(10, "RIVETS", 118, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_FMD, M_SAW, M_GPRC));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 24; p->glob[G_GLCH] = 20; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 24;
    p->glob[G_VERB] = 30; p->glob[G_DAMP] = 50; p->glob[G_ACNT] = 50; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SB, 16, SE, 64, SF, 18, SG, 60, SC, 50, P_DEC, 30, P_REL, 6, P_DIST, 80, P_VOL, 111);
    BEAT(p, 0, "X.....x...x.....|X.....x.x.....x.|X.....x...x..x..|X.....x.x...2.x.", 37);
    SET(p, 1, SA, 108, SG, 60, P_DEC, 16, P_REL, 6, P_BASE, 30, P_DIST, 76, P_BITS, 40, P_SRR, 36, P_VOL, 98);
    BEAT(p, 1, "....X..x....X...|....X.....x.X..x|....X..x....X.?.|....X..x..x.X.3.", 60);
    static const uint8_t ghost[] = {7, 26, 31, 39, 46, 55, 58};
    for (unsigned i = 0; i < sizeof ghost; i++) lk(p, 1, ghost[i], P_VOL, 56);   /* the ghosts, 9 dB down */
    SET(p, 2, SA, 116, SG, 90, P_DEC, 6, P_REL, 4, P_BASE, 44, P_BITS, 50, P_SRR, 50, P_VOL, 74, P_PAN, 70);
    BEAT(p, 2, "x?X.x?X.x?X.x?Xx|x?X.x?X.x?X.x?X2|x?X.x?X.x?X.x?Xx|x?X.x?X.x2X.x?X4", 60);
    lk(p, 2, 30, P_DEC, 30); lk(p, 2, 62, P_DEC, 30);                       /* open, every second bar */
    SET(p, 3, SA, RATIO(13), SB, 80, SC, 16, SD, RATIO(26), SE, 40, SF, 10, SG, 40, SH, 100, P_DEC, 14, P_REL, 4,
        P_GATE, 2, P_DIST, 76, P_SPU, 20, P_VOL, 65, P_PAN, 44);
    BEAT(p, 3, ".......x.......x|...x.......x..2.|.......x.......x|...x...x...3..x.", 49);
    lk(p, 3, 7, SA, RATIO(17)); lk(p, 3, 39, SA, RATIO(17));               /* the brighter strike */
    lfo(p, 3, 0, SB, 6, 2, 48, 80, 0);
    SET(p, 4, SA, 20, SB, 24, SE, 60, SF, 110, SG, 0, SH, 64, P_ATK, 0, P_HOLD, 8, P_DEC, 44, P_REL, 14, P_GATE, 4,
        P_WDTH, 22, P_LPQ, 0, P_FDEC, 30, P_WOF, 92, P_DIST, 76, P_VOL, 127, P_LEVL, 127);
    BEAT(p, 4, "x..x..x...x..x..|x..x..x...x...x.|x..x..x...x..x..|x..x..x.x.x..x.x",
         37, 49, 37, 40, 42, 37, 49, 37, 44, 42, 37, 49, 37, 40, 47, 37, 49, 37, 40, 42, 44, 47);
    SET(p, 5, SA, 90, SC, 100, SD, 90, SF, 10, P_DEC, 10, P_REL, 4, P_BASE, 32, P_DIST, 76, P_BITS, 40, P_SRR, 40,
        P_DTIM, 48, P_DSND, 40, P_DFB, 40, P_DWID, 30, P_SPU, 30, P_VOL, 79, P_PAN, 40);
    BEAT(p, 5, "..........x.....|...x..........x.|..........x.....|...x......x...x.", 72);
    trim(p, 84);

    /* A12 SYRUP: warm IDM in A, worn tape: a detuned pad through Amaj7 F#m9 Dmaj7 E6/9, a nostalgic bell melody
     * in echoes, a soft sine bass; lazy drums, a bitten snare; the tape wobbles */
    p = pat_v5(11, "SYRUP", 96, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_FMS, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_TAPE] = 50; p->glob[G_HISS] = 14; p->glob[G_VERB] = 70; p->glob[G_ADPC] = 10;
    p->glob[G_SWNG] = 24; p->glob[G_DAMP] = 60;
    SET(p, 0, SE, 60, SF, 34, SG, 20, SC, 30, P_DEC, 40, P_REL, 10, P_VOL, 127);
    BEAT(p, 0, "x.......x.x.....|x......x..x.....|x.......x.x.....|x......x..x..q..", 36);
    SET(p, 1, SA, 66, SC, 30, P_DEC, 30, P_REL, 14, P_BASE, 20, P_BITS, 20, P_SPU, 60, P_VOL, 127, P_LEVL, 103);
    BEAT(p, 1, "....x.......x...|....x.......x..q|....x.......x...|....x..q....x...", 60);
    SET(p, 2, SA, 96, P_DEC, 18, P_REL, 8, P_BASE, 32, P_VOL, 64, P_PAN, 58);
    BEAT(p, 2, "x.x.x.xqx.x.x.xq|x.x.x.xqx.x.x.x.|x.x.x.xqx.x.x.xq|x.x.xqxqx.x.x.x.", 60);
    SET(p, 3, SA, SEMI(4), SB, SEMI(7), SC, SEMI(11), SD, 70, SE, 20, SG, 100, SH, 20, P_ATK, 74, P_HOLD, 127,
        P_DEC, 127, P_REL, 90, P_GATE, 127, P_WDTH, 34, P_LPQ, 16, P_SPU, 80, P_VOL, 66);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 57, 54, 50, 52);
    lk(p, 3, 16, SA, SEMI(3)); lk(p, 3, 16, SB, SEMI(10)); lk(p, 3, 16, SC, SEMI(14));   /* F#m9 */
    lk(p, 3, 48, SB, SEMI(9)); lk(p, 3, 48, SC, SEMI(14));                                /* E6/9 */
    MOD(p, 3, 0, 14, 100, 70);
    SET(p, 4, SA, RATIO(10), SB, 40, SC, 60, SD, RATIO(8), SE, 24, SF, 40, SH, MODE4(0), P_DEC, 76, P_REL, 60,
        P_GATE, 20, P_DTIM, 64, P_DSND, 40, P_DFB, 40, P_SPU, 70, P_VOL, 51, P_PAN, 70);
    BEAT(p, 4, "x.....x...x.....|x.......x.......|x.....x...x.....|x...x...x.......", 76, 73, 71, 69, 66, 76, 78, 81, 80, 76, 71);
    SET(p, 5, SB, 6, SC, 20, SH, 20, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 40, P_WDTH, 26, P_VOL, 64);
    BEAT(p, 5, "x.....x...x.....|x.....x...x.....|x.....x...x.....|x.....x...x...x.", 45, 45, 52, 42, 42, 49, 38, 38, 45, 40, 40, 47, 44);
    trim(p, 74);

    /* A02 CUTUP, flavour 1, after Prefuse 73 (2001): D minor at 156, the beat in half time, swung hard. The drums lead, a
     * hip-hop beat cut up: a fat kick and a crushed snare stuttering into ratchets and rolls, hats chopped in
     * thirty-seconds; VO-6 cut to short vowel chops that stutter, a new vowel each; a dusty chord stab chopped off the
     * beat, Dm9 then Bbmaj7 over D. Under it an FM bass that zaps down onto each note, fed back and crushed, locked
     * with the kick, D1 to D2; the bus repeating slices; no melody */
    p = pat_v5(1, "CUTUP", 156, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_VOX, M_FMD, M_ENS));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 24; p->glob[G_GLCH] = 30; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 36;
    p->glob[G_TAPE] = 50; p->glob[G_HISS] = 50; p->glob[G_VERB] = 34; p->glob[G_DAMP] = 70; p->glob[G_ACNT] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SB, 12, SE, 60, SF, 26, SG, 30, SC, 50, P_DEC, 36, P_REL, 8, P_DIST, 82, P_SRR, 16, P_VOL, 127);
    BEAT(p, 0, "X.....x...x....x|..x.......x.2...|X.....x...x....x|..x...x.x.3.4...", 38);
    SET(p, 1, SA, 84, SC, 30, SD, 50, SG, 60, P_DEC, 20, P_REL, 6, P_BASE, 24, P_DIST, 80, P_BITS, 40, P_SRR, 36,
        P_SPU, 30, P_VOL, 93);
    BEAT(p, 1, "........X.....x.|...x....X..x..2.|........X.....x.|...x....X.3.x4.4", 60);
    static const uint8_t ghost12[] = {14, 19, 27, 46, 51, 60};
    for (unsigned i = 0; i < sizeof ghost12; i++) lk(p, 1, ghost12[i], P_VOL, 56);
    lk(p, 1, 30, P_SRR, 80); lk(p, 1, 58, P_PTCH, 70);
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);
    SET(p, 2, SA, 112, SG, 80, P_ATK, 0, P_DEC, 5, P_REL, 3, P_BASE, 42, P_SRR, 44, P_BITS, 44, P_VOL, 68, P_PAN, 64);
    BEAT(p, 2, "x2x.x3xxx2x.x3x.|x2x.x3x2x2x.x4x.|x2x.x3xxx2x.x3x.|x2xxx3x.x2x4x4x4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);
    lfo(p, 2, 1, P_PAN, 0, 1, 24, 90, 0);
    SET(p, 3, SA, VOWEL(0), SB, 70, SC, 30, SD, 64, SE, 20, SF, 70, SG, 40, SH, 0, P_ATK, 0, P_DEC, 30, P_REL, 10,
        P_GATE, 4, P_WDTH, 40, P_DIST, 74, P_BITS, 30, P_SRR, 30, P_DTIM, 32, P_DSND, 40, P_DFB, 30, P_SPU, 40,
        P_VOL, 123, P_PAN, 56);
    BEAT(p, 3, "......x2......4.|..x.......3...2.|......x2......4.|..x...2...3.4.3.",
         57, 50, 53, 60, 57, 53, 50, 53, 57, 53, 50, 57, 50, 53);
    static const uint8_t chop[] = {6, 7, 14, 18, 26, 30, 38, 39, 46, 50, 54, 58, 60, 62};
    for (unsigned i = 0; i < sizeof chop; i++) lk(p, 3, chop[i], SA, VOWEL((i * 3) % 8));   /* a new vowel each */
    SET(p, 4, SA, RATIO(3), SB, 70, SC, 30, SD, RATIO(9), SE, 40, SF, 24, SG, 50, SH, 110, P_HOLD, 20, P_DEC, 60,
        P_REL, 14, P_GATE, 10, P_WDTH, 36, P_DIST, 80, P_BITS, 30, P_XOR, 10, P_VOL, 85);
    BEAT(p, 4, "x.....x...x....x|..x.......x.2...|x.....x...x....x|..x...x.x.3.4...",
         26, 26, 29, 31, 33, 34, 33, 34, 34, 38, 36, 34, 38, 36, 33, 31);
    SET(p, 5, SA, PCH(4), SB, PCH(7), SC, PCH(11), SD, 40, SF, 40, SG, 30, P_ATK, 0, P_HOLD, 10, P_DEC, 40, P_REL, 20,
        P_GATE, 6, P_WDTH, 30, P_LPQ, 0, P_DIST, 74, P_BITS, 30, P_SRR, 30, P_DTIM, 48, P_DSND, 34, P_DFB, 40,
        P_SPU, 50, P_VOL, 126, P_LEVL, 108, P_PAN, 70);
    BEAT(p, 5, ".......x.x......|...x.......2....|.......x.x......|...x...3........", 53, 53, 53, 53, 50, 50, 50, 50);
    chords(p, 5, 32, 64, 3, 7, 12);                                         /* Bbmaj7 over D: D F A */
    trim(p, 61);

    /* A04 GRISTLE, flavour 1, after Clark (Warp, the 2000s), with SOPHIE's metal: E minor at 168, a slow industrial
     * groove in half time with everything fast on top. The skeleton holds, swung: a hard punchy kick with a click,
     * folded and driven, locked with the bass; a metal crack of a snare on the half-time beat, ghost rolls round it.
     * Over it, busy and aggressive: hats on nearly every step, ratcheting, their decay wandering; FM metal boings, their
     * ratio zapping down hard, a new sweep each hit; pitch-drop zaps answering the snare. Under it a rubbery FM bass,
     * its ratio rising into each note, fed back and crushed, sliding between E1 and E2 and through the minor; no melody */
    p = pat_v5(3, "GRISTLE", 168, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_FMD, M_FMD, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 18; p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 36;
    p->glob[G_TAPE] = 36; p->glob[G_HISS] = 40; p->glob[G_VERB] = 34; p->glob[G_DAMP] = 60; p->glob[G_ACNT] = 70;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SB, 28, SE, 64, SF, 18, SG, 70, SC, 50, P_DEC, 32, P_REL, 6, P_DIST, 88, P_SRR, 12, P_VOL, 127);
    BEAT(p, 0, "X.....x...x..x..|x.x.....x.x...2.|X.....x...x..x..|x.x...x.x.3.x.4.", 40);
    SET(p, 1, SA, 90, SC, 100, SD, 90, SG, 70, P_DEC, 26, P_REL, 8, P_BASE, 22, P_DIST, 86, P_BITS, 40, P_SRR, 30,
        P_SPU, 36, P_VOL, 94);
    BEAT(p, 1, "........X..x..x.|...x....X..x.x.x|........X..x..x.|...x....X.3.x4.4", 60);
    static const uint8_t ghost13[] = {11, 14, 19, 27, 29, 31, 43, 46, 51, 60};
    for (unsigned i = 0; i < sizeof ghost13; i++) lk(p, 1, ghost13[i], P_VOL, 52);   /* the ghosts, 9 dB down */
    lk(p, 1, 56, P_DEC, 40);
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);
    SET(p, 2, SA, 114, SG, 80, P_ATK, 0, P_DEC, 4, P_REL, 3, P_BASE, 44, P_SRR, 44, P_BITS, 44, P_VOL, 62, P_PAN, 60);
    BEAT(p, 2, "x2X2x3X2x2X2x3X4|x2X2x3X2x2X3x2X4|x2X2x3X2x2X2x3X4|x3X2x4X2x2X3x4X4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);
    lfo(p, 2, 1, P_PAN, 0, 1, 20, 84, 0);
    SET(p, 3, SA, RATIO(17), SB, 100, SC, 20, SD, RATIO(26), SE, 60, SF, 10, SG, 60, SH, 120, P_DEC, 14, P_REL, 4,
        P_GATE, 2, P_DIST, 82, P_BITS, 24, P_SPU, 30, P_VOL, 55, P_PAN, 40);
    BEAT(p, 3, ".x..x.3..x.x.2..|x..2.x...x3..x.x|.x..x.3..x.x.2..|x.2..x.3.x.4.x.4", 52, 59, 55, 59, 52, 55);
    lfo(p, 3, 0, SH, 6, 3, 48, 100, 0);                                     /* the zap, new each hit */
    lfo(p, 3, 1, P_PAN, 6, 3, 40, 100, 0);
    SET(p, 4, SA, RATIO(3), SB, 80, SC, 36, SD, RATIO(6), SE, 40, SF, 30, SG, 40, SH, 30, P_HOLD, 127, P_DEC, 127,
        P_REL, 20, P_GATE, 30, P_WDTH, 34, P_DIST, 82, P_BITS, 16, P_VOL, 111);
    BEAT(p, 4, "X.....s...x..x..|x.x.....s.x...2.|X.....s...x..x..|x.x...s.x.3.x.4.",
         28, 40, 28, 31, 28, 31, 40, 35, 33, 28, 40, 28, 33, 28, 40, 28, 31, 33, 35, 38);
    lfo(p, 4, 0, SB, 1, 1, 16, 80, 0);                                      /* its depth, breathing */
    SET(p, 5, SE, 110, SF, 20, SG, 60, SC, 20, P_DEC, 16, P_REL, 4, P_DIST, 84, P_BITS, 30, P_SRR, 20, P_SPU, 30,
        P_VOL, 74, P_PAN, 80);
    BEAT(p, 5, "......x.......x.|..x.......x.x...|......x.......x.|..x...x...x.x.x.", 52, 47, 52, 47, 52, 47, 52);
    trim(p, 56);

    /* A08 ORCHARD, flavour 2, after Boards of Canada: E dorian at 92, bouncing. The idea is a four-note call, G A B
     * and down to E, answered in the last bar by the dorian C#, on one of the owner's warm waves, worn and wobbling, its
     * echo answering it; under it Em7 over G for two bars, A7, Em over B on a chorused pad; a deep sustained sine bass, its
     * sub an octave under on E1, A1 and B1, driven for its harmonics and locked with the kick; a slow swung beat with a
     * ghost before the snare */
    p = pat_v5(7, "ORCHARD", 92, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_GSIN, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 24; p->glob[G_TAPE] = 76; p->glob[G_HISS] = 52; p->glob[G_ADPC] = 20;
    p->glob[G_VERB] = 70; p->glob[G_DAMP] = 80; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 56, SF, 26, SG, 20, SC, 40, P_DEC, 40, P_REL, 10, P_DIST, 72, P_VOL, 120);
    BEAT(p, 0, "x......x.x......|x.......x.....x.|x......x.x......|x.......x...q...", 40);
    SET(p, 1, SA, 60, SC, 30, SD, 40, SE, 20, SF, 20, SG, 20, P_DEC, 32, P_REL, 14, P_BASE, 18, P_SRR, 30, P_BITS, 24,
        P_DIST, 72, P_SPU, 50, P_VOL, 113);
    BEAT(p, 1, "...qx.......x...|...qx.......x..q|...qx.......x...|...qx.......x...", 60);
    SET(p, 2, SA, 100, P_ATK, 10, P_DEC, 12, P_REL, 8, P_BASE, 36, P_SRR, 40, P_BITS, 30, P_VOL, 59, P_PAN, 60);
    BEAT(p, 2, "x.x?x.x.x.x?x.x.|x.x?x.x.x.x?x.x?|x.x?x.x.x.x?x.x.|x.x?x.x.x.x?xqx.", 60);
    SET(p, 3, SA, PCH(4), SB, PCH(7), SC, PCH(9), SD, 30, SF, 60, SG, 30, P_ATK, 80, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 80, P_VOL, 109);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 43, 43, 45, 47);
    chords(p, 3, 32, 48, 4, 7, 10);                                         /* A7 */
    chords(p, 3, 48, 64, 3, 5, 8);                                          /* Em over B */
    SET(p, 4, SE, 0, SC, 64, SG, 0, SH, 10, P_HOLD, 127, P_DEC, 127, P_REL, 40, P_GATE, 60, P_WDTH, 24, P_DIST, 72,
        P_VOL, 106);
    BEAT(p, 4, "x......x........|x.......x.......|x......x........|x.......x.....x.", 40, 40, 40, 40, 45, 45, 47, 47, 43);
    SET(p, 5, SA, OS(40), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 14, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 40, P_WDTH, 28, P_DIST, 64, P_SRR, 24, P_DTIM, 96, P_DSND, 32, P_DFB, 50,
        P_DBAS, 10, P_DWID, 30, P_SPU, 70, P_VOL, 89, P_PAN, 64);
    BEAT(p, 5, "x.x.x.....x.....|x.x.x.....x.....|x.x.x.....x.....|x.x.x.....x.....",
         55, 57, 59, 52, 55, 57, 59, 52, 55, 57, 59, 52, 55, 57, 61, 59);
    trim(p, 62);

    /* A09 SPLINTR, flavour 1, after Aphex Twin (drill'n'bass): F minor at 160. The drums lead: a kick and a snare
     * ratcheting and rolling, the snare's tonal body ringing in its low-pass three octaves up and pitched an octave on
     * some retrigs, crushed on others; hats in thirty-seconds ground to 8 bits; a metal rim flicking between them; and
     * every second loop the drums drop out of the last two beats. Under it a huge sustained SWAV-SAW bass with a little
     * unison (its detune the point), played an octave over its root so its subs sit on F1, Ab1, Db1 and Eb1; once a
     * loop, over the F, a three-note dark hook, Ab G F */
    p = pat_v5(8, "SPLINTR", 160, MACHINES(M_GSIN, M_GSIN, M_GPRC, M_GPRC, M_SAW, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 0; p->glob[G_GLCH] = 32; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 24;
    p->glob[G_VERB] = 40; p->glob[G_DAMP] = 50; p->glob[G_ACNT] = 60; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SB, 20, SE, 64, SF, 16, SG, 70, SC, 60, P_DEC, 32, P_REL, 6, P_DIST, 88, P_SRR, 24, P_VOL, 127);
    BEAT(p, 0, "X..x..2...x..x..|X.x...x...3...x.|X..x..2...x..x..|X...x.x...h.h.h.", 41);
    SET(p, 1, SB, 20, SE, 30, SF, 14, SD, 110, SG, 40, SC, 0, P_DEC, 24, P_REL, 8, P_WDTH, 40, P_LPQ, 50, P_DIST, 86,
        P_BITS, 30, P_SRR, 30, P_SPU, 30, P_VOL, 100);
    BEAT(p, 1, "....X..x.2..X..3|..x.X...x.3.X.4.|....X..x.2..X..3|..2.X.x.h.h.hhhh", 60);
    lk(p, 1, 15, P_PTCH, 76); lk(p, 1, 30, P_PTCH, 76); lk(p, 1, 47, P_SRR, 80); lk(p, 1, 41, P_PTCH, 52);
    lk(p, 1, 9, P_SRR, 70);
    SET(p, 2, SA, 116, SG, 80, P_DEC, 6, P_REL, 4, P_BASE, 46, P_BITS, 50, P_SRR, 40, P_VOL, 85, P_PAN, 70);
    BEAT(p, 2, "x2x2x3x2x2x3x2x4|x2x3x2x2x3x2x2x2|x2x2x3x2x2x3x2x4|x2x3x2x2hhhhhhhh", 60);
    SET(p, 3, SA, 90, SC, 100, SD, 90, SF, 10, P_DEC, 10, P_REL, 4, P_BASE, 30, P_DIST, 76, P_SPU, 30, P_VOL, 65,
        P_PAN, 40);
    BEAT(p, 3, ".x....x..x...x..|...x..x.....x..x|.x....x..x...x..|...x..x.....hh..", 72);
    SET(p, 4, SA, 60, SB, 20, SC, 40, SE, 60, SF, 100, SG, 0, SH, 64, P_ATK, 2, P_HOLD, 127, P_DEC, 127, P_REL, 30,
        P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_DIST, 75, P_VOL, 127);
    BEAT(p, 4, "x...............|x.......x.......|x...............|x.......x.......", 41, 41, 44, 41, 37, 39);
    SET(p, 5, SA, OS(62), SB, 64, SC, 127, SE, 40, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 10, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 48, P_WDTH, 28, P_DIST, 69, P_SRR, 30, P_DTIM, 48, P_DSND, 34, P_DFB, 50,
        P_DWID, 30, P_SPU, 50, P_VOL, 65);
    BEAT(p, 5, "................|................|..x...x...x.....|................", 56, 55, 53);
    trim(p, 52);
    p->kit[4][P_LEVL] = 97;               /* the bass over the trim: SWAV-SAW clips 10.5 dB under GND-SIN */

    /* A15 FOUNDRY, flavour 2, after Boards of Canada: the progression of "Kaini Industries", from the owner's MIDI,
     * its bass notes too (B, C#, G#, C#); the rest is MONO2K's own. G# dorian at 102. The idea is a lead bass: a deep,
     * gritty SWAV-SAW running a four-bar line from the first step over the original's four bars (G#m7 over B,
     * C#sus4, G#m9, C#9), one rhythm a bar, sliding into some notes, each bar on the progression's bass note, played an
     * octave over its root so its sub sits on C#1 to D#2, its filter moving; the chords rootless, swelling on a chorused
     * pad, driven, crushed and worn by the tape. The drums never quite repeat: a hard kick; a crushed snare whose length
     * and crack change, ghosts round it, its rate drifting; short hats in swung sixteenths whose decay wanders, metal
     * ticks and pitched blips among them; and a gritty shaker and metal percussion thrown into a dotted echo */
    p = pat_v5(14, "FOUNDRY", 102, MACHINES(M_GSIN, M_GPRC, M_GPRC, M_ENS, M_SAW, M_GPRC));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 22; p->glob[G_TAPE] = 76; p->glob[G_HISS] = 60; p->glob[G_ADPC] = 40;
    p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_VERB] = 56; p->glob[G_DAMP] = 76; p->glob[G_ACNT] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 64, SF, 20, SG, 40, SC, 40, P_DEC, 32, P_REL, 6, P_DIST, 82, P_SRR, 12, P_VOL, 113);
    BEAT(p, 0, "X.....x...x.....|x.........X.x...|x.....x...x.....|x.........X.....", 44);
    SET(p, 1, SA, 92, SC, 40, SD, 60, SG, 50, P_DEC, 18, P_REL, 6, P_BASE, 24, P_DIST, 78, P_BITS, 30, P_SRR, 30,
        P_SPU, 40, P_VOL, 90);
    BEAT(p, 1, "....X..x....X...|....X.......X..x|....X..x....X...|....X..x....X.x.", 60);
    static const uint8_t ghost16[] = {7, 31, 39, 55, 62};
    for (unsigned i = 0; i < sizeof ghost16; i++) lk(p, 1, ghost16[i], P_VOL, 50);   /* the ghosts, 9 dB down */
    lk(p, 1, 28, P_DEC, 30); lk(p, 1, 60, P_DEC, 30);
    lk(p, 1, 44, SC, 100); lk(p, 1, 44, SD, 90);
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);
    SET(p, 2, SA, 116, SG, 80, P_ATK, 0, P_DEC, 4, P_REL, 3, P_BASE, 44, P_SRR, 44, P_BITS, 44, P_VOL, 57, P_PAN, 58);
    BEAT(p, 2, "x?X.x?X.x?X.x?X2|x?X.x?X.x?X3x?X.|x?X.x?X.x?X.x?X2|x?X.x?X.x2X.x?X4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);
    lfo(p, 2, 1, P_PAN, 0, 1, 20, 84, 0);
    for (int st = 9; st < 64; st += 18) { lk(p, 2, st, SC, 110); lk(p, 2, st, SD, 90); }
    lk(p, 2, 19, SB, 70); lk(p, 2, 51, SB, 76);
    SET(p, 3, SA, PCH(4), SB, PCH(7), SC, PCH(9), SD, 40, SF, 50, SG, 30, P_ATK, 60, P_HOLD, 127, P_DEC, 127,
        P_REL, 70, P_GATE, 127, P_WDTH, 30, P_LPQ, 0, P_DIST, 74, P_SRR, 20, P_BITS, 16, P_SPU, 60, P_VOL, 103);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 47, 49, 51, 53);
    chord(p, 3, 16, 5, 7, 10);                                              /* C#sus4 (7) */
    chord(p, 3, 32, 3, 5, 7);                                               /* G#m9, no third: D# F# G# A# */
    chord(p, 3, 48, 3, 6, 10);                                              /* C#9, rootless: F G# B D# */
    lfo(p, 3, 0, P_WDTH, 1, 0, 24, 74, 0);
    SET(p, 4, SA, 40, SB, 16, SE, 40, SF, 110, SG, 0, SH, 64, P_ATK, 0, P_HOLD, 127, P_DEC, 127, P_REL, 20,
        P_GATE, 30, P_WDTH, 34, P_LPQ, 0, P_DIST, 78, P_VOL, 127, P_LEVL, 127);
    BEAT(p, 4, "x..x..x.x...x.x.|x..x..x.s...x.x.|x..x..x.x...x.x.|x..x..x.s...x.x.",
         47, 44, 51, 49, 47, 42, 37, 44, 47, 49, 44, 42, 44, 51, 46, 49, 51, 44, 37, 41, 44, 47, 51, 49);
    for (int st = 0; st < 64; st += 16) lk(p, 4, st, P_GATE, 90);           /* each bar's bass note held */
    lfo(p, 4, 0, P_WDTH, 1, 1, 16, 74, 0);
    SET(p, 5, SA, 110, SC, 60, SD, 70, SE, 30, SF, 10, SG, 70, P_DEC, 8, P_REL, 4, P_BASE, 40, P_BITS, 40, P_SRR, 40,
        P_DTIM, 48, P_DSND, 36, P_DFB, 36, P_DWID, 30, P_SPU, 30, P_VOL, 86, P_PAN, 40);
    BEAT(p, 5, "..x..x?...x.x..x|.?x..x...xx...x.|..x..x?...x.x..x|.?x..x..x.x.2.x.", 72);
    lfo(p, 5, 0, SC, 6, 3, 48, 90, 0);                                      /* shaker to metal, hit to hit */
    lfo(p, 5, 1, P_PAN, 0, 1, 14, 90, 0);
    trim(p, 74);

    /* A12 SYRUP, still the bank before (ea69d53), was written on SWAV-ENS's old page and the bus's old laws: onto
     * today's, once (every other part is written on today's) */
    static const uint8_t OLD_SLOTS[] = {11};
    for (unsigned i = 0; i < sizeof OLD_SLOTS; i++) {
        ens_v6(PAT(OLD_SLOTS[i]));
        bus_v9(PAT(OLD_SLOTS[i]));
    }
    /* the parts with an FM+ track, written on MONO2K's old FM pages: onto the Monomachine's, once, until each is
     * voiced on them */
    static const uint8_t FM_OLD[] = {1, 3, 4, 6, 10, 11, 15};   /* CUTUP GRISTLE CAROM VESPER RIVETS SYRUP SPLICE */
    for (unsigned i = 0; i < sizeof FM_OLD; i++) fm_v9(PAT(FM_OLD[i]));
    /* and every part, its GND-SIN tracks written on that machine's old page (TUNE first): onto the Monomachine's */
    for (int i = 0; i < 16; i++) gsin_v11(PAT(i));
    /* and its HOLDs, written in 14.6 ms steps (the times in the comments): in beats at the part's tempo, once */
    for (int i = 0; i < 16; i++) hold_v12(PAT(i));
    /* and its VOX-6 tracks (A02 CUTUP's), written on MONO2K's old page: onto VO-6's, once */
    for (int i = 0; i < 16; i++) vox_v13(PAT(i));
    /* and every part's amp and LFOs, on format 15's laws: onto 16's, once */
    for (int i = 0; i < 16; i++) feel_v15(PAT(i));
    /* and its bus (the reverb, the sends, the glitch), on format 16's: onto 17's, once */
    for (int i = 0; i < 16; i++) fx_v16(PAT(i));
}
