/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on a computer: renders the factory patterns to WAV, drives the controls through a script and saves the
 * screen as PBM frames, and measures the render time. Build and run:
 *   cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c
 *   build/host/m2k_host build/host */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#define M2K_TEXT_AUDIT 1                  /* every frame's text checked (ta_check) */
#include "../firmware/src/m2k_core.c"
#include "m2k_bank_rev9.c"                /* the bank before, for the tests */

static void wav_write(const char *path, const int16_t *s, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t data = (uint32_t)frames * 4u, riff = 36 + data, sr = M2K_SR, br = sr * 4;
    uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    uint32_t fmt_len = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt_len, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f);
    fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(s, 4, (size_t)frames, f);
    fclose(f);
}

static void pbm_write(const char *path, const uint8_t *fbuf)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P4\n240 240\n");
    fwrite(fbuf, 1, M2K_FB_BYTES, f);
    fclose(f);
}

typedef struct { int ms; char kind; int a, b; const char *shot; } ev_t;

/* the tour: kind b button (a, b = down), e encoder (a, delta), k key (a, down), m MIDI in (a from, b status), s
 * screenshot */
static const ev_t TOUR[] = {
    {1200, 's', 0, 0, "00_boot"},             /* (the splash's waterfall) */
    {2520, 's', 0, 0, "01_home_stopped"},     /* (the splash 2.1 s, its wipe into HOME 0.4 s) */
    {2560, 'b', B_PLAY, 1, 0}, {2580, 'b', B_PLAY, 0, 0},
    {3400, 's', 0, 0, "02_home_playing"},
    {3500, 'e', E_K3, 4, 0}, {3560, 's', 0, 0, "03_home_tempo_roll"}, {3900, 's', 0, 0, "04_home_tempo"},
    {5600, 'e', E_SELECT, 1, 0},
    {5700, 'b', B_EDIT, 1, 0}, {5720, 'b', B_EDIT, 0, 0},
    {6300, 's', 0, 0, "05_syn_card"},
    {6400, 'e', E_K1, 3, 0}, {6460, 's', 0, 0, "06_syn_roll"}, {6800, 's', 0, 0, "07_syn_focus"},
    {7000, 'b', B_ENV, 1, 0}, {7020, 'b', B_ENV, 0, 0}, {7090, 's', 0, 0, "08_amp_dissolve"}, {7600, 's', 0, 0, "09_amp"},
    {7700, 'b', B_SEL, 1, 0}, {7720, 'b', B_SEL, 0, 0}, {8300, 's', 0, 0, "10_filt"},
    {8400, 'b', B_FX, 1, 0}, {8420, 'b', B_FX, 0, 0}, {8900, 's', 0, 0, "11_fx_eq"},
    {9000, 'b', B_FX, 1, 0}, {9020, 'b', B_FX, 0, 0}, {9500, 's', 0, 0, "12_fx_delay"},
    {9600, 'b', B_FX, 1, 0}, {9620, 'b', B_FX, 0, 0}, {9700, 'e', E_K2, 30, 0}, {10200, 's', 0, 0, "13_fx_destroy"},
    {10300, 'b', B_LFO, 1, 0}, {10320, 'b', B_LFO, 0, 0}, {11200, 's', 0, 0, "14_lfo"},
    {11300, 'b', B_GLO, 1, 0}, {11320, 'b', B_GLO, 0, 0}, {11900, 's', 0, 0, "15_glo_time"},
    {12000, 'b', B_GLO, 1, 0}, {12020, 'b', B_GLO, 0, 0}, {12600, 's', 0, 0, "16_glo_bus"},
    {12700, 'b', B_SEQ, 1, 0}, {12720, 'b', B_SEQ, 0, 0},
    {12800, 'b', B_EDIT, 1, 0}, {12820, 'b', B_EDIT, 0, 0},
    {13300, 'k', 7, 1, 0}, {13350, 'e', E_PRESET, 2, 0}, {13360, 'b', B_OCTU, 1, 0}, {13380, 'b', B_OCTU, 0, 0}, {13500, 's', 0, 0, "17a_grid_step_card"}, {13520, 'k', 7, 0, 0},
    {13600, 'k', 9, 1, 0}, {13700, 'e', E_K2, 20, 0}, {13800, 's', 0, 0, "17b_grid_lock"}, {14000, 'k', 9, 0, 0},
    {14200, 'e', E_ALGO, 1, 0}, {14260, 's', 0, 0, "18_machine_glitch"}, {14900, 's', 0, 0, "19_machine_card"},
    {15000, 'b', B_ARP, 1, 0}, {15020, 'b', B_ARP, 0, 0}, {15600, 's', 0, 0, "20_arp"},
    {15700, 'b', B_GLO, 1, 0}, {15720, 'b', B_GLO, 0, 0}, {16000, 'b', B_GLO, 1, 0}, {16020, 'b', B_GLO, 0, 0},
    {16300, 'b', B_GLO, 1, 0}, {16320, 'b', B_GLO, 0, 0}, {16600, 'b', B_GLO, 1, 0}, {16620, 'b', B_GLO, 0, 0},
    {17100, 's', 0, 0, "21_set_light"},     /* (taps 280 ms apart: closer, a second tap latches GLO) */
    {17200, 'e', E_K1, 1, 0}, {17700, 's', 0, 0, "22_set_dark"},
    {17800, 'b', B_HOME, 1, 0}, {17820, 'b', B_HOME, 0, 0}, {18400, 's', 0, 0, "23_home_dark"},
    {18500, 'b', B_EDIT, 1, 0}, {18520, 'b', B_EDIT, 0, 0}, {19200, 's', 0, 0, "24_syn_dark"},
    {19300, 'b', B_EDIT, 1, 0}, {19320, 'b', B_EDIT, 0, 0}, {19400, 'b', B_EDIT, 1, 0}, {19420, 'b', B_EDIT, 0, 0},
    {21100, 's', 0, 0, "26_syn_snd_dark"},
    {21200, 'x', 0, 0, 0},
};

/* the manual's scenes: B01's T1 with a trig of each of the eight types on every other step, in GRID (30); step 5's
 * FILTER trig held (31); the sound library, SYN > SND (25); GLO > IN (32); and on, T1's AMP and LFO pages (09b, 14b) */
static const ev_t MANUAL[] = {
    {2600, 'b', B_SEQ, 1, 0}, {2620, 'b', B_SEQ, 0, 0},
    {3400, 's', 0, 0, "30_grid_trig_types"},
    {5000, 'k', 7, 1, 0}, {5400, 's', 0, 0, "31_trig_filter"}, {5500, 'k', 7, 0, 0},
    {5600, 'b', B_SEQ, 1, 0}, {5620, 'b', B_SEQ, 0, 0},
    {5700, 'b', B_EDIT, 1, 0}, {5720, 'b', B_EDIT, 0, 0}, {5800, 'b', B_EDIT, 1, 0}, {5820, 'b', B_EDIT, 0, 0},
    {5900, 'b', B_EDIT, 1, 0}, {5920, 'b', B_EDIT, 0, 0}, {7200, 's', 0, 0, "25_syn_snd"},
    {7300, 'e', E_K4, 1, 0}, {8400, 's', 0, 0, "26_snd_kick"},
    {8500, 'b', B_GLO, 1, 0}, {9100, 's', 0, 0, "27_glo_help"}, {9200, 'b', B_ARP, 1, 0}, {9220, 'b', B_ARP, 0, 0},
    {9300, 'b', B_GLO, 0, 0}, {9400, 'b', B_HOME, 1, 0}, {9420, 'b', B_HOME, 0, 0},
    {9500, 'k', 0, 1, 0}, {9520, 'k', 0, 0, 0}, {9540, 'k', 4, 1, 0}, {9560, 'k', 4, 0, 0}, {10200, 's', 0, 0, "28_pads"},
    {10300, 'b', B_HOME, 1, 0}, {10400, 'k', 2, 1, 0}, {10420, 'k', 2, 0, 0}, {11000, 's', 0, 0, "29_home_held"},
    {11100, 'b', B_HOME, 0, 0}, {11200, 'b', B_EDIT, 1, 0}, {11900, 's', 0, 0, "29b_edit_held"}, {12000, 'b', B_EDIT, 0, 0},
    {12100, 'b', B_SEQ, 1, 0}, {12800, 's', 0, 0, "29c_seq_held"}, {12900, 'b', B_SEQ, 0, 0},
    {13000, 'b', B_HOME, 1, 0}, {13020, 'b', B_HOME, 0, 0}, {13600, 'b', B_GLO, 1, 0}, {13700, 'b', B_SEQ, 1, 0},
    {14300, 's', 0, 0, "29d_clear_ring"}, {14400, 'b', B_SEQ, 0, 0}, {14420, 'b', B_GLO, 0, 0},
    {14500, 'b', B_GLO, 1, 0}, {14520, 'b', B_GLO, 0, 0}, {14800, 'b', B_GLO, 1, 0}, {14820, 'b', B_GLO, 0, 0},
    {15100, 'b', B_GLO, 1, 0}, {15120, 'b', B_GLO, 0, 0}, {15400, 'b', B_GLO, 1, 0}, {15420, 'b', B_GLO, 0, 0},
    {15600, 'e', E_K3, 1, 0}, {16200, 's', 0, 0, "29e_ask"}, {16300, 'b', B_OCTD, 1, 0}, {16320, 'b', B_OCTD, 0, 0},
    {16400, 'b', B_PLAY, 1, 0}, {16420, 'b', B_PLAY, 0, 0}, {17100, 'b', B_FX, 1, 0}, {17200, 'k', 5, 1, 0},
    {17800, 's', 0, 0, "29f_perform"}, {17900, 'k', 5, 0, 0}, {17920, 'b', B_FX, 0, 0},
    {18000, 'b', B_LFO, 1, 0}, {18600, 's', 0, 0, "29g_lfo_held"}, {18700, 'b', B_LFO, 0, 0},
    {18800, 'b', B_GLO, 1, 0}, {19400, 's', 0, 0, "29h_glo_map"}, {19500, 'b', B_GLO, 0, 0},
    {19600, 'b', B_PLAY, 1, 0}, {19620, 'b', B_PLAY, 0, 0},
    /* SEQ double-tapped: its layer latched (29i, its message gone), tapped again let go; PLAY held a second alone:
     * its card (29j) */
    {20100, 'b', B_SEQ, 1, 0}, {20120, 'b', B_SEQ, 0, 0}, {20240, 'b', B_SEQ, 1, 0}, {20260, 'b', B_SEQ, 0, 0},
    {22600, 's', 0, 0, "29i_seq_latched"}, {22700, 'b', B_SEQ, 1, 0}, {22720, 'b', B_SEQ, 0, 0},
    {23000, 'b', B_PLAY, 1, 0}, {24200, 's', 0, 0, "29j_play_card"}, {24300, 'b', B_PLAY, 0, 0},
    {24400, 'b', B_PLAY, 1, 0}, {24420, 'b', B_PLAY, 0, 0},
    /* GLO > MIDI: SYIN to USB; GLO > IN: THRU on, a pulse in on the TRS jack (its square lit, not followed) (32) */
    {24600, 'b', B_GLO, 1, 0}, {24620, 'b', B_GLO, 0, 0}, {24800, 'e', E_K2, 1, 0},
    {25000, 'b', B_GLO, 1, 0}, {25020, 'b', B_GLO, 0, 0}, {25200, 'e', E_K2, 1, 0},
    {25750, 'm', M2K_TRS, 0xF8, 0}, {25800, 's', 0, 0, "32_glo_in"},
    /* T1's AMP on a swell (ATK 48, 0.17 s, its square rising; HOLD a half beat, DEC 1.4 s, REL 1.8 s: from GND-SIN's
     * INIT, HOLD 24 DEC 113 REL 37), ATK in focus (09b); its LFO 1 on BASE, a sine, SPD 60 (a bar, from 48) and DPTH 90
     * (+17 %), SPD in focus (14b) */
    {26000, 'b', B_ENV, 1, 0}, {26020, 'b', B_ENV, 0, 0},
    {26200, 'e', E_K2, -8, 0}, {26250, 'e', E_K3, -37, 0}, {26300, 'e', E_K4, 43, 0}, {26350, 'e', E_K1, 48, 0},
    {27100, 's', 0, 0, "09b_amp_swell"},
    {27200, 'b', B_LFO, 1, 0}, {27220, 'b', B_LFO, 0, 0},
    {27300, 'e', E_K2, 17, 0}, {27350, 'e', E_K3, 19, 0},
    {27500, 'b', B_LFO, 1, 0}, {27520, 'b', B_LFO, 0, 0},
    {27700, 'e', E_K4, 26, 0}, {27750, 'e', E_K2, 12, 0},
    {28500, 's', 0, 0, "14b_lfo_cycle"},
    /* GLO tapped, then ALGORITHM back to its first page and on to VERB: the reverb's decay drawn (16b) */
    {28700, 'b', B_GLO, 1, 0}, {28720, 'b', B_GLO, 0, 0}, {29000, 'e', E_ALGO, -1, 0}, {29050, 'e', E_ALGO, -1, 0}, {29100, 'e', E_ALGO, -1, 0},
    {29150, 'e', E_ALGO, -1, 0}, {29200, 'e', E_ALGO, -1, 0}, {29250, 'e', E_ALGO, -1, 0}, {29300, 'e', E_ALGO, -1, 0},
    {29400, 'e', E_ALGO, 1, 0}, {29450, 'e', E_ALGO, 1, 0},
    {30000, 's', 0, 0, "16b_glo_verb"},
    /* PADS off (GLO + ARP), the ARP page, MODE on, ARP twice: the arp's HOLD (in the header and on the stage); a chord
     * played and let go, the arp playing it on (20b) */
    {30050, 'b', B_GLO, 1, 0}, {30060, 'b', B_ARP, 1, 0}, {30070, 'b', B_ARP, 0, 0}, {30080, 'b', B_GLO, 0, 0},
    {30500, 'b', B_ARP, 1, 0}, {30520, 'b', B_ARP, 0, 0}, {30700, 'e', E_K1, 30, 0},
    {31000, 'b', B_ARP, 1, 0}, {31020, 'b', B_ARP, 0, 0}, {31100, 'b', B_ARP, 1, 0}, {31120, 'b', B_ARP, 0, 0},
    {31300, 'k', 0, 1, 0}, {31310, 'k', 4, 1, 0}, {31320, 'k', 7, 1, 0},
    {31500, 'k', 0, 0, 0}, {31510, 'k', 4, 0, 0}, {31520, 'k', 7, 0, 0},
    {33600, 's', 0, 0, "20b_arp_hold"},
    {33700, 'x', 0, 0, 0},
};

static void manual_scene(void)
{
    m2k_init(2000);
    cur_pat = 16;
    sq.queued = 16;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[16].kit[t], P_COUNT);
    for (int i = 0; i < 8; i++) pat[16].step[0][2 * i].flags = (uint8_t)(ST_TRIG | TTYPE[i]);
}

/* the text audit: in a frame just drawn, two texts whose ink touches (or is a dot apart), a text cut short by the
 * screen or a clip, one that leaves its window or its cell (a knob's, the stage's columns, the header's and the
 * footer's: begun anywhere in the cell, it lies in its content), one whose ink comes within 2 px of a rule (the bands'
 * and the cells'). Each found once, with when; the count is what the tour returns */
static int ta_found, ta_quiet;               /* (quiet: the self-test's own cases of the audit) */
static uint32_t ta_seen[256];
static int ta_nseen;
static void ta_report(const char *kind, const ta_t *a, const ta_t *b, int ms)
{
    uint32_t h = 2166136261u;
    for (const char *c = kind; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
    for (const char *c = a->s; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
    h = (h ^ (uint32_t)a->x0) * 16777619u;
    if (b) { for (const char *c = b->s; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u; h = (h ^ (uint32_t)b->x0) * 16777619u; }
    for (int i = 0; i < ta_nseen; i++) if (ta_seen[i] == h) return;
    if (ta_nseen < 256) ta_seen[ta_nseen++] = h;
    ta_found++;
    if (ta_quiet) return;
    if (b) printf("TEXT %s at %d ms: \"%s\" (%d,%d..%d,%d) and \"%s\" (%d,%d..%d,%d) [page %d.%d]\n", kind, ms, a->s, a->x0, a->y0,
                  a->x1, a->y1, b->s, b->x0, b->y0, b->x1, b->y1, ui.page, ui.sub);
    else printf("TEXT %s at %d ms: \"%s\" (%d,%d..%d,%d) [page %d.%d]\n", kind, ms, a->s, a->x0, a->y0, a->x1, a->y1, ui.page, ui.sub);
}

static void ta_check(int ms)
{
    if (ui.boot_t0 == 0xFFFFFFFFu || (uint32_t)ms - ui.boot_t0 < ui.splash_end) return;   /* the splash is art */
    if (ui.trans_t && (uint32_t)ms - ui.trans_t < 400u) return;                       /* a glitch between pages, the wipe */
    if (ui.toast_t && (uint32_t)ms - ui.toast_t < 130u) return;                       /* a message sweeping in */
    for (int i = 0; i < ta_n; i++) {
        const ta_t *a = &ta[i];
        if (a->cut) ta_report("cut", a, 0, ms);
        if (a->x1 < 0) continue;
        for (int k = ta_nbox - 1; k >= 0; k--) {         /* begun in a box, the last drawn on top */
            const ta_box_t *w = &ta_box[k];
            if (w->win) {                                 /* a window: it must end inside its frame */
                if (a->x0 > w->x0 && a->y0 > w->y0 && a->x0 < w->x1 && a->y0 < w->y1) {
                    if (a->x1 > w->x1 - 2 || a->y1 > w->y1 - 2) ta_report("past its window", a, 0, ms);
                    break;
                }
            } else if (a->x0 >= w->x0 && a->y0 >= w->y0 && a->x0 <= w->x1 && a->y0 <= w->y1) {   /* a cell: in its content */
                if (a->x0 < w->cx0 || a->y0 < w->cy0 || a->x1 > w->cx1 || a->y1 > w->cy1) {
                    ta_t c = {w->cx0, w->cy0, w->cx1, w->cy1, 0, "(its cell)"};
                    ta_report("past its cell", a, &c, ms);
                }
                break;
            }
        }
        for (int k = 0; k < ta_nrule; k++) {             /* no ink within 2 px of a rule */
            const ta_rule_t *r = &ta_rule[k];
            if (a->x0 <= r->x1 + 2 && r->x0 <= a->x1 + 2 && a->y0 <= r->y1 + 2 && r->y0 <= a->y1 + 2) {
                ta_t c = {r->x0, r->y0, r->x1, r->y1, 0, "(a rule)"};
                ta_report("on a rule", a, &c, ms);
                break;
            }
        }
        for (int j = i + 1; j < ta_n; j++) {
            const ta_t *b = &ta[j];
            if (a->x1 < 0 || b->x1 < 0) continue;
            if (a->x0 <= b->x1 + 1 && b->x0 <= a->x1 + 1 && a->y0 <= b->y1 + 1 && b->y0 <= a->y1 + 1) ta_report("touch", a, b, ms);
        }
    }
}

/* every screen the knobs reach, for the text audit: each machine on T1, each page and its pages, at rest and with
 * each knob turned to its least, middle and most; a held step with every tag; the library's lists */
static int text_sweep(void)
{
    const char *dump = getenv("M2K_SWEEP_DIR");
    m2k_init(2000);
    sq.rec = 1; sq.ready = 1; ui.pads = 1; sq.fill = 1; sq.arp_hold = 1;   /* the header at its fullest: REC and the modes */
    uint32_t now = 4000;
    static int16_t b[256];
    for (int f = 0; f < 44100 * 3; f += 128) m2k_render(b, 128);
    m2k_frame(now);
    for (int m = 0; m < M_COUNT; m++) {
        ui.trk = 0;
        set_machine(m);
        for (int pg = 0; pg < PG_COUNT; pg++)
            for (int sub = 0; sub < nsub(pg); sub++) {
                ui.page = (uint8_t)pg; ui.sub = (uint8_t)sub;
                now += 3000; ui.trans_t = 0; ui.toast_t = 0; ui.card_t = 0; ui.focus_t = 0;
                const uint8_t *fbuf = m2k_frame(now);
                ta_check((int)now);
                if (dump) {                    /* M2K_SWEEP_DIR: every machine's every page, to look at */
                    char path[512];
                    snprintf(path, sizeof path, "%s/m%02d_p%d_%d.pbm", dump, m, pg, sub);
                    pbm_write(path, fbuf);
                }
                for (int k = 0; k < 4; k++) {
                    int code = knob_param(pg, sub, k);
                    if (code == NONE) continue;
                    static const int V[3] = {0, 64, 127};
                    const int set = code >= 0xC0 && (code & 0x3F) < S_COUNT ? code & 0x3F : -1;   /* (not PROJ, SLOT ...) */
                    const uint8_t kept = set >= 0 ? store_.set[set] : 0;
                    for (int vi = 0; vi < 3; vi++) {
                        if (code < 0x80) PAT(cur_pat)->kit[0][code] = (uint8_t)V[vi];
                        else if (code < 0xC0) PAT(cur_pat)->glob[code & 0x7F] = (uint8_t)V[vi];
                        else if (set >= 0) store_.set[set] = (uint8_t)V[vi];
                        ui.focus = (uint8_t)k; ui.focus_t = now; ui.roll_t = now - 2000;
                        now += 40; ui.trans_t = 0; ui.toast_t = 0;
                        m2k_frame(now); ta_check((int)now);
                    }
                    if (set >= 0) store_.set[set] = kept;
                }
            }
    }
    /* the layers' knobs (SEQ held: LEN SPD SWNG BPM; PERFORM: the bus), each at its least, middle and most */
    static const uint8_t LAY[2] = {B_SEQ, B_FX};
    for (int l = 0; l < 2; l++) {
        ui.page = PG_AMP; ui.sub = 0; ui.btn[LAY[l]] = 1;          /* (held a moment: before its map shows) */
        for (int k = 0; k < 4; k++) {
            int code = knob_at(k);
            static const int V[3] = {0, 64, 127};
            for (int vi = 0; vi < 3; vi++) {
                if (code >= 0x80 && code < 0xC0) PAT(cur_pat)->glob[code & 0x7F] = (uint8_t)V[vi];
                ui.focus = (uint8_t)k; ui.focus_t = now; ui.roll_t = now - 2000;
                now += 40; ui.trans_t = 0; ui.toast_t = 0;
                ui.seq_t0 = ui.lay_t0[0] = (now - 100) | 1u;
                m2k_frame(now); ta_check((int)now);
            }
        }
        ui.btn[LAY[l]] = 0;
    }
    /* a held step in GRID with every tag: a condition, a ratchet, a trig type, accent and slide */
    m2k_init(2001);
    ui.page = PG_HOME; ui.grid = 1;
    m2k_step_t *st = &PAT(cur_pat)->step[0][3];
    st->flags = ST_TRIG | ST_ACC | ST_SLD | ST_NOFLT; st->cond = C_COUNT - 1; st->rtrg = 4; st->note = 61;
    ui.trk = 0; ui.held[0] = 3; ui.nheld = 1;
    now += 4000; ui.trans_t = 0; ui.toast_t = 0;
    m2k_frame(now); ta_check((int)now);
    ui.nheld = 0; ui.grid = 0;
    /* the library: every category shown */
    ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1);
    for (int c = 0; c < CAT_COUNT; c++) { ui.snd_cat = (uint8_t)c; now += 3000; ui.trans_t = 0; ui.toast_t = 0; m2k_frame(now); ta_check((int)now); }
    /* each layer latched, its map marked; each button's help card */
    static const uint8_t LAT[4] = {B_GLO, B_SEQ, B_HOME, B_FX};
    ui.page = PG_AMP; ui.sub = 0;
    for (int i = 0; i < 4; i++) {
        ui.latch = (uint8_t)(LAT[i] + 1);
        now += 3000; ui.trans_t = 0; ui.toast_t = 0; m2k_frame(now); ta_check((int)now);
    }
    ui.latch = 0;
    static const uint8_t CB[6] = {B_SEL, B_ENV, B_ARP, B_PLAY, B_OCTD, B_OCTU};
    for (int i = 0; i < 6; i++) {
        now += 3000; ui.trans_t = 0; ui.toast_t = 0;
        ui.btn[CB[i]] = 1; ui.card = (uint8_t)(CB[i] + 1); ui.card_used = 0; ui.card_t0 = now - 1500;
        m2k_frame(now); ta_check((int)now);
        ui.btn[CB[i]] = 0; ui.card = 0;
    }
    printf("text sweep: %s\n", ta_found ? "TEXT TOUCHES OR IS CUT (above)" : "every text clear");
    return ta_found != 0;
}

/* events against the screen's clock, a screenshot where asked, the sound rendered between */
static void run_events(const ev_t *E, const char *out)
{
    char path[512];
    int16_t blkbuf[256];
    int ms = 0, ei = 0;
    for (int s = 0; ; s += 128) {
        int now = (int)((int64_t)s * 1000 / M2K_SR);
        while (E[ei].ms <= now) {
            const ev_t *e = &E[ei++];
            if (e->kind == 'x') return;
            if (e->kind == 'b') m2k_button(e->a, e->b);
            if (e->kind == 'e') m2k_encoder(e->a, e->b);
            if (e->kind == 'k') m2k_key(e->a, e->b);
            if (e->kind == 'm') m2k_midi_from(e->a, e->b, 0, 0);
            if (e->kind == 's') {
                snprintf(path, sizeof path, "%s/%s.pbm", out, e->shot);
                pbm_write(path, m2k_frame((uint32_t)now));
            }
        }
        m2k_render(blkbuf, 128);
        if (now - ms >= 16) { ms = now; m2k_frame((uint32_t)now); ta_check(now); }
    }
}

/* measure()'s render on the store as it stands (no init): a pattern built in place */
static void fx_measure(int p, int mask, double sec, int16_t *keep, double *pk_db, double *rms_db)
{
    cur_pat = p;
    sq.queued = (uint8_t)p;
    pat[p].mute = (uint8_t)(~mask & 0x3F);
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
    seq_play(1);
    int frames = (int)(M2K_SR * sec) / 128 * 128;
    static int16_t buf[2 * 128];
    double ss = 0;
    int peak = 0;
    for (int f = 0; f < frames; f += 128) {
        m2k_render(buf, 128);
        for (int i = 0; i < 256; i++) { int v = buf[i], a = v < 0 ? -v : v; if (a > peak) peak = a; ss += (double)v * v; }
        if (keep) m_cpy(keep + 2 * f, buf, sizeof buf);
    }
    *pk_db = 20 * log10(peak / 32767.0 + 1e-9);
    *rms_db = 10 * log10(ss / (frames * 2.0) / (32767.0 * 32767.0) + 1e-12);
}

/* render seconds of pattern p with the tracks in mask; peak and rms (dB). demo_dry: without the bus's drive (a
 * part balanced under the knee before the bank's master drive) */
static int demo_dry;
static void measure(int p, int mask, double sec, int16_t *keep, double *pk_db, double *rms_db)
{
    m2k_init(2000);
    if (demo_dry) pat[p].glob[G_DRIV] = 0;
    cur_pat = p;
    sq.queued = (uint8_t)p;
    pat[p].mute = (uint8_t)(~mask & 0x3F);
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
    seq_play(1);
    int frames = (int)(M2K_SR * sec) / 128 * 128;
    static int16_t buf[2 * 128];
    double ss = 0;
    int peak = 0;
    for (int f = 0; f < frames; f += 128) {
        m2k_render(buf, 128);
        for (int i = 0; i < 256; i++) {
            int v = buf[i], a = v < 0 ? -v : v;
            if (a > peak) peak = a;
            ss += (double)v * v;
        }
        if (keep) m_cpy(keep + 2 * f, buf, sizeof buf);
    }
    *pk_db = 20 * log10(peak / 32767.0 + 1e-9);
    *rms_db = 10 * log10(ss / (frames * 2.0) / (32767.0 * 32767.0) + 1e-12);
}

static int analyze(const char *out)
{
    char path[512];
    int bad = 0;
    printf("pattern   mix pk/rms   T1        T2        T3        T4        T5        T6     (rms dB, crest)\n");
    for (int p = 0; p < 16; p++) {             /* bank A: the demos */
        double pk, rms;
        double sec = 10.0;
        int frames = (int)(M2K_SR * sec) / 128 * 128;
        int16_t *keep = malloc((size_t)frames * 4);
        measure(p, 0x3F, sec, keep, &pk, &rms);
        snprintf(path, sizeof path, "%s/a%02d_%s.wav", out, p + 1, pat[p].name);
        for (char *c = path + strlen(out) + 1; *c; c++) if (*c == ' ') *c = '_';
        wav_write(path, keep, frames);
        free(keep);
        printf("A%02d %-7s %5.1f/%5.1f", p + 1, pat[p].name, pk, rms);
        if (pk > -0.3) { printf(" CLIP"); bad++; }
        for (int t = 0; t < M2K_TRACKS; t++) {
            double tp, tr;
            measure(p, 1 << t, 6.0, 0, &tp, &tr);
            const char *flag = tr < -48 ? "!quiet" : (tp - tr) > 30 ? "!click" : "";
            if (*flag) bad++;
            printf("  %5.1f/%2.0f%-6s", tr, tp - tr, flag);
        }
        printf("\n");
    }
    printf("%d flags\n", bad);
    return 0;
}

/* bank A's patterns for listening: each from its start, every track, for some seconds (the batch's slots, or all) */
static int demo_render(const char *out, int argc, char **argv)
{
    double sec = argc > 3 ? atof(argv[3]) : 30.0;
    char path[512];
    for (int p = 0; p < 16; p++) {
        int want = argc <= 4;
        for (int i = 4; i < argc; i++) want |= atoi(argv[i]) == p + 1;
        if (!want) continue;
        int frames = (int)(M2K_SR * sec) / 128 * 128;
        int16_t *keep = malloc((size_t)frames * 4);
        double pk, rms;
        measure(p, 0x3F, sec, keep, &pk, &rms);
        snprintf(path, sizeof path, "%s/a%02d_%s.wav", out, p + 1, pat[p].name);
        wav_write(path, keep, frames);
        free(keep);
        printf("A%02d %-7s %.0f s: peak %.1f dBFS, rms %.1f dBFS -> %s\n", p + 1, pat[p].name, sec, pk, rms, path);
    }
    return 0;
}

/* each track of bank A alone, two loops, and every trig it plays: the frame, the note the engine was given, the
 * track's parameters as they stood; then its most played note held on its kit (tools/demo_pitch.py measures each
 * against its note). The factory is rebuilt between tracks, so the held note's kit change does not stay */
/* a bank of 16 patterns as packed blobs (each: its length, two bytes LE, then pat_pack's bytes): written from bank A,
 * read back over bank A after every m2k_init, so a bank kept as a fixture plays on today's engine (converted on load) */
static uint8_t bank_fix[16 * BLOB_MAX];
static int bank_len;

static int bank_export(const char *path)
{
    static uint8_t bl[BLOB_MAX];
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    m2k_init(2000);
    for (int p = 0; p < 16; p++) {
        int n = pat_pack(&pat[p], bl);
        uint8_t h[2] = {(uint8_t)n, (uint8_t)(n >> 8)};
        fwrite(h, 1, 2, f);
        fwrite(bl, 1, (size_t)n, f);
    }
    fclose(f);
    printf("bank A: 16 patterns into %s\n", path);
    return 0;
}

static int bank_load(void)
{
    int o = 0;
    for (int p = 0; p < 16 && o + 2 <= bank_len; p++) {
        int n = bank_fix[o] | (bank_fix[o + 1] << 8);
        if (o + 2 + n > bank_len || !pat_unpack(&pat[p], bank_fix + o + 2, n)) return 0;
        o += 2 + n;
    }
    return 1;
}

static int pitch_dump(const char *out)
{
    char path[512];
    for (int p = 0; p < 16; p++) {
        m2k_init(2000);
        if (bank_len && !bank_load()) { fprintf(stderr, "the bank fixture does not load\n"); return 1; }
        int steps = pat[p].glob[G_LEN] < 1 ? 1 : pat[p].glob[G_LEN] > 64 ? 64 : pat[p].glob[G_LEN];
        int frames = (int)(2.0 * steps * M2K_SR * 15.0 / (30 + pat[p].glob[G_TMPO]) + M2K_SR) / 16 * 16;
        for (int t = 0; t < M2K_TRACKS; t++) {
            m2k_init(2000);
            if (bank_len) bank_load();
            cur_pat = p;
            sq.queued = (uint8_t)p;
            pat[p].mute = (uint8_t)(~(1 << t) & 0x3F);
            lockmap_build();
            for (int k = 0; k < M2K_TRACKS; k++) m_cpy(trk[k].eff, pat[p].kit[k], P_COUNT);
            seq_play(1);
            int16_t *keep = malloc((size_t)frames * 4);
            snprintf(path, sizeof path, "%s/p%02d_t%d.txt", out, p + 1, t + 1);
            FILE *ev = fopen(path, "w");
            if (!ev || !keep) { perror(path); return 1; }
            fprintf(ev, "# A%02d %s T%d machine %d bpm %d steps %d\n", p + 1, pat[p].name, t + 1, pat[p].machine[t],
                    30 + pat[p].glob[G_TMPO], steps);
            uint32_t seen = mon.trig[t];
            for (int f = 0; f < frames; f += 16) {
                m2k_render(keep + 2 * f, 16);
                if (mon.trig[t] != seen) {
                    fprintf(ev, "%d %d %d %u", f, trk[t].note, trk[t].vel, (unsigned)(mon.trig[t] - seen));
                    for (int k = 0; k < P_COUNT; k++) fprintf(ev, " %d", trk[t].eff[k]);
                    fprintf(ev, "\n");
                    seen = mon.trig[t];
                }
            }
            fclose(ev);
            snprintf(path, sizeof path, "%s/p%02d_t%d.wav", out, p + 1, t + 1);
            wav_write(path, keep, frames);
            /* and the track's most played note held 1.5 s on its kit, the sequencer stopped: the engine's own tuning */
            int count[128] = {0}, note = 60, best = 0;
            for (int s = 0; s < steps; s++)
                if ((pat[p].step[t][s].flags & ST_TRIG) && ++count[pat[p].step[t][s].note & 127] > best) {
                    best = count[pat[p].step[t][s].note & 127];
                    note = pat[p].step[t][s].note & 127;
                }
            m2k_init(2000);
            if (bank_len) bank_load();
            cur_pat = p;
            int held = (int)(1.5 * M2K_SR) / 16 * 16;
            pat[p].kit[t][P_HOLD] = 127;
            pat[p].kit[t][P_DEC] = 127;
            m2k_midi(0x90 | t, note, 100);
            for (int f = 0; f < held; f += 16) m2k_render(keep + 2 * f, 16);
            snprintf(path, sizeof path, "%s/p%02d_t%d_held%d.wav", out, p + 1, t + 1, note);
            wav_write(path, keep, held);
            free(keep);
        }
    }
    printf("bank A: 16 patterns x %d tracks into %s\n", M2K_TRACKS, out);
    return 0;
}

#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); fails++; } else printf("ok   %s\n", m); } while (0)

/* B01, the blank kit, opened as the panel opens a pattern: its locks and every track's settings (A01's would stay
 * otherwise, and an LFO of A01's reaching across tracks with them) */
static void open_b01(void)
{
    cur_pat = 16;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[16].kit[t], P_COUNT);
}

/* the bus's tape at a TAPE value on a held 1047 Hz sine: the pitch per cycle (between interpolated rising zero
 * crossings), its slow part (a 159 ms average: one flutter period) and the rest; peak deviation in cents of each */
static void tape_pitch_at(int tape, double *wow_c, double *flut_c)
{
    m2k_init(7);
    open_b01();
    kit_default(&pat[16], 0, M_GSIN);              /* the plain sine the references were measured on */
    pat[16].kit[0][P_SYN + 3] = 0;                 /* no pitch envelope */
    pat[16].kit[0][P_SYN + 5] = 30;                /* the click the bus's references were measured with (GND-SIN's init before 12) */
    pat[16].kit[0][P_HOLD] = 127;
    pat[16].kit[0][P_DEC] = 127;
    pat[16].glob[G_TAPE] = (uint8_t)tape;
    m2k_midi(0x90, 84, 100);
    const int frames = 5 * M2K_SR / 16 * 16;                   /* whole blocks: the render writes 16 at a time */
    int16_t *b = malloc((size_t)frames * 4);
    for (int f = 0; f < frames; f += 16) m2k_render(b + 2 * f, 16);
    static double hz[8000];
    int n = 0;
    double last = -1;
    for (int i = M2K_SR; i < frames - 1 && n < 8000; i++) {
        double a = b[2 * i], c = b[2 * i + 2];
        if (a < 0 && c >= 0) {
            double x = i + a / (a - c);
            if (last > 0) hz[n++] = M2K_SR / (x - last);
            last = x;
        }
    }
    free(b);
    int w = (int)(0.159 * 1047);
    double mean = 0;
    for (int i = 0; i < n; i++) mean += hz[i];
    mean /= n;
    *wow_c = *flut_c = 0;
    for (int i = w; i < n - w; i++) {
        double s = 0;
        for (int k = -w / 2; k < w / 2; k++) s += hz[i + k];
        s /= 2 * (w / 2);
        double cw = fabs(1200 * log2(s / mean)), cf = fabs(1200 * log2(hz[i] / s));
        if (cw > *wow_c) *wow_c = cw;
        if (cf > *flut_c) *flut_c = cf;
    }
}

/* ------------------------------------------------------------------------------------- the bus measured */

/* the reference the bus's effects are measured on (B01; nothing left to chance, so two renders differ only by the
 * control): with tracks, a kick on the beat, eighth hats, a saw bass line and a held chord, at 120; without, the
 * sequencer running on silence (the tape rolls: hiss and dust, and the glitch's clock) */
static void bus_ref(int tracks, int ctl, int v, int gsiz)
{
    m2k_init(2000);
    m2k_pattern_t *p = &pat[16];
    p->glob[G_TMPO] = 120 - 30;
    p->glob[G_LEN] = 16;
    p->glob[G_VERB] = 0;
    p->glob[G_GSIZ] = (uint8_t)gsiz;
    if (tracks) {
        static const uint8_t M[4] = {M_GSIN, M_GPRC, M_SAW, M_ENS}, LEV[4] = {87, 55, 87, 66};
        for (int t = 0; t < 4; t++) { kit_default(p, t, M[t]); p->kit[t][P_LEVL] = LEV[t]; p->kit[t][P_VOL] = 127; }
        p->kit[0][P_SYN + 3] = 60; p->kit[0][P_SYN + 4] = 40; p->kit[0][P_SYN + 5] = 30;   /* PDEP PDEC CLIK (page 12) */
        p->kit[0][P_DEC] = 30;
        p->kit[1][P_SYN] = 112; p->kit[1][P_DEC] = 12; p->kit[1][P_BASE] = 40;
        p->kit[2][P_DEC] = 44; p->kit[2][P_WDTH] = 40;
        p->kit[3][P_HOLD] = 127; p->kit[3][P_DEC] = 127; p->kit[3][P_GATE] = 127; p->kit[3][P_WDTH] = 40;
        static const uint8_t BASS[16] = {36, 0, 0, 36, 0, 0, 43, 0, 0, 0, 46, 0, 0, 0, 0, 0};
        for (int s = 0; s < 16; s++) {
            if (s % 4 == 0) p->step[0][s] = (m2k_step_t){.note = 36, .flags = ST_TRIG};
            if (s % 2 == 1) p->step[1][s] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
            if (BASS[s]) p->step[2][s] = (m2k_step_t){.note = BASS[s], .flags = ST_TRIG};
        }
        p->step[3][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
        for (int t = 0; t < 4; t++) feel_kit_v15(p->kit[t]);   /* written on format 15's amp: the same mix since */
    }
    p->glob[ctl] = (uint8_t)v;
    cur_pat = 16;
    sq.queued = 16;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, p->kit[t], P_COUNT);
    seq_play(1);
}

/* the reverb measured alone (its bus_block, every other control off), at DCAY dc and DAMP dm: out[0] the RT60 of the
 * whole, out[1..5] of the octaves at 250 Hz .. 4 kHz (1.5 s of white noise, +-12000 into the send, then its decay:
 * Schroeder's integral, T20 x 3, or T10 x 6 when the floor comes first), out[6] its impulse response's energy in
 * 177 Hz .. 5.7 kHz, out[7] in all and out[8..12] in each octave (dB against the impulse's, 30000 into the send) */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } vbq_t;
static void vbq_set(vbq_t *f, int hp, double fc)
{
    double w = 2 * M_PI * fc / M2K_SR, c = cos(w), sn = sin(w), al = sn / (2 * 0.7071), a0 = 1 + al;
    f->b0 = (hp ? (1 + c) : (1 - c)) / 2 / a0; f->b1 = (hp ? -(1 + c) : (1 - c)) / a0; f->b2 = f->b0;
    f->a1 = -2 * c / a0; f->a2 = (1 - al) / a0; f->z1 = f->z2 = 0;
}
static double vbq(vbq_t *f, double x)
{
    double y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}
typedef struct { vbq_t f[4]; } vband_t;                  /* two high-passes, two low-passes */
static void vband_set(vband_t *b, double lo, double hi) { for (int k = 0; k < 4; k++) vbq_set(&b->f[k], k < 2, k < 2 ? lo : hi); }
static double vband(vband_t *b, double x) { for (int k = 0; k < 4; k++) x = vbq(&b->f[k], x); return x; }
static double vt20(const double *e, int n)
{
    double tot = 0, acc, t5 = -1, t15 = -1, t25 = -1;
    for (int i = 0; i < n; i++) tot += e[i];
    if (tot <= 0) return 0;
    acc = tot;
    for (int i = 0; i < n && t25 < 0; i++) {
        double d = 10 * log10(acc / tot + 1e-30);
        if (t5 < 0 && d <= -5) t5 = (double)i / M2K_SR;
        if (t15 < 0 && d <= -15) t15 = (double)i / M2K_SR;
        if (d <= -25) t25 = (double)i / M2K_SR;
        acc -= e[i];
    }
    return t25 >= 0 && t5 >= 0 ? 3 * (t25 - t5) : t15 >= 0 && t5 >= 0 ? 6 * (t15 - t5) : 0;
}
static void verb_at(int dc, int dm, double sec, double *out)
{
    static int32_t L[M2K_SUB], R[M2K_SUB], rs[M2K_SUB];
    static int16_t o[2 * M2K_SUB];
    static const double FC[5] = {250, 500, 1000, 2000, 4000};
    int cut = (int)(1.5 * M2K_SR) / M2K_SUB * M2K_SUB, n = (int)(sec * M2K_SR) / M2K_SUB * M2K_SUB;
    double *e[6];
    vband_t bd[5][2], mid[2];
    for (int k = 0; k < 6; k++) e[k] = calloc((size_t)n, sizeof(double));
    for (int k = 0; k < 5; k++) for (int c = 0; c < 2; c++) vband_set(&bd[k][c], FC[k] / sqrt(2), FC[k] * sqrt(2));
    for (int pass = 0; pass < 2; pass++) {               /* the noise, then the impulse */
        m2k_init(7);
        m2k_pattern_t *p = &pat[16];
        p->glob[G_VERB] = (uint8_t)dc;
        p->glob[G_DAMP] = (uint8_t)dm;
        cur_pat = 16;
        uint32_t nz = 12345;
        double ea = 0, em = 0, eb[5] = {0};
        vband_set(&mid[0], 177, 5657); vband_set(&mid[1], 177, 5657);
        if (pass) for (int k = 0; k < 5; k++) for (int c = 0; c < 2; c++) vband_set(&bd[k][c], FC[k] / sqrt(2), FC[k] * sqrt(2));
        for (int f = 0; f < (pass ? n : cut + n); f += M2K_SUB) {
            m_set(L, 0, sizeof L); m_set(R, 0, sizeof R); m_set(rs, 0, sizeof rs);
            if (!pass && f < cut)
                for (int i = 0; i < M2K_SUB; i++) { nz ^= nz << 13; nz ^= nz >> 17; nz ^= nz << 5; rs[i] = (int32_t)(nz % 24001) - 12000; }
            if (pass && !f) rs[0] = 30000;
            bus_block(L, R, rs, o);
            for (int i = 0; i < M2K_SUB; i++) {
                double l = o[2 * i], r = o[2 * i + 1];
                if (!pass) {
                    for (int k = 0; k < 5; k++) {
                        double bl = vband(&bd[k][0], l), br = vband(&bd[k][1], r);
                        if (f >= cut) e[k + 1][f - cut + i] = bl * bl + br * br;
                    }
                    if (f >= cut) e[0][f - cut + i] = l * l + r * r;
                } else {
                    double ml = vband(&mid[0], l), mr = vband(&mid[1], r);
                    ea += l * l + r * r; em += ml * ml + mr * mr;
                    for (int k = 0; k < 5; k++) { double bl = vband(&bd[k][0], l), br = vband(&bd[k][1], r); eb[k] += bl * bl + br * br; }
                }
            }
        }
        if (pass) {
            out[6] = 10 * log10(em / 9e8 + 1e-30);
            out[7] = 10 * log10(ea / 9e8 + 1e-30);
            for (int k = 0; k < 5; k++) out[8 + k] = 10 * log10(eb[k] / 9e8 + 1e-30);
        }
    }
    for (int k = 0; k < 6; k++) { out[k] = vt20(e[k], n); free(e[k]); }
}

/* GLCH on the reference mix, every value over BARS bars (tools/conv17.py): the events started a bar, the share of the
 * time one sounds, the glitch's energy (the output less GLCH 0's, against GLCH 0's, dB) */
static int glch_curve(int bars)
{
    const int frames = bars * 2 * M2K_SR / 16 * 16;
    int16_t *dry = malloc((size_t)frames * 4), *b = malloc((size_t)frames * 4);
    bus_ref(1, G_GLCH, 0, 64);
    for (int f = 0; f < frames; f += 16) m2k_render(dry + 2 * f, 16);
    double ed = 0;
    for (int i = 0; i < 2 * frames; i++) ed += (double)dry[i] * dry[i];
    for (int v = 0; v < 128; v++) {
        bus_ref(1, G_GLCH, v, 64);
        long starts = 0, on = 0;
        int last = 0;
        for (int f = 0; f < frames; f += 16) {
            m2k_render(b + 2 * f, 16);
            starts += mon.glitch && !last;
            on += mon.glitch != 0;
            last = mon.glitch;
        }
        double e = 0;
        for (int i = 0; i < 2 * frames; i++) { double d = (double)b[i] - dry[i]; e += d * d; }
        printf("%d %.4f %.3f %.2f\n", v, (double)starts / bars, 100.0 * on / (frames / 16), e > 0 ? 10 * log10(e / ed) : -150.0);
        fflush(stdout);
    }
    free(dry);
    free(b);
    return 0;
}

/* the reverb on the pairs read from stdin ("DCAY DAMP SECONDS" a line), its measures a line each (tools/conv17.py) */
static int verb_grid(void)
{
    int dc, dm;
    double sec, m[13];
    while (scanf("%d %d %lf", &dc, &dm, &sec) == 3) {
        verb_at(dc, dm, sec, m);
        printf("%d %d %.3f %.3f %.3f %.3f %.3f %.3f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", dc, dm, m[0], m[1], m[2], m[3], m[4], m[5], m[6],
               m[7], m[8], m[9], m[10], m[11], m[12]);
        fflush(stdout);
    }
    return 0;
}

/* every value of the bus's controls, measured on the engine's output (DIR/bus.tsv; the hiss as WAVs, separated from
 * its crackle by tools/bus_curves.py):
 *   GLCH  repeats started per bar and the share of the time one sounds (the engine's glitch flag), 256 bars of the
 *         sequencer on silence at 120, a sixteenth's slice; and the glitch's energy on the reference mix (its output
 *         less GLCH 0's, against GLCH 0's, dB; 64 bars)
 *   ADPC  DRIV  the reference mix through the control against itself without: the gain (least squares) and what is
 *         left over it (dB under the clean part), the output's rms and peak
 *   TAPE  wow and flutter (cents, a held 1047 Hz sine), the loss at 8.4 kHz (a held sine, dB)
 *   DCAY  the reverb's RT60 (the whole; 250 Hz, 1 kHz, 4 kHz) and its energy (dB), at DAMP 32
 *   DAMP  the same at DCAY 80 */
static int bus_measure(const char *dir)
{
    char path[512];
    snprintf(path, sizeof path, "%s/bus.tsv", dir);
    FILE *o = fopen(path, "w");
    if (!o) { perror(path); return 1; }
    fprintf(o, "ctl\tv\ta\tb\tc\td\te\n");
    /* the glitch: starts per bar, duty (256 bars: a chance's count wants many) */
    for (int v = 0; v < 128; v++) {
        bus_ref(0, G_GLCH, v, 64);
        const int bars = 256, frames = bars * 2 * M2K_SR / 16 * 16;
        int16_t lr[32];
        long starts = 0, on = 0, blocks = 0;
        int last = 0;
        for (int f = 0; f < frames; f += 16) {
            m2k_render(lr, 16);
            starts += mon.glitch && !last;
            on += mon.glitch != 0;
            last = mon.glitch;
            blocks++;
        }
        fprintf(o, "GLCH\t%d\t%.4f\t%.3f\t0\t0\n", v, (double)starts / bars, 100.0 * on / blocks);
    }
    {                                                    /* the glitch's energy over the reference mix */
        const int frames = 64 * 2 * M2K_SR / 16 * 16;
        int16_t *dry = malloc((size_t)frames * 4), *b = malloc((size_t)frames * 4);
        bus_ref(1, G_GLCH, 0, 64);
        for (int f = 0; f < frames; f += 16) m2k_render(dry + 2 * f, 16);
        double ed = 0;
        for (int i = 0; i < 2 * frames; i++) ed += (double)dry[i] * dry[i];
        for (int v = 0; v < 128; v++) {
            bus_ref(1, G_GLCH, v, 64);
            for (int f = 0; f < frames; f += 16) m2k_render(b + 2 * f, 16);
            double e = 0;
            for (int i = 0; i < 2 * frames; i++) { double d = (double)b[i] - dry[i]; e += d * d; }
            fprintf(o, "GLCE\t%d\t%.2f\t0\t0\t0\n", v, e > 0 ? 10 * log10(e / ed) : -150.0);
        }
        free(dry);
        free(b);
    }
    for (int v = 1; v < 128; v++) {                      /* the reverb: DCAY at DAMP 32, DAMP at DCAY 80 */
        double m[13];
        verb_at(v, 32, 1.5 + 0.18 * pow(80, (v - 1) / 126.0), m);
        fprintf(o, "DCAY\t%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.2f\n", v, m[0], m[1], m[3], m[5], m[6]);
    }
    for (int v = 0; v < 128; v++) {
        double m[13];
        verb_at(80, v, 4.0, m);
        fprintf(o, "DAMP\t%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.2f\n", v, m[0], m[1], m[3], m[5], m[6]);
    }
    /* the hiss and dust: 12 s after a second's fade-in, as mono WAVs (the left: both carry the same) */
    {
        const int settle = M2K_SR / 16 * 16, frames = 12 * M2K_SR / 16 * 16;
        int16_t *b = malloc((size_t)frames * 4);
        for (int v = 0; v < 128; v++) {
            bus_ref(0, G_HISS, v, 64);
            int16_t lr[32];
            for (int f = 0; f < settle; f += 16) m2k_render(lr, 16);
            for (int f = 0; f < frames; f += 16) m2k_render(b + 2 * f, 16);
            for (int i = 0; i < frames; i++) b[i] = b[2 * i];
            snprintf(path, sizeof path, "%s/hiss_%03d.wav", dir, v);
            FILE *w = fopen(path, "wb");
            if (!w) { perror(path); return 1; }
            uint32_t data = (uint32_t)frames * 2u, riff = 36 + data, sr = M2K_SR, br = sr * 2, fl = 16;
            uint16_t pcm = 1, ch = 1, ba = 2, bits = 16;
            fwrite("RIFF", 1, 4, w); fwrite(&riff, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w); fwrite(&fl, 4, 1, w);
            fwrite(&pcm, 2, 1, w); fwrite(&ch, 2, 1, w); fwrite(&sr, 4, 1, w); fwrite(&br, 4, 1, w);
            fwrite(&ba, 2, 1, w); fwrite(&bits, 2, 1, w); fwrite("data", 1, 4, w); fwrite(&data, 4, 1, w);
            fwrite(b, 2, (size_t)frames, w);
            fclose(w);
        }
        free(b);
    }
    /* the crunch and the drive against the clean mix */
    {
        const int frames = 8 * M2K_SR / 16 * 16;
        int16_t *ref = malloc((size_t)frames * 4), *b = malloc((size_t)frames * 4);
        static const int CTL[2] = {G_ADPC, G_DRIV};
        for (int c = 0; c < 2; c++)
            for (int v = 0; v < 128; v++) {
                bus_ref(1, CTL[c], v, 64);
                int16_t *dst = v ? b : ref;
                for (int f = 0; f < frames; f += 16) m2k_render(dst + 2 * f, 16);
                double xy = 0, xx = 0, yy = 0, pk = 0;
                for (int i = 2 * M2K_SR; i < 2 * frames; i++) {        /* past the first second */
                    double x = ref[i], y = dst[i];
                    xy += x * y; xx += x * x; yy += y * y;
                    if (fabs(y) > pk) pk = fabs(y);
                }
                double g = xy / xx, res = yy - 2 * g * xy + g * g * xx, n = 2.0 * frames - 2 * M2K_SR;
                fprintf(o, "%s\t%d\t%.3f\t%.2f\t%.2f\t%.2f\n", c ? "DRIV" : "ADPC", v, 20 * log10(g),
                        res > 0 ? 10 * log10(res / (g * g * xx)) : -150.0, 10 * log10(yy / n / (32767.0 * 32767.0)),
                        20 * log10(pk / 32767.0));
            }
        free(ref);
        free(b);
    }
    /* the tape: wow, flutter; the loss at 8.4 kHz */
    {
        const int frames = 2 * M2K_SR / 16 * 16;
        int16_t *b = malloc((size_t)frames * 4);
        double r0 = 0;
        for (int v = 0; v < 128; v++) {
            double w, fl;
            tape_pitch_at(v, &w, &fl);
            m2k_init(7);
            open_b01();
            kit_default(&pat[16], 0, M_GSIN);
            pat[16].kit[0][P_SYN + 3] = 0;
            pat[16].kit[0][P_SYN + 5] = 30;
            pat[16].kit[0][P_HOLD] = 127;
            pat[16].kit[0][P_DEC] = 127;
            pat[16].glob[G_TAPE] = (uint8_t)v;
            m2k_midi(0x90, 120, 100);
            for (int f = 0; f < frames; f += 16) m2k_render(b + 2 * f, 16);
            double s = 0;
            for (int i = M2K_SR; i < 2 * frames; i++) s += (double)b[i] * b[i];
            if (!v) r0 = s;
            fprintf(o, "TAPE\t%d\t%.2f\t%.2f\t%.2f\t0\n", v, w, fl, 10 * log10(s / r0));
        }
        free(b);
    }
    fclose(o);
    printf("the bus's controls, every value: %s/bus.tsv (and the hiss's WAVs)\n", dir);
    return 0;
}

/* the trig types: step 1 trigs ALL, step 5 with SKIP masked; after step 5, the amp run's level (a short GND-SIN, silent
 * by then), the filter run's brightness against just before (a held SWAV-SAW, its gate open, its low-pass shut and
 * opened by its envelope, closed again by then) and whether LFO 1 started over (a slow rising ramp, TRIG) */
static void trig_probe(unsigned skip, double *amp_db, double *bright_db, int *lfo_restart)
{
    const int at = 4 * M2K_SR / 8;                                  /* step 5 at 120: 0.5 s */
    const int frames = (at + M2K_SR / 16) / 16 * 16;
    int16_t *b = malloc((size_t)frames * 4);
    for (int run = 0; run < 2; run++) {
        m2k_init(7);
        open_b01();
        m2k_pattern_t *p = &pat[16];
        p->glob[G_TMPO] = 120 - 30;
        p->glob[G_LEN] = 16;
        kit_default(p, 0, run ? M_SAW : M_GSIN);
        uint8_t *k = p->kit[0];
        k[P_ATK] = 0;
        if (run) { k[P_HOLD] = 127; k[P_DEC] = 127; k[P_GATE] = 127; k[P_BASE] = 0; k[P_WDTH] = 20; k[P_FATK] = 0; k[P_FDEC] = 30; k[P_WOF] = 127; }
        else { k[P_HOLD] = 0; k[P_DEC] = 30; k[P_BASE] = 0; k[P_WDTH] = 127; k[P_WOF] = 64; }
        k[P_LFO + L_TRCK] = 0; k[P_LFO + L_DEST] = P_PAN + 1; k[P_LFO + L_WAVE] = 92; k[P_LFO + L_TRIG] = 30;   /* ramp, TRIG */
        k[P_LFO + L_MULT] = 0; k[P_LFO + L_SPD] = 20; k[P_LFO + L_PHAS] = 0; k[P_LFO + L_DPTH] = 64;              /* 0.16 Hz, no depth */
        p->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
        p->step[0][4] = (m2k_step_t){.note = 48, .flags = (uint8_t)(ST_TRIG | skip)};
        open_b01();
        seq_play(1);
        int32_t lfo0 = 0;
        for (int fr = 0; fr < frames; fr += 16) {
            if (fr == (at - M2K_SR / 100) / 16 * 16) lfo0 = mon.lfo[0][0];
            m2k_render(b + 2 * fr, 16);
        }
        if (!run) *lfo_restart = mon.lfo[0][0] < lfo0 / 2;
        /* the level after; the brightness as the share of the slope's energy in the signal's, after against before */
        double e0 = 0, e1 = 0, d0 = 0, d1 = 0;
        for (int i = at - M2K_SR / 16; i < at - M2K_SR / 100; i++) {
            double d = (double)b[2 * i] - b[2 * i - 2];
            e0 += (double)b[2 * i] * b[2 * i]; d0 += d * d;
        }
        for (int i = at + M2K_SR / 200; i < frames; i++) {
            double d = (double)b[2 * i] - b[2 * i - 2];
            e1 += (double)b[2 * i] * b[2 * i]; d1 += d * d;
        }
        if (run) *bright_db = 10 * log10(((d1 + 1) / (e1 + 1)) / ((d0 + 1) / (e0 + 1)));
        else *amp_db = 10 * log10(e1 / (frames - at - M2K_SR / 200) / (32768.0 * 32768.0) + 1e-12);
    }
    free(b);
}

/* the second core, played in turn: its job run when started (1) or when waited for (2); par_queued: a job that put
 * MIDI in the queue (the first core's alone) */
static int par_mode, par_queued;
static void (*par_pending)(void);
static void par_run(void (*job)(void)) { uint8_t w = mx.qw; job(); par_queued |= mx.qw != w; }
static int par_test_begin(void (*job)(void)) { if (par_mode == 1) par_run(job); else par_pending = job; return 1; }
static void par_test_end(void) { if (par_mode == 2 && par_pending) par_run(par_pending); par_pending = 0; }

/* the sequencer's tests: pattern i bare and open (INIT: no trigs or locks, no swing, every track at 1x, 120 BPM, 16
 * steps); one block rendered at a time, the tracks that trigged in it as a mask */
static uint8_t sblk_last[M2K_TRACKS];
static void sblk_sync(void) { for (int t = 0; t < M2K_TRACKS; t++) sblk_last[t] = (uint8_t)mon.trig[t]; }
static int sblk(void)
{
    static int16_t b[2 * M2K_SUB];
    m2k_render(b, M2K_SUB);
    int m = 0;
    for (int t = 0; t < M2K_TRACKS; t++) {
        if ((uint8_t)mon.trig[t] != sblk_last[t]) m |= 1 << t;
        sblk_last[t] = (uint8_t)mon.trig[t];
    }
    return m;
}
static m2k_pattern_t *bare(int i)
{
    pat_init(&pat[i], "BARE");
    cur_pat = i;
    sq.queued = (uint8_t)i;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[i].kit[t], P_COUNT);
    return &pat[i];
}
/* the clock in at BPM, its pulses delivered before each block that reaches them (sblk_next: the next one's sample) */
static double sblk_next, sblk_per;
static int sblk_ext(void)
{
    while (m2k_samples() >= sblk_next) { m2k_midi(0xF8, 0, 0); sblk_next += sblk_per; }
    return sblk();
}
static int sblk_by(int ext) { return ext ? sblk_ext() : sblk(); }
/* the notes out, each note's balance on its channel (on +1, off -1) */
static int mo_bal[16][128];
static void mo_drain(void)
{
    uint8_t m[3];
    int k;
    while ((k = m2k_midi_out(m, m2k_samples())) > 0) {
        if (k == 3 && (m[0] & 0xF0) == 0x90 && m[2]) mo_bal[m[0] & 15][m[1] & 127]++;
        else if (k == 3 && (m[0] & 0xF0) == 0x80) mo_bal[m[0] & 15][m[1] & 127]--;
    }
}
static int mo_hung(void)
{
    int n = 0;
    for (int c = 0; c < 16; c++) for (int i = 0; i < 128; i++) n += mo_bal[c][i] != 0;
    return n;
}

/* the panel as a hand plays it, for the controls' checks: a frame every 20 ms, a button tapped 40 ms, the sound
 * rendered between when asked */
static uint32_t ux_now;
static void ux_ms(int ms) { for (int i = 0; i < ms; i += 20) m2k_frame(ux_now += 20); }
static void ux_tap(int b) { m2k_button(b, 1); ux_ms(40); m2k_button(b, 0); ux_ms(40); }
static void ux_init(uint32_t seed) { m2k_init(seed); ux_now = 100000; m2k_frame(ux_now); ux_ms(SPLASH_MS + WIPE_MS + 100); }   /* (past the splash) */
static int ux_white(int w) { for (int k = 0; k < M2K_KEYS; k++) if (ui.kw[k] == w) return k; return -1; }
static int ux_black(int b) { for (int k = 0; k < M2K_KEYS; k++) if (ui.kb[k] == b) return k; return -1; }
static void ux_audio(int n) { static int16_t b[2 * 1024]; for (n &= ~15; n > 0; n -= 1024) m2k_render(b, n < 1024 ? n : 1024); }
static int ux_trigs(int t) { int n = 0; for (int s = 0; s < M2K_STEPS; s++) n += PAT(cur_pat)->step[t][s].flags & ST_TRIG; return n; }
static int ux_locks(int t, int s0, int s1)
{
    int n = 0;
    for (int k = 0; k < M2K_LOCKS; k++) n += PAT(cur_pat)->lock[k].trk == t && PAT(cur_pat)->lock[k].step >= s0 && PAT(cur_pat)->lock[k].step < s1;
    return n;
}
/* a pixel as px() sets it, on a copy of the screen: the byte-wise drawing held to it */
static uint8_t ux_ref[M2K_FB_BYTES];
static void ux_px(int x, int y, int c)
{
    if (x < clip_x0 || y < clip_y0 || x >= clip_x1 || y >= clip_y1 || (unsigned)x >= M2K_W || (unsigned)y >= M2K_H) return;
    uint8_t *b = &ux_ref[y * ROWB + (x >> 3)], m = (uint8_t)(0x80u >> (x & 7));
    if (c == 1) *b |= m;
    else if (c == 0) *b &= (uint8_t)~m;
    else *b ^= m;
}
static void ux_rect(int x, int y, int w, int h, int c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) ux_px(i, j, c); }

static uint32_t fuzz_r = 12345;                 /* the fuzz's numbers */
static uint32_t fuzz_rnd(void) { fuzz_r = fuzz_r * 1664525u + 1013904223u; return fuzz_r >> 8; }

/* revision 10's A03 LATTICE (LO-BIT CLUB, 3d10b96) as its stores keep it, packed: a demo of that revision untouched */
static const char LATTICE_R10[] =
    "500f4c415454494345000f09110406105a00403c001400006e60105852406452525200103c1e1a0a4c000e000624084e644000007f00"
    "0000204040404000404020007f00000000402d0040000e0a000000002000400a000000002000400a0000000020004000284000007864"
    "585a7d6e64400000140a40322800007f000000204040404000405032007f0000005a402d0040000e2000000000200040200000000020"
    "00402000000000200040002840000048503c7f640d050000005a40407f5000347f000000204040404000404020007f00000000403300"
    "40000e35000000002000403500000000200040350000000020004000284000004040244a4028144000142c104e7f4000007f00000020"
    "4040404000404020007f0000000040390040001e4a031b0c181800504a000000002000404a00000000200040002840000064417d2840"
    "506440467f7f50400c4000001e000000204040404000404020007f00000064402d0040007f6000000000200040600000000020004060"
    "000000002000400028400000303c105a5a28000600001e0c40414000007f000000204040404000404020007f0000006e402d0040000e"
    "750000000020004075000000002000407500000000200040002840000049494909494949512112211221122112dde6dde6dde6dde601"
    "09010101091101010000000100000000010081000100111a0100001a0100001a0100001a0100001a0100001a0100001a0100001a0100"
    "001a0100001a0100001a0100001a0100001a0100001a0100001a0100001a0100001a0100001a0100001a0100001a0100001a0100001a"
    "0100001a0100003e0100003e0100003f010000390100003e0100003e0100003f010000390100003e0100003e0100003f010000390100"
    "003e0100003e0100003f010000390100003c0100003c0100003c0102003c0100003c0102003c0100003c0100003c0102003c0100003c"
    "0102003c0100003c0100003c0100003c0102003c0100003c0102003c0100003c0100003c0102003c0100003c0102003c0100003c0100"
    "003c0100003c0102003c0100003c0102003c0100003c0100003c0102003c0100003c0102003c0100003c0100003c0100003c0102003c"
    "0100003c0102003c0100003c0100003c0102003c0100003c0102003c0100001a0100001a0100001b0500001a0100001a0100001a0100"
    "001a0100001b0500001a0100001a01000026010000320100003201000030030000300300003001040030030000300300003001040000"
    "00";

/* revision 11's A01 CIRCUIT (cfdd1da) as its stores keep it, packed in format 15: its amp and LFOs on the laws before
 * 16 */
static const char CIRCUIT_R11[] =
    "500f43495243554954000f1011000603600a403c4f0000003e510a58525252525252000216080c221b484c00284e08404040001673333f"
    "374a21385d2900404020007f0000000040480040000f0a000000002000400a000000002000400a000000002000406a7160000010000064"
    "5040000600001c0e4040400008341209171c4040483500404020007f00000018407f0040001720222d0c48400028200000000020004020"
    "00000000200040154129000077460a5464750c000007350d40645a00322817001514473c2d4700404031002d00000000407f0040001935"
    "0b090c3820001c350000000020004035000000002000406a207e0000120000002800324000185a1e40404000007f000000204040404000"
    "404020007f0000000040520040007f4a000000002000404a000000002000404a0000000020004000284000002d712c7f1856084000063e"
    "01455e3a000c32140c292b2e331a4000404020007f00000000407f00400018600000000020004060000000002000406012090c08040056"
    "40083a4500623f69086d1337400007414b42404000093607310a36404240400030582b005c0000001640570040001e7500000000200040"
    "75000000002000407506090c0808004e0028400000810481048104812410101010101010d0444444644444444504490449044904594000"
    "00a040000022a924a924a924a9242101000021010000210100002101000021010000210100002101000021010000210100002101000021"
    "0100002101000021010e002f0110002f0110002f0110002f0110002f0110002f0110002f0110002f0110002f010e002f01070039011000"
    "39011000390110003901100039011000390110003901100039010200390110003901100039011000390110003901100039011000390110"
    "00390102003901100039010e002101000021010000280100002b0100002101000021010000240100001f0100001d0100001d0100002401"
    "0000210100001f0100001f010000260100002b010e00230100003c0100003c0106003c010700400100003e0110003e0100003c01070047"
    "01070047010d0043010700450107003c010d003c0107004701070047010d0043010700450107003c010d00400107004801070048010d00"
    "410107004501070040010d003e0107004701070047010d0040010700430107003e010d003900013f0d24013e0d2c021d0d2202380d2202"
    "3e071e0302092803020a5a0308090503080a2c030b0905030b0a2c030e0905030e0a2c0312092803120a5a0318090503180a2c031b0905"
    "031b0a2c031e090c031e0a2c0322092803220a5a0328090503280a2c032b0905032b0a2c032e090c032e0a2c0332092803320a5a033809"
    "0503380a2c033b0905033b0a2c033e090c033e0a2c033c0905033c0a2c040600320406016404060278041d0032041d0164041d0278041f"
    "0032041f0164041f027804260017042601410426026c043900170439014504390270043d0017043d0145043d0270";

/* the sound engine's tests: pattern 40 bare (bare()), the drive off; eng_go takes its kits up and plays it, eng_run
 * renders (the last block's output in eng_out) */
static int16_t eng_out[2 * M2K_SUB];
static m2k_pattern_t *eng_pat(uint32_t seed)
{
    m2k_init(seed);
    m2k_pattern_t *q = bare(40);
    q->glob[G_DRIV] = 0;
    return q;
}
static void eng_go(m2k_pattern_t *q)
{
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, q->kit[t], P_COUNT);
    lockmap_build();
    seq_play(1);
}
static void eng_run(double sec) { for (int i = 0; i < (int)(sec * M2K_SR) / M2K_SUB; i++) m2k_render(eng_out, M2K_SUB); }
/* the amp heard: T1's clean sine (GND-SIN at C7, the filter open, alone) played with this ATK HOLD DEC REL, let go
 * after gate blocks (never: -1), and if retrig a second note on it there; each 16-sample block's level (the most of it
 * and the two before: two cycles and more) into lv, n blocks */
static void amp_probe(int atk, int hold, int dec, int rel, int gate, int retrig, int32_t *lv, int n)
{
    m2k_pattern_t *q = eng_pat(41);
    uint8_t *k = q->kit[0];
    k[P_ATK] = (uint8_t)atk; k[P_HOLD] = (uint8_t)hold; k[P_DEC] = (uint8_t)dec; k[P_REL] = (uint8_t)rel;
    k[P_VOL] = 100; k[P_LEVL] = 127;
    m_cpy(trk[0].eff, k, P_COUNT);
    voice_on(0, 96, 100, 0);
    int32_t a = 0, b = 0;
    for (int i = 0; i < n; i++) {
        if (i == gate) voice_off(0);
        if (i == retrig) voice_on(0, 96, 100, 0);
        m2k_render(eng_out, M2K_SUB);
        int32_t c = 0;
        for (int j = 0; j < M2K_SUB; j++) c = abs(eng_out[2 * j]) > c ? abs(eng_out[2 * j]) : c;
        lv[i] = c > a ? (c > b ? c : b) : a > b ? a : b;
        b = a; a = c;
    }
}
/* the first block at or past i0 where lv is at least (up 1) or under (up 0) the fraction f of ref: -1 if none */
static int amp_when(const int32_t *lv, int n, int i0, int up, double f, int32_t ref)
{
    for (int i = i0; i < n; i++) if (up ? lv[i] >= f * ref : lv[i] < f * ref) return i;
    return -1;
}
/* a held 440 Hz sine on T1 (GND-SIN, HOLD and DEC at INF, its note held from the keys: sine_on after eng_go) */
static void sine_track(m2k_pattern_t *q)
{
    kit_default(q, 0, M_GSIN);
    q->kit[0][P_HOLD] = 127; q->kit[0][P_DEC] = 127;
}
static void sine_on(void) { m2k_midi(0x90, 69, 100); }
/* the largest step from one sample to the next, either channel, in frames [a, b) */
static double step_max(const int16_t *x, int a, int b)
{
    double m = 0;
    for (int i = (a < 1 ? 1 : a); i < b; i++)
        for (int c = 0; c < 2; c++) { double d = fabs((double)x[2 * i + c] - x[2 * (i - 1) + c]); if (d > m) m = d; }
    return m;
}
/* track 0's delay has nothing left, in its quiet test's terms: every entry of its line silent, its filters at rest */
static int eng_dly_quiet(void)
{
    const trk_t *t = &trk[0];
    if (t->dquiet < (t->dmode ? DL_LEN / 2 : DL_LEN)) return 0;
    for (int c = 0; c < 2; c++) {
        int32_t v[4] = {t->dhp[c], t->dlp[c], t->dprev[c], t->dacc[c]};
        for (int k = 0; k < 4; k++) if (v[k] > 3 || v[k] < -3) return 0;
    }
    return 1;
}
/* the MOD effect's phaser (mod_tick, type 2) in 64 bits throughout, to hold it to */
static int32_t eng_phaser(uint32_t *mph, int32_t *map, int32_t *mfb, int32_t x, uint32_t inc, int32_t dep, int32_t mix)
{
    *mph += inc;
    int32_t tri = (int32_t)(*mph >> 16);
    tri = tri < 32768 ? tri : 65535 - tri;
    int64_t a = 3277 + (((int64_t)tri * dep * 206) >> 15), v = x + (*mfb >> 1);
    for (int k = 0; k < 4; k++) {
        int64_t u = v - ((a * map[k]) >> 15);
        v = ((a * u) >> 15) + map[k];
        map[k] = (int32_t)(u < -65536 ? -65536 : u > 65536 ? 65536 : u);
    }
    *mfb = (int32_t)(v < -65536 ? -65536 : v > 65536 ? 65536 : v);
    return (int32_t)(x + (((v - x) * mix) >> 8));
}

/* today's store (m2k_store_pack, n bytes) with pattern slot s of its first two images, the work and P1, the bytes b:
 * what a store saved by an older build holds, its demos packed as that build packed them. Its new length */
static int store_splice(int n, int s, const uint8_t *b, int bn)
{
    static uint8_t o[STORE_MAX];
    int np = store_buf[8], at = 10 + S_COUNT, w = at;
    memcpy(o, store_buf, (size_t)at);
    for (int im = 0; im < 2; im++) {
        const uint8_t *h = store_buf + at, *ln = h + 12, *d = ln + 2 * np;
        uint32_t used = h[8] | h[9] << 8 | h[10] << 16 | (uint32_t)h[11] << 24, off = 0, old = ln[2 * s] | ln[2 * s + 1] << 8;
        for (int i = 0; i < s; i++) off += ln[2 * i] | ln[2 * i + 1] << 8;
        uint32_t nu = used - old + (uint32_t)bn;
        memcpy(o + w, h, 8);
        for (int k = 0; k < 4; k++) o[w + 8 + k] = (uint8_t)(nu >> (8 * k));
        memcpy(o + w + 12, ln, (size_t)(2 * np));
        o[w + 12 + 2 * s] = (uint8_t)bn; o[w + 13 + 2 * s] = (uint8_t)(bn >> 8);
        w += 12 + 2 * np;
        memcpy(o + w, d, off); w += (int)off;
        memcpy(o + w, b, (size_t)bn); w += bn;
        memcpy(o + w, d + off + old, used - off - old); w += (int)(used - off - old);
        at += 12 + 2 * np + (int)used;
    }
    memcpy(o + w, store_buf + at, (size_t)(n - at));
    w += n - at;
    memcpy(store_buf, o, (size_t)w);
    return w;
}

/* the self-test's instrument starts past its splash (a press at once is a press); the splash's own case below calls
 * the real one, (m2k_init)(seed) */
#define m2k_init(s) (m2k_init(s), ui.splash_end = 0, ui.splashed = 1)
static int selftest(void)
{
    int fails = 0;
    double wow_c, flut_c;
    tape_pitch_at(127, &wow_c, &flut_c);
    printf("     tape at 127: wow +-%.1f c, flutter +-%.1f c\n", wow_c, flut_c);
    CHECK(wow_c > 25 && wow_c < 55 && flut_c < wow_c / 3,
          "tape: the wow drifts (25..55 c at TAPE 127), the flutter stays under a third of it");
    m2k_init(7);
    CHECK(!strcmp(pat[0].name, "CIRCUIT") && !strcmp(pat[2].name, "SUSPEND") && !strcmp(pat[4].name, "INIT") &&
          !strcmp(pat[16].name, "INIT"), "factory: A01 CIRCUIT, A03 SUSPEND, A05 and B01 INIT");
    {                                                  /* the bank is written on today's pages: it packs and comes back */
        static m2k_pattern_t q;
        static uint8_t qb[BLOB_MAX];
        int same = 1;
        for (int i = 0; i < 16; i++) same &= pat_unpack(&q, qb, pat_pack(&pat[i], qb)) && !memcmp(&q, &pat[i], sizeof q);
        CHECK(same, "factory: bank A on today's pages, every part packed and unpacked as it is");
    }
    CHECK(!strcmp(m2k_project_name(0), "DEMO") && !strcmp(m2k_project_name(1), "P2"), "projects: DEMO, P2");
    printf("     DEMO packed: %u bytes for 64 patterns (A01 %d bytes, B01 %d)\n", proj[0].used, proj[0].len[0], proj[0].len[16]);
    /* the pattern formats: older ones convert once, on their path; today's loads unchanged. Held on the bank before
     * (factory revision 9, host/m2k_bank_rev9.c): real parts written on the old pages, turned at build */
    factory_rev9();
    {
        /* the demos are written on today's chain: none is converted (A03 HAZE's T6 DTIM 64 would become 127) */
        CHECK(pat[2].kit[5][P_DTIM] == 64 && pat[2].kit[0][P_DSND] == 64 && pat[2].kit[0][P_DIST] == 66,
              "format: the factory is written on today's chain, not converted");
        static m2k_pattern_t a, b, c;
        static uint8_t bl[BLOB_MAX], bl2[BLOB_MAX];
        /* a version-4 pattern: the old blank kit on two tracks, a few old settings, locks, trigs on one note */
        m_set(&a, 0, sizeof a);
        pat_init(&a, "OLD");
        kit_v4(&a, 0, M_GSIN);
        kit_v4(&a, 1, M_SAW);
        a.kit[0][P_DIST] = 34; a.kit[0][P_DTIM] = 8; a.kit[0][P_DFB] = 64; a.kit[0][P_BASE] = 40; a.kit[0][P_WDTH] = 30;
        a.kit[0][P_SYN + 2] = 77; a.kit[0][P_LFO + L_SPD] = 21; a.kit[0][P_BITS] = 9;
        a.kit[0][P_SYN] = 63; a.kit[1][P_SYN] = 63;                  /* GND-SIN's TUNE a semitone down; SWAV-SAW's A */
        for (int st = 0; st < 16; st += 4) { a.step[0][st].flags = ST_TRIG; a.step[0][st].note = 36; }
        lk(&a, 0, 4, P_DTIM, 16);
        lk(&a, 0, 8, P_SYN + 1, 99);
        lk(&a, 0, 12, P_SYN, 65);
        int n = pat_pack(&a, bl);
        CHECK(bl[1] == 17, "format: patterns are written as version 17");
        n = pat_pack_v(&a, bl, 4);
        CHECK(pat_unpack(&b, bl, n), "format: a version-4 pattern loads");
        CHECK(b.kit[0][P_DIST] == T_V5_DIST[34] && b.kit[0][P_DTIM] == 32 && b.kit[0][P_DSND] == 64 &&
              b.kit[0][P_DFB] == (64 * 270 * 64 + 16384) >> 15 && b.kit[0][P_VOL] == T_V5_VOL[100],
              "format: version 4 -> 5 converts the chain (DIST, DTIM, DSND, DFB, VOL)");
        CHECK(b.kit[0][P_BASE] == v5_base(T_V5_FPOS[40], 36), "format: the filter is placed against the track's note");
        /* (GND-SIN's page since version 12: TUNE on H, its other knobs one earlier, values as they were) */
        CHECK(b.kit[0][P_SYN + 1] == 77 && b.kit[0][P_LFO + L_SPD] == T_SPD15[21] && b.kit[0][P_BITS] == 9 &&   /* (SPD's rate: 16) */
              !memcmp(b.kit[1], a.kit[1], 8) && b.step[0][4].note == 36 && b.step[0][4].flags == ST_TRIG,
              "format: machine, LFO and MONO2K's own settings, trigs untouched");
        int lock_ok = 0;
        for (int i = 0; i < M2K_LOCKS; i++) {
            if (b.lock[i].trk == 0 && b.lock[i].param == P_DTIM && b.lock[i].val == 64) lock_ok |= 1;
            if (b.lock[i].trk == 0 && b.lock[i].param == P_SYN + 0 && b.lock[i].val == 99) lock_ok |= 2;
        }
        CHECK(lock_ok == 3, "format: a chain lock converts, a machine lock does not");
        int tl = -1;
        for (int i = 0; i < M2K_LOCKS; i++) if (b.lock[i].trk == 0 && b.lock[i].param == P_SYN + 7) tl = b.lock[i].val;
        CHECK(b.kit[0][P_SYN + 7] == 32 && tl == 96 && b.kit[1][P_SYN] == 63,
              "format: version 5 -> 6 keeps GND-SIN's TUNE in semitones (-1: 32, a lock +1: 96), SWAV-SAW's A as it was");
        CHECK(pat_unpack(&c, bl, n) && !memcmp(&b, &c, sizeof b), "format: the conversion is deterministic");
        int n2 = pat_pack(&b, bl2);
        CHECK(pat_unpack(&c, bl2, n2) && !memcmp(b.kit, c.kit, sizeof b.kit) && !memcmp(b.lock, c.lock, sizeof b.lock),
              "format: version 6 loads unchanged (no second conversion)");
        n = pat_pack_v(&a, bl, 5);
        CHECK(pat_unpack(&c, bl, n) && c.kit[0][P_SYN + 7] == 32 && c.kit[0][P_DTIM] == 8,
              "format: version 5 takes only the TUNE");
        n = pat_pack_v(&a, bl, 3);
        CHECK(pat_unpack(&c, bl, n) && c.kit[1][0] == 0 && c.kit[1][7] == 64 && c.kit[0][P_DTIM] == 32,
              "format: version 3 takes SWAV-SAW's page and the chain");
        n = pat_pack(&pat[0], bl);
        CHECK(pat_unpack(&c, bl, n) && !memcmp(pat[0].kit, c.kit, sizeof c.kit), "format: a saved demo reloads as it was");
        /* no conversion moves a pitch: every demo saved and reloaded as version 4 and 5 keeps its notes, its PTCH,
         * its machines' pages and the locks on them (GND-SIN's page converts before 12, its locks with it, and no demo
         * moves its TUNE) */
        int moved = 0;
        for (int d = 0; d < 16; d++)
            for (int ver = 4; ver <= 5; ver++) {
                n = pat_pack_v(&pat[d], bl, ver);
                if (!pat_unpack(&c, bl, n)) { moved++; continue; }
                for (int t = 0; t < M2K_TRACKS; t++) {               /* SWAV-ENS's page converts at 7, FM+'s at 10 */
                    int conv = pat[d].machine[t] == M_ENS || pat[d].machine[t] == M_FMS || pat[d].machine[t] == M_FMD ||
                               pat[d].machine[t] == M_SID || pat[d].machine[t] == M_GSIN || pat[d].machine[t] == M_VOX;
                    moved += c.kit[t][P_PTCH] != pat[d].kit[t][P_PTCH] || (!conv && memcmp(c.kit[t], pat[d].kit[t], 8) != 0);
                    for (int s = 0; s < M2K_STEPS; s++)
                        moved += c.step[t][s].note != pat[d].step[t][s].note || c.step[t][s].flags != pat[d].step[t][s].flags;
                }
                for (int i = 0; i < M2K_LOCKS; i++) {
                    int m = pat[d].lock[i].trk != 0xFF ? pat[d].machine[pat[d].lock[i].trk] : M_ENS;
                    if (m != M_ENS && m != M_FMS && m != M_FMD && m != M_SID && m != M_GSIN && m != M_VOX && (pat[d].lock[i].param < 8 || pat[d].lock[i].param == P_PTCH))
                        moved += c.lock[i].val != pat[d].lock[i].val;
                }
            }
        CHECK(moved == 0, "format: versions 4 and 5 move no demo's notes, PTCH, machine page or their locks");
        /* SWAV-ENS onto the Monomachine's page (7): every old interval folds by octaves into -11..+12, keeping its
         * pitch class, exact from 1 to 12, 0 and 24 to +12; never OFF (61-63) nor the octave down (0-1) */
        int fold_ok = 1;
        for (int v = 0; v < 128; v++) {
            int old_i = (v * 25) >> 7, nv = ens_fold(v), c64 = T_ENS_PCH[nv], semi = c64 / 64;
            if (nv <= 1 || (nv >= 61 && nv <= 63) || c64 == ENS_OFF || c64 % 64 || ((semi - old_i) % 12 + 12) % 12 ||
                (old_i >= 1 && old_i <= 12 && semi != old_i) || ((old_i == 0 || old_i == 24) && semi != 12)) fold_ok = 0;
        }
        CHECK(fold_ok, "format: an old SWAV-ENS interval keeps its pitch class (exact 1..12, 0 and 24 to +12), never OFF");
        /* the bank (8): a version-7 kit (70 wide) loads with MONO2K's 32 and every value where it was; today's keeps it */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[9], sizeof w);                             /* A10 SMOKE (no FM, no SID) */
            for (int t = 0; t < M2K_TRACKS; t++) if (w.machine[t] == M_GSIN) w.machine[t] = M_GPRC;   /* (no page that converts) */
            for (int t = 0; t < M2K_TRACKS; t++) for (int k = 0; k < P_WBNK; k++) w.kit[t][k] = (uint8_t)((t * 31 + k * 7) & 127);
            for (int t = 0; t < M2K_TRACKS; t++) w.kit[t][P_HOLD] = 127;  /* INF: HOLD's conversion (13) leaves it */
            for (int t = 0; t < M2K_TRACKS; t++) { w.kit[t][P_SPU] = 0; w.kit[t][P_DSND] = 64; }   /* (17 turns sends) */
            int wn = pat_pack_v(&w, wb, 7);
            int same = pat_unpack(&w2, wb, wn);
            static m2k_pattern_t we;                                     /* (16 turns the amp's and the LFOs' alone) */
            m_cpy(&we, &w, sizeof we);
            feel_v15(&we);
            for (int t = 0; t < M2K_TRACKS; t++) same = same && !memcmp(w2.kit[t], we.kit[t], P_WBNK) && w2.kit[t][P_WBNK] == 0;
            CHECK(same, "format: version 7 -> 8 keeps every kit value, the bank MONO2K's 32");
            w.kit[3][P_WBNK] = 127;
            wn = pat_pack(&w, wb);
            CHECK(pat_unpack(&w2, wb, wn) && w2.kit[3][P_WBNK] == 127 && !memcmp(w2.kit, w.kit, sizeof w.kit),
                  "format: version 8 keeps the bank");
        }
        /* the bus's laws (9): every old value of each control goes to a new one that sounds as it (the old laws as the
         * code had them: GLCH o/256 a sixteenth, DRIV 256 + 10 o, TAPE o, ADPC its mix and its whole-sample rate,
         * HISS's crackle o / 2^18 a sample at (16 + o) / 64, its hiss o^2) */
        {
            int bad = 0;
            for (int o = 1; o < 128; o++) {
                int vg = T_V9_GLCH[o], vd = T_V9_DRIV[o], vt = T_V9_TAPE[o], va = T_V9_ADPC[o], vh = T_V9_HISS[o];
                if (fabs(log(2560.0 * pow(12.7, (vg - 64) / 63.0) / (256.0 * o))) > 0.025) bad++;   /* (9's law) */
                if (fabs(20 * log10(T_DRIV[vd] / (256.0 + 10 * o))) > 0.15) bad++;
                if (o > 3 ? fabs(log(T_TAPE_Q8[vt] / (256.0 * o))) > 0.02 : vt != 1) bad++;
                if ((va * 2 > 127 ? 127 : va * 2) != (o * 2 > 127 ? 127 : o * 2) || T_ADPC_DIV[va] != 256 * (1 + (o > 64 ? (o - 64) / 22 : 0))) bad++;
                if (fabs(log(T_CRK_P[vh] / (8.0 * o))) > 0.025 || fabs(log(T_CRK_A[vh] / (16.0 * (16 + o)))) > 0.02 ||
                    20 * log10(T_HISS_G[vh] / (double)(o * o)) < -0.5) bad++;
            }
            CHECK(!bad, "format: version 8 -> 9, every old bus value to one that sounds as it (GLCH DRIV TAPE ADPC HISS)");
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[16], sizeof w);
            w.glob[G_GLCH] = 30; w.glob[G_HISS] = 10; w.glob[G_ADPC] = 90; w.glob[G_DRIV] = 20; w.glob[G_TAPE] = 30;
            int wn = pat_pack_v(&w, wb, 8);
            int conv = pat_unpack(&w2, wb, wn) && w2.glob[G_GLCH] == T_V17_GLCH[91] && w2.glob[G_HISS] == 64 && w2.glob[G_ADPC] == 96 &&
                       w2.glob[G_DRIV] == 81 && w2.glob[G_TAPE] == 78;
            wn = pat_pack(&w, wb);
            CHECK(conv && pat_unpack(&w2, wb, wn) && !memcmp(w2.glob, w.glob, G_COUNT),
                  "format: a version-8 pattern's bus converts on load (GLCH 30 -> 91 at 9, then 17's ...), today's loads as it is");
        }
        /* FM+'s pages (10): an old FM+STAT and FM+DYN (RAT1 DEP1 DEC1 RAT2 DEP2 DEC2 FDBK ALGO / SWEP) carried over */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_set(&w, 0, sizeof w);
            pat_init(&w, "OLDFM");
            w.machine[1] = M_FMS; w.machine[2] = M_FMD;
            /* STAT: ratio 1.41 (T_RATIO 4: a step and 1FIN), 2 (6); DEP1 static, DEP2, feedback */
            const uint8_t os[8] = {16, 30, 127, 24, 90, 40, 50, 0}, od[8] = {12, 80, 60, 40, 30, 127, 20, 96};
            m_cpy(&w.kit[1][P_SYN], os, 8);
            m_cpy(&w.kit[2][P_SYN], od, 8);                  /* DYN: ratio 1, 3.5; DEP1 decaying, SWEP up */
            lk(&w, 1, 3, P_SYN + 0, 37);                     /* RAT1 to 3.0 (T_RATIO 9) */
            lk(&w, 1, 5, P_SYN + 2, 40);                     /* DEC1: goes */
            lk(&w, 2, 7, P_SYN + 4, 70);                     /* DEP2, held: to 2ENV */
            lk(&w, 1, 9, P_SYN + 1, 20);                     /* STAT's DEP1: to 1ENV */
            lk(&w, 1, 11, P_SYN + 6, 40);                    /* its FDBK: to 1FB */
            w.kit[0][P_LFO + L_TRCK] = 1 * 128 / M2K_TRACKS + 1; w.kit[0][P_LFO + L_DEST] = P_SYN + 4 + 1;   /* T2's DEP2 */
            int wn = pat_pack_v(&w, wb, 9);
            int ok = pat_unpack(&w2, wb, wn);
            const uint8_t *s2 = &w2.kit[1][P_SYN], *d2 = &w2.kit[2][P_SYN];
            double r1 = T_FMR24[fr24(s2[0])] / 65536.0 * (192 + s2[1]) / 256;
            ok = ok && fabs(r1 / 1.41 - 1) < 0.01 && s2[1] != 64 && fr24(s2[4]) == 19 &&
                 s2[2] == near_u32(T_FMB, 0, 64, T_FMK[30]) && s2[5] == near_u32(T_FMB, 0, 127, T_FMK[90]) &&
                 s2[3] == near_u32(T_FMFB, 0, 127, T_FMK[50] >> 1) && s2[6] == 64 && s2[7] == 64;
            CHECK(ok, "format: version 9 -> 10, an old FM+STAT's ratios to the 24 steps (1FIN the rest), its depths and feedback");
            int vl = near_u32(T_FMDEC, 1, 63, T_ENV_COEF[60]);
            ok = d2[0] == 64 && d2[4] == 119 && d2[3] == 64 + vl && d2[1] > 64 && d2[5] == near_u32(T_FMDV, 0, 64, T_FMK[30]) &&
                 d2[2] == CLAMP(near_u32(T_FMDV, 0, 256, T_FMK[80]) - (vl < 32 ? 4 * vl : 128), 0, 127) && d2[7] == 64;
            CHECK(ok, "format: an old FM+DYN's ratios to /64 and squared /64, its decay to 1VEN, SWEP to 1FEN up");
            int lr = 0, ld = 0, l2 = 0, l1 = 0, lf = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &w2.lock[i];
                if (l->trk == 1 && l->step == 3) lr = l->param == P_SYN + 0 && fr24(l->val) == 21;
                if (l->trk == 1 && l->step == 5) ld = 1;
                if (l->trk == 2 && l->step == 7) l2 = l->param == P_SYN + 5 && l->val == near_u32(T_FMDV, 0, 64, T_FMK[70]);
                if (l->trk == 1 && l->step == 9) l1 = l->param == P_SYN + 2 && l->val == near_u32(T_FMB, 0, 64, T_FMK[20]);
                if (l->trk == 1 && l->step == 11) lf = l->param == P_SYN + 3 && l->val == near_u32(T_FMFB, 0, 127, T_FMK[40] >> 1);
            }
            CHECK(lr && !ld && l2 && l1 && lf && w2.kit[0][P_LFO + L_DEST] == P_SYN + 5 + 1,
                  "format: an FM lock goes with its parameter (RAT1 to 1FRQ, DEP2 to 2ENV), DEC1's goes; an LFO follows");
            static uint8_t wb2[BLOB_MAX];
            wn = pat_pack(&w2, wb);
            int wn2 = pat_unpack(&w, wb, wn) ? pat_pack(&w, wb2) : 0;
            CHECK(wn2 == wn && !memcmp(wb, wb2, (size_t)wn), "format: today's version loads unchanged");
            for (int t = 0; t < M2K_TRACKS; t++)             /* (a version-10 SID, GND-SIN or VOX-6 page is the old one: none) */
                if (w2.machine[t] == M_GSIN || w2.machine[t] == M_SID || w2.machine[t] == M_VOX) w2.machine[t] = M_GPRC;
            wn = pat_pack_v(&w2, wb, 10);                   /* and a version-10 pattern: its FM pages already the new ones */
            wn2 = pat_unpack(&w, wb, wn) ? pat_pack(&w, wb2) : 0;
            feel_v15(&w2);                                  /* (its amp and LFOs: 16's; its bus: 17's) */
            fx_v16(&w2);
            int wn3 = pat_pack(&w2, wb);
            CHECK(wn2 == wn3 && !memcmp(wb, wb2, (size_t)wn3), "format: version 10's FM pages load unconverted");
        }
        /* SID's page (11): an old SID (WAV1 PW PWM WAV2 DTN2 FINE MODE MIX) carried over */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_set(&w, 0, sizeof w);
            pat_init(&w, "OLDSID");
            w.machine[2] = M_SID;
            const uint8_t os[8] = {32, 16, 40, 0, 88, 64, 70, 100};  /* PUL, duty 1/8, PWM, -, +9 semi, -, RING, a MIX */
            m_cpy(&w.kit[2][P_SYN], os, 8);
            lk(&w, 2, 4, P_SYN + 0, 70);                             /* WAV1 to T+S: MIX */
            lk(&w, 2, 6, P_SYN + 5, 70);                             /* FINE: goes */
            w.kit[0][P_LFO + 8 + L_TRCK] = 2 * 128 / M2K_TRACKS + 1; w.kit[0][P_LFO + 8 + L_DEST] = P_SYN + 1 + 1;   /* T3's PW */
            int wn = pat_pack_v(&w, wb, 10);
            const uint8_t *k = &w2.kit[2][P_SYN];
            int ok = pat_unpack(&w2, wb, wn) && (k[3] * 5) >> 7 == 2 && k[0] == 48 && k[1] == 20 && k[2] >= 64 &&
                     k[4] >> 5 == 1 && k[5] < 64 && k[6] == 32 + 9 * 32 / 12 && k[7] == 64;
            int lw = 0, lf = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                if (w2.lock[i].trk == 2 && w2.lock[i].step == 4) lw = w2.lock[i].param == P_SYN + 3 && (w2.lock[i].val * 5) >> 7 == 3;
                if (w2.lock[i].trk == 2 && w2.lock[i].step == 6) lf = 1;
            }
            CHECK(ok && lw && !lf && w2.kit[0][P_LFO + 8 + L_DEST] == P_SYN + 0 + 1,
                  "format: version 10 -> 11, an old SID's wave, duty, sweep, ring, interval to SID-6581's page; an LFO follows");
        }
        /* bank A is written on GND-SIN's old page and turned at build: every GND-SIN track's TUNE where it went, H, at
         * its centre (as every part has it) */
        {
            int gs = 0, gok = 0;
            m2k_init(2000);
            factory_rev9();
            for (int d = 0; d < 16; d++)
                for (int t = 0; t < M2K_TRACKS; t++)
                    if (pat[d].machine[t] == M_GSIN) { gs++; gok += pat[d].kit[t][P_SYN + 7] == 64; }
            CHECK(gs > 0 && gok == gs, "factory, the bank before: its GND-SIN tracks turned onto today's page at build (TUNE on H)");
        }
        /* and its HOLDs, written in 14.6 ms steps, in beats at each part's tempo: A01 MAMMOTH (164 BPM) T4's 263 ms
         * (18) is 23 steps of 11.4 ms, its locks 24 14 30 20 26 (350 .. 438 ms) 31 18 38 26 33 */
        {
            m2k_init(2000);
            factory_rev9();
            int ok = pat[0].glob[G_TMPO] == 164 - 30 && pat[0].kit[3][P_HOLD] == 23, nl = 0;
            for (int i = 0; i < M2K_LOCKS; i++)
                if (pat[0].lock[i].trk == 3 && pat[0].lock[i].param == P_HOLD) {
                    int v = pat[0].lock[i].val;
                    nl++;
                    ok = ok && (v == 23 || v == 31 || v == 18 || v == 38 || v == 26 || v == 33);
                }
            CHECK(ok && nl > 0, "factory, the bank before: its HOLDs in beats at each part's tempo (A01's T4: 263 ms, 23 steps at 164)");
            CHECK(pat[0].kit[3][P_DEC] == 61, "factory, the bank before: its amp on format 16's laws (A01's T4 DEC 50, 0.69 s"
                                              " to -60 dB, 61)");
        }
        /* VOX-6 onto VO-6's page (14): the old vowel to the nearest VOC1 VOC2 (an E, 400 and 2100 Hz, lands within 8 %
         * of both), BRTH to VOIC; a VOWL lock to VOC1's, a BRTH lock to VOIC's, the rest gone; an LFO on BRTH moves to
         * VOIC, one on VIBR stops; a version-14 pattern as it is; A02 CUTUP's voice turned at build */
        {
            static m2k_pattern_t w, w2, w3;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[2], sizeof w);                                         /* A03 HAZE */
            for (int i = 0; i < M2K_LOCKS; i++) w.lock[i].trk = 0xFF;
            w.machine[0] = M_VOX;
            const uint8_t ov[8] = {36, 70, 30, 64, 80, 70, 40, 20};              /* VOWL (an E) VSWP VTIM SHFT BRTH .. */
            m_cpy(&w.kit[0][P_SYN], ov, 8);
            lk(&w, 0, 2, P_SYN + 0, 0);                                           /* a VOWL lock: an A */
            lk(&w, 0, 3, P_SYN + 4, 127);                                         /* a BRTH lock */
            lk(&w, 0, 4, P_SYN + 5, 50);                                          /* a RESO lock: gone */
            w.kit[1][P_LFO + L_TRCK] = enc(0, M2K_TRACKS); w.kit[1][P_LFO + L_DEST] = P_SYN + 4 + 1;          /* on BRTH */
            w.kit[1][P_LFO + 8 + L_TRCK] = enc(0, M2K_TRACKS); w.kit[1][P_LFO + 8 + L_DEST] = P_SYN + 7 + 1;  /* on VIBR */
            w.kit[2][P_HOLD] = 40;                                                /* a version-13 HOLD: in beats already */
            int wn = pat_pack_v(&w, wb, 13);
            int ok = pat_unpack(&w2, wb, wn) && w2.kit[2][P_HOLD] == 40;
            const uint8_t *k = &w2.kit[0][P_SYN];
            double e1 = T_VO_F1HZ[k[0]] / 400.0, e2 = T_VO_F2HZ[k[1]] / 2100.0;
            ok = ok && e1 > 0.92 && e1 < 1.08 && e2 > 0.92 && e2 < 1.08 && k[0] == T_VOX14[36][0] && k[1] == T_VOX14[36][1];
            ok = ok && k[2] == 0 && k[3] == T_VOX14_BR[80] && k[3] > 0 && k[4] == 0 && k[5] == 64 && k[6] == 64 && k[7] == 64;
            int la = 0, lb = 0, lr = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &w2.lock[i];
                if (l->trk != 0) continue;
                if (l->step == 2) la = l->param == P_SYN + 0 && l->val == T_VOX14[0][0];
                if (l->step == 3) lb = l->param == P_SYN + 3 && l->val == T_VOX14_BR[127];
                if (l->step == 4) lr = 1;
            }
            ok = ok && la && lb && !lr && w2.kit[1][P_LFO + L_DEST] == P_SYN + 3 + 1 && w2.kit[1][P_LFO + 8 + L_DEST] == 0;
            int wn2 = pat_pack(&w2, wb);
            ok = ok && pat_unpack(&w3, wb, wn2) && !memcmp(&w3.kit[0][P_SYN], k, 8);
            wn2 = pat_pack_v(&w2, wb, 14);                                        /* a version-14 pattern: not again */
            ok = ok && pat_unpack(&w3, wb, wn2) && !memcmp(&w3.kit[0][P_SYN], k, 8);
            m2k_init(2000);
            factory_rev9();
            ok = ok && pat[1].machine[3] == M_VOX && pat[1].kit[3][P_SYN + 0] == T_VOX14[0][0] && pat[1].kit[3][P_SYN + 5] == 64;
            CHECK(ok, "format: version 13 -> 14, VOX-6 onto VO-6's page (the vowel to its F1 F2, BRTH to VOIC, their locks and"
                      " LFOs with them, the rest gone; its HOLD not turned again); version 14 as it is; CUTUP's voice turned at build");
        }
        /* the amp's and the LFOs' laws (16), each old value to the new one that sounds nearest: ATK 80 (0.61 s, straight)
         * to 66 (0.44 s, squared: the same loudness against time), DEC 64 (1.75 s to -60 dB) to 79 (1.74 s), REL 100
         * (19.1 s) to 126 (19.0 s), DEC's INF and ATK 0 as they were, a DEC lock 30 (0.18 s) to 37; an LFO's SPD to its
         * rate (64 at x16, 4.06 / 16 beats, to 60, a sixteenth; a lock 16 to 36), its DPTH 72 (+-16 steps) to 86 (the same
         * swing); an LFO on a DEC (64 -36: +-72 old steps, 25 ms to INF) to the swing that lands there on the new law's
         * steps (11: +-88), not the plain 16; version 16 as it is */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX], wb2[BLOB_MAX];
            pat_init(&w, "FEEL");
            w.kit[0][P_ATK] = 80; w.kit[0][P_DEC] = 64; w.kit[0][P_REL] = 100; w.kit[2][P_DEC] = 127; w.kit[2][P_ATK] = 0;
            w.kit[1][P_DEC] = 100;                                       /* (T2's own: its LFO reads T1's) */
            uint8_t *b1 = &w.kit[0][P_LFO], *b2 = &w.kit[1][P_LFO + 8];
            b1[L_DEST] = P_PAN + 1; b1[L_MULT] = enc(4, 8); b1[L_SPD] = 64; b1[L_DPTH] = 72;
            b2[L_TRCK] = enc(0, M2K_TRACKS); b2[L_DEST] = P_DEC + 1; b2[L_DPTH] = 28;
            lk(&w, 0, 4, P_DEC, 30);
            lk(&w, 0, 8, P_LFO + L_SPD, 16);
            int wn = pat_pack_v(&w, wb, 15);
            int ok = pat_unpack(&w2, wb, wn), ld = 0, ls = 0;
            const uint8_t *k = w2.kit[0];
            for (int i = 0; i < M2K_LOCKS; i++) {
                if (w2.lock[i].trk == 0 && w2.lock[i].step == 4) ld = w2.lock[i].param == P_DEC && w2.lock[i].val == 37;
                if (w2.lock[i].trk == 0 && w2.lock[i].step == 8) ls = w2.lock[i].param == P_LFO + L_SPD && w2.lock[i].val == 36;
            }
            ok = ok && k[P_ATK] == 66 && k[P_DEC] == 79 && k[P_REL] == 126 && w2.kit[2][P_DEC] == 127 && w2.kit[2][P_ATK] == 0 &&
                 ld && ls && k[P_LFO + L_SPD] == 60 && k[P_LFO + L_DPTH] == 86 && w2.kit[1][P_LFO + 8 + L_DPTH] == 11 &&
                 fabs(T_ADEC_MS10[79] / 17495.0 - 1) < 0.03 && fabs(T_ADEC_MS10[126] / 191424.0 - 1) < 0.03 &&
                 abs(T_LDPTH[86] - 8 * 256) < 128;
            int wn2 = pat_unpack(&w, wb, pat_pack(&w2, wb)) ? pat_pack(&w, wb2) : 0;
            ok = ok && wn2 > 0 && !memcmp(w.kit, w2.kit, sizeof w.kit);
            CHECK(ok, "format: version 15 -> 16, ATK DEC REL (their locks too) to the value that sounds nearest, an LFO's SPD"
                      " to its rate and DPTH to its swing (one on a DEC to the new law's steps); version 16 as it is");
        }
        /* HOLD in beats (13): a version-12 HOLD (14.6 ms a step) to the step nearest the time it held at the pattern's
         * tempo, its locks with it, INF kept; a version-13 pattern as it is */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[2], sizeof w);                                         /* A03 HAZE */
            for (int i = 0; i < M2K_LOCKS; i++) w.lock[i].trk = 0xFF;
            w.glob[G_TMPO] = 164 - 30;                                            /* 164 BPM: a step 11.4 ms */
            w.kit[0][P_HOLD] = 18; w.kit[1][P_HOLD] = 0; w.kit[2][P_HOLD] = 127; w.kit[3][P_HOLD] = 120;
            lk(&w, 0, 4, P_HOLD, 40);
            w.machine[5] = M_GSIN;                                                /* a version-12 GND-SIN page: as it is */
            const uint8_t gs[8] = {11, 22, 33, 44, 55, 66, 77, 60};
            m_cpy(&w.kit[5][P_SYN], gs, 8);
            int wn = pat_pack_v(&w, wb, 12);
            int ok = pat_unpack(&w2, wb, wn) && !memcmp(&w2.kit[5][P_SYN], gs, 8);
            int lh = 0;
            for (int i = 0; i < M2K_LOCKS; i++) if (w2.lock[i].trk == 0 && w2.lock[i].step == 4) lh = w2.lock[i].param == P_HOLD && w2.lock[i].val == 51;
            /* 18 x 14.6 = 263 ms: 23 x 11.43 = 263; 40 x 14.6 = 584 ms: 51; 120 x 14.6 = 1752 ms: 153 steps, held at 126 */
            ok = ok && w2.kit[0][P_HOLD] == 23 && w2.kit[1][P_HOLD] == 0 && w2.kit[2][P_HOLD] == 127 && w2.kit[3][P_HOLD] == 126 && lh;
            int wn2 = pat_pack(&w2, wb);
            static m2k_pattern_t w3;
            ok = ok && pat_unpack(&w3, wb, wn2) && w3.kit[0][P_HOLD] == 23;
            w.glob[G_TMPO] = 60 - 30;                                             /* 60 BPM: a step 31.25 ms */
            wn = pat_pack_v(&w, wb, 12);
            ok = ok && pat_unpack(&w2, wb, wn) && w2.kit[0][P_HOLD] == 8;          /* 263 ms: 8 x 31.25 = 250 */
            CHECK(ok, "format: version 12 -> 13, HOLD from 14.6 ms a step to 1 / 32 beat at the pattern's tempo, its locks too,"
                      " INF kept, 126 at most; GND-SIN's page as it was; version 13 as it is");
        }
        /* GND-SIN onto the Monomachine's page (12): TUNE to H, the other knobs one earlier, values as they were; its
         * locks and an LFO aimed at it from another track go with them; another machine's page stays */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[2], sizeof w);                                         /* A03 HAZE */
            w.machine[0] = M_GSIN; w.machine[1] = M_SAW; w.machine[3] = M_SID;
            const uint8_t og[8] = {60, 11, 22, 33, 44, 55, 66, 77};               /* TUNE FOLD SUB NOIS PDEP PDEC CLIK HARM */
            m_cpy(&w.kit[0][P_SYN], og, 8);
            m_cpy(&w.kit[1][P_SYN], og, 8);
            m_cpy(&w.kit[3][P_SYN], og, 8);                                       /* a version-11 SID page: today's */
            for (int i = 0; i < M2K_LOCKS; i++) w.lock[i].trk = 0xFF;
            lk(&w, 0, 2, P_SYN + 0, 70);                                          /* a TUNE lock */
            lk(&w, 0, 3, P_SYN + 6, 99);                                          /* a CLIK lock */
            lk(&w, 1, 3, P_SYN + 0, 50);                                          /* SWAV-SAW's A: stays */
            w.kit[2][P_LFO + L_TRCK] = enc(0, M2K_TRACKS); w.kit[2][P_LFO + L_DEST] = P_SYN + 4 + 1;   /* T3's LFO on T1's PDEP */
            w.kit[2][P_LFO + 8 + L_TRCK] = enc(1, M2K_TRACKS); w.kit[2][P_LFO + 8 + L_DEST] = P_SYN + 4 + 1;   /* and T2's E */
            int wn = pat_pack_v(&w, wb, 11);
            const uint8_t ng[8] = {11, 22, 33, 44, 55, 66, 77, 60};               /* FOLD SUB NOIS PDEP PDEC CLIK HARM TUNE */
            int ok = pat_unpack(&w2, wb, wn) && !memcmp(&w2.kit[0][P_SYN], ng, 8) && !memcmp(&w2.kit[1][P_SYN], og, 8) &&
                     !memcmp(&w2.kit[3][P_SYN], og, 8);
            int lt = 0, lc = 0, ls = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &w2.lock[i];
                if (l->trk == 0 && l->step == 2) lt = l->param == P_SYN + 7 && l->val == 70;
                if (l->trk == 0 && l->step == 3) lc = l->param == P_SYN + 5 && l->val == 99;
                if (l->trk == 1 && l->step == 3) ls = l->param == P_SYN + 0 && l->val == 50;
            }
            CHECK(ok && lt && lc && ls && w2.kit[2][P_LFO + L_DEST] == P_SYN + 3 + 1 && w2.kit[2][P_LFO + 8 + L_DEST] == P_SYN + 4 + 1,
                  "format: version 11 -> 12, GND-SIN's TUNE to H and the rest a knob earlier, its locks and an LFO with them;"
                  " SWAV-SAW's and SID-6581's pages as they were");
            static uint8_t wb2[BLOB_MAX];
            wn = pat_pack(&w2, wb);
            int wn2 = pat_unpack(&w, wb, wn) ? pat_pack(&w, wb2) : 0;
            CHECK(wn2 == wn && !memcmp(wb, wb2, (size_t)wn), "format: version 12 loads unchanged (no second conversion)");
        }
        /* the FX machines (MONO2K's own designs, no reference to hold them to): a SWAV-SAW on T1, out of the mix by its
         * VOL, into an FX on T2 opened with each of its trigs; CHORUS widens by WID, RING changes the sound by MIX, DYNAMIX
         * brings a soft hit up to a loud one */
        {
            static int16_t kb[4 * M2K_SR + 512];
            double pk, rm;
            const int fp = 47;                                         /* C16, a scratch slot */
            static const uint8_t GSRC[8] = {0, 0, 0, 40, 40, 30, 0, 64};    /* a GND-SIN source as these were written on */
            #define FXPAT(fxm, src, vl) do { m2k_init(2000); pat_init(&pat[fp], "FXT"); kit_default(&pat[fp], 0, src); kit_default(&pat[fp], 1, fxm); \
                if ((src) == M_GSIN) m_cpy(&pat[fp].kit[0][P_SYN], GSRC, 8); \
                pat[fp].kit[0][P_VOL] = 0; pat[fp].kit[1][P_HOLD] = 127; pat[fp].glob[G_DRIV] = 0; \
                for (int s_ = 0; s_ < 16; s_ += 4) { pat[fp].step[0][s_].flags = ST_TRIG; pat[fp].step[0][s_].note = 48; \
                    pat[fp].step[1][s_].flags = ST_TRIG; pat[fp].step[1][s_].note = 48; } (void)(vl); } while (0)
            double corr[2];
            for (int w = 0; w < 2; w++) {
                FXPAT(M_FXC, M_SAW, 0);
                pat[fp].kit[1][P_SYN + 5] = (uint8_t)(w ? 127 : 0);
                pat[fp].kit[1][P_SYN + 3] = 127;
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0, slr = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) { sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; slr += (double)kb[2 * i] * kb[2 * i + 1]; }
                corr[w] = slr / sqrt(sl * sr + 1);
            }
            CHECK(corr[0] > 0.999 && corr[1] < 0.9, "FX-CHORUS: mono at WID 0, wide at 127");
            /* its feedback rings: a short pluck through it at MIX 127, the energy 80-250 ms after the hit against the
             * hit's first 25 ms, FB 127 (+0.9) against FB 64 (none); the Monomachine's metal clangs are made so */
            double fbring[2];
            for (int f = 0; f < 2; f++) {
                FXPAT(M_FXC, M_GSIN, 0);
                pat[fp].kit[0][P_HOLD] = 0; pat[fp].kit[0][P_DEC] = 10;
                pat[fp].kit[1][P_SYN + 3] = 127; pat[fp].kit[1][P_SYN + 4] = (uint8_t)(f ? 127 : 64);
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double e0 = 0, e1 = 0;
                for (int i = 0; i < M2K_SR / 40; i++) e0 += (double)kb[2 * i] * kb[2 * i];
                for (int i = M2K_SR * 8 / 100; i < M2K_SR / 4; i++) e1 += (double)kb[2 * i] * kb[2 * i];
                fbring[f] = 10 * log10((e1 + 1) / (e0 + 1));
            }
            printf("     FX-CHORUS ring, 80-250 ms against the hit: FB 64 %.1f dB, FB 127 %.1f dB\n", fbring[0], fbring[1]);
            CHECK(fbring[1] > fbring[0] + 10, "FX-CHORUS: its feedback rings (+0.9 at FB 127)");
            /* FX-RING hears the track before after its VOL and its LEV, as the Monomachine's RINGMOD: the source
             * panned left, the FX right (its VOL 96 and LEV 127 unity); at MIX 0 and INP 45 (unity) the FX's side is the
             * source's, at MIX 127 it is
             * rung, and at the source's LEV 0 silent */
            double rl[3], rr[3];
            for (int c = 0; c < 3; c++) {
                FXPAT(M_FXR, M_GSIN, 0);
                pat[fp].kit[0][P_VOL] = 64; pat[fp].kit[0][P_PAN] = 0; pat[fp].kit[1][P_PAN] = 127;
                pat[fp].kit[0][P_LEVL] = pat[fp].kit[1][P_LEVL] = 127; pat[fp].kit[1][P_VOL] = 96;   /* the FX's own at unity */
                pat[fp].kit[0][P_HOLD] = 127; pat[fp].kit[0][P_GATE] = pat[fp].kit[1][P_GATE] = 127;  /* both held: steady */
                pat[fp].kit[1][P_SYN + 7] = 45; pat[fp].kit[1][P_SYN + 3] = (uint8_t)(c ? 127 : 0);
                if (c == 2) pat[fp].kit[0][P_LEVL] = 0;
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) { sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; }
                rl[c] = 10 * log10(sl + 1); rr[c] = 10 * log10(sr + 1);
            }
            printf("     FX-RING, its side against the source's: MIX 0 %+.2f dB, MIX 127 %+.2f dB; the source at LEV 0 %.0f dB\n",
                   rr[0] - rl[0], rr[1] - rl[1], rr[2] - rr[1]);
            CHECK(fabs(rr[0] - rl[0]) < 1.0 && fabs(rr[1] - rr[0]) > 1.0 && rr[2] - rr[1] < -60,
                  "FX-RING: the source as it is at MIX 0 (INP 45), rung at 127, silent at the source's LEV 0");
            double spread[2];
            for (int c = 0; c < 2; c++) {
                FXPAT(M_FXD, M_GSIN, 0);
                for (int s_ = 0; s_ < 16; s_ += 4) pat[fp].step[0][s_].note = 36;
                pat[fp].kit[0][P_VOL] = 0;
                pat[fp].kit[1][P_SYN + 4] = (uint8_t)(c ? 127 : 0);      /* RAT */
                pat[fp].kit[1][P_SYN + 2] = 40;                          /* THRS */
                pat[fp].kit[1][P_SYN + 5] = (uint8_t)(c ? 90 : 0);       /* GAIN */
                pat[fp].kit[0][P_DEC] = 37;                              /* 0.18 s (format 15's 30) */
                /* the second and fourth hits 8 dB lower going in (INP 40 of 100): a DIST under 64 at the source lowered
                 * them by 3 to 15 dB, as the sine's phase at the hit fell */
                lk(&pat[fp], 1, 4, P_SYN + 7, 40);
                lk(&pat[fp], 1, 12, P_SYN + 7, 40);
                fx_measure(fp, 0x3F, 2.0, kb, &pk, &rm);
                int a0 = 0, a1 = 0;
                int step = M2K_SR / 8;                                   /* a sixteenth at the blank's 120 bpm, in frames */
                for (int i = 0; i < 2 * step; i++) { int v = abs(kb[2 * i]); if (v > a0) a0 = v; v = abs(kb[2 * (i + 4 * step)]); if (v > a1) a1 = v; }
                spread[c] = 20 * log10((a0 + 1.0) / (a1 + 1.0));
            }
            printf("     FX-DYNAMIX: loud against soft %.1f dB at RAT 0, %.1f dB at RAT 127\n", spread[0], spread[1]);
            CHECK(spread[1] < spread[0] - 6, "FX-DYNAMIX: brings a soft hit up to a loud one");
            /* its quick attack (MACH_DEF's ATK) holds a note's first peak as it holds the next: one note from silence on
             * a plain SWAV-SAW (its high-pass leaves a spike at each of the saw's resets), the peak at its first reset
             * against its second. A block's gain ramped from the block before passed the first ~6-10 dB over */
            static const uint8_t DN[6] = {33, 40, 45, 48, 52, 57};
            double first = -99;
            for (int c = 0; c < 6; c++) {
                FXPAT(M_FXD, M_SAW, 0);
                for (int s_ = 4; s_ < 16; s_ += 4) pat[fp].step[0][s_].flags = pat[fp].step[1][s_].flags = 0;
                pat[fp].step[0][0].note = pat[fp].step[1][0].note = DN[c];
                fx_measure(fp, 0x3F, 0.25, kb, &pk, &rm);
                double per = M2K_SR / (440.0 * pow(2, (DN[c] - 69) / 12.0));
                int rs[2] = {0, 0};
                for (int k = 0; k < 2; k++) {                           /* the reset's peak, within half a ms */
                    int at = (int)((k + 1) * per);
                    for (int i = at - 22; i < at + 22; i++) if (abs(kb[2 * i]) > rs[k]) rs[k] = abs(kb[2 * i]);
                }
                double d = 20 * log10((rs[0] + 1.0) / (rs[1] + 1.0));
                if (d > first) first = d;
            }
            printf("     FX-DYNAMIX: a note's first peak over its second, at most %+.1f dB (A1 to A3)\n", first);
            CHECK(first < 5, "FX-DYNAMIX: a note's first peak held as the next (its gain falls within the block, no ramp from the last)");
            /* the MOD effect's chorus across the stereo (MONO2K's own): a saw through it wide, a low sine through it still
             * in the middle (the side high-passed), the phaser and MIX 0 mono */
            double mc[4];
            for (int c = 0; c < 4; c++) {
                m2k_init(2000);
                pat_init(&pat[fp], "MOD");
                kit_default(&pat[fp], 0, c == 1 ? M_GSIN : M_SAW);
                pat[fp].kit[0][P_HOLD] = 127; pat[fp].glob[G_DRIV] = 0;
                pat[fp].kit[0][P_MODT] = enc(c == 2 ? 2 : 0, 3); pat[fp].kit[0][P_MODR] = 60;
                pat[fp].kit[0][P_MODD] = 127; pat[fp].kit[0][P_MODM] = (uint8_t)(c == 3 ? 0 : 100);
                for (int s_ = 0; s_ < 16; s_ += 4) {
                    pat[fp].step[0][s_].flags = ST_TRIG; pat[fp].step[0][s_].note = (uint8_t)(c == 1 ? 29 : 48);
                }
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0, slr = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) { sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; slr += (double)kb[2 * i] * kb[2 * i + 1]; }
                mc[c] = slr / sqrt(sl * sr + 1);
            }
            printf("     MOD chorus L/R: a saw %.3f, an F1 sine %.4f; the phaser %.4f, MIX 0 %.4f\n", mc[0], mc[1], mc[2], mc[3]);
            CHECK(mc[0] < 0.9 && mc[1] > 0.99 && mc[2] > 0.999 && mc[3] > 0.999,
                  "MOD: the chorus wide on a saw, an F1 sine in the middle; the phaser and MIX 0 mono");
            /* GND-NOIS on the Monomachine's page: ST with STON across the stereo (L/R falls as ST rises), without it in
             * the middle; RED darkens (its top falls). GND-PERC, MONO2K's own noise, keeps the machine number the old
             * GND-NOIS had, so saved patterns load as they were */
            double gc[4], top[2];
            for (int c = 0; c < 6; c++) {
                static const uint8_t ST[6] = {127, 64, 127, 0, 0, 0}, STON[6] = {127, 127, 0, 0, 0, 0}, RED[6] = {0, 0, 0, 0, 0, 127};
                m2k_init(2000);
                pat_init(&pat[fp], "NOIS");
                kit_default(&pat[fp], 0, M_GNOI);
                pat[fp].kit[0][P_HOLD] = 127; pat[fp].glob[G_DRIV] = 0;
                pat[fp].kit[0][P_SYN + 0] = ST[c]; pat[fp].kit[0][P_SYN + 1] = RED[c]; pat[fp].kit[0][P_SYN + 2] = STON[c];
                for (int s_ = 0; s_ < 16; s_ += 4) { pat[fp].step[0][s_].flags = ST_TRIG; pat[fp].step[0][s_].note = 60; }
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0, slr = 0, hf = 0, all = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) {
                    sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; slr += (double)kb[2 * i] * kb[2 * i + 1];
                    double d1 = (double)kb[2 * i] - kb[2 * i - 2];                /* the first difference: the top */
                    hf += d1 * d1; all += (double)kb[2 * i] * kb[2 * i];
                }
                if (c < 4) gc[c] = slr / sqrt(sl * sr + 1);
                if (c >= 4) top[c - 4] = 10 * log10(hf / (all + 1) + 1e-12);
            }
            printf("     GND-NOIS L/R: ST 127 STON %.3f, ST 64 STON %.3f, ST 127 alone %.4f, ST 0 %.4f; its top, RED 127 against 0: %.1f dB\n",
                   gc[0], gc[1], gc[2], gc[3], top[1] - top[0]);
            CHECK(gc[0] < 0.1 && gc[1] > 0.45 && gc[1] < 0.75 && gc[2] > 0.999 && gc[3] > 0.999 && top[1] < top[0] - 15,
                  "GND-NOIS: ST with STON across the stereo, without it in the middle; RED darkens");
            CHECK(M_GPRC == 1 && M_GNOI == 14 && M_DKIK == 15 && M_DCYM == M_COUNT - 1,
                  "GND-PERC keeps the old GND-NOIS's number, GND-NOIS 14, the DRM- drums after it");
            #undef FXPAT
        }
        /* the bank on the screen: a DIGI-WAV track's SYN has a third page, BANK; the wave knob names the bank's waves */
        {
            m2k_init(7);
            open_b01();                                               /* T1 GND-SIN, T4 DIGI-WAV */
            kit_default(&pat[16], 3, M_WAV);
            int wt = -1;
            for (int t = 0; t < M2K_TRACKS; t++) if (pat[16].machine[t] == M_WAV) { wt = t; break; }
            ui.trk = (uint8_t)(wt < 0 ? 0 : wt);
            char v1[16], v2[16], n1[16], n2[16];
            const char *u;
            pat[16].kit[ui.trk][P_WBNK] = 0;
            fmt(pinfo(ui.trk, P_WBNK), ui.trk, 0, v1, &u);
            fmt(pinfo(ui.trk, P_SYN), ui.trk, 10, n1, &u);
            pat[16].kit[ui.trk][P_WBNK] = 127;
            fmt(pinfo(ui.trk, P_WBNK), ui.trk, 127, v2, &u);
            fmt(pinfo(ui.trk, P_SYN), ui.trk, 10, n2, &u);
            int three = nsub(PG_SYN) == 4 && knob_param(PG_SYN, 2, 0) == P_WBNK && knob_param(PG_SYN, 3, 0) == SETTING(18);
            ui.trk = 0;                                               /* T1: GND-SIN */
            int two = nsub(PG_SYN) == 3 && knob_param(PG_SYN, 2, 0) == SETTING(18) && knob_param(PG_SYN, 2, 2) == SETTING(20);
            CHECK(wt >= 0 && three && two && !strcmp(v1, "M2K") && !strcmp(v2, "M64") && !strcmp(n2, "OS06") && strcmp(n1, n2),
                  "ui: a wave machine's SYN has a BANK page (M2K / M64) before SND, its WAVE named from the bank; the others SND");
            m2k_init(7);
        }
        /* the owner's 64 as DIGI-WAV plays them, C4 held a second: the machine's own output (before the chain) stays in
         * 16 bits, no wave reaching the rail, and carries no DC (a mean over 261 cycles: a part cycle's worth at most) */
        {
            int over = 0, dcw = 0;
            long worst = 0;
            for (int k = 0; k < 64; k++) {
                m2k_init(7);
                open_b01();
                pat[16].machine[0] = M_WAV;
                static const uint8_t syn[8] = {0, 64, 127, 0, 0, 64, 0, 0};
                for (int j = 0; j < 8; j++) pat[16].kit[0][P_SYN + j] = syn[j];
                pat[16].kit[0][P_SYN] = (uint8_t)(2 * k);
                pat[16].kit[0][P_WBNK] = 127;
                pat[16].kit[0][P_HOLD] = 127;
                pat[16].kit[0][P_DEC] = 127;
                m2k_midi(0x90, 60, 100);
                int16_t lr[32];
                int pk = 0;
                long sum = 0, n = 0;
                for (int f = 0; f < M2K_SR; f += 16) {
                    m2k_render(lr, 16);
                    if (f < M2K_SR / 10) continue;                    /* past the onset */
                    for (int i = 0; i < M2K_SUB; i++) {
                        int v = blk[0][i];
                        pk = v > pk ? v : -v > pk ? -v : pk;
                        sum += v;
                        n++;
                    }
                }
                long mean = sum / n;
                over += pk >= 32767;
                dcw += mean > 300 || mean < -300;
                if (labs(mean) > worst) worst = labs(mean);
            }
            m2k_init(7);
            printf("     the owner's 64 played: %d at the rail, %d with DC left, the worst mean %ld\n", over, dcw, worst);
            CHECK(!over && !dcw, "digi-wav: the owner's 64 play in 16 bits, their DC out");
        }
        /* LEV's silence step by step, and LFOs off an ENS page: an old pattern made for it */
        {
            static m2k_pattern_t e, g;
            static uint8_t eb[BLOB_MAX];
            m_set(&e, 0, sizeof e);
            pat_init(&e, "ENSOLD");
            const uint8_t oldk[8] = {21, 36, 61, 30, 0, 64, 0, 0};             /* +4 +7 +11, LEV 0 */
            for (int t = 1; t <= 3; t++) { e.machine[t] = M_ENS; m_cpy(e.kit[t], oldk, 8); }
            e.kit[2][P_SYN + 6] = 100;                                          /* T3: LEV up, one trig locked to 0 */
            e.machine[4] = M_GSIN;
            for (int t = 1; t <= 3; t++) for (int st = 0; st < 16; st += 4) { e.step[t][st].flags = ST_TRIG; e.step[t][st].note = 48; }
            lk(&e, 1, 4, P_SYN + 6, 100); lk(&e, 1, 12, P_SYN + 6, 100);       /* T2: raised on 4 and 12 */
            lk(&e, 1, 12, P_SYN + 0, 26);                                      /* and an interval locked there (+5) */
            lk(&e, 2, 8, P_SYN + 6, 0);
            uint8_t *lf = &e.kit[0][P_LFO];                                     /* T1's LFOs: onto T2's PCH2, T5's A */
            lf[L_TRCK] = enc(1, M2K_TRACKS); lf[L_DEST] = P_SYN + 0 + 1; lf[L_DPTH] = 90;
            lf[8 + L_TRCK] = enc(4, M2K_TRACKS); lf[8 + L_DEST] = P_SYN + 0 + 1; lf[8 + L_DPTH] = 90;
            lf[16 + L_TRCK] = enc(1, M2K_TRACKS); lf[16 + L_DEST] = P_ATK + 1;
            lk(&e, 0, 4, P_LFO + 16 + L_DEST, P_SYN + 2 + 1);                 /* LFO 3 locked onto T2's PCH3 at step 4 */
            int en = pat_pack_v(&e, eb, 6);
            int ok_load = pat_unpack(&g, eb, en);
            int off2[16] = {0}, off3[16] = {0}, up12 = -1, lev_left = 0, lfo_dest_lock = -1;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &g.lock[i];
                if (l->trk == 0xFF) continue;
                if (l->trk == 1 && l->param <= P_SYN + 2 && l->val == 62) off2[l->step]++;
                if (l->trk == 2 && l->param <= P_SYN + 2 && l->val == 62) off3[l->step]++;
                if (l->trk == 1 && l->step == 12 && l->param == P_SYN) up12 = l->val;
                if (l->trk >= 1 && l->trk <= 3 && l->param == P_SYN + 6) lev_left++;
                if (l->trk == 0 && l->param == P_LFO + 16 + L_DEST) lfo_dest_lock = l->val;
            }
            CHECK(ok_load && g.kit[1][P_SYN] == T_ENS_ET[4 + 10] && off2[0] == 3 && off2[8] == 3 && !off2[4] && !off2[12] &&
                  up12 == T_ENS_ET[5 + 10] && lev_left == 0,
                  "format: a chord LEV 0 silenced but locks raise keeps its voices there, OFF on the other trigs");
            CHECK(off3[8] == 3 && !off3[0] && !off3[4] && g.kit[3][P_SYN] == 62 && g.kit[3][P_SYN + 1] == 62,
                  "format: a LEV lock to 0 is an OFF trig; a chord never raised is OFF");
            CHECK(g.kit[0][P_LFO + L_DEST] == 0 && g.kit[0][P_LFO + 8 + L_DEST] == P_SYN + 7 + 1 && g.kit[0][P_LFO + 16 + L_DEST] == P_ATK + 1 &&
                  lfo_dest_lock == 0, "format: an LFO aimed at an ENS interval is cleared (also a step's lock onto one); others kept"
                  " (the one on T5's GND-SIN TUNE where TUNE went, H)");
        }
        /* and every chord of bank A as it stood at ea69d53 (the fixture, version 6) keeps its notes at every trig */
        {
            FILE *f = fopen("tools/ref/fixtures/bank_a_ea69d53.bin", "rb");
            static uint8_t fx[16 * BLOB_MAX];
            int fl = f ? (int)fread(fx, 1, sizeof fx, f) : 0, o = 0, chords = 0, kept = 0, exact_bad = 0, off_bad = 0;
            if (f) fclose(f);
            for (int d = 0; d < 16 && o + 2 <= fl; d++) {
                int n6 = fx[o] | (fx[o + 1] << 8);
                static m2k_pattern_t was, now;
                static uint8_t raw[BLOB_MAX];
                m_cpy(raw, fx + o + 2, (size_t)n6);
                raw[1] = 7;                                       /* read as is: the old page's values */
                int ok1 = pat_unpack(&was, raw, n6), ok2 = pat_unpack(&now, fx + o + 2, n6);
                o += 2 + n6;
                if (!ok1 || !ok2) { off_bad++; continue; }
                for (int t = 0; t < M2K_TRACKS; t++) {
                    if (was.machine[t] != M_ENS) continue;
                    for (int st = 0; st < M2K_STEPS; st++) {
                        if (!(was.step[t][st].flags & ST_TRIG)) continue;
                        uint8_t ok[8], nk[8];
                        m_cpy(ok, was.kit[t], 8); m_cpy(nk, now.kit[t], 8);
                        for (int i = 0; i < M2K_LOCKS; i++) {
                            if (was.lock[i].trk == t && was.lock[i].step == st && was.lock[i].param < 8) ok[was.lock[i].param] = was.lock[i].val;
                            if (now.lock[i].trk == t && now.lock[i].step == st && now.lock[i].param < 8) nk[now.lock[i].param] = now.lock[i].val;
                        }
                        unsigned pcw = 1, pcn = 1;                    /* the root's class: bit 0 */
                        for (int k = 0; k < 3; k++) {
                            int oi = (ok[k] * 25) >> 7, c64 = T_ENS_PCH[nk[k]];
                            if (ok[6]) pcw |= 1u << (oi % 12);
                            if (c64 == ENS_OFF) { if (ok[6]) off_bad++; continue; }
                            pcn |= 1u << (((c64 / 64) % 12 + 12) % 12);
                            if (oi >= 1 && oi <= 12 && c64 != oi * 64) exact_bad++;
                            if (nk[k] >= 61 && nk[k] <= 63) off_bad++;
                        }
                        chords++;
                        kept += pcw == pcn;
                    }
                }
            }
            printf("     bank A's SWAV-ENS trigs: %d, their notes kept: %d\n", chords, kept);
            CHECK(fl > 0 && chords > 0 && kept == chords && exact_bad == 0 && off_bad == 0,
                  "format: every chord of bank A (ea69d53) keeps its notes through version 7, none in OFF");
        }
    }
    /* the clipboard, a sound: track 6's copied with EDIT held + REC, pasted onto track 1 with EDIT + PLAY */
    {
        m2k_init(7);
        uint32_t now = 100000;
        #define TAP_B(bb) do { m2k_button(bb, 1); m2k_frame(now += 20); m2k_button(bb, 0); m2k_frame(now += 20); } while (0)
        #define GLO_B(bb) do { m2k_button(B_EDIT, 1); m2k_frame(now += 20); TAP_B(bb); m2k_button(B_EDIT, 0); m2k_frame(now += 20); } while (0)
        cur_pat = 0;
        m2k_track(5);
        GLO_B(B_REC);
        CHECK(!strcmp(ui.toast, "COPY") && !strcmp(ui.toast2, "T6 SOUND"), "clipboard: track 6's sound copied (T6 SOUND)");
        m2k_track(0);
        GLO_B(B_PLAY);
        CHECK(!strcmp(ui.toast, "T1 SOUND PASTED") && pat[0].machine[0] == pat[0].machine[5] &&
              !memcmp(pat[0].kit[0], pat[0].kit[5], P_COUNT), "clipboard: pasted onto track 1, machine and kit");
        #undef GLO_B
        #undef TAP_B
        m2k_init(7);                                             /* the paste is not saved: the tests after start clean */
    }
    /* every value's text fits the smallest buffer it is written to (12 bytes), on every page and machine */
    {
        int longest = 0;
        char buf[64];
        const char *u;
        const pi_t *sets[] = {PI_COMMON, PI_LFO, PI_MOD, PI_GLOB, PI_SET};
        const int counts[] = {P_LFO - 8, 24, 4, G_COUNT, 3};
        for (int m = 0; m < M_COUNT + 5; m++) {
            const pi_t *tab = m < M_COUNT ? MI[m].syn : sets[m - M_COUNT];
            int cnt = m < M_COUNT ? 8 : counts[m - M_COUNT];
            for (int i = 0; i < cnt; i++)
                for (int t = 0; t < M2K_TRACKS; t++)
                    for (int v = 0; v < 128; v++) {
                        memset(buf, 0, sizeof buf);
                        fmt(&tab[i], t, v, buf, &u);
                        int n = (int)strlen(buf) + 1;
                        if (n > longest) longest = n;
                    }
        }
        printf("     the longest value text: %d bytes\n", longest);
        CHECK(longest <= 12, "ui: every value's text fits in 12 bytes");
    }
    /* the sequencer's keys, as a hand plays them: pages, the length, step entry */
    {
        uint32_t now = 100000;
        #define TAP_B(bb) do { m2k_button(bb, 1); m2k_frame(now += 20); m2k_button(bb, 0); m2k_frame(now += 20); } while (0)
        int kp[4], kw0 = -1, kw1 = -1, kw2 = -1;
        for (int k = 0; k < M2K_KEYS; k++) {
            if (ui.kb[k] >= 6 && ui.kb[k] < 10) kp[ui.kb[k] - 6] = k;
            if (ui.kw[k] == 0) kw0 = k;
            if (ui.kw[k] == 2) kw1 = k;
            if (ui.kw[k] == 4) kw2 = k;
        }
        m2k_pattern(16);                                         /* B01 INIT, 16 steps */
        m2k_frame(now);
        TAP_B(B_SEQ);                                            /* GRID */
        m2k_key(kp[3], 1); m2k_frame(now += 50); m2k_key(kp[3], 0); m2k_frame(now += 50);
        CHECK(ui.spage == 0 && seq_len() == 16, "grid: P4 past a 16-step pattern's end keeps the steps in view");
        m2k_pattern_t keep = pat[16];
        m2k_key(kp[1], 1);
        for (int i = 0; i < 40; i++) m2k_frame(now += 20);       /* held 0.8 s */
        m2k_key(kp[1], 0); m2k_frame(now += 20);
        int same = 1;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < 16; st++)
            if (memcmp(&pat[16].step[t][st + 16], &keep.step[t][st], sizeof keep.step[t][st])) same = 0;
        CHECK(seq_len() == 32 && ui.spage == 1 && same, "grid: holding P2 makes the pattern 32 steps, the beat going on");
        pat[16] = keep;
        TAP_B(B_SEQ);                                            /* KEYS */
        TAP_B(B_REC);                                            /* stopped: step entry */
        int c0 = ui.cursor;
        m2k_key(kw0, 1); m2k_key(kw0, 0);
        m2k_key(kw2, 1); m2k_key(kw2, 0);
        m2k_button(B_EDIT, 1); m2k_button(B_EDIT, 0);
        m2k_key(kw1, 1); m2k_key(kw1, 0);
        m2k_step_t *sp = pat[16].step[ui.trk];
        int lo = 53 + 12 * ui.oct[ui.trk];                       /* (the track's keys at its home's octave) */
        CHECK(sp[c0].note == lo + kw0 && sp[c0 + 1].note == lo + kw2 && !(sp[c0 + 2].flags & ST_TRIG) &&
              sp[c0 + 3].note == lo + kw1 && (sp[c0 + 3].flags & ST_TRIG), "keys: step entry writes notes, EDIT a rest");
        TAP_B(B_REC);
        pat[16] = keep;
        m2k_pattern(0);
        m2k_frame(now += 20);
        #undef TAP_B
    }
    /* edit, reload */
    int v0 = pat[0].kit[0][P_DEC];
    pat[0].kit[0][P_DEC] = 3;
    pattern_reload();
    CHECK(pat[0].kit[0][P_DEC] == v0, "reload brings the saved pattern back");
    /* save a pattern, edit, reload: the saved edit stays */
    pat[0].kit[0][P_DEC] = 11;
    pattern_save();
    pat[0].kit[0][P_DEC] = 99;
    pattern_reload();
    CHECK(pat[0].kit[0][P_DEC] == 11, "save sets the reload point");
    /* the trig types (the Monomachine's trig tracks): each of the eight restarts exactly the amp, the filter's envelope
     * and the LFOs it names, and none of the others */
    {
        static const unsigned SK[8] = {0, ST_NOFLT | ST_NOLFO, ST_NOAMP | ST_NOLFO, ST_NOAMP | ST_NOFLT, ST_NOLFO, ST_NOFLT,
                                       ST_NOAMP, ST_TTYPE};
        int ok = 1;
        for (int i = 0; i < 8; i++) {
            double a, br; int lr;
            trig_probe(SK[i], &a, &br, &lr);
            int wa = !(SK[i] & ST_NOAMP), wf = !(SK[i] & ST_NOFLT), wl = !(SK[i] & ST_NOLFO);
            int good = (wa ? a > -40 : a < -90) && (wf ? br > 10 : br < 3) && lr == wl;
            printf("     trig %-7s amp %6.1f dB  filter %+5.1f dB  LFO %s%s\n", (const char *[]){"ALL", "AMP", "FILTER",
                   "LFO", "AMP+FLT", "AMP+LFO", "FLT+LFO", "NONE"}[i], a, br, lr ? "restarts" : "runs on", good ? "" : "  <- wrong");
            ok &= good;
        }
        CHECK(ok, "trig types: each restarts the amp, the filter's envelope and the LFOs it names, no other");
        m2k_init(7);
        open_b01();
        m2k_pattern_t *p = &pat[16];
        p->step[0][4] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
        ui.grid = 1; ui.nheld = 1; ui.held[0] = 4; ui.trk = 0;
        m2k_encoder(E_MASTER, 1);
        int t1 = p->step[0][4].flags;
        m2k_encoder(E_MASTER, 1);
        m2k_encoder(E_ALGO, 1);                                     /* the accent / slide knob keeps the trig type */
        int t2 = p->step[0][4].flags;
        m2k_encoder(E_MASTER, -1); m2k_encoder(E_MASTER, -1); m2k_encoder(E_MASTER, -1);
        int t3 = p->step[0][4].flags & ST_TTYPE;
        ui.grid = 0; ui.nheld = 0;
        CHECK(t1 == (ST_TRIG | ST_NOFLT | ST_NOLFO) && t2 == (ST_TRIG | ST_ACC | ST_NOAMP | ST_NOLFO) && t3 == ST_TTYPE,
              "trig types: a held step and VOL step through them (ALL AMP FILTER ... NONE), the accent knob keeps them");
        static uint8_t tb[BLOB_MAX];
        static m2k_pattern_t tw;
        p->step[0][9] = (m2k_step_t){.note = 50, .flags = ST_TRIG | ST_NOFLT};
        int tn = pat_pack(p, tb);
        CHECK(pat_unpack(&tw, tb, tn) && tw.step[0][9].flags == (ST_TRIG | ST_NOFLT), "trig types: saved and loaded with the step");
    }
    /* trigs and locks survive a pack */
    int trigs = 0, locks = 0;
    for (int t = 0; t < 6; t++) for (int st = 0; st < 64; st++) trigs += pat[0].step[t][st].flags & 1;
    for (int i = 0; i < M2K_LOCKS; i++) locks += pat[0].lock[i].trk != 0xFF;
    m2k_pattern_t keep = pat[0];
    pattern_save(); pattern_reload();
    int same = !memcmp(&keep.kit, &pat[0].kit, sizeof keep.kit) && !memcmp(keep.machine, pat[0].machine, 6);
    for (int t = 0; t < 6; t++) for (int st = 0; st < 64; st++)
        if ((keep.step[t][st].flags & 1) && memcmp(&keep.step[t][st], &pat[0].step[t][st], 4)) same = 0;
    CHECK(same, "pack / unpack keeps sounds, trigs, notes, conditions");
    printf("     A01: %d trigs, %d locks\n", trigs, locks);
    /* a project: save into P2, change, load back */
    project_save(1);
    CHECK(cur_proj == 1, "save project into P2 opens it");
    pat[5].kit[2][P_VOL] = 1;
    project_load(1);
    CHECK(pat[5].kit[2][P_VOL] != 1, "load project discards unsaved work");
    project_load(0);
    CHECK(!strcmp(pat[1].name, "TIDE"), "load DEMO again");
    /* the store, out and back */
    pat[3].kit[1][P_PAN] = 17;
    int n = m2k_store_pack();
    printf("     store: %d bytes\n", n);
    m2k_init(9);
    CHECK(pat[3].kit[1][P_PAN] != 17, "fresh init is the factory");
    CHECK(m2k_store_unpack(n) == 1, "store unpacks");
    CHECK(pat[3].kit[1][P_PAN] == 17, "the working state comes back");
    /* export / import */
    int e = m2k_proj_export(0);
    CHECK(m2k_proj_import(3, e) == 1 && !strcmp(m2k_project_name(3), "DEMO"), "export DEMO, import into P4");
    store_buf[0] = 'X';
    CHECK(m2k_proj_import(2, e) == 0, "a bad file is refused");
    /* a full project: fill every pattern with trigs and locks until the image refuses */
    for (int i = 0; i < M2K_PATTERNS; i++) {
        for (int t = 0; t < 6; t++) for (int st = 0; st < 64; st++) { pat[i].step[t][st].flags = 1; pat[i].step[t][st].note = 60; }
        for (int k = 0; k < M2K_LOCKS; k++) pat[i].lock[k] = (m2k_lock_t){(uint8_t)(k % 6), (uint8_t)(k % 64), 9, 9};
    }
    CHECK(project_save(2) == 0, "a project too big for an image is refused, not corrupted");
    CHECK(!strcmp(m2k_project_name(2), "P3"), "the refused slot is untouched");
    /* a store saved before today's demos: a slot of today's (A01-A04) still holding the one its revision shipped takes
     * today's, the owner's stay, and A05-A16 have no demo now: whatever they hold stays, never blanked. Revision 0
     * (before the store kept one: A16 was INIT, the owner's once written in), 1 to 9, by name (7: the bank before its
     * reorder, every part in another slot; 8: before CUTUP and RIVETS swapped) */
    for (int rev = 0; rev < 10; rev++) {
        static const char *const OLDN[10] = {"SPU RM", "DAYDRM", "GLITCH", "DREXC", "FACETS", "TORQUE", "PYLONS", "HAZE", "RIVETS",
                                             "MAMMOTH"};
        int sl = rev == 2 ? 4 : rev == 3 || rev == 6 ? 8 : rev == 4 ? 3 : rev == 5 ? 5 : rev == 7 || rev == 9 ? 0 : rev == 8 ? 1 : 2;   /* a slot the next revision replaced */
        m2k_init(11);
        m2k_pattern_t keep2 = pat[sl], keep0 = pat[0], keep6 = pat[6];
        s_cpy(pat[sl].name, OLDN[rev]);                      /* an old demo, as shipped */
        if (rev == 3) { s_cpy(pat[0].name, "HAZE"); pat[0].kit[5][P_DIST] = 74; }   /* A01 as revision 3 shipped it: HAZE's lead at DIST 74 */
        if (rev == 7) { s_cpy(pat[6].name, "VESPER"); pat[6].kit[5][P_DIST] = 74; }   /* A07 VESPER, in its slot before and after the reorder */
        s_cpy(pat[7].name, "MYSONG");                        /* the owner's */
        if (rev) {
            s_cpy(pat[15].name, rev >= 8 ? "SPLICE" : rev >= 5 ? "FOUNDRY" : "SHARDS");   /* the A16 its revision shipped */
        } else {
            pattern_init_blank(&pat[15]);
            pat[15].step[0][0].flags = ST_TRIG;              /* A16 INIT, but written in: the owner's */
        }
        project_save(0);
        int n = m2k_store_pack();
        if (rev & 1) {                                       /* version 3, at the factory's revision it was saved at */
            store_buf[7] = (uint8_t)rev;
        } else if (rev) {                                    /* version 2: no counts (the build's own), no sounds */
            store_buf[7] = (uint8_t)rev;
            memmove(store_buf + 8, store_buf + 10, (size_t)n - 10);
            store_buf[4] = 2;
            n -= 2;
        } else {
            memmove(store_buf + 7, store_buf + 10, (size_t)n - 10);  /* version 1: before the factory's revision was kept */
            store_buf[4] = 1;
            n -= 3;
        }
        m2k_init(12);
        CHECK(m2k_store_unpack(n) == 1, rev ? "store: one saved at an earlier factory revision loads" : "store: one saved before today's demos loads");
        if (sl < FACTORY_DEMOS)
            CHECK(!strcmp(pat[sl].name, keep2.name) && !memcmp(pat[sl].kit, keep2.kit, sizeof keep2.kit),
                  "store: an old demo left in A01-A04 becomes today's");
        else
            CHECK(!strcmp(pat[sl].name, OLDN[rev]) && pat_pack_img(&pat[sl]), "store: an old demo in A05-A16 stays (no demo there now)");
        int a16 = rev ? !strcmp(pat[15].name, rev >= 8 ? "SPLICE" : rev >= 5 ? "FOUNDRY" : "SHARDS")
                      : !strcmp(pat[15].name, "INIT") && (pat[15].step[0][0].flags & ST_TRIG);
        CHECK(!strcmp(pat[7].name, "MYSONG") && a16, rev ? "store: the owner's kept, an earlier revision's A16 too (never blanked)"
                                                         : "store: the owner's patterns are kept");
        if (rev == 3)
            CHECK(!memcmp(pat[0].kit, keep0.kit, sizeof keep0.kit), "store: A01 as revision 3 shipped it (HAZE) takes today's");
        if (rev == 7)
            CHECK(!strcmp(pat[6].name, "VESPER") && pat[6].kit[5][P_DIST] == 74 && memcmp(pat[6].kit, keep6.kit, sizeof keep6.kit),
                  "store: A07 VESPER as it was (A05-A16 have no demo now)");
    }
    /* revision 9 moved CUTUP to A02 and RIVETS to A11 (host/m2k_bank_rev9.c): its A02 CUTUP takes today's, a RIVETS
     * there is not what it shipped (revision 8's was): the owner's; its A11 RIVETS stays */
    CHECK(name_is(OLD_DEMO[9][1], "CUTUP") && name_is(OLD_DEMO[9][10], "RIVETS") && name_is(OLD_DEMO[8][1], "RIVETS") &&
          name_is(OLD_DEMO[8][10], "CUTUP"), "store: revision 9's demos, CUTUP at A02 and RIVETS at A11");
    for (int k = 0; k < 2; k++) {
        m2k_init(13);
        m2k_pattern_t today = pat[1];
        s_cpy(pat[1].name, k ? "RIVETS" : "CUTUP");
        s_cpy(pat[10].name, "RIVETS");
        project_save(0);
        int n = m2k_store_pack();
        store_buf[7] = 9;
        m2k_init(14);
        int ok = m2k_store_unpack(n) == 1 && !strcmp(pat[10].name, "RIVETS");
        if (!k) CHECK(ok && !memcmp(&pat[1], &today, sizeof today), "store: revision 9's A02 CUTUP becomes today's TIDE, its A11 RIVETS stays");
        else CHECK(ok && !strcmp(pat[1].name, "RIVETS"), "store: a RIVETS at A02 in revision 9's store is the owner's: kept");
    }
    /* revision 10 (LO-BIT CLUB) on: a slot takes today's only while it is byte for byte what its revision shipped. A03
     * LATTICE as it shipped becomes today's SUSPEND, in the work and in P1; with a knob turned under its name it is the
     * owner's; A05 PROCESS under the owner's work and A06-A16 untouched all stay (no demo there now) */
    {
        static uint8_t lb[BLOB_MAX], rb[BLOB_MAX];
        int ln = 0;
        for (const char *h = LATTICE_R10; h[0] && h[1]; h += 2) { unsigned v; sscanf(h, "%2x", &v); lb[ln++] = (uint8_t)v; }
        for (int k = 0; k < 2; k++) {
            m2k_init(15);
            m2k_pattern_t today = pat[2];
            for (int i = 0; i < 16; i++) { memset(pat[i].name, 0, 8); s_cpy(pat[i].name, OLD_DEMO[10][i]); }
            int same = pat_unpack(&pat[2], lb, ln) && pat_pack(&pat[2], rb) == ln;   /* (format 16's layout: as long) */
            if (k) pat[2].kit[3][P_VOL] ^= 1;         /* a knob turned a detent, the name kept */
            int vol = pat[2].kit[3][P_VOL], mine = 0;
            for (int s = 0; s < 64; s++) { pat[4].step[0][s].flags = ST_TRIG; pat[4].step[0][s].note = (uint8_t)(36 + s % 12); }
            project_save(0);
            int n = m2k_store_pack(), len;
            if (!k) n = store_splice(n, 2, lb, ln);     /* untouched: its bytes as that build packed them */
            store_buf[7] = 10;
            m2k_init(16);
            int ok = m2k_store_unpack(n) == 1;
            const uint8_t *b = img_get(&proj[0], 2, &len);
            for (int s = 0; s < 64; s++) mine += pat[4].step[0][s].flags & ST_TRIG;
            int rest = !strcmp(pat[4].name, "PROCESS") && mine == 64;
            for (int i = 5; i < 16; i++) rest &= !strcmp(pat[i].name, OLD_DEMO[10][i]) && proj[0].len[i] > 0;
            if (!k)
                CHECK(ok && same && !memcmp(&pat[2], &today, sizeof today) && len == pat_pack(&today, rb) && !memcmp(b, rb, (size_t)len),
                      "store: revision 10's A03 LATTICE as it shipped becomes today's, in the work and in P1");
            else
                CHECK(ok && !strcmp(pat[2].name, "LATTICE") && pat[2].kit[3][P_VOL] == vol && len == ln,
                      "store: revision 10's LATTICE with a knob turned under its name is the owner's: kept");
            CHECK(ok && rest, "store: revision 10's A05 PROCESS with the owner's work and A06-A16 stay, none blanked");
        }
    }
    /* revision 11 (12: format 16's amp and LFO laws): A01 CIRCUIT as it shipped, packed in format 15, loads as today's
     * (its conversion on load the one bank A takes at build) and, untouched in a revision-11 store, becomes today's in
     * the work and in P1; a version-3 store's sounds take format 16's laws, a version-4 store's load as they are */
    {
        static uint8_t cb[BLOB_MAX], rb[BLOB_MAX];
        static m2k_pattern_t q;
        int cn = 0, len;
        for (const char *h = CIRCUIT_R11; h[0] && h[1]; h += 2) { unsigned v; sscanf(h, "%2x", &v); cb[cn++] = (uint8_t)v; }
        m2k_init(18);
        m2k_pattern_t today = pat[0];
        CHECK(today.kit[0][P_DEC] == 97 && today.kit[2][P_LFO + L_DPTH] == 11 && today.kit[1][P_LFO + L_SPD] == 60,
              "factory: bank A turned at build onto format 16's laws (A01's kick DEC 78, 4.4 s, as 97; its hats' LFO on DEC)");
        CHECK(cb[1] == 15 && fnv(cb, (uint32_t)cn) == 0x73cc380fu && pat_unpack(&q, cb, cn) && !memcmp(&q, &today, sizeof q),
              "format: version 15 -> 16, A01 CIRCUIT as revision 11 shipped it loads as today's (the amp's and LFOs' laws)");
        project_save(0);
        int n = store_splice(m2k_store_pack(), 0, cb, cn);
        store_buf[7] = 11;
        m2k_init(19);
        int ok = m2k_store_unpack(n) == 1;
        const uint8_t *b = img_get(&proj[0], 0, &len);
        CHECK(ok && !memcmp(&pat[0], &today, sizeof today) && len == pat_pack(&today, rb) && !memcmp(b, rb, (size_t)len),
              "store: revision 11's A01 CIRCUIT as it shipped becomes today's, in the work and in P1");
        m2k_init(20);
        static uint8_t k0[M2K_SOUNDS][P_COUNT];
        for (int i = 0; i < M2K_SOUNDS; i++) m_cpy(k0[i], snd[i].kit, P_COUNT);
        n = m2k_store_pack();
        int v5 = store_buf[4] == 5;
        m2k_init(21);
        ok = m2k_store_unpack(n) == 1;
        for (int i = 0; i < M2K_SOUNDS; i++) ok &= !memcmp(k0[i], snd[i].kit, P_COUNT);
        int conv = 1, moved = 0;
        for (int sv = 3; sv <= 4; sv++) {
            store_buf[4] = (uint8_t)sv;
            m2k_init(22);
            conv &= m2k_store_unpack(n) == 1;
            for (int i = 0; i < M2K_SOUNDS; i++) {
                uint8_t e[P_COUNT];
                m_cpy(e, k0[i], P_COUNT);
                if (sv < 4) feel_kit_v15(e);
                fx_kit_v16(e);
                conv &= !memcmp(e, snd[i].kit, P_COUNT);
                moved += memcmp(e, k0[i], P_COUNT) != 0;
            }
        }
        CHECK(v5 && ok && conv && moved > 0,
              "store: version 5; a version-4 store's sounds take format 17's sends, a version-3's 16's amp and LFO laws too");
    }
    {   /* OLD_PACKED's row for this revision is today's bank A packed: a bank changed is a new revision */
        int row = -1, same = 1;
        for (unsigned k = 0; k < sizeof OLD_PACKED / sizeof OLD_PACKED[0]; k++) if (OLD_PACKED[k].rev == FACTORY_REV) row = (int)k;
        m2k_init(17);
        for (int i = 0; i < 16; i++) {
            int n = pat_pack_img(&pat[i]);
            same &= row >= 0 && n == OLD_PACKED[row].len[i] && fnv(blob, (uint32_t)n) == OLD_PACKED[row].fnv[i];
        }
        if (!same) {
            printf("     OLD_PACKED wants: {%d, {", FACTORY_REV);
            for (int i = 0; i < 16; i++) printf("%d%s", pat_pack_img(&pat[i]), i < 15 ? ", " : "}, {");
            for (int i = 0; i < 16; i++) { int n = pat_pack_img(&pat[i]); printf("0x%08xu%s", fnv(blob, (uint32_t)n), i < 15 ? ", " : "}},\n"); }
        }
        CHECK(same, "store: OLD_PACKED holds today's bank A, packed, as this revision's");
    }
    {   /* the Patch Editor keeps a pattern's most free in a store for the FM-1 (web/editor.js BLOB_MAX) */
        FILE *f = fopen("web/editor.js", "rb");
        static char js[1 << 17];
        size_t jn = f ? fread(js, 1, sizeof js - 1, f) : 0;
        if (f) fclose(f);
        js[jn] = 0;
        char want[40];
        snprintf(want, sizeof want, "const BLOB_MAX = %d;", (int)BLOB_MAX);
        CHECK(!f || strstr(js, want), "store: the most a pattern packs to is the Patch Editor's BLOB_MAX");
    }
    /* a store that does not read whole leaves everything as it was: cut in the second project, an image's length one
     * more than its patterns', a pattern longer than any packs to, the sounds cut short */
    {
        static uint8_t good[STORE_MAX];
        m2k_init(18);
        s_cpy(pat[0].name, "MYWORK");
        for (int s = 0; s < 64; s++) pat[0].step[1][s].flags = ST_TRIG;
        project_save(1);
        s_cpy(pat[5].name, "UNSAVED");
        int n = m2k_store_pack(), whole = 1;
        memcpy(good, store_buf, (size_t)n);
        uint32_t w0 = 10 + S_COUNT;                                           /* the working image, then P1, P2 */
        uint32_t p1 = w0 + 12 + 2 * M2K_PATTERNS + (uint32_t)(good[w0 + 8] | good[w0 + 9] << 8 | good[w0 + 10] << 16);
        uint32_t p2 = p1 + 12 + 2 * M2K_PATTERNS + (uint32_t)(good[p1 + 8] | good[p1 + 9] << 8 | good[p1 + 10] << 16);
        int a4 = 0;                                                           /* P1's A01-A04, together */
        for (int i = 0; i < 4; i++) a4 += good[p1 + 12 + 2 * i] | good[p1 + 13 + 2 * i] << 8;
        for (int c = 0; c < 4; c++) {
            memcpy(store_buf, good, (size_t)n);
            int len = n;
            if (c == 0) len = (int)p2 + 40;                                   /* cut in P2 */
            if (c == 1) store_buf[w0 + 8]++;                                  /* the working image's length one more */
            if (c == 2) {                                                     /* P1's A01 longer than a pattern packs to */
                int l[4] = {BLOB_MAX + 1, 0, 0, a4 - BLOB_MAX - 1};
                for (int i = 0; i < 4; i++) { store_buf[p1 + 12 + 2 * i] = (uint8_t)l[i]; store_buf[p1 + 13 + 2 * i] = (uint8_t)(l[i] >> 8); }
            }
            if (c == 3) len = n - 100;                                        /* in the sounds */
            whole &= m2k_store_unpack(len) == 0 && !strcmp(pat[0].name, "MYWORK") && !strcmp(pat[5].name, "UNSAVED") && cur_proj == 1;
        }
        memcpy(store_buf, good, (size_t)n);
        CHECK(whole && m2k_store_unpack(n) == 1, "store: one cut short or damaged is refused whole, what was open stays");
    }
    /* a setting turned (GLO > MIDI's channel) is the store's: saved once it has stood 3 s, not once a detent; one set to
     * what it was is no change */
    {
        static int16_t sb[2 * 128];
        m2k_init(19);
        m2k_dirty();
        ui.page = PG_GLO; ui.sub = 4;
        int early = 0;
        for (int d = 0; d < 5; d++) {                 /* five detents, a second apart */
            edit(0, 1);
            for (int f = 0; f < M2K_SR; f += 128) m2k_render(sb, 128);
            early |= m2k_dirty();
        }
        for (int f = 0; f < 2 * M2K_SR + M2K_SR / 10; f += 128) m2k_render(sb, 128);
        int due = m2k_dirty(), once = !m2k_dirty(), v = store_.set[S_MCH];
        m2k_setting(S_MCH, v);
        for (int f = 0; f < 4 * M2K_SR; f += 128) m2k_render(sb, 128);
        int same = !m2k_dirty();
        int n = m2k_store_pack();
        m2k_init(20);
        CHECK(!early && due && once && same && v > 0 && m2k_store_unpack(n) == 1 && store_.set[S_MCH] == v,
              "settings: a turn saved 3 s after its last detent, once; set alike, nothing; it comes back with the store");
    }
    /* damaged stores (bytes flipped, cut short) and patterns of every layout (bytes flipped): each refused, or loaded
     * with its values in range, then played and drawn (under the sanitizers: nothing reaches past a table) */
    {
        static uint8_t good[STORE_MAX], bl[BLOB_MAX];
        static int16_t sb[2 * 128];
        static m2k_pattern_t src[16];
        m2k_init(21);
        for (int i = 16; i < 20; i++) for (int s = 0; s < 64; s += 3) { pat[i].step[i % 6][s].flags = ST_TRIG; pat[i].step[i % 6][s].note = 40; }
        project_save(0);
        int n = m2k_store_pack(), loaded = 0, ranged = 1;
        memcpy(good, store_buf, (size_t)n);
        for (int it = 0; it < 300; it++) {
            memcpy(store_buf, good, (size_t)n);
            int len = n, k = 1 + (int)(fuzz_rnd() % 8);
            for (int j = 0; j < k; j++) store_buf[fuzz_rnd() % (uint32_t)n] ^= (uint8_t)(1u << (fuzz_rnd() % 8));
            if (fuzz_rnd() % 4 == 0) len = (int)(fuzz_rnd() % (uint32_t)n);
            m2k_init(22);
            if (!m2k_store_unpack(len)) continue;
            loaded++;
            for (int i = 0; i < M2K_PATTERNS; i++) {
                for (int t = 0; t < M2K_TRACKS; t++) for (int q = 0; q < P_COUNT; q++) ranged &= pat[i].kit[t][q] <= 127;
                ranged &= pat[i].glob[G_LEN] >= 1 && pat[i].glob[G_LEN] <= 64 && pat[i].glob[G_SPD1] <= 127 && pat[i].glob[G_GLCH] <= 127;
            }
            cur_pat = (int)(fuzz_rnd() % 20);
            work_reset_current();
            seq_play(1);
            for (int f = 0; f < 128 * 6; f += 128) m2k_render(sb, 128);
            m2k_frame(500 + 40u * (uint32_t)it);
            seq_play(0);
        }
        m2k_init(23);
        factory_rev9();                                   /* real parts on every old machine */
        memcpy(src, pat, sizeof src);
        int unpacked = 0;
        for (int it = 0; it < 650; it++) {
            int ver = 3 + it % 13, bn = pat_pack_v(&src[it % 16], bl, ver), k = 1 + (int)(fuzz_rnd() % 6);
            for (int j = 0; j < k; j++) bl[fuzz_rnd() % (uint32_t)bn] ^= (uint8_t)(1u << (fuzz_rnd() % 8));
            m2k_init(24);                                 /* (each as it would boot: no track's last machine left) */
            if (!pat_unpack(&pat[0], bl, bn)) continue;
            unpacked++;
            for (int t = 0; t < M2K_TRACKS; t++) for (int q = 0; q < P_COUNT; q++) ranged &= pat[0].kit[t][q] <= 127;
            for (int i = 0; i < M2K_LOCKS; i++) ranged &= pat[0].lock[i].val <= 127;
            cur_pat = 0;
            work_reset_current();
            seq_play(1);
            for (int f = 0; f < 128 * 4; f += 128) m2k_render(sb, 128);
            m2k_frame(9000 + 40u * (uint32_t)it);
            seq_play(0);
        }
        printf("     damaged: %d of 300 stores loaded, %d of 650 patterns\n", loaded, unpacked);
        CHECK(loaded > 30 && unpacked > 300 && ranged, "damaged stores and patterns: refused or in range, played and drawn");
    }
    /* GLO > MIDI: the clock out (24 a beat at the tempo, start first, stop last), the clock in (a step every sixth
     * pulse, the tempo from them), the notes out, the channel */
    {
        static int16_t mb[2 * 128];
        uint8_t m[3];
        m2k_init(13);
        open_b01();
        m2k_pattern_t *p = &pat[16];
        p->glob[G_TMPO] = 120 - 30;
        p->glob[G_SWNG] = 0;
        p->step[0][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
        p->step[0][2] = (m2k_step_t){.note = 62, .flags = ST_TRIG};
        m2k_setting(S_SYNO, 127);
        m2k_setting(S_NOTO, 127);
        seq_play(1);
        int pulses = 0, start = 0, ons = 0, offs = 0, first = -1;
        for (int s = 0; s < M2K_SR / 2; s += 128) {         /* half a second at 120 BPM: a beat */
            m2k_render(mb, 128);
            int k;
            while ((k = m2k_midi_out(m, m2k_samples())) > 0) {
                if (first < 0) first = m[0];
                pulses += m[0] == 0xF8; start += m[0] == 0xFA;
                ons += k == 3 && m[0] == 0x90 && m[2]; offs += k == 3 && m[0] == 0x80;
            }
        }
        seq_play(0);
        int stop = 0, k;
        while ((k = m2k_midi_out(m, m2k_samples())) > 0) stop += m[0] == 0xFC;
        CHECK(first == 0xFA && start == 1 && pulses >= 23 && pulses <= 25 && stop == 1,
              "MIDI: the clock out, 24 pulses a beat at the tempo, start first and stop at the end");
        CHECK(ons == 2 && offs >= 1, "MIDI: the notes out, each track's trigs on its channel, and their ends");
        /* the clock in at 100 BPM: 1102.5 samples a pulse, delivered between blocks */
        m2k_init(14);
        open_b01();
        m2k_setting(S_SYNI, 127);
        m2k_midi(0xFA, 0, 0);
        double next = 0;
        int sent = 0;
        for (int s = 0; sent < 6 * 9; s += 16) {
            while (s >= next && sent < 6 * 9) { m2k_midi(0xF8, 0, 0); sent++; next += 1102.5; }
            m2k_render(mb, 16);
        }
        for (int i = 0; i < 8; i++) m2k_render(mb, 16);
        CHECK(sq.playing && sq.step == 9 % seq_len() && seq_bpm() >= 99 && seq_bpm() <= 101,
              "MIDI: the clock in, a step every sixth pulse, its tempo the sequencer's");
        m2k_midi(0xFC, 0, 0);
        CHECK(!sq.playing, "MIDI: stop from the clock in");
        /* SYNC IN off: the transport still drives it, continue too (a DAW starting mid-song sends continue) */
        m2k_setting(S_SYNI, 0);
        m2k_midi(0xFB, 0, 0);
        int cont_on = sq.playing;
        m2k_midi(0xFC, 0, 0);
        m2k_midi(0xFA, 0, 0);
        int start_on = sq.playing;
        m2k_midi(0xFC, 0, 0);
        CHECK(cont_on && start_on && !sq.playing, "MIDI: SYNC IN off, start and continue play, stop stops");
        /* the channel: track 1 on channel 3, so 0x92 plays track 1 and 0x90 the track in view */
        m2k_setting(S_MCH, 2 * 8 + 4);
        ui.trk = 4; sq.ktrk = 4;
        m2k_midi(0x92, 64, 100);
        int t0 = trk[0].note == 64 && trk[0].gate;
        m2k_midi(0x82, 64, 0);
        CHECK(t0, "MIDI: CHAN puts track 1 on its channel");
        m2k_setting(S_MCH, 0); m2k_setting(S_SYNI, 0);
    }
    /* MIDI in by where it comes from (the FM-1's USB or TRS jack): the clock and transport only from SYNC IN's; with
     * SYNC IN off the transport from NOTE IN's; notes, CCs and mutes only from NOTE IN's; THRU passes the jack's on over
     * USB (its clock not while SYNC OUT sends MONO2K's), never USB's */
    {
        static int16_t mb[2 * 128];
        static const uint8_t OLDV[4] = {0, 32, 96, 127}, OLDC[4] = {0, 0, 3, 3}, NTC[4] = {3, 1, 2, 0};
        static const char *const SRC_N[4] = {"OFF", "USB", "TRS", "BOTH"};
        const int U = M2K_USB, T = M2K_TRS;
        uint8_t m[3];
        char o[16];
        const char *u;
        m2k_init(15);
        open_b01();
        /* an old store: SYNC IN's switch OFF (0 at first, 32 turned) or ON (96, the page's 127); NOTE IN and THRU 0 */
        int old = notes_from() == 3 && !set_on(S_THRU);
        for (int i = 0; i < 4; i++) { store_.set[S_SYNI] = OLDV[i]; old &= clk_from() == OLDC[i]; }
        /* SYIN turned on the panel from an old OFF: USB, TRS, BOTH, and no further; from an old ON down: TRS */
        ui.page = PG_GLO; ui.sub = 4;
        store_.set[S_SYNI] = 32;
        int turn = knob_param(PG_GLO, 4, 1) == SETTING(S_SYNI);
        for (int i = 1; i <= 4; i++) {
            edit(1, 1);
            fmt(pinfo(0, SETTING(S_SYNI)), 0, store_.set[S_SYNI], o, &u);
            turn &= clk_from() == (i < 3 ? i : 3) && !strcmp(o, SRC_N[i < 3 ? i : 3]);
        }
        store_.set[S_SYNI] = 96;
        edit(1, -1);
        turn &= clk_from() == 2;
        /* GLO > IN: NOTE IN (BOTH USB TRS OFF) and THRU */
        int page = knob_param(PG_GLO, 5, 0) == SETTING(S_NTIN) && knob_param(PG_GLO, 5, 1) == SETTING(S_THRU) &&
                   knob_param(PG_GLO, 5, 2) == NONE && knob_param(PG_GLO, 5, 3) == NONE;
        for (int i = 0; i < 4; i++) {
            store_.set[S_NTIN] = enc(i, 4);
            fmt(pinfo(0, SETTING(S_NTIN)), 0, store_.set[S_NTIN], o, &u);
            page &= notes_from() == NTC[i] && !strcmp(o, SRC_N[NTC[i]]);
        }
        CHECK(old && turn && page, "MIDI in: an old store's SYNC IN off stays OFF, on takes USB and TRS (BOTH), NOTE IN "
                                   "BOTH, THRU off; SYIN turns OFF USB TRS BOTH; GLO > IN's NOTE IN and THRU");
        /* SYNC IN USB: the jack's song position, start, pulses and stop pass it by; TRS: the other way; BOTH: either */
        m2k_setting(S_NTIN, enc(0, 4));
        m2k_setting(S_SYNI, enc(1, 4));
        m2k_midi_from(T, 0xF2, 5, 0);
        m2k_midi_from(T, 0xFA, 0, 0);
        int usb = !sq.playing && sq.step == 0;
        m2k_midi_from(U, 0xF2, 5, 0);
        usb &= sq.step == 5;
        m2k_midi_from(U, 0xFA, 0, 0);
        usb &= sq.playing && mx.ext;
        m2k_midi_from(T, 0xF8, 0, 0);
        usb &= mx.pulses == 0;
        m2k_midi_from(U, 0xF8, 0, 0);
        usb &= mx.pulses == 1;
        m2k_midi_from(T, 0xFC, 0, 0);
        usb &= sq.playing;
        m2k_midi_from(U, 0xFC, 0, 0);
        usb &= !sq.playing;
        m2k_setting(S_SYNI, enc(2, 4));
        m2k_midi_from(U, 0xFA, 0, 0);
        int trs = !sq.playing;
        m2k_midi_from(T, 0xFA, 0, 0);
        trs &= sq.playing && mx.ext;
        m2k_midi_from(U, 0xF8, 0, 0);
        trs &= mx.pulses == 0;
        m2k_midi_from(T, 0xF8, 0, 0);
        trs &= mx.pulses == 1;
        m2k_midi_from(U, 0xFC, 0, 0);
        trs &= sq.playing;
        m2k_midi_from(T, 0xFC, 0, 0);
        trs &= !sq.playing;
        m2k_setting(S_SYNI, enc(3, 4));
        m2k_midi_from(T, 0xFA, 0, 0);
        int both = sq.playing && mx.ext;
        m2k_midi_from(U, 0xFC, 0, 0);
        both &= !sq.playing;
        m2k_midi_from(U, 0xFB, 0, 0);
        both &= sq.playing && mx.ext;
        m2k_midi_from(T, 0xFC, 0, 0);
        both &= !sq.playing;
        CHECK(usb && trs && both, "MIDI in: the clock, start, continue, stop and song position only from SYNC IN's (USB, TRS, both)");
        /* SYNC IN off: start, continue and stop from NOTE IN's, at MONO2K's tempo */
        m2k_setting(S_SYNI, enc(0, 4));
        m2k_setting(S_NTIN, enc(1, 4));
        m2k_midi_from(T, 0xFA, 0, 0);
        int off = !sq.playing;
        m2k_midi_from(U, 0xFA, 0, 0);
        off &= sq.playing && !mx.ext;
        m2k_midi_from(T, 0xFC, 0, 0);
        off &= sq.playing;
        m2k_midi_from(U, 0xFC, 0, 0);
        off &= !sq.playing;
        m2k_setting(S_NTIN, enc(2, 4));
        m2k_midi_from(U, 0xFB, 0, 0);
        off &= !sq.playing;
        m2k_midi_from(T, 0xFB, 0, 0);
        off &= sq.playing;
        m2k_midi_from(T, 0xFC, 0, 0);
        off &= !sq.playing;
        m2k_setting(S_NTIN, enc(3, 4));
        m2k_midi_from(U, 0xFA, 0, 0);
        m2k_midi_from(T, 0xFA, 0, 0);
        off &= !sq.playing;
        CHECK(off, "MIDI in: SYNC IN off, start, continue and stop only from NOTE IN's");
        /* notes, CCs (SYN A on 48) and mutes (3) only from NOTE IN's: both, USB, TRS, none */
        m2k_setting(S_MCH, 0);
        ui.trk = 4; sq.ktrk = 4;
        int notes = 1;
        for (int c = 0; c < 4; c++) {
            m2k_setting(S_NTIN, enc(c, 4));
            for (int s = 0; s < 2; s++) {
                int src = s ? T : U, want = NTC[c] >> s & 1;
                m2k_midi_from(src, 0x90, 64, 100);
                int on = trk[0].note == 64 && trk[0].gate;
                voice_off(0);
                PAT(cur_pat)->kit[0][P_SYN] = 0;
                m2k_midi_from(src, 0xB0, 48, 99);
                int cc = PAT(cur_pat)->kit[0][P_SYN] == 99;
                PAT(cur_pat)->mute = 0;
                m2k_midi_from(src, 0xB0, 3, 127);
                int mute = PAT(cur_pat)->mute & 1;
                notes &= on == want && cc == want && mute == want;
            }
        }
        PAT(cur_pat)->mute = 0;
        CHECK(notes, "MIDI in: notes, CCs and mutes only from NOTE IN's (both, USB, TRS, none)");
        /* THRU: the jack's out over USB as they came, stamped with the sample they came in at, as the core's own are;
         * USB's never back; with SYNC OUT on, one clock: following the jack, MONO2K passes its start, pulses and stop
         * on once; following USB, the jack's are not passed */
        m2k_setting(S_NTIN, enc(0, 4));
        m2k_setting(S_SYNI, enc(0, 4));
        m2k_setting(S_SYNO, 0);
        while (m2k_midi_out(m, m2k_samples()) > 0) {}
        m2k_midi_from(T, 0x93, 60, 100);
        int thru = m2k_midi_out(m, m2k_samples()) == 0;
        m2k_midi_from(T, 0x83, 60, 0);
        m2k_setting(S_THRU, 127);
        m2k_render(mb, 128);
        m2k_midi_from(T, 0x93, 60, 100);
        thru &= m2k_midi_out(m, m2k_samples() - 1) == 0;
        thru &= m2k_midi_out(m, m2k_samples()) == 3 && m[0] == 0x93 && m[1] == 60 && m[2] == 100;
        m2k_midi_from(T, 0x83, 60, 0);
        thru &= m2k_midi_out(m, m2k_samples()) == 3 && m[0] == 0x83 && m[1] == 60 && m[2] == 0;
        m2k_midi_from(T, 0xC5, 7, 0);
        thru &= m2k_midi_out(m, m2k_samples()) == 2 && m[0] == 0xC5 && m[1] == 7;
        m2k_midi_from(T, 0xF2, 3, 1);
        thru &= m2k_midi_out(m, m2k_samples()) == 3 && m[0] == 0xF2 && m[1] == 3 && m[2] == 1;
        m2k_midi_from(T, 0xF8, 0, 0);
        thru &= m2k_midi_out(m, m2k_samples()) == 1 && m[0] == 0xF8;
        m2k_midi_from(U, 0x93, 62, 100);
        m2k_midi_from(U, 0x83, 62, 0);
        m2k_midi_from(U, 0xF8, 0, 0);
        thru &= m2k_midi_out(m, m2k_samples()) == 0;
        m2k_setting(S_SYNO, 127);
        m2k_setting(S_SYNI, enc(2, 4));
        m2k_midi_from(T, 0xFA, 0, 0);
        for (int i = 0; i < 3; i++) m2k_midi_from(T, 0xF8, 0, 0);
        m2k_midi_from(T, 0xFC, 0, 0);
        int fa = 0, f8 = 0, fc = 0, k;
        while ((k = m2k_midi_out(m, m2k_samples())) > 0) { fa += m[0] == 0xFA; f8 += m[0] == 0xF8; fc += m[0] == 0xFC; }
        int one = fa == 1 && f8 == 3 && fc == 1;
        m2k_setting(S_SYNI, enc(1, 4));
        m2k_midi_from(T, 0xFA, 0, 0);
        m2k_midi_from(T, 0xF8, 0, 0);
        one &= !sq.playing && m2k_midi_out(m, m2k_samples()) == 0;
        CHECK(thru && one, "MIDI in: THRU passes the TRS jack's notes, programs, position and clock on over USB as they "
                           "came, stamped as the core's own, never USB's; with SYNC OUT on, one clock");
        CHECK(mx.in_seen == 3 && mx.in_at[1] == m2k_samples(), "MIDI in: each port's last message kept, for GLO > IN");
        /* NOTE IN and THRU kept with the store */
        m2k_setting(S_NTIN, enc(2, 4));
        int n = m2k_store_pack();
        m2k_init(16);
        CHECK(m2k_store_unpack(n) == 1 && notes_from() == 2 && set_on(S_THRU) && clk_from() == 1,
              "MIDI in: SYNC IN's source, NOTE IN and THRU come back with the store");
    }
    /* the store, version 3: the sound library with it; a store for the FM-1 (16 patterns, one project) read here (64,
     * four): bank A as it was, the rest blank, the sounds kept; a project file of version 1 still imported */
    {
        m2k_init(31);
        CHECK(snd[0].name[0] && snd[0].machine == pat[0].machine[0] && !memcmp(snd[0].kit, pat[0].kit[0], P_COUNT),
              "sounds: the factory's library holds bank A's tracks, A01 T1 first");
        sound_save(63, &pat[2], 4, 0, -1);
        s_cpy(snd[63].name, "MINE");
        int n = m2k_store_pack();
        m2k_init(32);
        CHECK(m2k_store_unpack(n) == 1 && !strcmp(snd[63].name, "MINE") && snd[63].machine == pat[2].machine[4],
              "sounds: the library goes out and back with the store");
        pat[5].kit[1][P_PAN] = 21;
        pat[20].kit[1][P_PAN] = 22;
        n = m2k_store_pack_as(16, 1);
        m2k_init(33);
        int ok = m2k_store_unpack(n) == 1;
        m2k_pattern_t blank;
        pattern_init_blank(&blank);
        CHECK(ok && pat[5].kit[1][P_PAN] == 21 && !strcmp(pat[20].name, blank.name) && pat[20].kit[1][P_PAN] != 22 &&
              !strcmp(m2k_project_name(0), "DEMO") && !strcmp(m2k_project_name(2), "P3") && !strcmp(snd[63].name, "MINE"),
              "store: the FM-1's (16 patterns, one project) read on 64 and four: bank A kept, the rest blank, the sounds kept");
        int e = m2k_proj_export(0);
        memmove(store_buf + 5, store_buf + 6, (size_t)e - 6);   /* version 1: no count, 64 patterns */
        store_buf[4] = 1;
        CHECK(m2k_proj_import(3, e - 1) == 1 && !strcmp(m2k_project_name(3), "DEMO"), "project file: version 1 still imported");
        m2k_init(35);
        s_cpy(proj[2].name, "THIRD");
        m2k_project(2, 0);
        n = m2k_store_pack_as(16, 1);
        CHECK(n > 0 && m2k_store_unpack(n) == 1 && !strcmp(m2k_project_name(0), "THIRD") && m2k_project_current() == 0,
              "store for the FM-1: the open project goes, whichever it is");
    }
    /* SYN > SND: 128 slots, the demos' 24 sounds then 104 INIT; SAVE turned once keeps the track's sound in an INIT slot
     * (twice over a sound), LOAD once puts it on a track, an INIT slot loaded starts the machine afresh; CAT shows one
     * category (and the INIT slots), SLOT steps through those */
    {
        m2k_init(34);
        open_b01();
        int cats = 1;
        for (int i = 0; i < 24; i++) if (!snd[i].name[0] || !snd_cat[i]) cats = 0;
        CHECK(cats && !snd[24].name[0] && !snd[127].name[0] && !snd_cat[24] && !snd_cat[100],
              "sounds: 128 slots, the demos' 24 sounds (each with a category) and 104 INIT");
        ui.trk = 2;
        ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1);
        ui.snd_sel = 100;
        ui.now = 5000;                                            /* the screen's clock, for the second turn */
        int mach = pat[16].machine[2] = M_SID;
        pat[16].kit[2][P_DEC] = 77;
        edit(2, 1);                                               /* SAVE, into INIT: at once */
        int saved = snd[100].machine == mach && snd[100].kit[P_DEC] == 77 && snd[100].name[0] && snd_cat[100];
        pat[16].kit[2][P_DEC] = 30;
        ui.now = 9000;
        edit(2, 1);                                               /* over a sound: asked */
        int kept = snd[100].kit[P_DEC] == 77 && ask.kind == ASK_SND_OVER;
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);             /* YES */
        int replaced = snd[100].kit[P_DEC] == 30 && !ask.kind;
        ui.trk = 4;
        edit(1, 1);                                               /* LOAD, once */
        int loaded = pat[16].machine[4] == mach && pat[16].kit[4][P_DEC] == 30 && trk[4].eff[P_DEC] == 30;
        ui.snd_sel = 120;
        pat[16].kit[4][P_DEC] = 99;
        edit(1, 1);                                               /* an INIT slot: the machine afresh */
        m2k_pattern_t fresh;
        pat_init(&fresh, "X");
        kit_init(&fresh, 4, mach);
        int init = pat[16].machine[4] == mach && !memcmp(pat[16].kit[4], fresh.kit[4], P_COUNT);
        CHECK(saved && kept && replaced && loaded && init,
              "sounds: SAVE into INIT at once, over a sound when OCT+ says YES, LOAD once, INIT afresh (its machine's INIT)");
        ui.snd_sel = 0;
        int c0 = snd_cat[0];
        edit(3, 1);                                               /* CAT: KICK */
        int shown_ok = 1, n = 0;
        snd_list(ui.snd_sel, &n);
        for (int k = 0; k < n; k++) { int i = snd_nth(k); if (snd[i].name[0] && snd_cat[i] != ui.snd_cat) shown_ok = 0; }
        for (int k = 0; k < 40; k++) edit(0, 1);                  /* SLOT through them */
        CHECK(ui.snd_cat == CAT_KICK && shown_ok && n >= 32 && snd_shown(ui.snd_sel) && (c0 || 1),
              "sounds: CAT shows one category and the INIT slots; SLOT steps through those alone");
    }
    /* the machines' INITs (MACH_INIT): each its own sound on its own chain, kit_default's left as it was; their home
     * notes; rendered at those, no two alike (GND-PERC's INIT and GND-NOIS's were one white noise) */
    {
        m2k_pattern_t a, b;
        pat_init(&a, "X"); pat_init(&b, "X");
        int own = 1, apart = 1;
        for (int m = 0; m < M_COUNT; m++) {
            kit_init(&a, 0, m); kit_default(&b, 0, m);
            if (a.machine[0] != m || !memcmp(&a.kit[0][P_ATK], &b.kit[0][P_ATK], P_COUNT - P_ATK)) own = 0;
            for (int n = m + 1; n < M_COUNT; n++) {
                kit_init(&b, 0, n);
                if (!memcmp(&a.kit[0][P_ATK], &b.kit[0][P_ATK], P_COUNT - P_ATK)) apart = 0;
            }
        }
        int homes = mach_home(M_DKIK) == 36 && mach_home(M_GSIN) == 48 && mach_home(M_FMD) == 48 && mach_home(M_SAW) == 60 &&
                    mach_home(M_DSNR) == 60;
        for (int m = 0; m < M_COUNT; m++) homes &= mach_home(m) >= 36 && mach_home(m) <= 60;
        CHECK(own && apart && homes, "INIT: every machine's on its own chain (none kit_default's, no two alike); the kick's home C2, the basses' C3");
        /* a sound's shape, its level aside: its zero crossings and its slope's (pitch or noise, brightness), and its
         * first 100 ms against the rest (dB: a hit or a held note); 20 ln of a ratio, so 5 % a unit */
        static int16_t ib[2 * M2K_SR + 512];
        double fz[M_COUNT], fs[M_COUNT], fd[M_COUNT];
        int alike = 0, ca = -1, cb = -1;
        double close = 1e9;
        for (int m = 0; m < M_COUNT; m++) {
            if (m == M_FXC || m == M_FXR || m == M_FXD) continue;   /* (silent without a track before them) */
            m2k_init(2000);
            pat_init(&pat[47], "INI");
            kit_init(&pat[47], 0, m);
            pat[47].glob[G_DRIV] = 0;
            pat[47].step[0][0] = (m2k_step_t){.note = (uint8_t)mach_home(m), .flags = ST_TRIG};
            double pk, rm;
            fx_measure(47, 1, 1.0, ib, &pk, &rm);
            double e0 = 0, e1 = 0;
            int zc = 0, zs = 0;
            for (int i = 2; i < M2K_SR; i++) {
                double v = ib[2 * i];
                if (i < M2K_SR / 10) e0 += v * v; else e1 += v * v;
                zc += (ib[2 * i] < 0) != (ib[2 * i - 2] < 0);
                zs += (ib[2 * i] < ib[2 * i - 2]) != (ib[2 * i - 2] < ib[2 * i - 4]);
            }
            fz[m] = zc + 1; fs[m] = zs + 1; fd[m] = 10 * log10((e0 + 1) / (e1 + 1));
            for (int n = 0; n < m; n++) {
                if (n == M_FXC || n == M_FXR || n == M_FXD) continue;
                double d = 20 * fabs(log(fz[m] / fz[n])) + 20 * fabs(log(fs[m] / fs[n])) + fabs(fd[m] - fd[n]);
                if (d < close) { close = d; ca = n; cb = m; }
                alike += d < 1.0;
            }
        }
        printf("     INITs at their homes: the closest two %s and %s, %.1f apart (crossings, the slope's, decay)\n", MI[ca].name, MI[cb].name, close);
        CHECK(!alike, "INIT: the sixteen machines with a sound of their own sound apart at their home notes");
    }
    /* ALGORITHM on EDIT: a sound still its machine's INIT (or kit_default's, the INIT before) takes the next machine's
     * whole INIT, an FX its amp (HOLD 127) with it; a sound shaped keeps its chain and takes the page alone. The
     * selector's order: the ten tonal machines first, then the drums, the noises, the FX */
    {
        static const uint8_t ORD[M_COUNT] = {M_GSIN, M_SAW, M_PUL, M_ENS, M_FMS, M_FMP, M_FMD, M_VOX, M_WAV, M_SID,
                                             M_DKIK, M_DSNR, M_DHAT, M_DCYM, M_GPRC, M_GNOI, M_FXC, M_FXD, M_FXR};
        int order = !memcmp(M_ORDER, ORD, M_COUNT), once = 1;
        for (int m = 0; m < M_COUNT; m++) once &= M_ORDER[m_place(m)] == m;
        CHECK(order && once, "ALGORITHM's order: tonal first (GND-SIN, the SWAV and FM+ families, VOX-6, DIGI-WAV, SID), drums, noises, FX");
        CHECK(!strcmp(MI[M_SAW].badge, "SWV") && !strcmp(MI[M_PUL].badge, "SWV") && !strcmp(MI[M_ENS].badge, "SWV"),
              "the SWAV family's badge: SWV, SWAV-PUL's too");
        m2k_init(50);
        m2k_pattern(16);                                          /* B01, the INIT pattern: T1 GND-SIN at its INIT */
        ui.trk = 0; ui.page = PG_SYN; ui.sub = 0;
        m2k_pattern_t w;
        pat_init(&w, "X");
        m2k_encoder(E_ALGO, 1);                                   /* -> SWAV-SAW */
        kit_init(&w, 0, M_SAW);
        int whole = pat[16].machine[0] == M_SAW && !memcmp(pat[16].kit[0], w.kit[0], P_COUNT) && !memcmp(trk[0].eff, w.kit[0], P_COUNT);
        pat[16].kit[0][P_DEC] = 33; pat[16].kit[0][P_WDTH] = 77; pat[16].kit[0][P_SYN + 1] = 5;   /* shaped */
        uint8_t shaped[P_COUNT];
        m_cpy(shaped, pat[16].kit[0], P_COUNT);
        m2k_encoder(E_ALGO, 1);                                   /* -> SWAV-PUL: its page, the chain kept */
        kit_init(&w, 0, M_PUL);
        int page = pat[16].machine[0] == M_PUL && !memcmp(pat[16].kit[0], w.kit[0], 8) && !memcmp(pat[16].kit[0] + 8, shaped + 8, P_COUNT - 8);
        kit_default(&pat[16], 0, M_PUL);                          /* the INIT before these, as an old pattern */
        feel_kit_v15(pat[16].kit[0]);                             /* loads it: on format 16's laws */
        m2k_encoder(E_ALGO, 1);                                   /* -> SWAV-ENS, whole */
        kit_init(&w, 0, M_ENS);
        int old = pat[16].machine[0] == M_ENS && !memcmp(pat[16].kit[0], w.kit[0], P_COUNT);
        kit_init(&pat[16], 0, M_GSIN);
        m2k_encoder(E_ALGO, -1);                                  /* GND-SIN back to the end: FX-RING, whole */
        kit_init(&w, 0, M_FXR);
        int fx = pat[16].machine[0] == M_FXR && !memcmp(pat[16].kit[0], w.kit[0], P_COUNT) && pat[16].kit[0][P_HOLD] == 127 &&
                 pat[16].kit[0][P_DEC] == 127;
        CHECK(whole && page && old && fx, "ALGORITHM: an INIT sound takes the next machine's whole INIT (an FX its amp); a shaped one its page, the chain kept");
    }
    /* the notes that fit the sound: a machine picked or INITed puts its track's new trigs, pads and keys on its home
     * note, each track's keyboard octave its own (OCT- / OCT+ the track in view's); an INIT pattern opened, every
     * track at home: DRM-KICK at C2 from the keys' first C, not the zap of C4 */
    {
        m2k_init(51);
        m2k_pattern(16);                                          /* B01 INIT: GND-SIN DRM-KICK DRM-SNAR DRM-HATS FM+STAT SWAV-ENS */
        int blank = ui.note[0] == 48 && ui.oct[0] == -1 && ui.note[1] == 36 && ui.oct[1] == -2 && ui.note[2] == 60 &&
                    ui.oct[2] == 0 && ui.note[4] == 60 && ui.oct[4] == 0;
        m2k_track(1);
        m2k_key(7, 1);                                            /* the keys' first C */
        int kick = trk[1].note == 36;
        m2k_key(7, 0);
        ui.grid = 1;
        m2k_key(ux_white(0), 1); m2k_key(ux_white(0), 0);         /* a trig on step 1 */
        int trig = (pat[16].step[1][0].flags & ST_TRIG) && pat[16].step[1][0].note == 36;
        ui.grid = 0;
        m2k_track(0);
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);             /* T1's octave up, T2's kept */
        const uint8_t *L = m2k_leds();                            /* OCT- lit for an octave down: the track in view's */
        int own = ui.oct[0] == 0 && ui.oct[1] == -2 && !L[M2K_KEYS + B_OCTD] && !L[M2K_KEYS + B_OCTU];
        m2k_track(1);
        L = m2k_leds();
        own &= L[M2K_KEYS + B_OCTD] && !L[M2K_KEYS + B_OCTU];
        m2k_key(7, 1);
        own &= trk[1].note == 36;
        m2k_key(7, 0);
        m2k_track(4);
        ui.page = PG_SYN; ui.sub = 0;
        set_machine(M_DKIK);                                      /* FM+STAT's track a kick: at C2 */
        int picked = ui.note[4] == 36 && ui.oct[4] == -2;
        set_machine(M_GSIN);                                      /* and on to a sine: at C3 */
        int bass = ui.note[4] == 48 && ui.oct[4] == -1;
        m2k_button(B_GLO, 1); m2k_button(B_ARP, 1); m2k_button(B_ARP, 0); m2k_button(B_GLO, 0);   /* PADS */
        m2k_key(ux_white(1), 1); m2k_key(ux_white(1), 0);         /* T2's pad: its trig's note */
        m2k_key(ux_white(4), 1); m2k_key(ux_white(4), 0);         /* T5's: no trigs, its home */
        int pads = trk[1].note == 36 && trk[4].note == 48;
        CHECK(blank && kick && trig && own && picked && bass && pads,
              "notes: an INIT pattern's tracks at home (the kick's keys C2, a trig there); octaves per track; a machine picked, its home");
    }
    /* the library's notes: each sound kept with the note it was made at (the demos' roots; SAVE the track's root) and
     * loaded there, its track's keys and new trigs with it (a kick saved at C2 plays at C2); kept in the store's 'R';
     * a store before 'R' guesses by category (a kick at C2, a bass at C3, the rest C4); one cut inside it is refused */
    {
        m2k_init(52);
        int fac = 1;
        for (int i = 0; i < 6 * FACTORY_DEMOS; i++) fac &= snd_root[i] == pat_root(&pat[i / 6], i % 6);
        fac &= pat[0].machine[0] == M_DKIK && snd_root[0] == 33;  /* A01's kick on A1 */
        m2k_pattern(16);
        for (int s = 0; s < 16; s += 4) pat[16].step[1][s] = (m2k_step_t){.note = 36, .flags = ST_TRIG};
        m2k_track(1);
        ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1); ui.snd_cat = 0; ui.snd_sel = 100; ui.now = 5000;
        edit(2, 1);                                               /* SAVE: the kick at C2 */
        int saved = snd[100].machine == M_DKIK && snd_root[100] == 36 && snd_cat[100] == CAT_KICK;
        m2k_track(4);
        ui.now = 9000;
        edit(1, 1);                                               /* LOAD onto T5 */
        int loaded = pat[16].machine[4] == M_DKIK && ui.note[4] == 36 && ui.oct[4] == -2;
        m2k_key(7, 1);
        loaded &= trk[4].note == 36;
        m2k_key(7, 0);
        snd_root[101] = 77; s_cpy(snd[101].name, "HIGH"); snd[101].machine = M_SAW; snd_cat[101] = CAT_LEAD;
        int n = m2k_store_pack();
        m2k_init(53);
        int kept = m2k_store_unpack(n) == 1 && snd_root[100] == 36 && snd_root[101] == 77;
        int tag = n > 130 && store_buf[n - 130] == 'R' && store_buf[n - 129] == M2K_SOUNDS;
        m2k_init(54);
        int old = tag && m2k_store_unpack(n - 130) == 1 && snd_root[100] == 36 && snd_root[101] == 60 && !strcmp(snd[101].name, "HIGH");
        for (int i = 0; i < M2K_SOUNDS; i++) if (snd[i].name[0]) old &= snd_root[i] == root_guess(snd_cat[i]);
        old &= snd_root[3] == 48 && snd_cat[3] == CAT_BASS;       /* A01's sine bass */
        m2k_init(55);
        s_cpy(pat[0].name, "MINE");
        int cut = !m2k_store_unpack(n - 10) && !strcmp(pat[0].name, "MINE");
        CHECK(fac && saved && loaded && kept && old && cut,
              "sounds: each kept with its note (the demos' roots, SAVE's), loaded there (a kick at C2); 'R' in the store, older ones guessed");
    }
    /* the verbs: GLO + SEQ clears the pattern's steps (the sounds stay), again within 3 s undoes it, held a second
     * makes it INIT; GLO + OCT- / OCT+ undo and redo; in GRID they act on the track; GLO + EDIT opens the library */
    {
        m2k_init(36);
        m2k_pattern_t orig = pat[0], blank;
        pattern_init_blank(&blank);
        ui.page = PG_HOME;
        ui.now = 1000;
        m2k_button(B_GLO, 1); m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int cleared = 1;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) cleared &= !(pat[0].step[t][st].flags & ST_TRIG);
        int sounds = !strcmp(pat[0].name, "CIRCUIT") && pat[0].machine[2] == M_DHAT;
        ui.now = 2000;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);                  /* again: undone */
        int undone = !memcmp(&pat[0], &orig, sizeof orig);
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);                /* REDO: cleared again */
        int redone = !(pat[0].step[0][0].flags & ST_TRIG) && !strcmp(pat[0].name, "CIRCUIT");
        m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0);                /* UNDO */
        int undo2 = !memcmp(&pat[0], &orig, sizeof orig);
        ui.now = 9000;
        m2k_button(B_SEQ, 1);                                        /* held: the ring, then INIT */
        ui.now = 9600; ring_tick();
        int half = !strcmp(pat[0].name, "CIRCUIT");
        ui.now = 10100; ring_tick();
        m2k_button(B_SEQ, 0);
        int init = !memcmp(&pat[0], &blank, sizeof blank);
        m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0);
        int back = !memcmp(&pat[0], &orig, sizeof orig);
        ui.grid = 1; ui.trk = 3;                                     /* in GRID: the track */
        ui.now = 20000;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int trk_only = !(pat[0].step[3][0].flags & ST_TRIG) && memcmp(pat[0].step[0], orig.step[0], sizeof orig.step[0]) == 0;
        ui.grid = 0;
        m2k_button(B_EDIT, 1); m2k_button(B_EDIT, 0); m2k_button(B_GLO, 0);
        int lib = snd_page();
        CHECK(cleared && sounds && undone && redone && undo2 && half && init && back && trk_only && lib,
              "GLO + SEQ: clear (sounds stay), again: undo, held: INIT; GLO + OCT-/+ undo, redo; in GRID the track; GLO + EDIT: SND");
        m2k_init(37);
        m2k_button(B_GLO, 1); m2k_button(B_ARP, 1); m2k_button(B_ARP, 0); m2k_button(B_GLO, 0);
        int pads = ui.pads && !ui.grid;
        uint32_t t0 = mon.trig[0], t2 = mon.trig[2];
        static int16_t b16[256];
        m2k_key(0, 1); m2k_key(0, 0);                 /* F3: white 1, T1 */
        m2k_key(4, 1); m2k_key(4, 0);                 /* A3: white 3, T3 */
        m2k_render(b16, 128);
        int hit = mon.trig[0] != t0 && mon.trig[2] != t2;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) pat[0].step[t][st].flags = 0;
        seq_play(1);
        sq.rec = 1;
        for (int f = 0; f < 44100 / 2; f += 128) m2k_render(b16, 128);
        m2k_key(14, 1); m2k_key(14, 0);               /* G4: white 9, T1 again */
        int written = 0;
        for (int st = 0; st < M2K_STEPS; st++) written += pat[0].step[0][st].flags & ST_TRIG ? 1 : 0;
        seq_play(0);
        CHECK(pads && hit && written == 1, "PADS (GLO + ARP): white keys hit T1-T6, each on its own; with REC, a trig written");
        /* a pad plays its track's root: the note its trigs sit on most (not the first trig's), the lowest of equals;
         * a trig past the pattern's length does not count; a track without trigs, the note new trigs take */
        m2k_init(37);
        m2k_button(B_GLO, 1); m2k_button(B_ARP, 1); m2k_button(B_ARP, 0); m2k_button(B_GLO, 0);
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) pat[0].step[t][st].flags = 0;
        pat[0].glob[G_LEN] = 16;
        static const uint8_t RN[6][2] = {{0, 57}, {4, 45}, {8, 45}, {12, 52}, {20, 40}, {24, 40}};   /* T1 */
        for (int i = 0; i < 6; i++) { pat[0].step[0][RN[i][0]].flags = ST_TRIG; pat[0].step[0][RN[i][0]].note = RN[i][1]; }
        pat[0].step[2][3].flags = pat[0].step[2][9].flags = ST_TRIG;                                   /* T3: a tie */
        pat[0].step[2][3].note = 64; pat[0].step[2][9].note = 59;
        m2k_key(0, 1); m2k_key(0, 0);                 /* T1 */
        m2k_key(4, 1); m2k_key(4, 0);                 /* T3 */
        m2k_key(2, 1); m2k_key(2, 0);                 /* T2: no trigs */
        CHECK(trk[0].note == 45 && trk[2].note == 59 && trk[1].note == ui.note[1],
              "PADS: a pad plays its track's root (its trigs' most common note, the lowest of equals, within the length)");
        /* GLO + HOME: the keys' mode in turn, KEYS -> PADS -> GRID -> KEYS, on the page it was (no HOME), HOME's
         * release no latch; GLO + ARP still toggles PADS */
        m2k_init(39);
        ui.page = PG_AMP;
        ui.now = 30000;
        m2k_button(B_GLO, 1);
        m2k_button(B_HOME, 1); m2k_button(B_HOME, 0);
        int m1 = ui.pads && !ui.grid;
        ui.now += 300;
        m2k_button(B_HOME, 1); m2k_button(B_HOME, 0);
        int m2 = !ui.pads && ui.grid;
        ui.now += 300;
        m2k_button(B_HOME, 1); m2k_button(B_HOME, 0);
        int m3 = !ui.pads && !ui.grid;
        m2k_button(B_GLO, 0);
        int stay = ui.page == PG_AMP && !ui.latch;
        ui.now += 300;
        m2k_button(B_GLO, 1); m2k_button(B_ARP, 1); m2k_button(B_ARP, 0); m2k_button(B_GLO, 0);
        int arp = ui.pads;
        CHECK(m1 && m2 && m3 && stay && arp, "GLO + HOME: the keys' mode in turn, KEYS PADS GRID; the page kept; GLO + ARP still PADS");
        /* Ready (Demake's): stopped, REC on, PLAY: it waits; the first key starts the pattern from its first step and
         * is recorded there; PLAY again starts it now; REC off ends the wait; a MIDI note starts it; SYIN on: PLAY plays */
        {
            static int16_t rb[256];
            m2k_init(40);
            open_b01();
            for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) pat[16].step[t][st].flags = 0;
            ui.trk = 0; ui.page = PG_AMP; ui.now = 40000;
            m2k_button(B_REC, 1); ui.now += 50; m2k_button(B_REC, 0);                 /* REC tapped: step entry */
            int entry = sq.rec && !sq.playing;
            ui.now += 200;
            m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
            int waits = sq.ready && !sq.playing;
            for (int i = 0; i < 20; i++) m2k_render(rb, 128);                         /* nothing moves while it waits */
            int still = !sq.playing && !(pat[16].step[0][0].flags & ST_TRIG);
            m2k_key(4, 1);                                                             /* A: the first note */
            int started = sq.playing && !sq.ready;
            int rec0 = (pat[16].step[0][0].flags & ST_TRIG) && pat[16].step[0][0].note == 57 + 12 * ui.oct[0];
            m2k_key(4, 0);
            seq_play(0);
            ui.now += 200;
            m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);                             /* ready again */
            m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);                             /* PLAY again: now */
            int now = sq.playing && !sq.ready;
            seq_play(0);
            m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
            ui.now += 400;
            m2k_button(B_REC, 1); ui.now += 50; m2k_button(B_REC, 0);                 /* REC off: no wait */
            int off = !sq.rec && !sq.ready && !sq.playing;
            m2k_button(B_REC, 1); ui.now += 50; m2k_button(B_REC, 0);
            ui.now += 200;
            m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
            m2k_midi(0x90 | (midi_ch() & 15), 60, 100);                               /* a MIDI note starts it */
            int midi = sq.playing && !sq.ready;
            m2k_midi(0x80 | (midi_ch() & 15), 60, 0);
            seq_play(0);
            m2k_setting(S_SYNI, 127);
            m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);                             /* following a clock: no wait */
            int ext = sq.playing && !sq.ready;
            seq_play(0);
            m2k_setting(S_SYNI, 0);
            CHECK(entry && waits && still && started && rec0 && now && off && midi && ext,
                  "REC + PLAY stopped: Ready, the first note starts it on step 1, recorded; PLAY again now; REC off; MIDI; SYIN plays");
        }
        /* ALGORITHM: only on EDIT (SYN) the track's machine; on the other pages their pages, to the ends (LFO 1A..3B);
         * on a page of one (HOME), nothing */
        {
            m2k_init(41);
            int m0 = PAT(cur_pat)->machine[ui.trk];
            ui.page = PG_LFO; ui.sub = 0;
            for (int i = 0; i < 8; i++) m2k_encoder(E_ALGO, 1);
            int lfo_end = ui.sub == 5 && PAT(cur_pat)->machine[ui.trk] == m0;
            for (int i = 0; i < 2; i++) m2k_encoder(E_ALGO, -1);
            int lfo_back = ui.sub == 3;
            ui.page = PG_HOME; ui.sub = 0;
            m2k_encoder(E_ALGO, 1);
            int home = ui.page == PG_HOME && ui.sub == 0 && PAT(cur_pat)->machine[ui.trk] == m0;
            ui.page = PG_SYN; ui.sub = 0;
            m2k_encoder(E_ALGO, 1);
            int syn = PAT(cur_pat)->machine[ui.trk] != m0;
            CHECK(lfo_end && lfo_back && home && syn, "ALGORITHM: the page's pages (LFO 1A..3B, to its ends), nothing on HOME, the machine on EDIT");
        }
        /* SWAV-PUL's PW sweep (PWAD) bounces between 4 % and 96 %: no dropout through the silent ends every cycle
         * (the owner's "clip every few seconds, like looping": its level fell to a tenth); PWAD 0 as it was */
        {
            static int16_t pb[2 * 44100 * 8];
            m2k_init(42);
            m2k_pattern(16);
            m2k_pattern_t *q = PAT(cur_pat);
            for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) q->step[t][st].flags = 0;
            kit_default(q, 0, M_PUL);
            q->kit[0][P_HOLD] = 127; q->kit[0][P_DEC] = 127; q->kit[0][P_SYN + 5] = 8;
            q->mute = 0x3E;
            for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, q->kit[t], P_COUNT);
            ui.trk = 0; sq.ktrk = 0;
            live_on(57, 100);
            int n = (44100 * 8) & ~127;
            for (int f = 0; f < n; f += 128) m2k_render(pb + 2 * f, 128);
            live_off(57);
            double lo = 1e9, sum = 0; int nw = 0;
            for (int w = 10; w < n / 882; w++) {
                double m = 0, a = 0;
                for (int i = 0; i < 882; i++) m += pb[2 * (w * 882 + i)];
                m /= 882;
                for (int i = 0; i < 882; i++) { double x = pb[2 * (w * 882 + i)] - m; a += x * x; }
                a = sqrt(a / 882); sum += a; nw++; if (a < lo) lo = a;
            }
            CHECK(lo > 0.25 * sum / nw, "SWAV-PUL: PWAD's sweep bounces between 4 % and 96 %, no dropout every cycle");
        }
        /* GLO > SET: WIPE asks, OCT+ makes every pattern INIT (the sounds kept), OCT- or any other button keeps them;
         * DEMO asks, OCT+ brings bank A's back */
        m2k_init(38);
        ui.page = PG_GLO; ui.sub = 3;
        ui.now = 3000;
        int wk = -1, dk = -1;
        for (int k = 0; k < 4; k++) { int c = knob_param(PG_GLO, 3, k); if (c == SETTING(22)) wk = k; if (c == SETTING(23)) dk = k; }
        edit(wk, 1);
        m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0);             /* NO */
        int first = !strcmp(pat[0].name, "CIRCUIT") && !ask.kind;
        ui.now = 3500;
        edit(wk, 1);
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);             /* YES */
        int wiped = 1;
        for (int i = 0; i < M2K_PATTERNS; i++) wiped &= !strcmp(pat[i].name, "INIT");
        int kept = snd[0].name[0] != 0;
        int demos = 1;
        if (dk >= 0) {
            ui.now = 6000; edit(dk, 1); m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);
            demos = !strcmp(pat[0].name, "CIRCUIT") && !strcmp(pat[3].name, "LANTERN") && !strcmp(pat[16].name, "INIT");
        }
        CHECK(wk >= 0 && first && wiped && kept && demos, "GLO > SET: WIPE asks (OCT- NO, OCT+ YES: every pattern INIT, the sounds kept); DEMO, bank A back");
        /* DEMO (the FM-1's GLO > SET) brings back the slots with a demo, A01-A04, and says so on the screen: A05 the
         * player's since, kept */
        {
            m2k_init(40);
            for (uint32_t ms = 0; ms <= 3000; ms += 500) m2k_frame(ms);
            for (int i = 0; i < 4; i++) { s_cpy(pat[i].name, "WIPED"); pat[i].kit[0][P_VOL] = 3; }
            s_cpy(pat[4].name, "MINE");
            for (int s = 0; s < 64; s++) pat[4].step[0][s].flags = ST_TRIG;
            int mine = 0, before = ta_found;
            ask.kind = ASK_DEMO;
            ask_answer(1);
            m2k_frame(3500);
            ta_check(3500);
            for (int s = 0; s < 64; s++) mine += pat[4].step[0][s].flags & ST_TRIG;
            CHECK(!strcmp(pat[0].name, "CIRCUIT") && !strcmp(pat[3].name, "LANTERN") && pat[3].kit[0][P_VOL] != 3 &&
                  !strcmp(pat[4].name, "MINE") && mine == 64 && !strcmp(ui.toast2, "A01-A04, THE REST KEPT") && ta_found == before,
                  "DEMO: A01-A04 back, the slots with a demo; A05 the player's, kept; the message says so and fits");
        }
        /* HOME held: the white keys pick patterns, SEQ clears them (held: INIT), REC copies, PLAY pastes, the black
         * keys 1-4 the bank; EDIT held: SEQ INITs the sound, SAVE keeps it in the first INIT slot */
        {
        m2k_init(39);
        ui.now = 1000;
        m2k_button(B_HOME, 1);
        m2k_key(2, 1); m2k_key(2, 0);                 /* white 2: A02 */
        m2k_key(7, 1); m2k_key(7, 0);                 /* C4, white 5: A05 */
        int picked = cur_pat == 4;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int clr = 1;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) clr &= !(pat[1].step[t][st].flags & ST_TRIG) && !(pat[4].step[t][st].flags & ST_TRIG);
        int others = 0;
        for (int st = 0; st < M2K_STEPS; st++) others += pat[0].step[0][st].flags & ST_TRIG ? 1 : 0;
        int sounds = !strcmp(pat[1].name, "TIDE");
        ui.now = 1500;
        m2k_button(B_SEQ, 1);                         /* held a second: INIT */
        ui.now = 2600; ring_tick();
        m2k_button(B_SEQ, 0);
        int init = !strcmp(pat[1].name, "INIT") && !strcmp(pat[4].name, "INIT");
        m2k_button(B_HOME, 0);
        m2k_pattern(0);
        m2k_button(B_HOME, 1);
        m2k_button(B_REC, 1); m2k_button(B_REC, 0);   /* copy A01 */
        m2k_key(3, 1); m2k_key(3, 0);                 /* G#3, black 2: bank B */
        m2k_key(4, 1); m2k_key(4, 0);                 /* A3, white 3: B03 */
        m2k_key(6, 1); m2k_key(6, 0);                 /* B3, white 4: B04 */
        m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
        m2k_button(B_HOME, 0);
        int pasted = 0;
        for (int i = 16; i < 32; i++) pasted += !strcmp(pat[i].name, "CIRCUIT");
        CHECK(picked && clr && others && sounds && init && pasted == 2 && sq.playing == 0,
              "HOME held: keys pick patterns, SEQ clears them (sounds kept), held INIT; REC copies, black 2 bank B, PLAY pastes");
        m2k_init(40);
        ui.trk = 2;
        pat[0].kit[2][P_DEC] = 3;
        m2k_button(B_EDIT, 1);
        m2k_button(B_SAVE, 1); m2k_button(B_SAVE, 0);
        int kept_snd = snd[24].name[0] && snd[24].kit[P_DEC] == 3;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        m2k_button(B_EDIT, 0);
        m2k_pattern_t fr;
        pat_init(&fr, "X");
        kit_init(&fr, 2, pat[0].machine[2]);
        CHECK(kept_snd && !memcmp(pat[0].kit[2], fr.kit[2], P_COUNT) && !ui.grid,
              "EDIT held: SAVE keeps the sound in the first INIT slot, SEQ INITs it (its machine's INIT; no GRID)");
        }
        /* SEQ: a tap is GRID; held, the white keys are tools on the track's steps (their locks with them), the knobs
         * LEN SWING ACCENT TEMPO, and letting go is not a tap */
        {
        m2k_init(41);
        ui.now = 1000;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int tap = ui.grid == 1;
        ui.now = 2000;                                /* (closer, the second tap latches the layer) */
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        tap &= ui.grid == 0;
        ui.now = 3000;
        m2k_pattern_t *q = &pat[0];
        int len = seq_len(), t = ui.trk;
        for (int st = 0; st < M2K_STEPS; st++) q->step[t][st].flags = 0;
        for (int k = 0; k < M2K_LOCKS; k++) if (q->lock[k].trk == t) q->lock[k].trk = 0xFF;
        q->step[t][0].flags = ST_TRIG; q->step[t][0].note = 50;
        q->step[t][5].flags = ST_TRIG; q->step[t][5].note = 55;
        lock_set(t, 5, P_DEC, 99);
        m2k_button(B_SEQ, 1);
        m2k_key(2, 1); m2k_key(2, 0);                 /* G3, white 2: REVERSE */
        int rev = (q->step[t][len - 1].flags & ST_TRIG) && q->step[t][len - 6].note == 55 && lock_find(t, len - 6, P_DEC) >= 0;
        m2k_key(4, 1); m2k_key(4, 0);                 /* A3, white 3: SHIFT < */
        int shl = (q->step[t][len - 2].flags & ST_TRIG) && lock_find(t, len - 7, P_DEC) >= 0;
        m2k_button(B_REC, 1); m2k_button(B_REC, 0);   /* REC: copy the track's steps */
        int len0 = PAT(cur_pat)->glob[G_LEN];
        edit(0, -1);                                  /* KNOB 1 in the layer: LEN */
        int lenk = PAT(cur_pat)->glob[G_LEN] == len0 - 1;
        edit(0, 1);
        m2k_button(B_SEQ, 0);
        int nogrid = ui.grid == 0;
        ui.trk = (uint8_t)((t + 1) % M2K_TRACKS);
        m2k_button(B_SEQ, 1); m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0); m2k_button(B_SEQ, 0);   /* PLAY: paste */
        int pasted = !memcmp(q->step[ui.trk], q->step[t], sizeof q->step[t]) && lock_find(ui.trk, len - 7, P_DEC) >= 0;
        m2k_button(B_SEQ, 1); m2k_key(12, 1); m2k_key(12, 0); m2k_button(B_SEQ, 0);   /* F4, white 8: HALF */
        int half = seq_len() == len / 2;
        CHECK(tap && rev && shl && lenk && nogrid && pasted && half,
              "SEQ: a tap is GRID; held, REVERSE and SHIFT move the steps and their locks, REC / PLAY copy and paste, HALF, KNOB 1 LEN");
        /* a step held in GRID: REC copies it, PLAY pastes it onto another held, SEQ clears the held step's locks */
        m2k_init(42);
        ui.grid = 1; ui.trk = 0; ui.spage = 0;
        m2k_pattern_t *r = &pat[0];
        for (int st = 0; st < M2K_STEPS; st++) r->step[0][st].flags = 0;
        for (int k = 0; k < M2K_LOCKS; k++) if (r->lock[k].trk == 0) r->lock[k].trk = 0xFF;
        r->step[0][0].flags = ST_TRIG | ST_ACC; r->step[0][0].note = 67; lock_set(0, 0, P_DEC, 12);
        m2k_key(0, 1);                                /* F3: step 1 held */
        m2k_button(B_REC, 1); m2k_button(B_REC, 0);
        m2k_key(0, 0);
        m2k_key(7, 1);                                /* C4: step 5 held */
        m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
        int spasted = r->step[0][4].note == 67 && (r->step[0][4].flags & ST_ACC) && lock_find(0, 4, P_DEC) >= 0;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);   /* SEQ: its locks cleared, the step kept */
        int lclr = lock_find(0, 4, P_DEC) < 0 && (r->step[0][4].flags & ST_TRIG);
        m2k_key(7, 0);
        int stays = (r->step[0][4].flags & ST_TRIG) && ui.grid == 1;
        CHECK(spasted && lclr && stays, "a held step: REC copies it, PLAY pastes it onto another, SEQ clears its locks");
        }
    }
    /* performance: FX tapped is its page, held its black keys PERFORM (a repeat while held, KILL silences); LFO held,
     * the knobs snap back (OCT+ keeps them); GLO + white 9-14 solo while held, 7 a fill for the next loop, 8 taps the
     * tempo, 15 unmutes all; GLO + SELECT the tempo */
    {
        m2k_init(43);
        static int16_t pb[256];
        ui.now = 1000;
        m2k_button(B_FX, 1); m2k_button(B_FX, 0);
        int fxpage = ui.page == PG_FX;
        ui.page = PG_HOME;
        ui.now = 2000;
        seq_play(1);
        for (int f = 0; f < 44100; f += 128) m2k_render(pb, 128);
        m2k_button(B_FX, 1);
        m2k_key(1, 1);                                /* F#3, black 1: a quarter's repeat */
        for (int f = 0; f < 4410; f += 128) m2k_render(pb, 128);
        int rep_on = bus.g_on && bus.g_left > 100000;
        m2k_key(1, 0);
        for (int f = 0; f < 512; f += 128) m2k_render(pb, 128);
        int rep_off = !bus.g_on;
        m2k_key(25, 1);                               /* F#5, black 11: KILL */
        int32_t pk = 0;
        for (int f = 0; f < 4410; f += 128) { m2k_render(pb, 128); if (f > 1000) for (int i = 0; i < 256; i++) pk = pb[i] > pk ? pb[i] : -pb[i] > pk ? -pb[i] : pk; }
        m2k_key(25, 0);
        m2k_button(B_FX, 0);
        int killed = pk == 0 && ui.page == PG_HOME;
        seq_play(0);
        ui.page = PG_AMP; ui.sub = 0; ui.trk = 1;
        int dec0 = pat[0].kit[1][P_DEC];
        m2k_button(B_LFO, 1);
        edit(2, 10);                                  /* KNOB 3: DEC */
        int moved = pat[0].kit[1][P_DEC] != dec0;
        m2k_button(B_LFO, 0);
        int back = pat[0].kit[1][P_DEC] == dec0 && ui.page == PG_AMP;
        m2k_button(B_LFO, 1); edit(2, 10); m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0); m2k_button(B_LFO, 0);
        int kept = pat[0].kit[1][P_DEC] != dec0;
        CHECK(fxpage && rep_on && rep_off && killed && moved && back && kept,
              "PERFORM: FX tapped its page, held a repeat while a black key is held, KILL silent; LFO held: the knobs snap back, OCT+ keeps");
        m2k_init(44);
        m2k_button(B_GLO, 1);
        ui.now = 5000; m2k_key(19, 1); m2k_key(19, 0);   /* G4, white 9... (white 8 is F4) */
        m2k_button(B_GLO, 0);
        int fkey = -1, tkey = -1, skey = -1, ukey = -1;
        for (int k = 0; k < M2K_KEYS; k++) { if (ui.kw[k] == 6) fkey = k; if (ui.kw[k] == 7) tkey = k; if (ui.kw[k] == 8) skey = k; if (ui.kw[k] == 14) ukey = k; }
        m2k_button(B_GLO, 1);
        ui.now = 10000; m2k_key(tkey, 1); m2k_key(tkey, 0);
        ui.now = 10500; m2k_key(tkey, 1); m2k_key(tkey, 0);
        ui.now = 11000; m2k_key(tkey, 1); m2k_key(tkey, 0);
        int tapped = (int)seq_bpm() == 120;
        int tmp0 = pat[0].glob[G_TMPO];
        m2k_encoder(E_SELECT, 2);
        int sel = pat[0].glob[G_TMPO] == tmp0 + 2;
        m2k_key(fkey, 1); m2k_key(fkey, 0);
        int fillq = sq.fill_next == 1;
        m2k_key(skey, 1);                             /* solo T1 while held */
        int solo = sq.solo == 1;
        m2k_key(skey, 0);
        int unsolo = sq.solo == 0;
        pat[0].mute = 0x2A;
        m2k_key(ukey, 1); m2k_key(ukey, 0);
        int allon = pat[0].mute == 0;
        m2k_button(B_GLO, 0);
        seq_play(1);
        static int16_t pb2[256];
        int saw_fill = 0;
        for (int f = 0; f < 44100 * 30 && !saw_fill; f += 128) { m2k_render(pb2, 128); saw_fill |= sq.fill; }
        int fill_ends = 0;
        for (int f = 0; f < 44100 * 30 && !fill_ends; f += 128) { m2k_render(pb2, 128); fill_ends |= !sq.fill; }
        seq_play(0);
        CHECK(tapped && sel && fillq && solo && unsolo && allon && saw_fill && fill_ends,
              "GLO: white 8 taps the tempo (120), SELECT turns it, 7 fills the next loop and only it, 9-14 solo while held, 15 unmutes all");
    }
    /* the latch: GLO, SEQ, HOME, FX double-tapped stay as if held over the page the first tap left (taps slower step
     * through the pages); the button again lets go, and so does a page button the layer has no use for */
    {
        m2k_init(47);
        int wk[16], bk[11];
        for (int k = 0; k < M2K_KEYS; k++) { if (ui.kw[k] >= 0 && ui.kw[k] < 16) wk[ui.kw[k]] = k; if (ui.kb[k] >= 0 && ui.kb[k] < 11) bk[ui.kb[k]] = k; }
        ui.page = PG_AMP; ui.sub = 0;
        ui.now = 1000; m2k_button(B_SEQ, 1); ui.now = 1060; m2k_button(B_SEQ, 0);
        int g1 = ui.grid == 1;
        ui.now = 1200; m2k_button(B_SEQ, 1); ui.now = 1260; m2k_button(B_SEQ, 0);
        int seq_l = ui.latch == B_SEQ + 1 && ui.grid == 0 && seq_layer() && knob_at(0) == GL(G_LEN) && ui.page == PG_AMP;
        int len0 = seq_len();
        m2k_key(wk[7], 1); m2k_key(wk[7], 0);         /* white 8: HALF, the layer's */
        int tool = seq_len() == len0 / 2;
        ui.now = 3000; m2k_button(B_SEQ, 1); ui.now = 3050; m2k_button(B_SEQ, 0);
        int seq_off = !ui.latch && ui.grid == 0;
        ui.now = 5000; m2k_button(B_GLO, 1); ui.now = 5040; m2k_button(B_GLO, 0);
        int on_glo = ui.page == PG_GLO;
        ui.now = 5150; m2k_button(B_GLO, 1); ui.now = 5200; m2k_button(B_GLO, 0);
        int glo_l = ui.latch == B_GLO + 1 && ui.page == PG_AMP;
        m2k_key(wk[1], 1); m2k_key(wk[1], 0);         /* white 2: T2 muted */
        int muted = (pat[cur_pat].mute >> 1) & 1;
        const uint8_t *L = m2k_leds();
        int led_glo = L[wk[0]] >= 64 && L[wk[1]] < 64 && L[M2K_KEYS + B_GLO] == (((ui.now >> 8) & 1) ? 255 : 0);
        m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0); /* PLAY is PLAY meanwhile (the paste wants GLO held) */
        int stays = ui.latch == B_GLO + 1 && sq.playing;
        m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
        stays &= !sq.playing;
        ui.now = 6000; m2k_button(B_HOME, 1); m2k_button(B_HOME, 0);   /* HOME lets go of it, and is HOME */
        int glo_off = !ui.latch && ui.page == PG_HOME;
        ui.page = PG_AMP;
        ui.now = 8000; m2k_button(B_HOME, 1); ui.now = 8050; m2k_button(B_HOME, 0);
        ui.now = 8200; m2k_button(B_HOME, 1); ui.now = 8250; m2k_button(B_HOME, 0);
        int home_l = ui.latch == B_HOME + 1 && ui.page == PG_AMP;
        m2k_key(wk[2], 1); m2k_key(wk[2], 0);         /* white 3: A03 */
        int picked = cur_pat == 2;
        L = m2k_leds();
        int led_home = L[wk[2]] >= 64 || ((ui.now & 1023) >= 900);
        led_home &= L[wk[0]] >= 64 && L[bk[0]] >= 64;
        m2k_button(B_SEL, 1); m2k_button(B_SEL, 0);   /* a page button: lets go, and is its page */
        int home_off = !ui.latch && ui.page == PG_FILT;
        ui.now = 10000; m2k_button(B_FX, 1); ui.now = 10050; m2k_button(B_FX, 0);
        ui.now = 10150; m2k_button(B_FX, 1); ui.now = 10200; m2k_button(B_FX, 0);
        int fx_l = ui.latch == B_FX + 1 && ui.page == PG_FILT && perf_layer();
        m2k_key(bk[0], 1);
        int pf = perf.on[0];
        L = m2k_leds();
        int led_pf = L[bk[0]] >= 64 && L[bk[1]] < 64;
        m2k_key(bk[0], 0);
        int pf_off = !perf.on[0];
        ui.now = 11000; m2k_button(B_FX, 1); m2k_button(B_FX, 0);
        int fx_off = !ui.latch && ui.page == PG_FILT;
        ui.page = PG_GLO; ui.sub = 0;                 /* taps 300 ms apart step through GLO's pages */
        ui.now = 13000; m2k_button(B_GLO, 1); ui.now = 13040; m2k_button(B_GLO, 0);
        ui.now = 13340; m2k_button(B_GLO, 1); ui.now = 13380; m2k_button(B_GLO, 0);
        int slow = !ui.latch && ui.page == PG_GLO && ui.sub == 2;
        m2k_button(B_SEQ, 1);
        L = m2k_leds();
        int led_seq = L[wk[0]] >= 64 && L[wk[7]] >= 64 && L[wk[8]] < 64;
        m2k_button(B_SEQ, 0);
        CHECK(g1 && seq_l && tool && seq_off && on_glo && glo_l && muted && stays && glo_off && home_l && picked &&
              home_off && fx_l && pf && pf_off && fx_off && slow,
              "latch: SEQ, GLO, HOME, FX double-tapped stay held over the page left; again, HOME or a page lets go; slow taps page");
        CHECK(led_glo && led_home && led_pf && led_seq,
              "LEDs: GLO the tracks heard, a latched button blinking; HOME the used patterns and banks; PERFORM the effect on; SEQ 1-8");
        /* a help card: OCT- held a second alone shows it and its release does nothing; held shorter, the octave; a key
         * meanwhile, no card */
        m2k_init(48);
        int o0 = ui.oct[ui.trk];                      /* (T1, A01's kick: at home two octaves down) */
        ui.now = 1000; m2k_button(B_OCTD, 1); ui.now = 2100;
        int card = card_up();
        m2k_frame(ui.now);
        m2k_button(B_OCTD, 0);
        int quiet = ui.oct[ui.trk] == o0 && !ui.octd;
        ui.now = 3000; m2k_button(B_OCTD, 1); ui.now = 3300; m2k_button(B_OCTD, 0);
        int oct = o0 == -2 && ui.oct[ui.trk] == o0 - 1;
        ui.now = 4000; m2k_button(B_PLAY, 1); m2k_key(wk[0], 1); ui.now = 5200;
        int nocard = !card_up();
        m2k_key(wk[0], 0); m2k_button(B_PLAY, 0);
        seq_play(0);
        CHECK(card && quiet && oct && nocard, "a help card: OCT- held a second alone, its release then nothing; shorter the octave; a key meanwhile, none");
    }
    /* REC held and SEQ: the track in view's steps cleared, in KEYS (REC's release then neither arms nor fills); again
     * undone; held a second, its sound still kept (REC + SEQ never touches the sound) */
    {
        m2k_init(49);
        ui.grid = 0; ui.trk = 1;
        m2k_pattern_t *q = &pat[cur_pat];
        q->step[1][3].flags = ST_TRIG; q->step[0][2].flags = ST_TRIG;
        m2k_pattern_t keep;
        m_cpy(&keep, q, sizeof keep);
        ui.now = 1000; m2k_button(B_REC, 1); ui.now = 1100; m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int gone = !(q->step[1][3].flags & ST_TRIG) && (q->step[0][2].flags & ST_TRIG);
        ui.now = 1600; m2k_frame(ui.now);
        int nofill = !sq.fill;
        m2k_button(B_REC, 0);
        int norec = !sq.rec && ui.grid == 0;
        ui.now = 2000; m2k_button(B_REC, 1); m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0); m2k_button(B_REC, 0);
        int back = !memcmp(q->step[1], keep.step[1], sizeof keep.step[1]);
        q->kit[1][P_SYN + 2] ^= 33;
        ui.now = 4000; m2k_button(B_REC, 1); m2k_button(B_SEQ, 1);
        for (int ms = 4000; ms <= 5300; ms += 16) { ui.now = (uint32_t)ms; m2k_frame(ui.now); }
        m2k_button(B_SEQ, 0); m2k_button(B_REC, 0);
        int init = q->kit[1][P_SYN + 2] == (uint8_t)(keep.kit[1][P_SYN + 2] ^ 33) && !(q->step[1][3].flags & ST_TRIG) && !sq.rec;
        CHECK(gone && nofill && norec && back && init,
              "REC held and SEQ: the track's steps cleared (REC then neither arms nor fills), again undone, held a second the sound kept");
    }
    /* the DRM- drums (after the LXR's voices), held to the laws they were measured on against a float model of the LXR's
     * (correlation 1.0000): the kick's sweep a step every 64 samples (its init drum at C2: 211.6 Hz in the first step,
     * 168.9 at 10 ms, 109.4 at 20, the note by 100); the clap's hits, as many as CLAP says; the hats' open decay longer
     * than the closed; the cymbal's swell rising; none silent, none past full scale */
    {
        static int16_t raw[44100];
        int okd = 1;
        double hz[4] = {0};
        int hits = 0, n40[2] = {0, 0}, swell = 0;
        for (int c = 0; c < 6; c++) {
            static const uint8_t MC[6] = {M_DKIK, M_DSNR, M_DHAT, M_DHAT, M_DCYM, M_DCYM};
            static const uint8_t KC[6][8] = {{0, 44, 26, 21, 0, 64, 0, 0}, {40, 81, 14, 127, 100, 90, 48, 6},
                                             {54, 60, 127, 127, 127, 0, 5, 0}, {54, 60, 127, 127, 127, 0, 5, 17},
                                             {127, 64, 0, 64, 0, 110, 0, 107}, {127, 64, 0, 64, 0, 110, 90, 107}};
            m2k_init(50);
            m2k_pattern_t *q = PAT(cur_pat);
            kit_default(q, 0, MC[c]);
            m_cpy(q->kit[0], KC[c], 8);
            trk_t *t = &trk[0];
            m_cpy(t->eff, q->kit[0], P_COUNT);
            m_set(t->mod, 0, sizeof t->mod);
            int note = c == 0 ? 36 : 60;
            t->note = (uint8_t)note;
            mach_trig(t, MC[c]);
            int32_t pk = 0;
            double e2 = 0;
            for (int b = 0; b < 44100 / 16; b++) {
                mach_block(0, MC[c], note * 64);
                for (int i = 0; i < 16; i++) { int v = blk[0][i]; raw[b * 16 + i] = (int16_t)v; pk = v > pk ? v : -v > pk ? -v : pk; e2 += (double)v * v; }
            }
            okd &= pk > 4000 && pk <= 32767 && e2 > 0;
            if (c == 0) {                                        /* the sweep: rising zero crossings, the period around a time */
                static const int at[4] = {44, 441, 882, 4410};
                for (int k = 0; k < 4; k++) {                    /* each period's pitch at its middle, interpolated */
                    double z0 = -1, m0 = -1, f0 = 0;
                    for (int i = 1; i < 44100 && !hz[k]; i++)
                        if (raw[i - 1] < 0 && raw[i] >= 0) {
                            double z = i - 1 + (double)-raw[i - 1] / (raw[i] - raw[i - 1]);
                            if (z0 >= 0) {
                                double mid = (z0 + z) / 2, f = 44100.0 / (z - z0);
                                if (mid >= at[k]) hz[k] = m0 < 0 ? f : f0 + (f - f0) * (at[k] - m0) / (mid - m0);
                                m0 = mid; f0 = f;
                            }
                            z0 = z;
                        }
                }
            }
            if (c == 1) {                                        /* the clap: its falls in the first 100 ms (2 ms rms dips) */
                double last = 1e9;
                int down = 0;
                for (int w = 0; w < 50; w++) {
                    double r = 0;
                    for (int i = 0; i < 88; i++) r += (double)raw[w * 88 + i] * raw[w * 88 + i];
                    if (r > last * 8 && down) { hits++; down = 0; }
                    if (r < last) down = 1;
                    last = r;
                }
            }
            if (c == 2 || c == 3) {                              /* the hats: the 10 ms window 40 dB under the first */
                double r0 = 0;
                for (int i = 0; i < 441; i++) r0 += (double)raw[i] * raw[i];
                for (int w = 1; w < 100; w++) {
                    double r = 0;
                    for (int i = 0; i < 441; i++) r += (double)raw[w * 441 + i] * raw[w * 441 + i];
                    if (r < r0 * 1e-4) { n40[c - 2] = w * 10; break; }
                }
            }
            if (c == 5) {                                        /* the swell: 300 ms louder than its first 10 */
                double r0 = 0, r1 = 0;
                for (int i = 0; i < 441; i++) { r0 += (double)raw[i] * raw[i]; r1 += (double)raw[13230 + i] * raw[13230 + i]; }
                swell = r1 > r0 * 100;
            }
        }
        printf("     DRM-KICK at C2: %.1f %.1f %.1f %.1f Hz; the clap %d falls; the hats -40 dB at %d ms closed, %d open\n",
               hz[0], hz[1], hz[2], hz[3], hits + 1, n40[0], n40[1]);
        int sweep = fabs(hz[0] - 211.6) < 1.5 && fabs(hz[1] - 168.9) < 1.5 && fabs(hz[2] - 109.4) < 1.0 && fabs(hz[3] - 65.4) < 0.3;
        CHECK(okd && sweep && hits + 1 >= 6 && n40[0] > 0 && n40[1] > 2 * n40[0] && swell,
              "DRM- drums: the kick's sweep on the LXR's law and its steps, the clap's hits, open hats longer than closed, the swell; none silent or past full scale");
    }
    /* speeds: a track at 2x plays its steps twice as often as one at 1x, at 1/2x half as often, at 3/4x three quarters;
     * a new pattern sets them going from its first step; SEQ held, KNOB 2 is the track's speed */
    {
        m2k_init(45);
        m2k_pattern_t *q = &pat[0];
        for (int t = 0; t < M2K_TRACKS; t++)
            for (int st = 0; st < M2K_STEPS; st++) { q->step[t][st].flags = st < seq_len() ? ST_TRIG : 0; q->step[t][st].cond = 0; q->step[t][st].rtrg = 0; }
        for (int k = 0; k < M2K_LOCKS; k++) q->lock[k].trk = 0xFF;
        q->glob[G_SWNG] = 0;
        q->glob[G_SPD1 + 1] = enc(6, 7);              /* T2 2x */
        q->glob[G_SPD1 + 2] = enc(2, 7);              /* T3 1/2x */
        q->glob[G_SPD1 + 3] = enc(3, 7);              /* T4 3/4x */
        int cnt[M2K_TRACKS] = {0};
        uint8_t last[M2K_TRACKS];
        for (int t = 0; t < M2K_TRACKS; t++) last[t] = (uint8_t)mon.trig[t];
        seq_play(1);
        static int16_t sb[256];
        int32_t bar = (int32_t)(sps_q8 >> 8) * 16;   /* a pattern of 16, in samples */
        for (int f = 0; f < bar * 4 - 128; f += 16) {   /* (the trig counter is a byte: counted a block at a time) */
            m2k_render(sb, 16);
            for (int t = 0; t < M2K_TRACKS; t++) { cnt[t] += (uint8_t)((uint8_t)mon.trig[t] - last[t]); last[t] = (uint8_t)mon.trig[t]; }
        }
        seq_play(0);
        int n1 = cnt[0], n2 = cnt[1], nh = cnt[2], n34 = cnt[3];
        printf("     trigs in four loops: 1x %d, 2x %d, 1/2x %d, 3/4x %d\n", n1, n2, nh, n34);
        ui.trk = 1;
        m2k_button(B_SEQ, 1);
        int k2 = knob_at(1) == GL(G_SPD1 + 1);
        m2k_button(B_SEQ, 0);
        CHECK(n1 == 64 && n2 == 128 && nh == 32 && n34 == 48 && k2,
              "speeds: 2x twice the trigs of 1x, 1/2x half, 3/4x three quarters; SEQ held, KNOB 2 the track's speed");
    }
    /* a step held in GRID: the black keys 1-5 its trig type, 6-11 its condition; OCT- held and a knob turned takes
     * that knob's lock away (with no step held, the value back to its start), and OCT- alone still turns the octave */
    {
        m2k_init(46);
        int bk[11];
        for (int k = 0; k < M2K_KEYS; k++) if (ui.kb[k] >= 0 && ui.kb[k] < 11) bk[ui.kb[k]] = k;
        ui.grid = 1; ui.trk = 0; ui.spage = 0;
        m2k_pattern_t *q = &pat[0];
        q->step[0][0].flags = ST_TRIG; q->step[0][0].cond = 0;
        m2k_key(0, 1);                                /* step 1 held */
        m2k_key(bk[2], 1); m2k_key(bk[2], 0);         /* black 3: FILTER */
        int tt = (q->step[0][0].flags & ST_TTYPE) == TTYPE[2];
        m2k_key(bk[6], 1); m2k_key(bk[6], 0);         /* black 7: 50% */
        int cd = q->step[0][0].cond == C_P50 && ui.trk == 0;
        ui.page = PG_AMP; ui.sub = 0;
        edit(2, 5);                                   /* a lock on DEC */
        int locked = lock_find(0, 0, P_DEC) >= 0;
        m2k_button(B_OCTD, 1); edit(2, 1); m2k_button(B_OCTD, 0);
        int unlocked = lock_find(0, 0, P_DEC) < 0 && q->step[0][0].rtrg <= 1;
        m2k_key(0, 0);
        ui.grid = 0;
        m2k_pattern_t fr, fd;
        pat_init(&fr, "X");
        pat_init(&fd, "X");
        kit_init(&fr, 0, q->machine[0]);              /* home: the machine's INIT (A01's kick: DEC 42), not kit_default's */
        kit_default(&fd, 0, q->machine[0]);
        q->kit[0][P_DEC] = (uint8_t)(fr.kit[0][P_DEC] ^ 40);
        int o0 = ui.oct[ui.trk];
        m2k_button(B_OCTD, 1); edit(2, 1); m2k_button(B_OCTD, 0);
        int home = q->kit[0][P_DEC] == fr.kit[0][P_DEC] && fr.kit[0][P_DEC] != fd.kit[0][P_DEC] && ui.oct[ui.trk] == o0;
        m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0);
        int oct = ui.oct[ui.trk] == o0 - 1;
        CHECK(tt && cd && locked && unlocked && home && oct,
              "a held step: black 1-5 its trig type, 6-11 its condition; OCT- + knob: the lock away, else the INIT's value; OCT- alone the octave");
    }
    /* a key's release lets go of the note its press started, whatever is held by then (OCT+ tapped meanwhile, SEQ or
     * GLO held), and the keys let go of their notes when they become steps (SEQ tapped) or pads (GLO + ARP), or their
     * track changes in GRID (a note from MIDI held): no note drones, and the track's sequence plays on */
    {
        ux_init(7);
        int k0 = ux_white(0);
        m2k_key(k0, 1); ux_tap(B_OCTU); m2k_key(k0, 0);
        int oct = sq.nheld == 0;
        ux_init(7); m2k_key(k0, 1); m2k_button(B_SEQ, 1); m2k_key(k0, 0); m2k_button(B_SEQ, 0);
        int seq = sq.nheld == 0;
        ux_init(7); m2k_key(k0, 1); m2k_button(B_GLO, 1); m2k_key(k0, 0); m2k_button(B_GLO, 0);
        int glo = sq.nheld == 0;
        ux_init(7); m2k_key(k0, 1); m2k_button(B_GLO, 1); ux_tap(B_ARP); m2k_button(B_GLO, 0); m2k_key(k0, 0);
        int pads = ui.pads && sq.nheld == 0;
        ux_init(7); ux_tap(B_SEQ); m2k_midi(0x90, 60, 100); m2k_key(ux_black(2), 1); m2k_key(ux_black(2), 0);
        m2k_midi(0x80, 60, 0);
        int trk3 = ui.grid && sq.ktrk == 2 && sq.nheld == 0;
        ux_init(7); m2k_key(k0, 1); ux_tap(B_SEQ);
        int grid = ui.grid;
        m2k_key(k0, 0);
        grid &= sq.nheld == 0;
        uint8_t t0 = mon.trig[0];
        seq_play(1); ux_audio(44100 * 2); seq_play(0);
        CHECK(oct && seq && glo && pads && trk3 && grid && (uint8_t)(mon.trig[0] - t0) > 0,
              "keys: a key let go after OCT+, under SEQ or GLO, into GRID or PADS, or a track picked in GRID: no note held on, the track plays");
    }
    /* what a press held is let go on its release under any layer: GRID, a step held, SEQ pressed (its locks cleared),
     * the step let go first; GLO + white 9 (solo T1), GLO let go first; a page key tapped and let go under SEQ (its
     * hold would have made the pattern 32 steps long) */
    {
        ux_init(7);
        ux_tap(B_SEQ);
        m2k_key(ux_white(0), 1); m2k_button(B_SEQ, 1); m2k_key(ux_white(0), 0); m2k_button(B_SEQ, 0);
        int nostep = ui.nheld == 0;
        ux_init(7);
        m2k_button(B_GLO, 1); m2k_key(ux_white(8), 1); m2k_button(B_GLO, 0); m2k_key(ux_white(8), 0);
        int nosolo = sq.solo == 0;
        ux_init(7);
        m2k_pattern(16); ux_ms(40); ux_tap(B_SEQ);
        int l0 = seq_len();
        m2k_key(ux_black(7), 1); ux_ms(100); m2k_button(B_SEQ, 1); m2k_key(ux_black(7), 0); ux_ms(900); m2k_button(B_SEQ, 0);
        CHECK(nostep && nosolo && seq_len() == l0 && l0 == 16,
              "layers: a step, a solo, a page key pressed before a layer and let go under it are let go (no step held, no solo, no length)");
    }
    /* GLO + SEQ held for INIT while a queued pattern opens: the ring stops, the pattern that opened is not made INIT;
     * the clear stays undoable on the pattern it cleared */
    {
        ux_init(7);
        PAT(0)->glob[G_LEN] = 16;
        int n0 = ux_trigs(0);
        seq_play(1); ux_audio(44100);
        select_pattern(1);
        char nm[8];
        memcpy(nm, PAT(1)->name, 8);
        m2k_button(B_GLO, 1); m2k_button(B_SEQ, 1);
        int sw = 0;
        for (int i = 0; i < 12; i++) { ux_audio(4410); ux_ms(100); sw |= cur_pat == 1; }
        m2k_button(B_SEQ, 0); m2k_button(B_GLO, 0);
        seq_play(0);
        int kept = !memcmp(PAT(1)->name, nm, 8) && strcmp(nm, "INIT");
        select_pattern(0); ux_ms(40);
        int cleared = ux_trigs(0) == 0;
        m2k_button(B_GLO, 1); m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0); m2k_button(B_GLO, 0);
        CHECK(sw && kept && cleared && ux_trigs(0) == n0 && n0 > 0,
              "INIT ring: a queued pattern opening during the hold stops it (A02 not made INIT); A01's clear undone on A01");
    }
    /* GRID: a trig held two seconds (its card read) and let go without an edit keeps the trig and its locks; a quick
     * tap still removes it */
    {
        ux_init(7);
        ux_tap(B_SEQ);
        m2k_pattern_t *q = PAT(cur_pat);
        int t = ui.trk, s = -1;
        for (int i = 0; i < 16 && s < 0; i++) if (q->step[t][i].flags & ST_TRIG) s = i;
        lock_set(t, s, P_DEC, 99);
        m2k_key(ux_white(s), 1); ux_ms(2000); m2k_key(ux_white(s), 0);
        int kept = (q->step[t][s].flags & ST_TRIG) && lock_find(t, s, P_DEC) >= 0;
        m2k_key(ux_white(s), 1); ux_ms(100); m2k_key(ux_white(s), 0);
        int tapped = !(q->step[t][s].flags & ST_TRIG) && lock_find(t, s, P_DEC) < 0;
        CHECK(s >= 0 && kept && tapped, "GRID: a trig held 2 s and let go keeps itself and its locks; a quick tap removes it");
    }
    /* a trig taken away takes its condition and ratchet with it: tapped again, the new trig is plain (100 %, no
     * ratchet); step entry's rest the same (the owner: "the probability should be removed too") */
    {
        ux_init(7);
        ux_tap(B_SEQ);
        m2k_pattern_t *q = PAT(cur_pat);
        int t = ui.trk, s = -1;
        for (int i = 0; i < 16 && s < 0; i++) if (q->step[t][i].flags & ST_TRIG) s = i;
        q->step[t][s].cond = C_P50; q->step[t][s].rtrg = 2;
        m2k_key(ux_white(s), 1); ux_ms(100); m2k_key(ux_white(s), 0);       /* a quick tap: removed */
        int gone = !(q->step[t][s].flags & ST_TRIG) && q->step[t][s].cond == 0 && q->step[t][s].rtrg == 0;
        ux_ms(400);
        m2k_key(ux_white(s), 1); ux_ms(100); m2k_key(ux_white(s), 0);       /* again: a plain trig */
        int plain = (q->step[t][s].flags & ST_TRIG) && q->step[t][s].cond == 0 && q->step[t][s].rtrg == 0;
        ux_tap(B_SEQ);                                                        /* KEYS; step entry and its rest */
        ux_ms(400);
        q->step[t][0].flags = ST_TRIG; q->step[t][0].cond = C_1_2; q->step[t][0].rtrg = 3;
        ux_tap(B_REC);
        ui.cursor = 0;
        ux_tap(B_EDIT);
        int rest = !(q->step[t][0].flags & ST_TRIG) && q->step[t][0].cond == 0 && q->step[t][0].rtrg == 0;
        ux_tap(B_REC);
        CHECK(s >= 0 && gone && plain && rest, "a trig taken away (GRID's tap, step entry's rest) takes its condition and ratchet with it");
    }
    /* a question does what it asked: REPLACE S001 answered YES after SLOT moved on saves the track's sound into S001,
     * S002 untouched; LOAD P2 answered YES after PROJ moved on loads P2 */
    {
        ux_init(7);
        ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1); ui.snd_sel = 0; ui.snd_cat = 0;
        m2k_sound_t s1 = snd[1];
        PAT(cur_pat)->kit[ui.trk][P_DEC] ^= 21;
        m2k_encoder(E_K3, 1);                                  /* SAVE over S001: asked */
        m2k_encoder(E_K1, 1);                                  /* SLOT on to S002 */
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);          /* YES */
        int sok = !memcmp(&snd[1], &s1, sizeof s1) && snd[0].kit[P_DEC] == PAT(cur_pat)->kit[ui.trk][P_DEC];
        ux_init(7);
        ui.page = PG_GLO; ui.sub = 3; ui.proj_sel = 1;
        m2k_encoder(E_K3, 1);                                  /* LOAD P2: asked */
        m2k_encoder(E_K2, 1);                                  /* PROJ on to P3 */
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);
        CHECK(sok && cur_proj == 1, "ask: YES does what was asked (S001 replaced after SLOT moved, S002 kept; P2 loaded after PROJ moved)");
    }
    /* again within 3 s undoes only on the same thing: a step pasted onto step 5 then onto step 9; a track's steps pasted
     * onto T2 then T3; REC + SEQ on T1 then T2; EDIT + SEQ on T1 then T2: each done, none undone */
    {
        ux_init(7);
        ux_tap(B_SEQ);
        m2k_pattern_t *q = PAT(cur_pat);
        int t = ui.trk;
        for (int i = 0; i < 16; i++) q->step[t][i].flags = 0;
        lockmap_build();
        q->step[t][0] = (m2k_step_t){.note = 70, .flags = ST_TRIG | ST_ACC};
        m2k_key(ux_white(0), 1); ux_tap(B_REC); m2k_key(ux_white(0), 0); ux_ms(200);
        m2k_key(ux_white(4), 1); ux_tap(B_PLAY); m2k_key(ux_white(4), 0); ux_ms(500);
        m2k_key(ux_white(8), 1); ux_tap(B_PLAY); m2k_key(ux_white(8), 0);
        int steps = q->step[t][4].note == 70 && q->step[t][8].note == 70 && (q->step[t][8].flags & ST_ACC);
        ux_init(7);
        q = PAT(cur_pat);
        m2k_track(0); m2k_button(B_SEQ, 1); ux_tap(B_REC); m2k_button(B_SEQ, 0); ux_ms(100);
        m2k_track(1); m2k_button(B_SEQ, 1); ux_tap(B_PLAY); m2k_button(B_SEQ, 0); ux_ms(500);
        m2k_track(2); m2k_button(B_SEQ, 1); ux_tap(B_PLAY); m2k_button(B_SEQ, 0);
        int tracks = !memcmp(q->step[1], q->step[0], sizeof q->step[0]) && !memcmp(q->step[2], q->step[0], sizeof q->step[0]);
        ux_init(7);
        int n1 = ux_trigs(0), n2 = ux_trigs(1);
        m2k_button(B_REC, 1); ux_tap(B_SEQ); m2k_button(B_REC, 0); ux_ms(200);
        m2k_encoder(E_SELECT, 1); ux_ms(200);
        m2k_button(B_REC, 1); ux_tap(B_SEQ); m2k_button(B_REC, 0);
        int recs = n1 > 0 && n2 > 0 && ux_trigs(0) == 0 && ux_trigs(1) == 0;
        ux_init(7);
        q = PAT(cur_pat);
        uint8_t k1[P_COUNT], k2[P_COUNT];
        memcpy(k1, q->kit[0], P_COUNT); memcpy(k2, q->kit[1], P_COUNT);
        m2k_button(B_EDIT, 1); ux_tap(B_SEQ); m2k_button(B_EDIT, 0); ux_ms(200);
        m2k_encoder(E_SELECT, 1); ux_ms(200);
        m2k_button(B_EDIT, 1); ux_tap(B_SEQ); m2k_button(B_EDIT, 0);
        int snds = memcmp(k1, q->kit[0], P_COUNT) && memcmp(k2, q->kit[1], P_COUNT);
        CHECK(steps && tracks && recs && snds,
              "again on another step or track is done, not an undo (step paste 5 then 9; SEQ + PLAY T2, T3; REC + SEQ, EDIT + SEQ T1, T2)");
    }
    /* a message's lines are cut to their buffers: HOME + a bank key's line (32 letters once) and one far too long no
     * longer run into the lanes' state (they flashed, the text garbled) */
    {
        ux_init(7);
        int same = !memcmp(ui.seen_trig, mon.trig, sizeof ui.seen_trig);
        m2k_button(B_HOME, 1); m2k_key(ux_black(1), 1); m2k_key(ux_black(1), 0);
        int bank = !memcmp(ui.seen_trig, mon.trig, sizeof ui.seen_trig) && strlen(ui.toast2) < sizeof ui.toast2;
        m2k_button(B_HOME, 0);
        toast("A MESSAGE FAR TOO LONG FOR ITS LINE", "AND ITS SECOND LINE TOO LONG AS WELL");
        int cut = !memcmp(ui.seen_trig, mon.trig, sizeof ui.seen_trig) && strlen(ui.toast) == sizeof ui.toast - 1 &&
                  strlen(ui.toast2) == sizeof ui.toast2 - 1;
        ux_ms(40);
        CHECK(same && bank && cut, "toast: its lines cut to fit (HOME + a bank key's, one far too long): the lanes' state intact");
    }
    /* a knob a layer gave and the page has not (DIGI-WAV's BANK page has no KNOB 3; PERFORM's is ADPC) is no focus once
     * the layer is let go: the page shows its own, not a setting read past the settings (SCREEN, out of bounds) */
    {
        ux_init(7);
        ui.trk = 0; set_machine(M_WAV); ui.page = PG_SYN; ui.sub = 2;
        ux_ms(2000); m2k_button(B_FX, 1); ux_ms(500);
        m2k_encoder(E_K3, 1);                                  /* PERFORM's ADPC */
        m2k_button(B_FX, 0);
        m2k_frame(ux_now += 20);
        int other = 0;
        for (int i = 0; i < ta_n; i++) if (!strcmp(ta[i].s, "SCREEN") || !strcmp(ta[i].s, "LIGHT") || !strcmp(ta[i].s, "DARK")) other = 1;
        CHECK(knob_at(2) == NONE && !other && knob_value(NONE) == 0,
              "focus: a knob the page has not (after PERFORM's) is not shown, nor read past the settings");
    }
    /* LFO held: a knob turned snaps back on the pattern it was turned on, though the queued one opened meanwhile */
    {
        ux_init(7);
        PAT(0)->glob[G_LEN] = 16; PAT(1)->glob[G_LEN] = 16;
        ui.page = PG_AMP; ui.sub = 0; ui.trk = 0;
        int a0 = PAT(0)->kit[0][P_ATK], b0 = PAT(1)->kit[0][P_ATK];
        seq_play(1); ux_audio(4410);
        select_pattern(1);
        m2k_button(B_LFO, 1); ux_ms(40);
        m2k_encoder(E_K1, 30);
        int moved = PAT(0)->kit[0][P_ATK] != a0;
        for (int i = 0; i < 50 && cur_pat != 1; i++) { ux_audio(4410); ux_ms(100); }
        int opened = cur_pat == 1;
        m2k_button(B_LFO, 0);
        seq_play(0);
        CHECK(moved && opened && PAT(0)->kit[0][P_ATK] == a0 && PAT(1)->kit[0][P_ATK] == b0,
              "LFO held: the knob snaps back on the pattern it was turned on, the one queued meanwhile untouched");
    }
    /* the undo stays with its project: a clear, then another project loaded (the web's picker) or the store, and
     * GLO + OCT- has nothing to undo (it brought the other project's pattern in) */
    {
        ux_init(7);
        m2k_button(B_GLO, 1); m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0); m2k_button(B_GLO, 0);
        m2k_project(1, 0);
        m2k_button(B_GLO, 1); m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0); m2k_button(B_GLO, 0);
        int proj = !strcmp(PAT(0)->name, "INIT") && !strcmp(ui.toast, "NOTHING TO UNDO");
        ux_init(7);
        m2k_button(B_GLO, 1); m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0); m2k_button(B_GLO, 0);
        int n = m2k_store_pack();
        int loaded = m2k_store_unpack(n) == 1;
        m2k_button(B_GLO, 1); m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0); m2k_button(B_GLO, 0);
        CHECK(proj && loaded && !strcmp(ui.toast, "NOTHING TO UNDO") && ux_trigs(0) == 0,
              "undo: nothing to undo after another project or the store is loaded");
    }
    /* a fresh start (m2k_init: the web's reset) keeps no undo of the patterns before it, nor a question */
    {
        ux_init(7);
        m2k_button(B_GLO, 1); m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0); m2k_button(B_GLO, 0);
        ui.page = PG_GLO; ui.sub = 3; ui.proj_sel = 1;
        m2k_encoder(E_K3, 1);                                  /* LOAD P2: asked */
        ux_init(7);
        int noask = ask.kind == ASK_NONE;
        m2k_button(B_GLO, 1); m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0); m2k_button(B_GLO, 0);
        CHECK(noask && !strcmp(ui.toast, "NOTHING TO UNDO"), "init: no undo, no question left from before it");
    }
    /* GLO + OCT- (undo), GLO let go first: OCT-'s own release does nothing (it turned the octave) */
    {
        ux_init(7);
        int o0 = ui.oct[ui.trk];
        m2k_button(B_GLO, 1); m2k_button(B_OCTD, 1); m2k_button(B_GLO, 0); m2k_button(B_OCTD, 0);
        CHECK(ui.oct[ui.trk] == o0, "GLO + OCT-, GLO let go first: the octave stays");
    }
    /* the drawing's byte-wise fills and type (rect, frame, a glyph a byte at a time) as a pixel at a time draws them:
     * random boxes and texts, in ink, paper and inverted, in each size, cut by random clips and the screen's edges */
    {
        uint32_t r = 12345u;
        int same = 1;
        #define RN(n) ((int)(((r = r * 1103515245u + 12345u) >> 8) % (uint32_t)(n)))
        static const m2k_font_t *const FT[4] = {&F_S, &F_M, &F_L, &F_S};
        static const char *const TX[4] = {"WAVE 12", "S&H -3 X", "#%/;", "AW MQ"};
        for (int i = 0; i < 3000 && same; i++) {
            for (int b = 0; b < M2K_FB_BYTES; b++) fb[b] = ux_ref[b] = (uint8_t)RN(256);
            int cx = RN(240) / 2, cy = RN(240) / 2, cw = RN(241 - cx), ch = RN(241 - cy);
            clip(cx, cy, cw, ch);
            int x = RN(280) - 20, y = RN(280) - 20;
            int c = RN(3);
            if (i & 1) {
                int w = RN(90) - 4, h = RN(70) - 4;
                if (i & 2) {                                   /* (a frame: its four sides as frame() puts them) */
                    frame(x, y, w, h, c);
                    ux_rect(x, y, w, 1, c); ux_rect(x, y + h - 1, w, 1, c);
                    ux_rect(x, y + 1, 1, h - 2, c); ux_rect(x + w - 1, y + 1, 1, h - 2, c);
                } else { rect(x, y, w, h, c); ux_rect(x, y, w, h, c); }
            } else {
                const m2k_font_t *f = FT[RN(4)];
                const char *s = TX[RN(4)];
                int tr = RN(3) - 1, xx = x;
                text(f, x, y, s, c, tr);
                for (const char *p = s; *p; p++) {
                    int g = glyph_of(f, (unsigned char)*p);
                    for (int gy = 0; gy < f->h; gy++)
                        for (int gx = 0; gx < f->w[g]; gx++) if (glyph_bit(f, g, gx, gy)) ux_px(xx + gx, y + gy, c);
                    xx += f->w[g] + f->track + tr;
                }
            }
            if (memcmp(fb, ux_ref, M2K_FB_BYTES)) { same = 0; printf("     differs: case %d\n", i); }
        }
        #undef RN
        noclip();
        CHECK(same, "drawing: rects, frames and type a byte at a time are what a pixel at a time draws, clipped alike");
    }
    /* the drawing's cost: GLO's map held over the AMP page, and HOME's 96 cells, set few pixels one at a time (a
     * window's fill, the type and the empty cells were each a pixel at a time: 1.5 M instructions a GLO frame) */
    {
        ux_init(7);
        ui.page = PG_AMP; ui.sub = 0;
        m2k_button(B_GLO, 1); ux_ms(600);
        px_calls = 0; m2k_frame(ux_now += 20);
        uint32_t glo = px_calls;
        m2k_button(B_GLO, 0);
        ui.page = PG_HOME; ux_ms(600);
        px_calls = 0; m2k_frame(ux_now += 20);
        uint32_t home = px_calls;
        printf("     pixels drawn one at a time: a GLO frame %u, HOME %u\n", (unsigned)glo, (unsigned)home);
        CHECK(glo < 6000 && home < 6000, "drawing: a GLO frame and HOME draw their fills and type a byte at a time");
    }
    /* live recording: a knob turned on a track at 1/2x locks the track's own step, not the pattern's */
    {
        ux_init(7);
        m2k_pattern(16); ux_ms(40);
        m2k_pattern_t *q = PAT(16);
        q->glob[G_SPD1] = enc(2, 7);
        for (int s = 0; s < 16; s++) { q->step[0][s].flags = ST_TRIG; q->step[0][s].note = 60; }
        lockmap_build();
        ui.trk = 0; sq.ktrk = 0; ui.page = PG_AMP; ui.sub = 0;
        seq_play(1); sq.rec = 1;
        while (sq.step < 9) ux_audio(16);
        int tp = track_pos(0), ps = sq.step - 1;
        m2k_encoder(E_K1, 5);
        seq_play(0); sq.rec = 0;
        CHECK(tp != ps && lock_find(0, tp, P_ATK) >= 0 && lock_find(0, ps, P_ATK) < 0,
              "REC: a knob on a 1/2x track locks the track's own step");
    }
    /* SYN's pages follow the machine: ALGO off DIGI-WAV on its SND page, SELECT from it onto a track without BANK, a
     * DIGI-WAV sound loaded on SND: the library stays in view, never a page past the last; one turn of SAVE into an
     * INIT slot saves once (its next detent does not ask to replace what it just saved) */
    {
        ux_init(7);
        ui.trk = 0; set_machine(M_WAV); ui.page = PG_SYN; ui.sub = 3;
        m2k_encoder(E_ALGO, 1);
        int algo = PAT(cur_pat)->machine[0] != M_WAV && ui.sub < nsub(PG_SYN) && snd_page();
        PAT(cur_pat)->machine[1] = M_GSIN;
        set_machine(M_WAV); ui.page = PG_SYN; ui.sub = 3;
        m2k_encoder(E_SELECT, 1);
        int sel = ui.trk == 1 && ui.sub < nsub(PG_SYN) && snd_page();
        ux_init(7);
        ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1); ui.snd_cat = 0; ui.snd_sel = M2K_SOUNDS - 1;
        int init = !snd[M2K_SOUNDS - 1].name[0];
        m2k_encoder(E_K3, 1); ux_ms(40); m2k_encoder(E_K3, 1);
        int once = init && snd[M2K_SOUNDS - 1].name[0] && ask.kind == ASK_NONE;
        ask.kind = ASK_NONE;
        CHECK(algo && sel && once, "SYN: the SND page stays SND as the machine or the track changes; SAVE into INIT saves once a turn");
    }
    /* GLO held two seconds alone (its map read) and let go: the page stays; EDIT held as a modifier (+ REC) flips no
     * page, tapped it is SYN; GRID: OCT+ stops at the pattern's last page; PADS: a black key picks the keys' track too;
     * GRID's step LEDs and the view follow the track's own step (1/2x) */
    {
        ux_init(7);
        ui.page = PG_AMP; ui.sub = 0;
        m2k_button(B_GLO, 1); ux_ms(2000); m2k_button(B_GLO, 0);
        int glo = ui.page == PG_AMP;
        m2k_button(B_EDIT, 1); ux_tap(B_REC); m2k_button(B_EDIT, 0);
        int edit_amp = ui.page == PG_AMP;
        ux_tap(B_EDIT);
        int s0 = ui.sub, edit_syn = ui.page == PG_SYN;
        m2k_button(B_EDIT, 1); ux_tap(B_REC); m2k_button(B_EDIT, 0);
        int edit_mod = ui.page == PG_SYN && ui.sub == s0;
        m2k_pattern(16); ux_ms(40);
        ux_tap(B_SEQ);
        ux_tap(B_OCTU); ux_tap(B_OCTU);
        int pages = ui.spage == 0;
        PAT(16)->glob[G_LEN] = 32;
        ux_tap(B_OCTU); ux_tap(B_OCTU); ux_tap(B_OCTU);
        pages &= ui.spage == 1;
        ux_init(7);
        m2k_button(B_GLO, 1); ux_tap(B_ARP); m2k_button(B_GLO, 0);
        m2k_key(ux_black(2), 1); m2k_key(ux_black(2), 0);
        int pads = ui.pads && ui.trk == 2 && sq.ktrk == 2;
        ux_init(7);
        m2k_pattern(16); ux_ms(40);
        PAT(16)->glob[G_LEN] = 32; PAT(16)->glob[G_SPD1] = enc(2, 7);
        ux_tap(B_SEQ);
        seq_play(1);
        while (sq.step < 20) ux_audio(16);
        m2k_frame(ux_now += 20);
        int tp = track_pos(0), ps = sq.step - 1;
        const uint8_t *L = m2k_leds();
        int follow = tp / 16 != ps / 16 && ui.spage == tp / 16 && L[ux_white(tp % 16)] == 255;
        seq_play(0);
        CHECK(glo && edit_amp && edit_syn && edit_mod && pages && pads && follow,
              "GLO held long: no page; EDIT + REC: no page, EDIT tapped: SYN; GRID OCT+ to the last page; PADS track; GRID follows the track's step");
    }
    /* a track's steps copied and pasted keep every lock (256 on T1, back onto itself); a paste the locks' pool cannot
     * hold says so; eight steps with ten locks each (80) copied and pasted keep them all */
    {
        ux_init(7);
        m2k_pattern_t *q = PAT(cur_pat);
        for (int t = 0; t < M2K_TRACKS; t++) for (int s = 0; s < M2K_STEPS; s++) q->step[t][s].flags = 0;
        for (int k = 0; k < M2K_LOCKS; k++) q->lock[k].trk = 0xFF;
        q->glob[G_LEN] = 64;
        for (int s = 0; s < 64; s++) { q->step[0][s].flags = ST_TRIG; for (int k = 0; k < 4; k++) lock_set(0, s, P_BASE + k, 10 + s); }
        lockmap_build();
        m2k_track(0); m2k_button(B_SEQ, 1); ux_tap(B_REC); m2k_button(B_SEQ, 0); ux_ms(100);
        m2k_button(B_SEQ, 1); ux_tap(B_PLAY); m2k_button(B_SEQ, 0);
        int back = ux_locks(0, 0, 64) == 256;
        ux_ms(3100);
        m2k_track(1); m2k_button(B_SEQ, 1); ux_tap(B_PLAY); m2k_button(B_SEQ, 0);
        int full = !strcmp(ui.toast, "LOCKS FULL");
        ux_init(7);
        ux_tap(B_SEQ);
        q = PAT(cur_pat);
        for (int s = 0; s < M2K_STEPS; s++) q->step[0][s].flags = 0;
        for (int k = 0; k < M2K_LOCKS; k++) q->lock[k].trk = 0xFF;
        q->glob[G_LEN] = 32;
        for (int s = 0; s < 8; s++) { q->step[0][s].flags = ST_TRIG; for (int k = 0; k < 10; k++) lock_set(0, s, P_BASE + k, 20 + k); }
        lockmap_build();
        for (int i = 0; i < 8; i++) m2k_key(ux_white(i), 1);
        ux_tap(B_REC);
        for (int i = 0; i < 8; i++) m2k_key(ux_white(i), 0);
        m2k_key(ux_white(8), 1); ux_tap(B_PLAY); m2k_key(ux_white(8), 0);
        int steps = ux_locks(0, 8, 16) == 80;
        CHECK(back && full && steps, "copy, paste: a track's 256 locks kept, a full pool says LOCKS FULL, 8 steps' 80 locks kept");
    }
    /* the render on two cores (the FM-1's): the second core's tracks run before or after the first's (as they may,
     * at once) and every pattern of bank A sounds sample for sample as on one */
    {
        static int16_t a[2 * 44100], b[2 * 44100];
        int same = 1, split_seen = 0;
        for (int p = 0; p < 16; p++) {
            for (int mode = 0; mode < 3; mode++) {
                par_mode = mode;
                m2k_par_begin = mode ? par_test_begin : 0;
                m2k_par_end = mode ? par_test_end : 0;
                m2k_init(21);
                m2k_pattern(p);
                seq_play(1);
                int16_t *o = mode ? b : a;
                for (int f = 0; f < 44100; f += 128) m2k_render(o + 2 * f, 128 < 44100 - f ? 128 : (44100 - f) & ~15);
                if (mode && memcmp(a, b, sizeof a)) { same = 0; printf("     A%02d differs with the second core's tracks %s\n", p + 1, mode == 1 ? "first" : "last"); }
                split_seen |= par_from < M2K_TRACKS;
            }
        }
        m2k_par_begin = 0; m2k_par_end = 0;
        CHECK(same && split_seen, "two cores: bank A the same sample for sample, the second core's tracks first or last");
    }
    /* NOTES OUT on two cores: six tracks' gates end in the same blocks on both; the second core's job queues no MIDI
     * (a gate's end is marked on its core, its note-off sent by the first after both) and the notes come out as on
     * one core, the same messages in the same order on the same samples */
    {
        static uint8_t ev[3][1024][3];
        static uint32_t evt[3][1024];
        int nev[3] = {0}, split = 0;
        par_queued = 0;
        for (int mode = 0; mode < 3; mode++) {
            par_mode = mode;
            m2k_par_begin = mode ? par_test_begin : 0;
            m2k_par_end = mode ? par_test_end : 0;
            m2k_init(22);
            m2k_pattern_t *q = bare(16);
            for (int t = 0; t < M2K_TRACKS; t++) {
                kit_default(q, t, M_GSIN);
                m_cpy(trk[t].eff, q->kit[t], P_COUNT);
                for (int s = 0; s < 16; s++) q->step[t][s] = (m2k_step_t){.note = (uint8_t)(48 + t), .flags = ST_TRIG};
            }
            m2k_setting(S_NOTO, 127);
            seq_play(1);
            for (int i = 0; i < 2 * 44100 / M2K_SUB; i++) {
                sblk();
                uint8_t m[3];
                while (nev[mode] < 1024 && m2k_midi_out(m, m2k_samples()) > 0) {
                    m_cpy(ev[mode][nev[mode]], m, 3);
                    evt[mode][nev[mode]++] = m2k_samples();
                }
                split |= par_from < M2K_TRACKS;
            }
        }
        m2k_par_begin = 0; m2k_par_end = 0;
        int same = nev[0] > 150 && nev[1] == nev[0] && nev[2] == nev[0];
        for (int mode = 1; mode < 3 && same; mode++)
            same = !memcmp(ev[mode], ev[0], (size_t)nev[0] * 3) && !memcmp(evt[mode], evt[0], (size_t)nev[0] * 4);
        CHECK(same && split && !par_queued, "two cores: NOTES OUT queued by the first core alone, as on one core (the second's gates marked, sent after)");
    }
    /* a pattern queued comes in at the bar's end: the last step before it plays on its own pattern (machine, kit and
     * locks), and a note recorded in it is written there; the new pattern's first step, its tracks off 1x with it,
     * at the step it comes in on, not one early */
    {
        m2k_init(23);
        m2k_pattern_t *b = bare(17), *a = bare(16);
        kit_default(b, 0, M_SAW);
        for (int p = 16; p < 18; p++) {
            for (int t = 0; t < 3; t++) pat[p].step[t][0].flags = ST_TRIG;
            pat[p].glob[G_SPD1 + 1] = enc(6, 7);              /* T2 2x */
            pat[p].glob[G_SPD1 + 2] = enc(2, 7);              /* T3 1/2x */
        }
        a->step[0][15] = (m2k_step_t){.note = 72, .flags = ST_TRIG};
        lock_set(0, 15, P_PTCH, 100);
        seq_play(1);
        sblk_sync();
        int own = 1, rec = 0, early = 0, with = 0, at = -1;
        uint32_t t15 = 0;
        for (int i = 0; i < 3 * 44100 / M2K_SUB && at < 0; i++) {
            if (i == 100) sq.queued = 17;
            int pc = cur_pat, m = sblk();
            if (cur_pat != pc) { at = (int)m2k_samples(); with = (m & 7) == 7; break; }
            if (t15 && (m & 6)) early = 1;
            if ((m & 1) && trk[0].note == 72) t15 = m2k_samples();
            if (t15) own &= trk[0].eff[P_PTCH] == 100 && PAT(cur_pat)->machine[0] == M_GSIN;
            if (t15 && !rec && m2k_samples() - t15 > 400) {      /* 10 ms into the last step */
                sq.ktrk = 3; sq.rec = 1;
                live_on(65, 100); live_off(65);
                sq.rec = 0; rec = 1;
            }
        }
        int wrote = rec && pat[16].step[3][15].note == 65 && (pat[16].step[3][15].flags & ST_TRIG);
        for (int s = 0; s < M2K_STEPS; s++) wrote &= !(pat[17].step[3][s].flags & ST_TRIG);
        CHECK(t15 && own && wrote && !early && with && abs(at - (int)(16 * 5512.5)) <= 2 * M2K_SUB,
              "sequencer: a pattern queued comes in on the bar's end, its last step on its own kit and locks (a note recorded there), the tracks off 1x with it");
    }
    /* a length made shorter while playing: nothing past the new end plays, the pattern starts over */
    {
        m2k_init(24);
        m2k_pattern_t *q = bare(16);
        q->glob[G_LEN] = 32;
        q->step[0][0].flags = ST_TRIG;
        q->step[0][19] = (m2k_step_t){.note = 77, .flags = ST_TRIG};
        seq_play(1);
        while (sq.step != 19) sblk();
        q->glob[G_LEN] = 16;
        sblk_sync();
        int past = 0, first = 0;
        for (int i = 0; i < 2000; i++) {
            int m = sblk();
            if ((m & 1) && trk[0].note == 77) past++;
            else if ((m & 1) && !first) first = sq.step;
        }
        CHECK(!past && first == 1, "sequencer: a length made shorter while playing: nothing past the new end, the first step next");
    }
    /* A:B and 1ST count each track's own loops (off 1x too) from the pattern's start: 2:2 plays every second pass of a
     * track at 1x, 1/2x, 2x and 1/4x; A for a loop, then B: B's 1ST plays in its first loop, its 1:2 there too */
    {
        m2k_init(25);
        m2k_pattern_t *q = bare(16);
        q->glob[G_TMPO] = 255;
        q->glob[G_LEN] = 4;
        for (int t = 0; t < 4; t++) q->step[t][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG, .cond = C_2_2};
        q->glob[G_SPD1 + 1] = enc(2, 7);              /* T2 1/2x */
        q->glob[G_SPD1 + 2] = enc(6, 7);              /* T3 2x */
        q->glob[G_SPD1 + 3] = enc(1, 7);              /* T4 1/4x */
        seq_play(1);
        sblk_sync();
        int n[4] = {0}, even = 1;
        uint32_t last3 = 0, gap3 = 0;
        for (int i = 0; i < (169344000 / 285 >> 8) * 4 * 32 / M2K_SUB - 4; i++) {   /* 32 loops of the pattern */
            int m = sblk();
            for (int t = 0; t < 4; t++) n[t] += m >> t & 1;
            if (m & 4) {
                uint32_t g = m2k_samples() - last3;
                if (last3 && gap3 && (g + 2 * M2K_SUB < gap3 || g > gap3 + 2 * M2K_SUB)) even = 0;
                if (last3) gap3 = g;
                last3 = m2k_samples();
            }
        }
        m2k_init(26);
        m2k_pattern_t *b = bare(17);
        b->step[0][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG, .cond = C_1ST};
        b->step[1][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG, .cond = C_1_2};
        b->step[2][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
        bare(16);
        for (int p = 16; p < 18; p++) { pat[p].glob[G_TMPO] = 255; pat[p].glob[G_LEN] = 4; }
        sq.queued = 17;
        seq_play(1);
        sblk_sync();
        char s1[9] = {0}, s2[9] = {0};
        for (int nb = 0; nb < 8;) {
            int m = sblk();
            if (m & 4) { s1[nb] = m & 1 ? 'X' : '.'; s2[nb] = m & 2 ? 'X' : '.'; nb++; }
        }
        printf("     2:2 in 32 loops: 1x %d, 1/2x %d, 2x %d, 1/4x %d; B's 1ST %s, 1:2 %s\n", n[0], n[1], n[2], n[3], s1, s2);
        CHECK(n[0] == 16 && n[1] == 8 && n[2] == 32 && n[3] == 4 && even && !strcmp(s1, "X.......") && !strcmp(s2, "X.X.X.X."),
              "sequencer: A:B and 1ST on each track's own loops (1x, 1/2x, 2x, 1/4x), counted again when a pattern comes in");
    }
    /* SYNC IN: the tracks off 1x keep to the pattern's steps on the clock's tempo, through a tempo change (a step of
     * theirs that starts with one of the pattern's plays with it); a start sets them going again, a continue from a
     * song position puts them where it is. The first pulse after boot only starts the timing */
    {
        m2k_init(27);
        for (int i = 0; i < 10; i++) sblk();
        m2k_setting(S_SYNI, 127);
        m2k_midi(0xF8, 0, 0);
        int boot = mx.per_q8 == 0;
        m2k_pattern_t *q = bare(16);
        for (int t = 0; t < 3; t++) q->step[t][0].flags = ST_TRIG;
        q->step[0][8].flags = ST_TRIG;
        q->glob[G_SPD1 + 1] = enc(6, 7);              /* T2 2x: its first step on the pattern's 0 and 8 */
        q->glob[G_SPD1 + 2] = enc(2, 7);              /* T3 1/2x: on the pattern's 0 every second loop */
        q->glob[G_SPD1 + 3] = enc(0, 7);              /* T4 1/8x, every step: each on the pattern's 0 or 8 (the
                                                       * continue lands mid-step: none till the 8) */
        q->glob[G_SPD1 + 4] = enc(3, 7);              /* T5 3/4x, every step; T6 1x, every step */
        for (int s = 0; s < 16; s++) q->step[3][s].flags = q->step[4][s].flags = q->step[5][s].flags = ST_TRIG;
        sblk_per = 44100.0 * 60 / (100 * 24);
        sblk_next = m2k_samples();
        int bad = 0, n1 = 0, ta = 0, tb = 0, pre5 = 0;
        for (int part = 0; part < 4; part++) {
            if (part == 0) m2k_midi(0xFA, 0, 0);
            if (part == 2) { m2k_midi(0xFC, 0, 0); for (int i = 0; i < 400; i++) sblk_ext(); m2k_midi(0xFA, 0, 0); }
            if (part == 3) {
                m2k_midi(0xFC, 0, 0);
                for (int i = 0; i < 400; i++) sblk_ext();
                m2k_midi(0xF2, 21, 0);                    /* the song at step 21: the pattern's 5, T5 a quarter into its 15 */
                m2k_midi(0xFB, 0, 0);
            }
            if (part == 1) sblk_per = 44100.0 * 60 / (137 * 24);
            sblk_sync();
            for (int i = 0; i < 10 * 44100 / M2K_SUB; i++) {
                int m = (i & 7) ? sblk() : sblk_ext();     /* (the pulses come in every 128 samples) */
                n1 += m & 1;
                if (!(m & 1) != !(m & 2) || !(m & 1) != !(m & 8) || ((m & 4) && !(m & 1))) bad++;
                if (part == 3 && !ta && (m & 16)) pre5 = 1;
                if (part == 3 && !ta && (m & 32)) ta = (int)m2k_samples();
                if (part == 3 && ta && !tb && (m & 16)) tb = (int)m2k_samples();
            }
        }
        m2k_setting(S_SYNI, 0);
        printf("     the clock in: %d of T1's trigs, %d blocks where T2, T3 or T4 missed it\n", n1, bad);
        int third = !pre5 && ta && abs(tb - ta - (int)(44100.0 * 60 / (137 * 4) / 3)) <= 2 * M2K_SUB;   /* its 16 */
        CHECK(boot && n1 > 35 && !bad && third, "SYNC IN: the tracks off 1x on the pattern's steps (a tempo change, a start again, a continue); the first pulse only times");
    }
    /* a GATE lock: the step's gate its own, the kit's on the next */
    {
        m2k_init(28);
        m2k_pattern_t *q = bare(16);
        q->step[0][0].flags = ST_TRIG;
        q->step[0][4].flags = ST_TRIG;
        lock_set(0, 0, P_GATE, 127);
        seq_play(1);
        sblk();
        int32_t g0 = trk[0].gate_left;
        while (sq.step != 5) sblk();
        int32_t g4 = trk[0].gate_left;
        seq_play(0);
        CHECK(g4 > 0 && g0 > 8 * g4, "sequencer: a GATE lock is its step's gate (127: eight steps), the kit's on the next trig");
    }
    /* NOTES OUT: a note's end goes out on the channel its start did (CHAN turned meanwhile); a note played over a
     * step's (MIDI in on its track, the keys) ends the step's as it starts; so does a MIDI note-off of the step's
     * note, and STOP: none left hanging, none waiting for the next trig */
    {
        int hung = 0;
        for (int c = 0; c < 5; c++) {
            m2k_init(29);
            m_set(mo_bal, 0, sizeof mo_bal);
            m2k_pattern_t *q = bare(16);
            q->step[1][0] = (m2k_step_t){.note = 50, .flags = ST_TRIG};
            q->kit[1][P_GATE] = 64;                       /* four steps */
            m2k_setting(S_NOTO, 127);
            sq.ktrk = c == 2 ? 1 : 0;
            seq_play(1);
            for (int i = 0; i < 10; i++) sblk();
            if (c == 0) m2k_setting(S_MCH, 4 * 8 + 4);   /* CHAN 1 -> 5 */
            if (c == 1 || c == 2) {
                if (c == 1) m2k_midi(0x91, 64, 100); else live_on(64, 100);
                for (int i = 0; i < 20; i++) sblk();
                mo_drain();
                hung += mo_bal[1][50] != 0;               /* the step's ended with it */
                if (c == 1) m2k_midi(0x81, 64, 0); else live_off(64);
            }
            if (c == 3) m2k_midi(0x81, 50, 0);           /* its own note let go by MIDI in */
            if (c == 4) seq_play(0);
            for (int i = 0; i < 8 * 5512 / M2K_SUB; i++) sblk();    /* past the gate, before the next bar */
            mo_drain();
            hung += mo_hung();
        }
        CHECK(!hung, "NOTES OUT: a note ends on its own channel (CHAN turned), as a note is played over it (MIDI in, keys), let go by MIDI, at STOP");
    }
    /* a ratchet's hits stop with STOP and in a track muted; a start keeps nothing of the last play (a step recorded
     * ahead, a fill, a jump queued, PRE) */
    {
        m2k_init(30);
        m2k_pattern_t *q = bare(16);
        q->glob[G_TMPO] = 0;                              /* 30 BPM: a step half a second */
        q->step[0][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG, .rtrg = 4};
        seq_play(1);
        sblk_sync();
        int first = sblk() & 1, after = 0, muted = 0;
        for (int i = 0; i < 100; i++) sblk();
        seq_play(0);
        for (int i = 0; i < 44100 / M2K_SUB; i++) after += sblk() & 1;
        seq_play(1);
        sblk();
        q->mute = 1;
        for (int i = 0; i < 1300; i++) muted += sblk() & 1;
        seq_play(0);
        q->mute = 0;
        bare(17);
        bare(16)->step[0][0].flags = ST_TRIG;
        q->step[1][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG, .cond = C_PRE};
        sq.rec_skip[0] = 0; sq.fill = 1; sq.fill_auto = 1; sq.pre[1] = 1;
        sq.queued = 17; sq.jump = 1;
        seq_play(1);
        int m = sblk();
        int fresh = (m & 1) && !(m & 2) && !sq.fill && sq.rec_skip[0] == -1 && cur_pat == 16;
        for (int i = 0; i < 2000; i++) sblk();
        fresh &= cur_pat == 16;                           /* (the queued one at the bar's end) */
        seq_play(0);
        CHECK(first && !after && !muted && fresh, "sequencer: a ratchet stops with STOP and muted; a start clears a step recorded ahead, a fill, a jump, PRE");
    }
    /* MIDI: the clock in's stop passed on with its start and pulses; the clock out six pulses to a step exactly, so a
     * step's note still goes out with its pulse after a minute (at a tempo whose step is no whole number of sixths) */
    {
        m2k_init(31);
        bare(16);
        m2k_setting(S_SYNI, 127); m2k_setting(S_SYNO, 127);
        m2k_midi(0xFA, 0, 0);
        for (int i = 0; i < 12; i++) { m2k_midi(0xF8, 0, 0); for (int k = 0; k < 69; k++) sblk(); }
        m2k_midi(0xFC, 0, 0);
        int fa = 0, f8 = 0, fc = 0, k;
        uint8_t m[3];
        while (m2k_midi_out(m, m2k_samples()) > 0) { fa += m[0] == 0xFA; f8 += m[0] == 0xF8; fc += m[0] == 0xFC; }
        m2k_init(32);
        m2k_pattern_t *q = bare(16);
        int v = 255;
        while (v > 200 && (169344000u / (30u + (unsigned)v)) % 6u != 5u) v--;
        q->glob[G_TMPO] = (uint8_t)v;
        for (int s = 0; s < 16; s++) q->step[0][s].flags = ST_TRIG;
        m2k_setting(S_SYNO, 127); m2k_setting(S_NOTO, 127);
        seq_play(1);
        static uint32_t nt[2048], pt[2048];
        int nn = 0, np = 0;
        long pulses = 0;
        for (int i = 0; i < 60 * 44100 / M2K_SUB; i++) {
            sblk();
            while ((k = m2k_midi_out(m, m2k_samples())) > 0) {
                if (m[0] == 0xF8 && pulses++ % 6 == 0 && np < 2048) pt[np++] = m2k_samples();
                if (k == 3 && (m[0] & 0xF0) == 0x90 && nn < 2048) nt[nn++] = m2k_samples();
            }
        }
        seq_play(0);
        int with = nn > 1000 && np >= nn - 1;
        for (int i = 0; i < nn && i < np; i++) with &= nt[i] == pt[i];
        CHECK(fa == 1 && f8 == 12 && fc == 1 && with, "MIDI: the clock in's stop passed on; the clock out six pulses to a step exactly (a step's note with its pulse)");
    }
    /* REC: a note played just after a step is written to it, under the clock in too (on the step expected), and on a
     * track off 1x to its own step; one played just before the next, to the next (not played twice) */
    {
        int at[3] = {-1, -1, -1}, skip[3] = {0, 0, 0};
        for (int c = 0; c < 3; c++) {
            int ext = c != 1;
            m2k_init(33);
            m2k_pattern_t *q = bare(16);
            if (c == 1) q->glob[G_SPD1] = enc(2, 7);      /* T1 1/2x: the pattern's step 4 is its 2 */
            sq.ktrk = 0;
            sblk_per = 44100.0 * 60 / (120 * 24);
            sblk_next = m2k_samples();
            if (ext) {
                m2k_setting(S_SYNI, 127);
                for (int i = 0; i < 400; i++) sblk_ext();
                m2k_midi(0xFA, 0, 0);
            } else seq_play(1);
            sq.rec = 1;
            while (sq.step != 5) sblk_by(ext);            /* the pattern's step 4 just played */
            for (int i = 0; i < (c == 2 ? 300 : 28); i++) sblk_by(ext);   /* 10 ms, or 109 of its 125 */
            live_on(62, 100);
            for (int s = 0; s < M2K_STEPS; s++) if (q->step[0][s].flags & ST_TRIG) at[c] = s;
            skip[c] = sq.rec_skip[0];
            live_off(62);
            sq.rec = 0;
            seq_play(0);
            m2k_setting(S_SYNI, 0);
        }
        CHECK(at[0] == 4 && skip[0] == -1 && at[1] == 2 && skip[1] == -1 && at[2] == 5 && skip[2] == 5,
              "REC: a note just after a step written to it, under the clock in and off 1x on the track's own step; just before, the next");
    }
    /* the arp: its notes as far apart as ASPD shows (1/32: half a step), and a cycle of three keeps its order past its
     * 256th note */
    {
        m2k_init(35);
        m2k_pattern_t *q = bare(16);
        q->kit[0][P_AMOD] = enc(1, 6);                    /* UP */
        q->kit[0][P_ASPD] = enc(0, 4);                    /* 1/32 */
        sq.ktrk = 0;
        sblk();
        live_on(60, 100); live_on(64, 100); live_on(67, 100);
        sblk_sync();
        uint32_t first = 0, last = 0;
        int n = 0, twice = 0, prev = -1;
        while (n < 300) {
            if (!(sblk() & 1)) continue;
            if (!n) first = m2k_samples();
            last = m2k_samples();
            twice += trk[0].note == prev;
            prev = trk[0].note;
            n++;
        }
        live_off(60); live_off(64); live_off(67);
        double per = (double)(last - first) / (n - 1);
        printf("     the arp at 1/32, 120 BPM: a note every %.1f samples (2756.25 a 1/32)\n", per);
        CHECK(fabs(per - 2756.25) < 2 && !twice, "arp: ASPD's rate as shown (1/32 at 120: 62.5 ms), and three notes in order past the 256th");
    }
    /* the arp's HOLD: ARP once is its page, as before; twice holds the arp, so let go it plays on; a chord played after
     * the hands left is the new one; twice again it stops; a sustain pedal (CC 64) on the track's channel does the same;
     * another track takes the keys and the held arp stops */
    {
        ux_init(36);
        m2k_pattern_t *q = PAT(cur_pat);
        int t = ui.trk;
        q->kit[t][P_AMOD] = enc(1, 6);                    /* UP */
        q->kit[t][P_ASPD] = enc(0, 4);                    /* 1/32 */
        m_cpy(trk[t].eff, q->kit[t], P_COUNT);
        ux_tap(B_ARP);
        int page = ui.page == PG_ARP && !sq.arp_hold;
        ux_ms(400);
        ux_tap(B_ARP); ux_tap(B_ARP);
        int on = sq.arp_hold;
        int k0 = ux_white(0), k1 = ux_white(2), k2 = ux_white(4);
        m2k_key(k0, 1); m2k_key(k1, 1); ux_audio(4096); m2k_key(k0, 0); m2k_key(k1, 0);
        int n0 = mon.trig[t];
        ux_audio(M2K_SR / 2);
        int plays = (uint8_t)(mon.trig[t] - n0) >= 6 && sq.nheld == 2 && sq.nkeys == 0;
        m2k_key(k2, 1);
        int fresh = sq.nheld == 1;
        m2k_key(k2, 0);
        int kept = sq.nheld == 1;
        ux_ms(400);
        ux_tap(B_ARP); ux_tap(B_ARP);
        int off = !sq.arp_hold && sq.nheld == 0;
        n0 = mon.trig[t];
        ux_audio(M2K_SR / 4);
        int stops = (uint8_t)(mon.trig[t] - n0) == 0;
        int ch = (midi_ch() + t) & 15;
        m2k_midi(0xB0 | ch, 64, 127);
        m2k_key(k0, 1); m2k_key(k0, 0);
        int pedal = sq.arp_hold && sq.nheld == 1;
        m2k_midi(0xB0 | ch, 64, 0);
        int pedal_up = !sq.arp_hold && sq.nheld == 0;
        m2k_midi(0xB0 | ch, 64, 127);
        m2k_key(k0, 1); m2k_key(k0, 0);
        track_to((t + 1) % M2K_TRACKS);
        int moved = sq.nheld == 0;
        CHECK(page && on && plays && fresh && kept && off && stops && pedal && pedal_up && moved,
              "arp: ARP twice holds it (let go, it plays on; a new chord replaces it; twice again, it stops); CC 64 alike; another track stops it");
    }
    /* a speed turned while playing: the track goes on from where the pattern is (T2 turned from 1x to 2x on step 6:
     * its first step again on the pattern's 8, then with T1's 16) */
    {
        m2k_init(36);
        m2k_pattern_t *q = bare(16);
        q->step[0][0].flags = ST_TRIG;
        q->step[1][0].flags = ST_TRIG;
        seq_play(1);
        sblk_sync();
        uint32_t t2[4] = {0};
        int n2 = 0, with = 0;
        for (int i = 0; i < 5 * 44100 / M2K_SUB && n2 < 4; i++) {
            int m = sblk();
            if (sq.step == 7 && q->glob[G_SPD1 + 1] == SPD_1X) q->glob[G_SPD1 + 1] = enc(6, 7);
            if (m & 2) t2[n2++] = m2k_samples();
            with += (m & 3) == 3;
        }
        seq_play(0);
        CHECK(n2 == 4 && abs((int)t2[1] - (int)(8 * 5512.5)) <= 2 * M2K_SUB && abs((int)t2[2] - (int)(16 * 5512.5)) <= 2 * M2K_SUB && with == 2,
              "speeds: a track's speed turned while playing goes on from where the pattern is");
    }
    /* the amp (format 16, the owner's laws): the time shown is the time heard on the output, ATK to full and DEC and
     * REL to -60 dB (within two blocks and 3 %); the attack swells, half its level only after 0.62 of its time (a
     * square of the time: 0.71; a straight rise is there at 0.5); the knob spent where the times are: ATK about 20 ms at
     * 16, 67 ms at 32, 0.4 s at 64, 2.1 s at 96, 10 s at 127; DEC and REL 0.8 s at 64, 19 s at 126, REL 20 s at 127 */
    {
        static int32_t lv[M2K_SR * 12 / M2K_SUB];
        const double blk = 1000.0 * M2K_SUB / M2K_SR;           /* ms */
        int ok = 1;
        static const int AV[5] = {16, 32, 64, 96, 127};
        static const double AT[5] = {21, 67, 400, 2080, 10000};
        for (int i = 0; i < 5; i++) {
            int a = AV[i], n = (int)(T_AATK_MS10[a] / 10.0 / blk) + 400;
            double shown = T_AATK_MS10[a] / 10.0;
            amp_probe(a, 127, 127, 0, -1, -1, lv, n);
            int32_t full = lv[n - 1];
            double t_full = amp_when(lv, n, 0, 1, 0.99, full) * blk, t_half = amp_when(lv, n, 0, 1, 0.5, full) * blk;
            int good = fabs(t_full - shown) <= 2 * blk + 0.03 * shown && t_half >= 0.62 * shown && fabs(shown / AT[i] - 1) < 0.1;
            if (!good) printf("     ATK %d: shown %.1f ms, full at %.1f, half at %.1f\n", a, shown, t_full, t_half);
            ok &= good;
        }
        static const int DV[3] = {32, 64, 96};
        for (int i = 0; i < 3; i++) {
            int d = DV[i], n = (int)(T_ADEC_MS10[d] / 10.0 / blk * 1.2) + 50;
            double shown = T_ADEC_MS10[d] / 10.0;
            amp_probe(0, 0, d, 0, -1, -1, lv, n);
            int pk = 0;
            for (int j = 0; j < 20; j++) if (lv[j] > lv[pk]) pk = j;
            double t60 = (amp_when(lv, n, pk, 0, 1e-3, lv[pk]) - pk) * blk;
            amp_probe(0, 127, 127, d, 20, -1, lv, n + 20);
            double r60 = (amp_when(lv, n + 20, 20, 0, 1e-3, lv[19]) - 20) * blk;
            int good = fabs(t60 - shown) <= 2 * blk + 0.03 * shown && fabs(r60 - shown) <= 2 * blk + 0.03 * shown;
            if (!good) printf("     DEC / REL %d: shown %.1f ms, heard %.1f / %.1f\n", d, shown, t60, r60);
            ok &= good;
        }
        ok &= fabs(T_ADEC_MS10[64] / 8020.0 - 1) < 0.05 && fabs(T_ADEC_MS10[126] / 190000.0 - 1) < 0.05 &&
              fabs(T_ADEC_MS10[127] / 200000.0 - 1) < 0.05;
        CHECK(ok, "AMP: the time shown is the time heard (ATK to full, DEC and REL to -60 dB), the attack swelling, on the "
                  "owner's laws (ATK 0.4 s at 64, 10 s at 127; DEC and REL 0.8 s at 64, 20 s at 127)");
        /* a trig over a sounding note: an attack goes on from where the note is (no step down); ATK 0 starts from
         * zero, the percussive edge, as before */
        int n = 260, cont = 1;
        amp_probe(12, 127, 127, 30, 60, 160, lv, n);                    /* ATK 14 ms, REL 0.12 s: -18 dB at the trig */
        for (int j = 160; j < n - 1; j++) cont &= lv[j + 1] >= lv[j] * 99 / 100;
        CHECK(cont && lv[159] < lv[59] / 2 && lv[n - 1] > lv[59] * 98 / 100,
              "AMP: a trig on a releasing note rises from where it is to full, no step down (ATK 12)");
    }
    /* the LFOs (format 16): DPTH squared about 64, fine near the middle (72 swings a quarter octave of BASE, two steps:
     * format 15's 72 swung sixteen), full at the ends; the modulation glides between steps on what reads whole steps
     * (VOL PAN LPQ HPQ DEC: a half step lands between); SPD a cycle in 2^((84 - SPD) / 12) beats at x1, as shown */
    {
        int32_t sw[3];
        static const int DP[3] = {72, 0, 127};
        for (int i = 0; i < 3; i++) {
            m2k_init(42);
            trk_t *t = &trk[0];
            m_set(t->eff, 0, P_COUNT);
            t->eff[P_LFO + L_DEST] = P_PAN + 1;
            t->eff[P_LFO + L_WAVE] = enc(2, 7);                        /* a square: its whole swing at once */
            t->eff[P_LFO + L_DPTH] = (uint8_t)DP[i];
            modn = 0;
            lfo_block(0);
            sw[i] = modq[0].v < 0 ? -modq[0].v : modq[0].v;
            modn = 0;
        }
        CHECK(abs(sw[0] - 512) < 16 && abs(sw[1] - 128 * 256) < 64 && abs(sw[2] - 126 * 256) < 64,
              "LFO: DPTH 72 swings two steps (format 15's: sixteen), 0 and 127 the whole range");
        /* a half step of modulation: VOL's gain, PAN's, LPQ's and HPQ's filters, DEC's fall between their whole steps */
        int32_t vq[3], pq[3], lq[3], hq[3];
        uint32_t dq[3];
        for (int i = 0; i < 3; i++) {
            m2k_pattern_t *q = eng_pat(43);
            uint8_t *k = q->kit[0];
            k[P_HOLD] = 127; k[P_DEC] = 127; k[P_VOL] = 60; k[P_PAN] = 40; k[P_LPQ] = 40; k[P_HPQ] = 40;
            k[P_BASE] = 20; k[P_WDTH] = 40;
            m_cpy(trk[0].eff, k, P_COUNT);
            static const int P[4] = {P_VOL, P_PAN, P_LPQ, P_HPQ};
            for (int j = 0; j < 4; j++) trk[0].mod[P[j]] = i * 128;   /* (no LFO: it stays) */
            voice_on(0, 60, 100, 0);
            m2k_render(eng_out, M2K_SUB);
            m2k_render(eng_out, M2K_SUB);
            int32_t l = 0, r = 0;
            for (int j = 0; j < M2K_SUB; j++) { l += abs(eng_out[2 * j]); r += abs(eng_out[2 * j + 1]); }
            vq[i] = l + r; pq[i] = (int32_t)((int64_t)l * 4096 / (r + 1));
            lq[i] = trk[0].lp1.k; hq[i] = trk[0].hp2.k;
            trk[0].mod[P_DEC] = i * 128;
            trk[0].eff[P_DEC] = 60; trk[0].astage = 3; trk[0].aenv = 1u << 30;
            env_block(&trk[0]);
            dq[i] = trk[0].aenv;
        }
        #define BETWEEN(a) (((a)[1] > (a)[0] && (a)[1] < (a)[2]) || ((a)[1] < (a)[0] && (a)[1] > (a)[2]))
        CHECK(BETWEEN(vq) && BETWEEN(pq) && BETWEEN(lq) && BETWEEN(hq) && BETWEEN(dq),
              "LFO: half a step of modulation lands between the steps (VOL, PAN, LPQ, HPQ, DEC): it glides, no zipper");
        #undef BETWEEN
        /* SPD: 12 at x1 a cycle in 16 bars (32 s at 120 BPM), 60 a bar, 60 at x16 a sixteenth, 0 still; the screen says so
         * (the LFOs' clock 1.45 % slow, as format 15's: its 32, 16 at x1 a bar and 64 at x16 converted exactly) */
        int good = 1;
        static const int SV[4] = {12, 60, 60, 0}, MU[4] = {0, 0, 4, 0};
        static const double BEATS[4] = {64, 4, 0.25, 0};
        static const char *const SHOWN[4] = {"16 BARS", "1 BAR", "1/16 NOTE", "STOP STILL"};
        for (int i = 0; i < 4; i++) {
            m2k_init(44);
            trk_t *t = &trk[0];
            m_set(t->eff, 0, P_COUNT);
            t->eff[P_LFO + L_DEST] = P_PAN + 1;
            t->eff[P_LFO + L_SPD] = (uint8_t)SV[i];
            t->eff[P_LFO + L_MULT] = enc(MU[i], 8);
            t->eff[P_LFO + L_DPTH] = 72;
            bpm10 = 1200;
            uint32_t ph0 = t->lph[0];
            int nb = 0;
            uint64_t moved = 0;
            while (nb < 2000000 && moved < 4294967296ull) { uint32_t a = t->lph[0]; lfo_block(0); modn = 0; moved += (uint32_t)(t->lph[0] - a); nb++; }
            double beats = nb * (double)M2K_SUB / M2K_SR * 2.0;
            char o[24], w[40];
            const char *u;
            s_lfo_cycle(o, &u, SV[i], MU[i]);
            snprintf(w, sizeof w, "%s %s", o, u);
            int g = !strcmp(w, SHOWN[i]) && (SV[i] ? fabs(beats / (BEATS[i] * 1.01449) - 1) < 0.002 : t->lph[0] == ph0);
            if (!g) printf("     SPD %d x%d: %.3f beats a cycle, shown %s\n", SV[i], 1 << MU[i], beats, w);
            good &= g;
        }
        good &= T_SPD15[32] == 48 && T_SPD15[16] == 36 && T_SPD15[64] == 60 && T_LCYC[48] == 8116;   /* 8.116 beats: format 15's 32 */
        CHECK(good, "LFO: SPD 12 cycles in 16 bars at x1, 60 in a bar, 60 at x16 in a sixteenth, 0 stands, as the screen says;"
                    " format 15's speeds kept (32 as 48, 64 as 60)");
    }
    /* LFO ONE and HALF: a whole cycle and half of one from the trig's phase (PHAS), whatever it is */
    {
        int ok = 1;
        for (int mode = 3; mode <= 4; mode++)
            for (int ph = 0; ph <= 96; ph += 32) {
                m2k_init(37);
                trk_t *t = &trk[0];
                m_set(t->eff, 0, P_COUNT);
                t->eff[P_LFO + L_DEST] = 1;
                t->eff[P_LFO + L_TRIG] = enc(mode, 5);
                t->eff[P_LFO + L_SPD] = 64;
                t->eff[P_LFO + L_PHAS] = (uint8_t)ph;
                t->eff[P_LFO + L_DPTH] = 127;
                lfo_trig(t);
                double inc = lfo_inc(64 * 256, 0, bpm10) / 4294967296.0, want = mode == 3 ? 1.0 : 0.5;
                int n = 0;
                while (!t->ldone[0] && n < 100000) { lfo_block(0); modn = 0; n++; }
                ok &= n * inc >= want && n * inc < want + inc * 1.01;
            }
        CHECK(ok, "LFO: ONE runs a cycle, HALF half of one, from the trig's phase (PHAS)");
    }
    /* the sound engine: an audit's finds, each case failing before its fix */
    {
        /* glides: a slide at PORT 0 (glide 40) up a fifth lands on its note, as one down does; PORT 110 an octave up
         * gets there (the step's size rounded down: the first stopped 30 cents flat, the second never moved) */
        int land[2], oct;
        for (int d = 0; d < 2; d++) {
            m2k_pattern_t *q = eng_pat(60);
            kit_default(q, 0, M_GSIN);
            q->kit[0][P_HOLD] = 127; q->kit[0][P_GATE] = 127;
            q->step[0][0] = (m2k_step_t){.note = (uint8_t)(d ? 55 : 48), .flags = ST_TRIG};
            q->step[0][1] = (m2k_step_t){.note = (uint8_t)(d ? 48 : 55), .flags = ST_TRIG | ST_SLD};
            eng_go(q);
            eng_run(0.5);
            land[d] = trk[0].pitch == (d ? 48 : 55) * 64;
        }
        {
            m2k_pattern_t *q = eng_pat(61);
            kit_default(q, 0, M_GSIN);
            q->kit[0][P_HOLD] = 127; q->kit[0][P_GATE] = 127; q->kit[0][P_PORT] = 110;
            q->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
            q->step[0][1] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
            eng_go(q);
            eng_run(0.6);
            oct = trk[0].pitch == 60 * 64;
        }
        CHECK(land[0] && land[1] && oct, "glide: a slide up a fifth lands on its note as one down does; PORT 110 goes an octave up");
    }
    {
        /* another machine under a sounding track starts from rest, as a trig starts it: FM+PAR held on T2 (T1 a saw, the
         * FX machines' input) turned to each machine as SET MACHINE does, its first blocks against the same turn with the
         * state cleared by hand (it ran on the last machine's state: overflows, full-scale blocks) */
        static int16_t o[2][4 * M2K_SUB];
        int same = 1;
        for (int b = 0; b < M_COUNT; b++) {
            if (b == M_FMP) continue;
            for (int k = 0; k < 2; k++) {
                m2k_pattern_t *q = eng_pat(62);
                kit_default(q, 0, M_SAW);
                kit_default(q, 1, M_FMP);
                q->kit[0][P_HOLD] = q->kit[1][P_HOLD] = 127; q->kit[0][P_GATE] = q->kit[1][P_GATE] = 127;
                q->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
                q->step[1][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
                eng_go(q);
                eng_run(0.3);
                q->machine[1] = (uint8_t)b;
                m_cpy(q->kit[1], MACH_DEF[b], 8);
                m_cpy(trk[1].eff, q->kit[1], 8);
                if (k) {
                    trk_t *t = &trk[1];
                    m_set(t->ph, 0, sizeof t->ph); m_set(t->ms, 0, sizeof t->ms); m_set(t->gn, 0, sizeof t->gn);
                    m_set(t->vo, 0, sizeof t->vo); m_set(&t->vohp, 0, sizeof t->vohp); t->vo_pw = 0; t->vo_g = 0;
                    m_set(ens_mem[1], 0, sizeof ens_mem[1]);
                    mach_trig(t, b);
                }
                for (int j = 0; j < 4; j++) { m2k_render(eng_out, M2K_SUB); m_cpy(&o[k][j * M2K_SUB], blk[1], sizeof blk[1]); }
            }
            if (memcmp(o[0], o[1], sizeof o[0])) { same = 0; printf("     FM+PAR turned to machine %d under its sound runs on the old state\n", b); }
        }
        /* FM+STAT turned to FX-DYNMX: the FM's states read as its gain made a full-scale block, then seconds of silence */
        m2k_pattern_t *q = eng_pat(63);
        kit_default(q, 0, M_SAW);
        kit_default(q, 1, M_FMS);
        q->kit[0][P_HOLD] = q->kit[1][P_HOLD] = 127; q->kit[0][P_GATE] = q->kit[1][P_GATE] = 127;
        q->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
        q->step[1][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
        eng_go(q);
        eng_run(0.5);
        q->machine[1] = M_FXD;
        m_cpy(q->kit[1], MACH_DEF[M_FXD], 8);
        m_cpy(trk[1].eff, q->kit[1], 8);
        m2k_render(eng_out, M2K_SUB);
        int pk = 0, in = 0, heard = 0;
        for (int i = 0; i < M2K_SUB; i++) { pk = abs(blk[1][i]) > pk ? abs(blk[1][i]) : pk; in = abs(tout[0][i]) > in ? abs(tout[0][i]) : in; }
        for (int j = 0; j < M2K_SR / 20 / M2K_SUB; j++) { m2k_render(eng_out, M2K_SUB); for (int i = 0; i < M2K_SUB; i++) heard |= abs(blk[1][i]) > 200; }
        /* and at a pattern's change, before the new pattern's first trig: no full-scale hit (FM+STAT's states read as
         * DRM-SNAR's, DRM-HATS's, DRM-CYMB's gains) */
        static const uint8_t SW[3][2] = {{M_FMS, M_DSNR}, {M_FMS, M_DHAT}, {M_FMS, M_DCYM}};
        int full = 0;
        for (int j = 0; j < 3; j++) {
            m2k_init(64);
            for (int k = 0; k < 2; k++) {
                m2k_pattern_t *x = &pat[41 + k];
                pat_init(x, "SW");
                kit_default(x, 0, SW[j][k]);
                for (int s = 0; s < 16; s += 2) x->step[0][s] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
            }
            m2k_pattern(41);
            seq_play(1);
            eng_run(1.0);
            m2k_pattern(42);
            for (int f = 0; f < M2K_SR * 3 / 2 / M2K_SUB; f++) {
                m2k_render(eng_out, M2K_SUB);
                if (cur_pat == 42 && sq.step < 1) for (int i = 0; i < M2K_SUB; i++) full += abs(blk[0][i]) >= 32767;
            }
        }
        CHECK(same && pk <= 4 * in + 100 && heard && !full,
              "machines: another one under a sounding track starts from rest (SET MACHINE, a pattern's change), as a trig starts it");
    }
    {
        /* 64-bit products where a loud signal passes 2^31 (wrapped round: full-scale clicks): GND-PERC's METAL against a
         * thinned noise (its mix never past the metal's 32400), DRM-SNAR's gain on its filtered noise (no jump from full
         * scale to full scale), the phaser's allpasses on a loud track (held to the same in 64 bits throughout) */
        m2k_pattern_t *q = eng_pat(65);
        kit_default(q, 0, M_GPRC);
        q->kit[0][P_SYN + 0] = 96; q->kit[0][P_SYN + 1] = 0; q->kit[0][P_SYN + 2] = 127;
        q->kit[0][P_HOLD] = 127; q->kit[0][P_GATE] = 127;
        for (int s = 0; s < 16; s += 4) q->step[0][s] = (m2k_step_t){.note = (uint8_t)(36 + 3 * s), .flags = ST_TRIG};
        eng_go(q);
        int gp = 0;
        for (int f = 0; f < M2K_SR * 2 / M2K_SUB; f++) { m2k_render(eng_out, M2K_SUB); for (int i = 0; i < M2K_SUB; i++) gp = abs(blk[0][i]) > gp ? abs(blk[0][i]) : gp; }
        int flips = 0;
        for (int note = 36; note <= 72; note += 12) {
            q = eng_pat(66);
            kit_default(q, 0, M_DSNR);
            q->kit[0][P_SYN + 3] = 16; q->kit[0][P_SYN + 4] = 127; q->kit[0][P_SYN + 5] = 96;
            for (int s = 0; s < 16; s += 2) q->step[0][s] = (m2k_step_t){.note = (uint8_t)note, .flags = ST_TRIG};
            eng_go(q);
            int prev = 0;
            for (int f = 0; f < M2K_SR * 2 / M2K_SUB; f++) {
                m2k_render(eng_out, M2K_SUB);
                for (int i = 0; i < M2K_SUB; i++) { flips += abs(blk[0][i] - prev) >= 65000; prev = blk[0][i]; }
            }
        }
        static int16_t line[MOD_LEN];
        m2k_init(67);
        trk_t *t = &trk[0];
        uint32_t mph = 0;
        int32_t map[4] = {0}, mfb = 0, side;
        int ph_same = 1;
        for (int i = 0; i < M2K_SR; i++) {
            int32_t x = ((i & 1) ? 90000 : -90000) * (((i / 3) & 1) ? -1 : 1);      /* near Nyquist, a driven track's 2.7 x full */
            int32_t y = mod_tick(t, line, x, 2, T_MOD_INC[64], 127 * 206, 254, &side);   /* (MODD 127, the phaser's scale) */
            ph_same &= y == eng_phaser(&mph, map, &mfb, x, T_MOD_INC[64], 127, 254);
        }
        CHECK(gp <= 32401 && !flips && ph_same, "64-bit products: GND-PERC's METAL, DRM-SNAR's gain, the phaser's allpasses, on loud signals");
    }
    {
        /* the delay goes quiet after a hit under unity feedback (its one-poles stalled up to 252 off zero, and mu-law's
         * rounding gave an echo DFB / 64 as loud back the level it had, for ever): the band wide open, DFB 32, 48, 60 */
        int quiet = 1;
        static const uint8_t FB[3] = {32, 48, 60};
        for (int j = 0; j < 3; j++) {
            m2k_pattern_t *q = eng_pat(68);
            kit_default(q, 0, M_SAW);
            q->kit[0][P_DSND] = 0; q->kit[0][P_DFB] = FB[j]; q->kit[0][P_DBAS] = 0; q->kit[0][P_DWID] = 127;
            q->kit[0][P_DTIM] = 16; q->kit[0][P_DEC] = 40;
            q->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
            eng_go(q);
            eng_run(0.5);
            seq_play(0);
            int k = 0;
            for (; k < 20 * M2K_SR / M2K_SUB && !eng_dly_quiet(); k++) m2k_render(eng_out, M2K_SUB);
            if (!eng_dly_quiet()) { quiet = 0; printf("     the delay at DFB %d still runs 20 s on\n", FB[j]); }
        }
        CHECK(quiet, "delay: a tail under unity feedback ends, its line silent and its filters at rest");
    }
    {
        /* the reverb ends in silence, no DC held (the old combs ran negative and kept -9331 for ever): a GND-NOIS hit into
         * DCAY 127 DAMP 127 (24 s), DCAY 60 DAMP 96 and DCAY 1 DAMP 0; its lines empty and asleep within three of its
         * RT60s (from a hit at -3 dBFS to a step), the output at zero */
        int dc = 0;
        static const uint8_t VB[3][2] = {{127, 127}, {60, 96}, {1, 0}};
        for (int j = 0; j < 3; j++) {
            m2k_pattern_t *q = eng_pat(69);
            q->glob[G_VERB] = VB[j][0]; q->glob[G_DAMP] = VB[j][1];
            kit_default(q, 0, M_GNOI);
            q->kit[0][P_SPU] = 127; q->kit[0][P_DEC] = 40; q->kit[0][P_LEVL] = 127;
            q->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
            eng_go(q);
            eng_run(0.5);
            seq_play(0);
            double lim = 3 * T_RV_MS[VB[j][0]] / 1000.0 + 2, t = 0;
            for (; t < lim && !bus.rsleep; t += (double)M2K_SUB / M2K_SR) m2k_render(eng_out, M2K_SUB);
            eng_run(0.1);
            dc |= !bus.rsleep || eng_out[2 * M2K_SUB - 2] || eng_out[2 * M2K_SUB - 1];
            printf("     reverb DCAY %d DAMP %d: silent and asleep after %.1f s\n", VB[j][0], VB[j][1], t);
        }
        CHECK(!dc, "reverb: its tail ends at zero, its lines empty within three RT60s (no DC held)");
    }
    {
        /* a held PERFORM repeat keeps repeating (the ring went on being written, over its slice after 0.74 s): a
         * quarter's repeat held over a line of notes, the bus dry, its output a slice's period apart the same from 1.2 s */
        m2k_pattern_t *q = eng_pat(70);
        q->glob[G_VERB] = q->glob[G_TAPE] = q->glob[G_ADPC] = q->glob[G_HISS] = q->glob[G_GLCH] = 0;
        kit_default(q, 0, M_SAW);
        for (int s = 0; s < 16; s++) q->step[0][s] = (m2k_step_t){.note = (uint8_t)(40 + (s * 5) % 17), .flags = ST_TRIG};
        eng_go(q);
        eng_run(0.5);
        bus_perform(PF_R4, 1);
        eng_run(0.7);
        int per = 2 * (int)bus.g_len, n = M2K_SR / M2K_SUB * M2K_SUB, ok = per > 0 && per < n;
        int16_t *rec = malloc((size_t)n * 4);
        for (int f = 0; f < n; f += M2K_SUB) m2k_render(rec + 2 * f, M2K_SUB);
        for (int i = per; ok && i < n; i++) ok = rec[2 * i] == rec[2 * (i - per)] && rec[2 * i + 1] == rec[2 * (i - per) + 1];
        free(rec);
        bus_perform(PF_R4, 0);
        CHECK(ok, "PERFORM: a held repeat goes on repeating its slice");
    }
    {
        /* format 17: an old pattern's bus turns on load. The sends straight: VERB 64 to 32 (the reverb off: no level made
         * up), a lock of it too, DSND 80 to 68, a lock of 48 to 60, 0 and 127 as they were, a send of 1 still on; GLCH by
         * its table. At a node of the reverb's grid (old DCAY 127 DAMP 0: +3 dB) the node's pair, each send with its
         * level; between four nodes (old 72 72) half of each; today's loads as it is */
        static m2k_pattern_t w, w2;
        static uint8_t wb[BLOB_MAX], wb2[BLOB_MAX];
        m_set(&w, 0, sizeof w);
        pat_init(&w, "OLD17");
        w.glob[G_VERB] = 0; w.glob[G_DAMP] = 50; w.glob[G_GLCH] = 30;
        w.kit[0][P_SPU] = 64; w.kit[1][P_DSND] = 80; w.kit[2][P_DSND] = 0; w.kit[3][P_DSND] = 127; w.kit[4][P_SPU] = 1;
        lk(&w, 0, 4, P_SPU, 64);
        lk(&w, 1, 4, P_DSND, 48);
        int wn = pat_pack_v(&w, wb, 15), ok = pat_unpack(&w2, wb, wn), lks = 0;
        for (int i = 0; i < M2K_LOCKS; i++) {
            if (w2.lock[i].trk == 0 && w2.lock[i].param == P_SPU) lks += w2.lock[i].val == 32;
            if (w2.lock[i].trk == 1 && w2.lock[i].param == P_DSND) lks += w2.lock[i].val == 60;
        }
        ok = ok && w2.glob[G_VERB] == 0 && w2.kit[0][P_SPU] == 32 && w2.kit[1][P_DSND] == 68 && w2.kit[2][P_DSND] == 0 &&
             w2.kit[3][P_DSND] == 126 && w2.kit[4][P_SPU] == 1 && w2.glob[G_GLCH] == T_V17_GLCH[30] && lks == 2;
        CHECK(ok, "format: version 15 -> 17, the sends straight (VERB 64 -> 32, its lock too, DSND 80 -> 68, a lock 48 -> 60), GLCH by its table");
        /* a version-16 pattern takes 17's bus alone: today's A01 packed as 16 loads with its amp and LFOs as they are */
        static m2k_pattern_t x, y;
        m2k_init(23);
        x = pat[0];
        x.glob[G_GLCH] = 40;
        wn = pat_pack_v(&x, wb, 16);
        int same16 = pat_unpack(&y, wb, wn) && y.glob[G_GLCH] == T_V17_GLCH[40];
        for (int t = 0; t < M2K_TRACKS; t++) {
            same16 &= !memcmp(&y.kit[t][P_ATK], &x.kit[t][P_ATK], 4);
            for (int l = 0; l < 3; l++) same16 &= y.kit[t][P_LFO + 8 * l + L_SPD] == x.kit[t][P_LFO + 8 * l + L_SPD] &&
                                                 y.kit[t][P_LFO + 8 * l + L_DPTH] == x.kit[t][P_LFO + 8 * l + L_DPTH];
        }
        /* and the bank before is turned at build onto 17's bus too: A03 HAZE, written at DCAY 74 DAMP 80 */
        m_set(&x, 0, sizeof x);
        for (int i = 0; i < M2K_LOCKS; i++) x.lock[i].trk = 0xFF;
        x.glob[G_VERB] = 74; x.glob[G_DAMP] = 80;
        fx_v16(&x);
        m2k_init(24);
        factory_rev9();
        CHECK(same16 && pat[2].glob[G_VERB] == x.glob[G_VERB] && pat[2].glob[G_DAMP] == x.glob[G_DAMP],
              "format: version 16 takes 17's bus alone (its amp and LFOs as they are); the bank before turned onto 17's at build");
        w.glob[G_VERB] = 127; w.glob[G_DAMP] = 0;
        wn = pat_pack_v(&w, wb, 16);
        ok = pat_unpack(&w2, wb, wn) && w2.glob[G_VERB] == T_V17_RV[72][0] && w2.glob[G_DAMP] == T_V17_RV[72][1] &&
             w2.kit[0][P_SPU] == spu_v16(64, T_V17_RVL[72]) && w2.kit[0][P_SPU] != 32;
        w.glob[G_VERB] = 72; w.glob[G_DAMP] = 72;
        wn = pat_pack_v(&w, wb, 16);
        int dc = (T_V17_RV[40][0] + T_V17_RV[41][0] + T_V17_RV[49][0] + T_V17_RV[50][0] + 2) / 4;
        int dm = (T_V17_RV[40][1] + T_V17_RV[41][1] + T_V17_RV[49][1] + T_V17_RV[50][1] + 2) / 4;
        ok = ok && pat_unpack(&w2, wb, wn) && abs(w2.glob[G_VERB] - dc) <= 1 && abs(w2.glob[G_DAMP] - dm) <= 1;
        wn = pat_pack(&w2, wb);
        int wn2 = pat_unpack(&w, wb, wn) ? pat_pack(&w, wb2) : 0;
        CHECK(ok && wn2 == wn && !memcmp(wb, wb2, (size_t)wn),
              "format: 17's reverb pair at a node of its grid and between four, each send by its level; today's loads as it is");
    }
    {
        /* the reverb's length and colour (m2k_host --bus's measure): 0.3 s at DCAY 1, past 18 s at 127, 2 .. 3.3 s at
         * 64; DAMP 0 bright (the top's RT60 at 4 kHz past 0.8 of the bottom's at 250 Hz), 127 dark (under a quarter) */
        double a[13], b[13], c[13], d[13], e[13];
        verb_at(1, 0, 3, a);
        verb_at(127, 0, 16, b);
        verb_at(64, 32, 4, c);
        verb_at(100, 0, 8, d);
        verb_at(100, 127, 8, e);
        printf("     reverb RT60: DCAY 1 %.2f s, 64 %.2f s, 127 %.1f s; at 100 the top against the bottom: DAMP 0 %.2f, 127 %.2f\n",
               a[0], c[0], b[2], d[5] / d[1], e[5] / e[1]);
        CHECK(a[0] < 0.45 && b[2] > 18 && c[0] > 2 && c[0] < 3.3 && d[5] > 0.8 * d[1] && e[5] < 0.25 * e[1],
              "reverb: 0.3 s to past 18 s; DAMP from bright (the top as long) to dark (under a quarter)");
    }
    {
        /* the sends straight: a held sine's reverb at VERB 32 is 12 dB under VERB 127's (it was 24, the square); the
         * delay's first echo of a DRM-HATS hit at DSND 80 is 12 dB under DSND 127's (25 dB under the hit, it was) */
        double r[2];
        static int16_t o[2][2 * 2 * M2K_SR];
        static const uint8_t SV[2] = {32, 127};
        for (int k = 0; k < 2; k++) {
            for (int z = 0; z < 2; z++) {
                m2k_pattern_t *q = eng_pat(73);
                q->glob[G_VERB] = 64; q->glob[G_DAMP] = 0;
                kit_default(q, 0, M_GSIN);
                q->kit[0][P_HOLD] = 127; q->kit[0][P_DEC] = 127; q->kit[0][P_GATE] = 127; q->kit[0][P_SPU] = z ? SV[k] : 0;
                q->step[0][0] = (m2k_step_t){.note = 69, .flags = ST_TRIG};
                eng_go(q);
                for (int f = 0; f < 2 * M2K_SR / M2K_SUB * M2K_SUB; f += M2K_SUB) m2k_render(o[z] + 2 * f, M2K_SUB);
            }
            double s = 0;
            for (int i = M2K_SR; i < 4 * M2K_SR; i++) { double dd = (double)o[1][i] - o[0][i]; s += dd * dd; }
            r[k] = s;
        }
        double rv = 10 * log10(r[0] / r[1]), ec[2], hit = 0;
        static const uint8_t DV[2] = {80, 127};
        for (int k = 0; k < 2; k++) {
            m2k_pattern_t *q = eng_pat(74);
            q->glob[G_VERB] = 0;
            kit_default(q, 0, M_DHAT);
            q->kit[0][P_DTIM] = 32; q->kit[0][P_DFB] = 64; q->kit[0][P_DSND] = DV[k];
            q->step[0][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
            eng_go(q);
            int n = M2K_SR / M2K_SUB * M2K_SUB;
            for (int f = 0; f < n; f += M2K_SUB) m2k_render(o[0] + 2 * f, M2K_SUB);
            double pk = 0, h = 0;
            for (int i = 0; i < 2 * n; i++) {
                double v = fabs((double)o[0][i]);
                if (i < 2 * M2K_SR / 10) h = v > h ? v : h;
                else if (i >= 2 * (int)(0.15 * M2K_SR) && i < 2 * (int)(0.45 * M2K_SR)) pk = v > pk ? v : pk;
            }
            ec[k] = pk;
            hit = h;
        }
        printf("     sends: VERB 32 against 127 %.1f dB; DSND 80's first echo %.1f dB under DSND 127's, %.1f under the hit\n",
               rv, 20 * log10(ec[1] / ec[0]), 20 * log10(hit / ec[0]));
        CHECK(fabs(rv + 12) < 1.5 && fabs(20 * log10(ec[1] / ec[0]) - 12) < 1.5 && 20 * log10(hit / ec[0]) < 16,
              "sends: straight in level, VERB and DSND (a hat's echo at DSND 80 well heard, 12 dB under 127's)");
    }
    {
        /* GLCH low is subtle: at 5, over 64 bars of the reference mix, its glitch 35 dB under the mix (each repeat whole
         * was 16 under), every event leaving the dry whole with the wet 15 dB under it at most; and it draws its own dice:
         * the trigs on chances play the same with GLCH 0 and 100 */
        const int frames = 64 * 2 * M2K_SR / 16 * 16;
        int16_t *dry = malloc((size_t)frames * 4), *b = malloc((size_t)frames * 4);
        bus_ref(1, G_GLCH, 0, 64);
        for (int f = 0; f < frames; f += 16) m2k_render(dry + 2 * f, 16);
        bus_ref(1, G_GLCH, 5, 64);
        int whole = 1, events = 0, last = 0;
        for (int f = 0; f < frames; f += 16) {
            m2k_render(b + 2 * f, 16);
            if (bus.g_on && !last) { events++; whole &= bus.g_dry == 32767 && bus.g_wet <= 5833; }
            last = bus.g_on;
        }
        double e = 0, ed = 0;
        for (int i = 0; i < 2 * frames; i++) { double dd = (double)b[i] - dry[i]; e += dd * dd; ed += (double)dry[i] * dry[i]; }
        free(dry);
        free(b);
        printf("     GLCH 5: %d events in 64 bars, the glitch %.1f dB under the mix\n", events, -10 * log10(e / ed + 1e-30));
        CHECK(events > 0 && whole && 10 * log10(e / ed + 1e-30) < -35, "GLCH: low is subtle (rare, under the dry, quiet)");
        static uint8_t seen[2][4096];
        for (int k = 0; k < 2; k++) {
            m2k_init(75);
            m2k_pattern_t *q = &pat[0];
            q->glob[G_GLCH] = k ? 100 : 0;
            for (int t = 0; t < 4; t++) for (int s = 0; s < 16; s++) q->step[t][s] = (m2k_step_t){.note = 60, .flags = ST_TRIG, .cond = C_P50};
            cur_pat = 0; sq.queued = 0;
            lockmap_build();
            for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, q->kit[t], P_COUNT);
            seq_play(1);
            uint8_t tr[M2K_TRACKS];
            m_cpy(tr, mon.trig, sizeof tr);
            for (int f = 0; f < 4096; f++) {               /* (4096 blocks: 1.5 s, six bars' trigs at 120) */
                m2k_render(eng_out, M2K_SUB);
                seen[k][f] = 0;
                for (int t = 0; t < 4; t++) if (mon.trig[t] != tr[t]) { seen[k][f] |= (uint8_t)(1u << t); tr[t] = mon.trig[t]; }
            }
        }
        CHECK(!memcmp(seen[0], seen[1], sizeof seen[0]), "GLCH: its own dice (the trigs' chances play alike with it up)");
    }
    {
        /* every glitch and PERFORM change crossfades: on a held 440 Hz sine, no sample step larger than 1.3 x the sine's
         * own (a hard cut or a seam's jump is up to twice its swing a step: 1.3 x catches it); GLCH 10 (plain repeats)
         * over 48 bars, and each PERFORM key pressed mid-beat and let go a beat later, against the louder of the sine's
         * and the effect's own steps (a crush's stairs, a drive's edge) */
        const int frames = 48 * 2 * M2K_SR / 16 * 16;
        int16_t *b = malloc((size_t)frames * 4);
        double ds = 0, gs = 0;
        for (int k = 0; k < 2; k++) {
            m2k_pattern_t *q = eng_pat(76);
            q->glob[G_VERB] = 0; q->glob[G_GLCH] = k ? 10 : 0;
            sine_track(q);
            eng_go(q);
            sine_on();
            int on = 0;
            for (int f = 0; f < frames; f += 16) { m2k_render(b + 2 * f, 16); on += bus.g_on; }
            double m = step_max(b, M2K_SR, frames);
            if (k) gs = m; else ds = m;
            if (k && !on) gs = 1e9;
        }
        int bad = gs > 1.3 * ds;
        printf("     GLCH 10 on a sine: its largest step %.2f x the sine's\n", gs / ds);
        double worst = 0;
        int wk = -1;
        for (int k = 0; k < PF_COUNT; k++) {
            m2k_pattern_t *q = eng_pat(77);
            q->glob[G_VERB] = 0;
            sine_track(q);
            eng_go(q);
            sine_on();
            int n = 3 * M2K_SR / 16 * 16, at = (int)(1.1 * M2K_SR) / 16 * 16, off = at + M2K_SR / 2 / 16 * 16;
            for (int f = 0; f < n; f += 16) {
                if (f == at) bus_perform(k, 1);
                if (f == off) bus_perform(k, 0);
                m2k_render(b + 2 * f, 16);
            }
            double own = step_max(b, at + M2K_SR / 10, off), ref = own > ds ? own : ds;
            double s1 = step_max(b, at - 441, at + 882), s2 = step_max(b, off - 441, off + 882);
            double r = (s1 > s2 ? s1 : s2) / ref;
            if (r > worst) worst = r, wk = k;
        }
        free(b);
        printf("     PERFORM on a sine: the worst entry or exit %.2f x the steps about it (key %d)\n", worst, wk + 1);
        CHECK(!bad && worst < 1.3, "glitch and PERFORM: every entry, exit and seam crossfades (no step past the sound's own)");
    }
    {
        /* PERFORM's repeats on the grid: R8 pressed 40 ms past a step plays the eighth that began on the last eighth
         * (a quarter is a quarter at 120 BPM: 11025 half-rate samples, it was cut to 8192), coming round on each eighth;
         * the stop slows in a straight line to nothing within its beat and spins back up once let go; the stutter is
         * gated, about half of each 1/32 silent; DRIVE stays at the dry's level, never at full scale */
        m2k_pattern_t *q = eng_pat(78);
        q->glob[G_VERB] = 0;
        sine_track(q);
        eng_go(q);
        sine_on();
        eng_run(1.0 + 0.040);
        bus_perform(PF_R4, 1);
        int r4 = bus.g_len == (sps_q8 >> 9) * 4;
        bus_perform(PF_R4, 0);
        eng_run(0.3);
        bus_perform(PF_R8, 1);
        int grid = bus.g_start == bus.g_grid[1] && bus.g_len == (sps_q8 >> 9) * 2, round_ok = 1, was = sq.step;
        for (int f = 0; f < M2K_SR; f += M2K_SUB) {
            m2k_render(eng_out, M2K_SUB);
            if (sq.step != was && !((sq.step - 1) & 1)) round_ok &= (bus.g_pos >> 12) < M2K_SUB;   /* (an eighth begins) */
            was = sq.step;
        }
        bus_perform(PF_R8, 0);
        eng_run(0.2);
        bus_perform(PF_STOP, 1);
        uint32_t last = bus.g_rate, beat = (sps_q8 >> 9) * 4;
        int mono = 1, ticks = 0;
        while (bus.g_rate && ticks < (int)beat * 2) { m2k_render(eng_out, M2K_SUB); mono &= bus.g_rate <= last; last = bus.g_rate; ticks += M2K_SUB / 2; }
        int stop_ok = mono && !bus.g_rate && ticks <= (int)beat + 16;
        eng_run(0.2);
        bus_perform(PF_STOP, 0);
        eng_run(0.4);
        stop_ok &= !bus.g_on;
        bus_perform(PF_STUT, 1);
        eng_run(0.1);
        int n = M2K_SR / 2 / M2K_SUB * M2K_SUB, quiet = 0, loud = 0;
        static int16_t sb[2 * M2K_SR];
        for (int f = 0; f < n; f += M2K_SUB) m2k_render(sb + 2 * f, M2K_SUB);
        for (int i = 0; i + 32 <= n; i += 32) {
            int pk = 0;
            for (int j = 0; j < 32; j++) pk = abs(sb[2 * (i + j)]) > pk ? abs(sb[2 * (i + j)]) : pk;
            quiet += pk < 100; loud += pk > 3000;
        }
        bus_perform(PF_STUT, 0);
        int gated = quiet * 100 > 30 * (n / 32) && loud * 100 > 30 * (n / 32);
        printf("     PERFORM: R4 %s, R8 on the grid %d, came round on each eighth %d; the stop %d ticks to rest; the stutter %d %% silent, %d %% sounding\n",
               r4 ? "a quarter" : "cut", grid, round_ok, ticks, quiet * 100 / (n / 32), loud * 100 / (n / 32));
        /* DRIVE on the reference mix: its rms within 1.5 dB of the dry's, no sample at full scale */
        const int frames = 4 * M2K_SR / 16 * 16;
        int16_t *b = malloc((size_t)frames * 4), *dr = malloc((size_t)frames * 4);
        for (int k = 0; k < 2; k++) {
            bus_ref(1, G_GLCH, 0, 64);
            int16_t *dst = k ? b : dr;
            for (int f = 0; f < frames; f += 16) { if (k && f == M2K_SR / 16 * 16) bus_perform(PF_DRIV, 1); m2k_render(dst + 2 * f, 16); }
            if (k) bus_perform(PF_DRIV, 0);
        }
        double s0 = 0, s1 = 0;
        int full = 0;
        for (int i = 2 * 2 * M2K_SR; i < 2 * frames; i++) { s0 += (double)dr[i] * dr[i]; s1 += (double)b[i] * b[i]; full += abs(b[i]) >= 32767; }
        free(b);
        free(dr);
        double dd = 10 * log10(s1 / s0);
        printf("     PERFORM DRIVE: %+.1f dB against the dry, %d samples at full scale\n", dd, full);
        CHECK(r4 && grid && round_ok && stop_ok && gated && fabs(dd) < 1.5 && !full,
              "PERFORM: repeats on the grid, the stop slows to rest in its beat and spins up, the stutter gated, DRIVE level");
    }
    {
        /* DSND's LFO across 64 leaves the lines' layout (it followed the modulated value: a 16 KB line wiped at every
         * crossing): a ping-pong kit swept from 0 to 127 by a fast square */
        m2k_pattern_t *q = eng_pat(71);
        kit_default(q, 0, M_SAW);
        q->kit[0][P_HOLD] = 127; q->kit[0][P_GATE] = 127; q->kit[0][P_DSND] = 127; q->kit[0][P_DFB] = 50;
        uint8_t *l = &q->kit[0][P_LFO];
        l[L_DEST] = P_DSND + 1; l[L_DPTH] = 0; l[L_WAVE] = enc(2, 7); l[L_SPD] = 100; l[L_MULT] = enc(4, 8);
        q->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
        eng_go(q);
        eng_run(0.1);
        int flipped = 0, was = trk[0].dmode;
        for (int f = 0; f < 2 * M2K_SR / M2K_SUB; f++) { m2k_render(eng_out, M2K_SUB); flipped += trk[0].dmode != was; was = trk[0].dmode; }
        CHECK(was == 1 && !flipped, "delay: the lines' layout follows the kit's DSND, not an LFO's or a lock's");
    }
    {
        /* SWAV-SAW's drift draws on its own generator: two drifting saws, one on either core, render on two cores as on
         * one (the shared one drew in the cores' order, and from both at once) */
        static int16_t o[2][2 * (M2K_SR / 2 + M2K_SUB)];          /* (whole blocks past the half second) */
        for (int mode = 0; mode < 2; mode++) {
            par_mode = mode;
            m2k_par_begin = mode ? par_test_begin : 0;
            m2k_par_end = mode ? par_test_end : 0;
            m2k_pattern_t *q = eng_pat(72);
            for (int t = 0; t < M2K_TRACKS; t += 5) {
                kit_default(q, t, M_SAW);
                q->kit[t][P_SYN + 3] = 127; q->kit[t][P_HOLD] = 127; q->kit[t][P_GATE] = 127;
                q->step[t][0] = (m2k_step_t){.note = (uint8_t)(48 + t), .flags = ST_TRIG};
            }
            eng_go(q);
            for (int f = 0; f < M2K_SR / 2; f += M2K_SUB) m2k_render(o[mode] + 2 * f, M2K_SUB);
        }
        m2k_par_begin = 0; m2k_par_end = 0;
        CHECK(!memcmp(o[0], o[1], sizeof o[0]), "SWAV-SAW: its drift the same on two cores as on one");
    }
    {
        /* past 2^23 in unsigned arithmetic, as the rest wraps (signed, they overflowed): the EXP LFO's square of 65535 - x
         * (from its phase 0); the delay's write index, SWAV-ENS's chorus line and the tape's (minutes in), the render
         * the same as with the same indices a whole number of their lines from 0 */
        int ok = 1, r = 0;
        for (uint32_t ph = 0; ph < 0x80000000u; ph += 0x08000000u) {
            uint64_t y = 65535u - (ph >> 16);
            y = (y * y) >> 16;
            y = (y * y) >> 16;
            ok &= lfo_shape(4, ph, &r) == (int32_t)(y >> 1);
        }
        static int16_t o[2][2 * (M2K_SR / 2 + M2K_SUB)];          /* (whole blocks past the half second) */
        for (int k = 0; k < 2; k++) {
            m2k_pattern_t *q = eng_pat(73);
            q->glob[G_TAPE] = 64;
            kit_default(q, 0, M_SAW);
            kit_default(q, 1, M_ENS);
            q->kit[0][P_DSND] = 0; q->kit[0][P_HOLD] = 127; q->kit[1][P_SYN + 5] = 64; q->kit[1][P_HOLD] = 127;
            q->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
            q->step[1][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
            eng_go(q);
            m2k_render(eng_out, M2K_SUB);
            uint32_t at = (1u << 23) - 512;                            /* (a multiple of 16384, 1024 and 2048 less 512) */
            trk[0].dw = k ? at : at & (DL_LEN - 1);
            trk[1].ph[5] = k ? at : at & (ENS_LEN - 1);
            bus.tw = k ? at : at & 2047u;
            for (int f = 0; f < M2K_SR / 2; f += M2K_SUB) m2k_render(o[k] + 2 * f, M2K_SUB);
        }
        CHECK(ok && !memcmp(o[0], o[1], sizeof o[0]), "unsigned past 2^23: the EXP LFO, the delay's, SWAV-ENS's and the tape's positions");
    }
    /* the splash: 2.1 s from the first frame, then the wipe into HOME; a key, a button or a knob ends it at once and
     * is the splash's alone (its release too): no note, no page, the sequencer untouched; after it a press is a press */
    {
        int kw = 0;
        for (int k = 0; k < M2K_KEYS; k++) if (ui.kw[k] == 2) kw = k;
        (m2k_init)(60);
        int run = 1;
        for (uint32_t ms = 0; ms < SPLASH_MS; ms += 40) { m2k_frame(ms); run &= splash_on(); }
        m2k_frame(SPLASH_MS + 20);
        int wiped = !splash_on() && ui.in_trans && ui.trans_wipe;
        m2k_frame(SPLASH_MS + WIPE_MS + 40);
        int home = !ui.in_trans && ui.page == PG_HOME;
        (m2k_init)(61);
        m2k_frame(500);
        int page = ui.page;
        m2k_key(kw, 1);
        int skip = !splash_on() && sq.nheld == 0;
        m2k_frame(520);
        m2k_key(kw, 0);
        m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);                  /* (in the wipe: a press is a press) */
        int plays = sq.playing;
        seq_play(0);
        (m2k_init)(62);
        m2k_frame(500);
        m2k_button(B_ENV, 1); m2k_frame(540); m2k_button(B_ENV, 0);       /* the page's button: skipped, not the page */
        m2k_frame(580);
        int envp = ui.page == PG_HOME && !splash_on();
        (m2k_init)(62);
        m2k_frame(500);
        int lv = PAT(cur_pat)->kit[ui.trk][P_LEVL];
        m2k_encoder(E_K1, 3);                                            /* a knob: skipped, the level kept */
        int still = ui.page == page && !sq.playing && PAT(cur_pat)->kit[ui.trk][P_LEVL] == lv && !splash_on();
        for (uint32_t ms = 560; ms < 1400; ms += 40) m2k_frame(ms);
        m2k_key(kw, 1);
        int note = sq.nheld == 1;
        m2k_key(kw, 0);
        CHECK(run && wiped && home && skip && plays && envp && still && note,
              "splash: 2.1 s at power-on, then the wipe into HOME; a key, a button, a knob skips it, swallowed with its release");
    }
    /* the text audit: a text across a rule and past its cell is found, one inside its cell clear of the rules is not */
    {
        ux_init(63);
        int before = ta_found;
        ta_quiet = 1;
        cls();
        rule_v(GX, STAGE_Y + 2, STAGE_H - 4, 1);
        CELL(0, STAGE_Y, GX, STRIP_Y - 1, VX, STAGE_Y + 2, VX + VW, STRIP_Y - 2);
        text(&F_L, VX, 100, "SWAV-SAW", 1, 0);                      /* (178 px from 4: over the rule at 104) */
        ta_check((int)ux_now);
        int found = ta_found - before;
        cls();
        rule_v(GX, STAGE_Y + 2, STAGE_H - 4, 1);
        CELL(0, STAGE_Y, GX, STRIP_Y - 1, VX, STAGE_Y + 2, VX + VW, STRIP_Y - 2);
        text(&F_M, VX, 100, "SWAV-", 1, 0);
        ta_check((int)ux_now);
        int clear = ta_found - before == found;
        ta_quiet = 0;
        ta_found = before;
        CHECK(found == 2 && clear, "text audit: a text over a rule and past its cell found, one inside and clear not");
    }
    printf("%s (%d failed)\n", fails ? "SELFTEST FAILED" : "selftest passed", fails);
    return fails != 0;
}
#undef m2k_init

/* the browser and the FM-1's stores, through the Patch Editor's link (host/m2k_link_test.c): --web-store PATH writes
 * a store as the browser sends it to the FM-1 (64 patterns, one project; A01 named WEB16, sound 6 WEBSND);
 * --fm1-store PATH reads one the FM-1 sent (the link test's: A01 LINKA, sound 6 SNDA) as the browser does. Sound 6's
 * note goes with it, by its name (the link test's name_root) */
static uint8_t name_root(const char *s) { unsigned h = 0; while (*s) h = h * 31u + (uint8_t)*s++; return (uint8_t)(24 + h % 72); }

static int link_store(const char *mode, const char *path)
{
    int max;
    uint8_t *b = m2k_store_buf(&max);
    m2k_init(1);
    if (!strcmp(mode, "--web-store")) {
        memset(pat[0].name, 0, 8); memcpy(pat[0].name, "WEB16", 5);
        memset(snd[5].name, 0, 8); memcpy(snd[5].name, "WEBSND", 6);
        snd_root[5] = name_root("WEBSND");
        int n = m2k_store_pack_as(64, 1);
        FILE *f = fopen(path, "wb");
        if (!f || n <= 0 || b[8] != 64 || b[9] != 1) { printf("FAIL a store for the FM-1\n"); if (f) fclose(f); return 1; }
        fwrite(b, 1, (size_t)n, f);
        fclose(f);
        printf("ok   a store for the FM-1: %d B, 64 patterns, 1 project\n", n);
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    int n = (int)fread(b, 1, (size_t)max, f);
    fclose(f);
    int ok = b[8] == 64 && b[9] == 1 && m2k_store_unpack(n) && !strcmp(pat[0].name, "LINKA") && !strcmp(snd[5].name, "SNDA") &&
             snd_root[5] == name_root("SNDA") && !strcmp(pat[16].name, "INIT");
    printf("%s the FM-1's store read in the browser: its 64 patterns, its sounds and their notes\n", ok ? "ok  " : "FAIL");
    return !ok;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc > 1 && !strcmp(argv[1], "--text-sweep")) return text_sweep();
    if (argc > 1 && !strcmp(argv[1], "--verb-grid")) return verb_grid();
    if (argc > 2 && !strcmp(argv[1], "--glch-curve")) return glch_curve(atoi(argv[2]));
    if (argc > 1 && !strcmp(argv[1], "--bank-hash")) {        /* as host/m2k_link_test.c prints it, through the FM-1's cache */
        m2k_init(3);
        uint32_t h = 2166136261u;
        for (int i = 0; i < M2K_PATTERNS; i++) { int n = pat_pack(PAT(i), blob); for (int k = 0; k < n; k++) h = (h ^ blob[k]) * 16777619u; }
        const uint8_t *sb = (const uint8_t *)snd;
        for (unsigned k = 0; k < sizeof snd; k++) h = (h ^ sb[k]) * 16777619u;
        printf("     bank hash %08x\n", h);
        return 0;
    }
    if (argc > 2 && (!strcmp(argv[1], "--web-store") || !strcmp(argv[1], "--fm1-store"))) return link_store(argv[1], argv[2]);
    const char *out = argc > 1 ? argv[1] : "build/host";
    char path[512];
    if (argc > 2 && !strcmp(argv[2], "--analyze")) return analyze(out);
    if (argc > 2 && !strcmp(argv[2], "--pitch")) {           /* [FIXTURE]: a bank kept with --export-bank */
        if (argc > 3) {
            FILE *f = fopen(argv[3], "rb");
            if (!f) { perror(argv[3]); return 1; }
            bank_len = (int)fread(bank_fix, 1, sizeof bank_fix, f);
            fclose(f);
        }
        return pitch_dump(out);
    }
    if (argc > 2 && !strcmp(argv[2], "--export-bank")) return bank_export(out);
    if (argc > 2 && !strcmp(argv[2], "--splash")) {          /* every splash frame, 25 a second, then HOME: tools/splash.py */
        m2k_init(2000);
        int n = 0;
        for (uint32_t ms = 0; ms <= SPLASH_MS + WIPE_MS + 120; ms += 40, n++) {
            snprintf(path, sizeof path, "%s/splash_%03d.pbm", out, n);
            pbm_write(path, m2k_frame(ms));
        }
        printf("%d splash frames in %s (40 ms apart): .venv/bin/python tools/splash.py %s\n", n, out, out);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[2], "--bus")) return bus_measure(out);
    if (argc > 2 && !strcmp(argv[2], "--demo")) return demo_render(out, argc, argv);     /* [SECONDS] [SLOT ...] */
    if (argc > 2 && !strcmp(argv[2], "--demo-dry")) { demo_dry = 1; return demo_render(out, argc, argv); }
    /* 1: the three factory patterns, 12 s each */
    for (int p = 0; p < 3; p++) {
        m2k_init(2000);
        cur_pat = p;
        sq.queued = (uint8_t)p;
        lockmap_build();
        for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
        seq_play(1);
        int frames = M2K_SR * 12 / 128 * 128;                    /* whole blocks */
        int16_t *buf = malloc((size_t)frames * 4);
        clock_t c0 = clock();
        for (int f = 0; f < frames; f += 128) m2k_render(buf + 2 * f, 128);
        double sec = (double)(clock() - c0) / CLOCKS_PER_SEC;
        int peak = 0;
        double rms = 0;
        for (int i = 0; i < frames * 2; i++) { int a = abs(buf[i]); if (a > peak) peak = a; rms += (double)buf[i] * buf[i]; }
        rms = sqrt(rms / (frames * 2));
        snprintf(path, sizeof path, "%s/a%02d.wav", out, p + 1);
        wav_write(path, buf, frames);
        printf("A%02d %-8s peak %5d (%.1f dBFS) rms %.1f dBFS  render %.3f s for 12 s (%.1f%% of real time here)\n", p + 1,
               pat[p].name, peak, 20 * log10(peak / 32767.0 + 1e-9), 20 * log10(rms / 32767.0 + 1e-9), sec, sec / 12 * 100);
        free(buf);
    }
    /* 2: the tour, with screenshots; then the manual's scenes */
    m2k_init(2000);
    run_events(TOUR, out);
    manual_scene();
    run_events(MANUAL, out);
    printf("tour done%s\n", ta_found ? ": TEXT TOUCHES OR IS CUT (above)" : ", every text clear");
    return ta_found != 0;
}
