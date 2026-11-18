/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's projects, the modern Elektron way: every pattern carries its own sounds. A project is 4 banks (A-D)
 * of 16 patterns. The working patterns live unpacked in RAM (PAT(i); on the FM-1 two at a time, the others packed in
 * the working image); the saved projects are packed images: each pattern as a blob of its sounds, its trigs only and
 * its locks only, so a typical one is 0.5-1 KB, and a blank (INIT) one nothing.
 *   SAVE          the current pattern's reload point (into the open project's image)
 *   GLO + SAVE    reload: the current pattern back to that point
 *   hold SAVE     the whole project
 * The images live in RAM; the store (them, the working patterns, the settings) is one stream, into a buffer on the
 * computer (host, browser) and into the flash on the FM-1 (m2k_fw_store_put / _get), never whole in its RAM. */

#ifndef M2K_IMG_DATA
#define M2K_IMG_DATA (160u * 1024u)        /* a project's packed patterns (the FM-1's first build: 28 KB, 16 of them) */
#endif
#define IMG_DATA M2K_IMG_DATA
/* the most a pattern of NT tracks packs to: its head (2 + 8 + NT + its globals + 1: six tracks fill the 66 since format
 * 24, the 60 since 20, the 54 since 19, the 48 since 18), every kit (74 wide since format 22), its trig mask, every step
 * a trig, every lock. BLOB_MAX is this build's (six: 3632, the Patch Editor's, editor.js's; 3626 before 24, 3608 before
 * 22; the Move's a byte more, the reverb's type), BLOB_ANY format 27's, eight wide (4321: what a store may hold) */
#define BLOB_OF(nt) (2 + 8 + (nt) + G_COUNT_OF(24, nt) + 1 + (nt) * P_COUNT + (nt) * M2K_STEPS / 8 + \
                     (nt) * M2K_STEPS * 4 + 2 + M2K_LOCKS * 4)
#ifdef M2K_SPU
#define BLOB_MAX (BLOB_OF(M2K_TRACKS) + 1)
#else
#define BLOB_MAX BLOB_OF(M2K_TRACKS)
#endif
#define BLOB_ANY (BLOB_OF(M2K_TRACKS21) + 1)
_Static_assert(BLOB_OF(M2K_TRACKS6) == 3632, "a pattern of six packs to 3632 at most (editor.js's BLOB_MAX)");

typedef struct {
    char name[8];
    uint32_t used;
    uint16_t len[M2K_PATTERNS];
    uint8_t data[IMG_DATA];
} m2k_img_t;

static m2k_img_t proj[M2K_PROJECTS] POOL;
static m2k_img_t work_img POOL;              /* scratch: packing a whole project, the store */
static uint8_t blob[BLOB_MAX];
static int cur_proj;

/* -------------------------------------------------------------------------------------- one pattern, packed */

/* a format's tracks: 21, 23, 25 and 27 eight (a pattern that uses a track past the sixth, in a build of eight), 22, 24,
 * 26 and every one before 21 six */
static inline int fmt_nt(int ver) { return ver == 21 || ver == 23 || ver == 25 || ver == 27 ? M2K_TRACKS21 : M2K_TRACKS6; }
/* a format's kit: from 22 the ADSR's three after the bank (74), from 8 the bank (71), from 3 the MOD effect (70) */
static inline int fmt_kit(int ver) { return ver >= 22 ? P_COUNT : ver >= 8 ? P_COUNT_V21 : ver >= 3 ? P_WBNK : P_MODT; }

/* the globals from s to d between a pattern and a format's layout, GL of them, its per-track blocks (the speeds;
 * Grids' map, then the lanes; the lengths; the chances; POLY) NT wide where the pattern's are M2K_TRACKS: OUT, s the
 * pattern's and d the format's, or the other way. In, a track the build has not is passed over, one the format has
 * not left as it was */
static void glob_io(const uint8_t *s, uint8_t *d, int gl, int nt, int out)
{
    if (nt == M2K_TRACKS) { m_cpy(d, s, (uint32_t)gl); return; }   /* (the same layout) */
    static const uint8_t AT[7] = {0, G_SPD1, G_GRX, G_LANE1, G_TLEN1, G_TPRB1, G_POLY1};
    int n = 0;
    for (int k = 0; k < 7 && n < gl; k++) {
        const int per = k == 1 || k >= 3, w = !per ? (k ? G_LANE1 - G_GRX : G_SPD1) : nt;   /* a track each, or a run */
        for (int i = 0; i < w && n < gl; i++, n++) {
            if (per && i >= M2K_TRACKS) continue;
            if (out) d[n] = s[AT[k] + i]; else d[AT[k] + i] = s[n];
        }
    }
}

/* a pattern in the layout of a version (21, 23 and 25 in a build of eight; 24, 22, 20, 19 and 18, today's; the self-test
 * writes the older ones, 3..17; 3..7 kits 70 wide). Format 21 is 20 eight tracks wide: the machines, the per-track
 * globals, the mutes' bits, the kits, the trig mask, the trigs and the locks of eight; an LFO's TRCK past 127 aims past
 * the sixth. Format 22 is 20 with kits 74 wide (the ADSR's SUS FSUS FREL after the bank), 23 is 22 eight wide (21's).
 * Format 24 is 22 with POLY's voices after the chances (a track's each), 25 is 24 eight wide. 26 is 24 with a byte after
 * the globals, the Move's reverb type (G_RVTY, M2K_SPU), 27 is 25 with it; a build without it writes 0 there */
static int pat_pack_v(const m2k_pattern_t *p, uint8_t *o, int ver)
{
    const int nt = fmt_nt(ver);        /* (no more than the build has: 21, 23 and 25 only in a build of eight) */
    int n = 0, stride = fmt_kit(ver);
    o[n++] = 'P';
    o[n++] = (uint8_t)ver;             /* 2: amp envelope on the Monomachine's times; 3: MOD; 4: SWAV-SAW's new page;
                                        * 5: the track chain on the Monomachine's; 6: GND-SIN's TUNE in 3.125 c;
                                        * 7: SWAV-ENS's page; 8: the wave bank's choice (kits 71 wide); 9: the bus's
                                        * controls on their new laws; 10: FM+STAT's and FM+DYN's pages; 11: SID's;
                                        * 12: GND-SIN's (TUNE on its eighth knob); 13: HOLD in beats; 14: VO-6's page;
                                        * 15: the tracks' speeds, after the globals; 16: the amp's ATK DEC REL and the
                                        * LFOs' SPD and DPTH on the owner's laws; 17: the bus's effects rebuilt (the
                                        * reverb, its and the delay's sends, the glitch); 18: Grids (its map, its
                                        * lanes, after the speeds) and four more chances, after the conditions; 19:
                                        * the tracks' own lengths, after the lanes; 20: the tracks' chances (PROB),
                                        * after the lengths; 21: eight tracks; 22: the amp's and the filter's ADSR
                                        * (SUS FSUS FREL, kits 74 wide); 23: 22 eight tracks wide; 24: POLY, after
                                        * the chances; 25: 24 eight tracks wide; 26: the Move's reverb type after
                                        * the globals; 27: 26 eight tracks wide */
    m_cpy(o + n, p->name, 8); n += 8;
    m_cpy(o + n, p->machine, (uint32_t)nt); n += nt;
    const int gl = G_COUNT_OF(ver, nt);
    glob_io(p->glob, o + n, gl, nt, 1); n += gl;
#ifdef M2K_SPU
    if (ver >= 26) o[n++] = p->glob[G_RVTY];
#else
    if (ver >= 26) o[n++] = 0;
#endif
    o[n++] = p->mute;
    for (int t = 0; t < nt; t++) { m_cpy(o + n, p->kit[t], stride); n += stride; }
    uint8_t *mask = o + n;
    m_set(mask, 0, (uint32_t)(nt * M2K_STEPS / 8));
    n += nt * M2K_STEPS / 8;
    for (int t = 0; t < nt; t++)
        for (int s = 0; s < M2K_STEPS; s++) {
            const m2k_step_t *st = &p->step[t][s];
            if (!(st->flags & ST_TRIG)) continue;
            int b = t * M2K_STEPS + s;
            mask[b >> 3] |= (uint8_t)(1u << (b & 7));
            o[n++] = st->note; o[n++] = st->flags; o[n++] = st->cond; o[n++] = st->rtrg;
        }
    int nl = 0, at = n;
    n += 2;
    for (int i = 0; i < M2K_LOCKS; i++) {
        const m2k_lock_t *l = &p->lock[i];
        if (l->trk >= nt) continue;
        o[n++] = l->trk; o[n++] = l->step; o[n++] = l->param; o[n++] = l->val;
        nl++;
    }
    o[at] = (uint8_t)(nl & 0xFF);
    o[at + 1] = (uint8_t)(nl >> 8);
    return n;
}

static inline int is_trck(int prm) { return prm >= P_LFO && prm < P_LFO + 24 && (prm - P_LFO) % 8 == L_TRCK; }
static inline int is_dest(int prm) { return prm >= P_LFO && prm < P_LFO + 24 && (prm - P_LFO) % 8 == L_DEST; }
/* an LFO's DEST on the ADSR's three (72..74: OFF before format 22, past a kit of 71) */
static inline int dest_adsr(int v) { return v > P_COUNT_V21 && v <= P_COUNT; }

/* a kit that uses the ADSR's three (format 22): SUS FSUS FREL off their defaults (0 0 32: every older kit's), or an LFO
 * aimed at one */
static int kit_adsr(const uint8_t *k)
{
    if (k[P_SUS] || k[P_FSUS] || k[P_FREL] != FREL_DEF) return 1;
    for (int l = 0; l < 3; l++) if (dest_adsr(k[P_LFO + 8 * l + L_DEST])) return 1;
    return 0;
}

/* a kit of a format before 22 (or a sound of a store without them): the ADSR's three at what played as it did (SUS 0
 * the one-shot amp, FSUS 0 the filter's AD, FREL 32), an LFO aimed past its 71 parameters (OFF there) still OFF */
static void kit_v21(uint8_t *k)
{
    k[P_SUS] = 0;
    k[P_FSUS] = 0;
    k[P_FREL] = FREL_DEF;
    for (int l = 0; l < 3; l++) if (dest_adsr(k[P_LFO + 8 * l + L_DEST])) k[P_LFO + 8 * l + L_DEST] = 0;
}

/* a pattern that uses the ADSR's three: a kit's (kit_adsr), a lock on one, or a lock aiming an LFO at one */
static int pat_adsr(const m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) if (kit_adsr(p->kit[t])) return 1;
    for (int i = 0; i < M2K_LOCKS; i++) {
        const m2k_lock_t *l = &p->lock[i];
        if (l->trk < M2K_TRACKS && (l->param >= P_COUNT_V21 || (is_dest(l->param) && dest_adsr(l->val)))) return 1;
    }
    return 0;
}

#if M2K_TRACKS > M2K_TRACKS6
/* a pattern that uses a track past the sixth: anything of it other than as a pattern of six leaves it (track_blank: its
 * machine, its kit, a trig, a lock, its mute, its speed, lane, length or chance), or an LFO or a lock aimed at it */
static int pat_wide(const m2k_pattern_t *p)
{
    static m2k_pattern_t ref;                      /* the tracks past the sixth, blank (made once) */
    static uint8_t made;
    if (!made) { for (int t = M2K_TRACKS6; t < M2K_TRACKS; t++) track_blank(&ref, t); made = 1; }
    if (p->mute >> M2K_TRACKS6) return 1;
    for (int t = M2K_TRACKS6; t < M2K_TRACKS; t++) {
        if (p->machine[t] != ref.machine[t]) return 1;
        for (int k = 0; k < P_COUNT; k++) if (p->kit[t][k] != ref.kit[t][k]) return 1;
        if (p->glob[G_SPD1 + t] != SPD_1X || p->glob[G_LANE1 + t] || p->glob[G_TLEN1 + t] || p->glob[G_TPRB1 + t] ||
            p->glob[G_POLY1 + t]) return 1;
        for (int s = 0; s < M2K_STEPS; s++) if (p->step[t][s].flags & ST_TRIG) return 1;
    }
    for (int t = 0; t < M2K_TRACKS6; t++)
        for (int l = 0; l < 3; l++) if (p->kit[t][P_LFO + 8 * l + L_TRCK] >= TRCK_PAST) return 1;
    for (int i = 0; i < M2K_LOCKS; i++) {
        const m2k_lock_t *l = &p->lock[i];
        if (l->trk < M2K_TRACKS && (l->trk >= M2K_TRACKS6 || (is_trck(l->param) && l->val >= TRCK_PAST))) return 1;
    }
    return 0;
}
#endif

/* a pattern with a track on POLY (format 24): its voices off none */
static int pat_poly(const m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) if (p->glob[G_POLY1 + t]) return 1;
    return 0;
}

#ifdef M2K_SPU
/* a pattern that plays one of the Move's reverb types (format 26): its choice off the FDN (the knob's FDN, a byte of 6,
 * packs as before and comes back 0: the same choice) */
static int pat_rvty(const m2k_pattern_t *p) { return (p->glob[G_RVTY] * (SRV_N + 1)) >> 7 != 0; }
#endif

/* a pattern none of whose tracks has a length of its own packs as format 18, byte for byte as before (bank A's, a
 * blank's: the stores' demos and blanks still match, and 1.4 reads it); one with a length, as 19; one with a track's
 * chance under 100 %, as 20 (1.5 reads 19); one that uses the ADSR's three (pat_adsr), as 22 (1.9 reads 20 and 21);
 * one with a track on POLY (pat_poly), as 24 (1.10 reads 22 and before); one that plays the Move's reverb type
 * (pat_rvty, M2K_SPU), as 26 (the browser reads it, the type dropped); one that uses a track past the sixth (a build of
 * eight), as 21, or as 23 with the ADSR's, 25 with POLY, 27 with the type */
static int pat_pack(const m2k_pattern_t *p, uint8_t *o)
{
    int own = 0, prb = 0;
    for (int t = 0; t < M2K_TRACKS; t++) { own |= p->glob[G_TLEN1 + t]; prb |= p->glob[G_TPRB1 + t]; }
    const int adsr = pat_adsr(p), poly = pat_poly(p);
#ifdef M2K_SPU
    if (pat_rvty(p)) {
#if M2K_TRACKS > M2K_TRACKS6
        if (pat_wide(p)) return pat_pack_v(p, o, 27);
#endif
        return pat_pack_v(p, o, 26);
    }
#endif
#if M2K_TRACKS > M2K_TRACKS6
    if (pat_wide(p)) return pat_pack_v(p, o, poly ? 25 : adsr ? 23 : 21);
#endif
    return pat_pack_v(p, o, poly ? 24 : adsr ? 22 : prb ? 20 : own ? 19 : 18);
}

static uint8_t tune_v5(int v)
{
    v = 64 + (v - 64) * 32;
    return (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

/* a pattern's values in their ranges, as the knobs keep them: a damaged or hand-made store reaches past no table
 * (the kit and the locks 0..127, the notes too, ratchets 0..4, each global to its own: the tempo to 255, the length
 * 1..64, a track's 0..64). Before the conversions (they index by the old values) and after them. An LFO's TRCK may
 * reach past 127 to the tracks past the sixth this build has (format 21's), where WIDE */
static void pat_clamp(m2k_pattern_t *p, int wide)
{
    const int tmax = wide ? TRCK_PAST + M2K_TRACKS - M2K_TRACKS6 - 1 : 127;
    for (int t = 0; t < M2K_TRACKS; t++) {
        for (int k = 0; k < P_COUNT; k++) if (p->kit[t][k] > (is_trck(k) ? tmax : 127)) p->kit[t][k] = 127;
        for (int s = 0; s < M2K_STEPS; s++) {
            m2k_step_t *st = &p->step[t][s];
            st->note &= 127;
            if (st->rtrg > 4) st->rtrg = 4;
        }
    }
    for (int g = 0; g < G_COUNT; g++) if (g != G_TMPO && p->glob[g] > 127) p->glob[g] = 127;
    p->glob[G_LEN] = (uint8_t)CLAMP(p->glob[G_LEN], 1, 64);
    for (int t = 0; t < M2K_TRACKS; t++) if (p->glob[G_TLEN1 + t] > 64) p->glob[G_TLEN1 + t] = 64;
    for (int t = 0; t < M2K_TRACKS; t++) if (p->glob[G_TPRB1 + t] > 15) p->glob[G_TPRB1 + t] = 15;
    for (int t = 0; t < M2K_TRACKS; t++) {           /* POLY: none, or 2..8 voices (one is none) */
        uint8_t *v = &p->glob[G_POLY1 + t];
        *v = *v < 2 ? 0 : *v > M2K_TRACKS21 ? M2K_TRACKS21 : *v;
    }
    for (int i = 0; i < M2K_LOCKS; i++) if (p->lock[i].val > (is_trck(p->lock[i].param) ? tmax : 127)) p->lock[i].val = 127;
}

/* format 21 read by a build of fewer tracks (its first six): an LFO aimed past them moves nothing. Its kit's aim there:
 * DEST 0, TRCK its own track, and its DEST locks 0 on the steps no TRCK lock aims it back; a lock aiming it there: the
 * LFO off on that step (a DEST lock of 0, in its place) */
static void trck_past(m2k_pattern_t *p)
{
    for (int i = 0; i < M2K_LOCKS; i++) {
        m2k_lock_t *l = &p->lock[i];
        if (l->trk >= M2K_TRACKS || !is_trck(l->param) || trck_of(l->val) < M2K_TRACKS) continue;
        const int dp = l->param - L_TRCK + L_DEST;
        for (int j = 0; j < M2K_LOCKS; j++)
            if (p->lock[j].trk == l->trk && p->lock[j].step == l->step && p->lock[j].param == dp) p->lock[j].trk = 0xFF;
        l->param = (uint8_t)dp;
        l->val = 0;
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int b = P_LFO; b < P_LFO + 24; b += 8) {
            if (trck_of(p->kit[t][b + L_TRCK]) < M2K_TRACKS) continue;
            p->kit[t][b + L_TRCK] = trck_enc(t);
            p->kit[t][b + L_DEST] = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                m2k_lock_t *l = &p->lock[i];
                if (l->trk != t || l->param != b + L_DEST) continue;
                int back = 0;          /* (a TRCK lock on its step, in range now, aims it again) */
                for (int j = 0; j < M2K_LOCKS; j++)
                    back |= p->lock[j].trk == t && p->lock[j].step == l->step && p->lock[j].param == b + L_TRCK;
                if (!back) l->val = 0;
            }
        }
}

static int pat_unpack(m2k_pattern_t *p, const uint8_t *o, int len)
{
    int n = 0;
    if (len < 2 || o[0] != 'P' || o[1] < 1 || o[1] > 27) return 0;
    int ver = o[1], stride = fmt_kit(ver);    /* before 3 a kit ended before the MOD effect, before 8 before the bank,
                                               * before 22 before the ADSR's three */
    /* before 15 no speeds, 18 no Grids, 19 no lengths of the tracks' own (0: each the pattern's, as they played), 20
     * no chances (0: every trig), 22 no ADSR (kit_v21: SUS 0, FSUS 0, FREL 32, as they played), 24 no POLY (0: none),
     * 26 no reverb type (0: the FDN; a build without the Move's types reads 26 and 27's and passes it over); before 21
     * (and 22, 24, 26) six tracks (those past them blank, track_blank), 21, 23, 25 and 27 eight (those past this build's
     * passed over) */
    const int nt = fmt_nt(ver), gl = G_COUNT_OF(ver, nt);
    const int rv = ver >= 26;                                               /* (the reverb type's byte) */
    const int nc = ver >= 18 ? C_COUNT : C_COUNT_V17;                       /* (a condition past them was ALWAYS) */
    const int head = 2 + 8 + nt + gl + rv + 1 + nt * stride + nt * M2K_STEPS / 8 + 2;
    if (len < head) return 0;
    m_set(p, 0, sizeof *p);
    n = 2;
    m_cpy(p->name, o + n, 8); n += 8;
    p->name[7] = 0;
    m_cpy(p->machine, o + n, (uint32_t)(nt < M2K_TRACKS ? nt : M2K_TRACKS)); n += nt;
    glob_io(o + n, p->glob, gl, nt, 0); n += gl;
#ifdef M2K_SPU
    if (rv) p->glob[G_RVTY] = o[n];
#endif
    n += rv;
    if (ver < 15) for (int t = 0; t < M2K_TRACKS; t++) p->glob[G_SPD1 + t] = SPD_1X;
    p->mute = o[n++];
    if (nt > M2K_TRACKS) p->mute &= (uint8_t)M2K_TMASK;
    for (int t = 0; t < nt; t++, n += stride) {
        if (t >= M2K_TRACKS) continue;
        m_cpy(p->kit[t], o + n, stride);
        if (stride <= P_MODT) { p->kit[t][P_MODT] = 0; p->kit[t][P_MODR] = 40; p->kit[t][P_MODD] = 64; p->kit[t][P_MODM] = 0; }
        if (stride < P_COUNT) kit_v21(p->kit[t]);
    }
    const uint8_t *mask = o + n;
    n += nt * M2K_STEPS / 8;
    for (int t = 0; t < nt; t++) {
        const int here = t < M2K_TRACKS;
        if (here && p->machine[t] >= M_COUNT) p->machine[t] = M_GSIN;
        for (int s = 0; s < M2K_STEPS; s++) {
            m2k_step_t skip, *st = here ? &p->step[t][s] : &skip;      /* (a track past the build's: read, let go) */
            st->note = 60;
            int b = t * M2K_STEPS + s;
            if (!((mask[b >> 3] >> (b & 7)) & 1)) continue;
            if (n + 4 > len) return 0;
            st->note = o[n]; st->flags = o[n + 1]; st->cond = o[n + 2] < nc ? o[n + 2] : C_ALWAYS; st->rtrg = o[n + 3];
            n += 4;
        }
    }
    if (n + 2 > len) return 0;
    int nl = o[n] | (o[n + 1] << 8);
    n += 2;
    for (int i = 0; i < M2K_LOCKS; i++) p->lock[i].trk = 0xFF;
    const int np = ver >= 22 ? P_COUNT : P_COUNT_V21;                     /* (a lock past its kit was dropped) */
    for (int i = 0; i < nl && i < M2K_LOCKS; i++) {
        if (n + 4 > len) return 0;
        m2k_lock_t *l = &p->lock[i];
        *l = (m2k_lock_t){o[n], o[n + 1], o[n + 2], o[n + 3]};
        if (l->trk >= M2K_TRACKS || l->step >= M2K_STEPS || l->param >= np) l->trk = 0xFF;
        else if (ver < 22 && is_dest(l->param) && dest_adsr(l->val)) l->val = 0;   /* (OFF there: kit_v21's) */
        n += 4;
    }
    if (nt > M2K_TRACKS) trck_past(p);
    pat_clamp(p, nt > M2K_TRACKS6);
    if (ver == 1) amp_v1(p);
    if (ver < 4)                       /* SWAV-SAW took the Monomachine's page: carry the old one over as near as it goes */
        for (int t = 0; t < M2K_TRACKS6; t++) {
            if (p->machine[t] != M_SAW) continue;
            uint8_t *k = p->kit[t], o8[8];
            m_cpy(o8, k, 8);                       /* DTUN VOIC MIX SUB SHPE DRFT OCTV STRT */
            int voices = 1 + ((o8[1] * 7) >> 7);
            k[0] = voices > 1 ? o8[2] : 0;         /* UNIL from the side mix */
            k[1] = (uint8_t)(o8[0] * 2 > 127 ? 127 : o8[0] * 2);   /* UNIW from the detune */
            k[2] = voices >= 5 ? 100 : 0;          /* UNIX when there were five voices or more */
            k[3] = o8[5];                          /* DRFT */
            k[4] = o8[3];                          /* SUBX: the old sub was a square */
            k[5] = 0; k[6] = 0; k[7] = 64;         /* SUB1, SUB2, TUNE centred */
            for (int i = 0; i < M2K_LOCKS; i++)
                if (p->lock[i].trk == t && p->lock[i].param < 8) p->lock[i].trk = 0xFF;
        }
    if (ver < 3) {                     /* the reverb's return came up 12 dB: half the sends keep the same space */
        for (int t = 0; t < M2K_TRACKS6; t++) p->kit[t][P_SPU] >>= 1;
        for (int i = 0; i < M2K_LOCKS; i++) if (p->lock[i].trk != 0xFF && p->lock[i].param == P_SPU) p->lock[i].val >>= 1;
    }
    if (ver < 5) chain_v4(p);          /* the chain became the Monomachine's */
    if (ver < 6)                       /* GND-SIN's TUNE was a semitone a step: the Monomachine's +-2 semitones hold +-2 */
        for (int t = 0; t < M2K_TRACKS6; t++) {
            if (p->machine[t] != M_GSIN) continue;
            p->kit[t][P_SYN] = tune_v5(p->kit[t][P_SYN]);
            for (int i = 0; i < M2K_LOCKS; i++)
                if (p->lock[i].trk == t && p->lock[i].param == P_SYN) p->lock[i].val = tune_v5(p->lock[i].val);
        }
    if (ver < 7) ens_v6(p);           /* SWAV-ENS took the Monomachine's page */
    if (ver < 9) bus_v9(p);           /* the bus's controls took new laws */
    if (ver < 10) fm_v9(p);           /* FM+STAT and FM+DYN took the Monomachine's pages */
    if (ver < 11) sid_v10(p);         /* and SID SID-6581's */
    if (ver < 12) gsin_v11(p);        /* and GND-SIN's: TUNE on its eighth knob */
    if (ver < 13) hold_v12(p);        /* HOLD went from 14.6 ms a step to 1 / 32 beat */
    if (ver < 14) vox_v13(p);         /* VOX-6 took VO-6's page */
    if (ver < 16) feel_v15(p);        /* the amp's and the LFOs' laws, the owner's */
    if (ver < 17) fx_v16(p);          /* the bus's effects rebuilt: the reverb, the sends, the glitch */
    if (ver < 18) grids_v17(p);       /* Grids: its map as a pattern starts it, no track following a lane */
    pat_clamp(p, nt > M2K_TRACKS6);
#if M2K_TRACKS > M2K_TRACKS6
    for (int t = nt; t < M2K_TRACKS; t++) track_blank(p, t);   /* (a pattern of six: those past it blank) */
#endif
    return 1;
}

/* ------------------------------------------------------------------------------------------------ an image */

static uint32_t img_off(const m2k_img_t *im, int i)
{
    uint32_t o = 0;
    for (int k = 0; k < i; k++) o += im->len[k];
    return o;
}

static int img_put(m2k_img_t *im, int i, const uint8_t *b, int n)
{
    uint32_t off = img_off(im, i), old = im->len[i];
    if (im->used - old + (uint32_t)n > IMG_DATA) return 0;
    uint32_t tail = im->used - off - old;
    /* move the tail to its new place; regions overlap: copy in the safe direction */
    uint8_t *src = im->data + off + old, *dst = im->data + off + (uint32_t)n;
    if (dst < src) for (uint32_t k = 0; k < tail; k++) dst[k] = src[k];
    else for (uint32_t k = tail; k > 0; k--) dst[k - 1] = src[k - 1];
    m_cpy(im->data + off, b, (uint32_t)n);
    im->len[i] = (uint16_t)n;
    im->used = im->used - old + (uint32_t)n;
    return 1;
}

static const uint8_t *img_get(const m2k_img_t *im, int i, int *n)
{
    *n = im->len[i];
    return im->data + img_off(im, i);
}

static void img_clear(m2k_img_t *im, const char *name)
{
    m_set(im->name, 0, sizeof im->name);
    for (int i = 0; i < 7 && name[i]; i++) im->name[i] = name[i];
    im->used = 0;
    m_set(im->len, 0, sizeof im->len);
}

static m2k_pattern_t pat_tmp POOL;           /* scratch: the factory's blank, an old demo read back */
static void pattern_init_blank(m2k_pattern_t *p);

/* a blank pattern (INIT) packed: in an image it takes no bytes (its length 0), as an empty slot reads */
static uint8_t blank_blob[1024] POOL;             /* (a blank packs to about 500 bytes) */
static int blank_n;

static void blank_set(void)                         /* the blank, packed (-1: too large to keep; none then) */
{
    pattern_init_blank(&pat_tmp);
    int n = pat_pack(&pat_tmp, blob);
    if (n > (int)sizeof blank_blob) { blank_n = -1; return; }
    m_cpy(blank_blob, blob, (uint32_t)n);
    blank_n = n;
}

static int pat_pack_img(const m2k_pattern_t *p)     /* into blob; a blank is 0 */
{
    if (!blank_n) blank_set();
    int n = pat_pack(p, blob);
    if (n != blank_n) return n;
    for (int k = 0; k < n; k++) if (blob[k] != blank_blob[k]) return n;
    return 0;
}

static int img_put_pat(m2k_img_t *im, int i, const m2k_pattern_t *p) { int n = pat_pack_img(p); return img_put(im, i, blob, n); }

#if PAT_CACHE
/* the cache: two patterns unpacked (pat_idx), the rest packed in the working image. A slot is taken for another
 * pattern only from one not playing (written back first); the image keeps room for both held to be written back and
 * the playing one to grow to the most a pattern packs to (pat_can_open, pat_room), so writing back always fits. Should
 * it not (a store from elsewhere filled the image), what is held stays held: nothing is dropped unwritten */
static int pat_flush(int v) { return pat_idx[v] < 0 || img_put_pat(&work_img, pat_idx[v], &store_.p[v]); }   /* 0: no room */
static void pat_drop(void) { pat_idx[0] = pat_idx[1] = -1; }       /* the working image replaced: nothing held */

static m2k_pattern_t *pat_load(int i)
{
    int v = pat_idx[0] == cur_pat ? 1 : pat_idx[1] == cur_pat ? 0 : pat_idx[1] == (int)sq.queued ? 0 : 1, n;
    m2k_pattern_t *p = &store_.p[v];
    if (!pat_flush(v)) p = &pat_tmp;               /* the slot's pattern does not fit back: kept; i read, not held */
    else pat_idx[v] = (int16_t)i;
    const uint8_t *b = img_get(&work_img, i, &n);
    if (!n || !pat_unpack(p, b, n)) pattern_init_blank(p);
    return p;
}

/* the working image's bytes once what is held is written back, each held pattern as it packs now (or as it stands,
 * if more), all but pattern j's (-1: none left out) */
static uint32_t work_used(int j)
{
    uint32_t used = work_img.used - (j >= 0 ? work_img.len[j] : 0u);
    for (int v = 0; v < 2; v++) {
        int h = pat_idx[v];
        if (h < 0 || h == j) continue;
        uint32_t now = (uint32_t)pat_pack_img(&store_.p[v]);
        if (now > work_img.len[h]) used += now - work_img.len[h];
    }
    return used;
}

/* may pattern j play next: the others as they pack now (both held among them), and room for j to grow to the largest a
 * pattern packs to */
static int pat_can_open(int j) { return j == cur_pat || work_used(j) + BLOB_MAX <= IMG_DATA; }
#else
static int pat_can_open(int j) { (void)j; return 1; }
#endif

/* a pattern's name without unpacking it (the screen's lists): the slot's when it is held, else its blob's */
static const char *pat_name(int i)
{
#if PAT_CACHE
    static char nm[8];
    if (pat_idx[0] == i || pat_idx[1] == i) return PAT(i)->name;
    int n;
    const uint8_t *b = img_get(&work_img, i, &n);
    if (n < 10) return "INIT";
    m_cpy(nm, b + 2, 8);
    nm[7] = 0;
    return nm;
#else
    return PAT(i)->name;
#endif
}

/* may add more bytes go into the working image, the playing pattern's room kept (a paste into many) */
static int pat_room(int add)
{
#if PAT_CACHE
    return work_used(cur_pat) + (uint32_t)add + BLOB_MAX <= IMG_DATA;
#else
    (void)add;
    return 1;
#endif
}

/* every working pattern into the scratch image (on the FM-1 the working image itself: the two held written back);
 * 0 when they do not fit */
static int img_pack_work(const char *name)
{
#if PAT_CACHE
    int a = pat_flush(0), b = pat_flush(1);
    if (!a) a = pat_flush(0);                      /* (the second may have made room) */
    if (!a || !b) return 0;
    m_set(work_img.name, 0, sizeof work_img.name);
    for (int i = 0; i < 7 && name[i]; i++) work_img.name[i] = name[i];
    return 1;
#else
    img_clear(&work_img, name);
    for (int i = 0; i < M2K_PATTERNS; i++)
        if (!img_put_pat(&work_img, i, PAT(i))) return 0;
    return 1;
#endif
}

/* every working pattern into an image; 0 when they do not fit (the image is then left as it was) */
static int img_pack_all(m2k_img_t *dst, const char *name)
{
    if (!img_pack_work(name)) return 0;
    m_cpy(dst, &work_img, (uint32_t)(sizeof *dst - IMG_DATA + work_img.used));
    return 1;
}

/* the sound library: 128 slots (S001..S128, M2K_SOUNDS in m2k.h), each a track's machine and its parameters, a
 * category and the note it was made at, kept with the store. An empty slot (no name) is INIT: loading it starts the
 * track's machine afresh. The factory fills the first 96 with its own, twelve a category (m2k_lib.c), the rest INIT.
 * The store keeps a sound as it always has (SND_REC: its name, machine and a kit of 71), and the ADSR's three of every
 * slot in a section of their own ('A') only when a sound uses them: a library without them stores byte for byte as
 * before, and an older build reads it, those three left out */
typedef struct { char name[8]; uint8_t machine, kit[P_COUNT]; } m2k_sound_t;
#define SND_REC (9 + P_COUNT_V21)                  /* a sound's record in the store's 'S' (80) */
_Static_assert(__builtin_offsetof(m2k_sound_t, kit) == 9, "a sound's record is its first SND_REC bytes");
static m2k_sound_t snd[M2K_SOUNDS] POOL;
enum { CAT_ALL, CAT_KICK, CAT_SNARE, CAT_HATS, CAT_PERC, CAT_BASS, CAT_LEAD, CAT_PAD, CAT_FX, CAT_COUNT };
static const char *const CAT_N[CAT_COUNT] = {"ALL", "KICK", "SNARE", "HATS", "PERC", "BASS", "LEAD", "PAD", "FX"};
static const char *const CAT_S[CAT_COUNT] = {"", "KCK", "SNR", "HAT", "PRC", "BAS", "LD", "PAD", "FX"};
static uint8_t snd_cat[M2K_SOUNDS];                /* CAT_*; 0 none */
static uint8_t snd_root[M2K_SOUNDS];               /* the note each was made at: loaded, its track's keys go there */

/* a track's notes: the lowest and highest of its trigs (lo > hi with none) */
static void track_notes(const m2k_pattern_t *p, int t, int *lo, int *hi)
{
    *lo = 127; *hi = 0;
    for (int s = 0; s < M2K_STEPS; s++)
        if (p->step[t][s].flags & ST_TRIG) { int n = p->step[t][s].note; *lo = n < *lo ? n : *lo; *hi = n > *hi ? n : *hi; }
}

/* a track's root: the note its trigs sit on most within the pattern's length (the lowest of equals), so a drum tuned
 * to the key by its note keeps it and a line its home note; -1 without trigs */
static int pat_root(const m2k_pattern_t *p, int t)
{
    uint8_t cnt[128];
    m_set(cnt, 0, sizeof cnt);
    int best = -1, len = CLAMP(p->glob[G_LEN], 1, 64);
    for (int st = 0; st < len; st++) {
        if (!(p->step[t][st].flags & ST_TRIG)) continue;
        int n = p->step[t][st].note & 127;
        if (cnt[n] < 255) cnt[n]++;
        if (best < 0 || cnt[n] > cnt[best] || (cnt[n] == cnt[best] && n < best)) best = n;
    }
    return best;
}

/* a sound's category, guessed from its machine, its knobs and the notes it plays (lo > hi: none known), after bank A's
 * parts (docs/bank-a.md names each track's role). The times on format 16's laws: DEC 19 55 ms, 30 0.12 s, 59 0.6 s; ATK
 * 14 a 23 ms rise (format 15's 12, 24, 48 and 40) */
static int sound_guess(int m, const uint8_t *k, int lo, int hi)
{
    int none = lo > hi, mid = none ? 60 : (lo + hi) / 2, melodic = !none && hi - lo >= 5;
    switch (m) {
    case M_FXC: case M_FXR: case M_FXD: return CAT_FX;
    case M_GNOI: return k[P_DEC] < 59 ? CAT_HATS : CAT_SNARE;
    case M_DKIK: return mid >= 60 ? CAT_PERC : CAT_KICK;      /* a high drum voice: a tom, a perc */
    case M_DSNR: return CAT_SNARE;
    case M_DHAT: case M_DCYM: return CAT_HATS;
    case M_GPRC:                                     /* bright and filtered high: hats; metal and short: perc */
        return k[P_SYN + 2] < 40 && k[P_BASE] >= 32 ? CAT_HATS : k[P_SYN + 2] >= 40 && k[P_DEC] <= 19 ? CAT_PERC : CAT_SNARE;
    case M_GSIN:                                     /* held or a line: a bass (or perc up high); one note: a kick */
        if (none) return CAT_PERC;
        if (k[P_DEC] == 127 || melodic) return mid < 48 ? CAT_BASS : CAT_PERC;
        return k[P_SYN + 3] < 40 && mid >= 55 ? CAT_SNARE : CAT_KICK;
    case M_ENS: return CAT_PAD;
    default:                                         /* short hits: perc; low: bass; a slow attack: a pad */
        return k[P_DEC] <= 30 ? CAT_PERC : mid < 48 ? CAT_BASS : k[P_ATK] >= 14 ? CAT_PAD : CAT_LEAD;
    }
}

static void sound_name(char *o, const m2k_pattern_t *p, int t)   /* "MAMMO3": the pattern's name, the track */
{
    int i = 0;
    for (; i < 5 && p->name[i] && p->name[i] != ' '; i++) o[i] = p->name[i];
    o[i++] = (char)('1' + t);
    o[i] = 0;
}

/* a track's sound into a slot, made at note root (-1: its trigs' root, or its machine's home note without trigs) */
static void sound_save(int slot, const m2k_pattern_t *p, int t, int cat, int root)    /* cat 0: guessed */
{
    m2k_sound_t *d = &snd[slot % M2K_SOUNDS];
    m_set(d, 0, sizeof *d);
    sound_name(d->name, p, t);
    d->machine = p->machine[t];
    m_cpy(d->kit, p->kit[t], P_COUNT);
    int lo, hi;
    track_notes(p, t, &lo, &hi);
    snd_cat[slot % M2K_SOUNDS] = (uint8_t)(cat ? cat : sound_guess(d->machine, d->kit, lo, hi));
    if (root < 0) root = pat_root(p, t);
    snd_root[slot % M2K_SOUNDS] = (uint8_t)(root < 0 ? mach_home(d->machine) : root & 127);
}

/* a sound's note from a store before the notes were kept, by its category: a kick at C2, a bass at C3, the rest C4 */
static int root_guess(int cat) { return cat == CAT_KICK ? 36 : cat == CAT_BASS ? 48 : 60; }

#define FACTORY_DEMOS 4                          /* bank A's slots with a demo, A01-A04: factory() writes those alone */

#define LIB_REV 15                             /* the factory revision the library (m2k_lib.c) last changed in */
static void lib_factory(void);                  /* (m2k_lib.c) */
static void lib_refresh(void);
static void sounds_factory(void) { lib_factory(); }   /* the library, S001..S096; the rest INIT */

/* the blank pattern (INIT): six machines for six purposes, each its INIT sound (a bass, a kit, keys, a pad); those past
 * the sixth as pat_init leaves them (track_blank), so a blank packs as six */
static void pattern_init_blank(m2k_pattern_t *p)
{
    pat_init(p, "INIT");
    static const uint8_t M[M2K_TRACKS6] = {M_GSIN, M_DKIK, M_DSNR, M_DHAT, M_FMS, M_ENS};
    for (int t = 0; t < M2K_TRACKS6; t++) kit_init(p, t, M[t]);
    p->glob[G_TMPO] = 120 - 30;
    p->glob[G_VERB] = 31;                    /* (format 17's: the old 40 and 50 as they sounded) */
    p->glob[G_DAMP] = 86;
}

/* the working patterns from an image: empty or broken slots become blank patterns (on the FM-1 the image becomes the
 * working one, each pattern unpacked as it is wanted) */
static void img_unpack_all(const m2k_img_t *im)
{
#if PAT_CACHE
    if (im != &work_img) m_cpy(&work_img, im, (uint32_t)(sizeof *im - IMG_DATA + im->used));
    pat_drop();
#else
    for (int i = 0; i < M2K_PATTERNS; i++) {
        int n;
        const uint8_t *b = img_get(im, i, &n);
        if (!n || !pat_unpack(PAT(i), b, n)) pattern_init_blank(PAT(i));
    }
#endif
}

/* ---------------------------------------------------------------------------------------------- the actions */

static void work_reset_current(void)
{
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, PAT(cur_pat)->kit[t], P_COUNT);
}

static int pattern_save(void) { return img_put_pat(&proj[cur_proj], cur_pat, PAT(cur_pat)); }

static int pattern_reload(void)
{
    int n;
    const uint8_t *b = img_get(&proj[cur_proj], cur_pat, &n);
    if (n) { if (!pat_unpack(PAT(cur_pat), b, n)) return 0; }
    else pattern_init_blank(PAT(cur_pat));
    work_reset_current();
    return 1;
}

static int project_save(int slot)
{
    char name[8];
    m_cpy(name, proj[slot].name, 8);
    if (!name[0]) { name[0] = 'P'; name[1] = (char)('1' + slot); name[2] = 0; }
    if (!img_pack_all(&proj[slot], name)) return 0;
    cur_proj = slot;
    return 1;
}

static void seq_play(int on);
static void undo_forget(void);                   /* (m2k_ui.c) the undo's copy is of patterns about to go */

static void project_load(int slot)
{
    seq_play(0);
    undo_forget();
    img_unpack_all(&proj[slot]);
    cur_proj = slot;
    cur_pat = 0;
    sq.queued = 0;
    work_reset_current();
    keys_home();
}

/* the settings are the store's too: one turned is saved once it has stood a while (a knob turned is many detents) */
static uint32_t set_at;                          /* the sample of the last change */
static uint8_t set_wait;                         /* 1: one waits to be saved */

/* the factory: project 1 is DEMO (bank A the demos, the rest blank), projects 2-4 blank */
static void projects_factory(void)
{
    blank_set();                                 /* the blank, packed: what an image keeps as nothing */
#if PAT_CACHE
    pat_drop();
    img_clear(&work_img, "WORK");                /* every pattern blank (INIT) until the factory writes bank A */
#endif
    factory();
    for (int i = FACTORY_DEMOS; i < M2K_PATTERNS; i++) pattern_init_blank(PAT(i));
    sounds_factory();
    img_pack_all(&proj[0], "DEMO");
    set_wait = 0;
    for (int k = 1; k < M2K_PROJECTS; k++) {     /* the blank projects: every pattern INIT */
        char nm[8] = {'P', (char)('1' + k), 0};
        img_clear(&proj[k], nm);
    }
    cur_proj = 0;
}

/* ---------------------------------------------------------------------------------------------- the store */
/* everything the instrument keeps, as one blob for the platform to save: the settings, the working patterns,
 * the four projects. "M2K2", version, open project; then images as (name, used, len[], data). */

#define STORE_MAX (16 + S_COUNT + (M2K_PROJECTS + 1) * (12 + 2 * M2K_PATTERNS + IMG_DATA) + 6 + (SND_REC + 2) * M2K_SOUNDS + \
                   2 + 3 * M2K_SOUNDS)
#ifndef M2K_FW
static uint8_t store_buf[STORE_MAX] POOL;
#endif

/* the stream: st_put writes at st_at and moves on, st_get reads so (0 past the end, or when the flash fails) */
static uint32_t st_at;
static int st_bad;

static void st_put(const void *p, uint32_t n)
{
#ifdef M2K_FW
    if (!st_bad && m2k_fw_store_put(st_at, (const uint8_t *)p, n)) st_bad = 1;
#else
    if (st_at + n <= STORE_MAX) m_cpy(store_buf + st_at, p, n);
    else st_bad = 1;
#endif
    st_at += n;
}

static int st_get(void *p, uint32_t n, uint32_t len)
{
    if (st_at + n > len) return 0;
#ifdef M2K_FW
    if (m2k_fw_store_get(st_at, (uint8_t *)p, n)) return 0;
#else
    m_cpy(p, store_buf + st_at, n);
#endif
    st_at += n;
    return 1;
}

/* an image as the stream has it: its name, its length, the lengths of its first n patterns (n may differ from this
 * build's: the FM-1 keeps 16 a project, the browser 64), their data */
static void ser_img(const m2k_img_t *im, int n)
{
    uint8_t h[12], l[2];
    uint32_t used = 0;
    for (int i = 0; i < n && i < M2K_PATTERNS; i++) used += im->len[i];
    m_cpy(h, im->name, 8);
    for (int k = 0; k < 4; k++) h[8 + k] = (uint8_t)(used >> (8 * k));
    st_put(h, 12);
    for (int i = 0; i < n; i++) {
        uint16_t v = i < M2K_PATTERNS ? im->len[i] : 0;
        l[0] = (uint8_t)v; l[1] = (uint8_t)(v >> 8);
        st_put(l, 2);
    }
    st_put(im->data, used);
}

/* an image from the stream, of n patterns: the first this build keeps (those past it skipped, those short of it blank);
 * through the scratch image (an image refused is left as it was), or straight into im. im 0: skipped whole */
static int deser_img(m2k_img_t *im, uint32_t len, int direct, int n)
{
    m2k_img_t *t = direct && im ? im : &work_img;
    uint8_t h[12], l[2];
    uint32_t used, keep = 0, sum = 0;
    if (!st_get(h, 12, len)) return 0;
    used = (uint32_t)h[8] | ((uint32_t)h[9] << 8) | ((uint32_t)h[10] << 16) | ((uint32_t)h[11] << 24);
    if (!im) {                                          /* not kept: past it */
        if (st_at + 2u * (uint32_t)n + used > len) return 0;
        st_at += 2u * (uint32_t)n + used;
        return 1;
    }
    m_cpy(t->name, h, 8);
    t->name[7] = 0;
    m_set(t->len, 0, sizeof t->len);
    for (int i = 0; i < n; i++) {
        if (!st_get(l, 2, len)) return 0;
        uint16_t v = (uint16_t)(l[0] | (l[1] << 8));
        sum += v;
        if (i < M2K_PATTERNS) { t->len[i] = v; keep += v; }
    }
    if (sum != used || keep > IMG_DATA || !st_get(t->data, keep, len)) return 0;
    if (st_at + (used - keep) > len) return 0;
    st_at += used - keep;
    t->used = keep;
    if (!direct) m_cpy(im, t, (uint32_t)(sizeof *im - IMG_DATA + t->used));
    return 1;
}

/* an image in the stream checked and passed over, nothing taken: its lengths add up to its length, and when it is
 * kept, the patterns this build keeps fit an image, each no more than a pattern packs to */
static int img_check(uint32_t len, int n, int kept)
{
    uint8_t h[12], l[2];
    uint32_t used, keep = 0, sum = 0;
    if (!st_get(h, 12, len)) return 0;
    used = (uint32_t)h[8] | ((uint32_t)h[9] << 8) | ((uint32_t)h[10] << 16) | ((uint32_t)h[11] << 24);
    for (int i = 0; i < n; i++) {
        if (!st_get(l, 2, len)) return 0;
        uint32_t v = (uint32_t)(l[0] | (l[1] << 8));
        sum += v;
        if (i < M2K_PATTERNS) { keep += v; if (kept && v > BLOB_ANY) return 0; }   /* (format 21's too: read as six) */
    }
    if (sum != used || (kept && keep > IMG_DATA) || st_at + used > len) return 0;
    st_at += used;
    return 1;
}

/* the sound library after the images, whole when it is there: 'S', its count, the slots (SND_REC each); 'C', its count,
 * the categories; 'R', its count, the notes they were made at; 'A', its count, the ADSR's three of each (SUS FSUS FREL:
 * only when a sound uses them). An older store ends before 'A', 'R', 'C' or all of them */
static int snd_check(uint32_t len)
{
    uint8_t sh[2];
    if (st_at >= len) return 1;
    if (!st_get(sh, 2, len) || sh[0] != 'S' || st_at + sh[1] * (uint32_t)SND_REC > len) return 0;
    st_at += sh[1] * (uint32_t)SND_REC;
    if (st_at >= len) return 1;
    if (!st_get(sh, 2, len) || sh[0] != 'C' || st_at + sh[1] > len) return 0;
    st_at += sh[1];
    if (st_at >= len) return 1;
    if (!st_get(sh, 2, len) || sh[0] != 'R' || st_at + sh[1] > len) return 0;
    st_at += sh[1];
    if (st_at >= len) return 1;
    return st_get(sh, 2, len) && sh[0] == 'A' && st_at + 3u * sh[1] <= len;
}

/* does a sound of a library (snd, or a copy of it: the Move's saver's) use the ADSR's three (an empty slot's kit is
 * never played) */
static int snd_adsr(const m2k_sound_t *s)
{
    for (int i = 0; i < M2K_SOUNDS; i++) if (s[i].name[0] && kit_adsr(s[i].kit)) return 1;
    return 0;
}

#ifndef M2K_FW
uint8_t *m2k_store_buf(int *max)
{
    *max = STORE_MAX;
    return store_buf;
}
#endif

/* the factory's revision: a store saved before it carries the demos it replaced, by revision. 1: the bank rewritten
 * on the Monomachine's chain (version 5); 2: bank A's first four parts after the brief (docs/bank-a.md: HAZE,
 * SPLICE, RIBBON, FACETS); 3: the next four (SMOKE, TORQUE, TUNNEL, MOTEL); 4: brief v3's first four (PYLONS,
 * RIVETS, ORCHARD, SPLINTR), and HAZE, SPLICE and RIBBON set to sound on the per-machine DIST as they did; 5: A04
 * TENFOUR and A16 FOUNDRY, after Radiohead and Boards of Canada; 6: A06 CAROM, A07 VESPER, A12 CUTUP, A13 GRISTLE;
 * 7: A09 MAMMOTH (direction v5) for PYLONS; 8: the bank reordered by energy, MAMMOTH first; 9: CUTUP to A02,
 * RIVETS to A11; 10: a new bank, LO-BIT CLUB, whole; 11: four parts from the owner's sound sheets (CIRCUIT, TIDE,
 * SUSPEND, LANTERN), the rest of the bank blank (docs/bank-a.md); 12: the four on pattern format 16's amp and LFO laws,
 * as they sounded; 13: on format 17's bus (their reverbs, their sends and their delays' turned as an old pattern's);
 * 14: packed as format 18 (Grids' globals at their start), as they sound; 15: the same bank, the sound library its
 * own (m2k_lib.c: 96 sounds, a store's factory sounds of before replaced, the owner's kept) */
#define FACTORY_REV 15
static const char *const OLD_DEMO[FACTORY_REV][16] = {
    {"LATEX", "PVC", "SPU RM", "ELECTRO", "GLITCH", "CHROME", "ACID", "2STEP", "GROOVE", "HYPRPOP", "DUB", "CHIP",
     "HARDGRV", "IDM", "DUBSTEP", "INIT"},
    {"DEEP", "JUNGLE", "DAYDRM", "ELECTRO", "GLITCH", "PRISM", "FOG", "LIQUID", "DREXC", "STUTTR", "SYRUP", "SPU",
     "GROOVE", "ROBOT", "BRAIN", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "FACETS", "GLITCH", "PRISM", "FOG", "LIQUID", "DREXC", "STUTTR", "SYRUP", "SPU",
     "GROOVE", "ROBOT", "BRAIN", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "FACETS", "SMOKE", "TORQUE", "TUNNEL", "MOTEL", "DREXC", "STUTTR", "SYRUP", "SPU",
     "GROOVE", "ROBOT", "BRAIN", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "FACETS", "SMOKE", "TORQUE", "TUNNEL", "MOTEL", "PYLONS", "RIVETS", "SYRUP", "SPU",
     "GROOVE", "ORCHARD", "SPLINTR", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "TENFOUR", "SMOKE", "TORQUE", "TUNNEL", "MOTEL", "PYLONS", "RIVETS", "SYRUP", "SPU",
     "GROOVE", "ORCHARD", "SPLINTR", "FOUNDRY"},
    {"HAZE", "SPLICE", "RIBBON", "TENFOUR", "SMOKE", "CAROM", "VESPER", "MOTEL", "PYLONS", "RIVETS", "SYRUP", "CUTUP",
     "GRISTLE", "ORCHARD", "SPLINTR", "FOUNDRY"},
    {"HAZE", "SPLICE", "RIBBON", "TENFOUR", "SMOKE", "CAROM", "VESPER", "MOTEL", "MAMMOTH", "RIVETS", "SYRUP", "CUTUP",
     "GRISTLE", "ORCHARD", "SPLINTR", "FOUNDRY"},
    {"MAMMOTH", "RIVETS", "HAZE", "GRISTLE", "CAROM", "TENFOUR", "VESPER", "ORCHARD", "SPLINTR", "SMOKE", "CUTUP", "SYRUP",
     "RIBBON", "MOTEL", "FOUNDRY", "SPLICE"},
    {"MAMMOTH", "CUTUP", "HAZE", "GRISTLE", "CAROM", "TENFOUR", "VESPER", "ORCHARD", "SPLINTR", "SMOKE", "RIVETS", "SYRUP",
     "RIBBON", "MOTEL", "FOUNDRY", "SPLICE"},
    {"LUMEN", "PULSAR", "LATTICE", "SPUDUB", "PROCESS", "SQUELCH", "GROWL", "BREAKS", "SPRITES", "OPLL", "JUKE", "CHIPJNG",
     "VIOLET", "DUSKLIT", "FLUTE", "EMBERS"},
    {"CIRCUIT", "TIDE", "SUSPEND", "LANTERN", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT",
     "INIT", "INIT"},
    {"CIRCUIT", "TIDE", "SUSPEND", "LANTERN", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT",
     "INIT", "INIT"},
    {"CIRCUIT", "TIDE", "SUSPEND", "LANTERN", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT",
     "INIT", "INIT"},
    {"CIRCUIT", "TIDE", "SUSPEND", "LANTERN", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT", "INIT",
     "INIT", "INIT"},
};

static int name_is(const char *a, const char *b)
{
    for (int i = 0; i < 8; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; }
    return 1;
}

/* the demos of the revisions whose stores keep them as shipped (10 on): each bank-A slot's packed length and FNV-1a.
 * A slot takes today's demo only while it is byte for byte the one its revision shipped (a demo edited under its name
 * is the owner's). The row for FACTORY_REV is today's bank, held by the self-test (it prints the row it wants): a
 * batch that replaces demos bumps FACTORY_REV and adds it. Before 10 no packed demo survives: by name */
static const struct { uint8_t rev; uint16_t len[16]; uint32_t fnv[16]; } OLD_PACKED[] = {
    {10, {1131, 1083, 919, 1011, 1171, 1203, 1091, 1015, 1307, 1131, 1111, 1175, 767, 847, 839, 759},
     {0x4b103010u, 0x82293976u, 0xe0cfe4cau, 0x4296493eu, 0x3cf1a5d6u, 0x6ded61d3u, 0xb1e2d6b1u, 0xb50ef844u,
      0xc3f1ae57u, 0x68a1d2c2u, 0xe71862c7u, 0x3e87034cu, 0x37721b15u, 0x91e00738u, 0xb7030f9du, 0xbefb958fu}},
    {11, {1091, 935, 879, 911},
     {0x73cc380fu, 0x0f86d30bu, 0xd0488ca5u, 0x507c3f8cu, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u,
      0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u}},
    {12, {1091, 935, 879, 911},
     {0xacff2bd9u, 0x19e58dfau, 0xdce6eb49u, 0xfce8d719u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u,
      0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u}},
    {13, {1091, 935, 879, 911},
     {0x0b6423b9u, 0xbdf29132u, 0x59b9bcbau, 0xb84c63d1u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u,
      0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u}},
    {14, {1104, 948, 892, 924},
     {0x5e08bcdcu, 0x7671d05fu, 0x875724e1u, 0x495ffb00u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u,
      0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u}},
    {15, {1104, 948, 892, 924},
     {0x5e08bcdcu, 0x7671d05fu, 0x875724e1u, 0x495ffb00u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u,
      0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u, 0x811c9dc5u}},
};

static uint32_t fnv(const uint8_t *b, uint32_t n)
{
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}

/* bank-A slot i of a saved image still holds the demo its store's revision shipped */
static int demo_shipped(const m2k_img_t *im, int i, int rev)
{
    const uint8_t *b = im->data + img_off(im, i);
    int n = im->len[i];
    for (unsigned k = 0; k < sizeof OLD_PACKED / sizeof OLD_PACKED[0]; k++)
        if (OLD_PACKED[k].rev == rev) return n == OLD_PACKED[k].len[i] && fnv(b, (uint32_t)n) == OLD_PACKED[k].fnv[i];
    if (!n) return name_is(OLD_DEMO[rev][i], "INIT");             /* by name: an INIT only while it is blank */
    m2k_pattern_t *const o = &pat_tmp;
    if (!pat_unpack(o, b, n) || !name_is(o->name, OLD_DEMO[rev][i])) return 0;
    if (name_is(OLD_DEMO[rev][i], "INIT"))
        for (int t = 0; t < M2K_TRACKS; t++) for (int s = 0; s < M2K_STEPS; s++) if (o->step[t][s].flags & ST_TRIG) return 0;
    return 1;
}

/* a saved image of the DEMO project: a slot of today's demos still holding what its revision shipped takes today's,
 * built in pat[] by the caller. Anything else is the owner's, and a slot with no demo now (A05-A16) is never blanked */
static void refresh_demos(m2k_img_t *im, int rev)
{
    for (int i = 0; i < FACTORY_DEMOS; i++) if (demo_shipped(im, i, rev)) img_put_pat(im, i, PAT(i));
}

/* the store, version 5: "M2K2" 5, the open project and pattern, the factory's revision, how many patterns a project and
 * how many projects it holds; the settings; the working patterns; the projects; the sound library ('S', its count,
 * each slot; 'C' and 'R' so for their categories and notes). n and k as this build keeps them, or fewer (the browser
 * writing a store for the FM-1): an open project past k goes first, as the open one. (4 is the same, its sounds' sends
 * on pattern format 16's laws; 3 its amp and LFOs on format 15's too) */
static int store_pack_nk(int n, int k)
{
    uint8_t h[10] = {'M', '2', 'K', '2', 5, (uint8_t)cur_proj, (uint8_t)cur_pat, FACTORY_REV, (uint8_t)n, (uint8_t)k};
    if (cur_proj >= k) h[5] = 0;
    if (cur_pat >= n) h[6] = 0;
    st_at = 0;
    st_bad = 0;
    st_put(h, 10);
    st_put(store_.set, S_COUNT);
    if (!img_pack_work("WORK")) return 0;
    ser_img(&work_img, n);
    for (int i = 0; i < k; i++) ser_img(&proj[i == 0 && cur_proj >= k ? cur_proj : i < M2K_PROJECTS ? i : 0], n);
    uint8_t sh[2] = {'S', M2K_SOUNDS};
    st_put(sh, 2);
    for (int i = 0; i < M2K_SOUNDS; i++) st_put(&snd[i], SND_REC);
    sh[0] = 'C';                                        /* their categories */
    st_put(sh, 2);
    st_put(snd_cat, sizeof snd_cat);
    sh[0] = 'R';                                        /* the notes they were made at */
    st_put(sh, 2);
    st_put(snd_root, sizeof snd_root);
    if (snd_adsr(snd)) {                                   /* the ADSR's three, when a sound uses them */
        sh[0] = 'A';
        st_put(sh, 2);
        for (int i = 0; i < M2K_SOUNDS; i++) st_put(&snd[i].kit[P_COUNT_V21], 3);
    }
    return st_bad ? 0 : (int)st_at;
}

int m2k_store_pack(void) { return store_pack_nk(M2K_PATTERNS, M2K_PROJECTS); }

int m2k_store_pack_as(int patterns, int projects)
{
    if (patterns < 1 || patterns > M2K_PATTERNS || projects < 1 || projects > M2K_PROJECTS) return 0;
    return store_pack_nk(patterns, projects);
}

int m2k_store_unpack(int len)
{
    uint8_t o[10], set[S_COUNT];
    uint32_t n = (uint32_t)len;
    st_at = 0;
    if (!st_get(o, 5, n) || o[0] != 'M' || o[1] != '2' || o[2] != 'K' || o[3] != '2' || o[4] < 1 || o[4] > 5) return 0;
    int ver = o[4];
    if (!st_get(o + 5, ver == 1 ? 2 : ver == 2 ? 3 : 5, n)) return 0;
    int rev = ver >= 2 ? o[7] : 0;
    int np = ver >= 3 ? o[8] : M2K_PATTERNS, nk = ver >= 3 ? o[9] : M2K_PROJECTS;   /* before 3: this build's own */
    if (np < 1 || np > 64 || nk < 1 || nk > 16) return 0;
    if (!st_get(set, S_COUNT, n)) return 0;
    /* the whole stream first, nothing taken: a store cut short or damaged leaves the instrument as it was (and the
     * FM-1's link writes that back). The working image, the projects (those past this build's passed over), the sounds */
    uint32_t body = st_at;
    for (int i = 0; i <= nk; i++) if (!img_check(n, np, i <= M2K_PROJECTS)) return 0;
    if (ver >= 3 && !snd_check(n)) return 0;
    st_at = body;
    /* then taken whole. (A read failing now is the flash's: the factory, rather than half of each) */
    undo_forget();
#if PAT_CACHE
    pat_drop();                                             /* the working image is about to be replaced */
#endif
    if (!deser_img(&work_img, n, 1, np)) { projects_factory(); return 0; }   /* (the browser's: the scratch image) */
    for (int i = 0; i < nk; i++)
        if (!deser_img(i < M2K_PROJECTS ? &proj[i] : 0, n, 1, np)) { projects_factory(); return 0; }
    for (int i = nk; i < M2K_PROJECTS; i++) {               /* projects the store has not: blank ones */
        char nm[8] = {'P', (char)('1' + i), 0};
        img_clear(&proj[i], nm);
    }
    uint8_t sh[2];
    if (ver >= 3 && st_get(sh, 2, n) && sh[0] == 'S') {   /* the sound library: the slots this build keeps */
        m_set(snd, 0, sizeof snd);
        m_set(snd_cat, 0, sizeof snd_cat);
        int nr = 0, na = 0;                             /* the notes read; the ADSR's read */
        for (int i = 0; i < sh[1]; i++) {
            m2k_sound_t tmp;
            m_set(&tmp, 0, sizeof tmp);
            if (!st_get(&tmp, SND_REC, n)) break;
            if (i >= M2K_SOUNDS) continue;
            snd[i] = tmp;
            snd[i].name[7] = 0;
            if (snd[i].machine >= M_COUNT) snd[i].name[0] = 0;
            for (int k = 0; k < P_COUNT; k++) if (snd[i].kit[k] > 127) snd[i].kit[k] = 127;
            if (ver < 4) feel_kit_v15(snd[i].kit);   /* (3: the sounds on format 15's amp and LFO laws) */
            if (ver < 5) fx_kit_v16(snd[i].kit);     /* (4: their sends on format 16's) */
        }
        if (st_get(sh, 2, n) && sh[0] == 'C') {         /* the categories (a store before them: guessed) */
            for (int i = 0; i < sh[1]; i++) { uint8_t c; if (!st_get(&c, 1, n)) break; if (i < M2K_SOUNDS) snd_cat[i] = c < CAT_COUNT ? c : 0; }
            if (st_get(sh, 2, n) && sh[0] == 'R') {     /* their notes (a store before them: by category) */
                for (int i = 0; i < sh[1]; i++) {
                    uint8_t r;
                    if (!st_get(&r, 1, n)) break;
                    if (i < M2K_SOUNDS) { snd_root[i] = r & 127; nr = i + 1; }
                }
                if (st_get(sh, 2, n) && sh[0] == 'A')   /* their ADSR's (a store without: kit_v21's, as they played) */
                    for (int i = 0; i < sh[1]; i++) {
                        uint8_t a[3];
                        if (!st_get(a, 3, n)) break;
                        if (i >= M2K_SOUNDS) continue;
                        for (int k = 0; k < 3; k++) snd[i].kit[P_COUNT_V21 + k] = a[k] > 127 ? 127 : a[k];
                        na = i + 1;
                    }
            }
        }
        for (int i = na; i < M2K_SOUNDS; i++) if (snd[i].name[0]) kit_v21(snd[i].kit);   /* (an empty slot's never plays) */
        for (int i = 0; i < M2K_SOUNDS; i++) {
            if (snd[i].name[0] && !snd_cat[i]) snd_cat[i] = (uint8_t)sound_guess(snd[i].machine, snd[i].kit, 127, 0);
            if (i >= nr) snd_root[i] = (uint8_t)root_guess(snd_cat[i]);
        }
        if (rev < LIB_REV) lib_refresh();               /* the factory's sounds of before become the library's */
    }
    /* today's demos, where the old ones still are (not with the cache: the factory would write into the working
     * image being read; the FM-1 takes new demos through the Patch Editor) */
    if (!PAT_CACHE && rev < FACTORY_REV && name_is(proj[0].name, "DEMO")) {
        factory();
        refresh_demos(&proj[0], rev);
        if (o[5] == 0) refresh_demos(&work_img, rev);
    }
    img_unpack_all(&work_img);
    for (int i = 0; i < S_COUNT; i++) store_.set[i] = set[i] > 127 ? 127 : set[i];
    set_wait = 0;                                           /* (a setting turned before: replaced) */
    cur_proj = o[5] < M2K_PROJECTS ? o[5] : 0;
    cur_pat = o[6] < M2K_PATTERNS ? o[6] : 0;
    sq.queued = (uint8_t)cur_pat;
    work_reset_current();
    keys_home();
    return 1;
}

#ifndef M2K_FW
/* a project as a file: its saved image */
int m2k_proj_export(int slot)
{
    if (slot < 0 || slot >= M2K_PROJECTS) return 0;
    static const uint8_t h[6] = {'M', '2', 'K', 'P', 2, M2K_PATTERNS};   /* 2: the count of patterns follows */
    st_at = 0;
    st_bad = 0;
    st_put(h, 6);
    ser_img(&proj[slot], M2K_PATTERNS);
    return st_bad ? 0 : (int)st_at;
}

int m2k_proj_import(int slot, int len)
{
    const uint8_t *o = store_buf;
    if (slot < 0 || slot >= M2K_PROJECTS || len < 6 || o[0] != 'M' || o[1] != '2' || o[2] != 'K' || o[3] != 'P' || o[4] < 1 || o[4] > 2) return 0;
    st_at = o[4] == 1 ? 5 : 6;
    return deser_img(&proj[slot], (uint32_t)len, 0, o[4] == 1 ? 64 : o[5]);   /* 1: 64 a project, as the browser's */
}
#endif

void m2k_project(int slot, int save)
{
    if (slot < 0 || slot >= M2K_PROJECTS) return;
    if (save) project_save(slot);
    else project_load(slot);
}

const char *m2k_project_name(int slot) { return proj[slot % M2K_PROJECTS].name; }
int m2k_project_current(void) { return cur_proj; }
