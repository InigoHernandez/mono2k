/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's drawing cost: the instructions one frame of each page takes (m2k_frame, the screen drawn and presented), and
 * each frame of the splash, counted as CLAUDE.md counts the sound's: built as the FM-1 builds the core, the 64-bit
 * products out of line, and read from the kernel (proc_pid_rusage ri_instructions, a Mac's). The FM-1 draws at most 30
 * frames a second (a splash frame changes every strip: about 13 a second over its SPI).
 *   cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen \
 *      -o build/host/m2k_draw host/m2k_draw.c && build/host/m2k_draw [--splash | --perform]
 * (--perform: the sound's cost with each PERFORM effect held, perform_cpu; --library [DIR]: each library sound's,
 * library_cpu; --cpu: the render's yardstick, bank A's four and DENSE on one core and two; --hash: their renders'
 * FNV-1a, on one core and two, to hold a change of the engine to the same sound bit for bit) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
#include "m2k_core.c"
#include "m2k_lib_play.c"
#include "m2k_dense.c"

static uint64_t instr(void)
{
    struct rusage_info_v4 ri;
    proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri);
    return ri.ri_instructions;
}

static uint64_t base_cost;
static uint32_t now = 4000;

/* the least of a few runs of one frame (the kernel's count is the process's: a run can catch a little else) */
static uint64_t frame_cost(void)
{
    uint64_t best = ~0ull;
    for (int r = 0; r < 5; r++) {
        now += 40;
        ui.trans_t = 0; ui.in_trans = 0; ui.toast_t = 0; ui.glitch_t = 0;
        uint64_t a = instr();
        m2k_frame(now);
        uint64_t b = instr() - a;
        if (b < best) best = b;
    }
    return best > base_cost ? best - base_cost : 0;
}

static void audio(int ms) { static int16_t b[256]; for (int f = 0; f < 44100 * ms / 1000; f += 128) m2k_render(b, 128); }

static uint64_t worst;
static char worst_n[32];
static void row(const char *name, uint64_t c)
{
    printf("  %-22s %8llu\n", name, (unsigned long long)c);
    if (c > worst) { worst = c; snprintf(worst_n, sizeof worst_n, "%s", name); }
}

/* --perform: each PERFORM effect's cost on the sound, on one core (the whole render, as m2k_render runs here), in
 * instructions a sample: bank A's demo p played 2 s, then 2 s with the key held, against the same 2 s without it */
static uint64_t perform_run(int p, int key)
{
    static int16_t b[256];
    m2k_init(2000);
    cur_pat = p; sq.queued = (uint8_t)p; lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
    seq_play(1);
    for (int f = 0; f < 2 * 44100; f += 128) m2k_render(b, 128);
    if (key >= 0) bus_perform(key, 1);
    uint64_t a = instr();
    for (int f = 0; f < 2 * 44100; f += 128) m2k_render(b, 128);
    return (instr() - a) / (2 * 44100 / 128 * 128);
}
static int perform_cpu(void)
{
    uint64_t dry[4], worst = 0;
    printf("instructions a sample, one core: the render with the key held, and its cost over the dry render\n");
    printf("  %-9s", "dry");
    for (int p = 0; p < 4; p++) { dry[p] = perform_run(p, -1); printf(" %5llu      ", (unsigned long long)dry[p]); }
    printf("\n");
    for (int k = 0; k < PF_COUNT; k++) {
        printf("  %-9s", PF_NAME[k]);
        for (int p = 0; p < 4; p++) {
            uint64_t c = perform_run(p, k);
            if (c > worst) worst = c;
            printf(" %5llu %+4lld", (unsigned long long)c, (long long)c - (long long)dry[p]);
        }
        printf("\n");
    }
    printf("  the most: %llu\n", (unsigned long long)worst);
    return 0;
}

/* --library [DIR]: each library sound's cost on one core, instructions a sample while it sounds (or its delay rings):
 * its audition (m2k_lib_play.c) rendered block by block, less the same pattern without its trigs, over the blocks it
 * is heard in; its mean and its 90th percentile. DIR/cpu.json for tools/lib_audition.py */
static int library_cpu(const char *dir)
{
    static int16_t b[256];
    char path[512];
    FILE *f = 0;
    if (dir) {
        snprintf(path, sizeof path, "%s/cpu.json", dir);
        if (!(f = fopen(path, "w"))) { perror(path); return 1; }
        fprintf(f, "{\"unit\": \"Mac instructions a sample, one core, built as the FM-1 builds the core\", \"sounds\": [\n");
    }
    printf("instructions a sample while the sound is heard (mean, 90th percentile), over the same pattern silent\n");
    for (int i = 0; i < LIB_N; i++) {
        int frames = lib_setup(i, 0);
        uint64_t idle = 0;
        for (int fr = 0; fr < frames; fr += 128) { uint64_t a = instr(); m2k_render(b, 128); idle += instr() - a - base_cost; }
        double per = (double)idle / (frames / 128);
        lib_setup(i, 1);
        static double cs[4 * M2K_SR / 128 + 1];
        double sum = 0;
        int n = 0;
        for (int fr = 0; fr < frames; fr += 128) {
            uint64_t a = instr();
            m2k_render(b, 128);
            double c = (double)(instr() - a - base_cost) - per;
            if (!trk[0].sounding && trk[0].dsleep) continue;
            sum += c;
            cs[n++] = c;
        }
        for (int x = 1; x < n; x++) for (int y = x; y > 0 && cs[y - 1] > cs[y]; y--) { double t = cs[y]; cs[y] = cs[y - 1]; cs[y - 1] = t; }
        double mean = n ? sum / n / 128 : 0, worst = n ? cs[n * 9 / 10] : 0;   /* (the worst tenth's least: one trig's
                                                                                * block is not the sound's cost) */
        printf("  S%03d %-7s %-8s %5.0f %5.0f\n", i + 1, LIB[i].name, MI[LIB[i].m].name, mean, worst / 128);
        if (f) fprintf(f, "  {\"slot\": %d, \"name\": \"%s\", \"instr\": %.0f, \"peak\": %.0f}%s\n", i + 1, LIB[i].name, mean,
                       worst / 128, i + 1 < LIB_N ? "," : "");
    }
    if (f) { fprintf(f, "]}\n"); fclose(f); }
    return 0;
}

/* --cpu and --hash: bank A's demo p (0..3), or DENSE (4, m2k_dense.c), from its start */
static void play_from(int p)
{
    m2k_init(2000);
    if (p == 4) { dense_pattern(&pat[46]); p = 46; }
    if (p == 5) {                      /* FXS: the FX machines each hearing a track with the chain's every stage on */
        static const uint8_t MS[M2K_TRACKS] = {M_SAW, M_FXC, M_GSIN, M_GSIN, M_FXR, M_FXD};
        dense_pattern(&pat[47]);
        m2k_pattern_t *q = &pat[47];
        for (int t = 0; t < M2K_TRACKS; t++) {
            uint8_t keep[P_COUNT];
            m_cpy(keep, q->kit[t], P_COUNT);
            kit_default(q, t, MS[t]);
            for (int k = 8; k < P_COUNT; k++) q->kit[t][k] = keep[k];
            q->kit[t][P_HOLD] = 127;
        }
        q->kit[1][P_SYN + 3] = 127; q->kit[4][P_SYN + 1] = 127; q->kit[4][P_SYN + 3] = 127;
        p = 47;
    }
    m2k_pattern(p);
    seq_play(1);
}

/* two cores in turn, as the self-test plays them: the job run as it is started, its instructions (j) and the first
 * core's own tracks' meanwhile (o) counted a block at a time; the slower of the two is the block's wall */
static uint64_t job_i, own_i, own_t0, wall_x;
static int cpu_begin(void (*job)(void))
{
    uint64_t a = instr();
    job();
    job_i = instr() - a - base_cost;
    own_t0 = instr();
    return 1;
}
static void cpu_end(void)
{
    own_i = instr() - own_t0 - base_cost;
    wall_x += (job_i > own_i ? job_i - own_i : 0);      /* (the first core waits for the second this long) */
}

static int cpu_yard(void)
{
    static const char *const N[5] = {"A01", "A02", "A03", "A04", "DENSE"};
    static int16_t b[2 * M2K_SUB];
    const int warm = 2 * 44100, len = 4 * 44100;
    printf("Mac instructions a sample, built as the FM-1 builds the core, 4 s after 2 s: one core; two cores: the "
           "first's work, the second's, the wall (the slower core's)\n");
    for (int p = 0; p < 5; p++) {
        m2k_par_begin = 0; m2k_par_end = 0;
        play_from(p);
        for (int f = 0; f < warm; f += 128) { static int16_t w[256]; m2k_render(w, 128); }
        uint64_t a = instr();
        for (int f = 0; f < len; f += M2K_SUB) m2k_render(b, M2K_SUB);
        uint64_t one = instr() - a - base_cost;
        m2k_par_begin = cpu_begin; m2k_par_end = cpu_end;
        play_from(p);
        for (int f = 0; f < warm; f += 128) { static int16_t w[256]; m2k_render(w, 128); }
        uint64_t tot = 0, job = 0;
        wall_x = 0;
        for (int f = 0; f < len; f += M2K_SUB) {
            job_i = 0;
            uint64_t s = instr();
            m2k_render(b, M2K_SUB);
            uint64_t c = instr() - s - base_cost;
            if (job_i) c -= job_i + 4 * base_cost;      /* (the hooks' four counts out: a call each) */
            tot += c;
            job += job_i;
        }
        m2k_par_begin = 0; m2k_par_end = 0;
        printf("  %-6s %6.0f   %6.0f %6.0f %6.0f\n", N[p], (double)one / len, (double)tot / len, (double)job / len,
               (double)(tot + wall_x) / len);
    }
    return 0;
}

static uint32_t fnv_pcm(uint32_t h, const int16_t *s, int n)
{
    const uint8_t *b = (const uint8_t *)s;
    for (int i = 0; i < 4 * n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static int par_now(void (*job)(void)) { job(); return 1; }
static void par_none(void) {}
static int render_hash(void)
{
    static const char *const N[6] = {"A01", "A02", "A03", "A04", "DENSE", "FXS"};
    static int16_t b[2 * 128];
    for (int p = 0; p < 6; p++) {
        uint32_t h[2];
        for (int two = 0; two < 2; two++) {
            m2k_par_begin = two ? par_now : 0; m2k_par_end = two ? par_none : 0;
            play_from(p);
            h[two] = 2166136261u;
            for (int f = 0; f < 10 * 44100; f += 128) { m2k_render(b, 128); h[two] = fnv_pcm(h[two], b, 128); }
        }
        m2k_par_begin = 0; m2k_par_end = 0;
        printf("  %-6s %08x %08x%s\n", N[p], h[0], h[1], h[0] == h[1] ? "" : "  (two cores differ)");
    }
    return 0;
}

int main(int argc, char **argv)
{
    for (int r = 0; r < 20; r++) { uint64_t a = instr(); uint64_t b = instr() - a; if (!base_cost || b < base_cost) base_cost = b; }
    if (argc > 1 && !strcmp(argv[1], "--perform")) return perform_cpu();
    if (argc > 1 && !strcmp(argv[1], "--library")) return library_cpu(argc > 2 ? argv[2] : 0);
    if (argc > 1 && !strcmp(argv[1], "--cpu")) return cpu_yard();
    if (argc > 1 && !strcmp(argv[1], "--hash")) return render_hash();
    if (argc > 1 && !strcmp(argv[1], "--splash")) {
        /* every splash frame at 60 a second, as the host draws them */
        m2k_init(2000);
        uint64_t tot = 0, mx = 0;
        int n = 0, at = 0;
        for (uint32_t ms = 0; ms < 3000; ms += 16, n++) {
            uint64_t a = instr();
            m2k_frame(ms);
            uint64_t c = instr() - a - base_cost;
            tot += c;
            if (c > mx) { mx = c; at = (int)ms; }
            if (argc > 2 && (!strcmp(argv[2], "-a") || !(n % 8))) printf("  %4u ms %8llu\n", (unsigned)ms, (unsigned long long)c);
        }
        printf("splash: %d frames, mean %llu, most %llu (at %d ms) instructions a frame\n", n,
               (unsigned long long)(tot / (uint64_t)n), (unsigned long long)mx, at);
        return 0;
    }
    m2k_init(2000);
    m2k_frame(0);
    now = 4000;
    m2k_frame(now);
    printf("instructions a frame (the least of 5), drawn and presented\n");
    cur_pat = 0; sq.queued = 0; lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[0].kit[t], P_COUNT);
    ui.page = PG_HOME; row("HOME stopped", frame_cost());
    seq_play(1); audio(600);
    row("HOME playing", frame_cost());
    for (int pg = 1; pg < PG_COUNT; pg++)
        for (int sub = 0; sub < nsub(pg); sub++) {
            ui.page = (uint8_t)pg; ui.sub = (uint8_t)sub;
            char o[24], nm[32];
            page_label(pg, sub, o);
            snprintf(nm, sizeof nm, "%s", o);
            audio(50);
            row(nm, frame_cost());
        }
    /* every machine's SYN page, playing (the marks animate) */
    uint64_t synw = 0;
    int synm = 0;
    for (int m = 0; m < M_COUNT; m++) {
        ui.trk = 0; set_machine(m); ui.page = PG_SYN; ui.sub = 0; ui.card_t = 0; audio(30);
        uint64_t c = frame_cost();
        if (c > synw) { synw = c; synm = m; }
    }
    char sn[32];
    snprintf(sn, sizeof sn, "SYN worst (%s)", MI[synm].name);
    row(sn, synw);
    /* a knob in focus on AMP; a held step on HOME */
    ui.page = PG_AMP; ui.sub = 0; ui.focus = 0; ui.focus_t = now + 100; row("AMP, ATCK in focus", frame_cost());
    ui.focus_t = 0;
    ui.page = PG_HOME; ui.grid = 1; ui.held[0] = 0; ui.nheld = 1; row("HOME, a step held", frame_cost());
    ui.nheld = 0; ui.grid = 0;
    /* the held windows */
    ui.page = PG_AMP; ui.latch = B_GLO + 1; row("GLO held (map)", frame_cost());
    ui.latch = B_HOME + 1; row("HOME held (patterns)", frame_cost());
    ui.latch = B_SEQ + 1; row("SEQ held (tools)", frame_cost());
    ui.latch = B_FX + 1; row("PERFORM", frame_cost());
    ui.latch = 0; cat_open(); row("catalogue (EDIT held)", frame_cost()); cat_back();
    ui.latch = 0;
    store_.set[S_SCREEN] = 127; ui.page = PG_HOME; row("HOME dark", frame_cost());
    store_.set[S_SCREEN] = 0;
    /* a page turned: its first frame alone (the knobs measure their parameters' every value once, knob_font) */
    static const uint8_t TO[3][2] = {{PG_LFO, 0}, {PG_HOME, 0}, {PG_AMP, 0}};
    static const char *const TN[3] = {"LFO1A, turned to", "HOME, turned to", "AMP, turned to"};
    for (int i = 0; i < 3; i++) {
        ui.page = PG_GLO; ui.sub = GS_SET; frame_cost();
        ui.page = TO[i][0]; ui.sub = TO[i][1];
        now += 40; ui.trans_t = 0; ui.in_trans = 0; ui.toast_t = 0; ui.glitch_t = 0;
        uint64_t a = instr();
        m2k_frame(now);
        row(TN[i], instr() - a - base_cost);
    }
    printf("  heaviest: %s, %llu\n", worst_n, (unsigned long long)worst);
    return 0;
}
