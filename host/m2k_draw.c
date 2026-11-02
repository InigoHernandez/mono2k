/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's drawing cost: the instructions one frame of each page takes (m2k_frame, the screen drawn and presented), and
 * each frame of the splash, counted as CLAUDE.md counts the sound's: built as the FM-1 builds the core, the 64-bit
 * products out of line, and read from the kernel (proc_pid_rusage ri_instructions, a Mac's). The FM-1 draws at most 30
 * frames a second (a splash frame changes every strip: about 13 a second over its SPI).
 *   cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen \
 *      -o build/host/m2k_draw host/m2k_draw.c && build/host/m2k_draw [--splash | --perform]
 * (--perform: the sound's cost with each PERFORM effect held, perform_cpu; --library [DIR]: each library sound's,
 * library_cpu; --cpu: the render's yardstick, bank A's four and DENSE on one core and two; --hash [SKIP]: their renders'
 * FNV-1a, on one core and two, to hold a change of the engine to the same sound bit for bit; SKIP samples left out
 * first, 16 against an engine from before 1.6.1, whose bus played in the block) */
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
        static const uint8_t MS[M2K_TRACKS6] = {M_SAW, M_FXC, M_GSIN, M_GSIN, M_FXR, M_FXD};
        dense_pattern(&pat[47]);
        m2k_pattern_t *q = &pat[47];
        for (int t = 0; t < M2K_TRACKS6; t++) {
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

/* --split: the split by measure, as the FM-1's clock gives it, on this computer: the instruction counter as the
 * clock (m2k_clock) learns what each track and the bus take, the split it picks recorded a block at a time, then the
 * same render played on that split (par_force, no clock) and its wall counted as --cpu counts it; against the
 * estimates' split */
static uint64_t clk_off;                 /* (each read's own cost taken out of what follows it) */
static uint32_t instr_clock(void) { uint64_t v = instr() - clk_off; clk_off += base_cost; return (uint32_t)v; }
static int par_now(void (*job)(void));
static void par_none(void);
static uint8_t split_rec[4 * 44100 / M2K_SUB + 8];
static double split_wall(int p, int measured)
{
    static int16_t b[2 * M2K_SUB], w[256];
    const int warm = 2 * 44100, len = 4 * 44100, nb = len / M2K_SUB;
    if (measured) {                                       /* the split the measure picks, recorded */
        m2k_par_begin = par_now; m2k_par_end = par_none; m2k_clock = instr_clock;
        play_from(p);
        for (int f = 0; f < warm; f += 128) m2k_render(w, 128);
        for (int k = 0; k < nb; k++) { m2k_render(b, M2K_SUB); split_rec[k] = par_mask; }
        m2k_clock = 0;
    }
    m2k_par_begin = cpu_begin; m2k_par_end = cpu_end;
    play_from(p);
    for (int f = 0; f < warm; f += 128) m2k_render(w, 128);
    uint64_t tot = 0;
    wall_x = 0;
    for (int k = 0; k < nb; k++) {
        if (measured) par_force = split_rec[k];
        job_i = 0;
        uint64_t s0 = instr();
        m2k_render(b, M2K_SUB);
        uint64_t c = instr() - s0 - base_cost;
        if (job_i) c -= job_i + 4 * base_cost;
        tot += c;
    }
    par_force = -1;
    m2k_par_begin = 0; m2k_par_end = 0;
    return (double)(tot + wall_x) / len;
}
static int split_cmp(void)
{
    static const char *const N[5] = {"A01", "A02", "A03", "A04", "DENSE"};
    printf("the wall (the slower core), Mac instructions a sample: the estimates' split, the split by measure\n");
    for (int p = 0; p < 5; p++) printf("  %-6s %6.0f %6.0f\n", N[p], split_wall(p, 0), split_wall(p, 1));
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
/* skip: the samples left out first (16: the bus a block behind since 1.6.1, against an older engine's from 0) */
static int render_hash(int skip, int len)
{
    static const char *const N[6] = {"A01", "A02", "A03", "A04", "DENSE", "FXS"};
    static int16_t b[2 * 11 * 44100];
    for (int p = 0; p < 6; p++) {
        uint32_t h[2];
        for (int two = 0; two < 2; two++) {
            m2k_par_begin = two ? par_now : 0; m2k_par_end = two ? par_none : 0;
            play_from(p);
            int n = (len + skip + 127) / 128 * 128;
            for (int f = 0; f < n; f += 128) m2k_render(b + 2 * f, 128);
            h[two] = fnv_pcm(2166136261u, b + 2 * skip, len);
        }
        m2k_par_begin = 0; m2k_par_end = 0;
        printf("  %-6s %08x %08x%s\n", N[p], h[0], h[1], h[0] == h[1] ? "" : "  (two cores differ)");
    }
    return 0;
}

/* --guard DIR [P] [LOAD]: the FM-1's CPU guard (1.7) heard. Bank A's demo P (0..3, or DENSE 4) on an FM-1 pretended
 * LOAD % short of it (default 115: its wall, two cores as --cpu counts them, that much over a render's real time): the
 * loop renders 128 frames at a time into a ring of 6 while it is not full, the interrupt takes 2 every 2 renders' real
 * time and leaves silence for any it finds missing (the glue's app_audio), and the guard is called as the glue calls
 * it (m2k_fm1.c render_ahead), on the tracks' estimates (no clock: each reading a system call here). Writes DIR/before.wav (no guard: the sound runs
 * short) and after.wav, 30 s each as the speaker plays them, and how many halves ran short and how many fades */
static void wav16(const char *path, const int16_t *s, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t n = (uint32_t)frames * 4u, v;
    fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f);
    uint16_t h[2] = {1, 2}; fwrite(h, 2, 2, f);
    v = 44100; fwrite(&v, 4, 1, f); v = 44100 * 4; fwrite(&v, 4, 1, f);
    h[0] = 4; h[1] = 16; fwrite(h, 2, 2, f);
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(s, 4, (size_t)frames, f);
    fclose(f);
}
static int guard_sim(const char *dir, int p, int load)
{
    enum { BLK = 128, RING = 6, SECS = 30 };
    static int16_t ring[RING][2 * BLK], out[2 * SECS * 44100];
    const int nout = SECS * 44100 / BLK;
    double budget = 0;
    m2k_par_begin = cpu_begin; m2k_par_end = cpu_end;
    for (int g = -1; g < 2; g++) {     /* -1: the pattern's wall measured, for the budget; 0 before, 1 after */
        play_from(p);
        for (int f = 0; f < 44100; f += BLK) { static int16_t w[2 * BLK]; m2k_render(w, BLK); }
        uint32_t ring_w = 0, ring_r = 0, shorts = 0, shed_shorts = 0, fades = 0, per[M2K_TRACKS] = {0};
        double t = 0, ravg = 0, wsum = 0;
        int got = 0, isr = 1, k = 0;
        while (got < nout) {
            if (g >= 0 && t >= 2.0 * isr) {          /* the interrupt: two renders' worth, or silence for what is missing */
                for (int j = 0; j < 2 && got < nout; j++, got++) {
                    if (ring_w == ring_r) { shorts++; m_set(out + 2 * BLK * got, 0, 4 * BLK); continue; }
                    m_cpy(out + 2 * BLK * got, ring[ring_r % RING], 4 * BLK);
                    ring_r++;
                }
                isr++;
                continue;
            }
            if (g >= 0 && ring_w - ring_r >= RING) { t = 2.0 * isr; k = 0; continue; }   /* full: wait for it */
            if (g == 1) {
                const double pct = ravg / budget * 100;
                int tf = m2k_guard((int)pct, (ring_w - ring_r <= RING / 3 && pct > 95) || (shorts != shed_shorts && pct > 70));
                shed_shorts = shorts;
                if (tf >= 0) { fades++; per[tf]++; }
            }
            double w = 0;
            for (int f = 0; f < BLK; f += M2K_SUB) {   /* a render, its wall as --cpu counts it */
                job_i = 0;
                uint64_t x = wall_x, s = instr();
                m2k_render(ring[ring_w % RING] + 2 * f, M2K_SUB);
                uint64_t c = instr() - s - base_cost;
                if (job_i) c -= job_i + 4 * base_cost;
                w += (double)c + (double)(wall_x - x);
            }
            ring_w++;
            ravg += (w - ravg) / 8;
            if (g < 0) { wsum += w; if (++got == 2000) budget = wsum / 2000 * 100 / load; continue; }
            t += w / budget + (++k % 3 == 0 ? 0.05 : 0);   /* (and the loop's own: the panel, a strip, every third) */
        }
        if (g < 0) continue;
        char path[512];
        snprintf(path, sizeof path, "%s/%s.wav", dir, g ? "after" : "before");
        wav16(path, out, nout * BLK);
        printf("  %s: %u halves ran short, %u tracks faded", g ? "after (the guard)" : "before", shorts / 2, fades);
        for (int i = 0; i < M2K_TRACKS && fades; i++) printf(" T%d %u", i + 1, per[i]);
        printf("\n");
    }
    m2k_par_begin = 0; m2k_par_end = 0;
    return 0;
}

int main(int argc, char **argv)
{
    for (int r = 0; r < 20; r++) { uint64_t a = instr(); uint64_t b = instr() - a; if (!base_cost || b < base_cost) base_cost = b; }
    if (argc > 1 && !strcmp(argv[1], "--perform")) return perform_cpu();
    if (argc > 1 && !strcmp(argv[1], "--library")) return library_cpu(argc > 2 ? argv[2] : 0);
    if (argc > 1 && !strcmp(argv[1], "--cpu")) return cpu_yard();
    if (argc > 1 && !strcmp(argv[1], "--split")) return split_cmp();
    if (argc > 2 && !strcmp(argv[1], "--guard")) return guard_sim(argv[2], argc > 3 ? atoi(argv[3]) : 3, argc > 4 ? atoi(argv[4]) : 115);
    if (argc > 1 && !strcmp(argv[1], "--hash")) return render_hash(argc > 2 ? atoi(argv[2]) : 0, argc > 3 ? atoi(argv[3]) : 10 * 44100);
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
