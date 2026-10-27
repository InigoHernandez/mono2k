/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's drawing cost: the instructions one frame of each page takes (m2k_frame, the screen drawn and presented), and
 * each frame of the splash, counted as CLAUDE.md counts the sound's: built as the FM-1 builds the core, the 64-bit
 * products out of line, and read from the kernel (proc_pid_rusage ri_instructions, a Mac's). The FM-1 draws at most 30
 * frames a second (a splash frame changes every strip: about 13 a second over its SPI).
 *   cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen \
 *      -o build/host/m2k_draw host/m2k_draw.c && build/host/m2k_draw [--splash] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
#include "m2k_core.c"

static uint64_t instr(void)
{
    struct rusage_info_v4 ri;
    proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri);
    return ri.ri_instructions;
}

static uint64_t base_cost;
static uint32_t now = 4000;

/* the least of a few runs of one frame (the kernel's count is the process's: a run can catch a little else) */
static uint64_t frame_cost(void)
{
    uint64_t best = ~0ull;
    for (int r = 0; r < 5; r++) {
        now += 40;
        ui.trans_t = 0; ui.in_trans = 0; ui.toast_t = 0; ui.glitch_t = 0;
        uint64_t a = instr();
        m2k_frame(now);
        uint64_t b = instr() - a;
        if (b < best) best = b;
    }
    return best > base_cost ? best - base_cost : 0;
}

static void audio(int ms) { static int16_t b[256]; for (int f = 0; f < 44100 * ms / 1000; f += 128) m2k_render(b, 128); }

static uint64_t worst;
static char worst_n[32];
static void row(const char *name, uint64_t c)
{
    printf("  %-22s %8llu\n", name, (unsigned long long)c);
    if (c > worst) { worst = c; snprintf(worst_n, sizeof worst_n, "%s", name); }
}

int main(int argc, char **argv)
{
    for (int r = 0; r < 20; r++) { uint64_t a = instr(); uint64_t b = instr() - a; if (!base_cost || b < base_cost) base_cost = b; }
    if (argc > 1 && !strcmp(argv[1], "--splash")) {
        /* every splash frame at 60 a second, as the host draws them */
        m2k_init(2000);
        uint64_t tot = 0, mx = 0;
        int n = 0, at = 0;
        for (uint32_t ms = 0; ms < 3000; ms += 16, n++) {
            uint64_t a = instr();
            m2k_frame(ms);
            uint64_t c = instr() - a - base_cost;
            tot += c;
            if (c > mx) { mx = c; at = (int)ms; }
            if (argc > 2 && !(n % 8)) printf("  %4u ms %8llu\n", (unsigned)ms, (unsigned long long)c);
        }
        printf("splash: %d frames, mean %llu, most %llu (at %d ms) instructions a frame\n", n,
               (unsigned long long)(tot / (uint64_t)n), (unsigned long long)mx, at);
        return 0;
    }
    m2k_init(2000);
    m2k_frame(0);
    now = 4000;
    m2k_frame(now);
    printf("instructions a frame (the least of 5), drawn and presented\n");
    cur_pat = 0; sq.queued = 0; lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[0].kit[t], P_COUNT);
    ui.page = PG_HOME; row("HOME stopped", frame_cost());
    seq_play(1); audio(600);
    row("HOME playing", frame_cost());
    for (int pg = 1; pg < PG_COUNT; pg++)
        for (int sub = 0; sub < nsub(pg); sub++) {
            ui.page = (uint8_t)pg; ui.sub = (uint8_t)sub;
            char o[24], nm[32];
            page_label(pg, sub, o);
            snprintf(nm, sizeof nm, "%s", o);
            audio(50);
            row(nm, frame_cost());
        }
    /* every machine's SYN page, playing (the marks animate) */
    uint64_t synw = 0;
    int synm = 0;
    for (int m = 0; m < M_COUNT; m++) {
        ui.trk = 0; set_machine(m); ui.page = PG_SYN; ui.sub = 0; ui.card_t = 0; audio(30);
        uint64_t c = frame_cost();
        if (c > synw) { synw = c; synm = m; }
    }
    char sn[32];
    snprintf(sn, sizeof sn, "SYN worst (%s)", MI[synm].name);
    row(sn, synw);
    /* a knob in focus on AMP; a held step on HOME */
    ui.page = PG_AMP; ui.sub = 0; ui.focus = 0; ui.focus_t = now + 100; row("AMP, ATCK in focus", frame_cost());
    ui.focus_t = 0;
    ui.page = PG_HOME; ui.grid = 1; ui.held[0] = 0; ui.nheld = 1; row("HOME, a step held", frame_cost());
    ui.nheld = 0; ui.grid = 0;
    /* the held windows */
    ui.page = PG_AMP; ui.latch = B_GLO + 1; row("GLO held (map)", frame_cost());
    ui.latch = B_HOME + 1; row("HOME held (patterns)", frame_cost());
    ui.latch = B_SEQ + 1; row("SEQ held (tools)", frame_cost());
    ui.latch = B_FX + 1; row("PERFORM", frame_cost());
    ui.latch = 0;
    store_.set[S_SCREEN] = 127; ui.page = PG_HOME; row("HOME dark", frame_cost());
    store_.set[S_SCREEN] = 0;
    printf("  heaviest: %s, %llu\n", worst_n, (unsigned long long)worst);
    return 0;
}
