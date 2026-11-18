/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: GRIT and the chip's reverb (firmware/src/m2k_spu.c, M2K_SPU) tested and measured on their own,
 * before the bus calls them (phase 1, docs/spu-move-plan.md). Built as the Move's host is, eight tracks:
 *   cc -O2 -Wall -Wno-unused-function -DM2K_TRACKS=8 -DM2K_SPU -Ifirmware/src -Ifirmware/gen \
 *      -o build/host/m2k_spu_test host/m2k_spu_test.c -lm && build/host/m2k_spu_test
 * no argument: the self-test (exit 1 on a failure; run it under ASan + UBSan too); --rt: each type's RT60 and spectrum,
 * the FDN's beside them; --level: each type's level against the FDN's on bank A's sends (tools/gen_spu.py's GO);
 * --ir DIR: each type's impulse response as a WAV (--ir-hann DIR: its answer to a 16-sample Hann pulse, to compare
 * waveforms); --measure WAV ...: a WAV's, measured the same way (AFFINE's, say);
 * --render DIR: the owner's renders (bank A on the eight-track build, the SPU reverb in the FDN's place, GRIT on the
 * bus's output, both as phase 2 wires them); --bench: their cost in Mac instructions a sample, built as the FM-1 builds
 * the core (CLAUDE.md: add -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT) */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
#endif
#include "m2k_core.c"
#ifndef M2K_SPU
#error "the Move's build: -DM2K_SPU -DM2K_TRACKS=8"
#endif

#define FS 44100
#define FR(sec) ((int)((sec) * FS) / 128 * 128)   /* whole blocks */
static int fails;
static void check(int ok, const char *what, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void check(int ok, const char *what, const char *fmt, ...)
{
    va_list ap;
    printf("%s %-34s ", ok ? "ok  " : "FAIL", what);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    if (!ok) fails++;
}

static void spu_reset(void) { spu_init(); }

static void wav_write(const char *path, const int16_t *s, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t data = (uint32_t)frames * 4u, riff = 36 + data, sr = FS, br = sr * 4, fl = 16;
    uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fl, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f);
    fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(s, 4, (size_t)frames, f);
    fclose(f);
}

/* a 16-bit stereo WAV's frames (its data chunk found), or 0 */
static int16_t *wav_read(const char *path, int *frames)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return 0; }
    for (;;) {
        uint8_t c[8];
        if (fread(c, 1, 8, f) != 8) { fclose(f); return 0; }
        uint32_t n = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "data", 4)) {
            int16_t *s = malloc(n);
            *frames = (int)(fread(s, 1, n, f) / 4);
            fclose(f);
            return s;
        }
        fseek(f, (long)(n + (n & 1)), SEEK_CUR);
    }
}

/* ------------------------------------------------------------------------------------------- measurement ---- */

/* a biquad (RBJ's cookbook), double: the measurement's alone */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } bq_t;
static void bq_set(bq_t *q, int type, double f, double Q)
{
    double w = 2 * M_PI * f / FS, c = cos(w), al = sin(w) / (2 * Q), a0 = 1 + al;
    if (type == 0) { q->b0 = (1 - c) / 2; q->b1 = 1 - c; q->b2 = (1 - c) / 2; }     /* low-pass */
    else if (type == 1) { q->b0 = (1 + c) / 2; q->b1 = -(1 + c); q->b2 = (1 + c) / 2; }   /* high-pass */
    else { q->b0 = al; q->b1 = 0; q->b2 = -al; }                                    /* band-pass, 0 dB peak */
    q->b0 /= a0; q->b1 /= a0; q->b2 /= a0; q->a1 = -2 * c / a0; q->a2 = (1 - al) / a0;
    q->z1 = q->z2 = 0;
}
static double bq_run(bq_t *q, double x)
{
    double y = q->b0 * x + q->z1;
    q->z1 = q->b1 * x - q->a1 * y + q->z2;
    q->z2 = q->b2 * x - q->a2 * y;
    return y;
}
/* the band lo..hi of x (two-pole high- and low-passes, twice each), into y */
static void band(const double *x, double *y, int n, double lo, double hi)
{
    bq_t q[4];
    bq_set(&q[0], 1, lo, 0.7071); bq_set(&q[1], 1, lo, 0.7071);
    bq_set(&q[2], 0, hi, 0.7071); bq_set(&q[3], 0, hi, 0.7071);
    for (int i = 0; i < n; i++) {
        double v = x[i];
        if (lo > 0) v = bq_run(&q[1], bq_run(&q[0], v));
        if (hi < FS / 2) v = bq_run(&q[3], bq_run(&q[2], v));
        y[i] = v;
    }
}
/* RT60 from Schroeder's backward integral: the line fitted from -5 to -35 dB (T30), or to -25 (T20) where the decay
 * stops short of -35 (the IR ended); -1 when it never falls 25 dB */
static double rt60_of(const double *x, int n)
{
    double *e = malloc(sizeof(double) * (size_t)n), s = 0;
    for (int i = n - 1; i >= 0; i--) { s += x[i] * x[i]; e[i] = s; }
    double top = e[0], rt = -1;
    if (top <= 0) { free(e); return -1; }
    for (int lim = 35; lim >= 25 && rt < 0; lim -= 10) {
        int a = -1, b = -1;
        for (int i = 0; i < n; i++) {
            double l = 10 * log10(e[i] / top + 1e-300);
            if (a < 0 && l <= -5) a = i;
            if (l <= -lim) { b = i; break; }
        }
        if (a < 0 || b <= a) continue;
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        int m = 0;
        for (int i = a; i <= b; i += 4, m++) {
            double t = (double)i / FS, l = 10 * log10(e[i] / top);
            sx += t; sy += l; sxx += t * t; sxy += t * l;
        }
        double sl = (m * sxy - sx * sy) / (m * sxx - sx * sx);
        if (sl < 0) rt = -60 / sl;
    }
    free(e);
    return rt;
}
/* the decay of a long, near-endless tail (ECHO): the energy of successive windows of w samples, the fitted slope's
 * RT60 */
static double rt60_win(const double *x, int n, int w)
{
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int m = 0;
    for (int k = 1; (k + 1) * w <= n; k++, m++) {
        double en = 0;
        for (int i = k * w; i < (k + 1) * w; i++) en += x[i] * x[i];
        double t = (double)k * w / FS, l = 10 * log10(en + 1e-300);
        sx += t; sy += l; sxx += t * t; sxy += t * l;
    }
    double sl = (m * sxy - sx * sy) / (m * sxx - sx * sx);
    return sl < 0 ? -60 / sl : -1;
}

static const double OCT[7] = {125, 250, 500, 1000, 2000, 4000, 8000};
/* an IR's numbers: RT60 at 250 Hz - 2 kHz and in each octave, its energy by octave (dB from the whole) */
typedef struct { double rt, rto[7], spec[7]; } irm_t;
static irm_t ir_measure(const double *x, int n, int long_tail)
{
    irm_t m;
    double *y = malloc(sizeof(double) * (size_t)n), tot = 0;
    for (int i = 0; i < n; i++) tot += x[i] * x[i];
    band(x, y, n, 250, 2000);
    m.rt = long_tail ? rt60_win(y, n, 32720) : rt60_of(y, n);    /* (ECHO: its 0.742 s echo a window) */
    for (int o = 0; o < 7; o++) {
        band(x, y, n, OCT[o] / M_SQRT2, OCT[o] * M_SQRT2 < FS / 2 ? OCT[o] * M_SQRT2 : FS / 2);
        double en = 0;
        for (int i = 0; i < n; i++) en += y[i] * y[i];
        m.spec[o] = 10 * log10(en / (tot + 1e-300) + 1e-300);
        m.rto[o] = long_tail ? rt60_win(y, n, 32720) : rt60_of(y, n);
    }
    free(y);
    return m;
}

/* the FDN's law (gen_tables.py's rv_t60) */
static double fdn_t60(int v) { return v ? 0.3 * pow(80.0, (v - 1) / 126.0) : 0; }

/* ------------------------------------------------------------------------------------ the reverbs alone ---- */

#define IR_SEC 30
/* the SPU reverb's impulse response, type t at DCAY dc and DAMP dm, its level out at unity (the cells' scale: an
 * impulse of 16384 sent, x SRV_IN, fills the input): left and right into l, r (frames); the frames it took to sleep, or
 * -1 */
static int ir_hann;                     /* (a 16-sample Hann pulse instead, its top under 5 kHz: to compare waveforms with
                                         * another implementation whatever its decimator's phase) */
static int spu_ir(int t, int dc, int dm, double *l, double *r, int frames)
{
    spu_reset();
    int32_t rs[M2K_SUB], L[M2K_SUB], R[M2K_SUB], slept = -1;
    for (int f = 0; f + M2K_SUB <= frames; f += M2K_SUB) {
        m_set(rs, 0, sizeof rs); m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
        if (!f && ir_hann)
            for (int i = 0; i < M2K_SUB; i++) rs[i] = (int32_t)lround(8192 * (1 - cos(2 * M_PI * (i + 0.5) / M2K_SUB)));
        else if (!f) rs[0] = 16384;
        spu_rv_block(t, dc, dm, rs, L, R);
        const double u = spu_r.gout ? 4096.0 / spu_r.gout : 0;  /* (the cells' own scale: unity out) */
        for (int i = 0; i < M2K_SUB; i++) { l[f + i] = L[i] * u; r[f + i] = R[i] * u; }
        if (spu_r.sleep && slept < 0) slept = f;
    }
    return slept;
}
/* the FDN's (m2k_fx.c's reverb_block, from a fresh bus), the same impulse */
static void fdn_ir(int dc, int dm, double *l, double *r, int frames)
{
    m2k_init(2000);
    int32_t rs[M2K_SUB], L[M2K_SUB], R[M2K_SUB];
    for (int f = 0; f + M2K_SUB <= frames; f += M2K_SUB) {
        m_set(rs, 0, sizeof rs); m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
        if (!f) rs[0] = 16384;
        reverb_block(dc, dm, rs, L, R, 0);
        for (int i = 0; i < M2K_SUB; i++) { l[f + i] = L[i]; r[f + i] = R[i]; }
    }
}

/* ------------------------------------------------------------------------------------------- bank A ---- */

/* pattern p from its start, every track (m2k_draw's play_from) */
static void play(int p) { m2k_init(2000); m2k_pattern(p); seq_play(1); }
static int bpm_of(int p) { return 30 + PAT(p)->glob[G_TMPO]; }

/* frames of pattern p into out, as phase 2 plays it: the SPU reverb's type t (0: the FDN, as today) in the FDN's place,
 * on the record of each block before its bus plays it a block later (its wet added into the tracks' sums there and the
 * record's DCAY 0, so the FDN rests), at DCAY dc and DAMP dm (-1: the pattern's own); GRIT (character g, -1 none) on
 * the bus's output, on while gon(frame) says so. sends: every track's VERB send set to it (-1: as the pattern has it).
 * rs_out: the sends, kept (a frame each) */
static int grit_toggle_frames;          /* GRIT's on and off every this many frames (0: on throughout) */
static int gon(int f) { return !grit_toggle_frames || (f / grit_toggle_frames) % 2 == 0; }
static void render(int p, int t, int dc, int dm, int g, int sends, int16_t *out, int frames, int32_t *rs_out)
{
    play(p);
    if (sends >= 0) for (int k = 0; k < M2K_TRACKS; k++) { PAT(p)->kit[k][P_SPU] = (uint8_t)sends; trk[k].eff[P_SPU] = (uint8_t)sends; }
    spu_reset();
    for (int f = 0; f + M2K_SUB <= frames; f += M2K_SUB) {
        m2k_render(out + 2 * f, M2K_SUB);
        brec_t *rc = &brec[bcur ^ 1];                    /* (block n's record: its bus plays next call) */
        if (rs_out) m_cpy(rs_out + f, rc->rs, sizeof rc->rs);
        if (t) {
            spu_rv_block(t, dc >= 0 ? dc : rc->glob[G_VERB], dm >= 0 ? dm : rc->glob[G_DAMP], rc->rs, rc->L, rc->R);
            rc->glob[G_VERB] = 0;
        }
        if (g >= 0) spu_grit_block(out + 2 * f, M2K_SUB, gon(f), g);
    }
}

static uint32_t fnv_pcm(const int16_t *s, int n)
{
    uint32_t h = 2166136261u;
    const uint8_t *b = (const uint8_t *)s;
    for (int i = 0; i < 2 * n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static double rms_db(const int16_t *s, int n)
{
    double e = 0;
    for (int i = 0; i < n; i++) e += (double)s[i] * s[i];
    return 10 * log10(e / n / (32767.0 * 32767.0) + 1e-20);
}
static double peak_db(const int16_t *s, int n)
{
    int p = 0;
    for (int i = 0; i < n; i++) p = abs(s[i]) > p ? abs(s[i]) : p;
    return 20 * log10(p / 32767.0 + 1e-12);
}

/* ------------------------------------------------------------------------------------------ the self-test ---- */

static double snr_sine(const int16_t *o, int n, double f)   /* the fundamental's share of the rest (left), dB */
{
    double c = 0, s = 0, tot = 0;
    for (int i = 0; i < n; i++) { double v = o[2 * i]; c += v * cos(2 * M_PI * f * i / FS); s += v * sin(2 * M_PI * f * i / FS); tot += v * v; }
    double fund = 2 * (c * c + s * s) / n;
    return 10 * log10(fund / (tot - fund + 1e-9));
}
/* the largest second difference (a click's mark) of the left over [a, b) */
static int d2max(const int16_t *o, int a, int b)
{
    int m = 0;
    for (int i = a < 2 ? 2 : a; i < b; i++) { int d = o[2 * i] - 2 * o[2 * i - 2] + o[2 * i - 4]; d = abs(d); if (d > m) m = d; }
    return m;
}
static int d2max32(const int32_t *o, int a, int b)
{
    int m = 0;
    for (int i = a < 2 ? 2 : a; i < b; i++) { int d = o[i] - 2 * o[i - 1] + o[i - 2]; d = abs(d); if (d > m) m = d; }
    return m;
}

static void test_tables(void)
{
    int bad = 0;
    for (int i = 0; i < 256; i++) {
        int s = T_SPU_GAUSS[0xFF - i] + T_SPU_GAUSS[0x1FF - i] + T_SPU_GAUSS[0x100 + i] + T_SPU_GAUSS[i];
        bad += s < 0x7F7F || s > 0x7F81;
    }
    check(!bad && T_SPU_GAUSS[0x1FF] == 0x59B3 && T_SPU_GAUSS[0] == -1, "gaussian (psx-spx's 512)", "each four 7F7F..7F81h");
    int dc = 0x4000;
    for (int k = 0; k < 10; k++) dc += 2 * T_SPU_RS[k];
    check(dc == 0x7FFE, "resampler (psx-spx's 39 taps)", "DC %04Xh / 8000h", dc);
    int home = 1;
    for (int t = 0; t < SRV_N; t++) {
        home &= T_SRV_WALL[t][T_SRV_HOME_DCAY[t]] == (int16_t)T_SRV_REG[t][7];
        home &= T_SRV_IIR[t][T_SRV_HOME_DAMP[t]] == (int16_t)T_SRV_REG[t][2];
    }
    check(home, "each type's registers at its home", "vWALL and vIIR exact at its DCAY and DAMP");
}

static void test_grit(void)
{
    static int16_t a[2 * 4 * FS], b[2 * 4 * FS];
    const int n = 2 * FS;
    /* off: nothing touched, nothing run */
    for (int i = 0; i < 2 * n; i++) a[i] = (int16_t)(8000 * sin(i * 0.01));
    m_cpy(b, a, sizeof a);
    spu_reset();
    for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(b + 2 * f, M2K_SUB, 0, 0);
    check(!memcmp(a, b, (size_t)n * 4) && !spu_g.run, "GRIT off", "the bus as it came, idle");
    /* silence in, silence out */
    for (int c = 0; c < GRIT_N; c++) {
        m_set(b, 0, sizeof b);
        spu_reset();
        int nz = 0;
        for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(b + 2 * f, M2K_SUB, 1, c);
        for (int i = 0; i < 2 * n; i++) nz += b[i] != 0;
        check(!nz, "GRIT silence", "%s: %d samples not 0", T_GRIT_NAME[c], nz);
    }
    /* a sine through each character: the codec's noise and the interpolator's dulling, as a share of the tone */
    static const double MIN_SNR[GRIT_N] = {20, 25, 6};
    for (int c = 0; c < GRIT_N; c++) {
        for (int i = 0; i < n; i++) a[2 * i] = a[2 * i + 1] = (int16_t)(16384 * sin(2 * M_PI * 441.0 * i / FS));
        spu_reset();
        for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(a + 2 * f, M2K_SUB, 1, c);
        double s = snr_sine(a + FS, FS, 441.0);
        check(s > MIN_SNR[c], "GRIT 441 Hz at -6 dBFS", "%s: %.1f dB tone over the rest", T_GRIT_NAME[c], s);
    }
    /* full scale: a square at +-32767 (the codec's worst: every unit's peak at its coarsest), and a loud machine's own
     * signal, bank A's A02 driven to DRIV 127 through the limiter: bounded, no overflow (the sanitizers' run) */
    for (int c = 0; c < GRIT_N; c++) {
        for (int i = 0; i < n; i++) a[2 * i] = a[2 * i + 1] = (int16_t)((i / 50) & 1 ? 32767 : -32767);
        spu_reset();
        for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(a + 2 * f, M2K_SUB, 1, c);
        check(1, "GRIT full-scale square", "%s: peak %.1f dBFS", T_GRIT_NAME[c], peak_db(a + FS, 2 * FS));
    }
    {
        play(1);
        PAT(1)->glob[G_DRIV] = 127;
        for (int f = 0; f < n; f += M2K_SUB) m2k_render(a + 2 * f, M2K_SUB);
        for (int c = 0; c < GRIT_N; c++) {
            m_cpy(b, a, (size_t)n * 4);
            spu_reset();
            for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(b + 2 * f, M2K_SUB, 1, c);
            check(1, "GRIT on A02 at DRIV 127", "%s: in %.1f dBFS peak, out %.1f, rms %.1f -> %.1f dB", T_GRIT_NAME[c],
                  peak_db(a, 2 * n), peak_db(b, 2 * n), rms_db(a + FS, FS), rms_db(b + FS, FS));
        }
    }
    /* in and out: a 220 Hz tone, GRIT pressed at 0.5 s, let go at 1.5 s: no second difference past the steady
     * sounds' own (a click's mark), and it idles again once out */
    for (int c = 0; c < GRIT_N; c++) {
        const int m = 3 * FS;
        for (int i = 0; i < m; i++) a[2 * i] = a[2 * i + 1] = (int16_t)(8192 * sin(2 * M_PI * 220.0 * i / FS));
        spu_reset();
        int idle = 0;
        for (int f = 0; f < m; f += M2K_SUB) {
            spu_grit_block(a + 2 * f, M2K_SUB, f >= FS / 2 && f < 3 * FS / 2, c);
            if (f > 3 * FS / 2 + FS / 20) idle |= spu_g.run;
        }
        int on = d2max(a, FS, 3 * FS / 2 - 100), off = d2max(a, 2 * FS, 3 * FS), tr = d2max(a, FS / 2 - 50, FS / 2 + FS / 20);
        int tr2 = d2max(a, 3 * FS / 2 - 50, 3 * FS / 2 + FS / 20), st = on > off ? on : off;
        check(tr <= st + st / 4 + 16 && tr2 <= st + st / 4 + 16 && !idle, "GRIT in and out (10 ms)",
              "%s: second difference in %d, out %d, steady %d (on %d, off %d)", T_GRIT_NAME[c], tr, tr2, st, on, off);
    }
    /* the same input twice: the same output (its dice its own, seeded at the start) */
    {
        for (int i = 0; i < n; i++) a[2 * i] = (int16_t)(12000 * sin(i * 0.031)), a[2 * i + 1] = (int16_t)(9000 * sin(i * 0.017));
        m_cpy(b, a, (size_t)n * 4);
        spu_reset();
        for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(a + 2 * f, M2K_SUB, 1, 0);
        spu_reset();
        for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(b + 2 * f, M2K_SUB, 1, 0);
        check(!memcmp(a, b, (size_t)n * 4), "GRIT repeatable", "two runs alike");
    }
}

static void test_reverb(void)
{
    const int n = 20 * FS;
    double *l = malloc(sizeof(double) * (size_t)n), *r = malloc(sizeof(double) * (size_t)n);
    int32_t rs[M2K_SUB], L[M2K_SUB], R[M2K_SUB];
    /* at rest: no type, or DCAY 0: nothing done */
    spu_reset();
    int touched = 0;
    for (int f = 0; f < FS; f += M2K_SUB) {
        for (int i = 0; i < M2K_SUB; i++) rs[i] = 1000, L[i] = R[i] = 7;
        spu_rv_block(0, 64, 64, rs, L, R);
        spu_rv_block(3, 0, 64, rs, L, R);
        for (int i = 0; i < M2K_SUB; i++) touched |= L[i] != 7 || R[i] != 7;
    }
    check(!touched && !spu_r.type, "reverb off", "DCAY 0 or no type: the sums as they came");
    /* silence in: silence out, and asleep within a lap */
    for (int t = 1; t <= SRV_N; t++) {
        spu_reset();
        int nz = 0, at = -1;
        for (int f = 0; f < 3 * FS; f += M2K_SUB) {
            m_set(rs, 0, sizeof rs); m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
            spu_rv_block(t, T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], rs, L, R);
            for (int i = 0; i < M2K_SUB; i++) nz += L[i] || R[i];
            if (spu_r.sleep && at < 0) at = f;
        }
        check(!nz && at >= 0, "reverb silence", "%-10s 0 out, asleep at %.2f s", T_SRV_NAME[t - 1], at / (double)FS);
    }
    /* an impulse: it rings, falls, empties and sleeps; its tail ends with nothing left (no DC held) */
    for (int t = 1; t <= SRV_N; t++) {
        if (t == 8) continue;                            /* (ECHO: near forever, by design) */
        int s = spu_ir(t, T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], l, r, n);
        double pk = 0, last = 0;
        for (int i = 0; i < n; i++) { if (fabs(l[i]) > pk) pk = fabs(l[i]); }
        for (int i = (s > FS ? s - FS / 10 : 0); i < (s > 0 ? s : n); i++) last = fabs(l[i]) > last ? fabs(l[i]) : last;
        int after = 0;
        for (int i = s > 0 ? s + M2K_SUB : n; i < n; i++) after += l[i] != 0 || r[i] != 0;
        check(s > 0 && !after, "reverb impulse to sleep", "%-10s peak %5.0f, asleep at %5.2f s, its last 0.1 s within %.0f",
              T_SRV_NAME[t - 1], pk, s / (double)FS, last);
    }
    /* full scale: a loud machine's own signal at full send on every track (A03, every VERB 127) and a square of eight
     * tracks' ceilings: saturated as the chip's 16 bits are, bounded, no overflow (the sanitizers' run) */
    {
        static int16_t o[2 * 6 * FS];
        static int32_t rsk[6 * FS];
        for (int t = 1; t <= SRV_N; t++) {
            render(2, t, T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], -1, 127, o, 6 * FS, rsk);
            int32_t mx = 0;
            for (int i = 0; i < 6 * FS; i++) mx = abs(rsk[i]) > mx ? abs(rsk[i]) : mx;
            check(1, "reverb, A03 every send 127", "%-10s sends' peak %6d, out %.1f dBFS peak", T_SRV_NAME[t - 1], mx, peak_db(o, 12 * FS));
        }
        spu_reset();
        int32_t big = 0;
        for (int f = 0; f < 4 * FS; f += M2K_SUB) {
            for (int i = 0; i < M2K_SUB; i++) rs[i] = ((f + i) / 37) & 1 ? 8 * 32244 : -8 * 32244, L[i] = R[i] = 0;
            spu_rv_block(5, 127, 0, rs, L, R);
            for (int i = 0; i < M2K_SUB; i++) big = abs(L[i]) > big ? abs(L[i]) : big;
        }
        check(big < 1 << 18, "reverb, a square of 8 ceilings", "HALL at DCAY 127: wet peak %d (bus units, the limiter's range 2^17)", big);
    }
    /* switching: ROOM -> HALL -> SPACE ECHO -> DCAY 0 -> ROOM on A03's sends: no second difference past the wet's own */
    {
        static int16_t o[2 * 13 * FS];
        static int32_t rsk[13 * FS], wl[13 * FS];
        render(2, 0, -1, -1, -1, -1, o, 13 * FS, rsk);
        spu_reset();
        for (int f = 0; f + M2K_SUB <= 13 * FS; f += M2K_SUB) {
            int t = f < 3 * FS ? 1 : f < 6 * FS ? 5 : 7, dc = f >= 9 * FS && f < 10 * FS ? 0 : 60;
            if (f >= 10 * FS) t = 1;
            m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
            spu_rv_block(t, dc, 64, rsk + f, L, R);
            m_cpy(wl + f, L, sizeof L);
        }
        int st = d2max32(wl, FS, 3 * FS - 600);
        int s2 = d2max32(wl, 3 * FS + 2000, 6 * FS - 600), s3 = d2max32(wl, 6 * FS + 2000, 9 * FS - 600);
        st = st > s2 ? st : s2; st = st > s3 ? st : s3;
        int sw = 0;
        static const int AT[4] = {3, 6, 9, 10};
        for (int k = 0; k < 4; k++) { int d = d2max32(wl, AT[k] * FS - 400, AT[k] * FS + 1600); sw = d > sw ? d : sw; }
        check(sw <= st, "reverb type and DCAY 0 switches", "second difference at the switches %d, the wet's own %d", sw, st);
    }
    /* repeatable */
    {
        static int16_t o1[2 * 4 * FS], o2[2 * 4 * FS];
        render(2, 5, -1, -1, 0, -1, o1, 4 * FS, 0);
        render(2, 5, -1, -1, 0, -1, o2, 4 * FS, 0);
        check(!memcmp(o1, o2, sizeof o1), "A03 HALL + GRIT repeatable", "two renders alike");
    }
    free(l);
    free(r);
}

/* the eight-track build's own bank A as it is (the default: no type, no GRIT): render() with neither is m2k_render's */
static void test_bank_a(void)
{
    static int16_t o1[2 * 10 * FS], o2[2 * 10 * FS];
    int same = 1;
    for (int p = 0; p < 4; p++) {
        play(p);
        for (int f = 0; f < FR(10); f += 128) m2k_render(o1 + 2 * f, 128);
        render(p, 0, -1, -1, -1, -1, o2, FR(10), 0);
        same &= !memcmp(o1, o2, sizeof o1);
        printf("     A%02d %08x\n", p + 1, fnv_pcm(o1, FR(10)));
    }
    check(same, "bank A, nothing chosen", "the render as m2k_render's, sample for sample");
}

/* ------------------------------------------------------------------------------------------- the modes ---- */

static int rt_mode(void)
{
    const int n = IR_SEC * FS;
    double *l = malloc(sizeof(double) * (size_t)n), *r = malloc(sizeof(double) * (size_t)n);
    printf("RT60 (250 Hz - 2 kHz, s) and the IR's energy by octave (dB of the whole, 125 Hz .. 8 kHz), left\n");
    printf("  %-10s %-11s %6s   %s\n", "type", "DCAY DAMP", "RT60", "125    250    500     1k     2k     4k     8k");
    for (int t = 1; t <= SRV_N; t++) {
        int s = spu_ir(t, T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], l, r, n);
        irm_t m = ir_measure(l, s > 0 ? s : n, t == 8);
        printf("  %-10s %3d %3d     %6.2f  ", T_SRV_NAME[t - 1], T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], m.rt);
        for (int o = 0; o < 7; o++) printf(" %6.1f", m.spec[o]);
        printf("   RT by octave:");
        for (int o = 0; o < 7; o++) printf(" %.2f", m.rto[o]);
        printf("\n");
    }
    printf("the law: RT60 at DCAY v against the FDN's law, each type at its home DAMP\n");
    static const int V[6] = {1, 20, 40, 60, 80, 110};
    for (int t = 1; t <= SRV_N; t++) {
        if (t == 8) continue;
        printf("  %-10s", T_SRV_NAME[t - 1]);
        for (int k = 0; k < 6; k++) {
            int s = spu_ir(t, V[k], T_SRV_HOME_DAMP[t - 1], l, r, n);
            irm_t m = ir_measure(l, s > 0 ? s : n, t == 9);   /* (DELAY's echoes: a window each, not Schroeder's steps) */
            printf("  %3d: %5.2f/%5.2f", V[k], m.rt, fdn_t60(V[k]));
        }
        printf("\n");
    }
    printf("the FDN at the same DCAY and DAMP (m2k_fx.c reverb_block, the same impulse)\n");
    for (int t = 1; t <= SRV_N; t++) {
        int dc = T_SRV_HOME_DCAY[t - 1], dm = T_SRV_HOME_DAMP[t - 1];
        fdn_ir(dc, dm, l, r, n);
        irm_t m = ir_measure(l, n, 0);
        printf("  at %-10s %3d %3d     %6.2f  ", T_SRV_NAME[t - 1], dc, dm, m.rt);
        for (int o = 0; o < 7; o++) printf(" %6.1f", m.spec[o]);
        printf("   (its law %.2f)\n", fdn_t60(dc));
    }
    free(l);
    free(r);
    return 0;
}

static int ir_mode(const char *dir)
{
    const int n = 12 * FS;
    double *l = malloc(sizeof(double) * (size_t)n), *r = malloc(sizeof(double) * (size_t)n);
    int16_t *o = malloc((size_t)n * 4);
    char path[512];
    for (int t = 1; t <= SRV_N; t++) {
        spu_ir(t, T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], l, r, n);
        for (int i = 0; i < n; i++) { o[2 * i] = (int16_t)spu_sat((int32_t)l[i]); o[2 * i + 1] = (int16_t)spu_sat((int32_t)r[i]); }
        snprintf(path, sizeof path, "%s/ir_%d.wav", dir, t);
        wav_write(path, o, n);
    }
    printf("%d impulse responses in %s (an impulse of 16384 sent, the cells' scale out)\n", SRV_N, dir);
    return 0;
}

static int measure_mode(int argc, char **argv)
{
    printf("  %-28s %6s   %s\n", "file", "RT60", "125    250    500     1k     2k     4k     8k");
    for (int a = 2; a < argc; a++) {
        int frames = 0;
        int16_t *s = wav_read(argv[a], &frames);
        if (!s) { printf("  %s: not a 16-bit WAV\n", argv[a]); continue; }
        double *x = malloc(sizeof(double) * (size_t)frames);
        int end = 0;
        for (int i = 0; i < frames; i++) { x[i] = s[2 * i]; if (s[2 * i]) end = i + 1; }
        int long_tail = strstr(argv[a], "_8.") != 0;    /* (ECHO's) */
        irm_t m = ir_measure(x, long_tail ? frames : end, long_tail);
        const char *b = strrchr(argv[a], '/');
        printf("  %-28s %6.2f  ", b ? b + 1 : argv[a], m.rt);
        for (int o = 0; o < 7; o++) printf(" %6.1f", m.spec[o]);
        printf("\n");
        free(x);
        free(s);
    }
    return 0;
}

/* the FDN's wet energy on the sends rs (n frames) at DCAY dc, DAMP dm, from a fresh bus */
static double fdn_energy(const int32_t *rs, int n, int dc, int dm)
{
    int32_t L[M2K_SUB], R[M2K_SUB];
    double e = 0;
    m2k_init(2000);
    for (int f = 0; f < n; f += M2K_SUB) {
        m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
        reverb_block(dc, dm, rs + f, L, R, 0);
        for (int i = 0; i < M2K_SUB; i++) e += (double)L[i] * L[i] + (double)R[i] * R[i];
    }
    return e;
}
/* each type's level against the FDN's, its wet alone, on bank A's sends (A02 A03 A04, 30 s each): at home against the FDN
 * at the same DCAY and DAMP (the same time and colour: tools/gen_spu.py's GO, Q12, the three's mean in dB), and against
 * the FDN at the pattern's own (what that pattern plays today) */
static int level_mode(void)
{
    const int n = FR(30);
    static int16_t o[2 * 30 * FS];
    static int32_t rsk[30 * FS];
    int32_t L[M2K_SUB], R[M2K_SUB];
    double sum[SRV_N] = {0};
    printf("each type at home: its wet against the FDN's at the same DCAY and DAMP | at the pattern's own, dB\n");
    for (int p = 1; p < 4; p++) {
        render(p, 0, -1, -1, -1, -1, o, n, rsk);
        const int dc = PAT(p)->glob[G_VERB], dm = PAT(p)->glob[G_DAMP];
        const double eo = fdn_energy(rsk, n, dc, dm);
        printf("  A%02d (DCAY %d DAMP %d: the FDN's wet %.1f dB re 32767)\n", p + 1, dc, dm, 10 * log10(eo / n / 2 / (32767.0 * 32767.0)));
        for (int t = 1; t <= SRV_N; t++) {
            const int hd = T_SRV_HOME_DCAY[t - 1], hm = T_SRV_HOME_DAMP[t - 1];
            const double eh = fdn_energy(rsk, n, hd, hm);
            spu_reset();
            double es = 0;
            for (int f = 0; f < n; f += M2K_SUB) {
                m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
                spu_rv_block(t, hd, hm, rsk + f, L, R);
                for (int i = 0; i < M2K_SUB; i++) es += (double)L[i] * L[i] + (double)R[i] * R[i];
            }
            sum[t - 1] += 10 * log10(es / eh);
            printf("    %-10s %+5.1f | %+5.1f\n", T_SRV_NAME[t - 1], 10 * log10(es / eh), 10 * log10(es / eo));
        }
    }
    printf("GO for tools/gen_spu.py (Q12, the three's mean):");
    for (int t = 0; t < SRV_N; t++) printf(" %.0f", T_SRV_GOUT[t][T_SRV_HOME_DCAY[t]] * pow(10, -sum[t] / 3 / 20));
    printf("\n");
    return 0;
}

static int render_mode(const char *dir)
{
    const int n = FR(30);
    int16_t *o = malloc((size_t)n * 4);
    char path[512], name[64];
    FILE *idx;
    snprintf(path, sizeof path, "%s/renders.txt", dir);
    if (!(idx = fopen(path, "w"))) { perror(path); return 1; }
#define OUT(fmt, ...) do { snprintf(name, sizeof name, fmt, __VA_ARGS__); snprintf(path, sizeof path, "%s/%s.wav", dir, name); \
        wav_write(path, o, n); fprintf(idx, "%-28s peak %5.1f dBFS, rms %5.1f dBFS, fnv %08x\n", name, peak_db(o, 2 * n), \
        rms_db(o, 2 * n), fnv_pcm(o, n)); printf("%-28s peak %5.1f dBFS, rms %5.1f dBFS\n", name, peak_db(o, 2 * n), rms_db(o, 2 * n)); } while (0)
    /* the reverbs on A03 SUSPEND (bank A's widest sends): today's FDN, then the nine at home, then two on A03's own
     * DCAY 51 and DAMP 118 (the laws) */
    render(2, 0, -1, -1, -1, -1, o, n, 0);
    OUT("%s", "a03_fdn");
    for (int t = 1; t <= SRV_N; t++) {
        char lo[16];
        int k = 0;
        for (const char *c = T_SRV_NAME[t - 1]; *c && k < 15; c++) lo[k++] = *c == ' ' ? '_' : (char)(*c | 0x20);
        lo[k] = 0;
        render(2, t, T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], -1, -1, o, n, 0);
        OUT("a03_%d_%s", t, lo);
    }
    render(2, 1, -1, -1, -1, -1, o, n, 0);
    OUT("%s", "a03_room_dcay51_damp118");
    render(2, 5, -1, -1, -1, -1, o, n, 0);
    OUT("%s", "a03_hall_dcay51_damp118");
    /* GRIT on A01 CIRCUIT: off, each character on throughout, and PERFORM's pressed for two bars, let go for two */
    render(0, 0, -1, -1, -1, -1, o, n, 0);
    OUT("%s", "a01_grit_off");
    for (int c = 0; c < GRIT_N; c++) {
        char lo[16];
        int k = 0;
        for (const char *s = T_GRIT_NAME[c]; *s && k < 15; s++) lo[k++] = *s == ' ' ? '_' : (char)(*s | 0x20);
        lo[k] = 0;
        render(0, 0, -1, -1, c, -1, o, n, 0);
        OUT("a01_grit_%d_%s", c, lo);
    }
    grit_toggle_frames = (int)(2 * 4 * 60.0 / bpm_of(0) * FS);
    render(0, 0, -1, -1, 0, -1, o, n, 0);
    OUT("%s", "a01_grit_0_two_bars_on_off");
    /* and A03 with both: GRIT over SPACE ECHO at home, two bars on, two off */
    grit_toggle_frames = (int)(2 * 4 * 60.0 / bpm_of(2) * FS);
    render(2, 7, T_SRV_HOME_DCAY[6], T_SRV_HOME_DAMP[6], 0, -1, o, n, 0);
    OUT("%s", "a03_space_echo_grit_on_off");
    grit_toggle_frames = 0;
    /* the tour: A03's first 10 s with every track's VERB send at 64 (bank A's own are light: the reverb heard), through
     * the FDN at A03's DCAY and DAMP, then each type at home, one after another */
    {
        const int seg = FR(10);
        int16_t *t10 = malloc((size_t)seg * 4), *all = malloc((size_t)seg * 4 * (SRV_N + 1));
        for (int t = 0; t <= SRV_N; t++) {
            render(2, t, t ? T_SRV_HOME_DCAY[t - 1] : -1, t ? T_SRV_HOME_DAMP[t - 1] : -1, -1, 64, t10, seg, 0);
            for (int i = 0; i < seg; i++) {              /* (the seams: 5 ms in, 80 ms out, so the file has no cut) */
                double g = i < 220 ? i / 220.0 : seg - i < 3528 ? (seg - i) / 3528.0 : 1;
                t10[2 * i] = (int16_t)lround(t10[2 * i] * g);
                t10[2 * i + 1] = (int16_t)lround(t10[2 * i + 1] * g);
            }
            m_cpy(all + 2 * seg * t, t10, (uint32_t)seg * 4);
        }
        snprintf(path, sizeof path, "%s/a03_tour_sends64.wav", dir);
        wav_write(path, all, seg * (SRV_N + 1));
        fprintf(idx, "%-28s 10 s each: FDN, then types 1..9 at home; peak %5.1f dBFS, rms %5.1f dBFS\n", "a03_tour_sends64",
                peak_db(all, 2 * seg * (SRV_N + 1)), rms_db(all, 2 * seg * (SRV_N + 1)));
        printf("a03_tour_sends64             FDN, then the nine, 10 s each\n");
        free(t10);
        free(all);
    }
    fclose(idx);
    free(o);
    return 0;
}

#ifdef __APPLE__
static uint64_t instr(void)
{
    struct rusage_info_v4 ri;
    proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri);
    return ri.ri_instructions;
}
/* the cost of the modules alone, a block at a time (each call's count less the counter's own), in Mac instructions a
 * sample: GRIT on A01's output (each character, on throughout), the reverb on A03's sends (each type at home, and
 * asleep), the FDN on the same sends for scale */
static int bench_mode(void)
{
    const int n = FR(20);
    static int16_t o[2 * 20 * FS], g[2 * 20 * FS];
    static int32_t rsk[20 * FS];
    int32_t L[M2K_SUB], R[M2K_SUB];
    uint64_t base = ~0ull;
    for (int k = 0; k < 20; k++) { uint64_t a = instr(), b = instr() - a; base = b < base ? b : base; }
    printf("Mac instructions a sample, built as the FM-1 builds the core (the modules alone, 20 s)\n");
    render(0, 0, -1, -1, -1, -1, o, n, 0);
    {
        uint64_t tot = 0;
        spu_reset();
        m_cpy(g, o, sizeof g);
        { uint64_t a = instr(); for (int f = 0; f < n; f += M2K_SUB) spu_grit_block(g + 2 * f, M2K_SUB, 0, 0); tot = instr() - a; }   /* (one count: a call is cheaper than a read of the counter) */
        printf("  GRIT off                    %6.1f\n", (double)tot / n);
    }
    for (int c = 0; c < GRIT_N; c++) {
        uint64_t tot = 0;
        spu_reset();
        m_cpy(g, o, sizeof g);
        for (int f = 0; f < n; f += M2K_SUB) { uint64_t a = instr(); spu_grit_block(g + 2 * f, M2K_SUB, 1, c); tot += instr() - a - base; }
        printf("  GRIT %-22s %6.1f   (A01's output)\n", T_GRIT_NAME[c], (double)tot / n);
    }
    render(2, 0, -1, -1, -1, -1, o, n, rsk);
    {
        uint64_t tot = 0;
        m2k_init(2000);
        const int dc = PAT(2)->glob[G_VERB], dm = PAT(2)->glob[G_DAMP];
        for (int f = 0; f < n; f += M2K_SUB) {
            m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
            uint64_t a = instr(); reverb_block(dc, dm, rsk + f, L, R, 0); tot += instr() - a - base;
        }
        printf("  the FDN, A03's DCAY %d DAMP %d  %6.1f   (A03's sends, for scale)\n", dc, dm, (double)tot / n);
    }
    for (int t = 1; t <= SRV_N; t++) {
        uint64_t tot = 0;
        spu_reset();
        for (int f = 0; f < n; f += M2K_SUB) {
            m_set(L, 0, sizeof L); m_set(R, 0, sizeof R);
            uint64_t a = instr(); spu_rv_block(t, T_SRV_HOME_DCAY[t - 1], T_SRV_HOME_DAMP[t - 1], rsk + f, L, R); tot += instr() - a - base;
        }
        printf("  reverb %-20s %6.1f   (A03's sends)\n", T_SRV_NAME[t - 1], (double)tot / n);
    }
    {
        int32_t z[M2K_SUB];
        uint64_t tot = 0;
        m_set(z, 0, sizeof z);
        spu_reset();
        for (int f = 0; f < 2 * FS; f += M2K_SUB) spu_rv_block(5, 68, 65, z, L, R);
        { uint64_t a = instr(); for (int f = 0; f < n; f += M2K_SUB) spu_rv_block(5, 68, 65, z, L, R); tot = instr() - a; }
        printf("  reverb asleep (HALL)        %6.1f   (%s)\n", (double)tot / n, spu_r.sleep ? "asleep" : "AWAKE");
    }
    printf("memory: the work area %u B, its state %u B, GRIT's %u B; tables: gaussian %u, registers %u, laws %u\n",
           (unsigned)sizeof srv_wk, (unsigned)sizeof spu_r, (unsigned)sizeof spu_g, (unsigned)sizeof T_SPU_GAUSS,
           (unsigned)sizeof T_SRV_REG, (unsigned)(sizeof T_SRV_WALL + sizeof T_SRV_IIR + sizeof T_SRV_GOUT));
    return 0;
}
#else
static int bench_mode(void) { printf("--bench counts a Mac's instructions (proc_pid_rusage)\n"); return 0; }
#endif

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--rt")) return rt_mode();
    if (argc > 2 && !strcmp(argv[1], "--ir")) return ir_mode(argv[2]);
    if (argc > 2 && !strcmp(argv[1], "--ir-hann")) { ir_hann = 1; return ir_mode(argv[2]); }
    if (argc > 2 && !strcmp(argv[1], "--measure")) return measure_mode(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--level")) return level_mode();
    if (argc > 2 && !strcmp(argv[1], "--render")) return render_mode(argv[2]);
    if (argc > 1 && !strcmp(argv[1], "--bench")) return bench_mode();
    test_tables();
    test_grit();
    test_reverb();
    test_bank_a();
    printf(fails ? "SPU TEST FAILED: %d\n" : "spu test: all passed\n", fails);
    return fails ? 1 : 0;
}
