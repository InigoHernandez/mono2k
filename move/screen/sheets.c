/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the screen's and the pads' design sheets, drawn by dsp.so's own code. This builds move/m2k_move.c
 * (the plugin, the controller, the renderer, the core: one unit) for the Mac and drives it through its plugin API as
 * Schwung does (on_midi for the pads and the steps, set_param "in" for the knobs, the jog and the buttons, 128-frame
 * renders), so every frame is what the drawer draws from the published page state, and every LED what ui.js lights.
 * Writes, into DIR:
 *   NAME.pbm, manifest.tsv     each still (128 x 64, 1 = lit) and its label
 *   frames/NAME/NNNN.pbm, .led the motions' frames, each with its 63 LEDs (hex)
 *   padmap_*.tsv               the 32 pads, 16 steps and 4 track buttons of a state: their labels and palette colours
 *   timing.txt                 the cost a frame: the publish (the audio thread's) and the drawing (the drawer's)
 * and checks: every value a column writes and every touched name and value fits; the header's two ends never meet; a
 * frame drawn later from the same view, with the core in another state, is the same frame (the drawing reads the view
 * alone). move/screen/sheets.py makes the PNGs, the overview, the pad map and the GIFs.
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
#include "../m2k_move.c"

static char out[600];
static FILE *man;
static uint8_t sfb[MS_BYTES];

/* ------------------------------------------------------------------------------------------ the plugin, driven */

static int16_t sblk[256];
static void blocks(int n) { while (n-- > 0) v2_render(&mv, sblk, 128); }
static void run_ms(int ms) { blocks(ms * 441 / 1280); }
static void in(const char *s) { v2_set_param(&mv, "in", s); }
static void note(int n, int on, int vel) { uint8_t m[3] = {(uint8_t)(on ? 0x90 : 0x80), (uint8_t)n, (uint8_t)(on ? vel : 0)}; v2_on_midi(&mv, m, 3, MOVE_MIDI_SOURCE_INTERNAL); }
static void pad(int i, int on) { note(68 + i, on, 100); }
static void tap_pad(int i) { pad(i, 1); blocks(1); pad(i, 0); blocks(1); }
static void step(int k, int on) { note(16 + k, on, 100); }
static void btn(int cc, int v) { char s[32]; snprintf(s, sizeof s, "B %d %d", cc, v); in(s); }
static void tap(int cc) { btn(cc, 127); blocks(1); btn(cc, 0); blocks(1); }
static void knob(int k, int d) { char s[32]; snprintf(s, sizeof s, "K %d %d", k, d); in(s); }
static void tempo(int k, int d10, int coarse) { char s[40]; snprintf(s, sizeof s, "P %d %d %d", k, d10, coarse); in(s); }
static void touch(int k, int on) { char s[32]; snprintf(s, sizeof s, "T %d %d", k, on); in(s); }
static void jog(int d) { char s[32]; snprintf(s, sizeof s, "J %d", d); in(s); }
static void page_go(int page) { tap_pad(MS_PAD0 + ms_pad_of(page)); }
static void track(int t) { char s[8]; snprintf(s, sizeof s, "%d", t); v2_set_param(&mv, "track", s); blocks(1); }
static void play(int on) { v2_set_param(&mv, "play", on ? "1" : "0"); blocks(1); }

/* the plugin as the Move opens it: the tests' seed, pattern n, no store, the render drawing (no threads), Move's tempo
 * (SYNC, the rule on the Move: no host here, so the pattern's number shows) */
static void open_at(int pattern)
{
    if (mv.live) v2_destroy(&mv);
    char js[200];
    snprintf(js, sizeof js, "{\"defaults\": {\"seed\": 2000, \"pattern\": %d, \"data_dir\": \"\", \"draw_thread\": 0}}", pattern);
    if (!v2_create("", js)) { fprintf(stderr, "create failed\n"); exit(1); }
    ck.sync = MV_SYNC_MOVE;
    blocks(8);
}

/* ------------------------------------------------------------------------------------------------ the output */

static void pbm(const char *path, const uint8_t *fb)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P4\n%d %d\n", MS_W, MS_H);
    fwrite(fb, 1, MS_BYTES, f);
    fclose(f);
}

static void leds_out(const char *path, const mv_leds_t *l)
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    const uint8_t *g[4] = {l->step, l->pad, l->trk, l->btn};
    const int n[4] = {16, 32, 4, MV_NBTN};
    for (int a = 0; a < 4; a++) for (int k = 0; k < n[a]; k++) fprintf(f, "%02X", g[a][k]);
    fprintf(f, "\n");
    fclose(f);
}

/* the views of the stills, kept to draw again at the end with the core elsewhere */
#define NKEEP 64
static ms_view_t keep_v[NKEEP];
static uint8_t keep_f[NKEEP][MS_BYTES];
static char keep_n[NKEEP][32];
static int nkeep;

/* the page state published now (as the render publishes it), drawn as the drawer draws it */
static const mv_view_t *now_view(void)
{
    view_publish();
    m2k_screen_draw(&vw_now.scr, sfb);
    return &vw_now;
}

static void shot(const char *name, const char *label)
{
    const mv_view_t *v = now_view();
    char p[700];
    snprintf(p, sizeof p, "%s/%s.pbm", out, name);
    pbm(p, sfb);
    fprintf(man, "%s\t%s\n", name, label);
    if (nkeep < NKEEP) {
        keep_v[nkeep] = v->scr;
        m_cpy(keep_f[nkeep], sfb, MS_BYTES);
        s_cpyn(keep_n[nkeep], name, sizeof keep_n[nkeep]);
        nkeep++;
    }
}

static void frame_of(const char *name, int i)
{
    const mv_view_t *v = now_view();
    char p[700];
    snprintf(p, sizeof p, "%s/frames/%s", out, name);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/frames/%s/%04d.pbm", out, name, i);
    pbm(p, sfb);
    snprintf(p, sizeof p, "%s/frames/%s/%04d.led", out, name, i);
    leds_out(p, &v->led);
}

/* a state's pads, steps and track buttons: each its label (what it does there) and its palette colour */
static void padmap(const char *name)
{
    const mv_view_t *v = now_view();
    char p[700];
    snprintf(p, sizeof p, "%s/padmap_%s.tsv", out, name);
    FILE *f = fopen(p, "w");
    if (!f) { perror(p); exit(1); }
    for (int i = 0; i < 32; i++) {
        char lb[16] = "";
        if (i >= MS_PAD0) s_cpyn(lb, m2k_screen_page_name(MS_PAD_PAGE[i - MS_PAD0]), sizeof lb);
        else if (mc.perf) { int e = perf_of_key(i); if (e >= 0) s_cpyn(lb, PF_NAME[e], sizeof lb); }
        else { int n = key_note(i); if (n >= 0) s_note(lb, n); }
        fprintf(f, "pad\t%d\t%s\t%d\n", i, lb, v->led.pad[i]);
    }
    for (int k = 0; k < 16; k++) { char lb[8]; s_int(lb, k + 1); fprintf(f, "step\t%d\t%s\t%d\n", k, lb, v->led.step[k]); }
    for (int b = 0; b < 4; b++) fprintf(f, "trk\t%d\tT%d\t%d\n", b, b + 1 + (mc.shift ? 4 : 0), v->led.trk[b]);
    fprintf(f, "screen\t%s\n", m2k_screen_page_name(v->scr.page));
    fclose(f);
    snprintf(p, sizeof p, "%s/padmap_%s.pbm", out, name);
    pbm(p, sfb);
}

/* ------------------------------------------------------------------------------------------------ the audit */

static int bad;
static void fail(const char *what) { printf("audit: %s\n", what); bad++; }

/* every page of knobs on every machine, every value: what a column writes fits its 14 px whole; touched, the name (or
 * its four letters) fits beside LOCK and STEP 64, the value and its unit at x2 within the screen */
static void text_audit(void)
{
    open_at(0);
    char w[160];
    for (int m = 0; m < M_COUNT; m++) {
        for (int t = 0; t < M2K_TRACKS; t++) machine_to(t, m);
        for (int pg = 0; pg < MS_PAGES; pg++) {
            if (pg != MS_SYN && m) continue;
            for (int k = 0; k < 8; k++) {
                const int code = m2k_screen_param(pg, 0, k), t = m2k_screen_knob_track(pg, 0, k);
                const pi_t *pi = code == NONE ? 0 : pinfo(t, code);
                if (!pi || pi->s4[0] == '-') continue;
                const int pic = ms_pic_of(pi, t);
                char nm[40];
                s_cpy(s_cpy(nm, "A "), pi->name);
                const int room = MS_W - 1 - (ms_tw("LOCK") + 8) - (ms_tw("STEP 64") + 6) - 2;
                if (ms_tw(nm) > room && ms_tw(pi->s4) > room) {
                    snprintf(w, sizeof w, "%s %s: touched name %s too long", MS_PAGE_N[pg], MI[m].name, pi->name); fail(w); }
                for (int v = kind_min(pi->kind); v <= kind_max(pi->kind); v++) {
                    char o[32], f[32], dn[8];
                    const char *u;
                    fmt(pi, t, v, o, &u);
                    s_cpyn(f, pi->kind == K_DEST && v ? ms_dest_name(pinfo(t, v - 1), dn) : pi->kind == K_MCH ? o + 2 : o, sizeof f);
                    if ((pic == MP_CHOICE || pic == MP_TEXT || pic == MP_NUM) && ms_vfit(f, 14)) {
                        snprintf(w, sizeof w, "%s %s %s: value %s cut to %s", MS_PAGE_N[pg], MI[m].name, pi->s4, o, f); fail(w); break; }
                    const int big = ms_tw_z(o, 2) + (*u ? ms_tw(u) + 4 : 0);
                    if (big > MS_W - 4) {
                        snprintf(w, sizeof w, "%s %s %s: touched %s %s is %d px at x2", MS_PAGE_N[pg], MI[m].name, pi->s4, o, u, big); fail(w); break; }
                }
            }
        }
    }
}

/* the fullest header on every page: track 8 muted, REC, INT, a pattern queued, four bars; its two ends never within
 * 3 px (its extras give way: ms_hdr_gap, as d_header laid it) */
static int hdr_gap(const ms_view_t *v)
{
    uint8_t scratch[MS_BYTES];
    m2k_screen_draw(v, scratch);
    return ms_hdr_gap;
}

static void header_audit(void)
{
    open_at(3);
    ck.sync = MV_SYNC_INT;             /* (MONO2K's own tempo: INT in the header) */
    track(7);
    machine_to(7, M_FMD);
    PAT(cur_pat)->glob[G_LEN] = 64;
    arp_hold_set(7, 1);
    mute_to(7, 1);
    tap(86);                           /* REC */
    play(1);
    run_ms(200);
    select_pattern(5);                 /* (queued: A06 after A04) */
    char w[120];
    static const uint8_t PG[] = {MS_HOME, MS_SYN, MS_AMP, MS_FILT, MS_EFX, MS_DSTR, MS_LFO1, MS_LFO2, MS_LFO3, MS_TRK,
                                 MS_PAT, MS_GRID, MS_MIX, MS_GLO, MS_PERF, MS_CAT, MS_PATS};
    for (unsigned i = 0; i < sizeof PG; i++) {
        if (PG[i] == MS_PATS) { jog(1); blocks(4); }
        else page_go(PG[i]);
        blocks(VW_EVERY);
        const mv_view_t *v = now_view();
        const int g = hdr_gap(&v->scr);
        if (g < 3) { snprintf(w, sizeof w, "header on %s: its two ends %d px apart", MS_PAGE_N[PG[i]], g); fail(w); }
        if (PG[i] == MS_CAT) page_go(MS_CAT);
        if (PG[i] == MS_PERF) page_go(MS_PERF);
    }
}

/* the drawing reads the view alone: each still drawn again now, the core long since elsewhere, is the same frame */
static void purity_audit(void)
{
    open_at(2);
    play(1);
    run_ms(900);
    machine_to(0, M_VOX);
    int diff = 0;
    for (int i = 0; i < nkeep; i++) {
        m2k_screen_draw(&keep_v[i], sfb);
        if (memcmp(sfb, keep_f[i], MS_BYTES)) { char w[80]; snprintf(w, sizeof w, "%s drew differently from its view later", keep_n[i]); fail(w); diff++; }
    }
    printf("audit: %d stills drawn again from their views with the core elsewhere: %s\n", nkeep, diff ? "DIFFER" : "the same");
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

static double now_usd(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

/* each page, A02 playing: the publish (the controller's page state, the LEDs and the screen's gather: the audio
 * thread's) and the drawing (the drawer's), a sfb's time and instructions; and the old renderer's way for scale */
static void timing(FILE *f)
{
    open_at(1);
    play(1);
    run_ms(1500);
    static const uint8_t PG[] = {MS_HOME, MS_SYN, MS_AMP, MS_FILT, MS_EFX, MS_DSTR, MS_LFO1, MS_TRK, MS_PAT, MS_GRID,
                                 MS_MIX, MS_GLO, MS_PERF, MS_CAT, MS_PATS};
    const int N = 2000;
    double pw = 0, dw = 0;
    uint64_t pi = 0, di = 0;
    fprintf(f, "a frame (host, M-series Mac, -O2, %d each, A02 playing)\n", N);
    fprintf(f, "page       publish us  instructions   draw us  instructions\n");
    for (unsigned i = 0; i < sizeof PG; i++) {
        if (PG[i] == MS_PATS) { jog(1); blocks(1); } else page_go(PG[i]);
        blocks(2);
        volatile uint8_t sink = 0;
        uint64_t i0 = instr();
        double t0 = now_usd();
        for (int k = 0; k < N; k++) { ui.now += 17; view_publish(); sink ^= vw_now.scr.ms & 1; }
        double pu = (now_usd() - t0) / N;
        uint64_t pn = (instr() - i0) / (uint64_t)N;
        i0 = instr();
        t0 = now_usd();
        for (int k = 0; k < N; k++) { m2k_screen_draw(&vw_now.scr, sfb); sink ^= sfb[k & 1023]; }
        double du = (now_usd() - t0) / N;
        uint64_t dn = (instr() - i0) / (uint64_t)N;
        (void)sink;
        if (pu > pw) pw = pu;
        if (du > dw) dw = du;
        if (pn > pi) pi = pn;
        if (dn > di) di = dn;
        fprintf(f, "%-9s  %10.2f  %12llu  %8.2f  %12llu\n", MS_PAGE_N[PG[i]], pu, (unsigned long long)pn, du, (unsigned long long)dn);
        if (PG[i] == MS_CAT) page_go(MS_CAT);
        if (PG[i] == MS_PERF) page_go(MS_PERF);
        if (PG[i] == MS_PATS) { tap(50); }
    }
    fprintf(f, "most: publish %.2f us (%llu instructions), draw %.2f us (%llu instructions)\n", pw, (unsigned long long)pi,
            dw, (unsigned long long)di);
}

/* ------------------------------------------------------------------------------------------- the specimens */

/* the type at x1, every glyph the font has, and the line icons and the knobs' pictures, as two screens */
static void specimens(void)
{
    ms_fb = sfb;
    m_set(sfb, 0, sizeof sfb);
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
    snprintf(p, sizeof p, "%s/zz_type.pbm", out);
    pbm(p, sfb);
    fprintf(man, "zz_type\tTHE TYPE AT X1, X2, X3\n");

    m_set(sfb, 0, sizeof sfb);
    for (int i = 0; i < I_COUNT; i++) ms_icon(i, 2 + (i % 10) * 12, 7 + (i / 10) * 9, 1);
    open_at(0);
    static const struct { int code; int v; } K[8] = {{P_DIST, 90}, {P_VOL, 100}, {P_ATK, 40}, {P_DEC, 70},
                                                    {P_BASE, 40}, {P_LPQ, 100}, {P_PAN, 30}, {P_LFO + L_WAVE, 40}};
    ms_view_t v;
    m_set(&v, 0, sizeof v);
    m2k_scr_t s;
    m_set(&s, 0, sizeof s);
    s.page = MS_AMP; s.touch = -1; s.held = 0xFF;
    for (int k = 0; k < 8; k++) {
        PAT(cur_pat)->kit[0][K[k].code] = (uint8_t)K[k].v;
    }
    for (int k = 0; k < 8; k++) {           /* (each kind once: a page's knob gathered, its code swapped in) */
        const pi_t *pi = pinfo(0, K[k].code);
        ms_knob_t *kn = &v.knob[k];
        char o[32];
        const char *u;
        fmt(pi, 0, K[k].v, o, &u);
        m_set(kn, 0, sizeof *kn);
        kn->pic = (uint8_t)ms_pic_of(pi, 0);
        kn->q = (uint16_t)ms_q(pi, K[k].v);
        kn->n = (uint8_t)kind_choices(pi->kind);
        kn->c = (uint8_t)choice_of(pi->kind, K[k].v);
        if (kn->pic == MP_ENV) kn->sub = (uint8_t)(pi->kind == K_AHLD ? 2 : pi->s4[0] == 'A' && pi->s4[1] == 'T' ? 1 : 0);
        if (kn->pic == MP_FILT) kn->sub = (uint8_t)(pi->kind == K_FOCT ? 0 : pi->kind == K_FWID ? 1 : pi->s4[0] == 'H' ? 2 : 3);
        if (kn->pic == MP_WAVE) for (int i = 0; i < 14; i++) kn->wy[i] = (int8_t)((wave_at(o, i, 13) * 5) / 7);
        ms_pic(kn, k * 16, MS_KP, -1);
    }
    snprintf(p, sizeof p, "%s/zz_icons.pbm", out);
    pbm(p, sfb);
    fprintf(man, "zz_icons\tTHE ICONS, THE KNOBS' PICTURES\n");
}

/* ------------------------------------------------------------------------------------------------- the scenes */

static int track_of(int m)
{
    for (int t = 0; t < M2K_TRACKS; t++) if (PAT(cur_pat)->machine[t] == m) return t;
    return -1;
}

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

    /* HOME: A01 stopped; A02 playing; a knob touched */
    open_at(0);
    shot("01_home", "HOME  A01 STOPPED");
    padmap("home");
    open_at(1);
    play(1);
    run_ms(2400);
    shot("02_home_play", "HOME  A02 PLAYING");
    touch(5, 1); blocks(VW_EVERY);
    shot("03_home_touch", "HOME  SPD TOUCHED");
    touch(5, 0);

    /* SYN on machines bank A plays, and some it does not (on T7) */
    static const uint8_t SYNM[4] = {M_ENS, M_DKIK, M_VOX, M_WAV};
    for (int i = 0; i < 4; i++) {
        int n, t = -1;
        for (n = 0; n < 4 && t < 0; n++) { open_at(n); t = track_of(SYNM[i]); }
        if (t < 0) { open_at(0); t = 6; machine_to(t, SYNM[i]); }
        track(t);
        page_go(MS_SYN);
        play(1);
        run_ms(1200);
        char nm[32], lb[48];
        snprintf(nm, sizeof nm, "04_syn_%d", i);
        snprintf(lb, sizeof lb, "SYN  %s", MI[SYNM[i]].name);
        shot(nm, lb);
    }

    /* the track's pages on A02's T1 (SWAV-ENS, its LFO on WDTH), playing; EFX on T5 (FM+STAT, the ping-pong) */
    open_at(1);
    play(1);
    run_ms(1700);
    static const struct { int page, t; const char *name, *label; } PG[] = {
        {MS_AMP, 0, "05_amp", "AMP  THE ENVELOPE, LIVE"}, {MS_FILT, 0, "06_filt", "FILT  THE RESPONSE, LIVE"},
        {MS_EFX, 4, "07_efx", "EFX  EQ, PING-PONG"}, {MS_DSTR, 0, "08_dstr", "DSTR  THE CRUSH, THE MOD"},
        {MS_LFO1, 0, "09_lfo1", "LFO1  TO WDTH"}, {MS_LFO3, 0, "09_lfo3", "LFO3"},
        {MS_PAT, 0, "11_pat", "PAT  SYNC: THE MOVE'S TEMPO, THE LENGTHS"}, {MS_GRID, 0, "12_grid", "GRID"},
        {MS_MIX, 0, "13_mix", "MIX  (PROPOSAL) LEVELS, LIVE"}, {MS_GLO, 0, "14_glo", "GLO  THE BUS, MUTES"}};
    for (unsigned i = 0; i < sizeof PG / sizeof PG[0]; i++) {
        track(PG[i].t);
        page_go(PG[i].page);
        run_ms(100);
        shot(PG[i].name, PG[i].label);
    }
    track(0);
    {                                                  /* (staged: LFO2 a deep sine on VOL, a bar a cycle) */
        uint8_t *k = PAT(cur_pat)->kit[0];
        k[P_LFO + 8 + L_DEST] = P_VOL + 1; k[P_LFO + 8 + L_WAVE] = 20; k[P_LFO + 8 + L_DPTH] = 124;
        k[P_LFO + 8 + L_SPD] = 60; k[P_LFO + 8 + L_TRIG] = 0;
        page_go(MS_LFO2);
        run_ms(700);
        shot("09_lfo2", "LFO2  SIN TO VOL, DEEP");
    }
    ck.sync = MV_SYNC_INT;
    page_go(MS_PAT);
    blocks(VW_EVERY);
    shot("11_pat_int", "PAT  INT: MONO2K'S OWN TEMPO");
    ck.sync = MV_SYNC_MOVE;
    page_go(MS_AMP);
    touch(0, 1); blocks(VW_EVERY);
    shot("15_touch_atk", "AMP  ATK TOUCHED");
    touch(0, 0);
    page_go(MS_FILT);
    touch(0, 1); blocks(VW_EVERY);
    shot("15_touch_base", "FILT  BASE TOUCHED");
    touch(0, 0);
    blocks(500);

    /* TRK: the arp, three keys held, HOLD (Loop) */
    open_at(1);
    PAT(cur_pat)->kit[0][P_AMOD] = 22;                 /* (UP) */
    PAT(cur_pat)->kit[0][P_ARNG] = 40;
    m_cpy(trk[0].eff, PAT(cur_pat)->kit[0], P_COUNT);
    page_go(MS_TRK);
    play(1);
    pad(0, 1); pad(2, 1); pad(4, 1);                   /* (C E G) */
    tap(58);
    run_ms(1800);
    shot("10_trk", "TRK  THE ARP HELD, PLAYING");
    padmap("keys");
    pad(0, 0); pad(2, 0); pad(4, 0);

    /* a step held: its locks (A02's T1 has eight); a knob touched on one of them */
    open_at(1);
    int ls = -1;
    for (int i = 0; i < 16 && ls < 0; i++) if (has_locks(0, i)) ls = i;
    if (ls < 0) { ls = 4; lock_set(0, 4, P_BASE, 90); lock_set(0, 4, P_LPQ, 100); lockmap_build(); }
    int lp = -1;
    for (int i = 0; i < M2K_LOCKS; i++) if (PAT(cur_pat)->lock[i].trk == 0 && PAT(cur_pat)->lock[i].step == ls) { lp = PAT(cur_pat)->lock[i].param; break; }
    const int lpage = lp < 8 ? MS_SYN : lp < 16 ? MS_AMP : lp < 24 ? MS_FILT : lp < 32 ? MS_EFX : lp < 36 ? MS_DSTR
                    : lp < 42 ? MS_TRK : lp < 66 ? MS_LFO1 + (lp - 42) / 8 : MS_DSTR;
    page_go(lpage);
    step(ls, 1); blocks(VW_EVERY);
    shot("16_plock", "A STEP HELD: ITS LOCKS MARKED");
    for (int k = 0; k < 8; k++) if (m2k_screen_param(lpage, 0, k) == lp) { touch(k, 1); blocks(VW_EVERY); shot("16_plock_touch", "A LOCKED KNOB TOUCHED"); touch(k, 0); }
    step(ls, 0); blocks(400);

    /* the octave's message */
    tap(55);
    blocks(VW_EVERY);
    shot("17_toast_oct", "UP: THE OCTAVE, A MOMENT");
    blocks(700);

    /* SOUNDS (the pad), scrolled; a category */
    open_at(0);
    track(3);
    page_go(MS_CAT);
    jog(20); blocks(VW_EVERY);
    shot("18_cat", "SOUNDS  SCROLLED, ALL");
    knob(1, 1); knob(1, 1); blocks(VW_EVERY);
    shot("18_cat_cat", "SOUNDS  KNOB 2: A CATEGORY");
    page_go(MS_CAT);

    /* the patterns: the jog turned */
    open_at(0);
    play(1);
    run_ms(500);
    jog(3); blocks(VW_EVERY);
    shot("19_pats", "PATTERNS  THE JOG TURNED");
    btn(3, 127); blocks(1); btn(3, 0); blocks(VW_EVERY);
    jog(-1); blocks(VW_EVERY);
    shot("19_pats_next", "PATTERNS  A04 QUEUED");
    tap(50);

    /* PERFORM (the pad): nothing held, one, three; its keys' second page */
    open_at(1);
    play(1);
    run_ms(1000);
    page_go(MS_PERF);
    blocks(VW_EVERY);
    shot("20_perf", "PERFORM  THE KEYS ARE ITS EFFECTS");
    pad(1, 1); run_ms(200);
    shot("20_perf_one", "PERFORM  RPT 1/8 HELD");
    padmap("perform");
    pad(7, 1); pad(13, 1); run_ms(200);
    shot("20_perf_three", "PERFORM  THREE AT ONCE");
    pad(1, 0); pad(7, 0); pad(13, 0); blocks(4);
    tap(55); blocks(VW_EVERY + 600);
    shot("20_perf_page2", "PERFORM  UP: EACH BANK'S 9-11");
    page_go(MS_PERF);

    /* REC, a mute, the eighth track, Shift held */
    open_at(3);
    track(7);
    mute_to(7, 1);
    tap(86);
    play(1);
    run_ms(1900);
    page_go(MS_HOME);
    btn(49, 127); blocks(VW_EVERY);
    shot("21_home_shift", "HOME  T8 MUTED, REC, SHIFT");
    btn(49, 0); blocks(VW_EVERY);
    page_go(MS_SYN);
    run_ms(100);
    shot("21_syn_t8", "SYN  T8 MUTED, REC");

    /* the tempo: Shift + knob 1 on AMP, turned (Move's and MONO2K's, asked of Move); then Move did not follow (INT) */
    open_at(1);
    play(1);
    run_ms(600);
    page_go(MS_AMP);
    btn(49, 127);
    tempo(0, 5, 0); blocks(VW_EVERY);
    shot("22_tempo_shift", "SHIFT + KNOB 1: THE TEMPO, ANY PAGE");
    btn(49, 0);
    blocks(500);
    tq.state = MV_TQ_ALONE;            /* (staged: Move did not follow, MONO2K plays on alone) */
    toast("INT", "MOVE: LINK OFF");
    blocks(VW_EVERY);
    shot("22_tempo_alone", "MOVE DID NOT FOLLOW: INT, A HINT");
    blocks(700);
    page_go(MS_PAT);
    blocks(VW_EVERY);
    shot("22_tempo_alone_pat", "INT: THE HEADER, PAT");
    tq.state = MV_TQ_NONE;

    specimens();

    /* the motions, at the screen's pace (two panel frames, 34.8 ms: 12 blocks a sfb) */
    /* a knob touched: A02 T1's DEC on AMP, touched, turned down and back, let go (shown a moment more) */
    open_at(1);
    page_go(MS_AMP);
    play(1);
    run_ms(500);
    for (int i = 0; i < 90; i++) {
        if (i == 8) touch(2, 1);
        if (i >= 12 && i < 40) knob(2, -1);
        if (i >= 44 && i < 72) knob(2, 1);
        if (i == 76) touch(2, 0);
        blocks(12);
        frame_of("knob", i);
    }
    /* the page pads pressed: HOME, SYN, AMP, FILT, LFO1, TRK, MIX, PERFORM and back, ~0.7 s each */
    static const uint8_t TOUR[] = {MS_HOME, MS_SYN, MS_AMP, MS_FILT, MS_LFO1, MS_TRK, MS_MIX, MS_PERF, MS_PERF};
    for (int i = 0; i < (int)sizeof TOUR * 20; i++) {
        if (i % 20 == 0) { pad(MS_PAD0 + ms_pad_of(TOUR[i / 20]), 1); }
        if (i % 20 == 4) { pad(MS_PAD0 + ms_pad_of(TOUR[i / 20]), 0); }
        blocks(12);
        frame_of("pages", i);
    }
    /* the playhead: A01 over two bars on HOME */
    open_at(0);
    play(1);
    for (int i = 0; i < 120; i++) { blocks(12); frame_of("play", i); }
    fclose(man);

    text_audit();
    header_audit();
    purity_audit();
    printf("audit: %s\n", bad ? "FAILED" : "every value, name and header fits; the drawing reads the view alone");
    snprintf(p, sizeof p, "%s/timing.txt", out);
    FILE *f = fopen(p, "w");
    timing(stdout);
    if (f) { timing(f); fclose(f); }
    if (mv.live) v2_destroy(&mv);
    return bad ? 1 : 0;
}
