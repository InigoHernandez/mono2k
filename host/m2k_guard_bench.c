/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's CPU guard bench (1.11): the FM-1's ring of 6 blocks, its audio interrupt (two blocks every 5.8 ms, silence
 * and a short for what it finds missing), its main loop (the renders, the panel, the screen's frames and strips, a
 * flash write now and then) and its guard, run on this computer against load traces: 1.7's rule (copied below, for
 * the comparison) against 1.11's, which is the code the FM-1 runs (the core's m2k_guard and the glue's m2k_pace.h).
 * The render is the core's own: its sound, its tracks sounding, ringing and resting. Its time is the core's cost model
 * (each track's estimate as par_split weighs it: T_MCOST and the stages, its delay while it rings; the bus; the first
 * core's own part), read through the FM-1's clock hook (m2k_clock), so the split and the guard see each track's cost as
 * they would there, and the two cores run as the FM-1's do (a block's wall the slower one's). One calibration: DENSE's
 * render takes the FM-1's whole real time (the owner's dense pattern read CPU 97 % with cracks), so bank A sits where its
 * cost puts it (A04 ~74 %); a trace scales that, and adds the screen's frames (1 ms each, ~10 strips) and hiccups.
 * Deterministic: the same numbers every run, on any computer.
 *   cc -O2 -Ifirmware/src -Ifirmware/gen -Ifirmware/fm1 -o build/host/m2k_guard_bench host/m2k_guard_bench.c
 *   build/host/m2k_guard_bench [--wav DIR] [--only NAME ...]
 * Prints each trace old against new (shorts: the interrupt's, as DROP counts them; tails cut, rests, track-seconds
 * rested, the longest rest, seconds the screen gave way) and the checks; exits 1 when one fails */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m2k_core.c"
#include "m2k_dense.c"

#define RING 6u
#define BLK 128u
#define TPU 24u                                 /* the FM-1's ticks a microsecond */
#define BLK_TICKS (BLK * 10000u / 441u * TPU)
#define PACE_TICKS_MS (1000u * TPU)
#include "m2k_pace.h"

#define US(x) ((uint64_t)((x) * TPU))
#define C_SEQ 60                                /* the first core's own part, the sequencer and the LFOs (Mac
                                                 * instructions a sample: ~2 % of bank A's, m2k_prof) */

/* ---- the render's time: the cost model as the FM-1's clock. A timed section's start returns the clock, its end
 * moves it on by the section's cost (clk_k: what it times) */
static uint64_t clk;
static double unit = 1.0, scale = 1.0;          /* ticks a model unit (Mac instructions a sample, a block); the trace's */
static int clk_open;
static double est_of(int k)
{
    if (k < M2K_TRACKS) {
        const trk_t *t = &trk[k];
        return (t->sounding ? t->ccost : C_IDLE) + (t->dsleep ? 0 : t->dmode ? C_DLY2 : C_DLY);
    }
    return k == M2K_TRACKS ? C_BUS + C_PAR : C_SEQ;
}
static uint32_t model_clock(void)
{
    if (!clk_open) { clk_open = 1; return (uint32_t)clk; }
    clk_open = 0;
    clk += (uint64_t)(est_of(clk_k) * unit * scale + 0.5);
    return (uint32_t)clk;
}
/* the second core: its job run in place, timed; the first core's own tracks meanwhile; the block's wall the slower */
static uint64_t job_t, own_from, overlap;
static int bench_begin(void (*job)(void)) { uint64_t a = clk; job(); job_t = clk - a; own_from = clk; return 1; }
static void bench_end(void) { uint64_t own = clk - own_from; overlap += job_t < own ? job_t : own; }

static uint32_t jrng = 1;
static uint32_t xr(void) { jrng ^= jrng << 13; jrng ^= jrng >> 17; jrng ^= jrng << 5; return jrng; }

/* a render of 128 frames into o: its time on the FM-1, ticks (the model's, +-3 % for the cache and the flash) */
static uint32_t render(int16_t *o)
{
    uint64_t a = clk;
    overlap = 0;
    m2k_render(o, (int)BLK);
    double dt = (double)(clk - a - overlap) * (0.97 + (xr() % 601) / 10000.0);
    return (uint32_t)dt;
}

static void start(int p)
{
    m2k_init(2000);
    m2k_par_begin = bench_begin; m2k_par_end = bench_end;
    m2k_clock = model_clock; m2k_clock_per_ms = 1000u * TPU;
    clk = 0; clk_open = 0; jrng = 1;
    if (p == 4) { dense_pattern(&pat[46]); p = 46; }
    m2k_pattern(p);
    seq_play(1);
}

/* DENSE's render, the calibration: its mean wall a block, model units */
static void calibrate(void)
{
    static int16_t b[2 * BLK];
    unit = 1.0; scale = 1.0;
    start(4);
    for (int i = 0; i < 2 * 44100 / (int)BLK; i++) render(b);
    uint64_t a = clk, w = 0;
    const int n = 6 * 44100 / (int)BLK;
    for (int i = 0; i < n; i++) { uint64_t c = clk; overlap = 0; m2k_render(b, (int)BLK); w += clk - c - overlap; }
    (void)a;
    unit = (double)BLK_TICKS / ((double)w / n);
}

/* ---- 1.7's guard, as it was (m2k_ui.c and m2k_fm1.c at 1.10), for the comparison: danger a third of the ring left
 * and the render over 95 %, or a new short and over 70 %; the track quietest for what it costs rests (the track in
 * view the last), 8 renders' wait; back when the load over ~64 renders plus its whole cost is under 90 %, 32 after */
static struct { uint8_t wait, n; int8_t rest[M2K_TRACKS]; int32_t slow; } og;
static int old_pct(int32_t c) { uint32_t tpb = m2k_clock_per_ms * 160u / 441u; return (int)((uint32_t)(c >> 4) * 100u / tpb); }
static void old_back(int t)
{
    trk[t].held = 0;
    int j = 0;
    for (int i = 0; i < og.n; i++) if (og.rest[i] != t) og.rest[j++] = og.rest[i];
    og.n = (uint8_t)j;
}
static int old_guard(int load_pct, int danger)
{
    og.slow += ((CLAMP(load_pct, 0, 999) << 8) - og.slow) / 64;
    if (trk[ui.trk].held) old_back(ui.trk);
    if (og.wait) { og.wait--; return -1; }
    if (!danger) {
        if (og.n) {
            const int t = og.rest[og.n - 1];
            if ((og.slow >> 8) + old_pct(trk[t].park_cost) < 90) { old_back(t); og.wait = 32; }
        }
        return -1;
    }
    og.wait = 8;
    int best = -1;
    uint32_t bs = 0;
    for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int t = 0; t < M2K_TRACKS; t++) {
            const trk_t *k = &trk[t];
            if (!k->sounding || k->held || (t == ui.trk) != pass) continue;
            uint32_t v = ((uint32_t)CLAMP(k->lvl, 0, 65535) << 12) / ((uint32_t)trk_cost(t) + 1u);
            if (best < 0 || v < bs) { best = t; bs = v; }
        }
    if (best >= 0) { trk_park(best, trk_cost(best), 1); og.rest[og.n++] = (int8_t)best; }
    return best;
}

/* ---- the traces */
typedef struct {
    const char *name, *what;
    int pn;                                     /* bank A's 0..3, DENSE 4 */
    double from, to;                            /* the load's scale, at the start and the end: a ramp between */
    double period;                              /* or s: FROM then TO in turn, this long each (bars heavy and light) */
    double step_at;                             /* or s: FROM until then, TO after (a busy part, then a light one) */
    double spike, spike_at[2], spike_len;       /* a spike's scale, when, how long (s) */
    int flash_ms, flash_every;                  /* a flash write's stall, ms, and every so many seconds */
    int stall_ms, stall_every;                  /* a longer stall (a page's first frame, an erase): ms, every s */
    int rest;                                   /* GLO > PLAY's REST (S_REST's choice: 0 AUTO, 3 T3, 7 NONE) */
    int frame_us, strips;                       /* a frame's drawing (0: 1 ms; a SYN or FILT page ~2) and its strips
                                                 * (0: 10; a page in motion all 30) */
} trace_t;

#define SECS 20
static const trace_t TR[] = {
    {"heavy", "DENSE at 93 %, a SYN page's 2 ms frames", 4, .from = 0.93, .to = 0.93, .frame_us = 2000},
    {"heavy97", "DENSE at 97 %, 2 ms frames: at the limit", 4, .from = 0.97, .to = 0.97, .frame_us = 2000},
    {"screen", "A04 at 88 %, 2.5 ms frames of 30 strips", 3, .from = 1.19, .to = 1.19, .frame_us = 2500, .strips = 30},
    {"screen98", "DENSE at 98 %, 4 ms frames of 30 strips (a knob turned)", 4, .from = 0.98, .to = 0.98, .frame_us = 4000, .strips = 30},
    {"spike", "A04 with two 0.6 s spikes to ~120 %", 3, .from = 1.0, .to = 1.0, .spike = 1.6, .spike_at = {6, 14}, .spike_len = 0.6},
    {"ramp", "DENSE from 85 % to 115 % over 20 s", 4, .from = 0.85, .to = 1.15},
    {"waves", "DENSE 2 s at 104 %, 2 s at 88 %, in turn", 4, .from = 1.04, .to = 0.88, .period = 2},
    {"waves70", "DENSE 2 s at 106 %, 2 s at 72 %, in turn", 4, .from = 1.06, .to = 0.72, .period = 2},
    {"recover", "DENSE at 110 % for 6 s, then 76 %", 4, .from = 1.1, .to = 0.76, .step_at = 6},
    {"hiccup70", "A04 at ~70 %, flash writes and stalls", 3, .from = 0.95, .to = 0.95, .flash_ms = 8, .flash_every = 2,
     .stall_ms = 20, .stall_every = 5},
    {"hiccup80", "A04 at ~80 %, flash writes and stalls", 3, .from = 1.08, .to = 1.08, .flash_ms = 8, .flash_every = 2,
     .stall_ms = 20, .stall_every = 5},
    {"hiccup85", "A04 at ~85 %, flash writes and stalls", 3, .from = 1.15, .to = 1.15, .flash_ms = 8, .flash_every = 2,
     .stall_ms = 20, .stall_every = 5},
    {"A01", "bank A as it is, a flash write every 3 s", 0, .from = 1.0, .to = 1.0, .flash_ms = 8, .flash_every = 3},
    {"A02", "bank A as it is, a flash write every 3 s", 1, .from = 1.0, .to = 1.0, .flash_ms = 8, .flash_every = 3},
    {"A03", "bank A as it is, a flash write every 3 s", 2, .from = 1.0, .to = 1.0, .flash_ms = 8, .flash_every = 3},
    {"A04", "bank A as it is, a flash write every 3 s", 3, .from = 1.0, .to = 1.0, .flash_ms = 8, .flash_every = 3},
    {"A04+15", "A04 on an FM-1 15 % slower, 2 ms frames", 3, .from = 1.15, .to = 1.15, .flash_ms = 8, .flash_every = 3,
     .frame_us = 2000},
    {"dense", "DENSE at 100 %", 4, .from = 1.0, .to = 1.0},
    {"dense110", "DENSE at 110 %: over", 4, .from = 1.1, .to = 1.1},
    {"rest-t3", "DENSE at 110 %, REST T3", 4, .from = 1.1, .to = 1.1, .rest = 3},
    {"rest-none", "DENSE at 110 %, REST NONE", 4, .from = 1.1, .to = 1.1, .rest = 7},
};
#define NTR ((int)(sizeof TR / sizeof TR[0]))

typedef struct {
    int shorts, silent, tails, rests, frames;
    double rested, longest, wide;
    int first_kind, first_trk, first_rest, min_gap;   /* the first step (1 tail, 3 rest) and its track, the first rest's;
                                                       * renders between steps */
    int ends_resting;                           /* tracks resting at the end */
    int cascades;                               /* rests the step before had made needless (note_step) */
    double last_back;                           /* when the last rested track came back, s */
    int wide_fast;                              /* frames drawn sooner than the guard's gap while it asked for it */
} res_t;

static int16_t out[2 * 30 * 44100];

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

/* the FM-1 running trace tr for its seconds, with 1.7's guard (neu 0), 1.11's (1) or none (2) */
static uint64_t T;                              /* now, ticks */
static uint32_t ring_w, ring_r, isr_k, shorts, shed_shorts;
static int16_t fring[RING][2 * BLK];
static int nout, nmax;
static res_t R;
static uint32_t rest_since[M2K_TRACKS];
static int last_step;
static const trace_t *tr;
static int neu, verbose;
static uint32_t last_dt;                        /* the last render's time: the true load, unaveraged */
static double since_sum;                        /* the true load since the last step (its fade's render out) */
static int since_n;

static uint64_t isr_at(uint32_t k) { return (uint64_t)k * 256u * 24000000u / 44100u; }

/* time goes on by dt: the interrupt takes its two blocks wherever its time falls */
static void advance(uint64_t dt)
{
    const uint64_t end = T + dt;
    while (isr_at(isr_k) < end) {
        for (int j = 0; j < 2; j++) {
            if (ring_w == ring_r) {             /* (the glue's app_audio: a short, silence for the rest of it) */
                shorts++;
                for (; j < 2; j++, nout++) { R.silent++; if (nout < nmax) m_set(out + 2 * BLK * nout, 0, 4 * BLK); }
                break;
            }
            if (nout < nmax) m_cpy(out + 2 * BLK * nout, fring[ring_r % RING], 4 * BLK);
            nout++;
            ring_r++;
        }
        isr_k++;
    }
    T = end;
}

static double secs(void) { return (double)T / (TPU * 1e6); }

static void scale_now(void)
{
    const double s = secs(), u = s / SECS;
    scale = tr->period ? ((int)(s / tr->period) & 1 ? tr->to : tr->from)
          : tr->step_at ? (s < tr->step_at ? tr->from : tr->to) : tr->from + (tr->to - tr->from) * u;
    for (int i = 0; i < 2; i++)
        if (tr->spike && s >= tr->spike_at[i] && s < tr->spike_at[i] + tr->spike_len) scale *= tr->spike;
}

static void note_step(int t, int kind, int renders)
{
    if (t < 0) return;
    /* a cascade: a rest soon after a step (a third of a second) though the true load had gone under the real time
     * since that step: the step before was enough, the average had yet to show it */
    const double since = since_n ? since_sum / since_n : 0;
    if (kind == 3 && last_step >= 0 && renders - last_step < GRD_SEC / 3 && since_n && since < 100) R.cascades++;
    if (verbose)
        printf("    %6.2f s  %s T%d  load %3d %%, true %3.0f %%, since the last %3.0f %% over %d, %u ahead\n", secs(),
               kind == 3 ? "rest" : "tail", t + 1, (int)(ravg / (BLK_TICKS / 100u)), last_dt * 100.0 / BLK_TICKS, since,
               since_n, ring_w - ring_r);
    since_sum = 0;
    since_n = -1;
    if (!R.first_kind) { R.first_kind = kind; R.first_trk = t; }
    if (kind == 3 && R.first_rest < 0) R.first_rest = t;
    if (last_step >= 0 && renders - last_step < R.min_gap) R.min_gap = renders - last_step;
    last_step = renders;
    if (kind == 3) R.rests++; else R.tails++;
}

static void render_ahead(int *renders)
{
    for (uint32_t k = 0; k < RING / 2u && ring_w - ring_r < RING; k++) {
        const int pct = (int)(ravg / (BLK_TICKS / 100u));
        if (neu == 1) {
            uint8_t was[M2K_TRACKS];
            for (int t = 0; t < M2K_TRACKS; t++) was[t] = trk[t].held;
            int got = m2k_guard(pct, (int)(ring_w - ring_r), (int)RING, (int)shorts);
            if (got >= 0) note_step(got, trk[got].held && !was[got] ? 3 : 1, *renders);
        } else if (!neu) {
            int got = old_guard(pct, (ring_w - ring_r <= RING / 3u && pct > 95) || (shorts != shed_shorts && pct > 70));
            shed_shorts = shorts;
            note_step(got, 3, *renders);
        }
        for (int t = 0; t < M2K_TRACKS; t++) {
            if (trk[t].held && !rest_since[t]) rest_since[t] = *renders + 1;
            if (!trk[t].held && rest_since[t]) {
                R.last_back = secs();
                double d = (*renders + 1 - rest_since[t]) * (double)BLK / 44100;
                if (d > R.longest) R.longest = d;
                rest_since[t] = 0;
            }
            if (trk[t].held) R.rested += (double)BLK / 44100;
        }
        if (m2k_guard_gap()) R.wide += (double)BLK / 44100;
        scale_now();
        uint32_t dt = render(fring[ring_w % RING]);
        last_dt = dt;
        if (since_n++ >= 0) since_sum += dt * 100.0 / BLK_TICKS;   /* (the fade's render left out) */
        advance(dt);
        pace_rendered(dt);
        ring_w++;
        (*renders)++;
    }
    pace_busy((uint32_t)T);
}

static res_t run(const trace_t *trc, int is_new, const char *wavdir)
{
    tr = trc; neu = is_new;
    m_set(&R, 0, sizeof R);
    R.min_gap = 1 << 30;
    R.first_rest = -1;
    last_step = -1;
    since_sum = 0; since_n = 0;
    m_set(rest_since, 0, sizeof rest_since);
    m_set(&og, 0, sizeof og);
    scale = trc->from;
    start(trc->pn);
    store_.set[S_REST] = (uint8_t)enc(trc->rest, M2K_TRACKS6 + 2);
    ui.trk = 0;
    {   /* a second's run-in, as the FM-1 has played a while (its measures warm) */
        static int16_t b[2 * BLK];
        for (int i = 0; i < 44100 / (int)BLK; i++) render(b);
    }
    T = 0; ring_w = ring_r = isr_k = shorts = shed_shorts = 0;
    ravg = 0; busy_ticks = 0; busy_t0 = 0; frame_gap = 33;
    for (uint32_t i = 0; i < RING; i++) ravg = ravg - (ravg >> 3) + (render(fring[ring_w++]) >> 3);   /* (the ring full) */
    nout = 0; nmax = (int)(sizeof out / sizeof out[0] / (2 * BLK));
    int renders = 0, strips = 0;
    uint64_t frame_t = 0, lcd_free = 0, end = US(1e6 * SECS);
    int next_flash = trc->flash_every, next_stall = trc->stall_every;
    while (T < end) {
        uint64_t t0 = T;
        render_ahead(&renders);
        advance(US(20));                                         /* the panel, MIDI */
        render_ahead(&renders);
        const uint32_t since = (uint32_t)((T - frame_t) / (1000u * TPU));
        if (!strips && pace_frame_due(since, ring_w - ring_r)) {   /* a frame: drawn whole, then its strips */
            const int fu = trc->frame_us ? trc->frame_us : 1000;
            if (m2k_guard_gap() && since < (uint32_t)m2k_guard_gap()) R.wide_fast++;
            frame_t = T;
            advance(US(fu));
            busy_ticks += (uint32_t)US(fu);
            strips = trc->strips ? trc->strips : 10;
            R.frames++;
        }
        if (strips && T >= lcd_free) {                           /* a strip turned and sent (the SPI on its own) */
            advance(US(10));
            busy_ticks += (uint32_t)US(10);
            lcd_free = T + US(500);
            strips--;
        }
        if (trc->flash_every && secs() >= next_flash) { advance(US(1000 * trc->flash_ms)); next_flash += trc->flash_every; }
        if (trc->stall_every && secs() >= next_stall) { advance(US(1000 * trc->stall_ms)); next_stall += trc->stall_every; }
        if (T == t0 + US(20)) advance(US(30));                    /* (nothing else to do: the loop spins) */
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        if (trk[t].held) {
            R.ends_resting++;
            double d = (renders - rest_since[t]) * (double)BLK / 44100;
            if (d > R.longest) R.longest = d;
        }
    R.shorts = (int)shorts;
    if (wavdir) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s-%s.wav", wavdir, trc->name, is_new == 2 ? "bare" : is_new ? "1.11" : "1.7");
        wav16(path, out, nout < nmax ? nout * (int)BLK : nmax * (int)BLK);
    }
    return R;
}

static int fails;
static void check(int ok, const char *what)
{
    printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) fails++;
}

int main(int argc, char **argv)
{
    const char *wav = 0, *only[NTR];
    int nonly = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (!strcmp(argv[i], "--only")) { while (i + 1 < argc && argv[i + 1][0] != '-' && nonly < NTR) only[nonly++] = argv[++i]; }
    }
    calibrate();
    printf("the CPU guard on an FM-1 simulated (DENSE's render = its real time; 1 ms frames, 10 strips each)\n");
    printf("  %-10s %-5s %7s %6s %6s %6s %9s %8s %5s %7s %7s  %s\n", "trace", "rule", "shorts", "silent", "tails", "rests",
           "rested s", "longest", "casc", "wide s", "frames", "(none: no guard at all; shorts: the interrupt's, as DROP counts)");
    res_t O[NTR], N[NTR], Z[NTR];
    int ran[NTR];
    for (int i = 0; i < NTR; i++) {
        ran[i] = !nonly;
        for (int j = 0; j < nonly; j++) ran[i] |= !strcmp(only[j], TR[i].name);
        if (!ran[i]) continue;
        Z[i] = run(&TR[i], 2, wav);
        O[i] = run(&TR[i], 0, wav);
        N[i] = run(&TR[i], 1, wav);
        static const char *const RN[3] = {"none", "1.7", "1.11"};
        for (int k = 0; k < 3; k++) {
            const res_t *r = k == 2 ? &N[i] : k ? &O[i] : &Z[i];
            printf("  %-10s %-5s %7d %6d %6d %6d %9.2f %8.2f %5d %7.2f %7d  %s\n", k ? "" : TR[i].name, RN[k],
                   r->shorts, r->silent, r->tails, r->rests, r->rested, r->longest, r->cascades, r->wide, r->frames, k ? "" : TR[i].what);
        }
    }
    printf("checks\n");
    int bank = 1, all_short = 1, hic = 1, gap = 1, fewer = 1, room = 1, ladder = 1, back = 1, nb = 0;
    for (int i = 0; i < NTR; i++) {
        if (!ran[i]) continue;
        const trace_t *t = &TR[i];
        if (t->name[0] == 'A') { nb++; bank &= N[i].tails == 0 && N[i].rests == 0; }
        if (t->rest != 7) all_short &= N[i].shorts <= O[i].shorts;
        if (!strncmp(t->name, "hiccup", 6)) { hic &= N[i].tails == 0 && N[i].rests == 0 && N[i].wide > 0; }
        hic &= N[i].wide_fast == 0;
        gap &= N[i].min_gap > 4 && N[i].cascades == 0;
        if (t->rest == 0) { fewer &= N[i].rests <= O[i].rests; room &= N[i].rested <= O[i].rested + 0.5; }
        if (!strcmp(t->name, "spike")) { ladder &= N[i].first_kind == 1; back &= N[i].ends_resting == 0; }
    }
    if (nb) check(bank, "bank A never sheds (no tail cut, no rest), as it is and on an FM-1 15 % slower, flash writes and all");
    check(all_short, "the sound runs short no more often than with 1.7's guard, on every trace (REST NONE aside)");
    check(hic, "a short at 70-85 % (a flash write, a stall): the screen gives way (its frames GRD_GAP apart), never a cut");
    check(gap, "no cascade: a step only after the last has taken effect (4 renders apart at least), no rest the one before had made needless");
    check(fewer && room, "no more rests than 1.7's, and none resting longer (half a second's grace), AUTO");
    for (int i = 0; i < NTR; i++) {
        if (!ran[i]) continue;
        if (!strcmp(TR[i].name, "spike")) check(ladder && back, "a spike: a tail is cut first, and nothing still rests at its end");
        if (!strcmp(TR[i].name, "recover"))
            check(N[i].rests > 0 && N[i].ends_resting == 0 && N[i].last_back < TR[i].step_at + 2,
                  "room again: the rested track is back within 2 s of the load's fall");
        if (!strcmp(TR[i].name, "rest-t3")) check(N[i].first_rest == 2, "REST T3: the first track to rest is T3");
        if (!strcmp(TR[i].name, "rest-none")) {
            int a = -1;
            for (int j = 0; j < NTR; j++) if (ran[j] && !strcmp(TR[j].name, "dense110")) a = j;
            check(N[i].rests == 0 && (a < 0 || N[i].shorts >= N[a].shorts), "REST NONE: no track rests (the sound may run short)");
        }
    }
    printf(fails ? "guard bench: %d FAILED\n" : "guard bench: passed\n", fails);
    return fails != 0;
}
