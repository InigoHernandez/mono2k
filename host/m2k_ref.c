/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's side of the comparison: reads the same spec as tools/ref/mm_ref (name, Monomachine machine number, note,
 * velocity, gate, tail) and renders each line through MONO2K's counterpart, from the blank pattern, as a WAV.
 *   cc -O2 -Ifirmware/src -Ifirmware/gen -o build/host/m2k_ref host/m2k_ref.c -lm
 *   build/host/m2k_ref build/ref/m2k < tools/ref/basic.spec */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m2k_core.c"

/* the Monomachine's machine numbers (as its own screen names them) to MONO2K's machines */
static int machine_of(int mm)
{
    switch (mm) {
    case 1: return M_GSIN;
    case 2: return M_GNOI;
    case 3: return M_SID;
    case 4: return M_SAW;
    case 5: return M_PUL;
    case 6: return M_WAV;
    case 8: return M_FMS;
    case 9: return M_FMP;
    case 10: return M_FMD;
    case 11: return M_VOX;
    case 14: return M_ENS;
    case 15: return M_FXC;
    case 16: return M_FXD;
    case 17: return M_FXR;
    default: return -1;
    }
}

static void wav(const char *path, const int16_t *s, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t bytes = frames * 4, v;
    uint16_t h;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f);
    v = M2K_SR; fwrite(&v, 4, 1, f); v = M2K_SR * 4; fwrite(&v, 4, 1, f); h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(s, 4, frames, f);
    fclose(f);
}

static void run(int16_t *out, uint32_t frames, uint32_t *ms)
{
    for (uint32_t i = 0; i < frames; i += 128) {
        m2k_render(out + i * 2, 128);
        *ms += 3;                                   /* about 128 frames */
        m2k_frame(*ms);
    }
}

/* a line's "cc=v" controllers for track t, as main() reads them ("m2k:" MONO2K's alone, "mm:" skipped, "m2k:pN=v" a
 * kit parameter as it is) */
static void controls(const char *c, int t)
{
    m2k_pattern_t *p = &pat[cur_pat];
    while (*c && *c != '\n') {
        int cc, v, used, skip = 0;
        if (!strncmp(c, "mm:", 3)) { c += 3; skip = 1; }
        else if (!strncmp(c, "m2k:", 4)) c += 4;
        if (*c == 'p') {
            if (sscanf(c, "p%d=%d%n", &cc, &v, &used) != 2 || cc < 0 || cc >= P_COUNT) break;
            p->kit[t][cc] = (uint8_t)v;
            trk[t].eff[cc] = (uint8_t)v;
        } else {
            if (sscanf(c, "%d=%d%n", &cc, &v, &used) != 2) break;
            if (!skip) m2k_midi(0xB0 | t, cc, v);
        }
        c += used;
        while (*c == ' ' || *c == '\t') c++;
    }
}

/* the loop of tools/ref/power.loop, as tools/ref/mm_ref's `loop` plays it on the Monomachine: each track its
 * machine and controls on its own MIDI channel, the notes step by step at the tempo; the bus's drive as given (0: the
 * bus as it is). DIR/loop_m2k_drivN.wav */
static int loop(const char *dir, int drive)
{
    m2k_init(1234);
    uint32_t ms = 0;
    for (int i = 0; i < 200; i++) m2k_frame(ms += 16);
    m2k_pattern(16);
    m2k_pattern_t *p = &pat[cur_pat];
    sq.ktrk = 5;                                   /* the keys elsewhere: every channel plays its own track */
    p->glob[G_DRIV] = (uint8_t)drive;
    double tempo = 120, seconds = 15;
    int steps = 16, nn = 0;
    struct { int track, step, note, vel, gate; } notes[256];
    char line[1024];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] == 'L') sscanf(line + 1, "%lf %d %lf", &tempo, &steps, &seconds);
        else if (line[0] == 'N' && nn < 256) {
            if (sscanf(line + 1, "%d %d %d %d %d", &notes[nn].track, &notes[nn].step, &notes[nn].note, &notes[nn].vel,
                       &notes[nn].gate) == 5) nn++;
        } else if (line[0] == 'T') {
            int t, mm, used;
            if (sscanf(line + 1, "%d %d%n", &t, &mm, &used) != 2 || t < 0 || t >= M2K_TRACKS) continue;
            int m = machine_of(mm);
            if (m < 0) { fprintf(stderr, "no counterpart for machine %d\n", mm); return 1; }
            kit_default(p, t, m);
            m_cpy(trk[t].eff, p->kit[t], P_COUNT);
            const char *c = line + 1 + used;
            while (*c == ' ' || *c == '\t') c++;
            controls(c, t);
        }
    }
    for (int i = 0; i < 4; i++) m2k_frame(ms += 16);
    const double step_frames = M2K_SR * 15.0 / tempo;
    const uint32_t total = (uint32_t)(seconds * M2K_SR) & ~15u;
    int16_t *s = calloc((size_t)total * 2, sizeof *s);
    uint32_t off_at[64];
    int off_ch[64], off_note[64], noff = 0;
    for (uint32_t f = 0, step = 0; f < total; f += 16) {
        uint32_t frame_of_step = (uint32_t)(step * step_frames + 0.5);
        if (f + 16 > frame_of_step) {
            for (int i = 0; i < noff;)
                if (off_at[i] <= frame_of_step) {
                    m2k_midi(0x80 | off_ch[i], off_note[i], 0);
                    off_at[i] = off_at[noff - 1]; off_ch[i] = off_ch[noff - 1]; off_note[i] = off_note[noff - 1];
                    noff--;
                } else i++;
            for (int i = 0; i < nn; i++)
                if (notes[i].step == (int)(step % (uint32_t)steps) && noff < 64) {
                    m2k_midi(0x90 | notes[i].track, notes[i].note, notes[i].vel);
                    off_at[noff] = (uint32_t)((step + notes[i].gate) * step_frames + 0.5) - 1;
                    off_ch[noff] = notes[i].track;
                    off_note[noff++] = notes[i].note;
                }
            step++;
        }
        m2k_render(s + f * 2, 16);
        if ((f & 2047) == 0) m2k_frame(ms += 46);
    }
    char path[512];
    snprintf(path, sizeof path, "%s/loop_m2k_driv%d.wav", dir, drive);
    wav(path, s, total);
    free(s);
    printf("loop_m2k_driv%d\n", drive);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[2], "--loop")) return loop(argv[1], argc > 3 ? atoi(argv[3]) : 0);
    if (argc != 2) { fprintf(stderr, "m2k_ref DIR < SPEC | m2k_ref DIR --loop [DRIVE] < LOOP\n"); return 2; }
    char line[512];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char name[64];
        int mm, note, vel;
        double gate, tail;
        if (sscanf(line, "%63s %d %d %d %lf %lf", name, &mm, &note, &vel, &gate, &tail) != 6) continue;
        int m = machine_of(mm);
        if (m < 0) { fprintf(stderr, "%s: no counterpart for machine %d\n", name, mm); continue; }
        m2k_init(1234);
        uint32_t ms = 0;
        for (int i = 0; i < 200; i++) m2k_frame(ms += 16);   /* past the splash */
        m2k_pattern(16);                                       /* B01, a blank pattern */
        m2k_pattern_t *p = &pat[cur_pat];
        kit_default(p, 0, M_GSIN);                             /* its T1 as the references were made on: kit_default's
                                                                * chain, not GND-SIN's INIT */
        p->machine[0] = (uint8_t)m;
        m_cpy(p->kit[0], MACH_DEF[m], 8);
        m_cpy(trk[0].eff, p->kit[0], P_COUNT);
        /* the line's controllers, through MONO2K's own copy of the Monomachine's CC map */
        const char *c = line;
        for (int f = 0; f < 6 && *c; f++) { while (*c && *c != ' ' && *c != '\t') c++; while (*c == ' ' || *c == '\t') c++; }
        int pre = 0;
        while (*c && *c != '\n') {
            int cc, v, used, skip = 0;
            if (!strncmp(c, "pre=", 4)) {   /* "pre=MS": the note played MS ms first, then 300 ms of rest (in sequence) */
                if (sscanf(c + 4, "%d%n", &pre, &used) != 1 || pre < 0 || pre > 5000) break;
                c += 4 + used;
                while (*c == ' ' || *c == '\t') c++;
                continue;
            }
            /* "mm:cc=v" is the Monomachine's alone (a control MONO2K keeps elsewhere); "m2k:cc=v" is MONO2K's alone */
            if (!strncmp(c, "bpm=", 4)) {   /* "bpm=N": the pattern's tempo (the Monomachine's set on its panel) */
                if (sscanf(c + 4, "%d%n", &v, &used) != 1 || v < 30 || v > 157) break;
                p->glob[G_TMPO] = (uint8_t)(v - 30);
                c += 4 + used;
                while (*c == ' ' || *c == '\t') c++;
                continue;
            }
            if (!strncmp(c, "mm:", 3)) { c += 3; skip = 1; }
            else if (!strncmp(c, "m2k:", 4)) c += 4;
            if (*c == 'p') {               /* "m2k:pN=v": MONO2K's kit parameter N as it is (one no controller reaches) */
                if (sscanf(c, "p%d=%d%n", &cc, &v, &used) != 2 || cc < 0 || cc >= P_COUNT) break;
                p->kit[0][cc] = (uint8_t)v;
                trk[0].eff[cc] = (uint8_t)v;
                c += used;
                while (*c == ' ' || *c == '\t') c++;
                continue;
            }
            if (sscanf(c, "%d=%d%n", &cc, &v, &used) != 2) break;
            if (skip) { c += used; while (*c == ' ' || *c == '\t') c++; continue; }
            m2k_midi(0xB0, cc, v);
            c += used;
            while (*c == ' ' || *c == '\t') c++;
        }
        for (int i = 0; i < 4; i++) m2k_frame(ms += 16);
        {   /* let what glides settle (the delay's time, as the reference renderer waits 1.5 s on the Monomachine) */
            static int16_t pre[M2K_SR / 2 * 2];
            run(pre, M2K_SR / 2 & ~127u, &ms);
        }
        uint32_t g = (uint32_t)(gate * M2K_SR) & ~127u, tl = (uint32_t)(tail * M2K_SR) & ~127u;
        int16_t *s = calloc((size_t)(g + tl) * 2, sizeof *s);
        if (pre) {
            uint32_t pg = ((uint32_t)pre * (M2K_SR / 1000)) & ~127u, pr = (M2K_SR * 3 / 10) & ~127u;
            int16_t *b = calloc((size_t)(pg > pr ? pg : pr) * 2, sizeof *b);
            m2k_midi(0x90, note, vel);
            run(b, pg, &ms);
            m2k_midi(0x80, note, 0);
            run(b, pr, &ms);
            free(b);
        }
        m2k_midi(0x90, note, vel);
        run(s, g, &ms);
        m2k_midi(0x80, note, 0);
        run(s + g * 2, tl, &ms);
        char path[512];
        snprintf(path, sizeof path, "%s/%s.wav", argv[1], name);
        wav(path, s, g + tl);
        free(s);
        puts(name);
    }
    return 0;
}
