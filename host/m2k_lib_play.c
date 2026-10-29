/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound library's audition (m2k_host DIR --library renders it, m2k_draw --library counts what each sound costs):
 * library sound i alone on T1 of B01 (INIT, its other tracks muted, the bus as INIT leaves it: no drive, no tape, no
 * hiss, no glitch, the sounds send nothing to the reverb), 120 BPM, a phrase for its category at the sound's note */

enum { PH_KICK, PH_SNARE, PH_HATS, PH_OPEN, PH_CRASH, PH_PERC, PH_BASS, PH_LEAD, PH_PAD, PH_FX, PH_HITS, PH_ONE,
       PH_COUNT };
typedef struct { uint8_t step; int8_t note; uint8_t flags; } ph_t;   /* note: semitones over the sound's; 0xFF ends */
#define PH_END {0xFF, 0, 0}
static const ph_t PH_K[] = {{0, 0, 0}, {4, 0, 0}, {8, 0, 0}, {12, 0, 0}, {16, 0, 0}, PH_END};    /* on the beats */
static const ph_t PH_S[] = {{4, 0, 0}, {12, 0, 0}, {20, 0, 0}, PH_END};                        /* 2 and 4 */
static const ph_t PH_H[] = {{0, 0, 0}, {2, 0, 0}, {4, 0, 0}, {6, 0, 0}, {8, 0, 0}, {10, 0, 0}, {12, 0, 0}, {14, 0, 0},
                            {16, 0, 0}, {18, 0, 0}, {20, 0, 0}, {22, 0, 0}, PH_END};             /* eighths */
static const ph_t PH_O[] = {{2, 0, 0}, {6, 0, 0}, {10, 0, 0}, {14, 0, 0}, {18, 0, 0}, {22, 0, 0}, PH_END};   /* offbeats */
static const ph_t PH_C[] = {{0, 0, 0}, {16, 0, 0}, PH_END};
static const ph_t PH_P[] = {{0, 0, 0}, {3, 0, 0}, {6, 0, 0}, {10, 0, 0}, {12, 0, 0}, {16, 0, 0}, {19, 0, 0}, {22, 0, 0},
                            PH_END};                                                           /* a syncopated bar */
static const ph_t PH_B[] = {{0, 0, ST_ACC}, {3, 0, 0}, {6, 12, 0}, {8, 10, ST_SLD}, {10, 7, 0}, {12, 0, 0}, {14, 3, 0},
                            {16, 0, ST_ACC}, {19, 0, 0}, {22, 12, 0}, PH_END};                 /* a riff, one slide */
static const ph_t PH_L[] = {{0, 7, 0}, {2, 10, 0}, {4, 12, 0}, {7, 10, 0}, {8, 7, 0}, {10, 5, 0}, {12, 3, 0}, {14, 5, 0},
                            {16, 0, 0}, PH_END};                                               /* a minor phrase */
static const ph_t PH_D[] = {{0, 0, 0}, {16, 5, 0}, PH_END};                                    /* i, then iv */
static const ph_t PH_F[] = {{0, 0, 0}, {16, 0, 0}, PH_END};                                    /* two gestures */
static const ph_t PH_X[] = {{0, 0, 0}, {6, 0, 0}, {12, 0, 0}, {16, 0, 0}, {22, 0, 0}, PH_END}; /* scattered hits */
static const ph_t PH_1[] = {{0, 0, 0}, PH_END};                                                /* one, held on */
static const ph_t *const PHRASE[PH_COUNT] = {PH_K, PH_S, PH_H, PH_O, PH_C, PH_P, PH_B, PH_L, PH_D, PH_F, PH_X, PH_1};
static const char *const PH_NAME[PH_COUNT] = {"kick", "snare", "hats", "offbeats", "crash", "perc", "riff", "phrase",
                                              "chords", "gestures", "hits", "one"};
static const uint8_t PH_SECS[PH_COUNT] = {3, 3, 3, 3, 4, 3, 3, 3, 4, 4, 4, 4};

/* by its category, a few by their character */
static int lib_phrase(int i)
{
    static const struct { const char *name; uint8_t ph; } OWN[] = {
        {"OPENHAT", PH_OPEN}, {"SIZZLE", PH_OPEN}, {"CRASH", PH_CRASH}, {"GLITCH", PH_HITS}, {"LASER", PH_HITS},
        {"RINGHIT", PH_HITS}, {"DRONE", PH_ONE}, {"STATIC", PH_ONE}, {"SCRAPE", PH_ONE}, {"ROBOT", PH_ONE},
    };
    for (unsigned k = 0; k < sizeof OWN / sizeof OWN[0]; k++) if (!strcmp(LIB[i].name, OWN[k].name)) return OWN[k].ph;
    static const uint8_t BY_CAT[CAT_COUNT] = {PH_FX, PH_KICK, PH_SNARE, PH_HATS, PH_PERC, PH_BASS, PH_LEAD, PH_PAD, PH_FX};
    return BY_CAT[LIB[i].cat];
}

/* sound i on track t, playing its phrase from the start (trigs 0: none, the same pattern silent), the others muted;
 * the frames to render. Its LFOs move t (as LOAD on SYN > SND has them) */
static int lib_setup_on(int i, int trigs, int t)
{
    m2k_init(2000);
    m2k_pattern_t *p = PAT(16);
    pattern_init_blank(p);
    p->glob[G_TMPO] = 120 - 30;
    p->glob[G_LEN] = 32;
    for (int k = 0; k < M2K_TRACKS; k++) for (int s = 0; s < M2K_STEPS; s++) p->step[k][s].flags = 0;
    p->mute = (uint8_t)(0x3F & ~(1u << t));
    m2k_sound_t sd;
    lib_sound(i, &sd);
    p->machine[t] = sd.machine;
    m_cpy(p->kit[t], sd.kit, P_COUNT);
    for (int l = 0; l < 3; l++) p->kit[t][P_LFO + 8 * l + L_TRCK] = enc(t, M2K_TRACKS);
    int ph = lib_phrase(i);
    for (const ph_t *s = PHRASE[ph]; trigs && s->step != 0xFF; s++)
        p->step[t][s->step] = (m2k_step_t){(uint8_t)(LIB[i].root + s->note), (uint8_t)(ST_TRIG | s->flags), C_ALWAYS, 0};
    mon.peak[t] = 0;                               /* (the track's peak before LEVL, the render's: no frame decays it) */
    cur_pat = 16;
    sq.queued = 16;
    lockmap_build();
    for (int k = 0; k < M2K_TRACKS; k++) m_cpy(trk[k].eff, p->kit[k], P_COUNT);
    seq_play(1);
    return PH_SECS[ph] * M2K_SR / 128 * 128;
}

static int lib_setup(int i, int trigs) { return lib_setup_on(i, trigs, 0); }   /* on T1 */
