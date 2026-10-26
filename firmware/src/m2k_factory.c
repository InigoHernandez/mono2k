/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's factory patterns: sixteen sound designs in bank A to show what six mono machines can do (the blank kit
 * fills the other banks).
 * Each pattern is a whole song part: its six machines and every parameter, its bus, its tempo, its steps.
 * Steps are written as a grid, one character a step ('|' and ' ' only separate):
 *   .  rest          x  trig        X  accent        s  slide         S  accent + slide
 *   Q  75 %          ?  50 %        q  25 %          e  12 %
 *   2 3 4  ratchet   h  1:2         H  2:2           k  4:4           f  FILL     n  not FILL    p  PRE   o  1ST
 * Notes are given in order and cycle over the trigs. */

static void beat(m2k_pattern_t *p, int t, const char *g, const uint8_t *notes, int nn)
{
    int s = 0, k = 0;
    for (const char *c = g; *c && s < M2K_STEPS; c++) {
        if (*c == '|' || *c == ' ') continue;
        int on = 1, fl = 0, cond = C_ALWAYS, rt = 0;
        switch (*c) {
        case '.': on = 0; break;
        case 'x': break;
        case 'X': fl = ST_ACC; break;
        case 's': fl = ST_SLD; break;
        case 'S': fl = ST_ACC | ST_SLD; break;
        case 'Q': cond = C_P75; break;
        case '?': cond = C_P50; break;
        case 'q': cond = C_P25; break;
        case 'e': cond = C_P12; break;
        case '2': rt = 2; break;
        case '3': rt = 3; break;
        case '4': rt = 4; break;
        case 'h': cond = C_1_2; break;
        case 'H': cond = C_2_2; break;
        case 'k': cond = C_4_4; break;
        case 'f': cond = C_FILL; break;
        case 'n': cond = C_NFILL; break;
        case 'p': cond = C_PRE; break;
        case 'o': cond = C_1ST; break;
        default: on = 0; break;
        }
        if (on) {
            m2k_step_t *st = &p->step[t][s];
            st->flags = (uint8_t)(ST_TRIG | fl);
            st->note = notes[k % nn];
            st->cond = (uint8_t)cond;
            st->rtrg = (uint8_t)rt;
            k++;
        }
        s++;
    }
}
#define NOTES(...) (const uint8_t[]){__VA_ARGS__}, (int)sizeof((const uint8_t[]){__VA_ARGS__})
#define BEAT(p, t, g, ...) beat(p, t, g, NOTES(__VA_ARGS__))

/* a pattern from before version 2 timed ATK HOLD DEC REL on MONO2K's first curve: each value moves to the one whose
 * time on the Monomachine's curve is nearest, in the kit and in the locks */
static uint8_t amp_v1_val(int prm, int v) { return prm >= P_ATK && prm <= P_REL ? T_AMPV1[prm - P_ATK][v & 127] : (uint8_t)v; }
static void amp_v1(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int k = P_ATK; k <= P_REL; k++) p->kit[t][k] = amp_v1_val(k, p->kit[t][k]);
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk != 0xFF) p->lock[i].val = amp_v1_val(p->lock[i].param, p->lock[i].val);
}

/* patterns before version 5 ran MONO2K's own chain; from 5 it is the Monomachine's (measured): every chain setting
 * goes to the one that gives the same Hz, dB or time (tools/gen_tables.py, T_V5_*). The filter now follows the
 * note, so its old cutoffs are placed against the note the track plays most */
static int v5_base(int pos, int note) { return pos <= 0 ? 0 : CLAMP(16 + (pos - 8 * note + (pos >= 8 * note ? 6 : -6)) / 12, 0, 127); }

static uint8_t v5_val(int prm, int v, const uint8_t *k, int note)
{
    v &= 127;                          /* (the tables' reach, whatever a store held) */
    switch (prm) {
    case P_DIST: return T_V5_DIST[v];
    case P_VOL: return T_V5_VOL[v];
    case P_BASE: return (uint8_t)(v ? v5_base(T_V5_FPOS[v], note) : 0);
    case P_WDTH: {
        int b = k[P_BASE] & 127, lp = b + v;
        if (lp >= 127 && k[P_LPQ] == 0) return 127;
        int nb = b ? v5_base(T_V5_FPOS[b], note) : 0;
        return (uint8_t)CLAMP(v5_base(T_V5_FPOS[CLAMP(lp, 0, 127)], note) - nb, 0, 127);
    }
    case P_HPQ: return T_V5_HPQ[v];
    case P_LPQ: return T_V5_LPQ[v];
    case P_FATK: return T_V5_FATK[v];
    case P_FDEC: return T_V5_FDEC[v];
    case P_BOF: case P_WOF: return T_V5_OFS[v];
    case P_EQF: return T_V5_EQF[v];
    case P_EQG: return T_V5_EQG[v];
    case P_SRR: return T_V5_SRR[v];
    case P_DTIM: return (uint8_t)(v * 4 > 127 ? 127 : v * 4);
    case P_DSND: return (uint8_t)(64 - (v + 1) / 2);
    case P_DFB: return (uint8_t)((v * 270 * 64 + 16384) >> 15);
    case P_DBAS: return T_V5_DHP[v];
    case P_DWID: {
        int nb = T_V5_DHP[k[P_DBAS] & 127];
        return (uint8_t)CLAMP(T_V5_DLP[CLAMP(k[P_DBAS] + v, 0, 127)] - nb, 0, 127);
    }
    default: return (uint8_t)v;
    }
}

static void chain_v4(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        int count[128] = {0}, note = 60, best = 0;
        for (int s = 0; s < M2K_STEPS; s++)
            if ((p->step[t][s].flags & ST_TRIG) && ++count[p->step[t][s].note & 127] > best) { best = count[p->step[t][s].note & 127]; note = p->step[t][s].note & 127; }
        uint8_t old[P_COUNT], *k = p->kit[t];
        m_cpy(old, k, P_COUNT);
        for (int i = 0; i < M2K_LOCKS; i++) {          /* the locks against the old kit, before it changes */
            m2k_lock_t *l = &p->lock[i];
            if (l->trk == t) l->val = v5_val(l->param, l->val, old, note);
        }
        for (int prm = P_ATK; prm < P_BITS; prm++) k[prm] = v5_val(prm, old[prm], old, note);
    }
}

/* SWAV-ENS took the Monomachine's page (version 7): an old interval (0..24 semitones over the root) folds into the
 * table's -11..+12 by octaves, and one that would fold to 0 is +12 (the voice stays and keeps its note); always the
 * equal-tempered value in the middle of its group, so never OFF (61-63) nor the octave down (0-1). DTUN's fixed spread
 * is not the chorus's moving copy: CHRL 0. SHPE, PW, LEV and SUB have no counterpart: one saw at the note, the voices
 * at full level. LEV's silence is kept step by step: a trig where the old level was 0 (the kit's, or a lock's) gets
 * PCH2..4 locked OFF, the chord keeping its intervals where a lock raised it (a pattern without room for those locks
 * keeps the voices). Interval locks convert, the rest go. An LFO aimed at an ENS track's page is cleared, its
 * intervals too: the new axis has OFF at 61-63 and the octave down at 0-1, so a sweep would drop voices or jump an
 * octave (no factory pattern had one); so is a step's lock of an LFO's destination onto it */
static uint8_t ens_fold(int v)
{
    int i = (v * 25) >> 7;
    while (i > 12) i -= 12;
    if (i == 0) i = 12;
    return T_ENS_ET[i + 10];
}

static int ens_lock(m2k_pattern_t *p, int t, int st, int prm, int v)
{
    int free = -1;
    for (int i = 0; i < M2K_LOCKS; i++) {
        if (p->lock[i].trk == t && p->lock[i].step == st && p->lock[i].param == prm) { p->lock[i].val = (uint8_t)v; return 1; }
        if (free < 0 && p->lock[i].trk == 0xFF) free = i;
    }
    if (free < 0) return 0;
    p->lock[free] = (m2k_lock_t){(uint8_t)t, (uint8_t)st, (uint8_t)prm, (uint8_t)v};
    return 1;
}

static int ens_target(int trck) { return ((trck & 127) * M2K_TRACKS) >> 7; }        /* the track an LFO reaches */

static void ens_v6(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_ENS) continue;
        uint8_t *k = p->kit[t];
        /* which trigs the old level silenced, before its locks go */
        uint8_t mute[M2K_STEPS];
        int any_up = k[P_SYN + 6] > 0;
        for (int st = 0; st < M2K_STEPS; st++) mute[st] = k[P_SYN + 6] == 0;
        for (int i = 0; i < M2K_LOCKS; i++)
            if (p->lock[i].trk == t && p->lock[i].param == P_SYN + 6) {
                mute[p->lock[i].step] = p->lock[i].val == 0;
                any_up |= p->lock[i].val > 0;
            }
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param >= 8) continue;
            if (l->param < 3) l->val = ens_fold(l->val);
            else l->trk = 0xFF;
        }
        for (int i = 0; i < 3; i++) k[P_SYN + i] = any_up ? ens_fold(k[P_SYN + i]) : 62;
        k[P_SYN + 3] = 0; k[P_SYN + 4] = 64; k[P_SYN + 5] = 0; k[P_SYN + 6] = 64; k[P_SYN + 7] = 64;
        if (any_up)
            for (int st = 0; st < M2K_STEPS; st++)
                if (mute[st] && (p->step[t][st].flags & ST_TRIG))
                    for (int i = 0; i < 3; i++) ens_lock(p, t, st, P_SYN + i, 62);
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] == M_ENS && dest >= P_SYN && dest < P_SYN + 8) b[L_DEST] = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {          /* a step's lock of this LFO's destination */
                m2k_lock_t *lk2 = &p->lock[i];
                if (lk2->trk != t || lk2->param != P_LFO + 8 * l + L_DEST) continue;
                int to2 = to, d2 = lk2->val - 1;
                for (int j = 0; j < M2K_LOCKS; j++)
                    if (p->lock[j].trk == t && p->lock[j].step == lk2->step && p->lock[j].param == P_LFO + 8 * l + L_TRCK)
                        to2 = ens_target(p->lock[j].val);
                if (p->machine[to2] == M_ENS && d2 >= P_SYN && d2 < P_SYN + 8) lk2->val = 0;
            }
        }
}

/* the bus's controls took new laws (version 9, tools/gen_tables.py): each old value goes to the new one that sounds as
 * it. GLCH, DRIV, TAPE and ADPC as they were (TAPE's 1..3, under a cent of wow, to its least); HISS by its crackle, the
 * old crackle as it was, its hiss there as loud or louder (its own law brings it in sooner) */
static void bus_v9(m2k_pattern_t *p)
{
    uint8_t *g = p->glob;
    g[G_GLCH] = T_V9_GLCH[g[G_GLCH] & 127];
    g[G_HISS] = T_V9_HISS[g[G_HISS] & 127];
    g[G_DRIV] = T_V9_DRIV[g[G_DRIV] & 127];
    g[G_TAPE] = T_V9_TAPE[g[G_TAPE] & 127];
    g[G_ADPC] = T_V9_ADPC[g[G_ADPC] & 127];
}

/* FM+STAT and FM+DYN took the Monomachine's pages (version 10). The old ones were MONO2K's own: RAT1 DEP1 DEC1 RAT2
 * DEP2 DEC2 FDBK, then ALGO (STAT) or SWEP (DYN), the second operator fed back and modulating the first in series.
 * Both new machines run their operators side by side, so an old sound is carried over as near as the pages go, not
 * as it was: each ratio to the nearest the new page holds (STAT's 24 steps with 1FIN making up the rest; DYN's
 * straight /64 and squared /64); each depth to the new law's nearest; a decay to the new decay of the nearest time,
 * which starts from 64's depth (STAT's 1ENV, DYN's 2ENV) or, on DYN's first operator, as 1VEN's push down to a 1VOL
 * that keeps the old start; the feedback to the first operator (STAT) or stays on the second (DYN); SWEP to 1FEN.
 * A lock goes with its parameter where it has a place; DEC1, DEC2, ALGO's go, and so does an LFO aimed at them */
static int near_u32(const uint32_t *T, int lo, int hi, uint32_t want)
{
    int best = lo;
    for (int v = lo; v <= hi; v++)
        if ((T[v] > want ? T[v] - want : want - T[v]) < (T[best] > want ? T[best] - want : want - T[best])) best = v;
    return best;
}

static int isqrt(int x)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= x) r++;
    return r;
}

/* a ratio (Q8) to FM+STAT's step, as its knob value, and the 1FIN that makes up the rest */
static int fms_ratio(int q8, int *fin)
{
    int best = 0;
    uint64_t r = (uint64_t)q8 << 8;
    for (int s = 1; s < 24; s++) {    /* nearest on a log scale, by products (no division) */
        uint64_t x = T_FMR24[s], y = T_FMR24[best];
        int closer = x >= r ? (y >= r ? x < y : x * y < r * r) : (y >= r ? r * r < x * y : x > y);
        if (closer) best = s;
    }
    if (fin) {
        int f = (int)(((uint32_t)q8 << 16) / T_FMR24[best]) - 192;   /* (192 + fin) / 256 = r / step */
        *fin = CLAMP(f, 0, 127);
    }
    return (best * 128 + 23) / 24;
}

static int fm_dec(int d) { return 65 + near_u32(T_FMEC, 1, 63, T_ENV_COEF[d]) - 1; }        /* an old decay's 1ENV */

/* where an old page's parameter went: STAT, DYN (-1: nowhere) */
static const int8_t FM_TO[2][8] = {{0, 2, -1, 4, 5, -1, 3, -1}, {0, 2, -1, 4, 5, -1, 6, 1}};

static int fm_val(int dyn, int prm, int v, const uint8_t *old)
{
    v &= 127;                         /* (the tables' reach, whatever a store held; old[] is masked by the caller) */
    switch (prm) {
    case 0: return dyn ? (T_RATIO[(v * 32) >> 7] >= 512 ? 127 : (T_RATIO[(v * 32) >> 7] + 2) >> 2)
                       : fms_ratio(T_RATIO[(v * 32) >> 7], 0);
    case 3: return dyn ? (T_RATIO[(v * 32) >> 7] >= 1024 ? 127 : isqrt(T_RATIO[(v * 32) >> 7] * 16))
                       : fms_ratio(T_RATIO[(v * 32) >> 7], 0);
    case 1:
        if (dyn) {                    /* 1VOL: the old start, less 1VEN's push */
            int d = old[2] < 127 ? near_u32(T_FMDEC, 1, 63, T_ENV_COEF[old[2]]) : 0;
            int e = d < 32 ? 4 * d : 128;
            return CLAMP(near_u32(T_FMDV, 0, 256, T_FMK[v]) - e, 0, 127);
        }
        return near_u32(T_FMB, 0, 64, T_FMK[v]);
    case 4: return dyn ? near_u32(T_FMDV, 0, 64, T_FMK[v]) : near_u32(T_FMB, 0, 127, T_FMK[v]);
    case 6: return near_u32(T_FMFB, 0, 127, T_FMK[v] >> 1);
    case 7: {                         /* SWEP: the old push at C4, as 1FEN's note-independent one */
        int sw = v - 64;
        return sw >= 0 ? 64 + CLAMP(isqrt(12 * sw), 0, 63) : 64 - CLAMP(isqrt(48 * -sw), 0, 64);
    }
    default: return 64;
    }
}

static void fm_v9(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        int m = p->machine[t], dyn = m == M_FMD;
        if (m != M_FMS && m != M_FMD) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        for (int i = 0; i < 8; i++) o[i] &= 127;
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param >= P_SYN + 8) continue;   /* P_SYN is 0: none under it */
            int prm = l->param - P_SYN, to = FM_TO[dyn][prm];
            if (prm == 1 && !dyn && o[2] < 127) to = -1;          /* STAT's 1ENV holds the decay */
            if (prm == 4 && dyn && o[5] < 127) to = -1;           /* and DYN's 2ENV */
            if (to < 0) { l->trk = 0xFF; continue; }
            l->val = (uint8_t)fm_val(dyn, prm, l->val, o);
            l->param = (uint8_t)(P_SYN + to);
        }
        int fin = 64;
        if (dyn) {
            k[0] = (uint8_t)fm_val(1, 0, o[0], o);
            k[1] = (uint8_t)fm_val(1, 7, o[7], o);
            k[2] = (uint8_t)fm_val(1, 1, o[1], o);
            k[3] = (uint8_t)(o[2] < 127 ? 64 + near_u32(T_FMDEC, 1, 63, T_ENV_COEF[o[2]]) : 64);
            k[4] = (uint8_t)fm_val(1, 3, o[3], o);
            k[5] = (uint8_t)(o[5] < 127 ? fm_dec(o[5]) : fm_val(1, 4, o[4], o));
            k[6] = (uint8_t)fm_val(1, 6, o[6], o);
        } else {
            k[0] = (uint8_t)fms_ratio(T_RATIO[(o[0] * 32) >> 7], &fin);
            k[1] = (uint8_t)fin;
            k[2] = (uint8_t)(o[2] < 127 ? fm_dec(o[2]) : fm_val(0, 1, o[1], o));
            k[3] = (uint8_t)fm_val(0, 6, o[6], o);
            k[4] = (uint8_t)fm_val(0, 3, o[3], o);
            k[5] = (uint8_t)fm_val(0, 4, o[4], o);
            k[6] = 64;
        }
        k[7] = 64;
    }
    for (int t = 0; t < M2K_TRACKS; t++)              /* LFOs aimed at an FM page: to the parameter's new place */
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1, m = p->machine[to];
            if ((m != M_FMS && m != M_FMD) || dest < P_SYN || dest >= P_SYN + 8) continue;
            int n = FM_TO[m == M_FMD][dest - P_SYN];
            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n + 1);
        }
}

/* SID took the Monomachine's SID-6581 page (version 11). The old one was MONO2K's own, two oscillators: WAV1 PW PWM
 * WAV2 DTN2 FINE MODE MIX. Carried over as near as the new page goes: the first oscillator's wave (a combination to
 * MIX), its duty (PW now a distance from 64), PWM to the sweep, MODE to MOD, the second oscillator's interval to MFRQ
 * (where it stood over C4; MFRQ is the same at every note). WAV2, FINE and MIX have no place; a lock goes with its
 * parameter, an LFO too */
static const int8_t SID_TO[8] = {3, 0, 1, -1, 6, -1, 4, -1};

static int sid_val(int prm, int v)
{
    static const uint8_t W[8] = {0, 40, 64, 115, 90, 90, 90, 90}, M[4] = {0, 64, 32, 100};
    switch (prm) {
    case 0: return W[(v * 8) >> 7];
    case 1: { int m = v < 128 - v ? v : 128 - v; return CLAMP(64 - m, 0, 127); }
    case 2: return v >> 1;
    case 4: return CLAMP(32 + ((v - 64) * 24 / 64) * 32 / 12, 0, 127);
    case 6: return M[(v * 4) >> 7];
    default: return 64;
    }
}

/* format 14: VOX-6 on the Monomachine's VO-6 page (VOC1 VOC2 V-SW VOIC CONS CLEN CVOL TUNE), from MONO2K's own (VOWL
 * VSWP VTIM SHFT BRTH RESO BUZZ VIBR): the vowel nearest the old one (T_VOX14: VOWL's F1 F2 to the nearest VOC1 VOC2),
 * BRTH to VOIC (the noise's share alike), V-SW off, no consonant, CLEN CVOL TUNE in the middle. A VOWL lock becomes
 * VOC1's, a BRTH lock VOIC's, the others go (their knobs are gone); an LFO on VOWL stays on VOC1, one on BRTH moves to
 * VOIC, one on the rest stops */
static void vox_v13(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_VOX) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        for (int i = 0; i < 8; i++) o[i] &= 127;
        k[0] = T_VOX14[o[0]][0]; k[1] = T_VOX14[o[0]][1]; k[2] = 0; k[3] = T_VOX14_BR[o[4]];
        k[4] = 0; k[5] = 64; k[6] = 64; k[7] = 64;
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param < P_SYN || l->param >= P_SYN + 8) continue;
            l->val &= 127;
            if (l->param == P_SYN) l->val = T_VOX14[l->val][0];
            else if (l->param == P_SYN + 4) { l->param = P_SYN + 3; l->val = T_VOX14_BR[l->val]; }
            else l->trk = 0xFF;
        }
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] != M_VOX || dest < P_SYN || dest >= P_SYN + 8) continue;
            b[L_DEST] = (uint8_t)(dest == P_SYN ? P_SYN + 1 : dest == P_SYN + 4 ? P_SYN + 3 + 1 : 0);
        }
}

/* format 16: the amp and the LFOs on the owner's laws (gen_tables.py). ATK DEC REL to the value that sounds nearest
 * (T_AMP15: DEC and REL by their time to -60 dB, ATK by its rise's loudness), an LFO's SPD to the nearest rate (T_SPD15)
 * and its DPTH, now squared about 64, to the same swing (T_DPTH15); in the kit and in the locks. An LFO on a DEC or a
 * REL swings over the new law's steps: its depth the one that lands where the old swing did (feel_lfo) */
static int amp_row(int prm) { return prm == P_ATK ? 0 : prm == P_DEC ? 1 : prm == P_REL ? 2 : -1; }

static uint8_t feel_val(int prm, int v)
{
    v &= 127;
    if (amp_row(prm) >= 0) return T_AMP15[amp_row(prm)][v];
    if (prm >= P_LFO && prm < P_LFO + 24 && (prm - P_LFO) % 8 == L_SPD) return T_SPD15[v];
    if (prm >= P_LFO && prm < P_LFO + 24 && (prm - P_LFO) % 8 == L_DPTH) return T_DPTH15[v];
    return (uint8_t)v;
}

/* an old depth d on a DEC or REL (row 1, 2) whose old value is c: the new depth whose swing, at 17 points across it,
 * lands nearest (in the new law's Q8 steps) to where the old one's (whole steps, as the old engine read them) sound */
static uint8_t feel_lfo(int row, int c, int d)
{
    int best = 64;
    int32_t be = -1, cn = T_AMP15[row][c & 127] * 256;
    for (int n = 0; n < 128; n++) {
        int32_t e = 0;
        for (int32_t v = -32767; v <= 32767; v += 4095) {   /* (the shape by the depth: 2^15 by 2^14 at most) */
            int32_t po = CLAMP((c & 127) * 256 + ((v * ((d & 127) - 64)) >> 6), 0, 127 * 256) >> 8;
            int32_t x = T_AMP15[row][po] * 256 - CLAMP(cn + ((v * T_LDPTH[n]) >> 14), 0, 127 * 256);
            e += x < 0 ? -x : x;
        }
        if (be < 0 || e < be) { be = e; best = n; }
    }
    return (uint8_t)best;
}

/* a kit's LFO page value (DPTH or SPD) at index prm, the LFO aiming at kit tk's parameter dest (tk the old one) */
static uint8_t feel_lfo_val(int prm, int v, int dest, const uint8_t *tk)
{
    int row = amp_row(dest);
    if ((prm - P_LFO) % 8 == L_DPTH && row > 0) return feel_lfo(row, tk[dest], v);
    return feel_val(prm, v);
}

static void feel_v15(m2k_pattern_t *p)
{
    static uint8_t old[M2K_TRACKS][P_COUNT];        /* the kits before: an LFO's depth reads its target's old value */
    m_cpy(old, p->kit, sizeof old);
    for (int t = 0; t < M2K_TRACKS; t++) {
        for (int prm = P_ATK; prm <= P_REL; prm++) p->kit[t][prm] = feel_val(prm, old[t][prm]);
        for (int l = 0; l < 3; l++) {
            const uint8_t *b = &old[t][P_LFO + 8 * l];
            p->kit[t][P_LFO + 8 * l + L_SPD] = T_SPD15[b[L_SPD] & 127];
            p->kit[t][P_LFO + 8 * l + L_DPTH] = feel_lfo_val(P_LFO + 8 * l + L_DPTH, b[L_DPTH], b[L_DEST] - 1,
                                                             old[ens_target(b[L_TRCK])]);
        }
    }
    for (int i = 0; i < M2K_LOCKS; i++) {
        m2k_lock_t *l = &p->lock[i];
        if (l->trk >= M2K_TRACKS) continue;
        if (l->param >= P_LFO && l->param < P_LFO + 24) {
            const uint8_t *b = &old[l->trk][P_LFO + (l->param - P_LFO) / 8 * 8];
            l->val = feel_lfo_val(l->param, l->val, b[L_DEST] - 1, old[ens_target(b[L_TRCK])]);
        } else l->val = feel_val(l->param, l->val);
    }
}

/* a sound of the library saved before format 16: its kit on the same laws (its LFOs aiming at its own values) */
static void feel_kit_v15(uint8_t *k)
{
    uint8_t o[P_COUNT];
    m_cpy(o, k, P_COUNT);
    for (int prm = P_ATK; prm <= P_REL; prm++) k[prm] = feel_val(prm, o[prm]);
    for (int l = 0; l < 3; l++) {
        uint8_t *b = &k[P_LFO + 8 * l];
        b[L_SPD] = T_SPD15[b[L_SPD] & 127];
        b[L_DPTH] = feel_lfo_val(P_LFO + 8 * l + L_DPTH, b[L_DPTH], b[L_DEST] - 1, o);
    }
}

/* format 13: HOLD in beats, as the Monomachine's (v / 32 of a beat, measured at 60, 120 and 240 BPM), no longer 14.6 ms
 * a step: the kit's and its locks' HOLD to the step nearest the time they held, at the pattern's tempo (INF stays) */
static uint8_t hold_to(int v, int bpm) { return v >= 127 ? 127 : (uint8_t)CLAMP((v * bpm * 146 + 9375) / 18750, 0, 126); }

static void hold_v12(m2k_pattern_t *p)
{
    int bpm = 30 + p->glob[G_TMPO];
    for (int t = 0; t < M2K_TRACKS; t++) p->kit[t][P_HOLD] = hold_to(p->kit[t][P_HOLD], bpm);
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk != 0xFF && p->lock[i].param == P_HOLD) p->lock[i].val = hold_to(p->lock[i].val, bpm);
}

/* format 12: GND-SIN on the Monomachine's page, TUNE on its eighth knob (CC 55) and MONO2K's own before it. A..H were
 * TUNE FOLD SUB NOIS PDEP PDEC CLIK HARM, are FOLD SUB NOIS PDEP PDEC CLIK HARM TUNE: the kit, its locks and every LFO
 * aimed at one of them (from any track) move, values as they were */
static int gsin_to(int k) { return k == 0 ? 7 : k - 1; }

static void gsin_v11(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_GSIN) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        for (int i = 0; i < 8; i++) k[gsin_to(i)] = o[i];
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk == t && l->param < P_SYN + 8) l->param = (uint8_t)(P_SYN + gsin_to(l->param - P_SYN));
        }
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] != M_GSIN || dest < P_SYN || dest >= P_SYN + 8) continue;
            b[L_DEST] = (uint8_t)(P_SYN + gsin_to(dest - P_SYN) + 1);
        }
}

static void sid_v10(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_SID) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param >= P_SYN + 8) continue;   /* P_SYN is 0: none under it */
            int prm = l->param - P_SYN;
            if (SID_TO[prm] < 0) { l->trk = 0xFF; continue; }
            l->val = (uint8_t)sid_val(prm, l->val);
            l->param = (uint8_t)(P_SYN + SID_TO[prm]);
        }
        k[0] = (uint8_t)sid_val(1, o[1]); k[1] = (uint8_t)sid_val(2, o[2]); k[2] = 127; k[3] = (uint8_t)sid_val(0, o[0]);
        k[4] = (uint8_t)sid_val(6, o[6]); k[5] = 0; k[6] = (uint8_t)sid_val(4, o[4]); k[7] = 64;
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] != M_SID || dest < P_SYN || dest >= P_SYN + 8) continue;
            int n = SID_TO[dest - P_SYN];
            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n + 1);
        }
}

static void lk(m2k_pattern_t *p, int t, int s, int prm, int v)
{
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == 0xFF) { p->lock[i] = (m2k_lock_t){(uint8_t)t, (uint8_t)s, (uint8_t)prm, (uint8_t)v}; return; }
}

static void set(m2k_pattern_t *p, int t, const uint8_t *pv, int n)   /* (param, value) pairs */
{
    for (int i = 0; i + 1 < n; i += 2) p->kit[t][pv[i]] = pv[i + 1];
}
#define SET(p, t, ...) do { const uint8_t _v[] = {__VA_ARGS__}; set(p, t, _v, sizeof _v); } while (0)

/* an LFO: its destination (a parameter of the same track), wave, multiplier index (x1 .. x128), speed, depth,
 * trig mode (FREE TRIG HOLD ONE HALF) */
static void lfo(m2k_pattern_t *p, int t, int l, int dest, int wave, int mult, int spd, int dpth, int trig)
{
    uint8_t *b = &p->kit[t][P_LFO + 8 * l];
    b[L_DEST] = (uint8_t)(dest + 1);
    b[L_WAVE] = enc(wave, 7);
    b[L_MULT] = enc(mult, 8);
    b[L_SPD] = (uint8_t)spd;
    b[L_DPTH] = (uint8_t)dpth;
    b[L_TRIG] = enc(trig, 5);
}

/* GND-SIN's old page's init (TUNE first, a pitch drop and a click): what the parts and the older formats were written on */
static const uint8_t GSIN_V11[8] = {64, 0, 0, 0, 40, 40, 30, 0};

/* the blank kit of MONO2K's chain before version 5: what an old pattern was written against (the self-test's) */
static void kit_v4(m2k_pattern_t *p, int t, int m)
{
    static const uint8_t D[] = {0, 0, 64, 64, 0, 100, 64, 0, 0, 127, 0, 0, 0, 64, 64, 64, 64, 64, 0, 12, 0, 50, 0, 110};
    kit_default(p, t, m);
    if (m == M_GSIN) m_cpy(&p->kit[t][P_SYN], GSIN_V11, 8);
    m_cpy(&p->kit[t][P_ATK], D, sizeof D);
    p->kit[t][P_LEVL] = 110;
}

#define MACHINES(...) (const uint8_t[]){__VA_ARGS__}

/* every track's LEV scaled: the mix's level under the bus's knee */
static void trim(m2k_pattern_t *p, int lev)
{
    for (int t = 0; t < M2K_TRACKS; t++) p->kit[t][P_LEVL] = (uint8_t)CLAMP(p->kit[t][P_LEVL] * lev / 100, 0, 127);
}

/* the modulation effect: 0 chorus, 1 flanger, 2 phaser; rate (0.1..10 Hz), depth, mix */
#define MOD(p, t, type, rate, dpth, mix) SET(p, t, P_MODT, enc(type, 3), P_MODR, rate, P_MODD, dpth, P_MODM, mix)

/* (the machines' A..H by name, SA..SH, and the LFOs' pages, L1..L3: m2k_seq.c, with the machines' inits) */
/* choices */
#define WAVE(i) ((uint8_t)((i) * 127 / 31))   /* DIGI-WAV: 5 BELL 6 GLAS 7 PLST 8 LTEX 19 MTL1 20 MTL2 25 BITS 26 8BIT 30 CRSH */
#define GSIZE(i) enc(i, 5)             /* 1/64 1/32 1/16 1/8 1/4 */
#define FR24(i) enc(i, 24)             /* FM+'s 24 ratio steps, as the sound decodes them (fr24) */
/* the new bank's (docs/bank-a.md): SWAV-ENS on its own page, the owner's waves */
/* an ENS interval, -12 .. +12 semitones but 0, equal tempered (the middle of its group on the Monomachine's table) */
#define PCH(n) ((uint8_t)((n) <= -12 ? 0 : (n) < 0 ? T_ENS_ET[(n) + 11] : T_ENS_ET[(n) + 10]))
#define PCH_OFF 62
#define OS(n) ((uint8_t)(2 * ((n) - 1)))     /* DIGI-WAV on the owner's 64 (P_WBNK 127): OSn exactly */

/* the bank's bus drive, a master limiter's: each part is balanced under the knee without it (m2k_host --demo-dry),
 * then driven about 6 dB into the bus's soft clip, every slot at HAZE's loudness (docs/bank-a.md) */
#define BANK_DRIVE 88

/* an ENS chord on one step, or on every trig from step a to b */
static void chord(m2k_pattern_t *p, int t, int s, int i2, int i3, int i4)
{
    lk(p, t, s, SA, PCH(i2));
    lk(p, t, s, SB, PCH(i3));
    lk(p, t, s, SC, PCH(i4));
}

static void chords(m2k_pattern_t *p, int t, int a, int b, int i2, int i3, int i4)
{
    for (int s = a; s < b; s++)
        if (p->step[t][s].flags & ST_TRIG) chord(p, t, s, i2, i3, i4);
}

/* a DIGI-WAV part on another wave from step a to b (each trig there), at the VOL that gives that wave the part's
 * level: the owner's waves stand 16 dB apart, so a part is balanced wave by wave (tools/demo_levels.py) */
static void wave_span(m2k_pattern_t *p, int t, int a, int b, int wave, int vol)
{
    for (int s = a; s < b; s++)
        if (p->step[t][s].flags & ST_TRIG) { lk(p, t, s, SA, wave); lk(p, t, s, P_VOL, vol); }
}

/* ------------------------------------------------------------------------------------------------- bank A */

static void pattern_init_blank(m2k_pattern_t *p);   /* (m2k_proj.c) */

/* a part of the bank (docs/bank-a.md), written on today's pages: its machines at their init, the bank's bus drive */
static m2k_pattern_t *part(int i, const char *name, int bpm, const uint8_t *machines)
{
    m2k_pattern_t *p = PAT(i);
    pat_init(p, name);
    for (int t = 0; t < M2K_TRACKS; t++) kit_default(p, t, machines[t]);
    p->glob[G_TMPO] = (uint8_t)(bpm - 30);
    p->glob[G_LEN] = 64;
    p->glob[G_VERB] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    return p;
}
#define SPEED(i) enc(i, 7)             /* a track's: 0 1/8x 1 1/4x 2 1/2x 3 3/4x 4 1x 5 3/2x (triplets) 6 2x */
#define DW4(i) ((uint8_t)((i) * 32 + 16))   /* DRM-KICK's and DRM-SNAR's waves: 0 SIN 1 TRI 2 SAW 3 SQR */
#define DW5(i) ((uint8_t)((i) * 26 + 13))   /* DRM-HATS' and DRM-CYMB's: 0 SIN 1 TRI 2 SAW 3 SQR 4 NOIS */
#define SID5(i) enc(i, 5)              /* SID-6581's WAVE: 0 TRI 1 SAW 2 PULS 3 MIX 4 NOIS */
#define SIDM(i) enc(i, 4)              /* its MOD: 0 OFF 1 RING 2 SYNC 3 R+S */

/* each trig of track t a lock of prm, the next value of v in turn (cycling: over a count that is not the trigs', it
 * lands differently each bar) */
static void cyc(m2k_pattern_t *p, int t, int prm, const uint8_t *v, int n)
{
    for (int st = 0, k = 0; st < M2K_STEPS; st++)
        if (p->step[t][st].flags & ST_TRIG) lk(p, t, st, prm, v[k++ % n]);
}
#define CYC(p, t, prm, ...) cyc(p, t, prm, NOTES(__VA_ARGS__))

/* a part written from its score (tools/sheet/compose.py): its trigs, six bytes each (track, step, note, flags,
 * condition, retrigs), and its locks, four (track, step, parameter, value) */
static void trigs(m2k_pattern_t *p, const uint8_t *v, int n)
{
    for (int i = 0; i + 5 < n; i += 6) {
        m2k_step_t *st = &p->step[v[i]][v[i + 1]];
        st->note = v[i + 2]; st->flags = v[i + 3]; st->cond = v[i + 4]; st->rtrg = v[i + 5];
    }
}

static void locks(m2k_pattern_t *p, const uint8_t *v, int n)
{
    for (int i = 0; i + 3 < n; i += 4) lk(p, v[i], v[i + 1], v[i + 2], v[i + 3]);
}

/* bank A: four parts (docs/bank-a.md, written to docs/composing.md): every sound picked by the owner from the sound
 * sheets (tools/sheet/), each 4-bar pattern a 16-bar form (what enters after the first pass, what alternates, what comes
 * on the third and fourth, LFOs over 16 bars), at the bank's loudness. The rest of the bank blank */
static void factory(void)
{
    m2k_pattern_t *p;

    /* A01 CIRCUIT. A minor at 126, electro. The kick broken (steps 1, 8 and 11) and on the root; the snare and the
     * hats in from the second pass, the hats a different cell each bar, the open hat once, on the fourth pass's turn; a
     * sine bass, a long root just after the kick then short answers on the chord's notes, each bar stepping into the
     * next chord (Am9 Am9 Fmaj7 G6); the ENS chord hits in the gaps the kick and bass leave, a different place each
     * bar, their filter opening over 16 bars; FM+STAT keys on the second and fourth passes, an arpeggio with its own
     * melody, a sparser answer on the third */
    p = part(0, "CIRCUIT", 126, MACHINES(M_DKIK, M_DSNR, M_DHAT, M_GSIN, M_ENS, M_FMS));
    SET(p, 0, SA, 2, SB, 22, SC, 8, SD, 12, SE, 34, SF, 27, SG, 72, SH, 76, P_HOLD, 40, P_DEC, 78, P_REL, 8,
        P_BASE, 22, P_WDTH, 115, P_HPQ, 51, P_LPQ, 63, P_FATK, 55, P_FDEC, 74, P_BOF, 33, P_WOF, 56, P_EQF, 93,
        P_EQG, 41, P_LEVL, 72, P_GATE, 15, P_MODT, 106, P_MODR, 113, P_MODD, 96);
    SET(p, 1, SA, 16, SB, 0, SC, 0, SD, 100, SE, 80, P_DEC, 28, P_REL, 14, P_BASE, 8, P_WDTH, 52, P_HPQ, 18, P_LPQ, 9,
        P_FATK, 23, P_FDEC, 28, P_EQF, 72, P_EQG, 53, P_SPU, 24, P_LEVL, 127, P_GATE, 23, L1 + L_DEST, 34,
        L1 + L_WAVE, 45, L1 + L_TRIG, 12, L1 + L_MULT, 72, L1 + L_SPD, 64, L1 + L_DPTH, 40, P_MODT, 21, P_MODR, 65,
        P_MODD, 41);
    SET(p, 2, SA, 119, SB, 70, SC, 10, SD, 84, SE, 100, SF, 117, SG, 12, P_HOLD, 7, P_DEC, 53, P_REL, 13, P_VOL, 100,
        P_PAN, 90, P_BASE, 50, P_WDTH, 40, P_HPQ, 23, P_FATK, 21, P_FDEC, 20, P_BOF, 71, P_WOF, 60, P_EQF, 45,
        P_EQG, 71, P_DFB, 49, P_DWID, 45, P_LEVL, 127, P_GATE, 25, L1 + L_DEST, 11, L1 + L_WAVE, 9, L1 + L_TRIG, 12,
        L1 + L_MULT, 56, L1 + L_DPTH, 28, P_MODT, 106, P_MODR, 32, P_MODD, 126);
    SET(p, 3, SA, 18, SG, 50, P_HOLD, 24, P_DEC, 90, P_REL, 30, P_LEVL, 82, P_GATE, 127);
    SET(p, 4, SA, 45, SB, 113, SC, 44, SD, 127, SE, 24, SF, 86, SG, 8, P_HOLD, 6, P_DEC, 62, P_REL, 1, P_DIST, 69,
        P_VOL, 94, P_PAN, 58, P_BASE, 12, P_WDTH, 50, P_HPQ, 20, P_LPQ, 12, P_FATK, 41, P_FDEC, 43, P_BOF, 46,
        P_WOF, 51, P_EQF, 26, P_LEVL, 127, P_GATE, 24, L3 + L_DEST, 18, L3 + L_WAVE, 9, L3 + L_TRIG, 12,
        L3 + L_MULT, 8, L3 + L_SPD, 4, L3 + L_DPTH, 86, P_MODT, 64, P_MODR, 8, P_MODD, 58, P_MODM, 69);
    SET(p, 5, SA, 98, SB, 63, SC, 105, SD, 8, SE, 109, SF, 19, SG, 55, P_HOLD, 7, P_DEC, 65, P_REL, 75, P_DIST, 66,
        P_BASE, 9, P_WDTH, 54, P_HPQ, 7, P_LPQ, 49, P_FATK, 10, P_FDEC, 54, P_WOF, 66, P_DTIM, 48, P_DSND, 88,
        P_DFB, 43, P_DWID, 92, P_SPU, 22, P_LEVL, 87, P_GATE, 30, L3 + L_DEST, 6, L3 + L_WAVE, 9, L3 + L_TRIG, 12,
        L3 + L_MULT, 8, L3 + L_SPD, 8, L3 + L_DPTH, 78);
    p->glob[G_SWNG] = 10;
    p->glob[G_TAPE] = 79;
    p->glob[G_VERB] = 62;
    p->glob[G_DAMP] = 81;
    p->glob[G_HISS] = 10;
    static const uint8_t A01_TRIGS[] = {   /* track, step, note, flags, condition, retrigs */
        0,0,33,1,0,0, 0,7,33,1,0,0, 0,10,33,1,0,0, 0,16,33,1,0,0, 0,23,33,1,0,0, 0,26,33,1,0,0, 0,32,33,1,0,0,
        0,39,33,1,0,0, 0,42,33,1,0,0, 0,48,33,1,0,0, 0,55,33,1,0,0, 0,58,33,1,0,0, 0,61,33,1,14,0,
        1,4,47,1,16,0, 1,12,47,1,16,0, 1,20,47,1,16,0, 1,28,47,1,16,0, 1,36,47,1,16,0, 1,44,47,1,16,0, 1,52,47,1,16,0,
        1,60,47,1,16,0, 1,62,47,1,14,0, 1,63,47,1,7,0,
        2,2,57,1,16,0, 2,6,57,1,16,0, 2,10,57,1,16,0, 2,14,57,1,16,0, 2,18,57,1,16,0, 2,22,57,1,16,0, 2,26,57,1,16,0,
        2,29,57,1,2,0, 2,30,57,1,16,0, 2,34,57,1,16,0, 2,38,57,1,16,0, 2,42,57,1,16,0, 2,46,57,1,16,0, 2,50,57,1,16,0,
        2,54,57,1,16,0, 2,56,57,1,2,0, 2,58,57,1,16,0, 2,62,57,1,14,0,
        3,2,33,1,0,0, 3,8,33,1,0,0, 3,11,40,1,0,0, 3,14,43,1,0,0, 3,18,33,1,0,0, 3,24,33,1,0,0, 3,27,36,1,0,0,
        3,30,31,1,0,0, 3,34,29,1,0,0, 3,40,29,1,0,0, 3,43,36,1,0,0, 3,46,33,1,0,0, 3,50,31,1,0,0, 3,56,31,1,0,0,
        3,59,38,1,0,0, 3,60,43,1,14,0, 3,62,35,1,0,0,
        4,6,60,1,0,0, 4,29,60,1,6,0, 4,31,60,1,7,0, 4,38,64,1,0,0, 4,57,62,1,16,0, 4,61,62,1,0,0,
        5,0,60,1,7,0, 5,3,71,1,7,0, 5,5,71,1,13,0, 5,7,67,1,7,0, 5,10,69,1,7,0, 5,13,60,1,13,0, 5,16,60,1,7,0,
        5,19,71,1,7,0, 5,21,71,1,13,0, 5,23,67,1,7,0, 5,26,69,1,7,0, 5,29,60,1,13,0, 5,32,64,1,7,0, 5,35,72,1,7,0,
        5,37,72,1,13,0, 5,39,65,1,7,0, 5,42,69,1,7,0, 5,45,64,1,13,0, 5,48,62,1,7,0, 5,51,71,1,7,0, 5,53,71,1,13,0,
        5,55,64,1,7,0, 5,58,67,1,7,0, 5,61,62,1,13,0,
    };
    trigs(p, A01_TRIGS, sizeof A01_TRIGS);
    static const uint8_t A01_LOCKS[] = {   /* track, step, parameter, value */
        1,63,13,36, 1,62,13,44, 2,29,13,34, 2,56,13,34, 2,62,7,30, 3,2,9,40, 3,2,10,90, 3,8,9,5, 3,8,10,44, 3,11,9,5,
        3,11,10,44, 3,14,9,5, 3,14,10,44, 3,18,9,40, 3,18,10,90, 3,24,9,5, 3,24,10,44, 3,27,9,5, 3,27,10,44,
        3,30,9,12, 3,30,10,44, 3,34,9,40, 3,34,10,90, 3,40,9,5, 3,40,10,44, 3,43,9,5, 3,43,10,44, 3,46,9,12,
        3,46,10,44, 3,50,9,40, 3,50,10,90, 3,56,9,5, 3,56,10,44, 3,59,9,5, 3,59,10,44, 3,62,9,12, 3,62,10,44,
        3,60,9,5, 3,60,10,44, 4,6,0,50, 4,6,1,100, 4,6,2,120, 4,29,0,50, 4,29,1,100, 4,29,2,120, 4,31,0,50,
        4,31,1,100, 4,31,2,120, 4,38,0,23, 4,38,1,65, 4,38,2,108, 4,57,0,23, 4,57,1,69, 4,57,2,112, 4,61,0,23,
        4,61,1,69, 4,61,2,112,
    };
    locks(p, A01_LOCKS, sizeof A01_LOCKS);

    /* A02 TIDE. E dorian at 122. The ENS pad on Em7 and A9, two bars each, its filter over 16 bars; the kick on the
     * beat and on the root; the snare from the second pass; an FM bass off the beat with an octave; FM+STAT keys
     * answering, ending two ways; the hats on the third and fourth passes */
    p = part(1, "TIDE", 122, MACHINES(M_ENS, M_DKIK, M_DSNR, M_FMD, M_FMS, M_DHAT));
    SET(p, 0, SD, 82, SE, 56, SF, 44, SG, 71, P_ATK, 37, P_HOLD, 127, P_DEC, 127, P_REL, 114, P_BASE, 10, P_WDTH, 46,
        P_LPQ, 51, P_FATK, 18, P_FDEC, 95, P_WOF, 80, P_SPU, 52, P_LEVL, 112, P_GATE, 127, L1 + L_DEST, 18,
        L1 + L_WAVE, 27, L1 + L_TRIG, 12, L1 + L_MULT, 24, L1 + L_SPD, 16, L1 + L_DPTH, 52, L3 + L_DEST, 18,
        L3 + L_WAVE, 9, L3 + L_TRIG, 12, L3 + L_MULT, 8, L3 + L_SPD, 4, L3 + L_DPTH, 84, P_MODT, 21, P_MODR, 18,
        P_MODD, 83, P_MODM, 58);
    SET(p, 1, SA, 2, SB, 22, SC, 8, SD, 12, SE, 34, SF, 27, SG, 72, SH, 76, P_HOLD, 40, P_DEC, 78, P_REL, 8,
        P_BASE, 22, P_WDTH, 115, P_HPQ, 51, P_LPQ, 63, P_FATK, 55, P_FDEC, 74, P_BOF, 33, P_WOF, 56, P_EQF, 93,
        P_EQG, 41, P_LEVL, 61, P_GATE, 15, P_MODT, 106, P_MODR, 113, P_MODD, 96);
    SET(p, 2, SA, 16, SB, 0, SC, 0, SD, 100, SE, 80, P_DEC, 28, P_REL, 14, P_BASE, 8, P_WDTH, 52, P_HPQ, 18, P_LPQ, 9,
        P_FATK, 23, P_FDEC, 28, P_EQF, 72, P_EQG, 53, P_SPU, 24, P_LEVL, 115, P_GATE, 23, L1 + L_DEST, 34,
        L1 + L_WAVE, 45, L1 + L_TRIG, 12, L1 + L_MULT, 72, L1 + L_SPD, 64, L1 + L_DPTH, 40, P_MODT, 21, P_MODR, 65,
        P_MODD, 41);
    SET(p, 3, SA, 96, SC, 112, SD, 28, SE, 64, SF, 59, SG, 111, P_HOLD, 8, P_DEC, 109, P_REL, 75, P_VOL, 112,
        P_BASE, 4, P_WDTH, 46, P_LPQ, 20, P_FATK, 51, P_FDEC, 117, P_BOF, 35, P_WOF, 18, P_EQF, 125, P_EQG, 51,
        P_LEVL, 127, P_GATE, 37, P_MODT, 21, P_MODR, 48, P_MODD, 47);
    SET(p, 4, SA, 98, SB, 63, SC, 105, SD, 8, SE, 109, SF, 19, SG, 55, P_HOLD, 7, P_DEC, 65, P_REL, 75, P_DIST, 66,
        P_BASE, 9, P_WDTH, 54, P_HPQ, 7, P_LPQ, 49, P_FATK, 10, P_FDEC, 54, P_WOF, 66, P_DTIM, 48, P_DSND, 88,
        P_DFB, 43, P_DWID, 92, P_SPU, 22, P_LEVL, 85, P_GATE, 30);
    SET(p, 5, SA, 119, SB, 70, SC, 10, SD, 84, SE, 100, SF, 117, SG, 12, P_HOLD, 7, P_DEC, 53, P_REL, 13, P_VOL, 76,
        P_PAN, 90, P_BASE, 50, P_WDTH, 40, P_HPQ, 23, P_FATK, 21, P_FDEC, 20, P_BOF, 71, P_WOF, 60, P_EQF, 45,
        P_EQG, 71, P_DFB, 49, P_DWID, 45, P_LEVL, 127, P_GATE, 25, L1 + L_DEST, 11, L1 + L_WAVE, 9, L1 + L_TRIG, 12,
        L1 + L_MULT, 56, L1 + L_DPTH, 28, P_MODT, 106, P_MODR, 32, P_MODD, 126);
    p->glob[G_SWNG] = 20;
    p->glob[G_VERB] = 96;
    p->glob[G_DAMP] = 68;
    p->glob[G_HISS] = 10;
    static const uint8_t A02_TRIGS[] = {   /* track, step, note, flags, condition, retrigs */
        0,0,64,1,0,0, 0,8,64,1,0,0, 0,16,64,1,0,0, 0,24,64,1,0,0, 0,32,61,1,0,0, 0,40,61,1,0,0, 0,48,61,1,0,0,
        0,56,61,1,0,0,
        1,0,40,1,0,0, 1,4,40,1,0,0, 1,8,40,1,0,0, 1,12,40,1,0,0, 1,16,40,1,0,0, 1,20,40,1,0,0, 1,24,40,1,0,0,
        1,28,40,1,0,0, 1,32,40,1,0,0, 1,36,40,1,0,0, 1,40,40,1,0,0, 1,44,40,1,0,0, 1,48,40,1,0,0, 1,52,40,1,0,0,
        1,56,40,1,0,0, 1,60,40,1,0,0,
        2,4,59,1,16,0, 2,12,59,1,16,0, 2,20,59,1,16,0, 2,28,59,1,16,0, 2,36,59,1,16,0, 2,44,59,1,16,0, 2,52,59,1,16,0,
        2,60,59,1,16,0, 2,63,59,1,7,0,
        3,2,28,1,0,0, 3,6,28,1,0,0, 3,10,40,1,0,0, 3,14,28,1,0,0, 3,18,28,1,0,0, 3,22,28,1,0,0, 3,26,40,1,0,0,
        3,30,28,1,0,0, 3,34,33,1,0,0, 3,38,33,1,0,0, 3,42,45,1,0,0, 3,46,33,1,0,0, 3,50,33,1,0,0, 3,54,33,1,0,0,
        3,58,45,1,0,0, 3,62,33,1,0,0,
        4,3,71,1,0,0, 4,6,67,1,0,0, 4,11,74,1,0,0, 4,19,71,1,7,0, 4,27,69,1,7,0, 4,35,73,1,0,0, 4,38,71,1,0,0,
        4,43,76,1,6,0, 4,44,69,1,7,0, 4,51,73,1,7,0, 4,59,71,1,7,0,
        5,2,52,1,14,0, 5,6,52,1,14,0, 5,10,52,1,14,0, 5,14,52,1,14,0, 5,18,52,1,14,0, 5,22,52,1,14,0, 5,26,52,1,14,0,
        5,29,52,1,14,0, 5,30,52,1,14,0, 5,34,52,1,14,0, 5,38,52,1,14,0, 5,42,52,1,14,0, 5,46,52,1,14,0,
        5,50,52,1,14,0, 5,54,52,1,14,0, 5,56,52,1,14,0, 5,58,52,1,14,0, 5,62,52,1,14,0,
    };
    trigs(p, A02_TRIGS, sizeof A02_TRIGS);
    static const uint8_t A02_LOCKS[] = {   /* track, step, parameter, value */
        0,0,0,11, 0,0,1,100, 0,0,2,116, 0,8,0,11, 0,8,1,100, 0,8,2,116, 0,16,0,11, 0,16,1,100, 0,16,2,116, 0,24,0,11,
        0,24,1,100, 0,24,2,116, 0,32,0,27, 0,32,1,108, 0,32,2,116, 0,40,0,27, 0,40,1,108, 0,40,2,116, 0,48,0,27,
        0,48,1,108, 0,48,2,116, 0,56,0,27, 0,56,1,108, 0,56,2,116, 2,63,13,36, 5,29,13,34, 5,56,13,34, 5,62,7,30,
    };
    locks(p, A02_LOCKS, sizeof A02_LOCKS);

    /* A03 SUSPEND. D at 110. Sus2 chords moved whole (Dsus2 Bbsus2 Gsus2) on the ENS pad, their 3rd coming in every
     * other pass (add9), its filter over 16 bars; a sparse kick; a sine rim from the second pass; a sine sub held,
     * walking down at the end of the fourth; FM+STAT keys on each chord's 2nd, 5th and 6th, a note added every other
     * pass; the cymbal on the second and fourth */
    p = part(2, "SUSPEND", 110, MACHINES(M_ENS, M_DKIK, M_GSIN, M_GSIN, M_FMS, M_DCYM));
    SET(p, 0, SD, 82, SE, 56, SF, 44, SG, 71, P_ATK, 20, P_HOLD, 127, P_DEC, 127, P_REL, 114, P_BASE, 10, P_WDTH, 42,
        P_LPQ, 51, P_FATK, 18, P_FDEC, 95, P_WOF, 80, P_SPU, 52, P_LEVL, 110, P_GATE, 127, L1 + L_DEST, 18,
        L1 + L_WAVE, 27, L1 + L_TRIG, 12, L1 + L_MULT, 24, L1 + L_SPD, 16, L1 + L_DPTH, 52, L3 + L_DEST, 18,
        L3 + L_WAVE, 9, L3 + L_TRIG, 12, L3 + L_MULT, 8, L3 + L_SPD, 4, L3 + L_DPTH, 82, P_MODT, 21, P_MODR, 18,
        P_MODD, 83, P_MODM, 58);
    SET(p, 1, SA, 2, SB, 22, SC, 8, SD, 12, SE, 34, SF, 27, SG, 72, SH, 76, P_HOLD, 40, P_DEC, 78, P_REL, 8,
        P_BASE, 22, P_WDTH, 115, P_HPQ, 51, P_LPQ, 63, P_FATK, 55, P_FDEC, 74, P_BOF, 33, P_WOF, 56, P_EQF, 93,
        P_EQG, 41, P_LEVL, 64, P_GATE, 15, P_MODT, 106, P_MODR, 113, P_MODD, 96);
    SET(p, 2, SA, 4, SB, 31, SC, 15, SD, 5, SE, 60, SF, 2, SG, 44, P_HOLD, 27, P_DEC, 53, P_REL, 89, P_PAN, 75,
        P_BASE, 23, P_WDTH, 34, P_HPQ, 24, P_LPQ, 109, P_FATK, 35, P_FDEC, 120, P_BOF, 43, P_WOF, 24, P_EQG, 37,
        P_FOLD, 34, P_XOR, 8, P_LEVL, 104, P_GATE, 23, P_MODT, 106, P_MODR, 36, P_MODD, 74, P_MODM, 46);
    SET(p, 3, P_ATK, 10, P_HOLD, 127, P_DEC, 127, P_REL, 70, P_LEVL, 77, P_GATE, 127);
    SET(p, 4, SA, 82, SB, 62, SC, 74, SD, 6, SF, 22, SG, 49, P_DEC, 57, P_REL, 78, P_DIST, 86, P_BASE, 1, P_WDTH, 59,
        P_HPQ, 10, P_LPQ, 54, P_FATK, 19, P_FDEC, 90, P_DTIM, 32, P_DSND, 87, P_DFB, 33, P_DWID, 65, P_SPU, 63,
        P_LEVL, 90, P_GATE, 12, L1 + L_DEST, 7, L1 + L_WAVE, 9, L1 + L_TRIG, 12, L1 + L_MULT, 8, L1 + L_DPTH, 38,
        P_MODT, 21, P_MODR, 24, P_MODD, 90, P_MODM, 86);
    SET(p, 5, SA, 73, SB, 124, SC, 51, SD, 104, SE, 65, SF, 6, SG, 58, SH, 9, P_HOLD, 8, P_DEC, 81, P_REL, 69,
        P_PAN, 86, P_BASE, 77, P_WDTH, 78, P_HPQ, 55, P_LPQ, 73, P_FATK, 19, P_FDEC, 9, P_BOF, 44, P_WOF, 88,
        P_EQF, 113, P_EQG, 85, P_FOLD, 30, P_SPU, 40, P_LEVL, 51, P_GATE, 36);
    p->glob[G_SWNG] = 20;
    p->glob[G_VERB] = 96;
    p->glob[G_DAMP] = 68;
    p->glob[G_HISS] = 10;
    static const uint8_t A03_TRIGS[] = {   /* track, step, note, flags, condition, retrigs */
        0,0,57,1,6,0, 0,1,57,1,7,0, 0,16,58,1,6,0, 0,17,58,1,7,0, 0,32,62,1,6,0, 0,33,57,1,7,0, 0,48,62,1,6,0,
        0,49,57,1,7,0,
        1,0,38,1,0,0, 1,10,38,1,0,0, 1,16,38,1,0,0, 1,26,38,1,0,0, 1,32,38,1,0,0, 1,42,38,1,0,0, 1,48,38,1,0,0,
        1,55,38,1,16,0, 1,58,38,1,0,0,
        2,4,69,1,16,0, 2,12,69,1,16,0, 2,20,69,1,16,0, 2,28,69,1,16,0, 2,36,69,1,16,0, 2,44,69,1,16,0, 2,52,69,1,16,0,
        2,60,69,1,16,0, 2,63,69,1,14,0,
        3,0,26,1,0,0, 3,16,34,1,0,0, 3,32,31,1,0,0, 3,48,31,1,0,0, 3,56,31,1,14,0, 3,60,29,1,14,0,
        4,2,64,1,0,0, 4,6,69,1,0,0, 4,9,71,1,0,0, 4,13,76,1,7,0, 4,18,60,1,0,0, 4,22,65,1,0,0, 4,25,67,1,0,0,
        4,29,72,1,7,0, 4,34,69,1,0,0, 4,38,74,1,0,0, 4,41,76,1,0,0, 4,45,81,1,7,0, 4,50,69,1,0,0, 4,54,74,1,0,0,
        4,57,76,1,0,0, 4,61,81,1,7,0, 4,62,76,1,13,0,
        5,0,57,1,11,0, 5,2,57,1,7,0, 5,6,57,1,7,0, 5,10,57,1,7,0, 5,14,57,1,7,0, 5,18,57,1,7,0, 5,22,57,1,7,0,
        5,26,57,1,7,0, 5,30,57,1,7,0, 5,34,57,1,7,0, 5,38,57,1,7,0, 5,42,57,1,7,0, 5,46,57,1,7,0, 5,50,57,1,7,0,
        5,54,57,1,7,0, 5,58,57,1,7,0, 5,62,57,1,7,0,
    };
    trigs(p, A03_TRIGS, sizeof A03_TRIGS);
    static const uint8_t A03_LOCKS[] = {   /* track, step, parameter, value */
        0,0,0,23, 0,0,1,93, 0,0,2,100, 0,16,0,31, 0,16,1,69, 0,16,2,125, 0,32,0,23, 0,32,1,93, 0,32,2,100, 0,48,0,23,
        0,48,1,93, 0,48,2,100, 0,1,0,50, 0,1,1,93, 0,1,2,100, 0,17,0,31, 0,17,1,69, 0,17,2,85, 0,33,0,54, 0,33,1,69,
        0,33,2,93, 0,49,0,54, 0,49,1,69, 0,49,2,93, 2,63,13,44, 5,0,7,90,
    };
    locks(p, A03_LOCKS, sizeof A03_LOCKS);

    /* A04 LANTERN. A at 100, swung. The ENS chord hits on add9 chords moved whole (Aadd9 Fadd9 Cadd9 Gadd9), their
     * filter over 16 bars; the kick and a sine rim; a sine sub on the roots; FM+STAT keys on each chord's 3rd, 5th and
     * 9th; the cymbal from the third pass */
    p = part(3, "LANTERN", 100, MACHINES(M_ENS, M_DKIK, M_GSIN, M_GSIN, M_FMS, M_DCYM));
    SET(p, 0, SA, 45, SB, 113, SC, 44, SD, 127, SE, 24, SF, 86, SG, 8, P_HOLD, 6, P_DEC, 62, P_REL, 1, P_DIST, 69,
        P_VOL, 127, P_PAN, 58, P_BASE, 51, P_WDTH, 105, P_HPQ, 20, P_LPQ, 12, P_FATK, 41, P_FDEC, 43, P_BOF, 46,
        P_WOF, 51, P_EQF, 26, P_LEVL, 127, P_GATE, 24, L3 + L_DEST, 18, L3 + L_WAVE, 9, L3 + L_TRIG, 12,
        L3 + L_MULT, 8, L3 + L_SPD, 4, L3 + L_DPTH, 84, P_MODT, 64, P_MODR, 8, P_MODD, 58, P_MODM, 69);
    SET(p, 1, SA, 2, SB, 22, SC, 8, SD, 12, SE, 34, SF, 27, SG, 72, SH, 76, P_HOLD, 40, P_DEC, 78, P_REL, 8,
        P_BASE, 22, P_WDTH, 115, P_HPQ, 51, P_LPQ, 63, P_FATK, 55, P_FDEC, 74, P_BOF, 33, P_WOF, 56, P_EQF, 93,
        P_EQG, 41, P_LEVL, 72, P_GATE, 15, P_MODT, 106, P_MODR, 113, P_MODD, 96);
    SET(p, 2, SA, 4, SB, 31, SC, 15, SD, 5, SE, 60, SF, 2, SG, 44, P_HOLD, 27, P_DEC, 53, P_REL, 89, P_PAN, 75,
        P_BASE, 23, P_WDTH, 34, P_HPQ, 24, P_LPQ, 109, P_FATK, 35, P_FDEC, 120, P_BOF, 43, P_WOF, 24, P_EQG, 37,
        P_FOLD, 34, P_XOR, 8, P_LEVL, 117, P_GATE, 23, P_MODT, 106, P_MODR, 36, P_MODD, 74, P_MODM, 46);
    SET(p, 3, P_ATK, 10, P_HOLD, 127, P_DEC, 127, P_REL, 60, P_LEVL, 70, P_GATE, 100);
    SET(p, 4, SA, 82, SB, 62, SC, 74, SD, 6, SF, 22, SG, 49, P_DEC, 57, P_REL, 78, P_DIST, 86, P_BASE, 1, P_WDTH, 59,
        P_HPQ, 10, P_LPQ, 54, P_FATK, 19, P_FDEC, 90, P_DTIM, 32, P_DSND, 87, P_DFB, 33, P_DWID, 65, P_SPU, 63,
        P_LEVL, 98, P_GATE, 12, L1 + L_DEST, 7, L1 + L_WAVE, 9, L1 + L_TRIG, 12, L1 + L_MULT, 8, L1 + L_DPTH, 38,
        P_MODT, 21, P_MODR, 24, P_MODD, 90, P_MODM, 86);
    SET(p, 5, SA, 73, SB, 124, SC, 51, SD, 104, SE, 65, SF, 6, SG, 58, SH, 9, P_HOLD, 8, P_DEC, 81, P_REL, 69,
        P_PAN, 86, P_BASE, 77, P_WDTH, 78, P_HPQ, 55, P_LPQ, 73, P_FATK, 19, P_FDEC, 9, P_BOF, 44, P_WOF, 88,
        P_EQF, 113, P_EQG, 85, P_FOLD, 30, P_SPU, 40, P_LEVL, 56, P_GATE, 36);
    p->glob[G_SWNG] = 30;
    p->glob[G_ADPC] = 7;
    p->glob[G_VERB] = 26;
    p->glob[G_DAMP] = 56;
    p->glob[G_HISS] = 10;
    static const uint8_t A04_TRIGS[] = {   /* track, step, note, flags, condition, retrigs */
        0,0,61,1,0,0, 0,6,61,1,0,0, 0,16,60,1,0,0, 0,22,60,1,0,0, 0,32,62,1,0,0, 0,38,62,1,0,0, 0,48,62,1,0,0,
        0,54,62,1,0,0,
        1,0,33,1,0,0, 1,7,33,1,0,0, 1,10,33,1,0,0, 1,16,33,1,0,0, 1,23,33,1,0,0, 1,26,33,1,0,0, 1,32,33,1,0,0,
        1,39,33,1,0,0, 1,42,33,1,0,0, 1,48,33,1,0,0, 1,55,33,1,0,0, 1,58,33,1,0,0,
        2,4,76,1,0,0, 2,12,76,1,0,0, 2,20,76,1,0,0, 2,28,76,1,0,0, 2,36,76,1,0,0, 2,44,76,1,0,0, 2,52,76,1,0,0,
        2,60,76,1,0,0,
        3,0,33,1,0,0, 3,10,33,1,0,0, 3,16,41,1,0,0, 3,26,41,1,0,0, 3,32,36,1,0,0, 3,42,36,1,0,0, 3,48,43,1,0,0,
        3,58,43,1,0,0,
        4,3,61,1,0,0, 4,8,64,1,0,0, 4,11,71,1,0,0, 4,14,69,1,7,0, 4,19,69,1,0,0, 4,24,72,1,0,0, 4,27,79,1,0,0,
        4,30,77,1,7,0, 4,35,64,1,0,0, 4,40,67,1,0,0, 4,43,74,1,0,0, 4,46,72,1,7,0, 4,51,71,1,0,0, 4,56,74,1,0,0,
        4,59,81,1,0,0, 4,62,79,1,7,0,
        5,2,64,1,14,0, 5,6,64,1,14,0, 5,8,64,1,2,0, 5,10,64,1,14,0, 5,14,64,1,14,0, 5,18,64,1,14,0, 5,22,64,1,14,0,
        5,24,64,1,2,0, 5,26,64,1,14,0, 5,30,64,1,14,0, 5,34,64,1,14,0, 5,38,64,1,14,0, 5,40,64,1,2,0, 5,42,64,1,14,0,
        5,46,64,1,14,0, 5,50,64,1,14,0, 5,54,64,1,14,0, 5,56,64,1,2,0, 5,58,64,1,14,0, 5,62,64,1,14,0,
    };
    trigs(p, A04_TRIGS, sizeof A04_TRIGS);
    static const uint8_t A04_LOCKS[] = {   /* track, step, parameter, value */
        0,0,0,38, 0,0,1,73, 0,0,2,116, 0,6,0,38, 0,6,1,73, 0,6,2,116, 0,16,0,50, 0,16,1,93, 0,16,2,100, 0,22,0,50,
        0,22,1,93, 0,22,2,100, 0,32,0,54, 0,32,1,69, 0,32,2,93, 0,38,0,54, 0,38,1,69, 0,38,2,93, 0,48,0,50, 0,48,1,93,
        0,48,2,100, 0,54,0,50, 0,54,1,93, 0,54,2,100, 5,8,13,40, 5,24,13,40, 5,40,13,40, 5,56,13,40,
    };
    locks(p, A04_LOCKS, sizeof A04_LOCKS);
    /* (A05-A16 have none: blank at the factory, the player's after; DEMO brings these four back alone) */
    /* the four as the owner approved them, written on format 15's amp and LFO laws: onto 16's, once */
    for (int i = 0; i < 4; i++) feel_v15(PAT(i));
}
