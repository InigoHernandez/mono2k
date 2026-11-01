/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the glue every build of it shares. The Move's dsp.so (move/m2k_move.c: Schwung's plugin API, the
 * saver, the drawer, the frames ui.js reads) and the site's playable Move (web/move/m2k_move_web.c: the same, in an
 * AudioWorklet) both include it after the core (firmware/src/m2k_core.c), so the two run one controller on one clock:
 *   the queue      every press and turn, in the order it came, drained into the controller before each block;
 *   the clock      Move's transport and tempo followed through the core's clock in, and the tempo asked of Move
 *                  (a build with no Move leaves ck.sync at MV_SYNC_INT: MONO2K's own clock, Play just plays);
 *   the block      the queue drained, the core rendered 16 samples at a time with Move's pulses between (mv_block);
 *   then the controller (m2k_move_ui.c) and the 128 x 64 screen (screen/m2k_screen.c).
 * The including build gives: mv_host_bpm (Move's tempo, 0 none) and mv_save_request (a save asked for: written, or
 * not, as the build keeps things). One instance: the core is a set of statics. */
#include <string.h>
#include "m2k_move_view.h"

#define SR         44100.0

static float mv_host_bpm(void);         /* Move's own tempo now (Schwung's get_bpm), 0 none */
static void mv_save_request(int reload);

/* ------------------------------------------------------------------------------------------------ the queue */

/* every press and turn, from on_midi and set_param to the next render, in the order they came. All three run on the
 * audio thread, one after another, so the queue needs no atomics; 256 is more than hands make in a block, and a press
 * past it is dropped and counted */
enum { EV_STEP, EV_PAD, EV_KNOB, EV_JOG, EV_BTN, EV_TOUCH, EV_RT, EV_TEMPO, EV_WROTE };
#define EV_N 256u
static struct {
    uint8_t kind[EV_N], a[EV_N];
    int16_t b[EV_N];
    uint32_t w, r, dropped;
} evq;

static void ev_push(int kind, int a, int b)
{
    if (evq.w - evq.r >= EV_N) { evq.dropped++; return; }
    evq.kind[evq.w % EV_N] = (uint8_t)kind;
    evq.a[evq.w % EV_N] = (uint8_t)a;
    evq.b[evq.w % EV_N] = (int16_t)CLAMP(b, -32767, 32767);
    evq.w++;
}

/* --------------------------------------------------------------------------------------------- the instance */

static struct {
    int live;
    uint64_t samples;                  /* frames rendered since create */
    uint32_t blocks;
    int want_play, want_track;         /* set_param's "play" and "track" (-1 nothing): the bench's, the tools' */
    uint8_t touched, pending;          /* the queue fed the controller this block; a publish owed since */
    uint32_t nodraw;                   /* publishes, when the render draws (no drawer thread) */
    uint32_t cpu_sum, cpu_max, cpu_n, cpu_mean, cpu_most;
    char dir[200];                     /* where the store lives ("" none: nothing read or written) */
    uint8_t save_on_close, loaded, draw_thread;
    uint8_t syni;                      /* the store's own SYNC IN (the FM-1's port choice), kept as it was in the file */
} mv;

/* ------------------------------------------------------------------------------------------------ the clock */

/* MONO2K follows Move's transport and tempo through the core's own clock in (m2k_midi_from: SYIN's source, here
 * USB). Move's pulses come once a block, each up to a block (2.9 ms) after it happened, so they are not fed as they
 * come: a phase follows Move's count of pulses, gently (a correction of 1/32 of the error a pulse, and an integrator
 * for a tempo slightly off), and the core is given each pulse at the 16-sample block where it falls, its period pinned
 * to Move's steady tempo (host->get_bpm: Move's own number, held; or the pulses measured over a window and held when
 * the host has none), so the tempo, the swing, the gates, the LFOs and the synced delays never wobble. Start, continue
 * and stop are Move's, passed on at once. Move has PLAY (module.json's button_passthrough): a press MONO2K sees with no
 * answer from Move within ~70 ms (no clock: no set, or a setting) plays MONO2K on its own clock, shown as INT */
#define CK_KP (1.0 / 32)
#define CK_KI (1.0 / 2048)
#define CK_SNAP 6.0                    /* pulses off: a jump, not a drift (taken at once) */
#define CK_STALE (SR / 2)              /* no pulse for half a second while running: Move has gone */
#define CK_PLAY_WAIT 24                /* blocks (70 ms) for Move to answer PLAY */
#define CK_ANSWERED (SR * 0.15)        /* a transport message this recent answered the PLAY just seen */
#define CK_WIN 97                      /* pulses measured, when the host gives no tempo (4 beats) */
#define CK_SLEW (1.0 / 16)             /* a re-lock's correction a pulse at most: MONO2K ~6 % off Move's tempo meanwhile */
#define CK_BAR 96                      /* a bar's pulses (4/4): a re-lock after a free run lands on Move's bar */
#define CK_LOCKED 0.05                 /* pulses off: the re-lock is done, the gentle lock again */
static struct {
    uint8_t sync, run, anchored;       /* MV_SYNC_*; Move's transport running; its first pulse seen */
    uint8_t free;                      /* the phase runs free at MONO2K's own tempo (the tempo asked of Move, or alone) */
    uint8_t relock;                    /* back to Move's phase after a free run: 1 to begin (the bar picked), 2 under way */
    int32_t off;                       /* MONO2K's pulses ahead of Move's, whole bars (a re-lock's: 0 otherwise) */
    int32_t nrx, ntx;                  /* Move's pulses since its start: seen, and given to the core */
    double ph, fc, inc;                /* Move's pulses since its start now; the integrator; pulses a sample */
    double lag;                        /* samples a pulse is seen after it happened, on average (half a block) */
    double bpm, err;                   /* the tempo played (Move's, or the one asked of it); the last phase error, pulses */
    double move_bpm;                   /* Move's own tempo now (the host's, else measured), 0 none */
    uint32_t per_q8;                   /* the core's period pinned (Q8 samples a pulse), 0 none */
    uint64_t last_rx, last_tr;         /* the last pulse, the last start / continue / stop (tr_any: one came) */
    uint8_t tr_any;
    int play_wait;
    /* the tempo measured, when the host gives none */
    uint64_t at[CK_WIN];
    int nat, hat;
    uint8_t settled;
    double held;
    /* each step's start, for the bench */
    uint32_t n1x_seen, steps;
    uint64_t step_at;
} ck;

/* the tempo asked of Move (the tempo knob under SYNC: tq_ask). MONO2K plays it at once, its phase free, and ui.js
 * writes it to /data/UserData/schwung/desired-tempo, which Schwung's Link sidecar gives the Link session (Move alone in
 * it), so Move follows. When Move's tempo reaches it MONO2K follows Move again, its phase brought back to Move's
 * gently (ck.relock). If Move has not followed TQ_WAIT_S after the write (no Link on Move, Move->Schwung off, another
 * peer in the session) MONO2K stays at it alone (INT), started and stopped by Move's PLAY still, until Move's tempo
 * next changes or Delete + the tempo knob: then it follows Move again (the owner's: a tempo set must not be lost at
 * the next PLAY when Move has no Link) */
#define TQ_TOL 0.05                    /* BPM: Move has followed */
#define TQ_WAIT_S 2.5                  /* seconds after ui.js wrote it */
#define TQ_NOWRITE_S 4.0               /* seconds after the last turn, if ui.js said nothing of a write */
#define TQ_MIN 200                     /* tenths: the tempo asked between 20.0 */
#define TQ_MAX 3000                    /* and 300.0 BPM */
static struct {
    uint8_t state, seq, wseq;          /* MV_TQ_*; the request's number (1..255, 0 never); the one ui.js wrote */
    uint8_t nofile;                    /* ui.js could not write it */
    uint16_t b10;                      /* the tempo asked, tenths */
    uint64_t at, wrote;                /* samples: the last change; ui.js's write of it (0 not yet) */
    double move0;                      /* Move's tempo when MONO2K was left alone (a change re-syncs) */
    int32_t acc10;                     /* tenths turned under INT, towards the pattern's whole BPM */
    uint32_t asks, follows, alones;    /* (the bench's counts) */
} tq;

static int mv_following(void) { return ck.run && mx.ext; }
static int mv_sync(void) { return ck.sync == MV_SYNC_INT || tq.state == MV_TQ_ALONE ? MV_SYNC_INT : MV_SYNC_MOVE; }
static uint32_t mv_bpm10(void)
{
    if (ck.sync == MV_SYNC_MOVE && tq.state != MV_TQ_NONE) return tq.b10;
    if (ck.sync == MV_SYNC_MOVE && ck.bpm > 0) return (uint32_t)(ck.bpm * 10 + 0.5);
    return seq_bpm() * 10u;
}

static void ck_measure_reset(void) { ck.nat = ck.hat = 0; ck.settled = 0; }

/* the tempo from the pulses when the host gives none (Schwung's own way, shadow_transport.c): a least-squares line
 * through the window's pulses (one seen a block late moves it 1/n as much as at an end), held: taken when the window
 * first fills, then only when it disagrees by more than its ends can be wrong (three blocks over its span); before,
 * from half a beat, only a tempo 2 % off (a stop at another tempo) */
static double ck_fit(int n)
{
    int i = (ck.hat + CK_WIN - n) % CK_WIN;
    const uint64_t t0 = ck.at[i];
    double st = 0, sit = 0;
    for (int k = 0; k < n; k++, i = (i + 1) % CK_WIN) {
        const double t = (double)(ck.at[i] - t0);
        st += t;
        sit += k * t;
    }
    const double si = n * (n - 1) / 2.0, sii = (double)(n - 1) * n * (2 * n - 1) / 6.0;
    return (n * sit - si * st) / (n * sii - si * si);
}

static void ck_measure(uint64_t now)
{
    ck.at[ck.hat] = now;
    ck.hat = (ck.hat + 1) % CK_WIN;
    if (ck.nat < CK_WIN) ck.nat++;
    if (ck.nat < 13) return;
    const double spt = ck_fit(ck.nat);
    if (spt <= 0) return;
    const double bpm = 60.0 * SR / (24.0 * spt), off = ck.held > 0 ? bpm / ck.held - 1 : 1;
    if (bpm < 20 || bpm > 999) return;
    const int full = ck.nat == CK_WIN;
    const double band = ck.nat < 25 ? 0.02 : 3.0 * 128.0 / (spt * (ck.nat - 1));
    if (off > band || off < -band || (full && !ck.settled)) ck.held = bpm;
    if (full) ck.settled = 1;
}

static void ck_start(int st, uint64_t now)
{
    ck.sync = MV_SYNC_MOVE;
    ck.run = 1;
    ck.anchored = 0;
    ck.nrx = ck.ntx = 0;
    ck.fc = 0;
    ck.off = 0;
    ck.relock = 0;
    ck.free = tq.state != MV_TQ_NONE;  /* (waiting for Move, or alone: MONO2K at the tempo asked, from the start) */
    ck.play_wait = 0;
    ck.last_rx = ck.last_tr = now;
    ck.tr_any = 1;
    ck_measure_reset();
    m2k_midi_from(M2K_USB, st, 0, 0);
}

static void ck_stop(void)
{
    ck.run = 0;
    ck.anchored = 0;
    ck.relock = 0;
    ck.play_wait = 0;
    ck_measure_reset();
    m2k_midi_from(M2K_USB, 0xFC, 0, 0);
}

static void ck_rt(int st, uint64_t now)
{
    if (st == 0xFA || st == 0xFB) { ck_start(st, now); return; }
    if (st == 0xFC) { ck.sync = MV_SYNC_MOVE; ck.last_tr = now; ck.tr_any = 1; ck_stop(); return; }
    if (st != 0xF8) return;
    ck.sync = MV_SYNC_MOVE;
    ck_measure(now);
    ck.last_rx = now;
    if (!ck.run) return;               /* (a clock without a start: its tempo, but nothing starts) */
    ck.nrx++;
    const double obs = (ck.nrx - 1) + ck.lag * ck.inc * (1 + ck.fc);   /* pulse nrx - 1, seen about lag ago */
    if (!ck.anchored) { ck.anchored = 1; ck.ph = obs + ck.off; ck.err = 0; return; }
    if (ck.relock == 1) {              /* after a free run: Move's bar nearest MONO2K's phase (at most half a bar away) */
        const double bars = (ck.ph - obs) / CK_BAR;
        ck.off = CK_BAR * (int32_t)(bars < 0 ? bars - 0.5 : bars + 0.5);
        ck.relock = 2;
    }
    const double e = obs + ck.off - ck.ph;
    ck.err = e;
    if (ck.free) return;               /* (MONO2K's own tempo: Move's pulses only counted) */
    if (ck.relock) {                   /* back to Move's phase without a jump: a capped nudge a pulse, no integrator */
        ck.ph += CLAMP(CK_KP * e, -CK_SLEW, CK_SLEW);
        if (e < CK_LOCKED && e > -CK_LOCKED) ck.relock = 0;
        return;
    }
    if (e > CK_SNAP || e < -CK_SNAP) { ck.ph = obs + ck.off; return; }
    ck.ph += CK_KP * e;
    ck.fc += CK_KI * e;
    ck.fc = ck.fc > 0.02 ? 0.02 : ck.fc < -0.02 ? -0.02 : ck.fc;
}

/* the tempo played, each block: the one asked of Move while it is (MONO2K's at once), else Move's: the host's (steady
 * by its contract), else the pulses measured */
static void ck_tempo(void)
{
    double b = 0;
    { const float f = mv_host_bpm(); if (f >= 20.0f && f <= 999.0f) b = f; }
    if (b <= 0) b = ck.held;
    ck.move_bpm = b;
    if (ck.sync == MV_SYNC_MOVE && tq.state != MV_TQ_NONE) b = tq.b10 / 10.0;
    if (b <= 0) { ck.inc = 0; ck.per_q8 = 0; return; }
    if (b != ck.bpm) { ck.bpm = b; ck.per_q8 = (uint32_t)(28224000.0 / b + 0.5); }
    ck.inc = b * 24.0 / (60.0 * SR);
}

/* a PLAY press seen (ui.js): Move answers it with a start or a stop when it sends its clock; else MONO2K plays alone */
static void mv_play_press(void)
{
    if (ck.sync == MV_SYNC_INT) { seq_play(!sq.playing); return; }
    if (ck.tr_any && mv.samples - ck.last_tr < (uint64_t)CK_ANSWERED) return;   /* (Move's start or stop came first) */
    ck.play_wait = CK_PLAY_WAIT;
}

/* the pulses due in the 16 samples about to render, at the block where each falls (a pulse due by the block's middle
 * goes in at its start) */
static void ck_pulses(void)
{
    if (!ck.run || !ck.anchored) return;
    if (ck.inc <= 0) {                 /* (no tempo yet: as they come) */
        for (; ck.ntx < ck.nrx; ck.ntx++) m2k_midi_from(M2K_USB, 0xF8, 0, 0);
        return;
    }
    const double half = 8.0 * ck.inc * (ck.free ? 1 : 1 + ck.fc);
    const int32_t cap = ck.free || ck.relock ? INT32_MAX - 1 : ck.nrx + ck.off + 1;   /* (locked: never ahead of Move) */
    for (int k = 0; k < 3 && ck.ntx <= cap && ck.ph + half >= ck.ntx; k++, ck.ntx++)
        m2k_midi_from(M2K_USB, 0xF8, 0, 0);
}

/* ------------------------------------------------------------------------------------- the tempo asked of Move */

/* MONO2K's phase back to Move's: at the next pulse its bar is picked, then nudged there (ck_rt) */
static void tq_relock(void)
{
    if (!ck.free) return;
    ck.free = 0;
    if (ck.run && ck.anchored) ck.relock = 1;
}

/* the tempo knob under SYNC: d10 tenths (coarse: whole BPM, landing on one). MONO2K at it now; ui.js asks Move */
static void tq_ask(int d10, int coarse)
{
    const int base = (int)mv_bpm10();
    int b = base + d10;
    if (coarse) b = (d10 > 0 ? base / 10 * 10 : (base + 9) / 10 * 10) + d10;
    b = CLAMP(b, TQ_MIN, TQ_MAX);
    if (b == base && tq.state == MV_TQ_NONE) return;
    tq.b10 = (uint16_t)b;
    tq.seq = (uint8_t)(tq.seq == 255 ? 1 : tq.seq + 1);
    tq.at = mv.samples;
    tq.wrote = 0;
    tq.nofile = 0;
    tq.state = MV_TQ_WAIT;
    tq.asks++;
    ck.free = 1;                       /* (MONO2K's phase its own until Move has followed) */
    ck.relock = 0;
    ck_tempo();
}

/* Delete + the tempo knob: Move's tempo again (the user asks) */
static void tq_cancel(void)
{
    if (tq.state == MV_TQ_NONE) return;
    tq.state = MV_TQ_NONE;
    tq_relock();
    ck_tempo();
}

/* ui.js wrote request SEQ (ok: the write worked) */
static void tq_written(int seq, int ok)
{
    if (tq.state != MV_TQ_WAIT || seq != tq.seq) return;
    tq.wseq = (uint8_t)seq;
    tq.wrote = mv.samples + 1;         /* (0: not yet) */
    tq.nofile = (uint8_t)!ok;          /* (no file written: Move cannot follow, no wait) */
}

/* each block: Move has followed (MONO2K follows it again), or not in time (alone, if playing), or, alone, Move's tempo
 * moved (follow it again) */
static void tq_tick(void)
{
    if (ck.sync != MV_SYNC_MOVE || tq.state == MV_TQ_NONE) return;
    const double mb = ck.move_bpm, want = tq.b10 / 10.0;
    if (tq.state == MV_TQ_WAIT) {
        if (mb > 0 && mb - want <= TQ_TOL && want - mb <= TQ_TOL) {
            tq.state = MV_TQ_NONE;
            tq.follows++;
            tq_relock();
            ck_tempo();
            return;
        }
        const int late = tq.nofile || (tq.wrote ? mv.samples + 1 - tq.wrote > (uint64_t)(TQ_WAIT_S * SR)
                                                : mv.samples - tq.at > (uint64_t)(TQ_NOWRITE_S * SR));
        if (!late) return;
        tq.alones++;
        mv.touched = 1;
        if (sq.playing && ck.run) {    /* playing: on alone at it, INT */
            tq.state = MV_TQ_ALONE;
            tq.move0 = mb;
            toast("INT", "MOVE: LINK OFF");
        } else {                       /* stopped: nothing to keep; Move's tempo (its start brings it) */
            tq.state = MV_TQ_NONE;
            ck.free = 0;
            ck_tempo();
            toast("MOVE: LINK OFF", "MOVE KEEPS ITS TEMPO");
        }
        return;
    }
    if (mb > 0 && (mb - tq.move0 > TQ_TOL || tq.move0 - mb > TQ_TOL)) {   /* ALONE: Move's tempo changed */
        tq.state = MV_TQ_NONE;
        tq_relock();
        ck_tempo();
        mv.touched = 1;
    }
}

/* ---------------------------------------------------------------------------------- the controller, the screen */

#include "m2k_move_ui.c"
#include "screen/m2k_screen.c"

/* the queue into the controller, and Move's transport into the clock, in the order they came */
static void ev_drain(void)
{
    for (; evq.r != evq.w; evq.r++) {
        const int k = evq.kind[evq.r % EV_N], a = evq.a[evq.r % EV_N], b = evq.b[evq.r % EV_N];
        if (k == EV_RT) { ck_rt(a, mv.samples); continue; }
        if (k == EV_WROTE) { tq_written(a, b != 0); continue; }
        mv.touched = 1;
        syn_sub_check();
        switch (k) {
        case EV_STEP: mv_step(a, b); break;
        case EV_PAD: mv_pad(a, b < 0 ? 0 : b, b >= 0); break;
        case EV_KNOB: mv_knob(a, b); break;
        case EV_TEMPO: mv_tempo_knob(a & 15, b, a >> 4); break;
        case EV_JOG: mv_jog(b); break;
        case EV_BTN: mv_button(a, b); break;
        case EV_TOUCH: mv_touch(a, b); break;
        }
        syn_sub_check();
    }
}

/* ------------------------------------------------------------------------------------------- the instance */

/* the instance cleared and the core at the factory (bank A, A01 open, stopped), Move's tempo the rule: before anything
 * of the build's own (its store, its settings) */
static void mv_begin(uint32_t seed)
{
    m_set(&evq, 0, sizeof evq);
    m_set(&mv, 0, sizeof mv);
    m_set(&ck, 0, sizeof ck);
    m_set(&tq, 0, sizeof tq);
    mv.want_play = mv.want_track = -1;
    ck.lag = 64;
    ck.sync = MV_SYNC_MOVE;
    m2k_par_begin = 0;                  /* one core for now: the render does every track and the bus itself */
    m2k_par_end = 0;
    m2k_clock = 0;
    m2k_init(seed);
}

/* after the build's store: the clock in is Move's (fed as USB's), no splash, pattern n open (-1 the store's), the
 * controller and the screen ready */
static void mv_ready(int pattern)
{
    store_.set[S_SYNI] = 48;            /* (the FM-1's port choice: a build keeping a store keeps its own aside) */
    ui.splash_end = 0;                  /* (no splash) */
    ui.splashed = 1;
    if (pattern >= 0 && pattern < M2K_PATTERNS) m2k_pattern(pattern);
    ui.toast_t = 0;                     /* (the pattern opened says so in the header) */
    mv_ctl_init();
    m2k_screen_init();                  /* (every kind's values swept here, never in a render) */
}

/* a block of FRAMES (Schwung's are 128: 8 of the core's 16): what came in since the last, then the core's render 16
 * samples at a time with Move's pulses between. The build then does its own: the saver's piece, the page state */
static void mv_block(int16_t *out, int frames)
{
    ui.now = (uint32_t)(mv.samples * 10u / 441u);       /* the UI's milliseconds (its toasts, its taps, the undo) */
    if (mv.want_track >= 0) { track_to(mv.want_track); mv.want_track = -1; mv.touched = 1; }
    if (mv.want_play >= 0) { seq_play(mv.want_play); mv.want_play = -1; }
    ck_tempo();
    ev_drain();
    tq_tick();
    mv_tick();
    if (ck.run && mv.samples - ck.last_rx > (uint64_t)CK_STALE) ck_stop();
    if (ck.play_wait && --ck.play_wait == 0) {          /* no answer from Move: MONO2K's own clock */
        ck.sync = MV_SYNC_INT;
        tq.state = MV_TQ_NONE;                          /* (a tempo asked of Move meanwhile: moot, no clock) */
        ck.free = 0;
        seq_play(!sq.playing);
        mv.touched = 1;
    }
    const int n = frames / M2K_SUB * M2K_SUB;   /* (Schwung's blocks are 128 frames: 8 of the core's) */
    const double inc = ck.inc * (ck.free ? 1 : 1 + ck.fc);   /* (free: MONO2K's own tempo, exactly) */
    for (int f = 0; f < n; f += M2K_SUB) {
        ck_pulses();
        if (ck.run && ck.per_q8 && mx.ext) mx.per_q8 = ck.per_q8;   /* (Move's tempo: steady, not the pulses' times) */
        m2k_render(out + 2 * f, M2K_SUB);
        if (sq.n1x != ck.n1x_seen) { ck.n1x_seen = sq.n1x; ck.steps = sq.n1x; ck.step_at = mv.samples + (uint64_t)f; }
        if (ck.run && ck.anchored) ck.ph += M2K_SUB * inc;
    }
    if (n < frames) memset(out + 2 * n, 0, (size_t)(frames - n) * 4u);
    mv.samples += (uint64_t)frames;
    mv.blocks++;
    if (m2k_dirty()) mv_save_request(0);   /* (the core's own rule: a setting turned, saved once it stands) */
}
