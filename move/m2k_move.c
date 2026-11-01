/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the firmware's core, unchanged, as the DSP of a Schwung overtake module (plugin API v2).
 * move/build.sh compiles it to dsp.so for the Move (aarch64 Linux, glibc 2.35).
 *
 * Schwung runs every entry point but create and destroy on its audio thread (core 3, SCHED_FIFO 70, a block of 128
 * frames every 2.9 ms), so nothing on that path allocates, locks, logs, formats with libc or touches a file. create and
 * destroy run on Schwung's loader thread: the store is read there, and written there on closing. Two threads of our
 * own, made in create and joined in destroy, demoted to SCHED_OTHER on cores 0-2 (Schwung's rule):
 *   the drawer   the 128 x 64 screen from the page state the audio thread publishes (its screen part, gathered by
 *                move/screen/m2k_screen.c on the audio thread and drawn here by m2k_screen_draw), into a triple
 *                buffer get_param("frame") reads;
 *   the saver    the store's file from a snapshot the audio thread copies a piece a block (a temp file, then a rename).
 * In: the pads and the steps to on_midi (sample-exact); Move's clock and transport to on_midi too (once a block);
 * the knobs, the jog, the buttons and the knobs' touch from ui.js, through set_param("in"). All of it goes into one
 * queue the next render drains, in order, into the controller (m2k_move_ui.c). Out: get_param("frame"), the screen and
 * the LEDs, once a frame (ui.js reads one every two of the panel's frames, ~28.7 a second: README, The screen).
 * The queue, Move's clock, the tempo asked of Move, the block, the controller and the screen are m2k_move_glue.c's,
 * shared with the site's playable Move (web/move/m2k_move_web.c); this file is Schwung's side of it. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "../firmware/src/m2k_core.c"
#include "vendor/schwung/plugin_api_v1.h"

#define EXPORT __attribute__((visibility("default")))

/* what the Move sends: the 16 steps and the 32 pads (bottom left to top right, rows of 8), notes on cable 0 */
#define NOTE_STEP1 16
#define NOTE_PAD1  68
#define PADS       32

static const host_api_v1_t *host;
static float mv_host_bpm(void) { return host && host->get_bpm ? host->get_bpm() : 0.0f; }

#include "m2k_move_glue.c"   /* the queue, the clock, the tempo asked of Move, the block, the controller, the screen */

/* ------------------------------------------------------------------------------------------------ the saver */

/* a save: the audio thread copies what the store keeps into the snapshot, a piece a block (the settings and the
 * library, eight patterns, then the projects' images 32 KB at a time); the saver's thread packs the working patterns
 * from it (pat_pack, the core's, pure) and writes the store as m2k_store_pack writes it (format "M2K2" 5) into a temp
 * file, then renames it over the last. The core's own pack, whole, takes ~1 ms on the Move: never in a block */
enum { SV_IDLE, SV_COPY, SV_READY };
#define SV_FILE "mono2k.m2ks"
static struct {
    _Atomic int state;
    int part, pi;
    uint32_t po;
    uint8_t want, reload;              /* a save asked for; with the open pattern's reload point first (Shift + Rec) */
    uint8_t cur_proj, cur_pat;
    uint8_t set[S_COUNT];
    m2k_pattern_t p[M2K_PATTERNS];
    m2k_img_t proj[M2K_PROJECTS];
    m2k_sound_t snd[M2K_SOUNDS];
    uint8_t cat[M2K_SOUNDS], root[M2K_SOUNDS];
    _Atomic uint32_t done, failed;     /* the saver's counts */
    uint32_t seen_done, seen_failed;   /* the audio thread's (its messages) */
} sv;
static m2k_img_t sv_img;               /* the saver's: the working patterns packed */
static uint8_t sv_blob[BLOB_MAX];

static int th_save_on;
static void mv_save_request(int reload)
{
    if (!th_save_on) { if (reload) toast("NOT SAVED", "NO PLACE TO SAVE"); return; }
    sv.want = 1;
    sv.reload |= (uint8_t)reload;
}

/* each block, at the end of the render: a piece of the snapshot, or a new one begun */
static void sv_step(void)
{
    const int st = atomic_load_explicit(&sv.state, memory_order_acquire);
    if (st == SV_IDLE) {
        if (!sv.want) return;
        if (sv.reload && !pattern_save()) toast("PROJECT FULL", "PATTERN NOT KEPT");
        sv.want = sv.reload = 0;
        sv.part = 0;
        atomic_store_explicit(&sv.state, SV_COPY, memory_order_relaxed);
    } else if (st != SV_COPY) return;
    if (sv.part == 0) {
        sv.cur_proj = (uint8_t)cur_proj;
        sv.cur_pat = (uint8_t)cur_pat;
        m_cpy(sv.set, store_.set, S_COUNT);
        sv.set[S_SYNI] = mv.syni;
        m_cpy(sv.snd, snd, sizeof snd);
        m_cpy(sv.cat, snd_cat, sizeof snd_cat);
        m_cpy(sv.root, snd_root, sizeof snd_root);
        sv.part = 1;
        return;
    }
    if (sv.part <= M2K_PATTERNS / 8) {
        const int i0 = (sv.part - 1) * 8;
        m_cpy(&sv.p[i0], PAT(i0), 8u * sizeof(m2k_pattern_t));
        if (++sv.part > M2K_PATTERNS / 8) { sv.pi = 0; sv.po = 0; }
        return;
    }
    const m2k_img_t *s = &proj[sv.pi];
    m2k_img_t *d = &sv.proj[sv.pi];
    if (!sv.po) { m_cpy(d->name, s->name, 8); d->used = s->used; m_cpy(d->len, s->len, sizeof d->len); }
    uint32_t n = d->used - sv.po;
    if (n > 32768u) n = 32768u;
    memcpy(d->data + sv.po, s->data + sv.po, n);
    sv.po += n;
    if (sv.po >= d->used) { sv.pi++; sv.po = 0; }
    if (sv.pi >= M2K_PROJECTS) atomic_store_explicit(&sv.state, SV_READY, memory_order_release);
}

static char *p_cat(char *d, const char *s, const char *end) { while (*s && d < end) *d++ = *s++; *d = 0; return d; }

static void sv_path(char *o, int n, const char *name)
{
    char *e = p_cat(o, mv.dir, o + n - 1);
    e = p_cat(e, "/", o + n - 1);
    p_cat(e, name, o + n - 1);
}

/* an image as the store has it (m2k_proj.c's ser_img) */
static int sv_img_out(FILE *f, const m2k_img_t *im)
{
    uint8_t h[12], l[2 * M2K_PATTERNS];
    uint32_t used = 0;
    for (int i = 0; i < M2K_PATTERNS; i++) used += im->len[i];
    memcpy(h, im->name, 8);
    for (int k = 0; k < 4; k++) h[8 + k] = (uint8_t)(used >> (8 * k));
    for (int i = 0; i < M2K_PATTERNS; i++) { l[2 * i] = (uint8_t)im->len[i]; l[2 * i + 1] = (uint8_t)(im->len[i] >> 8); }
    return fwrite(h, 12, 1, f) == 1 && fwrite(l, sizeof l, 1, f) == 1 && (!used || fwrite(im->data, used, 1, f) == 1);
}

/* the store from the snapshot, as m2k_store_pack writes it, into PATH (a temp file renamed over the last) */
static int sv_write_store(const char *path)
{
    /* the working patterns packed, a blank as nothing (pat_pack_img's), in order */
    memset(sv_img.name, 0, sizeof sv_img.name);
    memcpy(sv_img.name, "WORK", 4);
    sv_img.used = 0;
    for (int i = 0; i < M2K_PATTERNS; i++) {
        int n = pat_pack(&sv.p[i], sv_blob);
        if (n == blank_n && !memcmp(sv_blob, blank_blob, (size_t)n)) n = 0;
        if (sv_img.used + (uint32_t)n > IMG_DATA) return 0;
        memcpy(sv_img.data + sv_img.used, sv_blob, (size_t)n);
        sv_img.len[i] = (uint16_t)n;
        sv_img.used += (uint32_t)n;
    }
    char tmp[256];
    sv_path(tmp, sizeof tmp, SV_FILE ".tmp");
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    const uint8_t h[10] = {'M', '2', 'K', '2', 5, sv.cur_proj, sv.cur_pat, FACTORY_REV, M2K_PATTERNS, M2K_PROJECTS};
    int ok = fwrite(h, 10, 1, f) == 1 && fwrite(sv.set, S_COUNT, 1, f) == 1 && sv_img_out(f, &sv_img);
    for (int i = 0; i < M2K_PROJECTS && ok; i++) ok = sv_img_out(f, &sv.proj[i]);
    uint8_t sh[2] = {'S', M2K_SOUNDS};
    ok = ok && fwrite(sh, 2, 1, f) == 1 && fwrite(sv.snd, sizeof sv.snd, 1, f) == 1;
    sh[0] = 'C';
    ok = ok && fwrite(sh, 2, 1, f) == 1 && fwrite(sv.cat, sizeof sv.cat, 1, f) == 1;
    sh[0] = 'R';
    ok = ok && fwrite(sh, 2, 1, f) == 1 && fwrite(sv.root, sizeof sv.root, 1, f) == 1;
    ok = ok && fflush(f) == 0 && fsync(fileno(f)) == 0;
    ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(tmp, path) == 0;
    else unlink(tmp);
    return ok;
}

/* bytes as the store's file, by a temp file and a rename (destroy's: the core's own pack, the instance retired) */
static int sv_write_bytes(const uint8_t *b, int n)
{
    char tmp[256], path[256];
    sv_path(tmp, sizeof tmp, SV_FILE ".tmp");
    sv_path(path, sizeof path, SV_FILE);
    mkdir(mv.dir, 0755);
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    int ok = fwrite(b, (size_t)n, 1, f) == 1 && fflush(f) == 0 && fsync(fileno(f)) == 0;
    ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(tmp, path) == 0;
    else unlink(tmp);
    return ok;
}

/* ------------------------------------------------------------------------------------- the screen, the threads */

/* the page state: built on the audio thread once a panel frame (6 blocks, 17.4 ms: the OLED takes a slice a block),
 * and 3 blocks after a press at the soonest, published by a sequence lock the drawer reads without ever making the
 * audio thread wait. Its screen part is the renderer's gather (m2k_screen_gather): all the drawer reads */
#define VW_EVERY 6                     /* blocks between publishes, nothing pressed */
#define VW_SOON 3                      /* after a press, at the soonest */
static mv_view_t vw_pub, vw_now;
static _Atomic uint32_t vw_lock;
static uint32_t vw_count, vw_last;   /* publishes; the block of the last */

/* what the screen costs, for the bench (get_param "cost"): the publish on the audio thread, the drawing on the drawer's,
 * the reads; ns */
static struct { uint64_t pub_ns, pub_max, draw_ns, draw_max; uint32_t pub_n, draw_n, reads; } cost;
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);   /* (the vDSO's: no system call) */
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void view_publish(void)
{
    const uint64_t t0 = now_ns();
    mv_view_build(&vw_now);
    vw_now.seq = ++vw_count;
    atomic_fetch_add_explicit(&vw_lock, 1u, memory_order_acq_rel);     /* (odd: being written) */
    atomic_thread_fence(memory_order_release);
    memcpy(&vw_pub, &vw_now, sizeof vw_pub);
    atomic_fetch_add_explicit(&vw_lock, 1u, memory_order_release);
    const uint64_t d = now_ns() - t0;
    cost.pub_ns += d;
    if (d > cost.pub_max) cost.pub_max = d;
    cost.pub_n++;
}

static int view_read(mv_view_t *v)
{
    for (int tries = 0; tries < 16; tries++) {
        const uint32_t s0 = atomic_load_explicit(&vw_lock, memory_order_acquire);
        if (s0 & 1u) continue;
        memcpy(v, &vw_pub, sizeof *v);
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&vw_lock, memory_order_relaxed) == s0) return 1;
    }
    return 0;
}

/* the frames: a triple buffer. The drawer draws into its own, then trades it for the middle one (marked new); a read
 * takes the middle one when it is new. Neither waits */
static uint8_t fbuf[3][MS_BYTES];
static uint32_t fseq[3];
static _Atomic uint32_t fb_mid = 1;
static int fb_w, fb_r = 2;             /* the drawer's, the reader's */
static uint32_t fb_count;

static void frame_draw(const mv_view_t *v)
{
    const uint64_t t0 = now_ns();
    m2k_screen_draw(&v->scr, fbuf[fb_w]);
    const uint64_t d = now_ns() - t0;
    cost.draw_ns += d;                 /* (the drawer's own, or the render's without one: one writer either way) */
    if (d > cost.draw_max) cost.draw_max = d;
    cost.draw_n++;
    fseq[fb_w] = ++fb_count;
    fb_w = (int)(atomic_exchange_explicit(&fb_mid, (uint32_t)fb_w | 4u, memory_order_acq_rel) & 3u);
}

static pthread_t th_draw, th_save;
static _Atomic int th_stop;
static int th_draw_on;

static void nap_ms(int ms)
{
    struct timespec ts = {0, ms * 1000000L};
    nanosleep(&ts, 0);
}

/* a thread of ours off the audio's: SCHED_OTHER first (a thread inherits its maker's FIFO), cores 0-2 */
static void thread_demote(void)
{
#ifdef __linux__                       /* (the Move's; the sheets build this unit on a Mac and run no threads) */
    struct sched_param sp;
    memset(&sp, 0, sizeof sp);
    sched_setscheduler(0, SCHED_OTHER, &sp);
    cpu_set_t s;
    CPU_ZERO(&s);
    for (int c = 0; c < 3; c++) CPU_SET(c, &s);
    sched_setaffinity(0, sizeof s, &s);
#endif
}

/* the drawer: a frame for each page state published whose screen changed (a publish is a panel frame apart at most
 * once in 6 blocks, or 3 after a press), drawn from its copy alone */
static void *draw_main(void *arg)
{
    (void)arg;
    thread_demote();
    static mv_view_t v;
    static ms_view_t last;
    uint32_t seen = 0;
    int have = 0;
    while (!atomic_load_explicit(&th_stop, memory_order_acquire)) {
        if (view_read(&v) && v.seq != seen) {
            seen = v.seq;
            if (!have || memcmp(&v.scr, &last, sizeof last)) { frame_draw(&v); last = v.scr; have = 1; }
        }
        nap_ms(4);
    }
    return 0;
}

static void *save_main(void *arg)
{
    (void)arg;
    thread_demote();
    for (;;) {
        if (atomic_load_explicit(&sv.state, memory_order_acquire) == SV_READY) {
            char path[256];
            sv_path(path, sizeof path, SV_FILE);
            mkdir(mv.dir, 0755);
            if (sv_write_store(path)) atomic_fetch_add(&sv.done, 1u);
            else atomic_fetch_add(&sv.failed, 1u);
            atomic_store_explicit(&sv.state, SV_IDLE, memory_order_release);
            continue;
        }
        if (atomic_load_explicit(&th_stop, memory_order_acquire)) break;
        nap_ms(20);
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------ what ui.js reads */

static char *ap(char *e, const char *s) { while (*s) *e++ = *s++; *e = 0; return e; }
static const char HEX[] = "0123456789ABCDEF";
static char *ap_hex8(char *e, uint32_t v) { for (int sh = 28; sh >= 0; sh -= 4) *e++ = HEX[v >> sh & 15]; *e = 0; return e; }

/* "M2K1", the frame's number (8 hex), its 64 rows of 22 characters (6 pixels each from '0', MSB first, the last
 * character's low two bits past the edge), then the LEDs as two hex digits each: 16 steps, 32 pads, 4 track buttons
 * and MV_NBTN buttons (MV_BTN_CC's order); then how each knob turns ('0' + MV_KK_*, 8), the tempo asked of Move: its
 * number (2 hex, 00 never) and the tempo (4 hex, tenths of BPM) */
#define FRAME_LEN (4 + 8 + 64 * 22 + 2 * (16 + 32 + 4 + MV_NBTN) + 8 + 2 + 4)
static int frame_string(char *o)
{
    if (atomic_load_explicit(&fb_mid, memory_order_acquire) & 4u)
        fb_r = (int)(atomic_exchange_explicit(&fb_mid, (uint32_t)fb_r, memory_order_acq_rel) & 3u);
    cost.reads++;
    const uint8_t *fb = fbuf[fb_r];
    char *e = ap(o, "M2K1");
    e = ap_hex8(e, fseq[fb_r]);
    for (int y = 0; y < 64; y++) {
        const uint8_t *r = fb + 16 * y;
        uint32_t acc = 0;
        int nb = 0, i = 0;
        for (int c = 0; c < 22; c++) {
            while (nb < 6) { acc = acc << 8 | (i < 16 ? r[i] : 0); i++; nb += 8; }
            *e++ = (char)('0' + (acc >> (nb - 6) & 63));
            nb -= 6;
        }
    }
    const mv_leds_t *l = &vw_now.led;
    const uint8_t *seq[4] = {l->step, l->pad, l->trk, l->btn};
    const int len[4] = {16, 32, 4, MV_NBTN};
    for (int g = 0; g < 4; g++)
        for (int k = 0; k < len[g]; k++) { *e++ = HEX[seq[g][k] >> 4 & 15]; *e++ = HEX[seq[g][k] & 15]; }
    for (int k = 0; k < 8; k++) *e++ = (char)('0' + (vw_now.kkind[k] & 3));
    *e++ = HEX[vw_now.tq_seq >> 4 & 15]; *e++ = HEX[vw_now.tq_seq & 15];
    for (int sh = 12; sh >= 0; sh -= 4) *e++ = HEX[vw_now.tq_b10 >> sh & 15];
    *e = 0;
    return (int)(e - o);
}

/* "PLAY STEP BPM TRACK TRACKS PAGE LEN TRIGS CPU_MEAN CPU_MOST SLOT NAME" (step 1's, kept for the tools) */
static int ui_string(char *o)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = ui.trk, len = tlen_of(t), tp = sq.playing ? track_pos(t) : -1;
    uint32_t trigs = 0;
    for (int k = 0; k < 16; k++) {
        const int s = ui.spage * 16 + k;
        if (s < len && (p->step[t][s].flags & ST_TRIG)) trigs |= 1u << k;
    }
    char *e = o;
    e = s_int(e, sq.playing != 0); *e++ = ' ';
    e = s_int(e, tp); *e++ = ' ';
    e = s_int(e, (int)(mv_bpm10() / 10)); *e++ = ' ';
    e = s_int(e, t); *e++ = ' ';
    e = s_int(e, M2K_TRACKS); *e++ = ' ';
    e = s_int(e, ui.spage); *e++ = ' ';
    e = s_int(e, len); *e++ = ' ';
    for (int sh = 12; sh >= 0; sh -= 4) *e++ = HEX[trigs >> sh & 15];
    *e++ = ' ';
    e = s_int(e, (int)mv.cpu_mean); *e++ = ' ';
    e = s_int(e, (int)mv.cpu_most); *e++ = ' ';
    e = s_pat(e, cur_pat); *e++ = ' ';
    char n[9];
    memcpy(n, p->name, 8);
    n[8] = 0;
    int l = 8;
    while (l && (n[l - 1] == ' ' || n[l - 1] == 0)) l--;
    for (int i = 0; i < l; i++) *e++ = n[i] >= 32 && n[i] < 127 ? n[i] : '?';
    *e = 0;
    return (int)(e - o);
}

/* the page state as text, for the tests: "KEY=VALUE" pairs */
static int view_string(char *o)
{
    static mv_view_t v;
    mv_view_build(&v);
    char *e = o;
    e = ap(e, "pg="); e = s_int(e, v.page);
    e = ap(e, " pname="); e = ap(e, m2k_screen_page_name(v.page));
    e = ap(e, " trk="); e = s_int(e, v.trk);
    e = ap(e, " mach="); e = ap(e, v.machine);
    e = ap(e, " pat="); e = ap(e, v.slot);
    e = ap(e, " next="); e = ap(e, v.next[0] ? v.next : "-");
    e = ap(e, " play="); e = s_int(e, v.playing);
    e = ap(e, " rec="); e = s_int(e, v.rec);
    e = ap(e, " sync="); e = s_int(e, v.sync);
    e = ap(e, " fol="); e = s_int(e, v.following);
    e = ap(e, " bpm10="); e = s_int(e, v.bpm10);
    e = ap(e, " ov="); e = s_int(e, v.overlay);
    e = ap(e, " psel="); e = s_int(e, v.pat_sel);
    e = ap(e, " oct="); e = s_int(e, v.oct);
    e = ap(e, " bank="); e = s_int(e, v.bank);
    e = ap(e, " sp="); e = s_int(e, v.spage);
    e = ap(e, " np="); e = s_int(e, v.npage);
    e = ap(e, " follow="); e = s_int(e, v.follow);
    e = ap(e, " mutes="); e = s_int(e, v.mutes);
    e = ap(e, " held="); e = s_int(e, v.held);
    e = ap(e, " focus="); e = s_int(e, v.focus);
    e = ap(e, " perf="); e = s_int(e, (int)v.perf_on);
    e = ap(e, " pkeys="); e = s_int(e, v.perf); *e++ = ','; e = s_int(e, v.perf_pg);
    e = ap(e, " hold="); e = s_int(e, sq.arp_hold[v.trk]);
    e = ap(e, " k=");
    for (int k = 0; k < 8; k++) { if (k) *e++ = ','; e = s_int(e, v.knob[k].raw); }
    e = ap(e, " kf=");
    for (int k = 0; k < 8; k++) { if (k) *e++ = ','; e = s_int(e, v.knob[k].flags); }
    e = ap(e, " st=");
    for (int k = 0; k < 16; k++) { if (k) *e++ = ','; e = s_int(e, v.step[k]); }
    e = ap(e, " led=");
    for (int k = 0; k < 16; k++) { if (k) *e++ = ','; e = s_int(e, v.led.step[k]); }
    e = ap(e, " pads=");
    for (int k = 0; k < 32; k++) { if (k) *e++ = ','; e = s_int(e, v.led.pad[k]); }
    e = ap(e, " trkled=");
    for (int k = 0; k < 4; k++) { if (k) *e++ = ','; e = s_int(e, v.led.trk[k]); }
    e = ap(e, " cat="); e = ap(e, v.cat_slot); *e++ = ':'; e = ap(e, v.cat_name);
    e = ap(e, " spg="); e = s_int(e, v.scr.page);
    e = ap(e, " tk="); e = s_int(e, v.scr.touch.k); *e++ = ':'; e = ap(e, v.scr.touch.name);
    e = ap(e, " tval="); e = ap(e, v.scr.touch.val[0] ? v.scr.touch.val : "-"); *e++ = ':';
    e = ap(e, v.scr.touch.unit[0] ? v.scr.touch.unit : "-");
    e = ap(e, " tfocus="); e = s_int(e, v.tempo_focus);
    e = ap(e, " kk="); for (int k = 0; k < 8; k++) *e++ = (char)('0' + v.kkind[k]);
    *e = 0;
    e = ap(e, " tq="); e = s_int(e, v.tq_state); *e++ = ','; e = s_int(e, v.tq_seq); *e++ = ','; e = s_int(e, v.tq_b10);
    e = ap(e, " int="); e = s_int(e, v.scr.clk_int);
    e = ap(e, " toast="); e = ap(e, v.toast); *e++ = '|'; e = ap(e, v.toast2);
    return (int)(e - o);
}

/* the track in view of the open pattern, for the tests: its machine, its kit, its 64 steps (flags.note, the note a
 * trig's alone: a step without one keeps none in the store), its locks */
static int dump_string(char *o)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = ui.trk;
    char *e = o;
    e = ap(e, "m="); e = s_int(e, p->machine[t]);
    e = ap(e, " kit=");
    for (int k = 0; k < P_COUNT; k++) { if (k) *e++ = ','; e = s_int(e, p->kit[t][k]); }
    e = ap(e, " steps=");
    for (int s = 0; s < M2K_STEPS; s++) {
        if (s) *e++ = ',';
        const m2k_step_t *st = &p->step[t][s];
        e = s_int(e, st->flags); *e++ = '.'; e = s_int(e, (st->flags & ST_TRIG) ? st->note : 0);
    }
    int nl = 0;
    for (int i = 0; i < M2K_LOCKS; i++) if (p->lock[i].trk == t) nl++;
    e = ap(e, " locks="); e = s_int(e, nl);
    e = ap(e, " glob=");
    for (int g = 0; g < G_COUNT; g++) { if (g) *e++ = ','; e = s_int(e, p->glob[g]); }
    e = ap(e, " mute="); e = s_int(e, p->mute);
    return (int)(e - o);
}

/* Move's clock as followed, for the bench: "SYNC RUN NRX NTX PER_Q8 BPM10 STEPS STEP_AT ERR_MILLI FC_PPM EXT
 * PLAYING SAVES SAVE_FAILS SAVE_STATE TQ_STATE FREE RELOCK OFF TQ_SEQ TQ_B10 ASKS FOLLOWS ALONES" */
static int clock_string(char *o)
{
    char *e = o;
    e = s_int(e, ck.sync); *e++ = ' ';
    e = s_int(e, ck.run); *e++ = ' ';
    e = s_int(e, ck.nrx); *e++ = ' ';
    e = s_int(e, ck.ntx); *e++ = ' ';
    e = s_int(e, (int)mx.per_q8); *e++ = ' ';
    e = s_int(e, (int)bpm10); *e++ = ' ';
    e = s_int(e, (int)ck.steps); *e++ = ' ';
    e = s_int(e, (int)ck.step_at); *e++ = ' ';
    e = s_int(e, (int)(ck.err * 1000)); *e++ = ' ';
    e = s_int(e, (int)(ck.fc * 1e6)); *e++ = ' ';
    e = s_int(e, mx.ext); *e++ = ' ';
    e = s_int(e, sq.playing); *e++ = ' ';
    e = s_int(e, (int)atomic_load(&sv.done)); *e++ = ' ';
    e = s_int(e, (int)atomic_load(&sv.failed)); *e++ = ' ';
    e = s_int(e, atomic_load(&sv.state)); *e++ = ' ';
    e = s_int(e, tq.state); *e++ = ' ';
    e = s_int(e, ck.free); *e++ = ' ';
    e = s_int(e, ck.relock); *e++ = ' ';
    e = s_int(e, ck.off); *e++ = ' ';
    e = s_int(e, tq.seq); *e++ = ' ';
    e = s_int(e, tq.b10); *e++ = ' ';
    e = s_int(e, (int)tq.asks); *e++ = ' ';
    e = s_int(e, (int)tq.follows); *e++ = ' ';
    e = s_int(e, (int)tq.alones);
    return (int)(e - o);
}

/* the screen's cost, for the bench: "PUBLISHES PUB_NS_MEAN PUB_NS_MOST DRAWS DRAW_NS_MEAN DRAW_NS_MOST READS" */
static int cost_string(char *o)
{
    char *e = o;
    e = s_int(e, (int)cost.pub_n); *e++ = ' ';
    e = s_int(e, (int)(cost.pub_n ? cost.pub_ns / cost.pub_n : 0)); *e++ = ' ';
    e = s_int(e, (int)cost.pub_max); *e++ = ' ';
    e = s_int(e, (int)cost.draw_n); *e++ = ' ';
    e = s_int(e, (int)(cost.draw_n ? cost.draw_ns / cost.draw_n : 0)); *e++ = ' ';
    e = s_int(e, (int)cost.draw_max); *e++ = ' ';
    e = s_int(e, (int)cost.reads);
    *e = 0;
    return (int)(e - o);
}

/* ------------------------------------------------------------------------------------------- the plugin API */

/* a whole number at the start of s (a param's value), signed; *ok whether there was one */
static int s_snum(const char **ps, int *ok)
{
    const char *s = *ps;
    int v = 0, neg = 0, any = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9' && v < 100000) { v = v * 10 + (*s++ - '0'); any = 1; }
    *ps = s;
    if (ok) *ok = any;
    return neg ? -v : v;
}
static int s_num(const char *s) { int ok; int v = s_snum(&s, &ok); return ok && v >= 0 ? v : -1; }

/* module.json's "defaults" (Schwung hands the file to create_instance): "seed" fixes the dice (the tests'; else the
 * clock's), "pattern" the one opened, "data_dir" where the store lives ("" none), "save_on_close" 0 or 1,
 * "draw_thread" 0 to draw in the render (the bench's measure of it), "clock_lag" in samples */
static int json_int(const char *js, const char *key, int dflt)
{
    if (!js) return dflt;
    const char *p = strstr(js, key);
    if (!p) return dflt;
    p += strlen(key);
    while (*p == ' ' || *p == '"' || *p == ':' || *p == '\t') p++;
    int ok;
    int v = s_snum(&p, &ok);
    return ok ? v : dflt;
}

static int json_str(const char *js, const char *key, char *o, int n)
{
    if (!js) return 0;
    const char *p = strstr(js, key);
    if (!p) return 0;
    p += strlen(key);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    if (*p++ != '"') return 0;
    int i = 0;
    while (*p && *p != '"' && i < n - 1) o[i++] = *p++;
    o[i] = 0;
    return 1;
}

/* the store from its file, at create (before any render: nothing else touches the core). A file the core refuses is
 * kept aside as .bad, and the factory plays */
static void store_load(void)
{
    char path[256];
    sv_path(path, sizeof path, SV_FILE);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    int max = 0;
    uint8_t *b = m2k_store_buf(&max);
    const size_t n = fread(b, 1, (size_t)max, f);
    fclose(f);
    if (n > 0 && m2k_store_unpack((int)n)) { mv.loaded = 1; return; }
    char bad[256];
    sv_path(bad, sizeof bad, SV_FILE ".bad");
    rename(path, bad);
}

static void *v2_create(const char *module_dir, const char *json_defaults)
{
    (void)module_dir;
    if (mv.live) return NULL;           /* (the core is one: a second would share the first's state and break it) */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint32_t clock_seed = (uint32_t)ts.tv_nsec ^ (uint32_t)ts.tv_sec * 2654435761u;
    const int seed = json_int(json_defaults, "\"seed\"", -1);
    const int pattern = json_int(json_defaults, "\"pattern\"", -1);

    mv_begin(seed >= 0 ? (uint32_t)seed : clock_seed);   /* the factory: bank A, A01 open, stopped (the glue's) */
    if (!json_str(json_defaults, "\"data_dir\"", mv.dir, sizeof mv.dir)) p_cat(mv.dir, "/data/UserData/schwung/mono2k", mv.dir + sizeof mv.dir - 1);
    mv.save_on_close = (uint8_t)(json_int(json_defaults, "\"save_on_close\"", 1) != 0);
    mv.draw_thread = (uint8_t)(json_int(json_defaults, "\"draw_thread\"", 1) != 0);
    ck.lag = json_int(json_defaults, "\"clock_lag\"", 64);
    if (mv.dir[0]) store_load();
    mv.syni = store_.set[S_SYNI];       /* (the store's own SYNC IN, kept in the file: mv_ready makes it Move's clock) */
    mv_ready(pattern);
    m_set(&cost, 0, sizeof cost);

    m_set(&sv, 0, sizeof sv);
    atomic_store(&sv.state, SV_IDLE);
    m_set(fbuf, 0, sizeof fbuf);
    m_set(fseq, 0, sizeof fseq);
    atomic_store(&fb_mid, 1u);
    fb_w = 0; fb_r = 2; fb_count = 0;
    vw_count = 0;
    vw_last = 0;
    atomic_store(&vw_lock, 0u);
    view_publish();
    atomic_store(&th_stop, 0);
    th_draw_on = mv.draw_thread && pthread_create(&th_draw, 0, draw_main, 0) == 0;
    th_save_on = mv.dir[0] && pthread_create(&th_save, 0, save_main, 0) == 0;
    if (!th_draw_on) frame_draw(&vw_now);
    mv.live = 1;
    return &mv;
}

static void v2_destroy(void *inst)
{
    if (inst != &mv || !mv.live) return;
    mv.live = 0;
    atomic_store(&th_stop, 1);
    if (th_draw_on) pthread_join(th_draw, 0);
    if (th_save_on) pthread_join(th_save, 0);   /* (a save under way is finished first) */
    th_draw_on = th_save_on = 0;
    /* closing keeps what was played (the owner's choice, module.json's save_on_close), and a save asked for and not
     * yet written: the core's own pack, nothing else running now */
    if (mv.dir[0] && (mv.save_on_close || sv.want || atomic_load(&sv.state) != SV_IDLE)) {
        if (sv.want && sv.reload) pattern_save();
        store_.set[S_SYNI] = mv.syni;
        const int n = m2k_store_pack();
        if (n > 0) sv_write_bytes(m2k_store_buf(&(int){0}), n);
    }
}

static void v2_on_midi(void *inst, const uint8_t *msg, int len, int source)
{
    if (inst != &mv || !mv.live || len < 1) return;
    const int st = msg[0];
    if (st >= 0xF8) {                  /* Move's transport, a byte (cable 0's, which the shim marks external) */
        if (st == 0xF8 || st == 0xFA || st == 0xFB || st == 0xFC) ev_push(EV_RT, st, 0);
        return;
    }
    if (len < 3 || source != MOVE_MIDI_SOURCE_INTERNAL) return;
    const int cmd = st & 0xF0, n = msg[1], v = msg[2];
    if (cmd != 0x90 && cmd != 0x80) return;
    const int on = cmd == 0x90 && v > 0;
    if (n >= NOTE_STEP1 && n < NOTE_STEP1 + 16) ev_push(EV_STEP, n - NOTE_STEP1, on);
    else if (n >= NOTE_PAD1 && n < NOTE_PAD1 + PADS) ev_push(EV_PAD, n - NOTE_PAD1, on ? v : -1);
}

/* "in": ui.js's controls, ';'-separated: "K i d" knob i turned d steps (ui.js's curve), "P i d c" knob i turned as the
 * tempo, d tenths of BPM (c 1: whole BPM, landing on one), "J d" the jog, "B cc v" a button's CC, "T i v" knob i
 * touched (1) or let go (0), "W n ok" ui.js wrote the tempo asked number n for Move (ok 0: it could not) */
static void in_parse(const char *s)
{
    while (*s) {
        while (*s == ' ' || *s == ';') s++;
        const char c = *s;
        if (!c) break;
        s++;
        int ok1 = 0, ok2 = 0, ok3 = 0, a = s_snum(&s, &ok1), b = 0, e = 0;
        if (c != 'J') b = s_snum(&s, &ok2);
        if (c == 'P') e = s_snum(&s, &ok3);
        if (c == 'K' && ok1 && ok2) ev_push(EV_KNOB, a, b);
        else if (c == 'P' && ok1 && ok2 && a >= 0 && a < 8) ev_push(EV_TEMPO, a | (ok3 && e ? 16 : 0), b);
        else if (c == 'W' && ok1 && ok2) ev_push(EV_WROTE, a & 255, b);
        else if (c == 'J' && ok1) ev_push(EV_JOG, 0, a);
        else if (c == 'B' && ok1 && ok2 && a >= 0 && a < 128) ev_push(EV_BTN, a, b);
        else if (c == 'T' && ok1 && ok2) ev_push(EV_TOUCH, a, b);
        while (*s && *s != ';') s++;
    }
}

static void v2_set_param(void *inst, const char *key, const char *val)
{
    if (inst != &mv || !mv.live || !key || !val) return;
    if (s_is(key, "in")) { in_parse(val); return; }
    const int v = s_num(val);
    if (s_is(key, "play")) {
        if (v == 0 || v == 1) mv.want_play = v;
    } else if (s_is(key, "track")) {
        if (v >= 0 && v < M2K_TRACKS) mv.want_track = v;
    }
}

static int put_out(char *buf, int buf_len, const char *o, int n)
{
    if (n > buf_len - 1) n = buf_len - 1;
    memcpy(buf, o, (size_t)n);
    buf[n] = 0;
    return n;
}

static int v2_get_param(void *inst, const char *key, char *buf, int buf_len)
{
    if (inst != &mv || !mv.live || !key || !buf || buf_len <= 0) return -1;
    static char o[FRAME_LEN + 64 > 4096 ? FRAME_LEN + 64 : 4096];
    if (s_is(key, "frame")) return put_out(buf, buf_len, o, frame_string(o));
    if (s_is(key, "ui")) return put_out(buf, buf_len, o, ui_string(o));
    if (s_is(key, "view")) return put_out(buf, buf_len, o, view_string(o));
    if (s_is(key, "dump")) return put_out(buf, buf_len, o, dump_string(o));
    if (s_is(key, "clock")) return put_out(buf, buf_len, o, clock_string(o));
    if (s_is(key, "cost")) return put_out(buf, buf_len, o, cost_string(o));
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

/* a block: the glue's (what came in since the last, then the core's render 16 samples at a time with Move's pulses
 * between), then the snapshot's next piece, the page state, the cost */
static void v2_render(void *inst, int16_t *out, int frames)
{
    if (inst != &mv || !mv.live || frames <= 0) {
        if (out && frames > 0) memset(out, 0, (size_t)frames * 4u);
        return;
    }
    const uint32_t t0 = now_us();
    mv_block(out, frames);
    sv_step();
    const uint32_t d = atomic_load(&sv.done), x = atomic_load(&sv.failed);
    if (d != sv.seen_done) { sv.seen_done = d; toast("SAVED", "MONO2K.M2KS"); mv.touched = 1; }
    if (x != sv.seen_failed) { sv.seen_failed = x; toast("NOT SAVED", "FILE NOT WRITTEN"); mv.touched = 1; }
    mv.pending |= mv.touched;
    const uint32_t since = mv.blocks - vw_last;
    if (since >= VW_EVERY || (mv.pending && since >= VW_SOON)) {
        view_publish();
        vw_last = mv.blocks;
        mv.pending = 0;
        if (!th_draw_on && !(++mv.nodraw & 1u)) frame_draw(&vw_now);   /* (no drawer: here, every other publish) */
    }
    mv.touched = 0;
    const uint32_t dt = now_us() - t0;
    mv.cpu_sum += dt;
    if (dt > mv.cpu_max) mv.cpu_max = dt;
    if (++mv.cpu_n == 128u) {
        mv.cpu_mean = mv.cpu_sum / 128u;
        mv.cpu_most = mv.cpu_max;
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
