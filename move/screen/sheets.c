/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the screen's design sheets, drawn by the renderer itself (m2k_screen.c) from real states: bank
 * A's patterns playing, a few machines on SYN, a step's locks held, the catalogue scrolled, PERFORM, SYNC on and off.
 * Writes each frame as a 128 x 64 PBM (1 = lit) and a manifest of them with their labels; the motions' frames in
 * frames/<name>/; checks every knob's name and value text against its column (the Move's text audit); times a frame.
 * move/screen/sheets.py makes the PNGs, the overview and the GIFs from them.
 *   cc -O2 -DM2K_TRACKS=8 -Ifirmware/src -Ifirmware/gen -o build/move/screen_sheets move/screen/sheets.c
 *   build/move/screen_sheets build/move/sheets */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef __APPLE__
#include <libproc.h>
#include <unistd.h>
#endif
#include "../../firmware/src/m2k_core.c"
#include "m2k_screen.c"

static char out[600];
static FILE *man;

static void pbm(const char *path, const uint8_t *fb)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P4\n%d %d\n", MS_W, MS_H);
    fwrite(fb, 1, MS_BYTES, f);
    fclose(f);
}

/* a still: NAME.pbm and its line in the manifest (name, label) */
static void shot(const char *name, const char *label, const m2k_scr_t *s)
{
    char p[700];
    snprintf(p, sizeof p, "%s/%s.pbm", out, name);
    pbm(p, m2k_screen(s));
    fprintf(man, "%s\t%s\n", name, label);
}

static int16_t abuf[2 * 4096];
static uint32_t clock_ms;               /* the screen's time: the sound rendered so far */
static void run_ms(int ms)
{
    int frames = ms * 441 / 10;
    frames -= frames % M2K_SUB;
    while (frames > 0) { int n = frames > 4096 ? 4096 : frames; m2k_render(abuf, n); frames -= n; }
    clock_ms += (uint32_t)ms;
}

/* a fresh instrument on pattern n, track t (the seed the Move's tests use) */
static void fresh(int n, int t)
{
    m2k_init(2000);
    m2k_pattern(n);
    m2k_track(t);
    clock_ms = 1000;
}

static m2k_scr_t st(int page, int t)
{
    m2k_scr_t s;
    memset(&s, 0, sizeof s);
    s.page = (uint8_t)page; s.track = (uint8_t)t; s.touch = -1; s.held = 0xFF; s.ms = clock_ms;
    return s;
}

/* the first track of the open pattern playing machine m, else -1 */
static int track_of(int m)
{
    for (int t = 0; t < M2K_TRACKS; t++) if (PAT(cur_pat)->machine[t] == m) return t;
    return -1;
}

/* a frame of a motion: frames/NAME/NNNN.pbm */
static void frame_of(const char *name, int i, const m2k_scr_t *s)
{
    char p[700];
    snprintf(p, sizeof p, "%s/frames/%s", out, name);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/frames/%s/%04d.pbm", out, name, i);
    pbm(p, m2k_screen(s));
}

/* ------------------------------------------------------------------------------------------------ the audit */

/* every page of knobs on every machine: each name within its column and 2 px clear of the next's (where ms_label_x
 * puts them); every value a column writes (a choice's, a number's, a destination's) whole within 14 px, its unit,
 * sign, thousands and decimals aside; a touched value and its unit within the screen at x2 */
static int text_audit(void)
{
    int bad = 0;
    m2k_init(2000);
    for (int m = 0; m < M_COUNT; m++) {
        for (int t = 0; t < M2K_TRACKS; t++) machine_to(t, m);
        for (int pg = 0; pg < MS_PAGES; pg++) {
            if (pg != MS_SYN && m) continue;           /* (the machine names only SYN's) */
            const char *lb[8];
            int xs[8];
            m2k_scr_t s = st(pg, 0);
            for (int k = 0; k < 8; k++) { const pi_t *pi = ms_knob_pi(&s, k); lb[k] = pi ? ms_label(pi) : 0; }
            ms_label_x(lb, xs);
            for (int k = 0; k < 8; k++) {
                if (!lb[k]) continue;
                const int w = ms_tw(lb[k]);
                if (w > MS_LW || xs[k] < k * 16 || xs[k] + w > k * 16 + 16) {
                    printf("audit: %s %s: name %s (%d px) out of its column\n", MS_PAGE_N[pg], MI[m].name, lb[k], w); bad++; }
                if (k < 7 && lb[k + 1] && xs[k + 1] - (xs[k] + w) < 2) {
                    printf("audit: %s %s: names %s %s %d px apart\n", MS_PAGE_N[pg], MI[m].name, lb[k], lb[k + 1], xs[k + 1] - (xs[k] + w)); bad++; }
                const pi_t *pi = ms_knob_pi(&s, k);
                const int pic = ms_pic_of(pi, 0);
                for (int v = kind_min(pi->kind); v <= kind_max(pi->kind); v++) {
                    char o[32], f[32];
                    const char *u;
                    fmt(pi, 0, v, o, &u);
                    char dn[8];
                    s_cpyn(f, pi->kind == K_DEST && v ? ms_dest_name(pinfo(0, v - 1), dn) : pi->kind == K_MCH ? o + 2 : o, sizeof f);
                    if ((pic == MP_CHOICE || pic == MP_TEXT || pic == MP_NUM) && ms_vfit(f, 14)) {
                        printf("audit: %s %s %s: value %s cut to %s\n", MS_PAGE_N[pg], MI[m].name, pi->s4, o, f); bad++; break; }
                    const int big = ms_tw_z(o, 2) + (*u ? ms_tw(u) + 4 : 0);
                    if (big > MS_W - 4) { printf("audit: %s %s %s: touched %s %s is %d px at x2\n", MS_PAGE_N[pg], MI[m].name, pi->s4, o, u, big); bad++; break; }
                }
            }
        }
    }
    printf("audit: %s\n", bad ? "FAILED" : "every name and value fits");
    return bad;
}

/* -------------------------------------------------------------------------------------------- the time a frame */

static uint64_t instr(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri) == 0) return ri.ri_instructions;
#endif
    return 0;
}

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

static void timing(FILE *f)
{
    fresh(0, 0);
    m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
    seq_play(1);
    run_ms(1500);
    static const int P[] = {MS_HOME, MS_SYN, MS_AMP, MS_FILT, MS_EFX, MS_DSTR, MS_ARP, MS_LFO1, MS_GLO, MS_GRID, MS_SET,
                            MS_CAT, MS_PERF, MS_LIST};
    const int N = 2000;
    double worst = 0, sum = 0;
    uint64_t iworst = 0;
    fprintf(f, "page      us/frame  instructions/frame  (host, -O2, %d frames each)\n", N);
    for (unsigned i = 0; i < sizeof P / sizeof P[0]; i++) {
        m2k_scr_t s = st(P[i], 1);
        s.cat_sel = 20;
        volatile uint8_t sink = 0;
        uint64_t i0 = instr();
        double t0 = now_us();
        for (int k = 0; k < N; k++) { s.ms = clock_ms + (uint32_t)k * 33u; sink ^= m2k_screen(&s)[k & 1023]; }
        double us = (now_us() - t0) / N;
        uint64_t in = (instr() - i0) / (uint64_t)N;
        (void)sink;
        sum += us;
        if (us > worst) worst = us;
        if (in > iworst) iworst = in;
        fprintf(f, "%-8s  %8.2f  %18llu\n", MS_PAGE_N[P[i]], us, (unsigned long long)in);
        /* the same page with a knob touched */
        if (P[i] == MS_AMP) {
            s.touch = 2;
            i0 = instr(); t0 = now_us();
            for (int k = 0; k < N; k++) { s.ms = clock_ms + (uint32_t)k * 33u; sink ^= m2k_screen(&s)[k & 1023]; }
            us = (now_us() - t0) / N; in = (instr() - i0) / (uint64_t)N;
            if (us > worst) worst = us;
            if (in > iworst) iworst = in;
            fprintf(f, "%-8s  %8.2f  %18llu\n", "AMP+TCH", us, (unsigned long long)in);
        }
    }
    fprintf(f, "worst %.2f us, mean %.2f us, most instructions %llu a frame\n", worst, sum / (sizeof P / sizeof P[0]),
            (unsigned long long)iworst);
}

/* ------------------------------------------------------------------------------------------- the specimens */

/* the type at x1, every glyph the font has, and the line icons, as two screens */
static void specimens(void)
{
    m_set(ms_fb, 0, sizeof ms_fb);
    char row[40];
    for (int r = 0; r < 4; r++) {
        int n = 0;
        for (int c = 32 + r * 16; c < 48 + r * 16; c++) { row[n++] = (char)c; row[n++] = ' '; }
        row[n] = 0;
        ms_text(1, 7 + r * 8, row, 1);
    }
    ms_text(1, 41, "ATCK HOLD DEC REL 1/16 +12 C#3", 1);
    ms_text_z(1, 52, "120", 2, 1);
    ms_text_z(36, 62, "-42MS", 3, 1);
    char p[700];
    snprintf(p, sizeof p, "%s/type.pbm", out);
    pbm(p, ms_fb);
    fprintf(man, "type\tTHE TYPE AT X1, X2, X3\n");

    m_set(ms_fb, 0, sizeof ms_fb);
    for (int i = 0; i < I_COUNT; i++) ms_icon(i, 2 + (i % 10) * 12, 7 + (i / 10) * 9, 1);
    /* the knobs' pictures, each kind once */
    static const struct { int code; int v; } K[8] = {{P_DIST, 90}, {P_VOL, 100}, {P_ATK, 40}, {P_DEC, 70},
                                                    {P_BASE, 40}, {P_LPQ, 100}, {P_PAN, 30}, {P_LFO + L_WAVE, 40}};
    m2k_init(2000);
    for (int k = 0; k < 8; k++) {
        const pi_t *pi = pinfo(0, K[k].code);
        char o[16];
        const char *u;
        fmt(pi, 0, K[k].v, o, &u);
        ms_pic(ms_pic_of(pi, 0), pi, k * 16, MS_KP, K[k].v, ms_q(pi, K[k].v), o, -1);
        const char *lb = ms_label(pi);
        int w = ms_tw(lb);
        ms_text(k * 16 + (w >= MS_LW ? 0 : (16 - w) / 2), MS_KN, lb, 1);
    }
    snprintf(p, sizeof p, "%s/icons.pbm", out);
    pbm(p, ms_fb);
    fprintf(man, "icons\tTHE ICONS, THE KNOBS' PICTURES\n");
}

/* ------------------------------------------------------------------------------------------------- the scenes */

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sheets DIR\n"); return 1; }
    snprintf(out, sizeof out, "%s", argv[1]);
    mkdir(out, 0755);
    char p[700];
    snprintf(p, sizeof p, "%s/frames", out);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/manifest.tsv", out);
    man = fopen(p, "w");
    if (!man) { perror(p); return 1; }

    if (argc > 2 && !strcmp(argv[2], "--machines")) {
        m2k_init(2000);
        for (int n = 0; n < 4; n++) {
            m2k_pattern(n);
            printf("%s %-8s:", (char[]){(char)('A'), (char)('0'), (char)('1' + n), 0}, pat_name(n));
            for (int t = 0; t < M2K_TRACKS; t++) printf(" T%d %s", t + 1, MI[PAT(cur_pat)->machine[t]].name);
            printf("\n   LFOs:");
            for (int t = 0; t < M2K_TRACKS; t++)
                for (int l = 0; l < 3; l++) { int d = PAT(cur_pat)->kit[t][P_LFO + 8 * l + L_DEST]; if (d) printf(" T%d.%d>%s", t + 1, l + 1, pinfo(t, d - 1)->s4); }
            printf("\n   locks:");
            for (int t = 0; t < M2K_TRACKS; t++) { int n = 0; for (int i = 0; i < 64; i++) n += has_locks(t, i); if (n) printf(" T%d:%d", t + 1, n); }
            printf("  DSND:");
            for (int t = 0; t < M2K_TRACKS; t++) printf(" %d", PAT(cur_pat)->kit[t][P_DSND]);
            printf("\n");
        }
        return 0;
    }

    m2k_scr_t s;
    /* HOME: A01 stopped; A03 playing, a track in view */
    fresh(0, 0);
    s = st(MS_HOME, 0);
    shot("01_home", "HOME  A01 STOPPED", &s);
    fresh(1, 0);
    seq_play(1);
    run_ms(2400);
    s = st(MS_HOME, 0);
    s.spage = (uint8_t)(track_pos(0) / 16);
    shot("02_home_play", "HOME  A02 PLAYING", &s);
    s.touch = 5;
    shot("02_home_touch", "HOME  SPD TOUCHED", &s);

    /* SYN on machines bank A plays (found in its patterns), and some it does not (on T7) */
    static const uint8_t SYNM[6] = {M_FMD, M_ENS, M_DKIK, M_VOX, M_SAW, M_WAV};
    for (int i = 0; i < 6; i++) {
        int n = 0, t = -1;
        for (n = 0; n < 4 && t < 0; n++) { fresh(n, 0); t = track_of(SYNM[i]); }
        if (t < 0) { fresh(0, 0); t = 6; machine_to(t, SYNM[i]); }
        m2k_track(t);
        seq_play(1);
        run_ms(1200);
        s = st(MS_SYN, t);
        char nm[32], lb[48];
        snprintf(nm, sizeof nm, "03_syn_%d", i);
        snprintf(lb, sizeof lb, "SYN  %s", MI[SYNM[i]].name);
        shot(nm, lb, &s);
    }

    /* the track's pages on A02's T1 (SWAV-ENS, its LFOs on WDTH), playing; EFX on T5 (FM+STAT, the ping-pong) */
    const int mt = 0;
    fresh(1, mt);
    seq_play(1);
    run_ms(1700);
    static const struct { int page, t; const char *name, *label; } PG[] = {
        {MS_AMP, 0, "04_amp", "AMP  THE ENVELOPE, LIVE"}, {MS_FILT, 0, "05_filt", "FILT  THE RESPONSE, LIVE"},
        {MS_EFX, 4, "06_efx", "EFX  EQ, RATE, PING-PONG"}, {MS_DSTR, 0, "07_dstr", "DSTR  THE CRUSH, THE MOD"},
        {MS_LFO1, 0, "09_lfo1", "LFO1  TO WDTH, SHALLOW"}, {MS_GLO, 0, "10_glo", "GLO  MONO2K'S TEMPO"},
        {MS_GRID, 0, "12_grid", "GRID"}, {MS_SET, 0, "13_set", "SET"}};
    for (unsigned i = 0; i < sizeof PG / sizeof PG[0]; i++) {
        s = st(PG[i].page, PG[i].t);
        shot(PG[i].name, PG[i].label, &s);
    }
    {                                                  /* (staged: LFO2 a deep sine on VOL, a bar a cycle) */
        uint8_t *k = PAT(cur_pat)->kit[mt];
        k[P_LFO + 8 + L_DEST] = P_VOL + 1; k[P_LFO + 8 + L_WAVE] = 20; k[P_LFO + 8 + L_DPTH] = 124;
        k[P_LFO + 8 + L_SPD] = 60; k[P_LFO + 8 + L_TRIG] = 0;
        run_ms(700);
        s = st(MS_LFO2, mt);
        shot("09_lfo2", "LFO2  SIN TO VOL, DEEP", &s);
    }
    s = st(MS_GLO, mt);
    s.sync = 1;
    shot("11_glo_sync", "GLO  SYNC: THE MOVE'S TEMPO", &s);
    s = st(MS_AMP, mt);
    s.sync = 1;
    s.touch = 0;
    shot("14_touch_atk", "ATK TOUCHED  (SYNC ON)", &s);
    s = st(MS_FILT, mt);
    s.touch = 0;
    shot("14_touch_base", "BASE TOUCHED", &s);

    /* ARP with notes held */
    fresh(1, mt);
    PAT(cur_pat)->kit[mt][P_AMOD] = 22;                /* (UP) */
    PAT(cur_pat)->kit[mt][P_ARNG] = 40;
    seq_play(1);
    live_on_t(mt, 48, 100); live_on_t(mt, 55, 100); live_on_t(mt, 60, 100);
    sq.arp_hold[mt] = 1;
    run_ms(900);
    s = st(MS_ARP, mt);
    shot("08_arp", "ARP  HELD, PLAYING", &s);

    /* a step held: its locks (A02's T1 has eight); a knob touched on one of them */
    fresh(1, mt);
    int ls = -1, lt = mt;
    for (int i = 0; i < 64 && ls < 0; i++) if (has_locks(mt, i)) ls = i;
    if (ls < 0) { ls = 4; lock_set(mt, 4, P_BASE, 90); lock_set(mt, 4, P_LPQ, 100); }
    m2k_track(lt);
    int lp = -1;
    for (int i = 0; i < M2K_LOCKS; i++) if (PAT(cur_pat)->lock[i].trk == lt && PAT(cur_pat)->lock[i].step == ls) { lp = PAT(cur_pat)->lock[i].param; break; }
    const int lpage = lp < 8 ? MS_SYN : lp < 16 ? MS_AMP : lp < 24 ? MS_FILT : lp < 32 ? MS_EFX : lp < 36 ? MS_DSTR : lp < 42 ? MS_ARP
                    : lp < 66 ? MS_LFO1 + (lp - 42) / 8 : MS_DSTR;
    s = st(lpage, lt);
    s.held = (uint8_t)ls;
    s.spage = (uint8_t)(ls / 16);
    shot("15_plock", "A STEP HELD: ITS LOCKS LIT", &s);
    for (int k = 0; k < 8; k++) if (m2k_screen_param(lpage, lt, k) == lp) s.touch = (int8_t)k;
    shot("15_plock_touch", "A LOCKED KNOB TOUCHED", &s);

    /* the catalogue */
    fresh(0, 3);
    s = st(MS_CAT, 3);
    s.cat_sel = 21;
    shot("16_cat", "SOUNDS  SCROLLED, ALL", &s);
    s.cat_cat = CAT_BASS;
    for (int i = 0; i < M2K_SOUNDS; i++) if (snd_cat[i] == CAT_BASS) { s.cat_sel = (uint8_t)(i + 3); break; }
    shot("17_cat_bass", "SOUNDS  BASS ONLY", &s);

    /* PERFORM */
    fresh(1, 0);
    seq_play(1);
    run_ms(1000);
    s = st(MS_PERF, 0);
    shot("18_perf", "PERFORM  NOTHING HELD", &s);
    perf.on[PF_R8] = 1;
    shot("18_perf_one", "PERFORM  RPT 1/8", &s);
    perf.on[PF_CRSH] = 1; perf.on[PF_LPF] = 1;
    shot("18_perf_three", "PERFORM  THREE AT ONCE", &s);
    m_set(perf.on, 0, sizeof perf.on);

    /* the page list; REC, a mute, the eighth track */
    fresh(0, 1);
    s = st(MS_LIST, 1);
    s.list = MS_FILT;
    shot("19_list", "PAGES  THE JOG CLICKED", &s);
    fresh(3, 7);
    PAT(cur_pat)->mute |= 1u << 7;
    sq.rec = 1;
    seq_play(1);
    run_ms(1500);
    s = st(MS_HOME, 7);
    s.shift = 1;
    s.spage = (uint8_t)(track_pos(0) / 16);
    shot("20_home_shift", "HOME  T8 MUTED, REC, SHIFT", &s);
    s = st(MS_SYN, 7);
    shot("21_syn_t8", "SYN  T8 MUTED, REC", &s);

    specimens();

    /* the motions: a knob turned (A02 T1's DEC on AMP, touched, turned down and back, let go: 3 s); the pages turned by
     * the jog (cut, as calm motion has them); the playhead over two bars of A01 */
    fresh(1, mt);
    seq_play(1);
    run_ms(500);
    const int d0 = PAT(cur_pat)->kit[mt][P_DEC];
    for (int i = 0; i < 60; i++) {
        s = st(MS_AMP, mt);
        if (i >= 6 && i < 52) {
            s.touch = 2;
            int a = i - 6, v = a < 23 ? d0 - a : d0 - 23 + (a - 23);
            PAT(cur_pat)->kit[mt][P_DEC] = (uint8_t)CLAMP(v, 0, 127);
        }
        run_ms(50);
        s.ms = clock_ms;
        frame_of("knob", i, &s);
    }
    PAT(cur_pat)->kit[mt][P_DEC] = (uint8_t)d0;
    static const uint8_t TOUR[6] = {MS_HOME, MS_SYN, MS_AMP, MS_FILT, MS_EFX, MS_LFO1};
    for (int i = 0; i < 6 * 14; i++) {
        s = st(TOUR[i / 14], mt);
        if (TOUR[i / 14] == MS_HOME) s.spage = (uint8_t)(track_pos(mt) / 16);
        run_ms(50);
        s.ms = clock_ms;
        frame_of("pages", i, &s);
    }
    fresh(0, 0);
    seq_play(1);
    for (int i = 0; i < 80; i++) {
        run_ms(50);
        s = st(MS_HOME, 0);
        s.spage = (uint8_t)(track_pos(0) / 16);
        frame_of("play", i, &s);
    }
    fclose(man);

    int bad = text_audit();
    snprintf(p, sizeof p, "%s/timing.txt", out);
    FILE *f = fopen(p, "w");
    timing(stdout);
    if (f) { timing(f); fclose(f); }
    return bad ? 1 : 0;
}
