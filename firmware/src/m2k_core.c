/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K: the unity build of the core. Every unit is included here, so everything can stay static. */
#include "m2k.h"
#include "m2k_tables.h"
#include "m2k_waves64.h"
#include "m2k_fonts.h"
#include "m2k_icons.h"

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

/* a word at a time where both ends are aligned (the FM-1 builds with -fno-builtin: no memset to fall back on, and the
 * render clears and copies blocks of words every 16 samples) */
static void m_set(void *d, int v, uint32_t n)
{
    uint8_t *p = d;
    if (!(((uintptr_t)p | n) & 3u)) {
        uint32_t w = (uint8_t)v * 0x01010101u, *q = (uint32_t *)(void *)p;
        for (n >>= 2; n; n--) *q++ = w;
        return;
    }
    while (n--) *p++ = (uint8_t)v;
}
static void m_cpy(void *d, const void *s, uint32_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    if (!(((uintptr_t)p | (uintptr_t)q | n) & 3u)) {
        uint32_t *pw = (uint32_t *)(void *)p;
        const uint32_t *qw = (const uint32_t *)(const void *)q;
        for (n >>= 2; n; n--) *pw++ = *qw++;
        return;
    }
    while (n--) *p++ = *q++;
}

static uint32_t rng = 0x2000u;
static inline uint32_t rnd32(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static inline int32_t sat16(int32_t x) { return x > 32767 ? 32767 : x < -32767 ? -32767 : x; }

static inline int32_t sinp(uint32_t ph)
{
    uint32_t i = ph >> 22, f = (ph >> 6) & 0xFFFFu;
    int32_t a = T_SIN[i], b = T_SIN[i + 1];
    return a + (((b - a) * (int32_t)f) >> 16);
}

/* pitch in 1/64 semitones (MIDI note * 64) to a phase increment per sample */
static inline uint32_t inc_of(int32_t pq6)
{
    if (pq6 < 0) pq6 = 0;
    if (pq6 > 140 * 64) pq6 = 140 * 64;
    int32_t oct = pq6 / 768, r = pq6 - oct * 768;
    int32_t sh = 10 - oct;
    if (sh >= 0) return T_INC_TOP[r] >> sh;
    uint32_t v = T_INC_TOP[r];
    return v > (0x7FFFFFFFu >> (-sh - 1)) ? 0x7FFFFFFFu : v << -sh;
}

/* 64-bit products. On the FM-1 a 32 x 32 -> 64-bit multiply inlined beside a load now and then leaves a register a
 * bit off (felucca-bsp docs/CPU.md): there (M2K_MUL_OUT) these are out of line and use the product at once (their
 * shift), so the multiply completes before any load; tools/fm1/build.py fails a build with a 64-bit multiply beside a
 * load anywhere. Elsewhere they inline, the same results. A product that provably fits 32 bits is a plain multiply */
#ifdef M2K_MUL_OUT
#define MUL_FN static __attribute__((noinline))
#else
#define MUL_FN static inline
#endif
MUL_FN int64_t mul_sh(int32_t a, int32_t b, int sh) { return ((int64_t)a * b) >> sh; }
MUL_FN uint32_t mulu_hi(uint32_t a, uint32_t b, int sh) { return (uint32_t)(((uint64_t)a * b) >> sh); }
MUL_FN int64_t mul64_sh(int64_t a, int32_t b, int sh) { return (a * b) >> sh; }
/* a b + c d: out of the helper shifted up two (the shift its use), back here (|a b + c d| < 2^61) */
MUL_FN uint64_t dot_x4(int32_t a, int32_t b, int32_t c, int32_t d)
{
    return (uint64_t)((int64_t)a * b + (int64_t)c * d) << 2;
}
static inline int64_t dot(int32_t a, int32_t b, int32_t c, int32_t d) { return (int64_t)dot_x4(a, b, c, d) >> 2; }
/* s a >> 15 and s b >> 15 in one call, each its low word (as a cast to int32_t takes it), the second's high; whole
 * when |a|, |b| < 2^15 */
MUL_FN uint64_t mul2_q15(int32_t s, int32_t a, int32_t b)
{
    return (uint64_t)(uint32_t)(((int64_t)s * b) >> 15) << 32 | (uint32_t)(((int64_t)s * a) >> 15);
}

/* soft saturation: -131072..131072 in, full scale out */
static inline int32_t sat_soft(int32_t x)
{
    if (x >= 131071) return 32767;
    if (x <= -131071) return -32767;
    int32_t u = x + 131072, i = u >> 10, f = u & 1023;
    return T_SAT[i] + (((T_SAT[i + 1] - T_SAT[i]) * f) >> 10);
}

/* the bus limiter: straight to 3/4 of full scale, then a knee; -131072..131072 in */
static inline int32_t lim_soft(int32_t x)
{
    if ((uint32_t)x + 17407u <= 17407u + 16384u) return x;   /* the table is exactly x there (-17407 .. 16384) */
    if (x >= 131071) return 32767;
    if (x <= -131071) return -32767;
    int32_t u = x + 131072, i = u >> 10, f = u & 1023;
    return T_LIM[i] + (((T_LIM[i + 1] - T_LIM[i]) * f) >> 10);
}

/* the state the screen reads from the audio side */
static struct {
    int32_t peak[M2K_TRACKS];          /* 0..32767, decays in the UI */
    uint8_t trig[M2K_TRACKS];          /* counts up on every trig: the UI flashes on change */
    int32_t aenv[M2K_TRACKS];          /* Q15 */
    uint8_t astage[M2K_TRACKS];
    int32_t cut_hp[M2K_TRACKS], cut_lp[M2K_TRACKS];   /* q3 cutoff index, live */
    int32_t lfo[M2K_TRACKS][3];        /* -32767..32767 */
    uint8_t glitch;                    /* 1 while a PERFORM effect plays from the glitch's ring */
    int32_t dly[M2K_TRACKS];           /* delay return level */
    uint8_t lfo_view;                  /* the track the screen shows: its LFOs are worked out though they move nothing */
} mon;

/* a profile (M2K_PROF, host/m2k_prof.c and the FM-1's test build): the time each part of the render takes, in the
 * platform's own ticks: 0 the sequencer and the LFOs, 1 the machines (and by machine), 2 the track chain, 3 the
 * delays, 4 the bus */
#ifdef M2K_PROF
uint32_t m2k_prof_now(void);
uint32_t m2k_prof[8], m2k_prof_m[M_COUNT];
static uint32_t prof_t;
#define PROF_START() (prof_t = m2k_prof_now())
#define PROF(k) do { uint32_t n_ = m2k_prof_now(); m2k_prof[k] += n_ - prof_t; prof_t = n_; } while (0)
#define PROF_M(m) do { uint32_t n_ = m2k_prof_now(); m2k_prof[1] += n_ - prof_t; m2k_prof_m[m] += n_ - prof_t; prof_t = n_; } while (0)
#else
#define PROF_START() ((void)0)
#define PROF(k) ((void)0)
#define PROF_M(m) ((void)0)
#endif

#include "m2k_dsp.c"
#include "m2k_fx.c"
#include "m2k_grids.c"
#include "m2k_seq.c"
#include "m2k_gfx.c"
#include "m2k_ui.c"
