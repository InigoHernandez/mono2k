/* MONO2K on the Move: the firmware's core, unchanged, as the DSP of a Schwung overtake module (plugin API v2).
 * move/build.sh compiles it to dsp.so for the Move (aarch64 Linux, glibc 2.35).
 *
 * Schwung runs every entry point but create and destroy on its audio thread (core 3, SCHED_FIFO 70, a block of 128
 * frames every 2.9 ms), so nothing on that path allocates, locks, logs, formats with libc or touches a file. The pads
 * and the steps come to on_midi; the knobs, the buttons and the screen are ui.js's, which talks to this file through
 * set_param ("play", "track") and one get_param ("ui") a frame.
 *
 * Step 1, first sound: the factory's bank A at A01, as the FM-1 powers on; PLAY; the track in view; a step press
 * toggles that step's trig on the track; the pads play the track chromatically. One core: m2k_par_begin stays unset,
 * so the render's split (par_mask) never runs. */
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include "../firmware/src/m2k_core.c"
#include "vendor/schwung/plugin_api_v1.h"

#define EXPORT __attribute__((visibility("default")))

/* what the Move sends: the 16 steps and the 32 pads (bottom left to top right, rows of 8), notes on cable 0 */
#define NOTE_STEP1 16
#define NOTE_PAD1  68
#define PADS       32

/* the presses, from on_midi to the render: one writer, one reader, no lock. 256 is more than any hands make in a
 * block; a press past it is dropped and counted */
enum { EV_STEP, EV_PAD_ON, EV_PAD_OFF };
#define EV_N 256u
static struct {
    uint8_t kind[EV_N], a[EV_N], b[EV_N];
    _Atomic uint32_t w, r;
    _Atomic uint32_t dropped;
} evq;

/* the instance. The core is one set of statics (a unity build), so there is one MONO2K at a time */
static struct {
    int live;
    int page;                          /* the 16 steps the step keys edit: the playhead's page while playing */
    uint8_t pad_note[PADS], pad_trk[PADS];   /* what each pad held plays, so its release lets go of that */
    _Atomic int want_play, want_track; /* set_param's, taken by the next render (-1: nothing) */
    /* what get_param("ui") reports, published by each render: packed (see ui_publish), the pattern's name, the render's
     * cost (the mean and the most in microseconds over the last 128 blocks, ~0.37 s) */
    _Atomic uint64_t ui_a, ui_name, ui_cpu;
    uint32_t cpu_sum, cpu_max, cpu_n;
} mv;

static const host_api_v1_t *host;      /* (kept for the next steps: MIDI out, the clock) */

/* ---------------------------------------------------------------------------------------------- the presses */

static void ev_push(int kind, int a, int b)
{
    uint32_t w = atomic_load_explicit(&evq.w, memory_order_relaxed);
    if (w - atomic_load_explicit(&evq.r, memory_order_acquire) >= EV_N) {
        atomic_fetch_add_explicit(&evq.dropped, 1u, memory_order_relaxed);
        return;
    }
    evq.kind[w % EV_N] = (uint8_t)kind;
    evq.a[w % EV_N] = (uint8_t)a;
    evq.b[w % EV_N] = (uint8_t)b;
    atomic_store_explicit(&evq.w, w + 1u, memory_order_release);
}

/* a step key: its trig on the track in view toggled, on the page shown, as GRID places and takes one (the trig at the
 * track's note for new trigs; taken away with its condition, ratchet and locks, step_remove); past the track's
 * length nothing, as on the FM-1 */
static void step_key(int k)
{
    const int t = ui.trk, s = mv.page * 16 + k;
    if (s >= tlen_of(t)) return;
    m2k_step_t *st = &PAT(cur_pat)->step[t][s];
    if (st->flags & ST_TRIG) { step_remove(t, s); return; }
    st->flags = ST_TRIG;
    st->note = ui.note[t];
    step_pop(t, s);
}

/* a pad: the track in view played from it as the FM-1's keys play it (held, legato, its arp when its arp is on), the
 * pads the keys' notes on from F3 at the track's octave, a semitone a pad */
static void pad_on(int i, int vel)
{
    const int t = ui.trk;
    int n = 53 + i + 12 * ui.oct[t];
    n = CLAMP(n, 0, 127);
    mv.pad_note[i] = (uint8_t)n;
    mv.pad_trk[i] = (uint8_t)t;
    live_on_t(t, n, vel);
}

static void pad_off(int i)
{
    const int t = mv.pad_trk[i], n = mv.pad_note[i];
    for (int k = 0; k < sq.nkeys[t]; k++)
        if (sq.keys[t][k] == n) { live_off_t(t, n); return; }   /* (a track left meanwhile let go of it already) */
}

static void ev_drain(void)
{
    uint32_t r = atomic_load_explicit(&evq.r, memory_order_relaxed);
    const uint32_t w = atomic_load_explicit(&evq.w, memory_order_acquire);
    for (; r != w; r++) {
        const int a = evq.a[r % EV_N], b = evq.b[r % EV_N];
        switch (evq.kind[r % EV_N]) {
        case EV_STEP: step_key(a); break;
        case EV_PAD_ON: pad_on(a, b); break;
        case EV_PAD_OFF: pad_off(a); break;
        }
    }
    atomic_store_explicit(&evq.r, r, memory_order_release);
}

/* ------------------------------------------------------------------------------------------ what ui.js reads */

/* packed: playing 1 bit, the track's step + 1 (0: stopped) 7, BPM 10, track 3, the number of tracks 4, page 2, the
 * track's length 7, the page's trigs 16, the pattern 6 */
static void ui_publish(void)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = ui.trk, len = tlen_of(t), tp = sq.playing ? track_pos(t) : -1;
    if (tp >= 0) mv.page = tp / 16;
    if (mv.page * 16 >= len) mv.page = 0;
    uint32_t trigs = 0;
    for (int k = 0; k < 16; k++) {
        const int s = mv.page * 16 + k;
        if (s < len && (p->step[t][s].flags & ST_TRIG)) trigs |= 1u << k;
    }
    uint32_t bpm = seq_bpm();
    if (bpm > 1023u) bpm = 1023u;
    const uint64_t a = (uint64_t)(sq.playing != 0) | (uint64_t)(tp + 1) << 1 | (uint64_t)bpm << 8 |
                       (uint64_t)t << 18 | (uint64_t)M2K_TRACKS << 21 | (uint64_t)mv.page << 25 |
                       (uint64_t)len << 27 | (uint64_t)trigs << 34 | (uint64_t)cur_pat << 50;
    uint64_t name = 0;
    memcpy(&name, p->name, sizeof name);
    atomic_store_explicit(&mv.ui_a, a, memory_order_release);
    atomic_store_explicit(&mv.ui_name, name, memory_order_release);
}

/* "PLAY STEP BPM TRACK TRACKS PAGE LEN TRIGS CPU_MEAN CPU_MOST SLOT NAME", space-separated; STEP the track's, from 0
 * (-1 stopped), TRACK from 0, TRIGS the page's 16 as hex (bit k step k), CPU in microseconds a block, the name last */
static int ui_string(char *o)
{
    static const char HEX[] = "0123456789ABCDEF";
    const uint64_t a = atomic_load_explicit(&mv.ui_a, memory_order_acquire);
    const uint64_t nm = atomic_load_explicit(&mv.ui_name, memory_order_acquire);
    const uint64_t cpu = atomic_load_explicit(&mv.ui_cpu, memory_order_relaxed);
    char *e = o;
    e = s_int(e, (int)(a & 1)); *e++ = ' ';
    e = s_int(e, (int)(a >> 1 & 127) - 1); *e++ = ' ';
    e = s_int(e, (int)(a >> 8 & 1023)); *e++ = ' ';
    e = s_int(e, (int)(a >> 18 & 7)); *e++ = ' ';
    e = s_int(e, (int)(a >> 21 & 15)); *e++ = ' ';
    e = s_int(e, (int)(a >> 25 & 3)); *e++ = ' ';
    e = s_int(e, (int)(a >> 27 & 127)); *e++ = ' ';
    for (int sh = 12; sh >= 0; sh -= 4) *e++ = HEX[a >> (34 + sh) & 15];
    *e++ = ' ';
    e = s_int(e, (int)(cpu & 0xFFFFFFFFu)); *e++ = ' ';
    e = s_int(e, (int)(cpu >> 32)); *e++ = ' ';
    e = s_pat(e, (int)(a >> 50 & 63)); *e++ = ' ';
    char n[9];
    memcpy(n, &nm, 8);
    n[8] = 0;
    int l = 8;
    while (l && (n[l - 1] == ' ' || n[l - 1] == 0)) l--;
    for (int i = 0; i < l; i++) *e++ = n[i] >= 32 && n[i] < 127 ? n[i] : '?';
    *e = 0;
    return (int)(e - o);
}

/* ------------------------------------------------------------------------------------------- the plugin API */

/* a whole number at the start of s (a param's value), or -1 */
static int s_num(const char *s)
{
    int v = 0, any = 0;
    while (*s == ' ') s++;
    while (*s >= '0' && *s <= '9' && v < 100000) { v = v * 10 + (*s++ - '0'); any = 1; }
    return any ? v : -1;
}

/* module.json's "defaults" (Schwung hands the file to create_instance): "seed" fixes the core's dice (the tests', so a
 * render is the same every time; else the clock's, as the FM-1's power-on takes its timer's), "pattern" the one opened
 * (0 = A01, the FM-1's) */
static int json_int(const char *js, const char *key, int dflt)
{
    if (!js) return dflt;
    const char *p = strstr(js, key);
    if (!p) return dflt;
    p += strlen(key);
    while (*p == ' ' || *p == '"' || *p == ':' || *p == '\t') p++;
    int v = s_num(p);
    return v < 0 ? dflt : v;
}

static void *v2_create(const char *module_dir, const char *json_defaults)
{
    (void)module_dir;
    if (mv.live) return NULL;           /* (the core is one: a second would share the first's state and break it) */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint32_t clock_seed = (uint32_t)ts.tv_nsec ^ (uint32_t)ts.tv_sec * 2654435761u;
    const int seed = json_int(json_defaults, "\"seed\"", -1);
    const int pattern = json_int(json_defaults, "\"pattern\"", 0);

    atomic_store(&evq.w, 0u);
    atomic_store(&evq.r, 0u);
    atomic_store(&evq.dropped, 0u);
    m_set(&mv, 0, sizeof mv);
    atomic_store(&mv.want_play, -1);
    atomic_store(&mv.want_track, -1);

    m2k_par_begin = 0;                  /* one core for now: the render does every track and the bus itself */
    m2k_par_end = 0;
    m2k_clock = 0;
    m2k_init(seed >= 0 ? (uint32_t)seed : clock_seed);   /* the factory: bank A, A01 open, stopped */
    ui.splash_end = 0;                  /* (no splash: the screen is ui.js's) */
    ui.splashed = 1;
    if (pattern > 0 && pattern < M2K_PATTERNS) m2k_pattern(pattern);
    ui_publish();
    mv.live = 1;
    return &mv;
}

static void v2_destroy(void *inst)
{
    if (inst != &mv) return;
    mv.live = 0;
}

static void v2_on_midi(void *inst, const uint8_t *msg, int len, int source)
{
    if (inst != &mv || !mv.live || len < 3 || source != MOVE_MIDI_SOURCE_INTERNAL) return;
    const int cmd = msg[0] & 0xF0, n = msg[1], v = msg[2];
    if (cmd != 0x90 && cmd != 0x80) return;
    const int on = cmd == 0x90 && v > 0;
    if (n >= NOTE_STEP1 && n < NOTE_STEP1 + 16) {
        if (on) ev_push(EV_STEP, n - NOTE_STEP1, 0);
    } else if (n >= NOTE_PAD1 && n < NOTE_PAD1 + PADS) {
        ev_push(on ? EV_PAD_ON : EV_PAD_OFF, n - NOTE_PAD1, v);
    }
}

static void v2_set_param(void *inst, const char *key, const char *val)
{
    if (inst != &mv || !mv.live || !key || !val) return;
    const int v = s_num(val);
    if (s_is(key, "play")) {
        if (v == 0 || v == 1) atomic_store_explicit(&mv.want_play, v, memory_order_release);
    } else if (s_is(key, "track")) {
        if (v >= 0 && v < M2K_TRACKS) atomic_store_explicit(&mv.want_track, v, memory_order_release);
    }
}

static int v2_get_param(void *inst, const char *key, char *buf, int buf_len)
{
    if (inst != &mv || !mv.live || !key || !buf || buf_len <= 0) return -1;
    if (s_is(key, "ui")) {
        char o[96];
        int n = ui_string(o);
        if (n > buf_len - 1) n = buf_len - 1;
        memcpy(buf, o, (size_t)n);
        buf[n] = 0;
        return n;
    }
    return -1;                          /* (rui_poll, state, remote_snapshot_rt_safe...: not served) */
}

static int v2_get_error(void *inst, char *buf, int buf_len)
{
    (void)inst; (void)buf; (void)buf_len;
    return 0;                           /* (a refused second instance is NULL from create: nothing else fails) */
}

static uint32_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);   /* (the vDSO's: no system call) */
    return (uint32_t)ts.tv_sec * 1000000u + (uint32_t)(ts.tv_nsec / 1000);
}

/* a block: what set_param asked, the presses since the last block, the core's render as it is (interleaved int16, as
 * the FM-1 and the browser hear it), then what ui.js reads */
static void v2_render(void *inst, int16_t *out, int frames)
{
    if (inst != &mv || !mv.live || frames <= 0) {
        if (out && frames > 0) memset(out, 0, (size_t)frames * 4u);
        return;
    }
    const uint32_t t0 = now_us();
    const int tr = atomic_exchange_explicit(&mv.want_track, -1, memory_order_acq_rel);
    if (tr >= 0) m2k_track(tr);
    const int pl = atomic_exchange_explicit(&mv.want_play, -1, memory_order_acq_rel);
    if (pl >= 0) seq_play(pl);
    ev_drain();
    const int n = frames / M2K_SUB * M2K_SUB;   /* (Schwung's blocks are 128 frames: 8 of the core's) */
    if (n) m2k_render(out, n);
    if (n < frames) memset(out + 2 * n, 0, (size_t)(frames - n) * 4u);
    ui_publish();
    const uint32_t dt = now_us() - t0;
    mv.cpu_sum += dt;
    if (dt > mv.cpu_max) mv.cpu_max = dt;
    if (++mv.cpu_n == 128u) {
        atomic_store_explicit(&mv.ui_cpu, (uint64_t)(mv.cpu_sum / 128u) | (uint64_t)mv.cpu_max << 32, memory_order_relaxed);
        mv.cpu_sum = mv.cpu_max = mv.cpu_n = 0;
    }
}

static plugin_api_v2_t api = {
    .api_version = MOVE_PLUGIN_API_VERSION_2,
    .create_instance = v2_create,
    .destroy_instance = v2_destroy,
    .on_midi = v2_on_midi,
    .set_param = v2_set_param,
    .get_param = v2_get_param,
    .get_error = v2_get_error,
    .render_block = v2_render,
};

EXPORT plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *h)
{
    host = h;
    return &api;
}
