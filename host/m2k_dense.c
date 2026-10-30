/* SPDX-License-Identifier: GPL-3.0-only */
/* DENSE: a pattern as heavy as the owner's 1.6 overload (every track with effects, LFOs, chances and locks), a
 * fixture for the self-test (two cores, sample for sample) and the yardstick of the render's cost (m2k_draw --cpu):
 * six machines (the drums, a wide SWAV-SAW, FM+DYN, DIGI-WAV), each with its EQ, DIST, SRR, BITS, the fold and the
 * XOR, the MOD effect (chorus, flanger, phaser in turn), its delay (ping-pong on T2 T4 T6, one line on the others), a
 * reverb send, the filter and its envelope; all three LFOs moving the filter, the pan and the EQ; trigs on most steps,
 * a quarter of them on a chance or an A:B, ratchets, and 4 locks on 8 steps of every track (192). Included after
 * m2k_core.c */
static void dense_pattern(m2k_pattern_t *q)
{
    static const uint8_t M[M2K_TRACKS] = {M_DKIK, M_DSNR, M_DHAT, M_SAW, M_FMD, M_WAV};
    static const uint8_t ROOT[M2K_TRACKS] = {36, 38, 42, 45, 33, 57};
    static const uint8_t COND[4] = {C_P50, C_1_2, C_P75, C_2_2};
    pat_init(q, "DENSE");
    int nl = 0;
    for (int t = 0; t < M2K_TRACKS; t++) {
        kit_init(q, t, M[t]);
        uint8_t *k = q->kit[t];
        k[P_EQF] = (uint8_t)(60 + 8 * t); k[P_EQG] = 96; k[P_DIST] = 88; k[P_SRR] = 24; k[P_BITS] = 40;
        k[P_FOLD] = 24; k[P_XOR] = 16;
        k[P_MODT] = enc(t % 3, 3); k[P_MODR] = 50; k[P_MODD] = 80; k[P_MODM] = 64;
        k[P_DTIM] = (uint8_t)(40 + 4 * t); k[P_DSND] = t & 1 ? 100 : 28; k[P_DFB] = 70; k[P_DWID] = t & 1 ? 127 : 40;
        k[P_SPU] = 70;
        k[P_BASE] = 20; k[P_WDTH] = 90; k[P_HPQ] = 30; k[P_LPQ] = 60; k[P_FATK] = 10; k[P_FDEC] = 50; k[P_BOF] = 80;
        k[P_DEC] = 70; k[P_REL] = 60;
        if (M[t] == M_SAW) { k[P_SYN] = 110; k[P_SYN + 1] = 90; k[P_SYN + 2] = 100; k[P_SYN + 3] = 40; k[P_SYN + 4] = 60; }
        if (M[t] == M_FMD) { k[P_SYN + 2] = 90; k[P_SYN + 6] = 70; }
        if (M[t] == M_WAV) { k[P_SYN + 3] = 60; k[P_SYN + 6] = 80; k[P_SYN + 5] = 70; }
        static const uint8_t DEST[3] = {P_BASE, P_PAN, P_EQF}, WAVE[3] = {1, 0, 6}, SPD[3] = {60, 40, 52}, DP[3] = {104, 96, 88};
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &k[P_LFO + 8 * l];
            b[L_DEST] = (uint8_t)(DEST[l] + 1);
            b[L_WAVE] = enc(WAVE[l], 7);
            b[L_SPD] = (uint8_t)(SPD[l] + 4 * t);
            b[L_DPTH] = DP[l];
        }
        for (int s = 0; s < 16; s++) {
            if (t >= 3 && (s & 3) == 3) continue;                          /* (the synths rest a step a beat) */
            m2k_step_t *st = &q->step[t][s];
            st->note = (uint8_t)(ROOT[t] + (t >= 3 ? (s * 7) % 12 : 0));
            st->flags = (uint8_t)(ST_TRIG | ((s & 3) == 0 ? ST_ACC : 0));
            st->cond = (s & 3) == 2 ? COND[(s >> 2) & 3] : C_ALWAYS;
            st->rtrg = (uint8_t)(s == 7 || s == 15 ? 2 + (t & 1) : 0);
            if (s & 1 || nl + 4 > M2K_LOCKS) continue;
            static const uint8_t LP[4] = {P_BASE, P_DIST, P_DTIM, P_DEC};
            for (int i = 0; i < 4; i++)
                q->lock[nl++] = (m2k_lock_t){(uint8_t)t, (uint8_t)s, LP[i], (uint8_t)((s * 9 + t * 13 + i * 31) & 127)};
        }
    }
    q->glob[G_LEN] = 16;
    q->glob[G_VERB] = 90; q->glob[G_DAMP] = 64; q->glob[G_ADPC] = 30; q->glob[G_DRIV] = 70;
}
