/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's dsp.so driven as Schwung drives it: dlopen, move_plugin_init_v2, create_instance, then on_midi, set_param,
 * get_param and render_block in 128-frame blocks (Schwung's audio thread: here the main thread, marked so around each
 * of those calls). Built and run in the Move's container by move/proof.sh:
 *   bench DSP.so hash         bank A's A01..A04 through the plugin, FNV-1a over 10 s as host/m2k_draw.c --hash prints
 *                             them (seed 2000, from the start, one core)
 *   bench DSP.so time [SEC]   each block's render timed, A01..A04 playing; the screen's cost a frame (the publish on
 *                             the audio thread, the drawing on the drawer's, in real time), and drawn in the render
 *   bench DSP.so rt           the presses, the controls, Move's clock, saves and the reads with the render: every
 *                             allocation, file, thread and sleep call made on the audio thread counted (none allowed)
 *   bench DSP.so ctl          every control of the Move (the page pads, the keyboard, PERFORM, the jog's lists...),
 *                             what it does to the core (get_param "view" and "dump")
 *   bench DSP.so clock        Move's clock followed: synthetic streams with a block's jitter and late frames, start,
 *                             stop, continue, tempo changes, no host tempo, no clock at all
 *   bench DSP.so save DIR     saving and loading: the saver's file against the core's own pack, byte for byte, a
 *                             reload, a broken file, closing without a save
 *   bench DSP.so draw DIR     the screen in a few states, as PBM files */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include "../vendor/schwung/plugin_api_v1.h"
#include "../m2k_move_view.h"              /* (the pages, the pad map, the overlays: the controller's own) */

/* ------------------------------------------------------------------- the audio thread's rules, counted */

/* the calls dsp.so makes, through this program (-rdynamic: a dlopened library binds to its symbols first); counted
 * while the calling thread is inside an audio-thread entry point. The allocator over glibc's; the rest forwarded */
static __thread int in_audio;
static volatile unsigned long n_alloc, n_file, n_thread, n_sleep, n_log;
extern void *__libc_malloc(size_t);
extern void *__libc_calloc(size_t, size_t);
extern void *__libc_realloc(void *, size_t);
extern void *__libc_memalign(size_t, size_t);
extern void __libc_free(void *);
void *malloc(size_t n) { if (in_audio) n_alloc++; return __libc_malloc(n); }
void *calloc(size_t a, size_t b) { if (in_audio) n_alloc++; return __libc_calloc(a, b); }
void *realloc(void *p, size_t n) { if (in_audio) n_alloc++; return __libc_realloc(p, n); }
void free(void *p) { if (in_audio && p) n_alloc++; __libc_free(p); }
void *memalign(size_t a, size_t n) { if (in_audio) n_alloc++; return __libc_memalign(a, n); }
void *aligned_alloc(size_t a, size_t n) { if (in_audio) n_alloc++; return __libc_memalign(a, n); }
int posix_memalign(void **p, size_t a, size_t n) { if (in_audio) n_alloc++; *p = __libc_memalign(a, n); return *p ? 0 : 12; }

#define REAL(name) static __typeof__(name) *real_##name
REAL(fopen); REAL(fopen64); REAL(fread); REAL(fwrite); REAL(fclose); REAL(fflush); REAL(fsync); REAL(rename);
REAL(unlink); REAL(mkdir); REAL(nanosleep); REAL(pthread_create); REAL(pthread_join); REAL(sched_setaffinity);
REAL(sched_setscheduler);
static void real_init(void)
{
#define R(name) real_##name = (__typeof__(name) *)dlsym(RTLD_NEXT, #name)
    R(fopen); R(fopen64); R(fread); R(fwrite); R(fclose); R(fflush); R(fsync); R(rename); R(unlink); R(mkdir);
    R(nanosleep); R(pthread_create); R(pthread_join); R(sched_setaffinity); R(sched_setscheduler);
#undef R
}
#define FILECALL do { if (in_audio) n_file++; } while (0)
FILE *fopen(const char *p, const char *m) { FILECALL; return real_fopen(p, m); }
FILE *fopen64(const char *p, const char *m) { FILECALL; return real_fopen64(p, m); }
size_t fread(void *b, size_t s, size_t n, FILE *f) { FILECALL; return real_fread(b, s, n, f); }
size_t fwrite(const void *b, size_t s, size_t n, FILE *f) { FILECALL; return real_fwrite(b, s, n, f); }
int fclose(FILE *f) { FILECALL; return real_fclose(f); }
int fflush(FILE *f) { FILECALL; return real_fflush(f); }
int fsync(int fd) { FILECALL; return real_fsync(fd); }
int rename(const char *a, const char *b) { FILECALL; return real_rename(a, b); }
int unlink(const char *p) { FILECALL; return real_unlink(p); }
int mkdir(const char *p, mode_t m) { FILECALL; return real_mkdir(p, m); }
int nanosleep(const struct timespec *a, struct timespec *b) { if (in_audio) n_sleep++; return real_nanosleep(a, b); }
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg)
{
    if (in_audio) n_thread++;
    return real_pthread_create(t, a, f, arg);
}
int pthread_join(pthread_t t, void **r) { if (in_audio) n_thread++; return real_pthread_join(t, r); }
int sched_setaffinity(pid_t p, size_t n, const cpu_set_t *s) { if (in_audio) n_thread++; return real_sched_setaffinity(p, n, s); }
int sched_setscheduler(pid_t p, int pol, const struct sched_param *sp)
{
    if (in_audio) n_thread++;
    return real_sched_setscheduler(p, pol, sp);
}
static unsigned long rules(void) { return n_alloc + n_file + n_thread + n_sleep + n_log; }

/* ------------------------------------------------------------------------------------------ the host */

static void host_log(const char *m) { if (in_audio) n_log++; else fprintf(stderr, "  [log] %s\n", m); }
static float host_bpm = 120.0f;
static float get_bpm(void) { return host_bpm; }
static host_api_v1_t host;              /* as the shim fills it (zeroed, the reserved tail NULL) */
static plugin_api_v2_t *api;
static char dir[512];
static void *inst;
static int16_t blk[256];
static uint64_t nblk;                  /* blocks rendered since create */

static void *open_js(const char *js)
{
    void *in = api->create_instance(dir, js);
    if (!in) { fprintf(stderr, "create_instance failed\n"); exit(1); }
    nblk = 0;
    return in;
}

static void *open_at(int pattern, int seed)
{
    char js[256];
    if (seed >= 0) snprintf(js, sizeof js, "{\"defaults\": {\"seed\": %d, \"pattern\": %d, \"data_dir\": \"\"}}", seed, pattern);
    else snprintf(js, sizeof js, "{\"defaults\": {\"pattern\": %d, \"data_dir\": \"\"}}", pattern);
    return open_js(js);
}

/* the audio thread's calls */
static void A_render(void) { in_audio = 1; api->render_block(inst, blk, 128); in_audio = 0; nblk++; }
static void A_midi(const uint8_t *m, int n, int src) { in_audio = 1; api->on_midi(inst, m, n, src); in_audio = 0; }
static void A_note(int st, int n, int v) { uint8_t m[3] = {(uint8_t)st, (uint8_t)n, (uint8_t)v}; A_midi(m, 3, MOVE_MIDI_SOURCE_INTERNAL); }
static void A_rt(int st) { uint8_t b = (uint8_t)st; A_midi(&b, 1, MOVE_MIDI_SOURCE_EXTERNAL); }
static void A_set(const char *k, const char *v) { in_audio = 1; api->set_param(inst, k, v); in_audio = 0; }
static int A_get(const char *k, char *b, int n) { in_audio = 1; int r = api->get_param(inst, k, b, n); in_audio = 0; return r; }
static void blocks(int n) { while (n-- > 0) A_render(); }
static void in(const char *s) { A_set("in", s); }
static void btn(int cc, int v) { char s[32]; snprintf(s, sizeof s, "B %d %d", cc, v); in(s); }
static void tap(int cc) { btn(cc, 127); A_render(); btn(cc, 0); A_render(); }
static void step(int k, int down) { A_note(down ? 0x90 : 0x80, 16 + k, down ? 100 : 0); }
static void step_tap(int k) { step(k, 1); A_render(); step(k, 0); A_render(); }
static void pad(int i, int down, int vel) { A_note(down ? 0x90 : 0x80, 68 + i, down ? vel : 0); }
static void knob(int k, int d) { char s[32]; snprintf(s, sizeof s, "K %d %d", k, d); in(s); }
static void jog(int d) { char s[32]; snprintf(s, sizeof s, "J %d", d); in(s); }
static void page(int pg) { const int j = ms_pad_of(pg); pad(MS_PAD0 + j, 1, 100); A_render(); pad(MS_PAD0 + j, 0, 0); A_render(); }

static char vbuf[8192], dbuf[8192], cbuf[512];
static const char *view(void) { A_get("view", vbuf, sizeof vbuf); return vbuf; }
static const char *dump(void) { A_get("dump", dbuf, sizeof dbuf); return dbuf; }
static int vint(const char *s, const char *key)
{
    char k[32];
    snprintf(k, sizeof k, " %s=", key);
    const char *p = strstr(s, k);
    if (!p && !strncmp(s, k + 1, strlen(k) - 1)) p = s - 1;
    return p ? atoi(p + strlen(k)) : -9999;
}
static const char *vstr(const char *s, const char *key, char *o, int n)
{
    char k[32];
    snprintf(k, sizeof k, " %s=", key);
    const char *p = strstr(s, k);
    o[0] = 0;
    if (!p) return o;
    p += strlen(k);
    int i = 0;
    while (*p && *p != ' ' && i < n - 1) o[i++] = *p++;
    o[i] = 0;
    return o;
}
static void vlist(const char *s, const char *key, int *o, int n)
{
    char b[600];
    vstr(s, key, b, sizeof b);
    const char *p = b;
    for (int i = 0; i < n; i++) { o[i] = atoi(p); p = strchr(p, ','); if (!p) break; p++; }
}

static int fails, checks;
static void check(int ok, const char *what) { printf("  %s  %s\n", ok ? "ok  " : "FAIL", what); fails += !ok; checks++; }

static uint32_t fnv_pcm(uint32_t h, const int16_t *s, int n)
{
    const uint8_t *b = (const uint8_t *)s;
    for (int i = 0; i < 4 * n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

static void nap(int ms) { struct timespec ts = {0, ms * 1000000L}; real_nanosleep(&ts, 0); }
static void nap_us(int us) { struct timespec ts = {0, us * 1000L}; real_nanosleep(&ts, 0); }

static const char *const NAME[4] = {"A01", "A02", "A03", "A04"};

/* ------------------------------------------------------------------------------------------ hash, time */

static int do_hash(void)
{
    static int16_t b[2 * 11 * 44100];
    const int len = 10 * 44100, n = (len + 127) / 128 * 128;
    for (int p = 0; p < 4; p++) {
        double t0 = now_us();
        inst = open_at(p, 2000);
        double tc = now_us() - t0;
        api->set_param(inst, "play", "1");
        for (int f = 0; f < n; f += 128) api->render_block(inst, b + 2 * f, 128);
        printf("  %-6s %08x   (create_instance %.1f ms)\n", NAME[p], fnv_pcm(2166136261u, b, len), tc / 1000);
        api->destroy_instance(inst);
    }
    return 0;
}

static int perf_fd = -1;
static void perf_open(void)
{
    struct perf_event_attr a;
    memset(&a, 0, sizeof a);
    a.type = PERF_TYPE_HARDWARE;
    a.size = sizeof a;
    a.config = PERF_COUNT_HW_INSTRUCTIONS;
    a.exclude_kernel = 1;
    a.exclude_hv = 1;
    perf_fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
}
static uint64_t perf_read(void)
{
    uint64_t v = 0;
    if (perf_fd < 0 || read(perf_fd, &v, sizeof v) != (ssize_t)sizeof v) return 0;
    return v;
}

static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static int do_time(double sec)
{
    const int nb = (int)(sec * 44100 / 128);
    double *us = malloc(sizeof(double) * (size_t)nb);
    perf_open();
    printf("  one 128-frame block (2902 us of sound), %.0f s from 2 s in, one core:\n", sec);
    printf("  %-5s %8s %8s %8s %8s %7s %14s\n", "", "mean us", "p50", "p99", "most", "mean %", "instr/sample");
    for (int p = 0; p < 4; p++) {
        inst = open_at(p, 2000);
        api->set_param(inst, "play", "1");
        for (int f = 0; f < 2 * 44100; f += 128) api->render_block(inst, blk, 128);
        double sum = 0;
        uint64_t i0 = perf_read();
        for (int k = 0; k < nb; k++) {
            double t0 = now_us();
            api->render_block(inst, blk, 128);
            us[k] = now_us() - t0;
            sum += us[k];
        }
        uint64_t ins = perf_read() - i0;
        qsort(us, (size_t)nb, sizeof *us, cmp_d);
        char is[24];
        if (perf_fd >= 0 && ins) snprintf(is, sizeof is, "%.0f", (double)ins / ((double)nb * 128));
        else snprintf(is, sizeof is, "n/a");
        printf("  %-5s %8.1f %8.1f %8.1f %8.1f %6.1f%% %14s\n", NAME[p], sum / nb, us[nb / 2], us[nb * 99 / 100],
               us[nb - 1], sum / nb / 2902.0 * 100, is);
        api->destroy_instance(inst);
    }
    /* the screen, as the Move runs it (A04 playing, a page at a time, a knob turned now and then, ui.js's reads every
     * 12 blocks, the render paced to real time so the drawer thread draws as it would): the page state's publish on
     * the audio thread (the controller, the LEDs, the screen's gather) and the drawing on the drawer's, a frame each */
    static const int PG[] = {MS_HOME, MS_SYN, MS_AMP, MS_FILT, MS_EFX, MS_DSTR, MS_LFO1, MS_TRK, MS_PAT, MS_GRID, MS_MIX,
                             MS_GLO, MS_PERF, MS_CAT};
    inst = open_js("{\"defaults\": {\"seed\": 2000, \"pattern\": 3, \"data_dir\": \"\"}}");
    api->set_param(inst, "play", "1");
    for (int f = 0; f < 2 * 44100; f += 128) api->render_block(inst, blk, 128);
    char cb[256], fb[4096];
    unsigned pub_n = 0, pub_mean = 0, pub_most = 0, draw_n = 0, draw_mean = 0, draw_most = 0, reads = 0;
    double block_sum = 0, block_most = 0;
    int nbk = 0;
    for (unsigned g = 0; g < sizeof PG / sizeof PG[0]; g++) {
        const int j = ms_pad_of(PG[g]);
        uint8_t on[3] = {0x90, (uint8_t)(68 + MS_PAD0 + j), 100}, off[3] = {0x80, (uint8_t)(68 + MS_PAD0 + j), 0};
        api->on_midi(inst, on, 3, MOVE_MIDI_SOURCE_INTERNAL);
        api->on_midi(inst, off, 3, MOVE_MIDI_SOURCE_INTERNAL);
        for (int k = 0; k < 344; k++) {                  /* (a second a page) */
            if (k % 40 == 0) api->set_param(inst, "in", k % 80 ? "K 2 1" : "K 2 -1");
            const double t0 = now_us();
            api->render_block(inst, blk, 128);
            const double d = now_us() - t0;
            block_sum += d; nbk++;
            if (d > block_most) block_most = d;
            if (k % 12 == 0) api->get_param(inst, "frame", fb, sizeof fb);
            const double left = 2902 - (now_us() - t0);
            if (left > 0) nap_us((int)left);
        }
        if (PG[g] == MS_CAT || PG[g] == MS_PERF) { api->on_midi(inst, on, 3, MOVE_MIDI_SOURCE_INTERNAL); api->on_midi(inst, off, 3, MOVE_MIDI_SOURCE_INTERNAL); }
    }
    api->get_param(inst, "cost", cb, sizeof cb);
    sscanf(cb, "%u %u %u %u %u %u %u", &pub_n, &pub_mean, &pub_most, &draw_n, &draw_mean, &draw_most, &reads);
    printf("  the screen, 14 pages a second each (A04 playing, real time): %u publishes (the audio thread) %.1f us mean, "
           "%.1f most; %u frames drawn (the drawer thread) %.1f us mean, %.1f most; %u reads; a block %.1f us mean, "
           "%.1f most\n", pub_n, pub_mean / 1000.0, pub_most / 1000.0, draw_n, draw_mean / 1000.0, draw_most / 1000.0,
           reads, block_sum / nbk, block_most);
    api->destroy_instance(inst);
    /* the same, drawn in the render (no drawer thread: every other publish), for scale */
    inst = open_js("{\"defaults\": {\"seed\": 2000, \"pattern\": 3, \"data_dir\": \"\", \"draw_thread\": 0}}");
    api->set_param(inst, "play", "1");
    for (int k = 0; k < nb; k++) { if (k % 50 == 0) api->set_param(inst, "in", "K 0 1"); api->render_block(inst, blk, 128); }
    api->get_param(inst, "cost", cb, sizeof cb);
    sscanf(cb, "%u %u %u %u %u %u %u", &pub_n, &pub_mean, &pub_most, &draw_n, &draw_mean, &draw_most, &reads);
    printf("  drawn in the render instead (A04, no drawer thread): a frame %.1f us mean, %.1f most, on the audio thread\n",
           draw_mean / 1000.0, draw_most / 1000.0);
    api->destroy_instance(inst);
    free(us);
    return 0;
}

/* --------------------------------------------------------------------------------------------- ctl */

static int trig_at(int k) { int st[16]; vlist(view(), "st", st, 16); return st[k] & 1; }
static int step_flags(int k) { int st[16]; vlist(view(), "st", st, 16); return st[k]; }
static int kv(int k) { int v[8]; vlist(view(), "k", v, 8); return v[k]; }
static int kf(int k) { int v[8]; vlist(view(), "kf", v, 8); return v[k]; }
static int dump_kit(int p) { int v[80]; char b[2000]; vstr(dump(), "kit", b, sizeof b); const char *q = b; for (int i = 0; i < 71; i++) { v[i] = atoi(q); q = strchr(q, ','); if (!q) break; q++; } return v[p]; }
static int dump_step(int s, int *note)
{
    char b[2000];
    vstr(dump(), "steps", b, sizeof b);
    const char *q = b;
    for (int i = 0; i < s && q; i++) { q = strchr(q, ','); if (q) q++; }
    if (!q) return -1;
    if (note) { const char *d = strchr(q, '.'); *note = d ? atoi(d + 1) : -1; }
    return atoi(q);
}
static char sdump[8][2000];
static void save_steps(int slot) { vstr(dump(), "steps", sdump[slot], sizeof sdump[slot]); }
static int same_steps(int slot) { char b[2000]; vstr(dump(), "steps", b, sizeof b); return !strcmp(b, sdump[slot]); }

/* ------------------------------------------------------------- ctl: PADS and Capture (the owner's, 2026-11-02) */

static void view_track(int t) { char s[8]; snprintf(s, sizeof s, "%d", t); A_set("track", s); blocks(1); }
/* the track in view's root as the FM-1's PADS plays it, worked out again from the dump: the note its trigs sit on
 * most within the pattern's LEN, the lowest of equals (m2k_proj.c pat_root); -1 without trigs */
static int root_now(void)
{
    char b[2000];
    int glob[8];
    vstr(dump(), "steps", b, sizeof b);
    vlist(dump(), "glob", glob, 8);
    const int len = glob[2] < 1 ? 1 : glob[2] > 64 ? 64 : glob[2];   /* (G_LEN: the third global) */
    int cnt[128] = {0}, best = -1;
    const char *q = b;
    for (int s = 0; s < len && q; s++) {
        const int f = atoi(q);
        const char *d = strchr(q, '.');
        const int n = (d ? atoi(d + 1) : 0) & 127;
        if (f & 1) { cnt[n]++; if (best < 0 || cnt[n] > cnt[best] || (cnt[n] == cnt[best] && n < best)) best = n; }
        q = strchr(q, ',');
        if (q) q++;
    }
    return best;
}
/* the track in view's step playing (get_param "ui"), -1 stopped */
static int pos_now(void) { char b[256]; int pl = 0, tp = -1; A_get("ui", b, sizeof b); sscanf(b, "%d %d", &pl, &tp); return tp; }
/* playing on until the track in view's step s has just begun: a pad pressed now falls on it (early in it) */
static void until_step(int s) { for (int g = 0; g < 4000 && pos_now() != s; g++) A_render(); }
/* a pad hit: pressed, held two blocks, let go (three blocks in all; the press is heard at the first) */
static void hit(int i, int vel) { pad(i, 1, vel); blocks(2); pad(i, 0, 0); A_render(); }

/* PADS (Sample): the bottom row the tracks at their roots, the row over it their mutes, the lights, REC, PERFORM over it */
static void ctl_pads(void)
{
    inst = open_at(0, 2000);
    blocks(4);
    const char *v;
    int pads[32], bl[MV_NBTN], tn[8], root[8];
    for (int t = 0; t < 8; t++) { view_track(t); root[t] = root_now(); }
    view_track(0);
    tap(118);
    v = view();
    vlist(v, "btnled", bl, MV_NBTN);
    check(vint(v, "kpads") == 1 && strstr(v, " toast=PADS|T1-T8, MUTES ABOVE") && bl[11] == 16,
          "Sample (CC 118): PADS, said (PADS / T1-T8, MUTES ABOVE); Sample's LED azure");
    vlist(v, "pads", pads, 32);
    const int mutes = vint(v, "mutes");
    int lit = 1;
    for (int i = 0; i < 8; i++) lit &= pads[i] == 95 && pads[8 + i] == ((mutes >> i) & 1 ? 66 : 16);
    check(lit, "PADS' lights: the tracks dim azure; their mutes over them azure (heard) or dark red (muted)");
    int every = 1;
    for (int t = 0; t < 8; t++) {
        pad(t, 1, 100); blocks(2);
        v = view();
        vlist(v, "tnote", tn, 8);
        vlist(v, "pads", pads, 32);
        if (!((vint(v, "gates") >> t) & 1) || (root[t] >= 0 && tn[t] != root[t]) || pads[t] != 120 || vint(v, "trk") != 0) {
            every = 0;
            printf("  (pad %d: gates %d, note %d, root %d, LED %d, track in view %d)\n", t, vint(v, "gates"), tn[t], root[t],
                   pads[t], vint(v, "trk"));
        }
        pad(t, 0, 0); blocks(2);
    }
    check(every, "the bottom row: pad n plays track n at its root (its trigs' commonest note), lit white while held; the track in view stays");
    const int m3 = (mutes >> 3) & 1;
    pad(11, 1, 100); blocks(1); pad(11, 0, 0); blocks(1);
    v = view();
    vlist(v, "pads", pads, 32);
    check(((vint(v, "mutes") >> 3) & 1) == !m3 && pads[11] == (m3 ? 16 : 66) && strstr(v, m3 ? " toast=UNMUTED|T4" : " toast=MUTED|T4"),
          "the row over it: its 4th pad mutes T4 (stopped: at once, said, its pad dark red), as Mute + step 4 does");
    pad(11, 1, 100); blocks(1); pad(11, 0, 0); blocks(1);
    check(((vint(view(), "mutes") >> 3) & 1) == m3, "tapped again: heard again");
    A_set("play", "1"); blocks(60);                         /* (GLO's MUTES at its start: at the bar) */
    const int m4 = (vint(view(), "mutes") >> 4) & 1;
    pad(12, 1, 100); blocks(1); pad(12, 0, 0);
    const int waits = strstr(view(), " toast=MUTE AT THE BAR|T5") != NULL;
    int dark = 0, red = 0, steady = 1;
    for (int i = 0; i < 90; i++) { A_render(); vlist(view(), "pads", pads, 32); dark += pads[12] == 0; red += pads[12] == 66; }
    check(!m4 && waits && dark > 10 && red > 10, "playing, MUTES at the bar: the tap waits (MUTE AT THE BAR), its pad blinking dark and dark red");
    blocks(1400);
    for (int i = 0; i < 60; i++) { A_render(); vlist(view(), "pads", pads, 32); steady &= pads[12] == 66; }
    check(steady && ((vint(view(), "mutes") >> 4) & 1), "the bar came: T5 muted, its pad steady dark red");
    tap(86);                                                /* REC */
    view_track(1);
    int k = -1, nn = -1;
    for (int s = 2; s < 14 && k < 0; s++) if (!(dump_step(s, 0) & 1)) k = s;
    until_step(k);
    pad(1, 1, 120); blocks(2); pad(1, 0, 0); blocks(2);
    const int fk = dump_step(k, &nn);
    check(k >= 0 && (fk & 3) == 3 && nn == root[1], "REC playing: T2's pad writes T2's step playing, at its root, an accent (velocity 120)");
    tap(86);
    page(MS_PERF);
    pad(0, 1, 100); blocks(2);
    v = view();
    check(vint(v, "perf") == 1 && vint(v, "kpads") == 1, "PERFORM on: the rows are its effects over PADS (the bottom-left RPT 1/4)");
    pad(0, 0, 0); blocks(2);
    tap(118);
    check(vint(view(), "kpads") == 0 && strstr(view(), " toast=KEYS|WHEN PERFORM IS OFF"), "Sample under PERFORM: the mode for after it, said");
    tap(118);
    page(MS_PERF);
    pad(5, 1, 100); blocks(2);
    v = view();
    vlist(v, "pads", pads, 32);
    char pk[16];
    check(!strcmp(vstr(v, "pkeys", pk, sizeof pk), "0,0") && vint(v, "kpads") == 1 && pads[5] == 120,
          "PERFORM off: PADS again (T6's pad held, white)");
    pad(5, 0, 0); blocks(2);
    A_set("play", "0"); blocks(4);
    tap(118);
    v = view();
    vlist(v, "btnled", bl, MV_NBTN);
    check(vint(v, "kpads") == 0 && strstr(v, " toast=KEYS|PLAY THE TRACK") && bl[11] == 0, "Sample again: the piano (KEYS, said), Sample's LED off");
    pad(0, 1, 100); blocks(2);
    vlist(view(), "tnote", tn, 8);
    check((vint(view(), "gates") & 2) && tn[1] == 60 + 12 * vint(view(), "oct"), "the bottom-left pad: C on the track in view (T2) again");
    pad(0, 0, 0); blocks(2);
    api->destroy_instance(inst);
}

/* Capture (CC 52): stopped and playing, whole bars, the track's length, overdub, one Undo, PADS' several tracks */
static void ctl_capture(void)
{
    const char *v;
    int bl[MV_NBTN], tn[8];
    inst = open_at(15, 2000);                               /* A16: a blank, 16 steps; Move's 120 BPM, stopped */
    blocks(4);
    const int c = 60 + 12 * vint(view(), "oct");           /* (the keyboard's C: the track's octave) */
    tap(52);
    v = view();
    vlist(v, "btnled", bl, MV_NBTN);
    check(strstr(v, " toast=NOTHING TO CAPTURE|") && bl[12] == 16, "Capture with nothing played: said so (NOTHING TO CAPTURE); its LED dim");
    step_tap(9);                                            /* (a trig by hand: the capture overdubs) */
    const int hand = dump_step(9, 0) & 1;
    hit(7, 100);                                            /* (the C over, then over 2 bars' silence: 4.35 s) */
    blocks(1500);
    save_steps(0);
    hit(0, 100); blocks(86 - 3);                            /* C the downbeat; E 2 sixteenths on (250 ms: 86 blocks); */
    hit(2, 100); blocks(129 - 3);                           /* G 5 on, hard */
    hit(4, 120); blocks(10);
    v = view();
    vlist(v, "btnled", bl, MV_NBTN);
    check(vint(v, "capn") == 4 && bl[12] == 64, "stopped, REC off: every note kept for Capture (4); its LED lit");
    tap(52);
    int n0 = -1, n2 = -1, n5 = -1, stray = 0;
    const int f0 = dump_step(0, &n0), f2 = dump_step(2, &n2), f5 = dump_step(5, &n5);
    for (int s = 0; s < 64; s++) if (s != 0 && s != 2 && s != 5 && s != 9 && (dump_step(s, 0) & 1)) stray++;
    v = view();
    check((f0 & 3) == 1 && n0 == c && (f2 & 3) == 1 && n2 == c + 4 && (f5 & 3) == 3 && n5 == c + 7 && !stray,
          "Capture stopped: the phrase's first note the downbeat, the rest at 120 (steps 1, 3, 6), the hard one accented; the note 4 s before left out");
    check(hand && (dump_step(9, 0) & 1), "overdub: the trig placed by hand stays");
    check(strstr(v, " toast=CAPTURED 1 BAR|T1") && vint(v, "capn") == 0, "said: CAPTURED 1 BAR / T1; the notes taken");
    tap(52);
    check(strstr(view(), " toast=NOTHING TO CAPTURE|") != NULL, "Capture again: nothing (the same notes never written twice)");
    tap(56);
    check(same_steps(0), "one Undo: the capture taken back, all of it");
    hit(0, 100); blocks(431 - 3);                           /* sixteenths 0, 10, 20 on a track of 16 */
    hit(2, 100); blocks(430 - 3);
    hit(4, 100); blocks(4);
    tap(52);
    int n4 = -1;
    const int f4 = dump_step(4, &n4);
    check((f4 & 1) && n4 == c + 7 && !(dump_step(0, 0) & 1) && !(dump_step(10, 0) & 1) && strstr(view(), " toast=CAPTURED 1 BAR|T1"),
          "stopped, a phrase of 21 sixteenths on a track of 16: its last whole bar from bar 1 (its note at 21 on step 5), no more");
    tap(56);
    page(MS_PAT);
    knob(2, 16); blocks(1);
    check(vint(view(), "np") == 2, "A16 made 32 long (two bars)");
    save_steps(1);
    A_set("play", "1"); blocks(2);
    until_step(2); hit(0, 100);                             /* loop 1: steps 3 and 21 (bar 2) */
    until_step(20); hit(1, 100);
    until_step(6); hit(2, 100);                             /* loop 2: steps 7 and 26, hard */
    until_step(25); hit(3, 120);
    blocks(20);
    tap(52);
    int a6 = -1, a25 = -1;
    const int g6 = dump_step(6, &a6), g25 = dump_step(25, &a25);
    check((g6 & 3) == 1 && a6 == c + 4 && (g25 & 3) == 3 && a25 == c + 5,
          "Capture playing: each note on the bar and step it fell on (bar 1 step 7, bar 2 step 10, accented)");
    check(!(dump_step(2, 0) & 1) && !(dump_step(20, 0) & 1) && (dump_step(9, 0) & 1),
          "the track's length at most: loop 1's notes (over 2 bars back) left out; the hand's trig kept");
    check(strstr(view(), " toast=CAPTURED 2 BARS|T1") != NULL, "said: CAPTURED 2 BARS / T1");
    tap(56);
    check(same_steps(1), "Undo: as it was");
    until_step(20); hit(4, 100);                            /* bar 2 step 5, then the next loop's bar 2 step 3: 30 */
    until_step(18); hit(5, 100);                            /* sixteenths apart, but a bar before the last bar's loop */
    blocks(10);
    tap(52);
    int a18 = -1;
    const int h18 = dump_step(18, &a18);
    check((h18 & 1) && a18 == c + 9 && !(dump_step(20, 0) & 1) && strstr(view(), " toast=CAPTURED 1 BAR|T1"),
          "whole bars: the last note's bar back (1 bar); a note 30 sixteenths before it, in the bar before, left out");
    tap(56);
    A_set("play", "0"); blocks(4);
    tap(118);                                               /* PADS, stopped: T1, T2, T3 a beat apart, T3 hard */
    for (int t = 0; t < 3; t++) { view_track(t); save_steps(2 + t); }
    view_track(0);
    int r[3];
    hit(0, 100); vlist(view(), "tnote", tn, 8); r[0] = tn[0]; blocks(172 - 3);
    hit(1, 100); vlist(view(), "tnote", tn, 8); r[1] = tn[1]; blocks(172 - 3);
    hit(2, 120); vlist(view(), "tnote", tn, 8); r[2] = tn[2]; blocks(4);
    tap(52);
    v = view();
    const int said = strstr(v, " toast=CAPTURED 1 BAR|T1 T2 T3") != NULL;
    int each = 1, nt = -1;
    for (int t = 0; t < 3; t++) {
        view_track(t);
        const int f = dump_step(4 * t, &nt);
        if (!(f & 1) || nt != r[t] || ((f & 2) != 0) != (t == 2)) { each = 0; printf("  (T%d: step %d flags %d note %d, root %d)\n", t + 1, 4 * t + 1, f, nt, r[t]); }
    }
    check(said && each, "PADS, Capture: each track's hits into its own track (T1 step 1, T2 step 5, T3 step 9, accented), CAPTURED 1 BAR / T1 T2 T3");
    tap(56);
    int undone = 1;
    for (int t = 0; t < 3; t++) { view_track(t); undone &= same_steps(2 + t); }
    check(undone, "one Undo takes all three tracks' back");
    api->destroy_instance(inst);
}

static int do_ctl(void)
{
    char s[64];
    inst = open_at(0, 2000);
    blocks(4);
    const char *v = view();
    printf("  view at power-on: %.160s...\n", v);
    check(vint(v, "pg") == MS_HOME && vint(v, "trk") == 0 && !strcmp(vstr(v, "pat", s, sizeof s), "A01") &&
          vint(v, "sync") == 1 && vint(v, "play") == 0, "power-on: HOME, track 1, A01, SYNC, stopped");

    /* the page pads: each its page, lit bright in its family's colour, the rest of its row dim */
    int every = 1, lights = 1;
    for (int j = 0; j < 16; j++) {
        const int pg = MS_PAD_PAGE[j];
        if (pg == MS_CAT || pg == MS_PERF) continue;           /* (the two functions: below) */
        page(pg);
        v = view();
        int pads[32];
        vlist(v, "pads", pads, 32);
        if (vint(v, "pg") != pg || vint(v, "spg") != pg) { every = 0; printf("  (pad %d: page %d, wanted %d)\n", 16 + j, vint(v, "pg"), pg); }
        const int fam = ms_family(pg), on = fam == MS_FAM_SOUND ? 3 : 16, dim = fam == MS_FAM_SOUND ? 69 : 95;
        for (int i = 0; i < 16; i++) {
            const int want = i == j ? on : ms_family(MS_PAD_PAGE[i]) == MS_FAM_SOUND ? 69 : ms_family(MS_PAD_PAGE[i]) == MS_FAM_FN ? 115 : 95;
            if (pads[16 + i] != want) { lights = 0; printf("  (page %d: pad %d lit %d, wanted %d)\n", pg, 16 + i, pads[16 + i], want); }
        }
        (void)dim;
    }
    check(every, "the 14 page pads: each its page, one press, on the screen too");
    check(lights, "the page pads lit: the page in view bright (sound orange, the rest azure), the others dim, the functions magenta");

    /* the knobs: a range (AMP's VOL), a choice (DRM-KICK's WAVE, SYN A) */
    page(MS_AMP);
    int k0 = kv(5), m0 = dump_kit(13);
    knob(5, 3); blocks(1);
    check(kv(5) == k0 + 3 && dump_kit(13) == m0 + 3, "AMP's knob 6 turned +3: VOL, the kit and the screen");
    knob(5, -200); blocks(1);
    check(kv(5) == 0, "a turn past the range stops at 0");
    btn(119, 127); knob(5, 1); blocks(1); btn(119, 0); blocks(1);
    check(kv(5) == 68, "Delete + a knob: the value back to its start (DRM-KICK's INIT VOL, 68)");
    page(MS_SYN);
    int w0 = dump_kit(0);
    knob(0, 1); blocks(1);
    check((dump_kit(0) * 4) >> 7 == ((w0 * 4) >> 7) + 1, "SYN's knob 1 on a choice (the kick's WAVE): a step one choice");
    char kk[16], kk2[16], kk3[16], kk4[16];
    vstr(view(), "kk", kk, sizeof kk);
    knob(0, -1); blocks(1);
    page(MS_AMP); vstr(view(), "kk", kk2, sizeof kk2);
    page(MS_PAT); vstr(view(), "kk", kk3, sizeof kk3);
    page(MS_HOME); vstr(view(), "kk", kk4, sizeof kk4);
    check(kk[0] == '0' + MV_KK_CHOICE && !strcmp(kk2, "00000000") && kk3[0] == '0' + MV_KK_TEMPO && kk4[2] == '0' + MV_KK_TEMPO,
          "ui.js told how each knob turns: the kick's WAVE a short list, AMP ranges, PAT's knob 1 and HOME's 3 the tempo");
    page(MS_SYN);
    in("T 2 1"); blocks(1);
    v = view();
    check(vint(v, "focus") == 2 && vint(v, "tk") == 2, "knob 3 touched: its full name and value large");
    in("T 2 0"); blocks(1);
    int still = vint(view(), "focus") == 2;
    blocks(500);
    check(still && vint(view(), "focus") == -1, "let go: shown a moment more, then the page");
    /* MIX: a knob a track */
    page(MS_MIX);
    int l2 = kv(2);
    knob(2, -5); blocks(1);
    in("B 41 127;B 41 0"); blocks(1);                      /* (track 3 in view: its LEVL is the kit's) */
    check(kv(2) == l2 - 5 && dump_kit(37) == l2 - 5, "MIX: knob 3 is track 3's level (track 1 in view)");
    knob(2, 5); blocks(1);
    in("B 43 127;B 43 0"); blocks(1);

    /* the jog: no pages any more; the patterns' list (a click loads), the machine with Shift */
    page(MS_AMP);
    jog(1); blocks(1);
    v = view();
    check(vint(v, "pg") == MS_AMP && vint(v, "ov") == MV_OV_PATS && vint(v, "psel") == 1 && vint(v, "spg") == MS_PATS,
          "the jog turned: the page stays, the patterns' list opens on A02");
    tap(3);
    v = view();
    check(!strcmp(vstr(v, "pat", s, sizeof s), "A02") && vint(v, "ov") == 0 && vint(v, "pg") == MS_AMP,
          "a click: A02 (stopped: at once), the list closed, the page as it was");
    jog(-1); blocks(1); tap(3);
    check(!strcmp(vstr(view(), "pat", s, sizeof s), "A01"), "and back to A01");
    jog(2); blocks(1300);
    check(vint(view(), "ov") == 0 && !strcmp(vstr(view(), "pat", s, sizeof s), "A01"), "the list left alone: it closes, nothing loaded");
    char mach0[16], mach1[16];
    vstr(view(), "mach", mach0, sizeof mach0);
    btn(49, 127); jog(1); blocks(1); btn(49, 0); blocks(1);
    vstr(view(), "mach", mach1, sizeof mach1);
    check(strcmp(mach0, mach1) != 0 && vint(view(), "ov") == 0, "Shift + jog: the track's machine (the FM-1's ALGO)");
    btn(49, 127); jog(-1); blocks(1); btn(49, 0); blocks(1);
    check(!strcmp(vstr(view(), "mach", mach1, sizeof mach1), mach0), "and back");
    page(MS_SYN);

    /* the steps */
    int e = 0;
    while (e < 16 && trig_at(e)) e++;
    step_tap(e);
    check(trig_at(e), "a step tapped: a trig");
    step_tap(e);
    check(!trig_at(e), "tapped again: taken away");
    step(e, 1); blocks(2);
    check(step_flags(e) & 16, "held: shown held");
    knob(0, 5); blocks(1);
    check((kf(0) & 2) && (step_flags(e) & 2), "held + a knob: a lock (the knob shows it, the step has one)");
    int note0 = 0;
    dump_step(e, &note0);
    jog(2); blocks(1);
    int note1 = 0;
    dump_step(e, &note1);
    check(note1 == note0 + 2 && vint(view(), "ov") == 0, "held + the jog: its note (no list)");
    in("B 55 127;B 55 0"); blocks(1);
    step(e, 0); blocks(400);
    check(trig_at(e) && (step_flags(e) & 2) && (step_flags(e) & 64), "let go: the trig stays, locked, its ratchet (Up)");
    check(vint(dump(), "locks") >= 1, "the lock is in the pattern");
    page(MS_FILT);
    int e2 = 0;
    while (e2 < 16 && (e2 == e || trig_at(e2))) e2++;
    step_tap(e2);
    int placed = trig_at(e2);
    step_tap(e2);
    check(placed && !trig_at(e2) && vint(view(), "pg") == MS_FILT, "on FILT the steps are the sequencer still (a trig, and away)");
    page(MS_SYN);

    /* a held step's card: a tap stays quiet; held past one, or edited, it says what the step holds, the moment it
     * changes; a knob touched keeps the stage; let go, it is gone and the step keeps what was set */
    int hs = 0;
    char hv[16];
    while (hs < 16 && trig_at(hs)) hs++;
    step(hs, 1); blocks(2);
    check(vint(view(), "hn") == 0 && (step_flags(hs) & 16), "a step just pressed: no card yet");
    blocks(110);
    check(vint(view(), "hn") == 1 && vint(view(), "hstep") == hs + 1 && !strcmp(vstr(view(), "hcond", hv, sizeof hv), "100%")
          && vint(view(), "hrt") == 1, "held past a tap: its card (step, 100 %, one hit)");
    static const char *const HC[5] = {"90%", "75%", "60%", "50%", "40%"};
    btn(49, 127); blocks(1);
    for (int i = 0; i < 5; i++) {
        jog(1); blocks(1);
        check(!strcmp(vstr(view(), "hcond", hv, sizeof hv), HC[i]), "Hold + Shift + jog: each chance named as it is set");
    }
    jog(-1); blocks(1);
    check(!strcmp(vstr(view(), "hcond", hv, sizeof hv), "50%"), "the jog back: the chance before");
    btn(49, 0); blocks(1);
    char hn0[8], hn1[8];
    vstr(view(), "hnote", hn0, sizeof hn0);
    jog(1); blocks(1);
    check(strcmp(vstr(view(), "hnote", hn1, sizeof hn1), hn0) != 0, "Hold + jog: the note named as it is set");
    in("B 55 127;B 55 0"); blocks(1);
    check(vint(view(), "hrt") == 2, "Hold + Up: the ratchet named as it is set");
    in("T 0 1"); blocks(1);
    check(vint(view(), "hn") == 0 && vint(view(), "focus") == 0 && vint(view(), "held") == 1, "a knob touched keeps the stage (what is being locked)");
    in("T 0 0"); blocks(1000);
    check(vint(view(), "hn") == 1, "the knob let go: the card again");
    int hs2 = 0;
    while (hs2 < 16 && (hs2 == hs || !trig_at(hs2))) hs2++;   /* (a step with a trig: a later empty one is a length) */
    step(hs2, 1); blocks(2);
    check(vint(view(), "hn") == 2 && vint(view(), "hstep") == hs + 1, "a second held: counted, the first's values shown");
    step(hs2, 0); blocks(1);
    step(hs, 0); blocks(2);
    check(vint(view(), "hn") == 0, "let go: the card gone");
    step(hs, 1); blocks(2);
    check(vint(view(), "hn") == 0, "pressed again: quiet until held");
    blocks(110);
    check(!strcmp(vstr(view(), "hcond", hv, sizeof hv), "50%") && vint(view(), "hrt") == 2, "held again: the step kept its chance and ratchet");
    step(hs, 0); blocks(2);
    step_tap(hs);
    check(!trig_at(hs), "tapped away: the trig and its condition go");

    /* the jog's two jobs with a step held, one hand on it: a click turns it to the condition at once, and it stays for
     * the next step held; a click turns it back; no list opens, no step goes; let go, a click is the list's again */
    step(hs, 1); blocks(2);
    tap(3); blocks(1);
    check(vint(view(), "hn") == 1 && vint(view(), "hjog") == 1 && vint(view(), "ov") == 0,
          "Hold + a click: the card at once, the jog on the chance, no list");
    vstr(view(), "hnote", hn0, sizeof hn0);
    jog(1); blocks(1);
    check(!strcmp(vstr(view(), "hcond", hv, sizeof hv), "90%") && !strcmp(vstr(view(), "hnote", hn1, sizeof hn1), hn0),
          "Hold, a click, the jog: the chance moves, the note does not");
    step(hs, 0); blocks(2);
    int hj = hs + 1;
    while (hj < 16 && trig_at(hj)) hj++;
    step(hj, 1); blocks(2);
    jog(1); blocks(1);
    check(vint(view(), "hjog") == 1 && vint(view(), "hstep") == hj + 1 && !strcmp(vstr(view(), "hcond", hv, sizeof hv), "90%"),
          "the next step held: the jog still on the chance, no click needed");
    tap(3); blocks(1);
    vstr(view(), "hnote", hn0, sizeof hn0);
    jog(1); blocks(1);
    check(vint(view(), "hjog") == 0 && strcmp(vstr(view(), "hnote", hn1, sizeof hn1), hn0) != 0
          && !strcmp(vstr(view(), "hcond", hv, sizeof hv), "90%"), "a second click: the jog on the note again");
    step(hj, 0); blocks(2);
    step(hs, 1); blocks(2); tap(3); blocks(1); step(hs, 0); blocks(2);
    check(trig_at(hs), "a quick tap with a click in it: the trig stays");
    tap(3); blocks(1);
    check(vint(view(), "ov") == MV_OV_PATS, "no step held: a click opens the patterns' list, whatever the jog was on");
    tap(50); blocks(1);
    step(hs, 1); blocks(110);          /* (hjog is the card's: read it while there is one) */
    check(vint(view(), "hn") == 1 && vint(view(), "hjog") == 1 && vint(view(), "ov") == 0, "the list come and gone: the jog still on the chance");
    tap(3); blocks(1);
    check(vint(view(), "hjog") == 0, "a click: back on the note");
    step(hs, 0); blocks(2);
    step_tap(hs); step_tap(hj);
    check(!trig_at(hs) && !trig_at(hj), "(both tapped away)");

    /* a note's length, on a blank pattern: two fingers together still place two trigs; held past a tap, a later empty
     * step sets it (two locks, no trig there), a nearer one changes it, the last again clears it, past 8 says so; a
     * trig or an earlier step does as ever; Undo takes it back; it plays that long */
    btn(49, 127); step(4, 1); blocks(1); step(4, 0); btn(49, 0); blocks(2);
    check(!strcmp(vstr(view(), "pat", hv, sizeof hv), "A05") && !trig_at(0) && !trig_at(3), "a blank pattern to try lengths on");
    const int lk0 = vint(dump(), "locks");
    step(0, 1); blocks(2);
    step_tap(3); blocks(2);
    check(trig_at(0) && trig_at(3) && vint(view(), "hlen") == 0 && vint(dump(), "locks") == lk0, "two fingers within a tap: two trigs, as ever");
    step(0, 0); blocks(2);
    step_tap(3); step_tap(0); blocks(2);
    check(!trig_at(0) && !trig_at(3), "and both tapped away");
    step(0, 1); blocks(110);
    step_tap(3); blocks(2);
    check(trig_at(0) && !trig_at(3) && vint(view(), "hlen") == 4 && vint(view(), "held") == 1, "held past a tap, a later empty step tapped: the note lasts to it, no trig there");
    check(vint(dump(), "locks") == lk0 + 2, "its length is two locks");
    {
        int led[16];
        vlist(view(), "led", led, 16);
        check(led[1] == 95 && led[2] == 95 && led[3] == 95 && led[4] != 95, "the steps it lasts over dim azure while held");
    }
    step_tap(5); blocks(2);
    check(vint(view(), "hlen") == 6 && vint(dump(), "locks") == lk0 + 2, "a further step: longer, the same two locks");
    step_tap(2); blocks(2);
    check(vint(view(), "hlen") == 3 && !trig_at(2), "a nearer step: shorter");
    step_tap(2); blocks(2);
    check(vint(view(), "hlen") == 0 && vint(dump(), "locks") == lk0, "the last step again: its own length back, the locks gone");
    step_tap(8); blocks(2);
    check(vint(view(), "hlen") == 0 && !trig_at(8), "past 8 steps: said, nothing set, no trig");
    step_tap(7); blocks(2);
    check(vint(view(), "hlen") == 8, "8 steps, the most");
    tap(56);
    check(vint(view(), "hlen") == 0 && vint(dump(), "locks") == lk0, "Undo takes the length back");
    step_tap(2); blocks(2);
    step(0, 0); blocks(2);
    check(trig_at(0) && vint(dump(), "locks") == lk0 + 2, "let go: the trig stays, its length with it");
    step(4, 1); blocks(110);
    step_tap(2); blocks(2);
    check(trig_at(2) && vint(view(), "held") == 1, "an earlier empty step: a trig, as ever");
    step(4, 0); blocks(2);
    step_tap(2); step_tap(4); blocks(2);
    step(0, 1); blocks(110);
    step(6, 1); blocks(2);
    int two = trig_at(6);
    step(6, 0); blocks(2);
    check(!two && vint(view(), "hlen") == 7, "the first held alone: still the length");
    step_tap(6); blocks(2);                             /* (back to 3 steps through its own: 7 cleared, then 3) */
    step_tap(2); blocks(2);
    step(0, 0); blocks(2);
    check(vint(dump(), "locks") == lk0 + 2, "3 steps set");
    {
        A_set("play", "1");
        int on = 0, t0 = -1, t1 = -1;
        for (int i = 0; i < 300; i++) {
            A_render();
            if (vint(view(), "gates") & 1) { if (t0 < 0) t0 = i; t1 = i; on++; }
        }
        A_set("play", "0"); blocks(4);
        const double want = 3 * 44100.0 * 600 / (vint(view(), "bpm10") * 4.0) / 128;
        printf("  the note's gate: %d blocks, 3 steps at %d tenths of BPM are %.1f\n", t1 - t0 + 1, vint(view(), "bpm10"), want);
        check(t0 >= 0 && abs(t1 - t0 + 1 - (int)(want + 0.5)) <= 2, "it plays that long: the gate open 3 steps");
    }
    btn(119, 127); step(0, 1); blocks(1); step(0, 0); btn(119, 0); blocks(2);
    check(!trig_at(0) && vint(dump(), "locks") == lk0, "Delete + the step: its length with it");
    btn(49, 127); step(0, 1); blocks(1); step(0, 0); btn(49, 0); blocks(2);
    check(!strcmp(vstr(view(), "pat", hv, sizeof hv), "A01"), "back to A01");

    /* Delete: a lock away with a step held, a trig away, a value home */
    step(e, 1); blocks(1);
    btn(119, 127); knob(0, 1); blocks(1); btn(119, 0);
    step(e, 0); blocks(1);
    check(!(step_flags(e) & 2), "Delete + a knob, a step held: its lock away");
    btn(119, 127); step(e, 1); blocks(1); step(e, 0); btn(119, 0); blocks(1);
    check(!trig_at(e), "Delete + a step: its trig away");
    tap(56);
    check(trig_at(e), "Undo: back");
    btn(49, 127); tap(56); btn(49, 0); blocks(1);
    check(!trig_at(e), "Shift + Undo: redone");

    /* Copy: a step, a track, a pattern */
    step_tap(0);
    if (!trig_at(0)) step_tap(0);
    int f5 = trig_at(5);
    if (f5) { btn(119, 127); step(5, 1); blocks(1); step(5, 0); btn(119, 0); blocks(1); }
    btn(60, 127); step(0, 1); blocks(1); step(0, 0); step(5, 1); blocks(1); step(5, 0); btn(60, 0); blocks(1);
    check(trig_at(5), "Copy + a step, then another: pasted there");
    save_steps(0);
    btn(60, 127); btn(43, 127); blocks(1); btn(43, 0); btn(41, 127); blocks(1); btn(41, 0); btn(60, 0); blocks(1);
    in("B 41 127;B 41 0"); blocks(1);
    check(vint(view(), "trk") == 2 && same_steps(0), "Copy + track 1, then track 3: its steps and sound");
    in("B 43 127;B 43 0"); blocks(1);
    btn(60, 127); btn(49, 127); step(0, 1); blocks(1); step(0, 0); step(9, 1); blocks(1); step(9, 0);
    btn(49, 0); btn(60, 0); blocks(1);
    btn(49, 127); step(9, 1); blocks(1); step(9, 0); btn(49, 0); blocks(1);
    check(!strcmp(vstr(view(), "pat", s, sizeof s), "A10") && same_steps(0), "Copy + Shift + A01, then A10: the pattern");
    btn(119, 127); in("B 43 127;B 43 0"); blocks(1); btn(119, 0); blocks(1);
    int any = 0;
    for (int k = 0; k < 16; k++) any |= trig_at(k);
    check(!any, "Delete + a track button: its steps cleared");
    tap(56);

    /* patterns and banks */
    btn(49, 127); step(3, 1); blocks(1); step(3, 0); btn(49, 0); blocks(1);
    check(!strcmp(vstr(view(), "pat", s, sizeof s), "A04"), "Shift + step 4: A04 (stopped: at once)");
    btn(49, 127); tap(63); step(0, 1); blocks(1); step(0, 0); btn(49, 0); blocks(1);
    check(!strcmp(vstr(view(), "pat", s, sizeof s), "B01") && vint(view(), "bank") == 1, "Shift + Right: bank B; + step 1: B01");
    btn(49, 127); tap(62); step(0, 1); blocks(1); step(0, 0); btn(49, 0); blocks(1);

    /* the tracks */
    in("B 42 127;B 42 0"); blocks(1);
    check(vint(view(), "trk") == 1, "track button 2: track 2");
    btn(49, 127); in("B 43 127;B 43 0"); blocks(1);
    check(vint(view(), "trk") == 4, "Shift + track button 1: track 5");
    in("B 40 127;B 40 0"); blocks(1); btn(49, 0); blocks(1);
    check(vint(view(), "trk") == 7, "Shift + track button 4: track 8");
    in("B 43 127;B 43 0"); blocks(1);

    /* mutes */
    btn(88, 127); step(2, 1); blocks(1); step(2, 0); btn(88, 0); blocks(1);
    check(vint(view(), "mutes") == 4, "Mute + step 3: track 3 muted (the track in view's untouched)");
    btn(88, 127); in("B 41 127;B 41 0"); blocks(1); btn(88, 0); blocks(1);
    check(vint(view(), "mutes") == 0, "Mute + track button 3: heard again");
    tap(88);
    check(vint(view(), "mutes") == 1, "Mute tapped: the track in view");
    tap(88);

    /* the bars: A16 (a blank: 16 steps) made 32 long, on PAT (LEN) */
    btn(49, 127); step(15, 1); blocks(1); step(15, 0); btn(49, 0); blocks(1);
    page(MS_PAT);
    const int len0 = kv(2);
    int np1 = vint(view(), "np");
    knob(2, 16); blocks(1);
    check(len0 == 16 && np1 == 1 && vint(view(), "np") == 2, "A16, PAT's LEN 16 to 32: one bar, then two");
    tap(63);
    check(vint(view(), "sp") == 1 && vint(view(), "follow") == 0, "Right: the second bar, not following");
    tap(62);
    check(vint(view(), "sp") == 0, "Left: the first");
    btn(62, 127); btn(63, 127); blocks(1); btn(62, 0); btn(63, 0); blocks(1);
    check(vint(view(), "follow") == 1, "Left and Right together: following again");
    knob(2, len0 - kv(2)); blocks(1);
    btn(49, 127); step(0, 1); blocks(1); step(0, 0); btn(49, 0); blocks(1);     /* (A01 again) */
    /* the tempo under SYNC (Move's, 120, Move stopped): PAT's knob 1 asks Move for it, MONO2K at it at once */
    {
        char tv[32], w[32];
        int tqv[3];
        const int g0 = vint(dump(), "glob");
        in("P 0 15 0"); blocks(1);
        v = view();
        vlist(v, "tq", tqv, 3);
        check(vint(v, "bpm10") == 1215 && tqv[0] == MV_TQ_WAIT && tqv[2] == 1215 && (kf(0) & MV_K_MOVE) &&
              !strcmp(vstr(v, "tval", tv, sizeof tv), "121.5:SYNC") && vint(dump(), "glob") == g0,
              "PAT's knob 1, 15 tenths: MONO2K at 121.5 at once, asked of Move, shown large (SYNC); the pattern's own kept");
        static char fr[4096];
        blocks(6);                                           /* (published) */
        const int fn = A_get("frame", fr, sizeof fr);
        char want[16];
        snprintf(want, sizeof want, "%02X04BF", tqv[1]);
        check(fn == 4 + 8 + 64 * 22 + 2 * (52 + MV_NBTN) + 8 + 6 && !strcmp(fr + fn - 6, want) && fr[fn - 14] == '0' + MV_KK_TEMPO,
              "the frame's tail: knob 1 the tempo, the request's number and 121.5 (04BF) for ui.js to write");
        snprintf(w, sizeof w, "W %d 1", tqv[1]);
        in(w); blocks(800);                                  /* (ui.js wrote it; 2.3 s: still waiting) */
        int still = vint(view(), "bpm10") == 1215;
        blocks(100);
        v = view();
        vlist(v, "tq", tqv, 3);
        check(still && tqv[0] == MV_TQ_NONE && vint(v, "bpm10") == 1200 && strstr(v, " toast=MOVE: LINK OFF|MOVE KEEPS ITS TEMPO") != NULL,
              "Move did not follow in 2.5 s (stopped): Move's 120 again, MOVE: LINK OFF");
        in("P 0 100 1"); blocks(1);
        const int asked = vint(view(), "bpm10");
        host_bpm = 130.0f; blocks(2);                        /* (Link: Move took it) */
        v = view();
        vlist(v, "tq", tqv, 3);
        check(asked == 1300 && tqv[0] == MV_TQ_NONE && vint(v, "bpm10") == 1300, "fast (whole BPM) +10: Move followed, MONO2K follows it at 130");
        host_bpm = 120.0f; blocks(2);
        check(vint(view(), "bpm10") == 1200, "Move's tempo turned on Move: followed");
        in("P 0 5 0"); blocks(1); in("P 0 10 1"); blocks(1);
        check(vint(view(), "bpm10") == 1210, "120.5, then a whole BPM fast: lands on 121.0");
        btn(119, 127); in("P 0 3 0"); blocks(1); btn(119, 0); blocks(1);
        vlist(view(), "tq", tqv, 3);
        check(tqv[0] == MV_TQ_NONE && vint(view(), "bpm10") == 1200, "Delete + the tempo knob: Move's tempo again");
        /* Shift + knob 1 on every page: the tempo, shown large */
        int every_t = 1, b = 1200;
        for (int j = 0; j < 16; j++) {
            const int pg = MS_PAD_PAGE[j];
            page(pg);
            btn(49, 127); in("P 0 1 0"); blocks(1);
            v = view();
            char tk[32], kk5[16];
            vstr(v, "tk", tk, sizeof tk);
            vstr(v, "kk", kk5, sizeof kk5);
            b++;
            if (vint(v, "tfocus") != 1 || strcmp(tk, "0:TEMPO") || vint(v, "bpm10") != b || kk5[0] != '0' + MV_KK_TEMPO) {
                every_t = 0;
                printf("  (Shift + knob 1 on page %d: tfocus %d tk %s bpm10 %d kk %s)\n", pg, vint(v, "tfocus"), tk, vint(v, "bpm10"), kk5);
            }
            btn(49, 0); blocks(1);
            if (pg == MS_CAT || pg == MS_PERF) page(pg);
        }
        check(every_t, "Shift + knob 1 on all 16 page pads (SOUNDS and PERFORM too): the tempo, a tenth a step, shown large");
        page(MS_AMP);
        const int atk = kv(0);
        knob(0, 1); blocks(1);
        check(kv(0) == atk + 1 && vint(view(), "tfocus") == 0, "Shift let go: knob 1 its page's again (AMP's ATCK)");
        knob(0, -1); blocks(1);
        btn(49, 127); in("T 0 1"); blocks(1);
        check(vint(view(), "tfocus") == 1, "Shift + knob 1 touched: the tempo shown large while held");
        in("T 0 0"); btn(49, 0); blocks(500);
        check(vint(view(), "focus") == -1, "let go: gone a moment later");
        page(MS_PAT);
        btn(119, 127); in("P 0 1 0"); blocks(1); btn(119, 0); blocks(1);    /* (Move's tempo again) */
    }
    /* PAT holds the track's sequencing too (the FM-1's SEQ): its own length */
    knob(4, 8); blocks(1);
    check(step_flags(15) & 8, "PAT's TLEN to 8: steps 9-16 past the track's end");
    knob(4, -8); blocks(1);
    page(MS_GLO);
    knob(4, 64); blocks(1);
    check(kv(4) >= 64, "GLO's knob 5: MUTES (a setting) turned");
    knob(4, -127); blocks(1);
    page(MS_HOME);

    /* the keyboard: a two-row piano, the white keys C..C on the bottom row, the black over them */
    int quiet = 0;
    blocks(2000);
    for (int i = 0; i < 4; i++) { A_render(); for (int j = 0; j < 256; j++) if (abs(blk[j]) > quiet) quiet = abs(blk[j]); }
    pad(0, 1, 100);
    int loud = 0;
    for (int i = 0; i < 30; i++) { A_render(); for (int j = 0; j < 256; j++) if (abs(blk[j]) > loud) loud = abs(blk[j]); }
    int pads[32];
    vlist(view(), "pads", pads, 32);
    pad(0, 0, 0); blocks(300);
    int n0 = -1, ee = 0;
    while (ee < 15 && trig_at(ee)) ee++;
    step_tap(ee);
    dump_step(ee, &n0);
    if (quiet > 50 || loud <= 500 || pads[0] != 120) printf("  (quiet %d, loud %d, pad LED %d)\n", quiet, loud, pads[0]);
    check(quiet <= 50 && loud > 500 && pads[0] == 120, "the bottom-left pad plays the track from silence, lit while held");
    check(n0 == 60 + 12 * vint(view(), "oct"), "it is the octave's C; new trigs take it");
    step_tap(ee);
    static const int KEY[16] = {0, 2, 4, 5, 7, 9, 11, 12, 1, 3, -1, 6, 8, 10, -1, -1};
    int piano = 1, last = n0;
    for (int i = 0; i < 16; i++) {                         /* (a pad no key has plays nothing: new trigs keep the last) */
        pad(i, 1, 100); blocks(1);
        int nn = -1;
        step_tap(ee);
        dump_step(ee, &nn);
        const int want = KEY[i] < 0 ? last : n0 + KEY[i];
        if (nn != want) { piano = 0; printf("  (pad %d: note %d, wanted %d)\n", i, nn, want); }
        last = want;
        step_tap(ee);
        pad(i, 0, 0); blocks(1);
    }
    check(piano, "the pads: C D E F G A B C on the bottom row, C# D# - F# G# A# - - over them");
    vlist(view(), "pads", pads, 32);
    check(pads[10] == 0 && pads[14] == 0 && pads[15] == 0, "the pads no key has are dark");
    int roots = 0;
    for (int i = 0; i < 16; i++) roots += pads[i] == 3;
    check(roots >= 1, "the root lit (orange)");
    const int oc0 = vint(view(), "oct");
    tap(55);
    v = view();
    check(vint(v, "oct") == oc0 + 1 && !strncmp(vstr(v, "toast", s, sizeof s), "OCT", 3), "Up: the octave up, said a moment");
    tap(54);
    btn(49, 127); pad(0, 1, 100); blocks(2);
    check(vint(view(), "perf") == 0, "Shift + a pad: no PERFORM any more (a note)");
    pad(0, 0, 0); btn(49, 0); blocks(2);

    /* PERFORM: its pad puts its effects on the keyboard, bank A's on the bottom row, bank B's on the top */
    page(MS_PERF);
    v = view();
    check(vint(v, "pg") == MS_PERF && !strcmp(vstr(v, "pkeys", s, sizeof s), "1,0"), "PERFORM's pad: its page, the keys its effects");
    pad(0, 1, 100); blocks(2);
    check(vint(view(), "perf") == 1 << 0, "the bottom-left key: the first move (RPT 1/4), held");
    pad(0, 0, 0); blocks(2);
    check(vint(view(), "perf") == 0, "let go: off");
    pad(8, 1, 100); blocks(2);
    check(vint(view(), "perf") == 1 << 16, "the row over it: the first colour (LPF)");
    pad(8, 0, 0); blocks(2);
    tap(55);
    pad(0, 1, 100); blocks(2);
    check(vint(view(), "perf") == 1 << 11, "Up: the moves' second page (the bottom-left now FREEZE)");
    pad(0, 0, 0); blocks(2);
    vlist(view(), "pads", pads, 32);
    check(pads[6] == 0 && pads[0] == 115 && pads[8] == 109, "the moves magenta, the colours pink, a key with no effect dark");
    page(MS_FILT);
    check(vint(view(), "pg") == MS_FILT && !strcmp(vstr(view(), "pkeys", s, sizeof s), "1,1"), "a page: PERFORM's keys stay on");
    page(MS_PERF);
    check(vint(view(), "pg") == MS_FILT && !strcmp(vstr(view(), "pkeys", s, sizeof s), "0,1"), "its pad again: the keyboard back");
    page(MS_PERF); page(MS_CAT); page(MS_CAT);
    check(vint(view(), "pg") == MS_PERF, "SOUNDS opened over PERFORM and closed: PERFORM's page again");
    page(MS_PERF);
    check(vint(view(), "pg") == MS_FILT && !strcmp(vstr(view(), "pkeys", s, sizeof s), "0,0"), "PERFORM off: the page before it");
    page(MS_HOME);

    /* SOUNDS: its pad opens the catalogue */
    char d0[2000], d1[2000];
    vstr(dump(), "kit", d0, sizeof d0);
    page(MS_CAT);
    check(vint(view(), "ov") == MV_OV_CAT && vint(view(), "pg") == MS_CAT, "SOUNDS' pad: the sound catalogue");
    jog(1); blocks(1);
    char c1[32];
    vstr(view(), "cat", c1, sizeof c1);
    vstr(dump(), "kit", d1, sizeof d1);
    check(strcmp(d0, d1) != 0, "a turn: the next sound on the track, heard");
    page(MS_CAT);
    vstr(dump(), "kit", d1, sizeof d1);
    check(vint(view(), "ov") == 0 && !strcmp(d0, d1) && vint(view(), "pg") == MS_HOME, "its pad again: the track's own sound back, byte for byte, HOME");
    page(MS_CAT); jog(1); blocks(1); tap(50);
    vstr(dump(), "kit", d1, sizeof d1);
    check(vint(view(), "ov") == 0 && !strcmp(d0, d1), "Menu: the same");
    page(MS_CAT); jog(2); blocks(1); tap(3);
    vstr(dump(), "kit", d1, sizeof d1);
    check(vint(view(), "ov") == 0 && strcmp(d0, d1) != 0, "a click keeps the sound shown");
    tap(56);
    vstr(dump(), "kit", d1, sizeof d1);
    check(!strcmp(d0, d1), "Undo: the track's own again");
    btn(50, 127); blocks(200); btn(50, 0); blocks(1);
    check(vint(view(), "ov") == 0, "Menu held: no catalogue any more (its pad has it)");

    /* Rec, Loop */
    tap(86);
    check(vint(view(), "rec") == 1, "Rec: REC on");
    tap(86);
    check(vint(view(), "rec") == 0, "again: off");
    tap(58);
    check(vint(view(), "hold") == 1, "Loop: the track's arp HOLD");
    tap(58);

    /* the LEDs */
    step_tap(1);
    if (!trig_at(1)) step_tap(1);
    int led[16];
    vlist(view(), "led", led, 16);
    check(led[1] == 16, "a trig's step LED: azure");
    step(1, 1); blocks(1); knob(1, 3); blocks(1); step(1, 0); blocks(400);
    vlist(view(), "led", led, 16);
    check(led[1] == 95, "a locked trig: the dimmer azure");
    int tl[4];
    vlist(view(), "trkled", tl, 4);
    check(tl[0] == 120 && tl[1] == 95, "track buttons: the one in view white, the others dim");

    /* PLAY with no clock from Move: after ~70 ms MONO2K plays on its own clock (INT); again, at once */
    tap(85);
    int early = vint(view(), "play");
    blocks(30);
    v = view();
    check(!early && vint(v, "play") == 1 && vint(v, "sync") == 0, "Play, no answer from Move: MONO2K's own clock (INT)");
    vlist(view(), "led", led, 16);
    int lit = 0;
    for (int k = 0; k < 16; k++) lit += led[k] == 120;
    check(lit >= 1, "playing: the playhead's step LED white");
    /* INT: the tempo knob is the pattern's own, whole BPM (the tenths add up), the header and the value say INT */
    page(MS_PAT);
    {
        char tv[32];
        const int g1 = kv(0);
        in("P 0 25 0"); blocks(1);
        const int g2 = kv(0);
        in("P 0 5 0"); blocks(1);
        v = view();
        check(g2 == g1 + 2 && kv(0) == g1 + 3 && vint(v, "int") == 1 && strstr(vstr(v, "tval", tv, sizeof tv), ":INT"),
              "INT: the tempo knob the pattern's own, whole BPM (25 tenths +2, 5 more +1); INT in the header and the value");
        in("P 0 -30 0"); blocks(1);
    }
    tap(85);
    check(vint(view(), "play") == 0, "Play again (INT): stops at once");
    api->destroy_instance(inst);
    ctl_pads();
    ctl_capture();
    printf("  ctl: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}

/* -------------------------------------------------------------------------------------------- clock */

/* a synthetic Move: pulses at exact times, each seen at the first block boundary at or after it, a late frame (a block
 * more) now and then; the steps the core plays timed against the pulses that made them */
static uint32_t lcg = 12345;
static double rnd01(void) { lcg = lcg * 1664525u + 1013904223u; return (lcg >> 8) / 16777216.0; }

typedef struct {
    double n, mean, m2, most;          /* the steps' offsets from their pulses */
    double raw_most;                   /* the pulses' own lateness, as seen (what feeding them as they came would be) */
    int per_changes;
} cstat_t;

static void cstat_add(cstat_t *c, double d)
{
    c->n++;
    double dl = d - c->mean;
    c->mean += dl / c->n;
    c->m2 += dl * (d - c->mean);
}

static struct { double t0, spt; long next; double tnext; int run; } mv_clk;   /* the synthetic Move */
static long seen_steps;

/* one block of the synthetic Move and the plugin: the pulses due by this block's start delivered (some a block late),
 * the render, the step the core fired (if one) timed against its pulse */
static void clock_block(cstat_t *c, int measure, uint32_t *per_last)
{
    const double bs = (double)nblk * 128.0;
    const int late = rnd01() < 0.01;                  /* (1 % of the frames late: their pulses come with the next) */
    while (!late && mv_clk.run && mv_clk.tnext <= bs) {
        A_rt(0xF8);
        double raw = bs - mv_clk.tnext;
        if (measure && raw > c->raw_most) c->raw_most = raw;
        mv_clk.next++;
        mv_clk.tnext = mv_clk.t0 + mv_clk.next * mv_clk.spt;
    }
    A_render();
    A_get("clock", cbuf, sizeof cbuf);
    int sync, run, nrx, ntx, per, b10, steps, at;
    if (sscanf(cbuf, "%d %d %d %d %d %d %d %d", &sync, &run, &nrx, &ntx, &per, &b10, &steps, &at) == 8) {
        if (per_last && measure && (uint32_t)per != *per_last && *per_last) c->per_changes++;
        if (per_last) *per_last = (uint32_t)per;
        if (steps != seen_steps && steps > 0) {
            long k = steps - 1;          /* step k since the start: pulse 6k, at its time (it may not be seen yet) */
            if (measure && !(k & 1)) {
                double d = at - (mv_clk.t0 + 6.0 * k * mv_clk.spt);
                cstat_add(c, d);
                if (fabs(d) > c->most) c->most = fabs(d);
            }
            seen_steps = steps;
        }
    }
}

static void clk_start(double bpm, int st)
{
    const double bs = (double)nblk * 128.0;
    mv_clk.spt = 44100.0 * 60.0 / (24.0 * bpm);
    mv_clk.t0 = bs - 128.0 * rnd01();           /* the start happened in the block before */
    mv_clk.next = 0;
    mv_clk.tnext = mv_clk.t0;
    mv_clk.run = 1;
    host_bpm = (float)bpm;
    seen_steps = 0;
    A_rt(st);
}

static void clk_tempo(double bpm)                /* from the next pulse on */
{
    const double t = mv_clk.tnext;
    mv_clk.spt = 44100.0 * 60.0 / (24.0 * bpm);
    mv_clk.t0 = t - mv_clk.next * mv_clk.spt;
    host_bpm = (float)bpm;
}

/* the tempo turned in MONO2K: each block of the synthetic Move as clock_block's, every step the core fires recorded
 * (its count and sample), the clock's fields kept (ck[]: clock_string's 24) */
#define TS_MAX 8192
static struct { long n; long k[TS_MAX]; double at[TS_MAX]; int ck[24]; long relock_blocks; } ts;
static void tempo_block(void)
{
    const double bs = (double)nblk * 128.0;
    const int late = rnd01() < 0.01;
    while (!late && mv_clk.run && mv_clk.tnext <= bs) {
        A_rt(0xF8);
        mv_clk.next++;
        mv_clk.tnext = mv_clk.t0 + mv_clk.next * mv_clk.spt;
    }
    A_render();
    A_get("clock", cbuf, sizeof cbuf);
    int *a = ts.ck;
    if (sscanf(cbuf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &a[0], &a[1], &a[2],
               &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10], &a[11], &a[12], &a[13], &a[14], &a[15], &a[16],
               &a[17], &a[18], &a[19], &a[20], &a[21], &a[22], &a[23]) != 24) return;
    if (a[17]) ts.relock_blocks++;
    if (a[6] != seen_steps && a[6] > 0) {
        if (ts.n < TS_MAX) { ts.k[ts.n] = a[6] - 1; ts.at[ts.n] = a[7]; ts.n++; }
        seen_steps = a[6];
    }
}

/* no step glitch: every two steps (no swing between) as long as a tempo between LO and HI makes them, nudged by a
 * re-lock's CK_SLEW (1/16) at most, give or take the 16-sample grid the pulses fall on */
static int pairs_ok(double lo, double hi, const char *what)
{
    const double pmin = 12 * 44100.0 * 60.0 / (24.0 * hi) * (1 - 1.0 / 16 - 0.01) - 40;
    const double pmax = 12 * 44100.0 * 60.0 / (24.0 * lo) * (1 + 1.0 / 16 + 0.01) + 40;
    double most = 0, least = 1e9;
    int ok = 1;
    for (long i = 0; i + 1 < ts.n; i++) {
        if (ts.k[i] & 1) continue;
        long j = i + 1;
        while (j < ts.n && ts.k[j] != ts.k[i] + 2) j++;
        if (j >= ts.n) continue;
        const double p = ts.at[j] - ts.at[i];
        if (p > most) most = p;
        if (p < least) least = p;
        if (p < pmin || p > pmax) ok = 0;
        if (ts.k[j] - ts.k[i] != 2) ok = 0;
    }
    printf("  %-44s %ld steps, two steps %.0f..%.0f samples (allowed %.0f..%.0f)\n", what, ts.n, least, most, pmin, pmax);
    return ok && ts.n > 4;
}

/* the steps since FROM against Move's pulses (step k on pulse 6k - OFF: MONO2K's count runs OFF ahead): most off */
static double steps_off(long from, int off)
{
    double most = 0;
    for (long i = from; i < ts.n; i++) {
        if (ts.k[i] & 1) continue;
        const double d = fabs(ts.at[i] - (mv_clk.t0 + (6.0 * ts.k[i] - off) * mv_clk.spt));
        if (d > most) most = d;
    }
    return most;
}

static void cstat_print(const char *what, const cstat_t *c)
{
    const double sd = c->n > 1 ? sqrt(c->m2 / (c->n - 1)) : 0;
    printf("  %-44s %4.0f steps: offset %+6.1f, sd %5.1f, most %5.1f samples (%.2f ms); pulses seen up to %.0f late "
           "(%.2f ms)\n", what, c->n, c->mean, sd, c->most, c->most / 44.1, c->raw_most, c->raw_most / 44.1);
}

static int do_clock(int lag)
{
    char js[256];
    if (lag >= 0) snprintf(js, sizeof js, "{\"defaults\": {\"seed\": 2000, \"pattern\": 0, \"data_dir\": \"\", \"clock_lag\": %d}}", lag);
    else snprintf(js, sizeof js, "{\"defaults\": {\"seed\": 2000, \"pattern\": 0, \"data_dir\": \"\"}}");
    inst = open_js(js);
    host.get_bpm = get_bpm;
    blocks(8);
    const char *v;

    /* 1: 120 BPM, a minute, the frames a block late now and then */
    cstat_t c1 = {0};
    uint32_t per = 0;
    clk_start(120.0, 0xFA);
    for (int i = 0; i < 400; i++) clock_block(&c1, 0, &per);          /* (locking in: ~1.2 s) */
    for (int i = 0; i < 60 * 344; i++) clock_block(&c1, 1, &per);
    cstat_print("120 BPM, 60 s", &c1);
    v = view();
    check(vint(v, "fol") == 1 && vint(v, "sync") == 1 && vint(v, "bpm10") == 1200, "following Move at 120.0 BPM, SYNC");
    check(c1.per_changes == 0 && per == 235200, "the core's tempo steady: one period all along (235200, Q8 samples a pulse)");
    check(c1.most <= 32 && fabs(c1.mean) <= 8, "each step within 32 samples (0.73 ms) of its pulse, 8 on average");
    check(c1.raw_most > 128, "(the pulses as seen: more than a block late at worst)");

    /* 2: the tempo turned on Move: 137.5 */
    cstat_t c2 = {0};
    clk_tempo(137.5);
    for (int i = 0; i < 600; i++) clock_block(&c2, 0, &per);
    uint32_t p0 = per;
    for (int i = 0; i < 30 * 344; i++) clock_block(&c2, 1, &per);
    cstat_print("120 -> 137.5 BPM, 30 s after", &c2);
    check(p0 == (uint32_t)(28224000.0 / 137.5 + 0.5) && c2.per_changes == 0 && vint(view(), "bpm10") == 1375,
          "the tempo turned: the core's period follows once, then holds");
    check(c2.most <= 32, "and the steps stay within 32 samples");

    /* 3: stop, continue, start again */
    mv_clk.run = 0;
    A_rt(0xFC);
    blocks(2);
    A_get("ui", cbuf, sizeof cbuf);
    int stopped_at = -1, pl = -1;
    sscanf(cbuf, "%d %d", &pl, &stopped_at);
    check(pl == 0, "Move's stop: MONO2K stops");
    int played = (int)seen_steps;
    blocks(100);
    cstat_t c3 = {0};
    clk_start(137.5, 0xFB);
    for (int i = 0; i < 344; i++) clock_block(&c3, 0, &per);
    A_get("ui", cbuf, sizeof cbuf);
    sscanf(cbuf, "%d", &pl);
    check(pl == 1 && seen_steps > 0, "continue: playing again, on from where it stopped");
    (void)played;
    clk_start(100.0, 0xFA);
    cstat_t c4 = {0};
    for (int i = 0; i < 300; i++) clock_block(&c4, 0, &per);
    for (int i = 0; i < 20 * 344; i++) clock_block(&c4, 1, &per);
    cstat_print("start again at 100 BPM, 20 s", &c4);
    check(c4.most <= 32 && per == 282240, "start: from the first step, locked at 100 BPM");

    /* 4: a stream that stops without a stop: MONO2K stops half a second on */
    mv_clk.run = 0;
    blocks(250);
    A_get("clock", cbuf, sizeof cbuf);
    int sy, ru;
    sscanf(cbuf, "%d %d", &sy, &ru);
    check(!ru && !vint(view(), "play"), "the pulses gone without a stop: stopped after half a second");

    /* 5: no tempo from the host: the pulses measured (4 beats) and held */
    host.get_bpm = NULL;
    cstat_t c5 = {0};
    per = 0;
    clk_start(128.0, 0xFA);
    for (int i = 0; i < 4 * 344; i++) clock_block(&c5, 0, &per);
    for (int i = 0; i < 30 * 344; i++) clock_block(&c5, 1, &per);
    cstat_print("128 BPM, no host tempo (measured), 30 s", &c5);
    const int b10 = vint(view(), "bpm10");
    check(c5.per_changes == 0 && b10 >= 1278 && b10 <= 1282, "no host tempo: measured to 0.2 BPM, and held");
    check(c5.most <= 64, "and the steps within 64 samples (1.5 ms)");
    host.get_bpm = get_bpm;
    mv_clk.run = 0;
    A_rt(0xFC);
    blocks(10);

    /* 6: PLAY answered by Move: its start before or after the press is seen, no second start or stop */
    tap(85);
    blocks(2);
    clk_start(120.0, 0xFA);
    cstat_t c6 = {0};
    for (int i = 0; i < 60; i++) clock_block(&c6, 0, &per);
    v = view();
    check(vint(v, "play") == 1 && vint(v, "fol") == 1, "Play, then Move's start: following, nothing doubled");
    mv_clk.run = 0;
    A_rt(0xFC);
    blocks(3);
    tap(85);
    blocks(40);
    check(vint(view(), "play") == 0, "Move's stop, then the press seen: stopped, nothing doubled");
    api->destroy_instance(inst);

    /* 7: no clock at all: PLAY plays MONO2K alone (INT, its own tempo), then Move's start takes over */
    inst = open_js(js);
    blocks(4);
    tap(85);
    blocks(30);
    v = view();
    char gl[600];
    vstr(dump(), "glob", gl, sizeof gl);
    check(vint(v, "play") == 1 && vint(v, "sync") == 0 && vint(v, "bpm10") == 10 * (30 + atoi(gl)),
          "no clock: INT at the pattern's own tempo");
    cstat_t c7 = {0};
    per = 0;
    clk_start(90.0, 0xFA);
    for (int i = 0; i < 200; i++) clock_block(&c7, 0, &per);
    v = view();
    check(vint(v, "sync") == 1 && vint(v, "fol") == 1 && vint(v, "bpm10") == 900, "then Move starts: SYNC, following at 90");
    api->destroy_instance(inst);

    /* 8: the tempo turned in MONO2K while it follows Move at 120: 130 at once; ui.js writes it 250 ms on, Move (Link)
     * takes it 300 ms later; MONO2K follows Move again, back on its phase without a jump */
    char w[32];
    inst = open_js(js);
    host.get_bpm = get_bpm;
    blocks(8);
    clk_start(120.0, 0xFA);
    for (int i = 0; i < 600; i++) tempo_block();
    ts.n = 0; ts.relock_blocks = 0;
    for (int i = 0; i < 344; i++) tempo_block();
    in("B 49 127;P 0 100 1;B 49 0");
    tempo_block();
    const int per_now = ts.ck[4], seq = ts.ck[19], state_now = ts.ck[15];
    for (int i = 0; i < 86; i++) tempo_block();
    snprintf(w, sizeof w, "W %d 1", seq);
    in(w);
    for (int i = 0; i < 104; i++) tempo_block();
    const int waited = ts.ck[15] == MV_TQ_WAIT && ts.ck[16] == 1;
    if (!waited) printf("  (meanwhile: %s)\n", cbuf);
    clk_tempo(130.0);
    for (int i = 0; i < 12 * 344; i++) tempo_block();
    const int *a = ts.ck;
    check(per_now == (int)(28224000.0 / 130 + 0.5) && state_now == MV_TQ_WAIT,
          "turned to 130: the core's period at once (217108), Move asked");
    check(waited, "meanwhile MONO2K free at 130, waiting for Move");
    check(a[15] == MV_TQ_NONE && a[22] == 1 && a[16] == 0 && a[17] == 0 && a[18] == 0 && a[4] == 217108,
          "Move followed: MONO2K follows it again, re-locked in its own bar");
    if (a[22] != 1 || a[18]) printf("  (after: %s)\n", cbuf);
    printf("  re-lock took %.2f s\n", ts.relock_blocks * 128 / 44100.0);
    check(ts.relock_blocks * 128 < 4 * 44100, "the re-lock within 4 s");
    check(pairs_ok(120, 130, "120 -> 130 in MONO2K, Move following"), "no step glitch: every two steps between 120's and 130's, nudged 6 % at most");
    long from = 0;
    while (from < ts.n && ts.at[from] < (double)(nblk - 4 * 344) * 128) from++;
    const double off8 = steps_off(from, 0);
    printf("  the last 4 s: steps within %.1f samples of Move's pulses\n", off8);
    check(off8 <= 32, "then each step within 32 samples (0.73 ms) of Move's pulse, as before");

    /* 9: Move does not follow (no Link): 2.5 s after the write MONO2K plays on alone at 125 (INT, a hint); then
     * Move's tempo changes on Move (138): MONO2K follows it again, into Move's bar without a jump */
    ts.n = 0; ts.relock_blocks = 0;
    in("B 49 127;P 0 -50 1;B 49 0");
    tempo_block();
    snprintf(w, sizeof w, "W %d 1", ts.ck[19]);
    for (int i = 0; i < 86; i++) tempo_block();
    in(w);
    for (int i = 0; i < 850; i++) tempo_block();
    const int before = ts.ck[15];
    for (int i = 0; i < 30; i++) tempo_block();
    v = view();
    check(before == MV_TQ_WAIT && ts.ck[15] == MV_TQ_ALONE && vint(v, "sync") == 0 && vint(v, "int") == 1 &&
          vint(v, "bpm10") == 1250 && strstr(v, " toast=INT|MOVE: LINK OFF") != NULL,
          "not followed in 2.5 s: MONO2K on alone at 125, INT, MOVE: LINK OFF");
    for (int i = 0; i < 5 * 344; i++) tempo_block();
    check(pairs_ok(125, 125, "alone at 125 (Move at 130)"), "alone: its own 125, steadily");
    ts.n = 0;
    clk_tempo(138.0);
    for (int i = 0; i < 25 * 344; i++) tempo_block();
    a = ts.ck;
    printf("  re-lock took %.2f s, into the bar %d pulses on\n", ts.relock_blocks * 128 / 44100.0, a[18]);
    check(a[15] == MV_TQ_NONE && a[16] == 0 && a[17] == 0 && a[18] % 96 == 0 && vint(view(), "sync") == 1,
          "Move's tempo changed: MONO2K follows Move again (SYNC), in Move's bar");
    check(pairs_ok(125, 138, "re-synced from 125 alone to Move's 138"), "no step glitch while it gets there");
    from = 0;
    while (from < ts.n && ts.at[from] < (double)(nblk - 4 * 344) * 128) from++;
    const double off9 = steps_off(from, a[18]);
    printf("  the last 4 s: steps within %.1f samples of Move's pulses\n", off9);
    check(off9 <= 32, "then each step within 32 samples of Move's pulse");

    /* 10: alone again, then Move stops and starts at the tempo it had: MONO2K keeps its own (the owner's: a tempo set is
     * not lost at the next PLAY), started by Move's start; Delete + the tempo knob re-syncs */
    in("B 49 127;P 0 20 1;B 49 0");
    for (int i = 0; i < 1500; i++) tempo_block();
    const int alone10 = ts.ck[15] == MV_TQ_ALONE;
    mv_clk.run = 0;
    A_rt(0xFC);
    blocks(20);
    clk_start(138.0, 0xFA);
    for (int i = 0; i < 400; i++) tempo_block();
    check(alone10 && ts.ck[15] == MV_TQ_ALONE && ts.ck[4] != (int)(28224000.0 / 138 + 0.5) && vint(view(), "sync") == 0,
          "alone, then Move stopped and started: still alone at its own tempo (INT)");
    in("B 49 127;P 0 30 1;B 49 0");
    for (int i = 0; i < 200; i++) tempo_block();
    in("B 119 127;B 49 127;P 0 1 0;B 49 0;B 119 0");
    for (int i = 0; i < 6 * 344; i++) tempo_block();
    check(ts.ck[15] == MV_TQ_NONE && ts.ck[17] == 0 && ts.ck[4] == (int)(28224000.0 / 138 + 0.5),
          "asked, then Delete + the tempo knob: Move's tempo again, re-locked");
    api->destroy_instance(inst);
    printf("  clock: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}

/* --------------------------------------------------------------------------------------------- save */

static long file_read(const char *p, uint8_t *b, long max)
{
    FILE *f = real_fopen(p, "rb");
    if (!f) return -1;
    long n = (long)real_fread(b, 1, (size_t)max, f);
    real_fclose(f);
    return n;
}

static int wait_saves(int want)
{
    for (int i = 0; i < 400; i++) {
        A_render();
        A_get("clock", cbuf, sizeof cbuf);
        int a[15] = {0};
        sscanf(cbuf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6],
               &a[7], &a[8], &a[9], &a[10], &a[11], &a[12], &a[13], &a[14]);
        if (a[12] >= want && a[14] == 0) return 1;
        nap(5);
    }
    return 0;
}

static int do_save(const char *d)
{
    static uint8_t f1[2 << 20], f2[2 << 20];
    char js[512], path[600], bad[600], tmp[600], s[32];
    snprintf(path, sizeof path, "%s/mono2k.m2ks", d);
    snprintf(bad, sizeof bad, "%s/mono2k.m2ks.bad", d);
    snprintf(tmp, sizeof tmp, "%s/mono2k.m2ks.tmp", d);
    real_unlink(path); real_unlink(bad); real_unlink(tmp);
    snprintf(js, sizeof js, "{\"defaults\": {\"seed\": 2000, \"data_dir\": \"%s\"}}", d);
    inst = open_js(js);
    blocks(4);
    check(!strcmp(vstr(view(), "pat", s, sizeof s), "A01"), "no file: the factory, A01");
    /* edits: a pattern's steps and a lock, a sound, a mute, another pattern open */
    btn(49, 127); step(2, 1); blocks(1); step(2, 0); btn(49, 0); blocks(1);     /* A03 */
    for (int k = 0; k < 16; k += 3) step_tap(k);
    step(4, 1); blocks(1); knob(3, 7); blocks(1); step(4, 0); blocks(1);
    in("B 42 127;B 42 0"); blocks(1);
    jog(1); knob(0, -9); knob(5, 4); blocks(1); jog(-1);
    tap(88);
    char d1[8][4096];
    for (int t = 0; t < 8; t++) { char b[16]; snprintf(b, sizeof b, "%d", t); A_set("track", b); blocks(1); A_get("dump", d1[t], sizeof d1[t]); }
    A_set("track", "1"); blocks(1);
    n_file = 0;
    btn(49, 127); tap(86); btn(49, 0);                /* Shift + Rec: save */
    int saved = wait_saves(1);
    check(saved, "Shift + Rec: the saver thread wrote the store");
    check(n_file == 0, "no file call on the audio thread meanwhile");
    long n1 = file_read(path, f1, sizeof f1);
    struct stat stt;
    check(n1 > 10 && !memcmp(f1, "M2K2\x05", 5) && stat(tmp, &stt) != 0, "mono2k.m2ks: a store (M2K2 5), no temp file left");
    check(n1 > 12 && f1[10 + 2] == 0, "its SYNC IN the store's own (OFF), not the Move's clock in");
    printf("  the store: %ld bytes\n", n1);
    api->destroy_instance(inst);                      /* (closing saves too: the core's own pack) */
    long n2 = file_read(path, f2, sizeof f2);
    check(n2 == n1 && !memcmp(f1, f2, (size_t)n1), "the saver's file is the core's own pack, byte for byte");

    inst = open_js(js);                               /* the store read back */
    blocks(2);
    int same = !strcmp(vstr(view(), "pat", s, sizeof s), "A03");
    for (int t = 0; t < 8; t++) {
        char b[16], d2[4096];
        snprintf(b, sizeof b, "%d", t);
        A_set("track", b);
        blocks(1);
        A_get("dump", d2, sizeof d2);
        if (strcmp(d1[t], d2)) { same = 0; printf("  track %d differs\n", t + 1); }
    }
    check(same, "opened again: A03, every track's sound, steps, locks and the mute as saved");
    api->destroy_instance(inst);

    /* closing without saving (save_on_close 0) leaves the file as it was */
    snprintf(js, sizeof js, "{\"defaults\": {\"seed\": 2000, \"data_dir\": \"%s\", \"save_on_close\": 0}}", d);
    inst = open_js(js);
    blocks(2);
    for (int k = 0; k < 16; k++) step_tap(k);
    api->destroy_instance(inst);
    n2 = file_read(path, f2, sizeof f2);
    check(n2 == n1 && !memcmp(f1, f2, (size_t)n1), "save_on_close 0: closing writes nothing");

    /* a broken file: kept aside, the factory plays */
    FILE *f = real_fopen(path, "wb");
    real_fwrite("M2K2\x05junk", 1, 9, f);
    real_fclose(f);
    inst = open_js(js);
    blocks(2);
    check(!strcmp(vstr(view(), "pat", s, sizeof s), "A01") && stat(bad, &stt) == 0, "a broken file: kept as .bad, the factory");
    api->destroy_instance(inst);
    printf("  save: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}

/* --------------------------------------------------------------------------------------------- draw */

static void frame_pbm(const char *path)
{
    static char fr[4096];
    int n = A_get("frame", fr, sizeof fr);
    FILE *f = real_fopen(path, "wb");
    if (!f || n < 12 + 64 * 22) { if (f) real_fclose(f); return; }
    fprintf(f, "P4\n128 64\n");
    for (int y = 0; y < 64; y++) {
        uint8_t row[16] = {0};
        for (int x = 0; x < 128; x++) {
            const int c = fr[12 + y * 22 + x / 6] - '0';
            if ((c >> (5 - x % 6)) & 1) row[x >> 3] |= (uint8_t)(0x80 >> (x & 7));
        }
        real_fwrite(row, 16, 1, f);
    }
    real_fclose(f);
}

static void shot(const char *d, const char *name) { char p[600]; nap(40); A_render(); nap(40); snprintf(p, sizeof p, "%s/%s.pbm", d, name); frame_pbm(p); }

static int do_draw(const char *d)
{
    inst = open_at(0, 2000);
    blocks(4);
    shot(d, "1_home");
    static char fr[4096];
    int n = A_get("frame", fr, sizeof fr);
    int lit = 0;
    for (int i = 12; i < 12 + 7 * 22; i++) lit += fr[i] != '0';            /* (the header: rows 0..6) */
    check(n == 4 + 8 + 64 * 22 + 2 * (52 + MV_NBTN) + 14 && !strncmp(fr, "M2K1", 4) && lit > 10,
          "a frame: M2K1, 64 rows, the LEDs, the knobs' kinds and the tempo asked; the header lit");
    in("T 3 1"); blocks(8); shot(d, "2_focus"); in("T 3 0");
    blocks(500);
    page(MS_AMP); blocks(8); shot(d, "3_amp");
    jog(5); blocks(8); shot(d, "4_patterns"); tap(50);
    page(MS_CAT); jog(3); blocks(8); shot(d, "5_sounds"); page(MS_CAT);
    btn(49, 127); blocks(8); shot(d, "6_shift"); btn(49, 0);
    btn(88, 127); blocks(8); shot(d, "7_mute"); btn(88, 0);
    blocks(800);
    api->set_param(inst, "play", "1");
    page(MS_FILT);
    step(4, 1); blocks(80); shot(d, "8_held_playing"); step(4, 0);
    page(MS_PAT); blocks(600); shot(d, "9_pat");
    page(MS_MIX); blocks(60); shot(d, "10_mix");
    page(MS_PERF); pad(1, 1, 100); blocks(60); shot(d, "11_perform"); pad(1, 0, 0); page(MS_PERF);
    page(MS_AMP); btn(49, 127); in("P 0 5 0"); blocks(8); shot(d, "12_tempo_shift"); btn(49, 0);
    api->destroy_instance(inst);
    return fails != 0;
}

/* ---------------------------------------------------------------------------------------------- rt */

static int do_rt(void)
{
    char raw[256];
    inst = open_at(0, 2000);
    A_get("ui", raw, sizeof raw);
    printf("  get_param(\"ui\") at power-on: \"%s\"\n", raw);
    check(api->create_instance(dir, NULL) == NULL, "a second instance is refused (the core is one)");
    unsigned long r0 = rules();
    /* a step, an external note on its number, a pad, the track, PLAY (as in step 1) */
    int k = 0;
    while (k < 16 && trig_at(k)) k++;
    step_tap(k);
    int on = trig_at(k);
    step_tap(k);
    int off = !trig_at(k);
    uint8_t m[3] = {0x90, (uint8_t)(16 + k), 100};
    A_midi(m, 3, MOVE_MIDI_SOURCE_EXTERNAL);
    blocks(1);
    int ext = !trig_at(k);
    A_set("track", "3");
    A_set("track", "9");
    blocks(1);
    int t3 = vint(view(), "trk") == 3;
    A_set("track", "0");
    A_set("play", "1");
    blocks(40);
    int pl = vint(view(), "play") == 1;
    char small[8];
    int sn = A_get("ui", small, sizeof small);
    int unknown = A_get("rui_poll", raw, sizeof raw) == -1 && A_get("state", raw, sizeof raw) == -1;
    A_set("play", "0");

    /* a long run: every control as fast as hands and ui.js send them, Move's clock, saves asked for, reads */
    unsigned long nb = 0, calls = 0;
    host.get_bpm = get_bpm;
    for (int i = 0; i < 60 * 44100 / 128; i++) {
        if (i % 7 == 0) { pad((i / 7) % 32, 1, 30 + i % 97); calls++; }
        if (i % 7 == 3) { pad((i / 7) % 32, 0, 0); calls++; }
        if (i % 53 == 0) { step((i / 53) % 16, 1); calls++; }
        if (i % 53 == 5) { step((i / 53) % 16, 0); calls++; }
        if (i % 11 == 0) { char s[32]; snprintf(s, sizeof s, "K %d %d", i % 8, (i % 5) - 2); in(s); calls++; }
        if (i % 97 == 0) { jog(i % 3 - 1); calls++; }
        if (i % 401 == 0) { in((i / 401) % 2 ? "B 42 127;B 42 0" : "B 43 127;B 43 0"); calls++; }
        if (i % 613 == 0 || i % 613 == 9) {   /* (Sample: PADS on and off; Capture: what the pads played, written) */
            static const int cc[] = {88, 60, 119, 56, 62, 63, 55, 54, 58, 86, 3, 50, 118, 52};
            btn(cc[(i / 613) % (int)(sizeof cc / sizeof cc[0])], i % 613 ? 0 : 127);
            calls++;
        }
        if (i % 1777 == 0) { btn(49, 127); btn(86, 127); btn(86, 0); btn(49, 0); calls++; }   /* (a save asked: no dir, nothing) */
        if (i % 12 == 0) { A_get("frame", vbuf, sizeof vbuf); calls++; }
        if (i % 1000 == 0) A_rt(0xFA);
        if (i % 1000 == 700) A_rt(0xFC);
        if (i % 1000 < 700 && i % 7 == 0) A_rt(0xF8);
        if (i % 3001 == 0) { in("T 1 1"); in("T 1 0"); calls++; }
        if (i % 211 == 0) {            /* the tempo: Shift + knob 1, PAT's, ui.js's writes, a re-sync asked */
            char s[48];
            snprintf(s, sizeof s, "B 49 127;P 0 %d %d;B 49 0", (i / 211) % 2 ? 7 : -7, (i / 211) % 3 == 0);
            in(s);
            snprintf(s, sizeof s, "W %d %d", (i / 211) % 256, (i / 211) % 5 != 0);
            in(s);
            if (i % 2110 == 0) in("B 119 127;B 49 127;P 0 1 0;B 49 0;B 119 0");
            calls += 3;
        }
        if (i % 977 == 0) host_bpm = (float)(100 + (i / 977) % 40);   /* (Move's tempo moving: followed, re-synced) */
        A_render();
        nb++;
    }
    for (int i = 0; i < 1000; i++) pad(i % 32, 1, 100);
    for (int i = 0; i < 32; i++) pad(i, 0, 0);
    A_render();
    unsigned long r1 = rules();
    check(on, "a step key places a trig on the track in view");
    check(off, "tapped again, it takes it away");
    check(ext, "an external note on a step number changes no trig");
    check(t3, "set_param track 3 (and 9 ignored)");
    check(pl, "set_param play 1: playing");
    check(sn == 7 && small[7] == 0, "get_param truncates to the buffer it is given");
    check(unknown, "unknown keys (rui_poll, state) answer -1");
    printf("  %lu blocks and %lu presses, turns, buttons, reads, Move's clock and saves asked, then 1000 presses at once:\n"
           "    %lu allocations, %lu file calls, %lu thread calls, %lu sleeps, %lu logs on the audio thread\n",
           nb, calls, n_alloc, n_file, n_thread, n_sleep, n_log);
    check(r1 == r0, "nothing allocated, written, read, slept, threaded or logged on the audio thread");
    api->destroy_instance(inst);
    void *again = open_at(0, -1);
    check(again != NULL, "after destroy_instance a new instance opens");
    inst = again;
    api->destroy_instance(again);
    return fails != 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "bench DSP.so hash|time [SEC]|rt|ctl|clock|save DIR|draw DIR\n"); return 2; }
    real_init();
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    move_plugin_init_v2_fn init = (move_plugin_init_v2_fn)dlsym(h, MOVE_PLUGIN_INIT_V2_SYMBOL);
    if (!init) { fprintf(stderr, "no %s\n", MOVE_PLUGIN_INIT_V2_SYMBOL); return 1; }
    host.api_version = MOVE_PLUGIN_API_VERSION;
    host.sample_rate = MOVE_SAMPLE_RATE;
    host.frames_per_block = MOVE_FRAMES_PER_BLOCK;
    host.log = host_log;
    host.get_bpm = get_bpm;
    api = init(&host);
    if (!api || api->api_version != MOVE_PLUGIN_API_VERSION_2) { fprintf(stderr, "not a v2 plugin\n"); return 1; }
    snprintf(dir, sizeof dir, "%s", argv[1]);
    char *sl = strrchr(dir, '/');
    if (sl) *sl = 0;
    if (!strcmp(argv[2], "hash")) return do_hash();
    if (!strcmp(argv[2], "time")) return do_time(argc > 3 ? atof(argv[3]) : 20.0);
    if (!strcmp(argv[2], "rt")) return do_rt();
    if (!strcmp(argv[2], "ctl")) return do_ctl();
    if (!strcmp(argv[2], "clock")) return do_clock(argc > 3 ? atoi(argv[3]) : -1);
    if (!strcmp(argv[2], "save") && argc > 3) return do_save(argv[3]);
    if (!strcmp(argv[2], "draw") && argc > 3) return do_draw(argv[3]);
    fprintf(stderr, "bench DSP.so hash|time [SEC]|rt|ctl|clock|save DIR|draw DIR\n");
    return 2;
}
