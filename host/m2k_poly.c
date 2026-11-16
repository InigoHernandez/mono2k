/* SPDX-License-Identifier: GPL-3.0-only */
/* POLY's fixtures (1.11): the yardstick of a lead's six voices, every one sounding all the time (m2k_draw --poly-cpu,
 * --guard), and the two renders made for the owner (m2k_host DIR --poly-demo): a pad's chords held on an ADSR, and
 * drums on T1-T3 under three voices on T4-T6. Chords written as POLY records them (poly_note's order: the lowest the
 * lead's step, the next T+1's). Included after m2k_core.c */

/* chord N (K notes) on lead t's step s: sorted, the lowest the lead's, the next the voices'; FLAGS the lead's step's */
static void poly_chord(m2k_pattern_t *q, int t, int s, const uint8_t *n, int k, int flags)
{
    uint8_t c[8];
    for (int i = 0; i < k; i++) {
        int j = i;
        while (j > 0 && c[j - 1] > n[i]) { c[j] = c[j - 1]; j--; }
        c[j] = n[i];
    }
    for (int i = 0; i < k; i++) q->step[t + i][s] = (m2k_step_t){c[i], (uint8_t)(i ? ST_TRIG : flags), C_ALWAYS, 0};
}

/* the yardstick: T1 a lead of six (the FM-1's tracks), a six-note chord on every beat held over it (SUS, GATE a beat):
 * KIND 0 a plain machine (SID-6581's INIT), 1 SWAV-ENS's (its four voices each), 2 VO-6's, 3 SID-6581 with a delay on
 * the lead (ping-pong, fed back: each voice its own line) */
static void poly_pattern(m2k_pattern_t *q, int kind)
{
    static const uint8_t M[4] = {M_SID, M_ENS, M_VOX, M_SID};
    static const uint8_t CH[4][6] = {{48, 55, 60, 64, 67, 71}, {45, 52, 57, 60, 64, 69}, {41, 48, 53, 57, 60, 64},
                                     {43, 50, 55, 59, 62, 67}};
    pat_init(q, "POLY");
    kit_init(q, 0, M[kind]);
    uint8_t *k = q->kit[0];
    k[P_SUS] = 110; k[P_GATE] = 64; k[P_REL] = 60;
    if (kind == 3) { k[P_DSND] = 110; k[P_DTIM] = 60; k[P_DFB] = 80; k[P_DWID] = 127; }
    q->glob[G_POLY1] = M2K_TRACKS6;
    q->glob[G_TMPO] = 120 - 30;
    for (int s = 0; s < 16; s += 4) poly_chord(q, 0, s, CH[s / 4], M2K_TRACKS6, ST_TRIG);
}

/* the owner's renders. 0: a pad's chords, T1 a SWAV-SAW pad on POLY 4 (T1-T4) on an ADSR (a slow attack, a sustain,
 * a long release), its filter swept by an LFO (each voice its copy), Am7 Fmaj7 Cmaj7 G6 voice-led, a bar each, at 84
 * BPM; under it T5 a GND-SIN on the roots. 1: drums on T1-T3 (DRM-KICK, -SNAR, -HATS) and T4 an FM e-piano on POLY 3
 * (T4-T6): Dm7 G7 Cmaj7 Am7 stabs, syncopated, at 100 BPM */
static void poly_demo(m2k_pattern_t *q, int kind)
{
    pat_init(q, kind ? "STABS" : "PADS");
    for (int t = 0; t < M2K_TRACKS6; t++) for (int s = 0; s < M2K_STEPS; s++) q->step[t][s] = (m2k_step_t){60, 0, C_ALWAYS, 0};
    if (!kind) {
        static const uint8_t CH[4][4] = {{57, 60, 64, 67}, {57, 60, 64, 65}, {55, 60, 64, 71}, {55, 59, 62, 64}};
        static const uint8_t ROOT[4] = {33, 29, 36, 31};
        kit_init(q, 0, M_SAW);
        uint8_t *k = q->kit[0];
        k[P_ATK] = 52; k[P_HOLD] = 0; k[P_DEC] = 70; k[P_SUS] = 104; k[P_REL] = 96; k[P_GATE] = 120;
        k[P_BASE] = 6; k[P_WDTH] = 34; k[P_LPQ] = 34; k[P_SPU] = 60; k[P_VOL] = 92; k[P_LEVL] = 100;
        uint8_t *l = &k[P_LFO];
        l[L_DEST] = P_WDTH + 1; l[L_WAVE] = enc(1, 7); l[L_SPD] = 40; l[L_DPTH] = 88;
        q->glob[G_POLY1] = 4;
        kit_init(q, 4, M_GSIN);
        q->kit[4][P_SUS] = 90; q->kit[4][P_REL] = 70; q->kit[4][P_GATE] = 120; q->kit[4][P_LEVL] = 96;
        for (int b = 0; b < 4; b++) {
            poly_chord(q, 0, 16 * b, CH[b], 4, ST_TRIG);
            poly_chord(q, 0, 16 * b + 8, CH[b], 4, ST_TRIG);
            q->step[4][16 * b] = (m2k_step_t){ROOT[b], ST_TRIG, C_ALWAYS, 0};
            q->step[4][16 * b + 8] = (m2k_step_t){ROOT[b], ST_TRIG, C_ALWAYS, 0};
        }
        q->glob[G_LEN] = 64;
        q->glob[G_TMPO] = 84 - 30;
        q->glob[G_VERB] = 70; q->glob[G_DAMP] = 80;
        q->mute = 1u << 5;
        return;
    }
    static const uint8_t CH[4][3] = {{53, 57, 60}, {53, 59, 62}, {52, 55, 59}, {52, 55, 60}};
    static const uint8_t STAB[4] = {0, 3, 6, 10};
    kit_init(q, 0, M_DKIK);
    kit_init(q, 1, M_DSNR);
    kit_init(q, 2, M_DHAT);
    kit_init(q, 3, M_FMS);
    q->kit[3][P_GATE] = 10; q->kit[3][P_SPU] = 30; q->kit[3][P_LEVL] = 84;   /* (three voices: each under a track alone) */
    q->glob[G_POLY1 + 3] = 3;
    for (int s = 0; s < 64; s++) {
        if (!(s & 3) || s % 16 == 14) q->step[0][s] = (m2k_step_t){36, ST_TRIG, C_ALWAYS, 0};
        if (s % 8 == 4) q->step[1][s] = (m2k_step_t){60, ST_TRIG, C_ALWAYS, 0};
        if (s & 1) q->step[2][s] = (m2k_step_t){60, (uint8_t)(ST_TRIG | (s % 4 == 3 ? ST_ACC : 0)), C_ALWAYS, 0};
    }
    for (int b = 0; b < 4; b++)
        for (int i = 0; i < 4; i++) poly_chord(q, 3, 16 * b + STAB[i], CH[b], 3, (uint8_t)(ST_TRIG | (i ? 0 : ST_ACC)));
    q->glob[G_LEN] = 64;
    q->glob[G_TMPO] = 100 - 30;
    q->glob[G_VERB] = 40;
}
