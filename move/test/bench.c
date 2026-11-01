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
    knob(0, 7); blocks(1);
    check((dump_kit(0) * 4) >> 7 == ((w0 * 4) >> 7) + 1, "SYN's knob 1 on a choice (the kick's WAVE): one choice a turn");
    knob(0, -7); blocks(1);
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
    /* the tempo knob while Move's clock is the source */
    int g0 = kv(0);
    knob(0, 5); blocks(1);
    check(kv(0) != g0 + 5 && (kf(0) & 16), "the tempo knob shows Move's tempo and Move keeps it");
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
    check(vint(view(), "perf") == 1, "the bottom-left key: bank A's first, held");
    pad(0, 0, 0); blocks(2);
    check(vint(view(), "perf") == 0, "let go: off");
    pad(8, 1, 100); blocks(2);
    check(vint(view(), "perf") == 1 << 11, "the row over it: bank B's first");
    pad(8, 0, 0); blocks(2);
    tap(55);
    pad(0, 1, 100); blocks(2);
    check(vint(view(), "perf") == 1 << 8, "Up: each bank's 9-11 (the bottom-left now bank A's ninth)");
    pad(0, 0, 0); blocks(2);
    vlist(view(), "pads", pads, 32);
    check(pads[3] == 0 && pads[0] == 115 && pads[8] == 109, "their keys lit in the functions' colours, the rest dark");
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
    tap(85);
    check(vint(view(), "play") == 0, "Play again (INT): stops at once");
    api->destroy_instance(inst);
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
    check(n == 4 + 8 + 64 * 22 + 2 * 63 && !strncmp(fr, "M2K1", 4) && lit > 10, "a frame: M2K1, 64 rows, the LEDs; the header lit");
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
        if (i % 613 == 0) { static const int cc[] = {88, 60, 119, 56, 62, 63, 55, 54, 58, 86, 3, 50};
                            int c = cc[(i / 613) % 12]; btn(c, 127); calls++; }
        if (i % 613 == 9) { static const int cc[] = {88, 60, 119, 56, 62, 63, 55, 54, 58, 86, 3, 50};
                            int c = cc[(i / 613) % 12]; btn(c, 0); calls++; }
        if (i % 1777 == 0) { btn(49, 127); btn(86, 127); btn(86, 0); btn(49, 0); calls++; }   /* (a save asked: no dir, nothing) */
        if (i % 12 == 0) { A_get("frame", vbuf, sizeof vbuf); calls++; }
        if (i % 1000 == 0) A_rt(0xFA);
        if (i % 1000 == 700) A_rt(0xFC);
        if (i % 1000 < 700 && i % 7 == 0) A_rt(0xF8);
        if (i % 3001 == 0) { in("T 1 1"); in("T 1 0"); calls++; }
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
