/* MONO2K's dsp.so driven as Schwung drives it: dlopen, move_plugin_init_v2, create_instance, then on_midi, set_param,
 * get_param and render_block in 128-frame blocks. Built and run in the Move's container by move/proof.sh:
 *   bench DSP.so hash         bank A's A01..A04 through the plugin, FNV-1a over 10 s as host/m2k_draw.c --hash prints
 *                             them (seed 2000, from the start, one core)
 *   bench DSP.so time [SEC]   each block's render timed, A01..A04 playing
 *   bench DSP.so rt           the presses, the params and the reads with the render, every allocation and log counted,
 *                             and what they do checked through get_param("ui") */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include "../vendor/schwung/plugin_api_v1.h"

/* the allocator, counted: this program's malloc family over glibc's, so a dlopened dsp.so binds to it (-rdynamic) */
extern void *__libc_malloc(size_t);
extern void *__libc_calloc(size_t, size_t);
extern void *__libc_realloc(void *, size_t);
extern void *__libc_memalign(size_t, size_t);
extern void __libc_free(void *);
static volatile int counting;
static volatile unsigned long n_alloc, n_log;
void *malloc(size_t n) { if (counting) n_alloc++; return __libc_malloc(n); }
void *calloc(size_t a, size_t b) { if (counting) n_alloc++; return __libc_calloc(a, b); }
void *realloc(void *p, size_t n) { if (counting) n_alloc++; return __libc_realloc(p, n); }
void free(void *p) { if (counting && p) n_alloc++; __libc_free(p); }
void *memalign(size_t a, size_t n) { if (counting) n_alloc++; return __libc_memalign(a, n); }
void *aligned_alloc(size_t a, size_t n) { if (counting) n_alloc++; return __libc_memalign(a, n); }
int posix_memalign(void **p, size_t a, size_t n) { if (counting) n_alloc++; *p = __libc_memalign(a, n); return *p ? 0 : 12; }

static void host_log(const char *m) { n_log++; if (!counting) fprintf(stderr, "  [log] %s\n", m); }

static plugin_api_v2_t *api;
static char dir[512];

static void *open_at(int pattern, int seed)
{
    char js[128];
    if (seed >= 0) snprintf(js, sizeof js, "{\"defaults\": {\"seed\": %d, \"pattern\": %d}}", seed, pattern);
    else snprintf(js, sizeof js, "{\"defaults\": {\"pattern\": %d}}", pattern);
    void *in = api->create_instance(dir, js);
    if (!in) { fprintf(stderr, "create_instance failed\n"); exit(1); }
    return in;
}

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

static const char *const NAME[4] = {"A01", "A02", "A03", "A04"};

static int do_hash(void)
{
    static int16_t b[2 * 11 * 44100];
    const int len = 10 * 44100, n = (len + 127) / 128 * 128;
    for (int p = 0; p < 4; p++) {
        double t0 = now_us();
        void *in = open_at(p, 2000);
        double tc = now_us() - t0;
        api->set_param(in, "play", "1");
        for (int f = 0; f < n; f += 128) api->render_block(in, b + 2 * f, 128);
        printf("  %-6s %08x   (create_instance %.1f ms)\n", NAME[p], fnv_pcm(2166136261u, b, len), tc / 1000);
        api->destroy_instance(in);
    }
    return 0;
}

/* the instructions a block where the kernel lets a guest count them (a VM often does not: then "n/a") */
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
    static int16_t b[256];
    const int nb = (int)(sec * 44100 / 128);
    double *us = malloc(sizeof(double) * (size_t)nb);
    perf_open();
    printf("  one 128-frame block (2902 us of sound), %.0f s from 2 s in, one core:\n", sec);
    printf("  %-5s %8s %8s %8s %8s %7s %14s\n", "", "mean us", "p50", "p99", "most", "mean %", "instr/sample");
    for (int p = 0; p < 4; p++) {
        void *in = open_at(p, 2000);
        api->set_param(in, "play", "1");
        for (int f = 0; f < 2 * 44100; f += 128) api->render_block(in, b, 128);
        double sum = 0;
        uint64_t i0 = perf_read();
        for (int k = 0; k < nb; k++) {
            double t0 = now_us();
            api->render_block(in, b, 128);
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
        api->destroy_instance(in);
    }
    free(us);
    return 0;
}

/* get_param("ui")'s fields (m2k_move.c ui_string) */
typedef struct { int play, step, bpm, trk, ntrk, page, len, trigs, cmean, cmost; char slot[8], name[16]; } ui_t;
static int ui_read(void *in, ui_t *u)
{
    char buf[128];
    int n = api->get_param(in, "ui", buf, sizeof buf);
    if (n <= 0) return 0;
    memset(u, 0, sizeof *u);
    int got = sscanf(buf, "%d %d %d %d %d %d %d %x %d %d %7s %15[^\n]", &u->play, &u->step, &u->bpm, &u->trk, &u->ntrk,
                     &u->page, &u->len, (unsigned *)&u->trigs, &u->cmean, &u->cmost, u->slot, u->name);
    return got >= 11;
}

static int fails;
static void check(int ok, const char *what) { printf("  %s  %s\n", ok ? "ok  " : "FAIL", what); fails += !ok; }

static void msg3(void *in, int st, int d1, int d2, int src)
{
    uint8_t m[3] = {(uint8_t)st, (uint8_t)d1, (uint8_t)d2};
    api->on_midi(in, m, 3, src);
}

static int do_rt(void)
{
    static int16_t b[256];
    ui_t u, v;
    void *in = open_at(0, 2000);
    char raw[128];
    api->get_param(in, "ui", raw, sizeof raw);
    printf("  get_param(\"ui\") at power-on: \"%s\"\n", raw);
    check(ui_read(in, &u) && !u.play && u.step == -1 && !strcmp(u.slot, "A01") && u.trk == 0 && u.ntrk >= 6,
          "power-on: A01 open, stopped, track 1");
    check(api->create_instance(dir, NULL) == NULL, "a second instance is refused (the core is one)");

    counting = 1;                       /* from here every call is the audio thread's: nothing may allocate or log */
    /* a step key toggles its trig on the track in view, on the page shown */
    int k = 0;
    while (k < 16 && k < u.len && ((u.trigs >> k) & 1)) k++;
    msg3(in, 0x90, 16 + k, 100, MOVE_MIDI_SOURCE_INTERNAL);
    msg3(in, 0x80, 16 + k, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->render_block(in, b, 128);
    ui_read(in, &v);
    int on = (v.trigs >> k) & 1;
    msg3(in, 0x90, 16 + k, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->render_block(in, b, 128);
    ui_t w;
    ui_read(in, &w);
    int off = !((w.trigs >> k) & 1) && w.trigs == u.trigs;
    /* an external note and a step from another source change nothing */
    msg3(in, 0x90, 16 + k, 100, MOVE_MIDI_SOURCE_EXTERNAL);
    api->render_block(in, b, 128);
    ui_t x;
    ui_read(in, &x);
    int ext = x.trigs == u.trigs;
    /* a pad, stopped: track 1 sounds while it is held, from silence */
    int quiet = 0, loud = 0;
    for (int i = 0; i < 8; i++) { api->render_block(in, b, 128); for (int j = 0; j < 256; j++) quiet |= b[j]; }
    msg3(in, 0x90, 68 + 12, 100, MOVE_MIDI_SOURCE_INTERNAL);
    for (int i = 0; i < 40; i++) {
        api->render_block(in, b, 128);
        for (int j = 0; j < 256; j++) if (abs(b[j]) > loud) loud = abs(b[j]);
    }
    msg3(in, 0x90, 68 + 12, 0, MOVE_MIDI_SOURCE_INTERNAL);   /* (a note-on at velocity 0: its release) */
    for (int i = 0; i < 400; i++) api->render_block(in, b, 128);
    /* the track, then PLAY */
    api->set_param(in, "track", "3");
    api->set_param(in, "track", "9");   /* (out of range: ignored) */
    api->render_block(in, b, 128);
    ui_t t3;
    ui_read(in, &t3);
    api->set_param(in, "track", "0");
    api->set_param(in, "play", "1");
    for (int i = 0; i < 40; i++) api->render_block(in, b, 128);
    ui_t pl;
    ui_read(in, &pl);
    char small[8];
    int sn = api->get_param(in, "ui", small, sizeof small);
    int unknown = api->get_param(in, "rui_poll", raw, sizeof raw) == -1 && api->get_param(in, "state", raw, sizeof raw) == -1;

    /* a long run: pads, steps, tracks, PLAY and reads as fast as hands and ui.js could send them, with the render */
    unsigned long blocks = 0, calls = 0;
    for (int i = 0; i < 30 * 44100 / 128; i++) {
        if (i % 7 == 0) { msg3(in, 0x90, 68 + (i / 7) % 32, 30 + i % 97, MOVE_MIDI_SOURCE_INTERNAL); calls++; }
        if (i % 7 == 3) { msg3(in, 0x80, 68 + (i / 7) % 32, 0, MOVE_MIDI_SOURCE_INTERNAL); calls++; }
        if (i % 53 == 0) { msg3(in, 0x90, 16 + (i / 53) % 16, 100, MOVE_MIDI_SOURCE_INTERNAL); calls++; }
        if (i % 401 == 0) { api->set_param(in, "track", (i / 401) % 2 ? "2" : "1"); calls++; }
        if (i % 997 == 0) { api->set_param(in, "play", (i / 997) % 2 ? "0" : "1"); calls++; }
        if (i % 12 == 0) { api->get_param(in, "ui", raw, sizeof raw); calls++; }
        if (i % 1000 == 0) { uint8_t c = 0xF8; api->on_midi(in, &c, 1, MOVE_MIDI_SOURCE_EXTERNAL); calls++; }
        api->render_block(in, b, 128);
        blocks++;
    }
    /* a burst past the ring: dropped, not overrun */
    for (int i = 0; i < 1000; i++) msg3(in, 0x90, 68 + i % 32, 100, MOVE_MIDI_SOURCE_INTERNAL);
    for (int i = 0; i < 32; i++) msg3(in, 0x80, 68 + i, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->render_block(in, b, 128);
    counting = 0;

    check(on && v.trigs == (u.trigs | 1 << k), "a step key places a trig on the track in view");
    check(off, "the same step key takes it away again");
    check(ext, "an external note on a step number changes no trig");
    check(!quiet && loud > 1000, "a pad plays track 1 from silence (stopped)");
    check(t3.trk == 3, "set_param track 3 (and 9 ignored)");
    check(pl.play == 1 && pl.trk == 0 && pl.step >= 0, "set_param play 1: playing, the playhead moving");
    check(sn == 7 && small[7] == 0, "get_param truncates to the buffer it is given");
    check(unknown, "unknown keys (rui_poll, state) answer -1");
    printf("  %lu blocks and %lu presses, params and reads, then 1000 presses at once: %lu allocations, %lu logs\n",
           blocks, calls, n_alloc, n_log);
    check(n_alloc == 0 && n_log == 0, "nothing allocated or logged on the audio thread");
    api->destroy_instance(in);
    void *again = open_at(0, -1);
    check(again != NULL, "after destroy_instance a new instance opens");
    api->destroy_instance(again);
    return fails != 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "bench DSP.so hash|time [SEC]|rt\n"); return 2; }
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    move_plugin_init_v2_fn init = (move_plugin_init_v2_fn)dlsym(h, MOVE_PLUGIN_INIT_V2_SYMBOL);
    if (!init) { fprintf(stderr, "no %s\n", MOVE_PLUGIN_INIT_V2_SYMBOL); return 1; }
    static host_api_v1_t host;          /* as the shim fills it (zeroed, the reserved tail NULL) */
    host.api_version = MOVE_PLUGIN_API_VERSION;
    host.sample_rate = MOVE_SAMPLE_RATE;
    host.frames_per_block = MOVE_FRAMES_PER_BLOCK;
    host.log = host_log;
    api = init(&host);
    if (!api || api->api_version != MOVE_PLUGIN_API_VERSION_2) { fprintf(stderr, "not a v2 plugin\n"); return 1; }
    snprintf(dir, sizeof dir, "%s", argv[1]);
    char *sl = strrchr(dir, '/');
    if (sl) *sl = 0;
    if (!strcmp(argv[2], "hash")) return do_hash();
    if (!strcmp(argv[2], "time")) return do_time(argc > 3 ? atof(argv[3]) : 20.0);
    if (!strcmp(argv[2], "rt")) return do_rt();
    fprintf(stderr, "bench DSP.so hash|time [SEC]|rt\n");
    return 2;
}
