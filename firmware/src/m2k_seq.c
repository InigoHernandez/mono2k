/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's sequencer: six tracks of up to 64 steps, each step a trig with its note, accent and slide, and
 * parameter locks: any track parameter set for one step only, back to the kit on the next trig (as on the
 * Elektron machines). Swing, a pattern queued for the end of the bar, live recording, an arpeggiator per track. */

static struct {
    uint8_t playing, step, queued, rec;   /* step: the next to play (at the end, the wrap waits for it) */
    uint8_t ready;                     /* REC on, PLAY tapped stopped: the first note starts it, recorded (Demake's) */
    uint8_t full;                      /* a pattern queued could not open (the FM-1's patterns full): for the screen */
    uint8_t jump;                      /* the queued pattern now, at the next step (HOME + its key again) */
    uint8_t solo;                      /* the tracks soloed while GLO + white 9-14 are held (0 none) */
    uint8_t fill_next, fill_auto;      /* a fill for the next loop (GLO + white 7), and that loop's own */
    /* the tracks off 1x: each on its own step and clock (Q8 samples), over the pattern's length, from its start */
    uint8_t ts[M2K_TRACKS];
    uint32_t tk[M2K_TRACKS];           /* (their steps played since the pattern's start) */
    int32_t tctr[M2K_TRACKS];
    uint8_t tpat;                      /* the pattern those clocks belong to (a new one starts them over) */
    uint8_t tspd[M2K_TRACKS];          /* each track's speed as it last ran (turned: on from where the pattern is) */
    uint8_t relock;                    /* a continue: the next step puts them back where the pattern is */
    uint32_t n1x;                      /* the pattern's steps played since its start: theirs follow from it */
    int32_t ctr;                       /* Q8 samples to the next step (under the clock in, as expected: for REC) */
    uint32_t lockmap[M2K_TRACKS][2];   /* steps with locks, for the screen */
    /* live play */
    uint8_t held[8], nheld;            /* the keys held for the current track, in order */
    uint8_t ktrk;                      /* the track the keys play */
    int8_t rec_skip[M2K_TRACKS];       /* a step just recorded ahead: do not play it twice */
    /* trig conditions and ratchets */
    uint16_t loops[M2K_TRACKS];        /* each track's loops over its steps since play, or since its pattern came in */
    uint8_t fill, pre[M2K_TRACKS];
    uint8_t rt_left[M2K_TRACKS], rt_note[M2K_TRACKS], rt_skip[M2K_TRACKS];
    int32_t rt_ctr[M2K_TRACKS], rt_len[M2K_TRACKS];
    /* arp */
    int32_t arp_ctr;
    uint8_t arp_i, arp_dir;
    uint32_t tick;                     /* control blocks since start */
} sq;

/* MIDI (GLO > MIDI): the clock in drives the sequencer (its tempo, a step every sixth pulse), the clock out and the
 * notes go out stamped with the sample they belong to (m2k_midi_out hands them over as the sound reaches it) */
typedef struct { uint32_t at; uint8_t m[3], n; } mxq_t;   /* a message out, and the sample it belongs to */

static struct {
    uint32_t now;                      /* samples rendered since init */
    uint32_t last, per_q8;             /* the last pulse in, and the samples between pulses, Q8, smoothed (0: none) */
    uint32_t pulses;                   /* pulses in since start */
    uint8_t ext;                       /* playing to the clock in */
    uint8_t arm;                       /* a step armed by its pulse, */
    int32_t arm_ctr;                   /* due in this many Q8 samples (an odd step's swing) */
    int32_t out_ctr;                   /* Q8 samples to the next pulse out */
    uint8_t out_n;                     /* that pulse's place in its step (six add up to the step exactly) */
    uint8_t on[M2K_TRACKS], onch[M2K_TRACKS];   /* the note each track holds out (0xFF none), and its channel */
    uint8_t gend[M2K_TRACKS];          /* a track's gate ended in this block (marked on its core) */
    mxq_t q[64];
    uint8_t qr, qw;
    uint32_t in_at[2];                 /* the last message in from USB, from TRS (GLO > IN shows them), */
    uint8_t in_seen;                   /* once one has come: bit 0 USB, bit 1 TRS */
} mx;

static inline int set_on(int i) { return store_.set[i] >= 64; }
static inline int midi_ch(void) { return (store_.set[S_MCH] * 16) >> 7; }

/* where the clock in and the notes are taken from: bit 0 USB, bit 1 TRS. SYIN's four (OFF USB TRS BOTH) are that mask;
 * its knob writes them at 16 48 80 112 and they read with edges at 40 64 88, so the old switch's OFF (0, 32) stays
 * OFF and its ON (96, 127) takes both. NOTE IN's (BOTH USB TRS OFF) read evenly, so an old store's 0 takes both */
static inline int syin_choice(int v) { return v < 40 ? 0 : v < 64 ? 1 : v < 88 ? 2 : 3; }
static inline int clk_from(void) { return syin_choice(store_.set[S_SYNI]); }
static const uint8_t NTIN_FROM[4] = {3, 1, 2, 0};
static inline int notes_from(void) { return NTIN_FROM[(store_.set[S_NTIN] * 4) >> 7]; }

static void mx_out(int n, int a, int b, int c)
{
    if ((uint8_t)(mx.qw - mx.qr) >= 64) return;
    mxq_t *q = &mx.q[mx.qw++ & 63];
    q->at = mx.now; q->m[0] = (uint8_t)a; q->m[1] = (uint8_t)b; q->m[2] = (uint8_t)c; q->n = (uint8_t)n;
}

int m2k_midi_out(uint8_t msg[3], uint32_t played)
{
    if (mx.qr == mx.qw || (int32_t)(played - mx.q[mx.qr & 63].at) < 0) return 0;
    const uint8_t n = mx.q[mx.qr & 63].n;
    m_cpy(msg, mx.q[mx.qr & 63].m, 3);
    mx.qr++;
    return n;
}

uint32_t m2k_samples(void) { return mx.now; }

static void mx_note_off(int t)
{
    if (mx.on[t] == 0xFF) return;
    mx_out(3, 0x80 | mx.onch[t], mx.on[t], 0);       /* on the channel it went out on (CHAN may have turned) */
    mx.on[t] = 0xFF;
}

static void mx_note_on(int t, int note, int vel)
{
    mx_note_off(t);
    mx.onch[t] = (uint8_t)((midi_ch() + t) & 15);
    mx_out(3, 0x90 | mx.onch[t], note, vel);
    mx.on[t] = (uint8_t)note;
}

/* a gate ends in its track's block, on either core: only marked there, and sent by the first once both are done (the
 * queue is the first core's alone), with the same sample */
static void mx_gate_end(int ti) { mx.gend[ti] = 1; }

static void mx_gates_out(void)
{
    for (int t = 0; t < M2K_TRACKS; t++)
        if (mx.gend[t]) { mx.gend[t] = 0; mx_note_off(t); }
}

static inline const uint8_t *G(void) { return PAT(cur_pat)->glob; }
static inline uint32_t seq_bpm(void) { return ext_bpm ? ext_bpm : 30u + G()[G_TMPO]; }
static inline int seq_len(void) { int l = G()[G_LEN]; return l < 1 ? 1 : l > 64 ? 64 : l; }

static void seq_tempo(void)
{
    if (mx.ext && mx.per_q8) {         /* the clock in's: 24 pulses a beat, six a step */
        bpm10 = 282240000u / mx.per_q8;
        sps_q8 = mx.per_q8 * 6u;
        ext_bpm = (bpm10 + 5u) / 10u;
        return;
    }
    ext_bpm = 0;
    uint32_t b = seq_bpm();
    bpm10 = b * 10u;
    sps_q8 = 169344000u / b;
}

static int32_t step_len(int s)
{
    int32_t sw = 50 + (G()[G_SWNG] * 25) / 127;             /* percent of the pair for the even step */
    int32_t pair = (int32_t)(sps_q8 * 2u);                 /* <= 11.3 M: the product stays in 32 bits */
    int32_t even = (int32_t)(((uint32_t)pair * (uint32_t)sw) / 100u);
    return (s & 1) ? pair - even : even;
}

/* ------------------------------------------------------------------------------------------------- locks */

static void lockmap_build(void)
{
    m_set(sq.lockmap, 0, sizeof sq.lockmap);
    const m2k_pattern_t *p = PAT(cur_pat);
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk < M2K_TRACKS) sq.lockmap[p->lock[i].trk][p->lock[i].step >> 5] |= 1u << (p->lock[i].step & 31);
}

static int lock_find(int t, int s, int prm)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == t && p->lock[i].step == s && p->lock[i].param == prm) return i;
    return -1;
}

static int lock_set(int t, int s, int prm, int v)
{
    m2k_pattern_t *p = PAT(cur_pat);
    int i = lock_find(t, s, prm);
    if (i < 0)
        for (i = 0; i < M2K_LOCKS && p->lock[i].trk != 0xFF; i++)
            ;
    if (i >= M2K_LOCKS) return 0;
    p->lock[i] = (m2k_lock_t){(uint8_t)t, (uint8_t)s, (uint8_t)prm, (uint8_t)v};
    sq.lockmap[t][s >> 5] |= 1u << (s & 31);
    return 1;
}

static void lock_clear_step(int t, int s)
{
    m2k_pattern_t *p = PAT(cur_pat);
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == t && p->lock[i].step == s) p->lock[i].trk = 0xFF;
    sq.lockmap[t][s >> 5] &= ~(1u << (s & 31));
}

static inline int has_locks(int t, int s) { return (sq.lockmap[t][s >> 5] >> (s & 31)) & 1; }

static void apply_locks(int t, int s)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    m_cpy(trk[t].eff, p->kit[t], P_COUNT);
    if (!has_locks(t, s)) return;
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == t && p->lock[i].step == s) trk[t].eff[p->lock[i].param] = p->lock[i].val;
}

static int32_t gate_blocks(int t)
{
    int32_t gl = trk[t].eff[P_GATE];                /* the step's, a GATE lock with it (apply_locks) */
    if (gl < 1) gl = 1;
    return (int32_t)(((sps_q8 >> 8) * (uint32_t)gl) / (16u * M2K_SUB)) + 1;
}

/* --------------------------------------------------------------------------------------------- the clock */

/* does a trig with this condition play? A:B and 1ST count the track's own loops; PRE remembers the last condition
 * tested on the track */
static int cond_pass(int t, int c)
{
    static const uint8_t CHANCE[6] = {255, 192, 128, 85, 64, 32};
    static const uint8_t AB[8][2] = {{1, 2}, {2, 2}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}};
    int r;
    if (c == C_ALWAYS) return 1;
    if (c <= C_P12) r = (int)(rnd32() & 255) < CHANCE[c];
    else if (c <= C_4_4) { int i = c - C_1_2; int a = c == C_4_4 ? 4 : AB[i][0], b = c == C_4_4 ? 4 : AB[i][1]; r = sq.loops[t] % b == (unsigned)(a - 1); }
    else if (c == C_1ST) r = sq.loops[t] == 0;
    else if (c == C_N1ST) r = sq.loops[t] != 0;
    else if (c == C_PRE) return sq.pre[t];
    else if (c == C_NPRE) return !sq.pre[t];
    else if (c == C_FILL) r = sq.fill;
    else r = !sq.fill;
    sq.pre[t] = (uint8_t)r;
    return r;
}

/* a track's speed: 1/8x 1/4x 1/2x 3/4x 1x 3/2x 2x, its step that many times the pattern's (as a fraction, exact) */
static const uint8_t SPD_NUM[7] = {8, 4, 2, 4, 1, 2, 1}, SPD_DEN[7] = {1, 1, 1, 3, 1, 3, 2};
static inline int spd_of(int t) { return (G()[G_SPD1 + t] * 7) >> 7; }
static inline int32_t track_len(int t, int s) { int c = spd_of(t); return step_len(s) * SPD_NUM[c] / SPD_DEN[c]; }

/* the step track t plays (or last played): the pattern's, or its own off 1x */
static int track_pos(int t);

static void fire_track(int t, int s, int32_t len)
{
    m2k_pattern_t *p = PAT(cur_pat);
    {
        m2k_step_t st = p->step[t][s];
        if (!(st.flags & ST_TRIG) || (p->mute >> t & 1) || (sq.solo && !(sq.solo >> t & 1))) return;
        if (sq.rec_skip[t] == s) { sq.rec_skip[t] = -1; return; }
        if (sq.nheld && t == sq.ktrk) return;       /* the player has the track */
        if (!cond_pass(t, st.cond)) return;
        apply_locks(t, s);
        unsigned skip = st.flags & ST_TTYPE;
        voice_trig(t, st.note, (st.flags & ST_ACC) ? 127 : 100, st.flags & ST_SLD, skip);
        if (set_on(S_NOTO) && !(skip & ST_NOAMP)) mx_note_on(t, st.note, (st.flags & ST_ACC) ? 127 : 100);
        if (!(skip & ST_NOAMP)) {                    /* no AMP trig: the gate runs on as it was */
            int c = spd_of(t);
            trk[t].gate_left = c == 4 ? gate_blocks(t) : (gate_blocks(t) - 1) * SPD_NUM[c] / SPD_DEN[c] + 1;
        }
        int n = st.rtrg < 1 ? 1 : st.rtrg > 4 ? 4 : st.rtrg;
        sq.rt_left[t] = (uint8_t)(n - 1);
        if (n > 1) {
            sq.rt_len[t] = len / n;
            sq.rt_ctr[t] = sq.rt_len[t];
            sq.rt_note[t] = st.note;
            sq.rt_skip[t] = (uint8_t)skip;
            if (!(skip & ST_NOAMP)) trk[t].gate_left = sq.rt_len[t] / (M2K_SUB << 8) * 3 / 4 + 1;
        }
    }
}

static void fire_step(int s)                         /* the pattern's step: the tracks at 1x */
{
    for (int t = 0; t < M2K_TRACKS; t++) if (spd_of(t) == 4) fire_track(t, s, step_len(s));
    bus_step();
}

/* the tracks off 1x start over with the pattern, from their first step with its first */
static void tracks_restart(void)
{
    sq.tpat = (uint8_t)cur_pat;
    sq.n1x = 0;
    sq.relock = 0;
    m_set(sq.ts, 0, sizeof sq.ts);
    m_set(sq.tk, 0, sizeof sq.tk);
    m_set(sq.tctr, 0, sizeof sq.tctr);
    for (int t = 0; t < M2K_TRACKS; t++) sq.tspd[t] = (uint8_t)spd_of(t);
}

/* a track off 1x (speed c) meets the pattern where its step K starts with the pattern's P: as many steps of each kind
 * (the even, swung long, and the odd) gone by on both, in its time (without swing, any step of both) */
static inline uint32_t evens(uint32_t n, uint32_t len) { return n / len * ((len + 1) / 2) + (n % len + 1) / 2; }
static int track_meets(int c, uint32_t k, uint32_t p)
{
    if (k * SPD_NUM[c] != p * SPD_DEN[c]) return 0;
    uint32_t len = (uint32_t)seq_len();
    return step_len(0) == step_len(1) || evens(k, len) * SPD_NUM[c] == evens(p, len) * SPD_DEN[c];
}

/* track t, off 1x, put where the pattern is: its step P since the start, EL Q8 samples into it (of SL). The track's
 * step there starts now, or plays on (its trig gone by) and the next is due in what is left of it, swing aside;
 * PH: now, on the track's clock */
static void track_place(int t, uint32_t p, int32_t el, int32_t sl, int32_t ph)
{
    int c = spd_of(t), n = SPD_NUM[c], len = seq_len();
    int32_t e = el >> 8, l1 = sl >> 8;
    uint32_t f = e > 0 && l1 > 0 ? (uint32_t)(e < l1 ? e : l1 - 1) * 4096u / (uint32_t)l1 : 0;   /* Q12 of the step */
    uint32_t q = ((p % n) * 4096u + f) * SPD_DEN[c] / n, k = p / n * SPD_DEN[c] + (q >> 12);       /* the track's */
    int s = (int)(k % (uint32_t)len);
    sq.tspd[t] = (uint8_t)c;
    if (!(q & 4095)) { sq.tk[t] = k; sq.ts[t] = (uint8_t)s; sq.tctr[t] = ph; return; }
    int32_t l = track_len(t, s);
    sq.tk[t] = k + 1;
    sq.ts[t] = (uint8_t)(s + 1);
    sq.tctr[t] = ph + l - (int32_t)(((int64_t)l * (q & 4095)) >> 12);
}

/* the pattern's step P plays, at PH on the tracks' clocks: a track off 1x whose step starts with it plays it now, so
 * it never wanders off (under the clock in its own clock runs on a tempo measured); after a continue, every one takes
 * its place from it */
static void tracks_lock(uint32_t p, int32_t ph)
{
    int len = seq_len();
    for (int t = 0; t < M2K_TRACKS; t++) {
        int c = spd_of(t);
        if (c == 4) continue;
        if (sq.relock) { track_place(t, p, 0, 0, ph); continue; }
        uint32_t k = p * SPD_DEN[c] / SPD_NUM[c];
        if (!track_meets(c, k, p)) continue;
        int s = (int)(k % (uint32_t)len);
        sq.tspd[t] = (uint8_t)c;
        if (sq.tk[t] == k + 1) { sq.tctr[t] = ph + track_len(t, s); continue; }   /* it played it already */
        if (sq.tk[t] != k) {           /* lost (the tempo jumped): on from here, a wrap due left to come */
            sq.tk[t] = k;
            if (s || sq.ts[t] < len) sq.ts[t] = (uint8_t)s;
        }
        sq.tctr[t] = ph;
    }
    sq.relock = 0;
}

/* the tracks off 1x, each on its own clock; a step that starts with one of the pattern's waits for it. A new pattern
 * (or the start) sets them going from its first step; a speed turned takes the track on from where the pattern is */
static void tracks_off_1x(void)
{
    if (sq.tpat != cur_pat) tracks_restart();
    int len = seq_len();
    for (int t = 0; t < M2K_TRACKS; t++) {
        int c = spd_of(t);
        if (c != sq.tspd[t]) {
            sq.tspd[t] = (uint8_t)c;
            if (c != 4 && sq.n1x) {
                int32_t sl = step_len(sq.step ? sq.step - 1 : 0);
                track_place(t, sq.n1x - 1, sl - sq.ctr, sl, M2K_SUB << 8);
            }
        }
        if (c == 4 || sq.relock) continue;              /* (after a continue: placed by the first step) */
        sq.tctr[t] -= M2K_SUB << 8;
        while (sq.tctr[t] <= 0) {
            uint32_t k = sq.tk[t], p = k * SPD_NUM[c] / SPD_DEN[c];
            if (p >= sq.n1x && track_meets(c, k, p)) { sq.tctr[t] = 0; break; }
            if (sq.ts[t] >= len) { sq.ts[t] = 0; sq.loops[t]++; }
            int st = sq.ts[t];
            int32_t l = track_len(t, st);
            fire_track(t, st, l);
            sq.tctr[t] += l;
            sq.ts[t] = (uint8_t)(st + 1);
            sq.tk[t]++;
        }
    }
}

static int track_pos(int t)
{
    int s = (int)(spd_of(t) == 4 ? sq.step : sq.ts[t]) - 1;
    return s < 0 ? seq_len() - 1 : s;
}

/* the pattern's next step comes in another pattern (the queued one at the end, or jumped to) */
static inline int pat_coming_in(void) { return sq.queued != cur_pat && (sq.jump || sq.step >= seq_len()); }

/* the step that plays next */
static inline int step_due(void) { return sq.step >= seq_len() || (sq.jump && sq.queued != cur_pat) ? 0 : sq.step; }

/* right before the pattern's next step, the wrap: past the end (or a length made shorter), or a pattern jumped to,
 * it starts over, the queued pattern in. So the last step plays out on its own pattern, kit and locks, and a note
 * recorded in it stays there */
static void step_wrap(void)
{
    if (sq.step < seq_len() && !(sq.jump && sq.queued != cur_pat)) return;
    sq.jump = 0;
    sq.step = 0;
    for (int t = 0; t < M2K_TRACKS; t++) if (spd_of(t) == 4) sq.loops[t]++;
    if (sq.fill_next) { sq.fill = 1; sq.fill_next = 0; sq.fill_auto = 1; }   /* a loop of fill, then back */
    else if (sq.fill_auto) { sq.fill = 0; sq.fill_auto = 0; }
    if (sq.queued != cur_pat && !pat_can_open(sq.queued)) {   /* the FM-1's patterns full: this one plays on */
        sq.queued = (uint8_t)cur_pat;
        sq.full = 1;
    }
    if (sq.queued != cur_pat) {
        cur_pat = sq.queued;
        lockmap_build();
        for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, PAT(cur_pat)->kit[t], P_COUNT);
        m_set(sq.loops, 0, sizeof sq.loops);       /* its conditions count its own loops */
        tracks_restart();
    }
}

/* the pattern's next step, its trig at PH on the tracks' clocks; returns it */
static int step_fire(int32_t ph)
{
    step_wrap();
    int s = sq.step;
    fire_step(s);
    tracks_lock(sq.n1x++, ph);
    sq.step = (uint8_t)(s + 1);
    return s;
}

static void seq_block(void)
{
    sq.tick++;
    mx.now += M2K_SUB;
    seq_tempo();
    if (sq.playing && mx.ext) {        /* the clock in's pulse armed the step; an odd step waits for its swing */
        if (sq.ctr > 0) sq.ctr -= M2K_SUB << 8;   /* (the next step as expected, for REC; no further than due) */
        if (mx.arm && (mx.arm_ctr -= M2K_SUB << 8) <= 0) {
            mx.arm = 0;
            sq.ctr = step_len(step_fire(M2K_SUB << 8));
        }
        tracks_off_1x();                /* (off 1x: on the tempo the clock gives, put back on its steps) */
    } else if (sq.playing) {
        sq.ctr -= M2K_SUB << 8;
        while (sq.ctr <= 0) {
            int s = step_fire(sq.ctr + (M2K_SUB << 8));
            sq.ctr += step_len(s);
        }
        tracks_off_1x();
        if (set_on(S_SYNO)) {          /* the clock out: 24 a beat, on the grid (no swing), six to a step exactly */
            mx.out_ctr -= M2K_SUB << 8;
            while (mx.out_ctr <= 0) {
                mx_out(1, 0xF8, 0, 0);
                uint32_t n = mx.out_n;
                mx.out_ctr += (int32_t)(sps_q8 * (n + 1u) / 6u - sps_q8 * n / 6u);
                mx.out_n = (uint8_t)((n + 1u) % 6u);
            }
        }
    }
    /* ratchets: the rest of a step's hits, while it plays and the track is heard */
    const m2k_pattern_t *rp = PAT(cur_pat);
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (!sq.rt_left[t]) continue;
        if (!sq.playing || (rp->mute >> t & 1) || (sq.solo && !(sq.solo >> t & 1)) || (sq.nheld && t == sq.ktrk)) {
            sq.rt_left[t] = 0;
            continue;
        }
        sq.rt_ctr[t] -= M2K_SUB << 8;
        if (sq.rt_ctr[t] <= 0) {
            sq.rt_ctr[t] += sq.rt_len[t];
            sq.rt_left[t]--;
            voice_trig(t, sq.rt_note[t], 100, 0, sq.rt_skip[t]);
            if (!(sq.rt_skip[t] & ST_NOAMP)) trk[t].gate_left = sq.rt_len[t] / (M2K_SUB << 8) * 3 / 4 + 1;
        }
    }
    /* the arp, on its own clock */
    const uint8_t *k = PAT(cur_pat)->kit[sq.ktrk];
    int mode = (k[P_AMOD] * 6) >> 7;
    if (mode && sq.nheld) {
        static const uint8_t div[4] = {4, 8, 16, 32};    /* in 1/8 of a sixteenth (a step): 1/32 1/16 1/8 1/4, as shown */
        int32_t len = (int32_t)((sps_q8 * div[(k[P_ASPD] * 4) >> 7]) >> 3);
        sq.arp_ctr -= M2K_SUB << 8;
        if (sq.arp_ctr <= 0) {
            sq.arp_ctr += len;
            int rng = 1 + ((k[P_ARNG] * 4) >> 7);
            int n = sq.nheld * rng;
            uint8_t sorted[8];
            m_cpy(sorted, sq.held, sq.nheld);
            if (mode != 5)
                for (int i = 1; i < sq.nheld; i++)
                    for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; j--) { uint8_t x = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = x; }
            int i;
            switch (mode) {                 /* (the count kept within its cycle: a byte never wraps mid-cycle) */
            case 2: i = sq.arp_i % n; sq.arp_i = (uint8_t)((i + 1) % n); i = n - 1 - i; break;
            case 3: {
                int per = n > 1 ? 2 * n - 2 : 1, q = sq.arp_i % per;
                i = q < n ? q : per - q;
                sq.arp_i = (uint8_t)((q + 1) % per);
                break;
            }
            case 4: i = (int)(rnd32() % (uint32_t)n); break;
            default: i = sq.arp_i % n; sq.arp_i = (uint8_t)((i + 1) % n); break;
            }
            int note = sorted[i % sq.nheld] + 12 * (i / sq.nheld);
            m_cpy(trk[sq.ktrk].eff, PAT(cur_pat)->kit[sq.ktrk], P_COUNT);
            voice_on(sq.ktrk, note, 100, 0);
            trk[sq.ktrk].gate_left = gate_blocks(sq.ktrk) / 2 + 1;
        }
    }
}

static int seq_running(void) { return sq.playing; }

/* a start: from the first step, nothing kept of the last play (its loops, fill, PRE, a step recorded ahead, a jump,
 * a ratchet), the tracks off 1x with it */
static void seq_reset(void)
{
    sq.step = 0;
    sq.ctr = 0;
    m_set(sq.loops, 0, sizeof sq.loops);
    m_set(sq.pre, 0, sizeof sq.pre);
    m_set(sq.rt_left, 0, sizeof sq.rt_left);
    m_set(sq.rec_skip, 0xFF, sizeof sq.rec_skip);
    sq.fill = sq.fill_auto = 0;
    sq.jump = 0;
    tracks_restart();
    lockmap_build();
}

static void seq_play(int on)
{
    sq.ready = 0;                      /* (started, by a note or PLAY, or stopped: no longer waiting) */
    if (on && !sq.playing) {
        seq_reset();
        if (!mx.ext && set_on(S_SYNO)) {   /* a pulse with the first step */
            mx_out(1, 0xFA, 0, 0);
            mx.out_ctr = 0;
            mx.out_n = 0;
        }
    }
    if (!on) {
        for (int t = 0; t < M2K_TRACKS; t++) voice_off(t);          /* (their notes out end with them) */
        m_set(sq.rt_left, 0, sizeof sq.rt_left);                     /* a ratchet's hits too */
        if (sq.playing && !mx.ext && set_on(S_SYNO)) mx_out(1, 0xFC, 0, 0);
        mx.ext = 0;
        mx.arm = 0;
    }
    sq.playing = (uint8_t)on;
}

/* the clock in (SYNC IN): start or continue to it; each pulse times the tempo, every sixth arms a step */
static void clock_start(int cont)
{
    if (sq.playing && !mx.ext) seq_play(0);
    if (!cont) seq_reset();
    else if (!sq.playing) {            /* on from the step it stopped at (or the song position): the tracks off 1x too */
        lockmap_build();
        sq.ctr = 0;
        sq.relock = 1;
    }
    mx.ext = 1;
    mx.pulses = 0;
    mx.arm = 0;
    sq.playing = 1;
    if (set_on(S_SYNO)) mx_out(1, cont ? 0xFB : 0xFA, 0, 0);    /* passed on */
}

static void clock_pulse(void)
{
    uint32_t dt = mx.now - mx.last;
    mx.last = mx.now;
    if (dt > 0 && dt < M2K_SR / 2u) {   /* above 5 BPM; the first after a long silence (or boot) only starts the timing */
        uint32_t d8 = dt << 8;
        mx.per_q8 = mx.per_q8 ? (uint32_t)((int32_t)mx.per_q8 + (((int32_t)d8 - (int32_t)mx.per_q8) >> 3)) : d8;
    }
    if (set_on(S_SYNO) && mx.ext) mx_out(1, 0xF8, 0, 0);
    if (!mx.ext || !sq.playing) return;
    if (mx.pulses % 6u == 0) {
        if (mx.arm) {                  /* the last one's swing ran past its pulse */
            mx.arm = 0;
            sq.ctr = step_len(step_fire(M2K_SUB << 8));
        }
        mx.arm = 1;
        mx.arm_ctr = (step_due() & 1) ? step_len(0) - (int32_t)sps_q8 : 0;   /* an odd step: the even's extra length */
    }
    mx.pulses++;
}

/* ------------------------------------------------------------------------------------------- live play */

/* the step a note played now is written to on track t: the one it plays, or the next if that is nearer (then not
 * played again this time), on the track's own steps and clock off 1x, under the clock in on the step expected; the
 * one it plays if the next is another pattern's */
static int rec_step(int t)
{
    int off = spd_of(t) != 4, s = track_pos(t), len = seq_len();
    if ((off ? sq.tctr[t] : sq.ctr) >= track_len(t, s) / 2 || pat_coming_in()) return s;
    int nx = off ? sq.ts[t] : sq.step;
    if (nx >= len) nx = 0;
    sq.rec_skip[t] = (int8_t)nx;
    return nx;
}

/* Ready: the first note (a key, a pad, MIDI in) starts the pattern from its first step, and is recorded there */
static void ready_go(void) { if (sq.ready && !sq.playing) seq_play(1); }

static void live_on(int note, int vel)
{
    ready_go();
    int t = sq.ktrk;
    for (int i = 0; i < sq.nheld; i++) if (sq.held[i] == note) return;
    if (sq.nheld < 8) sq.held[sq.nheld++] = (uint8_t)note;
    int mode = (PAT(cur_pat)->kit[t][P_AMOD] * 6) >> 7;
    if (mode) {
        if (sq.nheld == 1) { sq.arp_ctr = 0; sq.arp_i = 0; }
        return;
    }
    m_cpy(trk[t].eff, PAT(cur_pat)->kit[t], P_COUNT);
    /* heard alike at any velocity, as the Monomachine plays a note (chain_kick40); a hard key records an accent */
    voice_on(t, note, 100, sq.nheld > 1 && PAT(cur_pat)->kit[t][P_PORT] > 0);
    mx_note_off(t);                    /* the gate the player's now: a step's note out ends */
    trk[t].gate_left = -1;
    if (sq.rec && sq.playing) {
        m2k_step_t *st = &PAT(cur_pat)->step[t][rec_step(t)];
        st->note = (uint8_t)note;
        st->flags = (uint8_t)(ST_TRIG | (vel > 110 ? ST_ACC : 0));
    }
}

/* a pad (the keys in PADS): track t hit at a note as a step trigs it, its gate a step's; while REC plays, written to
 * the nearest step of that track */
static void pad_hit(int t, int note)
{
    ready_go();
    m_cpy(trk[t].eff, PAT(cur_pat)->kit[t], P_COUNT);
    voice_trig(t, note, 100, 0, 0);
    trk[t].gate_left = gate_blocks(t);
    if (set_on(S_NOTO)) mx_note_on(t, note, 100);
    if (sq.rec && sq.playing) {
        m2k_step_t *st = &PAT(cur_pat)->step[t][rec_step(t)];
        st->note = (uint8_t)note;
        st->flags = ST_TRIG;
    }
}

static void live_off(int note)
{
    int t = sq.ktrk, j = 0;
    for (int i = 0; i < sq.nheld; i++) if (sq.held[i] != note) sq.held[j++] = sq.held[i];
    sq.nheld = (uint8_t)j;
    int mode = (PAT(cur_pat)->kit[t][P_AMOD] * 6) >> 7;
    if (mode) return;
    if (j && trk[t].note == note) {
        voice_on(t, sq.held[j - 1], 100, 1);         /* back to the last key still held */
        return;
    }
    if (!j || trk[t].note == note) voice_off(t);
}

/* the Monomachine's CC map (its MIDI implementation, as the gearmulator emulator encodes it): a channel per track,
 * SYN on 48-55, AMP 56-63, FILT 72-79, FX 80-87, LFO 1/2/3 on 88/104/112, level 7, mute 3. MONO2K's pages are laid
 * out the same, so a controller set up for a Monomachine drives the same parameters here. */
static int mm_cc_param(int cc)
{
    static const uint8_t base[7] = {48, 56, 72, 80, 88, 104, 112};
    static const uint8_t first[7] = {P_SYN, P_ATK, P_BASE, P_EQF, P_LFO, P_LFO + 8, P_LFO + 16};
    for (int i = 0; i < 7; i++)
        if (cc >= base[i] && cc < base[i] + 8) return first[i] + (cc - base[i]);
    if (cc == 7) return P_LEVL;
    return -1;
}

/* GLO > IN's THRU: what the TRS jack brings, out over USB with the core's own messages, stamped alike (the FM-1 a MIDI
 * interface too); its clock and transport not while SYNC OUT sends MONO2K's, which passes this one on when it follows
 * it: one clock on the USB */
static void midi_thru(int status, int d1, int d2)
{
    if (status >= 0xF8 && set_on(S_SYNO)) return;
    int n = status >= 0xF8 ? 1 : (status & 0xE0) == 0xC0 ? 2 : 3;   /* realtime a byte, Cx Dx two, the rest three */
    mx_out(n, status, n > 1 ? d1 : 0, n > 2 ? d2 : 0);
}

void m2k_midi_from(int src, int status, int d1, int d2)
{
    const int from = src == M2K_TRS ? 2 : 1;
    mx.in_at[from >> 1] = mx.now;
    mx.in_seen |= (uint8_t)from;
    if (from == 2 && set_on(S_THRU)) midi_thru(status, d1, d2);
    int ch = status & 15, cmd = status & 0xF0;
    int r = (ch - midi_ch() + 16) & 15, t = r < M2K_TRACKS ? r : sq.ktrk;
    if (status >= 0xF0) {
        if (!clk_from()) {             /* no clock in: start, continue and stop still drive the sequencer, from where
                                        * the notes come (a DAW starting mid-song sends continue; without the clock it
                                        * plays from its start) */
            if (!(notes_from() & from)) return;
            if (status == 0xFA || status == 0xFB) seq_play(1);
            else if (status == 0xFC) seq_play(0);
            return;
        }
        if (!(clk_from() & from)) return;    /* the clock and transport only from SYIN's: another's would fight it */
        if (status == 0xF8) clock_pulse();
        else if (status == 0xFA) clock_start(0);
        else if (status == 0xFB) clock_start(1);
        else if (status == 0xFC) {
            int thru = mx.ext && set_on(S_SYNO);
            seq_play(0);
            if (thru) mx_out(1, 0xFC, 0, 0);         /* passed on, as its start and pulses */
        } else if (status == 0xF2 && !sq.playing) {
            uint32_t pos = (uint32_t)((d1 & 127) | (d2 & 127) << 7);   /* in steps from the song's start */
            sq.step = (uint8_t)(pos % (uint32_t)seq_len());
            sq.n1x = pos;
        }
        return;
    }
    if (!(notes_from() & from)) return;   /* notes, CCs and mutes only from NOTE IN's */
    if (cmd == 0xB0) {
        if (d1 == 3) {
            PAT(cur_pat)->mute = (uint8_t)((PAT(cur_pat)->mute & ~(1u << t)) | (d2 ? 1u << t : 0));
            return;
        }
        int prm = mm_cc_param(d1);
        if (prm < 0) return;
        int v = d2 > 127 ? 127 : d2;
        if (prm == P_LFO + L_DEST || prm == P_LFO + 8 + L_DEST || prm == P_LFO + 16 + L_DEST) v = v > P_COUNT ? P_COUNT : v;
        PAT(cur_pat)->kit[t][prm] = (uint8_t)v;
        trk[t].eff[prm] = (uint8_t)v;
        return;
    }
    if (cmd == 0x90 && d2) {
        ready_go();
        if (t == sq.ktrk) { live_on(d1, d2); return; }
        m_cpy(trk[t].eff, PAT(cur_pat)->kit[t], P_COUNT);
        voice_on(t, d1, 100, 0);                    /* any velocity alike, as the Monomachine's (no accent) */
        mx_note_off(t);                             /* (the gate the player's now: a step's note out ends) */
        trk[t].gate_left = -1;
    } else if (cmd == 0x80 || cmd == 0x90) {
        if (t == sq.ktrk) { live_off(d1); return; }
        if (trk[t].note == d1) voice_off(t);
    }
}

void m2k_midi(int status, int d1, int d2) { m2k_midi_from(M2K_USB, status, d1, d2); }

/* ------------------------------------------------------------------------------------------ the factory */

static inline uint8_t enc(int i, int n) { return (uint8_t)((i * 128 + 64) / n); }

static const uint8_t MACH_DEF[M_COUNT][8] = {
    /* GND-SIN  */ {0, 0, 0, 0, 40, 0, 0, 64},     /* the Monomachine's init: a sine (its page TUNE alone, on H) */
    /* GND-PERC */ {64, 0, 0, 40, 0, 40, 0, 0},
    /* SID-6581 */ {0, 0, 127, 0, 0, 0, 64, 64},   /* the Monomachine's init: a triangle, the sweep reset at each trig */
    /* FM+STAT  */ {61, 64, 80, 32, 82, 64, 64, 64},  /* the Monomachine's init: 1/2, 1; a slow decay, a little feedback */
    /* FM+DYN   */ {64, 64, 64, 0, 74, 80, 32, 64},   /* the Monomachine's init: 1, 1.33; a slow decay, a little feedback */
    /* SWAV-SAW */ {0, 0, 0, 0, 0, 0, 0, 64},     /* the Monomachine's init: the base saw alone */
    /* SWAV-ENS */ {85, 100, 120, 0, 64, 0, 64, 64},   /* +4 +7 +11 (the Monomachine's: all OFF, 62) */
    /* DIGI-WAV */ {40, 64, 64, 0, 0, 70, 0, 0},
    /* VOX-6    */ {64, 64, 127, 0, 0, 64, 64, 64},   /* the Monomachine's VO-6 init: a voice on the middle vowel */
    /* FM+PAR   */ {61, 64, 82, 64, 104, 80, 64, 64},  /* the Monomachine's init: 1/2, 1, 2; the third decaying */
    /* SWAV-PUL */ {0, 0, 0, 0, 64, 0, 0, 64},     /* the Monomachine's init: the square alone */
    /* FX-CHORS */ {40, 50, 30, 64, 64, 64, 127, 100}, /* MONO2K's own: a slow chorus, half wet, no feedback */
    /* FX-RING  */ {0, 0, 0, 127, 0, 0, 0, 64},      /* a sine carrier, INP 64 (the Monomachine's init, but all wet: its MIX 0 is dry) */
    /* FX-DYNMX */ {20, 60, 64, 127, 64, 64, 0, 100}, /* a quick attack, 4:1-ish over -30 dB */
    /* GND-NOIS */ {0, 0, 0, 0, 0, 0, 0, 64},     /* the Monomachine's init: white, in the middle */
    /* DRM-KICK */ {0, 44, 26, 21, 0, 64, 0, 16},  /* the LXR's init kick: a sine swept x4.8 over 38 ms; a little click */
    /* DRM-SNAR */ {40, 81, 14, 127, 61, 64, 0, 6},  /* its snare: a triangle swept x14, noise at 22 kHz, half and half;
                                                   * a clap's hits 8 ms apart */
    /* DRM-HATS */ {54, 60, 127, 127, 127, 0, 5, 0},  /* its hats: a sine under a sine and the noise, 60 ms closed */
    /* DRM-CYMB */ {127, 64, 0, 64, 0, 110, 0, 107}, /* its cymbal: noise held at the top, a long fall */
};

static void kit_default(m2k_pattern_t *p, int t, int m)
{
    uint8_t *k = p->kit[t];
    static const uint8_t D[P_COUNT] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 64, 64, 64, 64, 64, 0,           /* ATK HOLD DEC REL DIST VOL PAN PORT: the Monomachine's init, as all */
        0, 127, 0, 0, 0, 32, 64, 64,           /* BASE WDTH HPQ LPQ FATK FDEC BOF WOF       of these but PTCH..GATE */
        64, 64, 0, 64, 64, 32, 0, 127,         /* EQF EQG SRR DTIM DSND DFB DBAS DWID */
        0, 0, 0, 0,                            /* BITS FOLD XOR SPU */
        64, 100, 0, 64, 0, 14,                 /* PTCH LEVL AMOD ASPD ARNG GATE */
    };
    m_cpy(k, D, P_COUNT);
    k[P_MODR] = 40;                    /* the modulation: a slow chorus, off until its MIX is up */
    k[P_MODD] = 64;
    for (int l = 0; l < 3; l++) {
        uint8_t *b = &k[P_LFO + 8 * l];
        b[L_TRCK] = enc(t, M2K_TRACKS);
        b[L_DEST] = 0;
        b[L_WAVE] = 0;
        b[L_TRIG] = 0;
        b[L_MULT] = 0;
        b[L_SPD] = 32;
        b[L_PHAS] = 0;
        b[L_DPTH] = 64;
    }
    p->machine[t] = (uint8_t)m;
    m_cpy(k, MACH_DEF[m], 8);
    if (m == M_FXC || m == M_FXR || m == M_FXD) k[P_HOLD] = 127;     /* an FX track stays open from its trig to the gate's end */
}

static void pat_init(m2k_pattern_t *p, const char *name)
{
    m_set(p, 0, sizeof *p);
    for (int i = 0; i < 7 && name[i]; i++) p->name[i] = name[i];
    static const uint8_t M[M2K_TRACKS] = {M_GSIN, M_FMD, M_GPRC, M_SID, M_VOX, M_ENS};
    for (int t = 0; t < M2K_TRACKS; t++) {
        kit_default(p, t, M[t]);
        for (int s = 0; s < M2K_STEPS; s++) p->step[t][s].note = 60;
    }
    for (int i = 0; i < M2K_LOCKS; i++) p->lock[i].trk = 0xFF;
    p->glob[G_TMPO] = 90;
    p->glob[G_LEN] = 16;
    p->glob[G_ACNT] = 60;
    p->glob[G_VERB] = 60;
    p->glob[G_DAMP] = 50;
    for (int t = 0; t < M2K_TRACKS; t++) p->glob[G_SPD1 + t] = SPD_1X;
}

#include "m2k_factory.c"
#include "m2k_proj.c"

/* ------------------------------------------------------------------------------------------------ store */

static uint8_t dirty;

void m2k_setting(int i, int v)
{
    if (i < 0 || i >= S_COUNT || store_.set[i] == (uint8_t)CLAMP(v, 0, 127)) return;
    store_.set[i] = (uint8_t)CLAMP(v, 0, 127);
    set_at = mx.now;                              /* saved 3 s after the last change (m2k_proj.c) */
    set_wait = 1;
}

int m2k_dirty(void)
{
    int d = dirty;
    if (set_wait && mx.now - set_at >= 3u * M2K_SR) { set_wait = 0; d = 1; }
    dirty = 0;
    return d;
}

int m2k_playing(void) { return sq.playing; }

static void ui_init(void);

void m2k_init(uint32_t seed)
{
    rng = seed | 1u;
    m_set(trk, 0, sizeof trk);
    m_set(&bus, 0, sizeof bus);
    m_set(&perf, 0, sizeof perf);
    perf.kill = 32767;
    m_set(&sq, 0, sizeof sq);
    m_set(&mx, 0, sizeof mx);
    modn = modpn = 0;
    m_set(mx.on, 0xFF, sizeof mx.on);
    mx.last = 0u - M2K_SR;                       /* no pulse in yet: the first only starts the timing */
    ext_bpm = 0;
    m_set(dl_mem, 0x7F, sizeof dl_mem);          /* mu-law silence: 0xFF and 0x7F both decode to ~0 */
    m_set(ens_mem, 0, sizeof ens_mem);
    m_set(mod_mem, 0, sizeof mod_mem);           /* the MOD effect's lines too: a reset hears no old chorus */
    m_set(gl_buf, 0, sizeof gl_buf);
    m_set(tp_buf, 0, sizeof tp_buf);
    m_set(cb_buf, 0, sizeof cb_buf);
    m_set(ap_buf, 0, sizeof ap_buf);
    for (int t = 0; t < M2K_TRACKS; t++) {
        trk[t].dl = dl_mem[t];
        trk[t].dquiet = DL_LEN;        /* the lines start silent */
        trk[t].lfsr = 0x1234567u + (uint32_t)t * 7919u;
        trk[t].lfsr2 = 0x9E3779B9u + (uint32_t)t * 104729u;
        trk[t].gate_left = -1;
        trk[t].dt_q8 = 400 << 8;
    }
    bus.hs = 0xACE1u;
    for (int i = 0; i < M2K_TRACKS; i++) sq.rec_skip[i] = -1;
    m_set(store_.set, 0, sizeof store_.set);
    projects_factory();
    cur_pat = 0;
    sq.queued = 0;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, PAT(0)->kit[t], P_COUNT);
    seq_tempo();
    ui_init();
}
