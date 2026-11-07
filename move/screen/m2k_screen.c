/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the 128 x 64 1-bit screen (move/screen/DESIGN.md). Part of the core's unity build: included
 * after firmware/src/m2k_core.c. Two halves:
 *   the gather (g_*)  reads the core's state and names (m2k_ui.c's pinfo, fmt, icon_of; the sequencer; the monitor)
 *                     and the controller's moment into a plain view (m2k_screen.h ms_view_t). The audio thread's.
 *   the draw (d_*)    draws a view into a frame, 1 KB, from the view alone, the type, the icons, the core's const tables
 *                     and its pure functions (env_level, filt_db, lfo_shape, sinp...): any thread.
 * The type is the owner's (assets/fonts/mono2k-pixel: "Elektron Pixel Font" by savingaurora, CC BY-SA 3.0, adapted by
 * MONO2K under CC BY-SA 4.0) at its native size, x1: F_S's bitmaps, which tools/gen_font.py keeps native and the FM-1
 * widens. One weight. The icons and pictures are MONO2K's own 1 px line drawings.
 *
 * The grid: 2 px, the FM-1's 4 px at the type's x1. Text sits on a baseline (the row under its 5-row capitals), so a
 * tag round a word is 7 rows. Calm (the owner's, 2026-11-01: "names on touch only", "good but calmer"): a knob's name
 * shows only while it is touched or turned; each page one picture and plenty of black. Bands:
 *   0..6     header: the pad that leads here (a map of the two page rows), the page's tag (filled: a sound page;
 *            framed: the rest), the track and its machine | the pattern, REC, the transport
 *   12..43   the stage: the page's one live picture; a knob touched, its full name and its value large
 *   47       a held step's lock, a line over the knob's box
 *   49..61   the knobs' pictures, 8 columns of 16 for the Move's 8 knobs, each in a 13 x 13 box at 2..14 of its
 *            column (a 3 px gutter between two); row 63 marks the knob touched, a line under its box
 * HOME, MIX's strips, SOUNDS and the patterns lay the middle out their own way under the same header.
 * Integer only, nothing allocated; the meters' fall is the one state kept between gathers. */
#include "m2k_screen.h"

static uint8_t ms_own[MS_BYTES];        /* m2k_screen()'s frame */
static uint8_t *ms_fb = ms_own;         /* the frame being drawn (m2k_screen_draw's) */

#define MS_RB      16                  /* bytes a row */
#define MS_HB      6                   /* the header's baseline */
#define MS_ST_T    12                  /* the stage */
#define MS_ST_B    43
#define MS_KS      13                  /* a knob's picture: 13 x 13, the same box for every kind */
#define MS_KX      2                   /* its first column in its 16 (a 3 px gutter between two) */
#define MS_KP      49                  /* its first row */
#define MS_KW      13                  /* a value written in a column: at most the box's width */
#define MS_LOCK_Y  (MS_KP - 2)         /* a lock: a line over the box */
#define MS_CAP     5                   /* the capitals' rows at x1 */
#define MS_FROWS   7                   /* a glyph's rows with its descenders */
#define MS_NW      14                  /* a name written in a column (an LFO's destination): its 16 less a pixel a side */
#define MS_FREE    0xFF                /* a knob with nothing on it */

/* ------------------------------------------------------------------------------------------------ drawing */

static inline void ms_px(int x, int y, int c)
{
    if ((unsigned)x >= MS_W || (unsigned)y >= MS_H) return;
    uint8_t *b = &ms_fb[y * MS_RB + (x >> 3)], m = (uint8_t)(0x80u >> (x & 7));
    if (c == 1) *b |= m; else if (c == 0) *b &= (uint8_t)~m; else *b ^= m;
}

/* a row of up to 8 pixels (MSB first) at x: the type's and the icons' rows, a byte or two at a time */
static void ms_row8(int x, int y, uint8_t bits, int c)
{
    if ((unsigned)y >= MS_H || !bits || x >= MS_W || x <= -8) return;
    if (x < 0) { bits = (uint8_t)(bits << -x); x = 0; }
    const int sh = x & 7, bx = x >> 3;
    uint8_t *p = &ms_fb[y * MS_RB + bx];
    const uint8_t b0 = (uint8_t)(bits >> sh), b1 = (uint8_t)(sh ? bits << (8 - sh) : 0);
    if (c == 1) { p[0] |= b0; if (bx < MS_RB - 1) p[1] |= b1; }
    else if (c == 0) { p[0] &= (uint8_t)~b0; if (bx < MS_RB - 1) p[1] &= (uint8_t)~b1; }
    else { p[0] ^= b0; if (bx < MS_RB - 1) p[1] ^= b1; }
}

/* c: 0 dark, 1 lit, 2 inverted; clipped to the screen, a byte at a time with the end bytes masked */
static void ms_rect(int x, int y, int w, int h, int c)
{
    int x0 = x < 0 ? 0 : x, x1 = x + w > MS_W ? MS_W : x + w, y0 = y < 0 ? 0 : y, y1 = y + h > MS_H ? MS_H : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    const int b0 = x0 >> 3, b1 = (x1 - 1) >> 3;
    const uint8_t m0 = (uint8_t)(0xFFu >> (x0 & 7)), m1 = (uint8_t)(0xFF00u >> (((x1 - 1) & 7) + 1));
    for (int r = y0; r < y1; r++) {
        uint8_t *p = &ms_fb[r * MS_RB];
        for (int b = b0; b <= b1; b++) {
            uint8_t m = (uint8_t)((b == b0 ? m0 : 0xFF) & (b == b1 ? m1 : 0xFF));
            if (c == 1) p[b] |= m; else if (c == 0) p[b] &= (uint8_t)~m; else p[b] ^= m;
        }
    }
}
static void ms_hline(int x, int y, int w, int c) { ms_rect(x, y, w, 1, c); }
static void ms_vline(int x, int y, int h, int c) { ms_rect(x, y, 1, h, c); }
static void ms_frame(int x, int y, int w, int h, int c)
{
    ms_hline(x, y, w, c); ms_hline(x, y + h - 1, w, c);
    ms_vline(x, y + 1, h - 2, c); ms_vline(x + w - 1, y + 1, h - 2, c);
}
/* rounded by a pixel: MONO2K's tags (the corners left as they were) */
static void ms_rfill(int x, int y, int w, int h, int c)
{
    ms_rect(x + 1, y, w - 2, h, c);
    ms_vline(x, y + 1, h - 2, c);
    ms_vline(x + w - 1, y + 1, h - 2, c);
}
static void ms_rframe(int x, int y, int w, int h, int c)
{
    ms_hline(x + 1, y, w - 2, c); ms_hline(x + 1, y + h - 1, w - 2, c);
    ms_vline(x, y + 1, h - 2, c); ms_vline(x + w - 1, y + 1, h - 2, c);
}
/* dotted, on the screen's checker ((x + y) even): every dotted line meets the others in step */
static void ms_dots_h(int x, int y, int w) { for (int i = x + ((x + y) & 1); i < x + w; i += 2) ms_px(i, y, 1); }
static void ms_dots_v(int x, int y, int h) { for (int j = y + ((x + y) & 1); j < y + h; j += 2) ms_px(x, j, 1); }
static void ms_line(int x0, int y0, int x1, int y1, int c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        ms_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}
/* an area under a curve, column x from row y down to the floor yb (not included), on the picture's own grid (x0):
 * tone 1 a tint (a dot every fourth column, every second row: the area at rest), 2 the part being turned (a hairline
 * every second column), 3 solid */
static void ms_fill(int x, int x0, int y, int yb, int tone)
{
    const int k = (x - x0) & 3;
    if (y >= yb || !tone) return;
    if (tone == 3) ms_vline(x, y, yb - y, 1);
    else if (tone == 2) { if (!(k & 1)) ms_vline(x, y, yb - y, 1); }
    else if (!k) for (int j = yb - 2; j >= y; j -= 2) ms_px(x, j, 1);
}
/* an editable point: 3 x 3 framed in a dark ring; solid while its knob is touched */
static void ms_handle(int x, int y, int hot)
{
    ms_rect(x - 2, y - 2, 5, 5, 0);
    if (hot) ms_rect(x - 1, y - 1, 3, 3, 1); else ms_frame(x - 1, y - 1, 3, 3, 1);
}
/* where a live thing is (a voice's level, an LFO's value): 3 x 3 solid in a dark ring */
static void ms_marker(int x, int y) { ms_rect(x - 2, y - 2, 5, 5, 0); ms_rect(x - 1, y - 1, 3, 3, 1); }

/* ---------------------------------------------------------------------------------------------- the type */

/* the owner's font at its own size: F_S's bitmaps are native (a byte a row, MS_FROWS rows), drawn as they are */
static const uint8_t *ms_glyph(int ch, int *w)
{
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < F_S.first || ch > F_S.last) ch = '?';
    const int i = ch - F_S.first;
    *w = F_S.w[i] / F_S.z;
    return &F_S.bits[F_S.off[i]];
}

static int ms_tw(const char *s)
{
    int w = 0, n = 0;
    for (; *s; s++, n++) { int g; ms_glyph(*s, &g); w += g; }
    return n ? w + n - 1 : 0;                 /* (a design pixel between two glyphs) */
}

/* on baseline b (the capitals the 5 rows above it); its width */
static int ms_text(int x, int b, const char *s, int c)
{
    const int x0 = x;
    for (; *s; s++) {
        int w;
        const uint8_t *g = ms_glyph(*s, &w);
        for (int r = 0; r < MS_FROWS; r++) ms_row8(x, b - MS_CAP + r, g[r], c);
        x += w + 1;
    }
    return x > x0 ? x - x0 - 1 : 0;
}
static int ms_text_r(int xr, int b, const char *s, int c) { int w = ms_tw(s); ms_text(xr - w + 1, b, s, c); return w; }
static int ms_text_c(int cx, int b, const char *s, int c) { int w = ms_tw(s); ms_text(cx - w / 2, b, s, c); return w; }

/* z times its size (a value large): each design pixel z x z, the same one weight */
static int ms_tw_z(const char *s, int z) { return ms_tw(s) * z; }
static int ms_text_z(int x, int b, const char *s, int z, int c)
{
    const int x0 = x;
    for (; *s; s++) {
        int w;
        const uint8_t *g = ms_glyph(*s, &w);
        for (int r = 0; r < MS_FROWS; r++)
            for (int i = 0; i < w; i++)
                if ((g[r] >> (7 - i)) & 1) ms_rect(x + i * z, b - MS_CAP * z + r * z, z, z, c);
        x += (w + 1) * z;
    }
    return x > x0 ? x - x0 - z : 0;
}

/* a word in a tag, 7 rows from b - 6 to b: filled (the word dark), or the word lit over a line a row under it (a
 * frame round it would touch it: the type fills the 5 rows inside); its width */
static int ms_tag(int x, int b, const char *s, int filled)
{
    int w = ms_tw(s);
    if (filled) { ms_rfill(x, b - 6, w + 4, 7, 1); ms_text(x + 2, b, s, 0); }
    else { ms_text(x + 2, b, s, 1); ms_hline(x + 1, b + 1, w + 2, 1); }
    return w + 4;
}

/* ------------------------------------------------------------------------------------------------- icons */

/* MONO2K's line icons, 7 x 5: the capitals' height, so they sit on a baseline as the type does. 1 px strokes on a
 * 1 px grid; drawn for MONO2K, after no one's set */
enum { I_HOME, I_SYN, I_AMP, I_FILT, I_EFX, I_DSTR, I_ARP, I_LFO, I_GLO, I_GRID, I_SET, I_CAT, I_PERF, I_PLAY,
       I_STOP, I_REC, I_SYNC, I_MUTE, I_SPKR, I_LOCK, I_HOLD, I_PERC, I_NOIS, I_SID, I_FM, I_SAW, I_PULS, I_ENS,
       I_WAV, I_VOX, I_KICK, I_SNAR, I_HATS, I_CYMB, I_CHOR, I_RING, I_DYN, I_MIX, I_PADS, I_COUNT };
static const uint8_t MS_ICON[I_COUNT][5] = {
    {0x10, 0x28, 0x44, 0x44, 0x7C},   /* HOME  ___#___ __#_#__ _#___#_ _#___#_ _#####_ */
    {0x60, 0x90, 0x92, 0x0A, 0x04},   /* SYN   _##____ #__#___ #__#__# ____#_# _____#_ */
    {0x40, 0xA0, 0x9C, 0x82, 0x80},   /* AMP   _#_____ #_#____ #__###_ #_____# #______ */
    {0x00, 0x10, 0xE8, 0x04, 0x02},   /* FILT  _______ ___#___ ###_#__ _____#_ ______# */
    {0x80, 0xA0, 0xA8, 0xAA, 0xAA},   /* EFX   #______ #_#____ #_#_#__ #_#_#_# #_#_#_# */
    {0x0E, 0x08, 0x38, 0x20, 0xE0},   /* DSTR  ____### ____#__ __###__ __#____ ###____ */
    {0x06, 0x00, 0x18, 0x00, 0x60},   /* ARP   _____## _______ ___##__ _______ _##____ */
    {0x00, 0x10, 0x28, 0x44, 0x82},   /* LFO   _______ ___#___ __#_#__ _#___#_ #_____# */
    {0x7C, 0x92, 0xFE, 0x92, 0x7C},   /* GLO   _#####_ #__#__# ####### #__#__# _#####_ */
    {0x92, 0x00, 0x92, 0x00, 0x92},   /* GRID  #__#__# _______ #__#__# _______ #__#__# */
    {0x44, 0xE4, 0x4E, 0x44, 0x44},   /* SET   _#___#_ ###__#_ _#__### _#___#_ _#___#_ */
    {0xBE, 0x00, 0xBE, 0x00, 0xBE},   /* CAT   #_##### _______ #_##### _______ #_##### */
    {0x0C, 0x18, 0x3C, 0x30, 0x60},   /* PERF  ____##_ ___##__ __####_ __##___ _##____ */
    {0x80, 0xE0, 0xF8, 0xE0, 0x80},   /* PLAY  #______ ###____ #####__ ###____ #______ */
    {0xF8, 0x88, 0x88, 0x88, 0xF8},   /* STOP  #####__ #___#__ #___#__ #___#__ #####__ */
    {0x70, 0xF8, 0xF8, 0xF8, 0x70},   /* REC   _###___ #####__ #####__ #####__ _###___ */
    {0x70, 0x9C, 0xAA, 0x72, 0x1C},   /* SYNC  _###___ #__###_ #_#_#_# _###__# ___###_ */
    {0x20, 0x6A, 0xE4, 0x6A, 0x20},   /* MUTE  __#____ _##_#_# ###__#_ _##_#_# __#____ */
    {0x24, 0x68, 0xEA, 0x68, 0x24},   /* SPKR  __#__#_ _##_#__ ###_#_# _##_#__ __#__#_ */
    {0x70, 0x50, 0xF8, 0xD8, 0xF8},   /* LOCK  _###___ _#_#___ #####__ ##_##__ #####__ */
    {0xA8, 0xF8, 0x70, 0x20, 0x20},   /* HOLD  #_#_#__ #####__ _###___ __#____ __#____ */
    {0x80, 0xD0, 0xA8, 0x84, 0x82},   /* PERC  #______ ##_#___ #_#_#__ #____#_ #_____# */
    {0x24, 0xB4, 0x4A, 0x02, 0x00},   /* NOIS  __#__#_ #_##_#_ _#__#_# ______# _______ */
    {0x54, 0xFE, 0x82, 0xFE, 0x54},   /* SID   _#_#_#_ ####### #_____# ####### _#_#_#_ */
    {0xE0, 0xA0, 0xFE, 0x0A, 0x0E},   /* FM    ###____ #_#____ ####### ____#_# ____### */
    {0x12, 0x36, 0x54, 0x98, 0x00},   /* SAW   ___#__# __##_## _#_#_#_ #__##__ _______ */
    {0xE2, 0xA2, 0xA2, 0xA2, 0xBE},   /* PULS  ###___# #_#___# #_#___# #_#___# #_##### */
    {0x7C, 0x00, 0x7C, 0x00, 0x7C},   /* ENS   _#####_ _______ _#####_ _______ _#####_ */
    {0x70, 0x50, 0xDA, 0x0A, 0x0E},   /* WAV   _###___ _#_#___ ##_##_# ____#_# ____### */
    {0x7C, 0x82, 0x7C, 0x40, 0x80},   /* VOX   _#####_ #_____# _#####_ _#_____ #______ */
    {0x70, 0x88, 0xA8, 0x88, 0x70},   /* KICK  _###___ #___#__ #_#_#__ #___#__ _###___ */
    {0x82, 0x44, 0xFE, 0x82, 0xFE},   /* SNAR  #_____# _#___#_ ####### #_____# ####### */
    {0x7C, 0x10, 0x7C, 0x10, 0x10},   /* HATS  _#####_ ___#___ _#####_ ___#___ ___#___ */
    {0x10, 0x7C, 0x82, 0x10, 0x10},   /* CYMB  ___#___ _#####_ #_____# ___#___ ___#___ */
    {0x44, 0xAA, 0x10, 0x44, 0xAA},   /* CHOR  _#___#_ #_#_#_# ___#___ _#___#_ #_#_#_# */
    {0x70, 0xD8, 0xA8, 0xD8, 0x70},   /* RING  _###___ ##_##__ #_#_#__ ##_##__ _###___ */
    {0x06, 0x18, 0x20, 0x40, 0x80},   /* DYN   _____## ___##__ __#____ _#_____ #______ */
    {0x48, 0x48, 0xEA, 0x4E, 0x4A},   /* MIX   _#__#__ _#__#__ ###_#_# _#__### _#__#_# */
    {0xD8, 0xD8, 0x00, 0xD8, 0xD8},   /* PADS  ##_##__ ##_##__ _______ ##_##__ ##_##__ (four pads; 5 wide) */
};
static void ms_icon(int ic, int x, int b, int c) { for (int r = 0; r < 5; r++) ms_row8(x, b - 5 + r, MS_ICON[ic][r], c); }

/* a machine's icon, by its family and voice */
static int ms_machine_icon(int m)
{
    switch (m) {
    case M_GSIN: return I_SYN;
    case M_GPRC: return I_PERC;
    case M_GNOI: return I_NOIS;
    case M_SID: return I_SID;
    case M_FMS: case M_FMD: case M_FMP: return I_FM;
    case M_SAW: return I_SAW;
    case M_PUL: return I_PULS;
    case M_ENS: return I_ENS;
    case M_WAV: return I_WAV;
    case M_VOX: return I_VOX;
    case M_DKIK: return I_KICK;
    case M_DSNR: return I_SNAR;
    case M_DHAT: return I_HATS;
    case M_DCYM: return I_CYMB;
    case M_FXC: return I_CHOR;
    case M_FXR: return I_RING;
    default: return I_DYN;
    }
}

/* ------------------------------------------------------------------------------------------------ the pages */

static const char *const MS_PAGE_N[MS_COUNT] = {"HOME", "SYN", "AMP", "FILT", "EFX", "DSTR", "LFO1", "LFO2", "LFO3",
                                                "TRK", "PAT", "GRID", "MIX", "GLO", "PERFORM", "SOUNDS", "PATTERNS"};

const char *m2k_screen_page_name(int page) { return page >= 0 && page < MS_COUNT ? MS_PAGE_N[page] : "?"; }

int m2k_screen_param(int page, int t, int k)
{
    if (k < 0 || k > 7 || t < 0 || t >= M2K_TRACKS) return NONE;
    switch (page) {
    case MS_HOME: { static const uint8_t g[4] = {G_TLEN1, G_SPD1, G_TPRB1, G_LANE1};   /* (the FM-1's HOME 1 and 2) */
                    static const uint8_t m[4] = {P_LEVL, P_PTCH, GL(G_TMPO), GL(G_SWNG)};
                    return k < 4 ? m[k] : GL(g[k - 4] + t); }
    case MS_SYN: return P_SYN + k;
    case MS_AMP: return P_ATK + k;
    case MS_FILT: return P_BASE + k;
    case MS_EFX: return P_EQF + k;
    case MS_DSTR: return k < 4 ? P_BITS + k : P_MODT + k - 4;
    case MS_LFO1: case MS_LFO2: case MS_LFO3: return P_LFO + 8 * (page - MS_LFO1) + k;
    case MS_TRK: return k < 6 ? P_PTCH + k : k == 6 && PAT(cur_pat)->machine[t] == M_WAV ? P_WBNK : NONE;
    case MS_PAT: { static const uint8_t g[8] = {G_TMPO, G_SWNG, G_LEN, G_ACNT, G_TLEN1, G_SPD1, G_TPRB1, G_LANE1};
                   return k < 4 ? GL(g[k]) : GL(g[k] + t); }   /* (the pattern's time, then the track's: the FM-1's SEQ) */
    case MS_GRID: return k < 7 ? GL(G_GRX + k) : GL(G_LANE1 + t);
    case MS_MIX: return k < M2K_TRACKS ? P_LEVL : NONE;
    case MS_GLO: return k < 4 ? BUS_K[k] : k == 4 ? SETTING(S_MUTQ) : NONE;
    case MS_PERF: return k < 4 ? BUS_K[k] : NONE;     /* (the bus's four, as on the FM-1's PERFORM) */
    default: return NONE;
    }
}

int m2k_screen_knob_track(int page, int t, int k) { return page == MS_MIX && k >= 0 && k < M2K_TRACKS ? k : t; }

/* ======================================================================================== the gather (core) */

/* the value a knob shows: the kit's, a global, a setting; a step held on the track in view, its lock where it has one */
static int ms_value(const m2k_scr_t *s, int t, int code)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    if (code == NONE) return 0;
    if (code >= 0xC0) return store_.set[code & 0x3F];
    if (code >= 0x80) return p->glob[code & 0x7F];
    if (t == s->track && s->held < M2K_STEPS) { int i = lock_find(t, s->held, code); if (i >= 0) return p->lock[i].val; }
    return p->kit[t][code];
}
static int ms_locked(const m2k_scr_t *s, int t, int code)
{
    return code < 0x80 && t == s->track && s->held < M2K_STEPS && lock_find(t, s->held, code) >= 0;
}

/* a knob's short name where its long one will not do (an LFO's destination as a value names it) */
static const char *ms_label(const pi_t *pi)
{
    static const char *const L[][2] = {
        {"ATCK", "ATK"}, {"HOLD", "HLD"}, {"PORT", "POR"}, {"BASE", "BAS"}, {"WDTH", "WID"}, {"B-OF", "BOF"},
        {"W-OF", "WOF"}, {"DTIM", "DTM"}, {"DSND", "DSN"}, {"DBAS", "DBS"}, {"DWID", "DWD"}, {"FOLD", "FLD"},
        {"VERB", "VRB"}, {"PTCH", "PCH"}, {"LEVL", "LEV"}, {"MODE", "ARP"}, {"GATE", "GAT"}, {"TRCK", "TRK"},
        {"DEST", "DES"}, {"WAVE", "WAV"}, {"MULT", "MUL"}, {"PHAS", "PHS"}, {"DPTH", "DPT"}, {"TYPE", "TYP"},
        {"RATE", "SPD"}, {"DIST", "DST"}, {"BITS", "BIT"}, {"TRIG", "TRG"}, {"TUNE", "TUN"}, {"PCH2", "PC2"},
        {"PCH3", "PC3"}, {"PCH4", "PC4"}, {"VOC1", "VC1"}, {"VOC2", "VC2"}, {"VOIC", "VOI"},
    };
    for (unsigned i = 0; i < sizeof L / sizeof L[0]; i++) if (s_is(pi->s4, L[i][0])) return L[i][1];
    return pi->s4;
}

/* a value's text as a column writes it: the unit left out (it shows large when touched), then the sign, then
 * thousands as K, then the decimals; past w it is cut (a kind that would be is drawn, not written: ms_writes) */
static int ms_vfit(char *o, int w)                   /* 1: it had to be cut */
{
    if (ms_tw(o) <= w) return 0;
    if (o[0] == '+') { char *d = o; while ((d[0] = d[1])) d++; }
    if (ms_tw(o) <= w) return 0;
    int n = 0, dig = 1;
    while (o[n]) { if (o[n] < '0' || o[n] > '9') dig = 0; n++; }
    if (dig && n >= 4) {                               /* 1009 -> 1.0K, 12000 -> 12K */
        if (n == 4) { o[3] = 'K'; o[2] = o[1]; o[1] = '.'; o[4] = 0; }
        else { o[n - 3] = 'K'; o[n - 2] = 0; }
        if (ms_tw(o) <= w) return 0;
    }
    for (char *d = o + 1; *d; d++) if (*d == '.') { *d = 0; break; }
    if (ms_tw(o) <= w) return 0;
    n = 0;
    while (o[n]) n++;
    while (n > 1 && ms_tw(o) > w) o[--n] = 0;
    return 1;
}

/* a parameter as a value names it (an LFO's destination): its short name, else its first three letters */
static const char *ms_dest_name(const pi_t *pi, char *o)
{
    const char *lb = ms_label(pi);
    if (ms_tw(lb) <= MS_NW) return lb;
    s_cpyn(o, pi->s4, 4);
    for (int n = 3; n > 1 && ms_tw(o) > MS_NW; n--) o[n - 1] = 0;
    return o;
}

/* the kinds whose values are names read off the moment (a destination, a track, a channel): always written, in up to
 * MS_NW (a name is not a number to draw); every other written value fits the box, MS_KW */
static int ms_named(int k) { return k == K_DEST || k == K_TRK || k == K_MCH; }

/* a kind writes its value in its column only if every value it takes fits the box whole (else its word would come and
 * go as the knob turns): worked out once a kind, through fmt, by m2k_screen_init (off the audio thread). The names
 * read off the moment always do */
static uint8_t ms_kfit[128];                          /* 0 not yet, 1 written, 2 drawn */
static int ms_writes(const pi_t *pi, int t)
{
    const int k = pi->kind;
    if (ms_named(k)) return 1;
    if (k >= 128) return 0;
    if (!ms_kfit[k]) {
        int ok = 1;
        for (int v = kind_min(k); v <= kind_max(k) && ok; v++) {
            char o[32];
            const char *u;
            fmt(pi, t, v, o, &u);
            if (ms_vfit(o, MS_KW)) ok = 0;
        }
        ms_kfit[k] = (uint8_t)(ok ? 1 : 2);
    }
    return ms_kfit[k] == 1;
}

/* the column a knob's picture takes (m2k_ui.c's icon_of, at 16 px): what it is, drawn; a value written where its
 * kind's every value fits the box, else drawn (a choice of up to 6 its places, more a dial) */
enum { MP_DIAL, MP_BIDIAL, MP_SLIDER, MP_ENV, MP_FILT, MP_WAVE, MP_DWAV, MP_PAN, MP_CHOICE, MP_SEGS, MP_TOGGLE,
       MP_TEXT, MP_NUM };
static int ms_pic_of(const pi_t *pi, int t)
{
    const int k = pi->kind, n = kind_choices(k);
    if (k == K_WAVE) return MP_DWAV;
    if (k == K_TUN2 || k == K_FINE || k == K_PCH) return MP_BIDIAL;
    if (k == K_PROB) return MP_DIAL;
    switch (icon_of(pi)) {
    case IC_DIAL: return bipolar(k) ? MP_BIDIAL : MP_DIAL;
    case IC_SLIDER: return MP_SLIDER;
    case IC_ENV: return MP_ENV;
    case IC_FILT: return MP_FILT;
    case IC_WAVE: return MP_WAVE;
    case IC_PAN: return MP_PAN;
    default:
        if (n == 2) return MP_TOGGLE;
        if (ms_writes(pi, t)) return n && n <= 8 ? MP_CHOICE : icon_of(pi) == IC_BOX ? MP_TEXT : MP_NUM;
        return n && n <= 6 ? MP_SEGS : bipolar(k) ? MP_BIDIAL : MP_DIAL;   /* (7 or 8: rates, a dial) */
    }
}

/* where a knob's value sits, 0..256 (PROB by its chance) */
static int ms_q(const pi_t *pi, int v)
{
    const int mx = kind_max(pi->kind), mn = kind_min(pi->kind);
    if (pi->kind == K_PROB) return PROB_PCT[v & 15] * 256 / 100;
    return CLAMP(((v - mn) * 256) / (mx - mn ? mx - mn : 1), 0, 256);
}

/* the tempo knob: the tempo heard (Move's under SYNC, or the one asked of it; MONO2K's own under INT), tenths */
static int ms_move_tempo(const m2k_scr_t *s, int code) { return code == GL(G_TMPO) && s->bpm10; }
static void ms_bpm_text(char *o, int b10) { char *e = s_int(o, b10 / 10); if (b10 % 10) { *e++ = '.'; *e++ = (char)('0' + b10 % 10); *e = 0; } }

void m2k_screen_init(void)
{
    /* every kind a knob can show, once: so the audio thread's gathers never sweep a kind's values (ms_writes) */
    for (int m = 0; m < M_COUNT; m++)
        for (int k = 0; k < 8; k++) { const pi_t *pi = &MI[m].syn[k]; if (pi->s4[0] != '-') ms_pic_of(pi, 0); }
    for (int i = 0; i < P_LFO - 8; i++) ms_pic_of(&PI_COMMON[i], 0);
    for (int i = 0; i < 24; i++) ms_pic_of(&PI_LFO[i], 0);
    for (int i = 0; i < 4; i++) ms_pic_of(&PI_MOD[i], 0);
    for (int i = 0; i < G_COUNT; i++) ms_pic_of(&PI_GLOB[i], 0);
    ms_pic_of(&PI_WBNK, 0);
    ms_pic_of(pinfo(0, SETTING(S_MUTQ)), 0);
}

/* a knob's picture from its value, o its text (as fmt writes it; a destination's name, a channel's number) */
static void g_pic(ms_knob_t *kn, const pi_t *pi, int t, int val, char *o)
{
    const int pic = ms_pic_of(pi, t);
    kn->pic = (uint8_t)pic;
    kn->q = (uint16_t)ms_q(pi, val);
    kn->n = (uint8_t)kind_choices(pi->kind);
    kn->c = (uint8_t)choice_of(pi->kind, val);
    switch (pic) {
    case MP_ENV: kn->sub = (uint8_t)(pi->kind == K_AHLD ? 2 : pi->s4[0] == 'A' && pi->s4[1] == 'T' ? 1 : 0); break;
    case MP_FILT: kn->sub = (uint8_t)(pi->kind == K_FOCT ? 0 : pi->kind == K_FWID ? 1 : pi->s4[0] == 'H' ? 2 : 3); break;
    case MP_WAVE: case MP_DWAV: {                      /* a cycle (its name's end to end, or DIGI-WAV's own from the bank
                                                        * it plays), its lowest and highest on the box's foot and top */
        int32_t s[MS_KS], mn = 0, mx = 0;
        if (pic == MP_WAVE) {
            kn->sub = (uint8_t)((o[0] == 'S' && o[1] == 'Q') || s_is(o, "PULS"));
            for (int i = 0; i < MS_KS; i++) s[i] = wave_at(o, i, MS_KS - 1);        /* (rows, down) */
        } else {
            const int big = (PAT(cur_pat)->kit[t][P_WBNK] * 2) >> 7, w = big ? (val >> 1 > 63 ? 63 : val >> 1) : (val * 31) / 127;
            const int16_t *wv = big ? &T_WAVES64[w * 256] : &T_WAVES[w * 256];
            for (int i = 0; i < MS_KS; i++) s[i] = -wv[i * 256 / MS_KS];
        }
        for (int i = 0; i < MS_KS; i++) { if (!i || s[i] < mn) mn = s[i]; if (!i || s[i] > mx) mx = s[i]; }
        for (int i = 0; i < MS_KS; i++)                 /* (-6..6, rounded; a flat one on the middle row) */
            kn->wy[i] = (int8_t)(mx > mn ? ((s[i] - mn) * 24 + (mx - mn)) / (2 * (mx - mn)) - 6 : 0);
        break;
    }
    case MP_CHOICE: case MP_TEXT: case MP_NUM:
        ms_vfit(o, ms_named(pi->kind) ? MS_NW : MS_KW);
        s_cpyn(kn->val, o, sizeof kn->val);
        break;
    default: break;
    }
}

static void g_knob(ms_view_t *v, const m2k_scr_t *s, int k)
{
    ms_knob_t *kn = &v->knob[k];
    m_set(kn, 0, sizeof *kn);
    kn->pic = MS_FREE;
    if (s->page >= MS_PAGES) return;
    const int code = m2k_screen_param(s->page, s->track, k), t = m2k_screen_knob_track(s->page, s->track, k);
    const pi_t *pi = code == NONE ? 0 : pinfo(t, code);
    if (!pi || pi->s4[0] == '-') return;
    const int val = ms_value(s, t, code);
    char o[32], dn[8];
    const char *u;
    fmt(pi, t, val, o, &u);
    if (pi->kind == K_DEST && val) s_cpyn(o, ms_dest_name(pinfo(t, val - 1), dn), sizeof o);
    if (pi->kind == K_MCH) { char c[32]; s_cpyn(c, o + 2, sizeof c); s_cpy(o, c); }   /* (CH10: the channel's number) */
    const int tempo = ms_move_tempo(s, code);
    if (tempo) ms_bpm_text(o, s->bpm10);
    g_pic(kn, pi, t, val, o);
    if (tempo) kn->q = (uint16_t)CLAMP((s->bpm10 / 10 - 30) * 256 / 255, 0, 256);
    kn->lock = (uint8_t)ms_locked(s, t, code);
}

/* the tempo shown large (Shift + knob 1 on any page, or the page's BPM): its tenths, SYNC (Move's and MONO2K's) or INT
 * (MONO2K's own) beside it, where it sits from 30 to 285 along the foot */
static void g_tempo(ms_touch_t *tc, const m2k_scr_t *s, int k)
{
    const int b10 = s->bpm10 ? s->bpm10 : (int)seq_bpm() * 10;
    tc->k = (int8_t)k;
    s_cpy(tc->name, "TEMPO");
    s_cpy(tc->s4, "BPM");
    ms_bpm_text(tc->val, b10);
    s_cpy(tc->unit, s->sync ? "SYNC" : "INT");
    tc->q = (uint16_t)CLAMP((b10 - 300) * 256 / 2550, 0, 256);
    tc->ruler = 1;
}

/* the knob shown large: its full name (A..H before a machine's, the track before MIX's), its value and unit whole */
static void g_touch(ms_view_t *v, const m2k_scr_t *s)
{
    ms_touch_t *tc = &v->touch;
    m_set(tc, 0, sizeof *tc);
    tc->k = -1;
    if (s->touch < 0 || s->touch > 7) return;
    if (s->tempo) { g_tempo(tc, s, s->touch); return; }
    if (s->page >= MS_PAGES) return;
    const int code = m2k_screen_param(s->page, s->track, s->touch), t = m2k_screen_knob_track(s->page, s->track, s->touch);
    if (code == GL(G_TMPO)) { g_tempo(tc, s, s->touch); return; }
    const pi_t *pi = code == NONE ? 0 : pinfo(t, code);
    if (!pi || pi->s4[0] == '-') return;
    tc->k = s->touch;
    char nm[40], o[32], *e = nm;
    if (s->page == MS_MIX) { *e++ = 'T'; *e++ = (char)('1' + t); *e++ = ' '; }
    else if (code < 8) { *e++ = (char)('A' + code); *e++ = ' '; }
    s_cpy(e, pi->name);
    s_cpyn(tc->name, nm, sizeof tc->name);
    s_cpyn(tc->s4, pi->s4, sizeof tc->s4);
    const int val = ms_value(s, t, code);
    const char *u;
    fmt(pi, t, val, o, &u);
    s_cpyn(tc->val, o, sizeof tc->val);
    s_cpyn(tc->unit, u ? u : "", sizeof tc->unit);
    tc->q = (uint16_t)ms_q(pi, val);
    tc->bip = (uint8_t)bipolar(pi->kind);
    tc->ruler = (uint8_t)!action(pi->kind);
    tc->lock = (uint8_t)ms_locked(s, t, code);
    tc->step = (uint8_t)(t == s->track && s->held < M2K_STEPS ? s->held + 1 : 0);
}

/* the steps held, for their card: what the first holds (the FM-1's step_info). A knob touched keeps the stage: it is
 * what is being locked */
static void g_held(ms_view_t *v, const m2k_scr_t *s)
{
    ms_held_t *h = &v->held;
    m_set(h, 0, sizeof *h);
    if (!s->nheld || s->held >= M2K_STEPS || s->page >= MS_PAGES || v->touch.k >= 0) return;
    const m2k_step_t st = PAT(cur_pat)->step[s->track][s->held];
    h->n = s->nheld;
    h->step = (uint8_t)(s->held + 1);
    h->rtrg = (uint8_t)(st.rtrg ? st.rtrg : 1);
    h->len = s->span;
    s_cpyn(h->cond, COND_N[st.cond < C_COUNT ? st.cond : C_ALWAYS], sizeof h->cond);
    s_note(h->note, st.note);
    if (st.flags & ST_TTYPE) s_cpyn(h->tag[0], TTYPE_N[ttype_of(st.flags)], sizeof h->tag[0]);
    if (st.flags & ST_ACC) s_cpyn(h->tag[1], "ACC", sizeof h->tag[1]);
    if (st.flags & ST_SLD) s_cpyn(h->tag[2], "SLD", sizeof h->tag[2]);
}

/* a machine's model as the screen names it: its voice after the family, the whole name where that is a letter */
static const char *ms_model(int m)
{
    const char *md = model_of(&MI[m]);
    return md[0] && !md[1] ? MI[m].name : md;
}

/* the meters: each track's peak since the last gather, falling an eighth a 33 ms (the FM-1's) */
static int32_t ms_meter[8];
static uint32_t ms_last;
static void g_meters(uint32_t now)
{
    const uint32_t dt = ms_last ? now - ms_last : 33u;
    ms_last = now ? now : 1u;
    for (int t = 0; t < M2K_TRACKS && t < 8; t++) {
        int32_t pk = mon.peak[t] * 2;
        mon.peak[t] = 0;
        ms_meter[t] = pk > ms_meter[t] ? CLAMP(pk, 0, 32767) : ms_meter[t] - ms_meter[t] * (int32_t)(dt > 132 ? 132 : dt) / 264;
    }
}

static void g_home(ms_view_t *v, const m2k_scr_t *s)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    for (int t = 0; t < M2K_TRACKS && t < 8; t++) {
        const int len = tlen_of(t), pr = PROB_PCT[p->glob[G_TPRB1 + t] & 15], ln = lane_of(p, t);
        uint16_t tr = 0, ho = 0;
        int n = 0;
        for (int i = 0; i < 16; i++) {
            const int st = s->spage * 16 + i;
            if (st >= len) break;
            n++;
            int hit;
            if (ln) {
                const uint32_t sl = (uint32_t)seq_len(), k = !sq.playing ? 0 : spd_of(t) == 4 ? sq.n1x : sq.tk[t];
                hit = grids_hit(p, ln - 1, (k ? (k - 1) / sl * sl : 0) + (uint32_t)st, 0) != 0;
            } else {
                const m2k_step_t *sp = &p->step[t][st];
                hit = (sp->flags & ST_TRIG) != 0;
                if (hit && (sp->cond != C_ALWAYS || pr < 100)) ho |= (uint16_t)(1u << i);   /* (it may not play) */
            }
            if (hit) tr |= (uint16_t)(1u << i);
        }
        v->st.home.trig[t] = tr;
        v->st.home.hollow[t] = ho;
        v->st.home.len[t] = (uint8_t)n;
        const int tp = sq.playing ? track_pos(t) - s->spage * 16 : -1;
        v->st.home.pos[t] = (int8_t)(tp >= 0 && tp < 16 ? tp : -1);
    }
}

static void g_stage(ms_view_t *v, const m2k_scr_t *s)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = s->track;
    ms_stage_t *st = &v->st;
    m_set(st, 0, sizeof *st);
    switch (s->page) {
    case MS_HOME: g_home(v, s); break;
    case MS_SYN: {
        const mi_t *mi = &MI[p->machine[t]];
        s_cpyn(st->syn.cls, mi->cls, sizeof st->syn.cls);
        s_cpyn(st->syn.style, s_is(mi->style, mi->cls) ? "" : mi->style, sizeof st->syn.style);
        s_cpyn(st->syn.model, ms_model(p->machine[t]), sizeof st->syn.model);
        break;
    }
    case MS_AMP:
        st->amp.a = (uint8_t)ms_value(s, t, P_ATK); st->amp.h = (uint8_t)ms_value(s, t, P_HOLD);
        st->amp.d = (uint8_t)ms_value(s, t, P_DEC); st->amp.r = (uint8_t)ms_value(s, t, P_REL);
        st->amp.st = mon.astage[t];
        st->amp.lv = mon.aenv[t];
        break;
    case MS_FILT: {                                   /* the cutoffs where the knobs have them, or the voice's now */
        const int base = ms_value(s, t, P_BASE), wdth = ms_value(s, t, P_WDTH);
        const int db = ms_value(s, t, P_BOF) - 64, dw = ms_value(s, t, P_WOF) - 64;
        const int ob = 7 * db * (db < 0 ? -db : db), ow = 7 * dw * (dw < 0 ? -dw : dw);
        const int hp0 = filt_q3(t, base << 8), lp0 = filt_q3(t, (base + wdth) << 8);
        const int live = trk[t].sounding && (mon.cut_hp[t] != hp0 || mon.cut_lp[t] != lp0);
        st->filt.hp = (int16_t)(live ? mon.cut_hp[t] : hp0);
        st->filt.lp = (int16_t)(live ? mon.cut_lp[t] : lp0);
        st->filt.hq = (uint8_t)ms_value(s, t, P_HPQ);
        st->filt.lq = (uint8_t)ms_value(s, t, P_LPQ);
        st->filt.hp1 = st->filt.lp1 = -1;
        if (ob || ow) {                                /* how far the envelope takes them */
            const int hp1 = filt_q3(t, (base << 8) + ob), lp1 = filt_q3(t, ((base + wdth) << 8) + ob + ow);
            if (hp1 != hp0) st->filt.hp1 = (int16_t)hp1;
            if (lp1 != lp0) st->filt.lp1 = (int16_t)lp1;
        }
        break;
    }
    case MS_EFX:
        st->efx.f = (uint8_t)ms_value(s, t, P_EQF); st->efx.g = (uint8_t)ms_value(s, t, P_EQG);
        st->efx.dt = (uint8_t)ms_value(s, t, P_DTIM); st->efx.snd = (uint8_t)ms_value(s, t, P_DSND);
        st->efx.fb = (uint8_t)ms_value(s, t, P_DFB); st->efx.wid = (uint8_t)ms_value(s, t, P_DWID);
        break;
    case MS_DSTR:
        st->dstr.bits = (uint8_t)ms_value(s, t, P_BITS); st->dstr.fold = (uint8_t)ms_value(s, t, P_FOLD);
        st->dstr.xr = (uint8_t)ms_value(s, t, P_XOR); st->dstr.type = (uint8_t)ms_value(s, t, P_MODT);
        st->dstr.dep = (uint8_t)ms_value(s, t, P_MODD); st->dstr.mix = (uint8_t)ms_value(s, t, P_MODM);
        st->dstr.inc = T_MOD_INC[ms_value(s, t, P_MODR)];
        break;
    case MS_TRK: {
        const int n = sq.nheld[t], rng = 1 + ((ms_value(s, t, P_ARNG) * 4) >> 7), tot = n * rng;
        st->arp.mode = (uint8_t)((ms_value(s, t, P_AMOD) * 6) >> 7);
        st->arp.n = (uint8_t)n;
        st->arp.rng = (uint8_t)rng;
        st->arp.hold = sq.arp_hold[t];
        st->arp.on = (int16_t)(sq.arp_i[t] && tot ? (int)((sq.arp_i[t] - 1) % (uint32_t)tot) : -1);
        break;
    }
    case MS_LFO1: case MS_LFO2: case MS_LFO3: {
        const int l = s->page - MS_LFO1, base = P_LFO + 8 * l, dest = ms_value(s, t, base + L_DEST);
        char o[24];
        const char *u;
        st->lfo.on = (uint8_t)(dest != 0);
        s_cpyn(st->lfo.dest, dest ? ms_label(pinfo(t, dest - 1)) : "OFF", sizeof st->lfo.dest);
        s_lfo_cycle(o, &u, ms_value(s, t, base + L_SPD), choice_of(K_MULT, ms_value(s, t, base + L_MULT)));
        if (*u && (s_is(u, "BAR") || s_is(u, "BARS") || s_is(u, "HZ"))) {
            char *e = o; while (*e) e++;
            *e++ = ' '; s_cpyn(e, u, (int)(o + sizeof o - e));
        }
        s_cpyn(st->lfo.cyc, o, sizeof st->lfo.cyc);
        st->lfo.wave = (uint8_t)((ms_value(s, t, base + L_WAVE) * 7) >> 7);
        st->lfo.dp = T_LDPTH[ms_value(s, t, base + L_DPTH)];
        st->lfo.ph = (uint16_t)(trk[t].lph[l] >> 16);
        st->lfo.val = trk[t].lval[l];
        break;
    }
    case MS_PAT:
        st->tm.bpm10 = (uint16_t)(s->bpm10 ? s->bpm10 : seq_bpm() * 10u);
        st->tm.sync = s->sync;
        st->tm.beat = (int8_t)(sq.playing ? (sq.step ? sq.step - 1 : 0) / 4 % 4 : -1);
        st->tm.len = (uint8_t)seq_len();
        st->tm.tlen = (uint8_t)tlen_of(t);
        st->tm.ppos = (int8_t)(sq.playing ? play_step() : -1);
        st->tm.tpos = (int8_t)(sq.playing ? track_pos(t) : -1);
        break;
    case MS_GRID: {
        const int cyc = grids_cyc(p), kn = sq.playing ? (int)sq.n1x - 1 : -1, base = kn < 0 ? 0 : kn - kn % cyc;
        st->grid.x8 = (uint8_t)gr8(p->glob[G_GRX]);
        st->grid.y8 = (uint8_t)gr8(p->glob[G_GRY]);
        st->grid.lane = (uint8_t)lane_of(p, t);
        st->grid.cyc = (uint8_t)cyc;
        st->grid.kn = (int8_t)(kn < 0 ? -1 : kn - base);
        for (int part = 0; part < 3; part++)
            for (int i = 0; i < cyc && i < 32; i++) {
                int lv = 0;
                if (grids_hit(p, part, (uint32_t)(base + i), &lv)) st->grid.hit[part] |= 1u << i;
                st->grid.lv[part][i] = (uint8_t)CLAMP(lv, 0, 255);
            }
        break;
    }
    case MS_MIX:
        for (int k = 0; k < M2K_TRACKS && k < 8; k++) st->mix.meter[k] = (uint8_t)(ms_meter[k] >> 7);
        break;
    case MS_GLO:
        st->glo.verb = p->glob[G_VERB]; st->glo.damp = p->glob[G_DAMP];
        st->glo.adpc = p->glob[G_ADPC]; st->glo.driv = p->glob[G_DRIV];
        break;
    case MS_PERF:
        for (int k = 0; k < PF_COUNT && k < 32; k++) if (perf.on[k]) st->perf.on |= 1u << k;
        st->perf.pg = s->perf_pg;
        break;
    case MS_CAT: {                                    /* the library's slots of the category shown, about the one heard */
        const int sel = s->cat_sel % M2K_SOUNDS, cat = s->cat_cat < CAT_COUNT ? s->cat_cat : 0;
        int n = 0, at = 0;
        for (int i = 0; i < M2K_SOUNDS; i++) {
            if (cat && snd[i].name[0] && snd_cat[i] != cat) continue;
            if (i == sel) at = n;
            n++;
        }
        const int first = CLAMP(at - MS_NROW / 2, 0, n > MS_NROW ? n - MS_NROW : 0);
        st->list.n = (uint8_t)(n > 255 ? 255 : n); st->list.first = (uint8_t)first; st->list.at = (uint8_t)at;
        for (int i = 0, k = 0, r = 0; i < M2K_SOUNDS && r < MS_NROW; i++) {
            if (cat && snd[i].name[0] && snd_cat[i] != cat) continue;
            if (k++ < first) continue;
            ms_row_t *rw = &st->list.row[r++];
            rw->idx = (uint8_t)i;
            rw->flags = (uint8_t)(snd[i].name[0] ? 0 : MS_ROW_EMPTY);
            rw->icon = (uint8_t)ms_machine_icon(snd[i].machine < M_COUNT ? snd[i].machine : 0);
            s_cpyn(rw->name, snd[i].name[0] ? snd[i].name : "INIT", sizeof rw->name);
        }
        s_cpyn(st->list.cat, CAT_N[cat], sizeof st->list.cat);
        break;
    }
    case MS_PATS: {                                   /* the patterns about the cursor, the open one and the one queued */
        const int at = s->pat_sel % M2K_PATTERNS, first = CLAMP(at - MS_NROW / 2, 0, M2K_PATTERNS - MS_NROW);
        st->list.n = M2K_PATTERNS; st->list.first = (uint8_t)first; st->list.at = (uint8_t)at;
        for (int r = 0; r < MS_NROW; r++) {
            ms_row_t *rw = &st->list.row[r];
            const int i = first + r;
            rw->idx = (uint8_t)i;
            rw->flags = (uint8_t)((i == cur_pat ? MS_ROW_OPEN : 0) | (i == sq.queued && i != cur_pat ? MS_ROW_NEXT : 0));
            char nm[9];
            m_cpy(nm, pat_name(i), 8);
            nm[8] = 0;
            int l = 8;
            while (l && (nm[l - 1] == ' ' || !nm[l - 1])) nm[--l] = 0;
            s_cpyn(rw->name, nm[0] ? nm : "INIT", sizeof rw->name);
        }
        break;
    }
    default: break;
    }
}

void m2k_screen_gather(ms_view_t *v, const m2k_scr_t *s)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = s->track < M2K_TRACKS ? s->track : 0;
    g_meters(s->ms);
    mon.lfo_view = (uint8_t)t;
    v->page = s->page < MS_COUNT ? s->page : MS_HOME;
    const int pad = ms_pad_of(v->page);
    v->pad = (uint8_t)(pad < 0 ? 0xFF : pad);
    v->fam = (uint8_t)ms_family(v->page);
    v->perf = s->perf;
    v->track = (uint8_t)t;
    v->ntrk = M2K_TRACKS;
    v->icon = (uint8_t)ms_machine_icon(p->machine[t]);
    v->muted = (uint8_t)mute_will(t);
    v->hold = sq.arp_hold[t];
    v->mutes = 0;
    for (int k = 0; k < M2K_TRACKS && k < 8; k++) if (mute_will(k)) v->mutes |= (uint8_t)(1u << k);
    v->playing = sq.playing;
    v->rec = sq.rec;
    v->shift = s->shift;
    v->mute = s->mute;
    v->clk_int = (uint8_t)!s->sync;
    v->pads = s->pads;
    v->bank = s->bank;
    v->spage = s->spage;
    const int vl = seq_len() > tlen_of(t) ? seq_len() : tlen_of(t);
    v->npage = (uint8_t)((vl + 15) / 16);
    s_pat(v->slot, cur_pat);
    if (sq.queued != cur_pat) s_pat(v->next, sq.queued); else v->next[0] = 0;
    char nm[9];
    m_cpy(nm, pat_name(cur_pat), 8);
    nm[8] = 0;
    s_cpyn(v->pname, nm, sizeof v->pname);
    s_cpyn(v->toast, s->toast ? s->toast : "", sizeof v->toast);
    s_cpyn(v->toast2, s->toast2 ? s->toast2 : "", sizeof v->toast2);
    v->ms = s->ms;
    m2k_scr_t ss = *s;
    ss.track = (uint8_t)t;
    ss.page = v->page;
    for (int k = 0; k < 8; k++) g_knob(v, &ss, k);
    g_touch(v, &ss);
    g_held(v, &ss);
    g_stage(v, &ss);
}

/* ========================================================================================== the draw (view) */

/* ---------------------------------------------------------------------------------------- the knobs' pictures */

/* The knob row: 8 columns of 16, each knob's picture in the same 13 x 13 box, at columns 2..14 of its 16 and rows
 * 49..61: every box shares one top row and one bottom row, and two boxes are 3 px apart. One language: a thin line is
 * the range or the shape, solid is the value (a dial's band, a fader's fill, a choice's place, a switch on, a pan's
 * handle); nothing dotted. A value written instead (a name, a short number, a choice that fits) sits centred in the
 * box, its capitals on its middle rows, alone. */

/* where (dx, dy) from a centre points: clockwise from 12 o'clock, 1024 a turn (atan to within a degree, integers) */
static int ms_ang(int dx, int dy)
{
    const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, mn = ax < ay ? ax : ay, mx = ax < ay ? ay : ax;
    const int t = mx ? (mn << 8) / mx : 0;                         /* (the octant's tan, Q8) */
    const int o = (t >> 1) + ((89 * t * (256 - t)) >> 17);        /* (its angle, 128 the octant) */
    const int a = ax <= ay ? o : 256 - o;
    if (dx >= 0) return dy <= 0 ? a : 512 - a;
    return dy > 0 ? 512 + a : (1024 - a) & 1023;
}

/* the dial, a knob seen from above: its ring whole (r 6); inside it the band, solid, from where the sweep starts (7:30,
 * round 270 degrees to 4:30) to the value, a two-sided one's from 12 o'clock; the pointer from the middle out */
static void ms_dial(int X, int Y, int q, int bip)
{
    const int c = MS_KS / 2, pv = q * 3;                           /* (the value along the sweep, 0..768) */
    const int lo = bip && pv > 384 ? 384 : bip ? pv : 0, hi = bip && pv < 384 ? 384 : pv;
    for (int j = 0; j < MS_KS; j++)
        for (int i = 0; i < MS_KS; i++) {
            const int dx = i - c, dy = j - c, d2 = dx * dx + dy * dy;
            if (d2 > 30 && d2 <= 40) ms_px(X + i, Y + j, 1);         /* (the ring) */
            else if (d2 > 20 && d2 <= 29) {                         /* (the band's ring, just inside) */
                const int p = (ms_ang(dx, dy) - 640) & 1023;
                if (p >= lo && p <= hi) ms_px(X + i, Y + j, 1);
            }
        }
    const uint32_t ph = (uint32_t)((640 + pv) & 1023) << 22;
    ms_line(X + c, Y + c, X + c + ((sinp(ph) * 5 + 16384) >> 15), Y + c - ((sinp(ph + 0x40000000u) * 5 + 16384) >> 15), 1);
}

/* a knob's picture in its box (X, Y its top left) */
static void ms_pic(const ms_knob_t *kn, int X, int Y)
{
    const int n = kn->n, c = kn->c, pic = kn->pic, sub = kn->sub, q = CLAMP(kn->q, 0, 256), e = MS_KS - 1, m = MS_KS / 2;
    switch (pic) {
    case MP_DIAL: case MP_BIDIAL: ms_dial(X, Y, q, pic == MP_BIDIAL); break;
    case MP_SLIDER: {                        /* a fader: its slot, solid from the floor to the level */
        const int f = (q * 9 + 128) >> 8;
        ms_rframe(X + 3, Y, 7, MS_KS, 1);
        if (f) ms_rect(X + 5, Y + e - 1 - f, 3, f, 1);
        break;
    }
    case MP_ENV: {                           /* the segment, its run the value: a rise then held, a hold, a fall then out */
        const int L = (q * e + 128) >> 8;
        if (sub == 2) { ms_vline(X, Y, MS_KS, 1); ms_hline(X, Y, L + 1, 1); ms_vline(X + L, Y, MS_KS, 1); }
        else if (sub == 1) { ms_line(X, Y + e, X + L, Y, 1); ms_hline(X + L, Y, MS_KS - L, 1); }
        else { ms_line(X, Y, X + L, Y + e, 1); ms_hline(X + L, Y + e, MS_KS - L, 1); }
        break;
    }
    case MP_FILT: {                          /* the band: the passband on the box's top row from lo to hi, each edge a
                                              * slope 3 wide down to the foot (an open side runs out flat); a resonance
                                              * a solid corner under its edge, as big as the Q (BASE moves the low edge,
                                              * WDTH the high one, HPQ the low edge's corner, LPQ the high edge's) */
        int lo = 3, hi = e, kl = 0, kh = 0;
        if (sub == 0) lo = 3 + ((q * 9 + 128) >> 8);
        else if (sub == 1) hi = 4 + ((q * 5 + 128) >> 8);
        else if (sub == 2) kl = (q * 6 + 128) >> 8;
        else { lo = 0; hi = e - 3; kh = (q * 6 + 128) >> 8; }
        ms_hline(X + lo, Y, hi - lo + 1, 1);
        if (lo) ms_line(X + lo - 3, Y + e, X + lo, Y, 1);
        if (hi < e) ms_line(X + hi, Y, X + hi + 3, Y + e, 1);
        for (int j = 1; j < kl; j++) ms_hline(X + lo - j / 4, Y + j, kl - j + j / 4, 1);   /* (from the slope in) */
        for (int j = 1; j < kh; j++) ms_hline(X + hi - (kh - 1 - j), Y + j, kh - j + j / 4, 1);
        break;
    }
    case MP_WAVE: case MP_DWAV: {            /* a cycle of the wave across the box, top to foot (g_pic fits it) */
        const int sq = pic == MP_WAVE && (sub & 1);
        int py = Y + m + kn->wy[0];
        ms_px(X, py, 1);
        for (int i = 1; i < MS_KS; i++) {
            const int wy = Y + m + kn->wy[i];
            if (sq) { ms_px(X + i, py, 1); ms_vline(X + i, wy < py ? wy : py, (wy < py ? py - wy : wy - py) + 1, 1); }
            else ms_line(X + i - 1, py, X + i, wy, 1);
            py = wy;
        }
        break;
    }
    case MP_PAN: {                           /* a line, its middle marked; the place a solid handle the box's height, in
                                              * a dark ring */
        const int p = CLAMP((q * e + 128) >> 8, 1, e - 1);
        ms_hline(X, Y + m, MS_KS, 1);
        ms_vline(X + m, Y + m - 3, 7, 1);
        ms_rect(X + p - 2, Y, 5, MS_KS, 0);
        ms_rect(X + p - 1, Y, 3, MS_KS, 1);
        break;
    }
    case MP_SEGS: {                          /* a choice too wide to write (up to 6): a floor, a tick a place on it, the
                                              * one chosen standing solid, the box's height and at least 3 wide */
        if (!n) break;
        const int pt = (MS_KS + 1) / n, cw = pt - 1, x1 = X + (MS_KS - (n * pt - 1)) / 2, w = cw < 3 ? 3 : cw;
        ms_hline(X, Y + e, MS_KS, 1);
        for (int i = 0; i < n; i++) {
            const int a = x1 + i * pt, mid = a + (cw - 1) / 2;
            if (i == c) ms_rect(cw < 3 ? mid - 1 : a, Y, w, MS_KS, 1); else ms_vline(mid, Y + e - 2, 2, 1);
        }
        break;
    }
    case MP_TOGGLE:                          /* a switch: off, its frame and its knob at the left; on, solid, the knob
                                              * dark at the right */
        if (c) { ms_rfill(X, Y + m - 3, MS_KS, 7, 1); ms_rect(X + e - 4, Y + m - 1, 3, 3, 0); }
        else { ms_rframe(X, Y + m - 3, MS_KS, 7, 1); ms_rect(X + 2, Y + m - 1, 3, 3, 1); }
        break;
    case MP_CHOICE: case MP_TEXT: case MP_NUM:   /* the value's word, alone, centred: its capitals the box's rows 4..8 */
        ms_text_c(X + m, Y + m + MS_CAP - 2, kn->val, 1);
        break;
    }
}

/* the eight knobs' pictures, alone in their row (their names show only while touched): a held step's lock a line over
 * its box, the knob touched a line under it, each as wide as the box */
static void d_knobs(const ms_view_t *v)
{
    for (int k = 0; k < 8; k++) {
        const ms_knob_t *kn = &v->knob[k];
        const int X = k * 16 + MS_KX;
        if (kn->pic == MS_FREE) continue;
        ms_pic(kn, X, MS_KP);
        if (kn->lock) ms_hline(X, MS_LOCK_Y, MS_KS, 1);
        if (v->touch.k == k) ms_hline(X, MS_H - 1, MS_KS, 1);
    }
}

/* ------------------------------------------------------------------------------------------------- header */

static char *ms_tname(char *o, int t) { o[0] = 'T'; o[1] = (char)('1' + t); o[2] = 0; return o; }

/* the pad leading to the page in view: the Move's two page rows as two rows of eight, the top row (the sound) over
 * the second (the rest); each pad a dot, the page's a block, PERFORM's too while its keys are on */
static void d_padmap(const ms_view_t *v)
{
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 8; c++) {
            const int i = (r ? 0 : 8) + c, x = c * 3, y = 1 + r * 3;
            if (i == v->pad || (i == 7 && v->perf)) ms_rect(x, y, 2, 2, 1);
            else ms_px(x, y + 1, 1);
        }
}

/* the pad and the page's tag (filled for a sound page, underlined for the rest), the track where the page is a
 * track's, then what has room: its machine's icon, a mute, PADS' four pads (the keyboard rows are the tracks), an
 * extra word (HOME's pattern name); at the right the transport, INT when MONO2K plays its own tempo (SYNC, Move's, is
 * the rule and says nothing), REC, the pattern (the one queued instead, after a >; Shift held, the bank; Mute held,
 * MUTE), the bars when there are more than one (the one the steps show filled) and there is room. The right is drawn
 * first: the left's extras give way to it */
static int ms_hdr_gap;                 /* (the audit's: the clear columns between the two ends) */
static void d_header(const ms_view_t *v, const char *title, int show_track, const char *extra)
{
    int rl = MS_W - 5;                                       /* the right part's leftmost lit column */
    ms_icon(v->playing ? I_PLAY : I_STOP, rl, MS_HB, 1);
    if (v->clk_int) { rl -= 3 + ms_tw("INT"); ms_text(rl, MS_HB, "INT", 1); }
    if (v->rec) { rl -= 8; ms_icon(I_REC, rl, MS_HB, 1); }
    char o[16];
    if (v->shift) { char *e = s_cpy(o, "BANK "); *e++ = (char)('A' + v->bank); *e = 0; }
    else if (v->mute) s_cpy(o, "MUTE");
    else if (v->next[0]) { o[0] = '>'; s_cpy(o + 1, v->next); }   /* (the pattern queued: it comes at the bar) */
    else s_cpy(o, v->slot);
    rl -= 4 + ms_tw(o);
    ms_text(rl, MS_HB, o, 1);
    char tn[4];
    ms_tname(tn, v->track);
    const int lmin = (v->pad != 0xFF ? 28 : 0) + ms_tw(title) + 8 + (show_track ? ms_tw(tn) + 3 : 0);   /* (the left's least) */
    if (v->npage > 1 && !v->shift && !v->mute && rl - 2 - 3 * v->npage - (lmin - 3) >= 3) {   /* the bars: 2 x 3 cells */
        rl -= 2 + 3 * v->npage;
        for (int b = 0; b < v->npage; b++) {
            const int bx = rl + b * 3;
            if (b == v->spage) ms_rect(bx, MS_HB - 3, 2, 3, 1); else ms_px(bx, MS_HB - 1, 1);
        }
    }
    int x = 0;                                               /* the left part's next free column */
    if (v->pad != 0xFF) { d_padmap(v); x = 28; }
    x += ms_tag(x, MS_HB, title, v->fam == MS_FAM_SOUND) + 4;
    if (show_track) {
        x += ms_text(x, MS_HB, tn, 1) + 3;
        if (x + 7 + 3 <= rl) { ms_icon(v->icon, x, MS_HB, 1); x += 7 + 3; }
        if (v->muted && x + 7 + 3 <= rl) { ms_icon(I_MUTE, x, MS_HB, 1); x += 7 + 3; }
    }
    if (v->pads && x + 5 + 3 <= rl) { ms_icon(I_PADS, x, MS_HB, 1); x += 5 + 3; }
    if (extra && extra[0] && x + ms_tw(extra) + 3 <= rl) x += ms_text(x, MS_HB, extra, 1) + 3;
    ms_hdr_gap = rl - (x - 3);                               /* (each left item leaves 3 or 4 columns after it) */
}

/* ------------------------------------------------------------------------------------------------- the stage */

/* a polyline's next point */
typedef struct { int x, y, on; } ms_pen_t;
static void ms_pen(ms_pen_t *pn, int x, int y)
{
    if (pn->on) ms_line(pn->x, pn->y, x, y, 1); else ms_px(x, y, 1);
    pn->x = x; pn->y = y; pn->on = 1;
}

/* AMP: the envelope, attack hold decay release on the voice's own curves (env_level), the lengths on the FM-1's log
 * scale; its area a tint, the segment touched in hairlines; a handle where each ends; the voice live a dotted cursor
 * and a marker on its level */
static void d_amp(const ms_view_t *v)
{
    const int X = 1, W = 126, top = MS_ST_T + 1, yb = MS_ST_B - 1, tk = v->touch.k;
    const int a = v->st.amp.a, h = v->st.amp.h, d = v->st.amp.d, r = v->st.amp.r;
    const int dec_inf = d == 127, fk = tk >= 0 && tk < 4 ? tk : -1, pk = yb - top;
    int wa = 6 + a * 24 / 127, wh = h == 127 ? 18 : 2 + h * 18 / 127, wd = dec_inf ? 18 : 12 + d * 30 / 127, wr = 8 + r * 24 / 127;
    const int tot = wa + wh + wd + wr, PW = W - 4;
    wa = wa * PW / tot; wh = wh * PW / tot; wd = wd * PW / tot; wr = PW - wa - wh - wd;
    int xs[5], ys[5];
    xs[0] = X + 1; xs[1] = xs[0] + wa; xs[2] = xs[1] + wh; xs[3] = xs[2] + wd; xs[4] = xs[3] + wr;
    for (int b = 0; b < 5; b++) ys[b] = yb;
    ms_pen_t pn = {0, 0, 0};
    for (int x = xs[0]; x <= xs[4]; x++) {
        int seg = x < xs[1] ? 0 : x < xs[2] ? 1 : x < xs[3] ? 2 : 3, len = xs[seg + 1] - xs[seg];
        int q = (x - xs[seg]) * 256 / (len ? len : 1);
        int y = yb - pk * env_level(seg, q, dec_inf, !a) / 256;
        if (x == xs[4]) y = yb;
        ms_fill(x, X, y + 1, yb, fk < 0 ? 1 : seg == fk ? 2 : 1);
        ms_pen(&pn, x, y);
        for (int b = 1; b < 5; b++) if (x == xs[b]) ys[b] = y;
    }
    ms_dots_h(X - 1, yb + 1, W + 2);
    for (int b = 1; b < 4; b++) ms_handle(xs[b], ys[b], b - 1 == fk);
    const int st = v->st.amp.st, lv = v->st.amp.lv;
    if (st) {
        const int ww[4] = {xs[1] - xs[0], xs[2] - xs[1], xs[3] - xs[2], xs[4] - xs[3]};
        int dn = lv ? (ilog2_q8(32768) - ilog2_q8((uint32_t)lv)) : 8 * 256;
        int ra = !a ? lv : (int)(atk_from((uint32_t)lv << 15) >> 15);
        int x = st == 1 ? xs[0] + ww[0] * ra / 32768 : st == 2 ? xs[1] + ww[1] / 2
              : st == 3 ? xs[2] + ww[2] * CLAMP(dn, 0, 512) / 512 : xs[3] + ww[3] * CLAMP(dn - (dec_inf ? 0 : 512), 0, 2048) / 2048;
        ms_dots_v(x, top, yb - top);
        ms_marker(x, yb - (pk * lv >> 15));
    }
}

/* FILT: the chain's response (m2k_ui.c's filt_db), one curve: the knobs' band, or while the track sounds where its
 * envelope and LFOs have it now; how far the envelope takes each cutoff dotted; the cutoffs' handles */
static void d_filt(const ms_view_t *v)
{
    const int X = 1, W = 126, top = MS_ST_T + 1, yb = MS_ST_B - 1, fk = v->touch.k;
    const int hp = v->st.filt.hp, lp = v->st.filt.lp, hq = v->st.filt.hq, lq = v->st.filt.lq;
    ms_pen_t pn = {0, 0, 0};
    for (int i = 0; i < W; i++) {
        int q = i * FQ_MAX / (W - 1), y = filt_row(filt_db(q, hp, lp, hq, lq), top, yb);
        ms_fill(X + i, X, y + 1, yb + 1, fk >= 0 && fk < 4 ? 2 : 1);   /* (the band's knobs touched: hairlines) */
        ms_pen(&pn, X + i, y);
    }
    ms_dots_h(X, filt_row(0, top, yb), W);
    ms_dots_h(X - 1, yb + 1, W + 2);
    if (v->st.filt.hp1 >= 0) ms_dots_v(X + v->st.filt.hp1 * (W - 1) / FQ_MAX, top, yb - top);
    if (v->st.filt.lp1 >= 0) ms_dots_v(X + v->st.filt.lp1 * (W - 1) / FQ_MAX, top, yb - top);
    const int xh = X + hp * (W - 1) / FQ_MAX, xl = X + lp * (W - 1) / FQ_MAX;
    const int yh = filt_row(filt_db(hp, hp, lp, hq, lq), top, yb), yl = filt_row(filt_db(lp, hp, lp, hq, lq), top, yb);
    if (fk == 1) ms_hline(xh, yb + 1, xl - xh + 1, 1);
    ms_handle(xh, yh, fk == 0 || fk == 2);
    ms_handle(xl, yl, fk == 3);
}

/* EFX: the EQ's band about its 0 dB line at the left; the delay's echoes at the right, each as loud as the feedback
 * leaves it, about a middle line when the send is the ping-pong's (each side in turn, as wide as DWID), one after the
 * other DTIM apart */
static void d_efx(const ms_view_t *v)
{
    const int f = v->st.efx.f, g = v->st.efx.g - 64, fk = v->touch.k, mid = (MS_ST_T + MS_ST_B) / 2;
    const int X = 1, W = 40;
    const int fx = f * (W - 1) / 127, bw = W / 3;
    ms_pen_t pn = {0, 0, 0};
    for (int i = 0; i < W; i++) {
        int d = i - fx, bump = d > -bw && d < bw ? (g * (bw - (d < 0 ? -d : d))) / bw : 0, y = mid - bump * 12 / 64;
        ms_pen(&pn, X + i, y);
        if (fk == 0 || fk == 1) ms_fill(X + i, X, y < mid ? y + 1 : mid + 1, y < mid ? mid : y, 2);
    }
    ms_dots_h(X, mid, W);
    ms_handle(X + fx, mid - g * 12 / 64, fk == 0 || fk == 1);
    const int dt = v->st.efx.dt, snd = v->st.efx.snd - 64, fb = v->st.efx.fb, wid = v->st.efx.wid;
    const int DX = 52, DW = 75, H = 13, pp = snd > 0, ghost = !snd;
    int lvl = ghost ? 256 : CLAMP(128 + 128 * (snd < 0 ? -snd : snd) / 64, 0, 256);   /* (the send: the first's height) */
    ms_vline(DX, mid - H, 2 * H + 1, 1);                 /* the sound itself */
    ms_vline(DX + 1, mid - H, 2 * H + 1, 1);
    if (pp) ms_dots_h(DX + 3, mid, DW - 3); else ms_dots_h(DX + 3, mid + H, DW - 3);
    for (int e = 0; e < 16 && lvl >= 8 && dt; e++) {   /* (no send: where they would be, dotted) */
        int x = DX + 4 + (dt * (e + 1) * (DW - 6)) / 256;
        if (x >= DX + DW) break;
        int hgt = (H * lvl) >> 8, y0, h;
        if (pp) {                                        /* each side in turn, its width the swing */
            int off = hgt * (64 + wid) / 191;
            y0 = (e & 1) ? mid : mid - off; h = off + 1;
        } else { y0 = mid + H - 2 * hgt; h = 2 * hgt + 1; }
        if (ghost) ms_dots_v(x, y0, h); else ms_vline(x, y0, h, 1);
        if (fk == 5 || fk == 6) ms_vline(x + 1, y0, h, 1);
        lvl = CLAMP((lvl * fb) >> 6, 0, 256);
    }
    if (!dt) ms_text_c(DX + DW / 2, mid + 3, "COMB", 1);
}

/* DSTR: a sine through the track's destruction (FOLD, XOR, BITS) at the left; the modulation effect's sweep at its
 * rate, as deep as DPTH, moving, at the right, its mix a line at its foot */
static void d_dstr(const ms_view_t *v)
{
    const int bits = 16 - (v->st.dstr.bits * 14 >> 7), fold = v->st.dstr.fold, xr = v->st.dstr.xr;
    const int xm = (xr * xr * 2) & 0x7FFF, mid = (MS_ST_T + MS_ST_B) / 2, X = 1, W = 58, A = 13;
    ms_pen_t pn = {0, 0, 0};
    for (int i = 0; i < W; i++) {
        int32_t s = sinp((uint32_t)i * (0xFFFFFFFFu / W) * 2) * 7 / 8;
        if (fold) s = fold16((s * (256 + fold * 6)) >> 8);
        if (xm) s = (int16_t)((uint16_t)s ^ (uint16_t)xm);
        if (bits < 16) s = (s >> (16 - bits)) * (1 << (16 - bits));
        int y = mid - (int)((s * A) >> 15);
        if (pn.on && bits < 9 && y != pn.y) ms_line(pn.x, pn.y, X + i, pn.y, 1);
        ms_pen(&pn, X + i, y);
    }
    ms_dots_h(X, mid, W);
    const int type = (v->st.dstr.type * 3) >> 7, dep = v->st.dstr.dep, mix = v->st.dstr.mix;
    const int MX = 68, MW = 59, amp = 2 + dep * 11 / 127, my = mid - 1;
    const uint32_t ph = v->ms * (v->st.dstr.inc * 44u);
    pn.on = 0;
    for (int i = 0; i < MW; i++) {
        int32_t tri = (int32_t)((ph + (uint32_t)i * (0xFFFFFFFFu / MW) * (type == 2 ? 2u : 1u)) >> 16);
        tri = tri < 32768 ? tri : 65535 - tri;
        ms_pen(&pn, MX + i, my + amp - (int)((tri * amp * 2) >> 15));
    }
    ms_hline(MX, MS_ST_B, mix * MW / 127, 1);
}

/* TRK: the arp's notes in its order, a step each rising (its range's octaves up the staircase), the one sounding lit;
 * HOLD in a tag; nothing held: what to do */
static void d_trk(const ms_view_t *v)
{
    const int n = v->st.arp.n, tot = n * v->st.arp.rng, mid = (MS_ST_T + MS_ST_B) / 2;
    if (v->st.arp.hold) ms_tag(MS_W - ms_tw("HOLD") - 4, MS_ST_T + 6, "HOLD", 1);
    if (!v->st.arp.mode) { ms_text_c(64, mid + 3, "ARP OFF", 1); return; }
    if (!n) { ms_text_c(64, mid + 3, v->st.arp.hold ? "PLAY TO HOLD" : "PLAY THE KEYS", 1); return; }
    const int yb = MS_ST_B - 1, cnt = tot < 32 ? tot : 32, pitch = CLAMP(120 / cnt, 3, 16), bw = pitch - 2;
    const int x0 = (MS_W - cnt * pitch + 2) / 2, on = v->st.arp.on;
    for (int i = 0; i < cnt; i++) {
        int x = x0 + i * pitch, y = yb - 3 - i * 20 / (cnt > 1 ? cnt - 1 : 1);
        if (i == on) ms_rect(x, y, bw, 3, 1); else ms_rframe(x, y, bw, 3, 1);
        if (i == on) ms_dots_v(x + bw / 2, y + 4, yb - y - 4);
    }
    ms_dots_h(0, yb + 1, MS_W);
}

/* LFO: where it goes at the left and its cycle at the right, small; its wave over a cycle as deep as DPTH swings it,
 * from a dotted zero line, live (a dotted cursor where its phase is, a marker on what it sends) */
static void d_lfo(const ms_view_t *v)
{
    char o[24];
    char *e = s_cpy(o, v->st.lfo.on ? "TO " : "");
    s_cpy(e, v->st.lfo.dest);
    ms_text(1, MS_ST_T + 5, o, 1);
    ms_text_r(MS_W - 2, MS_ST_T + 5, v->st.lfo.cyc, 1);
    const int X = 1, W = 126, mid = MS_ST_T + 20, A = 10, wave = v->st.lfo.wave;
    const int32_t dp = v->st.lfo.dp;
    ms_dots_h(X, mid, W);
    if (!v->st.lfo.on) return;
    ms_pen_t pn = {0, 0, 0};
    int32_t rr = 0;
    for (int i = 0; i < W; i++) {
        int32_t s = wave == 6 ? (int32_t)(hash32((uint32_t)(i / 9) + 77u) >> 16) - 32768
                              : lfo_shape(wave, (uint32_t)i * (0xFFFFFFFFu / (uint32_t)(W - 1)), &rr);
        int y = mid - (int)((((s * dp) >> 14) * A) >> 15);
        if (pn.on && wave == 6 && y != pn.y) ms_line(pn.x, pn.y, X + i, pn.y, 1);
        ms_pen(&pn, X + i, y);
        ms_fill(X + i, X, y < mid ? y + 1 : mid + 1, y < mid ? mid : y, v->touch.k == 7 ? 2 : 1);
    }
    int cx = X + (int)(((uint32_t)v->st.lfo.ph * (uint32_t)(W - 1)) >> 16);
    int cy = mid - (int)((((v->st.lfo.val * dp) >> 14) * A) >> 15);
    ms_dots_v(cx, mid - A - 1, 2 * A + 3);
    ms_marker(cx, cy);
}

/* PAT: the tempo large (SYNC's link over BPM when it is the Move's, INT when MONO2K's own), the bar's four beats
 * under it, lit as they pass;
 * at the right the pattern's length and the track's, each a line as long as its steps (64 the whole), the playhead a
 * marker on each */
static void d_pat(const ms_view_t *v)
{
    char o[12];
    ms_bpm_text(o, v->st.tm.bpm10 / 10 * 10);
    const int b = MS_ST_T + 19;
    int x = 2 + ms_text_z(2, b, o, 3, 1) + 4;
    ms_text(x, b, "BPM", 1);
    if (v->st.tm.sync) ms_icon(I_SYNC, x, b - 9, 1);
    else ms_text(x, b - 9, "INT", 1);
    for (int i = 0; i < 4; i++) { int bx = 2 + i * 14; if (i == v->st.tm.beat) ms_rect(bx, MS_ST_B - 5, 12, 5, 1); else ms_rframe(bx, MS_ST_B - 5, 12, 5, 1); }
    const int LX = 80, LW = 46;
    static const char *const N[2] = {"PAT", "TRK"};
    for (int r = 0; r < 2; r++) {
        const int len = r ? v->st.tm.tlen : v->st.tm.len, pos = r ? v->st.tm.tpos : v->st.tm.ppos, y = MS_ST_T + 9 + r * 14;
        const int w = len * LW / 64;
        ms_text(LX - 16, y + 3, N[r], 1);
        ms_hline(LX, y, w, 1);
        ms_dots_h(LX + w, y, LW - w);
        if (pos >= 0 && pos < len) ms_marker(LX + pos * LW / 64, y);
    }
}

/* GRID: Grids' map, 25 nodes a dot each, the four round X and Y larger, the place a marker on its dotted cross; the
 * three lanes' cycles at the right as they play, a bar a step as tall as its level, lit where it trigs, the step
 * playing a mark under it; the lane the track follows in a tag */
static void d_grid(const ms_view_t *v)
{
    const int X = 2, Y = MS_ST_T + 3, S = 6;
    const int x8 = v->st.grid.x8, y8 = v->st.grid.y8, i0 = x8 >> 6, j0 = y8 >> 6;
    const int qx = X + x8 * S / 64, qy = Y + y8 * S / 64;
    ms_dots_h(X, qy, 4 * S + 1);
    ms_dots_v(qx, Y, 4 * S + 1);
    for (int j = 0; j < 5; j++)
        for (int i = 0; i < 5; i++) {
            int cx = X + S * i, cy = Y + S * j;
            if ((i == i0 || i == i0 + 1) && (j == j0 || j == j0 + 1)) ms_rect(cx - 1, cy - 1, 3, 3, 1); else ms_px(cx, cy, 1);
        }
    ms_marker(qx, qy);
    static const char *const N[3] = {"BD", "SD", "HH"};
    const int cyc = v->st.grid.cyc ? v->st.grid.cyc : 16, pw = 80 / cyc;
    for (int l = 0; l < 3; l++) {
        const int b = MS_ST_T + 8 + l * 11, x = 48, h = 7;
        if (v->st.grid.lane == l + 1) ms_tag(32, b, N[l], 1); else ms_text(34, b, N[l], 1);
        for (int i = 0; i < cyc; i++) {
            const int bh = (v->st.grid.lv[l][i] * h + 254) / 255, bx = x + i * pw;
            if ((v->st.grid.hit[l] >> i) & 1) ms_rect(bx, b - 1 - bh, pw > 2 ? pw - 1 : 1, bh, 1);
            else if (bh) ms_vline(bx, b - 1 - bh, bh, 1);
            if (i == v->st.grid.kn) ms_hline(bx, b + 1, pw > 2 ? pw - 1 : 1, 1);
        }
    }
}

/* SYN: the machine: its class and style small, its voice's model three times the type */
static void d_syn(const ms_view_t *v)
{
    char o[28];
    char *e = s_cpy(o, v->st.syn.cls);
    if (v->st.syn.style[0]) { *e++ = ' '; s_cpy(e, v->st.syn.style); }
    ms_text(2, MS_ST_T + 7, o, 1);
    const char *md = v->st.syn.model;
    ms_text_z(2, MS_ST_B - 2, md, ms_tw_z(md, 3) <= 120 ? 3 : 2, 1);
}

/* MIX: a strip a track under its knob: its number (the track in view's in a tag), its level live as a bar from the
 * floor; a muted track its speaker struck instead */
static void d_mix(const ms_view_t *v)
{
    const int top = MS_ST_T + 9, yb = MS_ST_B, H = yb - top;
    for (int k = 0; k < v->ntrk && k < 8; k++) {
        const int x0 = k * 16, cx = x0 + 8;
        char n[2] = {(char)('1' + k), 0};
        if (k == v->track) { ms_rfill(cx - 3, MS_ST_T, 7, 7, 1); ms_text_c(cx + 1, MS_ST_T + 6, n, 0); }
        else ms_text_c(cx + 1, MS_ST_T + 6, n, 1);
        if ((v->mutes >> k) & 1) { ms_icon(I_MUTE, cx - 3, yb, 1); continue; }
        const int h = v->st.mix.meter[k] * H / 255;
        if (h) ms_rect(cx - 1, yb - h + 1, 3, h, 1);
        ms_hline(cx - 2, yb + 1, 5, 1);
    }
}

/* GLO: the bus: the reverb's tail over time (0.1 .. 30 s on a log scale, 60 dB down the stage) across the width, DAMP's
 * top dotted, DRIV lifting it, ADPC stepping it */
static void d_glo(const ms_view_t *v)
{
    const int verb = v->st.glo.verb, ad = v->st.glo.adpc, X = 2, W = 124, top = MS_ST_T + 1, ty = MS_ST_B - 1, dh = ty - top;
    if (!verb) { ms_text_c(64, (top + ty) / 2 + 3, "NO REVERB", 1); ms_dots_h(X, ty + 1, W); return; }
    const uint32_t t60 = T_RV_MS[verb], th = (t60 * 256u) / (256u + T_RV_DA[v->st.glo.damp]);
    const int dg = (ilog2_q8(T_DRIV[v->st.glo.driv]) - 8 * 256) * dh / 2560;
    const int sq_h = ad ? 1 + (ad * 2 > 127 ? 127 : ad * 2) * 3 / 127 : 1, sq_w = ad ? (T_ADPC_DIV[ad] >> 8) + 1 : 1;
    int hold = 0;
    ms_pen_t pn = {0, 0, 0};
    for (int i = 0; i < W; i++) {
        uint32_t e = (uint32_t)i * 2105u / (uint32_t)(W - 1), ms = (100u * T_EXP2F[e & 255]) >> (16 - (e >> 8));
        int yl = (int)(ms * (uint32_t)dh / (t60 ? t60 : 1)) - dg, yh = (int)(ms * (uint32_t)dh / (th ? th : 1)) - dg;
        yl = CLAMP(yl, 0, dh); yh = yh < 0 ? 0 : yh;
        if (sq_h > 1) { if (!(i % sq_w)) hold = yl / sq_h * sq_h; yl = hold; }
        ms_fill(X + i, X, top + yl + 1, ty, 1);
        ms_pen(&pn, X + i, top + yl);
        if (yh <= dh && !((X + i + top + yh) & 1)) ms_px(X + i, top + yh, 1);
    }
    ms_dots_h(X, ty + 1, W);
}

/* PERFORM: the keyboard's two rows as the Move has them, the colours over the moves (the moves' page the arrows
 * show), the effects playing lit, the rest framed; what plays named at the left, large while one
 * alone */
static void d_perf(const ms_view_t *v)
{
    const int X = 64, Y = MS_ST_T + 6, pg = v->st.perf.pg;
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 8; c++) {
            const int e = mv_perf_key(r ? c : 8 + c, pg), x = X + c * 8, y = Y + r * 9;
            if (e < 0) { ms_px(x + 3, y + 3, 1); continue; }
            if ((v->st.perf.on >> e) & 1) ms_rect(x, y, 6, 6, 1); else ms_frame(x, y, 6, 6, 1);
        }
    ms_text(X, MS_ST_B - 1, pg ? "MOVES 2" : "MOVES 1", 1);
    int n = 0, last = -1;
    for (int k = 0; k < PF_COUNT; k++) if ((v->st.perf.on >> k) & 1) { n++; last = k; }
    if (!v->perf) { ms_text(2, MS_ST_T + 7, "KEYS OFF", 1); return; }
    if (!n) { ms_text(2, MS_ST_T + 7, "HOLD A KEY", 1); return; }
    if (n == 1) { ms_text_z(2, MS_ST_T + 16, PF_NAME[last], ms_tw_z(PF_NAME[last], 2) <= 58 ? 2 : 1, 1); return; }
    for (int k = 0, r = 0; k < PF_COUNT && r < 3; k++) if ((v->st.perf.on >> k) & 1) ms_text(2, MS_ST_T + 7 + 9 * r++, PF_NAME[k], 1);
}

/* ---------------------------------------------------------------------------------------- a knob touched */

/* the stage gives way to the knob: its full name, LOCK when a held step's lock is what it shows; its value three times
 * the type, its unit beside it; where it sits along the foot */
static void d_touched(const ms_view_t *v, int y0, int y1)
{
    const ms_touch_t *tc = &v->touch;
    ms_rect(0, y0, MS_W, y1 - y0 + 1, 0);
    const int b0 = y0 + 6;
    int xr = MS_W - 1;
    if (tc->lock) xr -= ms_tag(xr - ms_tw("LOCK") - 3, b0, "LOCK", 1) + 4;
    if (tc->step) { char st[12]; s_int(s_cpy(st, "STEP "), tc->step); xr -= ms_text_r(xr, b0, st, 1) + 6; }
    ms_text(2, b0, ms_tw(tc->name) > xr - 2 ? tc->s4 : tc->name, 1);
    const int vb = y1 - 6;
    int z = 3;
    if (ms_tw_z(tc->val, 3) + (tc->unit[0] ? ms_tw(tc->unit) + 4 : 0) > MS_W - 4) z = 2;
    int w = ms_text_z(2, vb, tc->val, z, 1);
    if (tc->unit[0]) ms_text(2 + w + 4, vb, tc->unit, 1);
    if (!tc->ruler) return;
    const int q = CLAMP(tc->q, 0, 256), fx = 2 + (q * 123 + 128) / 256, ry = y1 - 1;
    ms_dots_h(2, ry, 124);
    if (tc->bip) { if (fx > 64) ms_hline(64, ry, fx - 64, 1); else ms_hline(fx, ry, 64 - fx, 1); ms_vline(64, ry - 1, 3, 1); }
    else ms_hline(2, ry, fx - 2, 1);
    ms_rect(CLAMP(fx - 1, 2, 124), ry - 2, 2, 4, 1);
}

/* ---------------------------------------------------------------------------------------- a step held */

/* the card's columns: the condition, the note, the ratchet */
#define MS_HC0 2
#define MS_HC1 62
#define MS_HC2 102

/* the stage gives way to a step held past a tap, as to a knob touched: which step (and how many more) and its length
 * if it has one, its trig type, accent and slide in tags where they fit; at x2 what the first holds, each over what
 * turns it */
static void d_held(const ms_view_t *v, int y0, int y1)
{
    const ms_held_t *h = &v->held;
    ms_rect(0, y0, MS_W, y1 - y0 + 1, 0);
    const int b0 = y0 + 6, vb = y0 + 21, hb = y0 + 30;
    char o[20], r[3] = {'X', (char)('0' + h->rtrg), 0};
    char *e = s_int(s_cpy(o, "STEP "), h->step);
    if (h->n > 1) { e = s_cpy(e, " +"); e = s_int(e, h->n - 1); }
    if (h->len) s_int(s_cpy(e, " LEN "), h->len);
    ms_text(2, b0, o, 1);
    const int xl = 2 + ms_tw(o) + 4;
    int xr = MS_W - 1;
    for (int i = 2; i >= 0; i--) {
        if (!h->tag[i][0] || xr - ms_tw(h->tag[i]) - 4 < xl) continue;
        xr -= ms_tag(xr - ms_tw(h->tag[i]) - 4, b0, h->tag[i], 1) + 2;
    }
    ms_text_z(MS_HC0, vb, h->cond, 2, 1);
    ms_text_z(MS_HC1, vb, h->note, 2, 1);
    ms_text_z(MS_HC2, vb, r, 2, 1);
    ms_text(MS_HC0, hb, "SHIFT+JOG", 1);
    ms_text(MS_HC1, hb, "JOG", 1);
    ms_text(MS_HC2, hb, "UP/DN", 1);
}

/* ------------------------------------------------------------------------------------------------- HOME */

/* the eight tracks as lanes, Hortator's and the S4's calm: each its number (the track in view's marked by a bar at the
 * edge), the 16 steps the step keys show, grouped by the beat: a trig a block, one that may not play (a condition, a
 * chance) framed, a rest nothing; a muted lane's trigs a dash; each track's playhead a line under its step */
static void d_home(const ms_view_t *v)
{
    for (int t = 0; t < v->ntrk && t < 8; t++) {
        const int y = 13 + t * 6, b = y + 4, muted = (v->mutes >> t) & 1;
        char n[2] = {(char)('1' + t), 0};
        if (t == v->track) ms_rect(0, b - 5, 2, 5, 1);
        ms_text(4, b, n, 1);
        const uint16_t tr = v->st.home.trig[t], ho = v->st.home.hollow[t];
        for (int i = 0; i < v->st.home.len[t]; i++) {
            const int x = 12 + i * 7 + (i >> 2) * 2;
            if ((tr >> i) & 1) {
                if (muted) ms_hline(x, y + 1, 4, 1);
                else if ((ho >> i) & 1) ms_frame(x, y, 4, 3, 1);
                else ms_rect(x, y, 4, 3, 1);
            }
            if (i == v->st.home.pos[t]) ms_hline(x, y + 4, 4, 1);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ the lists */

/* SOUNDS and PATTERNS: the rows about the cursor (the MNM-1's menu, Hortator's library), the cursor's lit in a bar;
 * the scroll a thin track at the edge */
static void d_list(const ms_view_t *v, int pats)
{
    const int n = v->st.list.n, first = v->st.list.first, at = v->st.list.at;
    for (int r = 0; r < MS_NROW && first + r < n; r++) {
        const ms_row_t *rw = &v->st.list.row[r];
        const int b = 17 + r * 9, on = first + r == at;
        char o[8];
        if (pats) s_pat(o, rw->idx); else s_num3(o, rw->idx + 1);
        if (on) ms_rfill(0, b - 6, 122, 7, 1);
        ms_text(2, b, o, !on);
        ms_text(22, b, rw->name, !on);
        if (pats) {
            if (rw->flags & MS_ROW_OPEN) ms_icon(v->playing ? I_PLAY : I_STOP, 113, b, !on);
            else if (rw->flags & MS_ROW_NEXT) ms_text_r(119, b, "NEXT", !on);
        } else if (!(rw->flags & MS_ROW_EMPTY)) ms_icon(rw->icon, 112, b, !on);
    }
    const int top = 11, h = MS_NROW * 9 - 2, sh = n > MS_NROW ? CLAMP(MS_NROW * h / n, 3, h) : h;
    const int sy = n > MS_NROW ? first * (h - sh) / (n - MS_NROW) : 0;
    ms_dots_v(126, top, h);
    ms_rect(125, top + sy, 3, sh, 1);
}

/* ---------------------------------------------------------------------------------------- a message */

/* a moment's message over the stage: its first line large where it fits, framed in a dark box (a line MS_TOAST_TW at
 * most: 4 px clear of the frame each side) */
static void d_toast(const ms_view_t *v)
{
    const int x = 8, y = 17, w = MS_W - 16, h = 24;
    ms_rect(x - 2, y - 2, w + 4, h + 4, 0);
    ms_rframe(x, y, w, h, 1);
    const int big = ms_tw_z(v->toast, 2) <= MS_TOAST_TW;
    if (!v->toast2[0]) {
        if (big) ms_text_z(64 - ms_tw_z(v->toast, 2) / 2, y + 17, v->toast, 2, 1); else ms_text_c(64, y + 15, v->toast, 1);
        return;
    }
    if (big) ms_text_z(64 - ms_tw_z(v->toast, 2) / 2, y + 13, v->toast, 2, 1); else ms_text_c(64, y + 11, v->toast, 1);
    ms_text_c(64, y + 21, v->toast2, 1);
}

/* --------------------------------------------------------------------------------------------- the frame */

void m2k_screen_draw(const ms_view_t *v, uint8_t *fb)
{
    ms_fb = fb;
    m_set(ms_fb, 0, MS_BYTES);
    const int pg = v->page < MS_COUNT ? v->page : MS_HOME;
    switch (pg) {
    case MS_HOME: {
        d_header(v, "HOME", 0, v->pname);
        d_home(v);
        if (v->touch.k >= 0) d_touched(v, MS_ST_T, MS_H - 4);
        if (v->held.n) d_held(v, MS_ST_T, MS_H - 4);
        break;
    }
    case MS_CAT: {
        char tn[16];                                   /* (the category shown, if one; else the track it goes on) */
        if (v->st.list.cat[0] && !s_is(v->st.list.cat, "ALL")) s_cpyn(tn, v->st.list.cat, sizeof tn);
        else ms_tname(tn, v->track);
        d_header(v, "SOUNDS", 0, tn);
        d_list(v, 0);
        break;
    }
    case MS_PATS:
        d_header(v, "PATTERNS", 0, 0);
        d_list(v, 1);
        break;
    default:
        d_header(v, MS_PAGE_N[pg], pg != MS_MIX && pg != MS_GLO && pg != MS_PERF, 0);
        switch (pg) {
        case MS_SYN: d_syn(v); break;
        case MS_AMP: d_amp(v); break;
        case MS_FILT: d_filt(v); break;
        case MS_EFX: d_efx(v); break;
        case MS_DSTR: d_dstr(v); break;
        case MS_TRK: d_trk(v); break;
        case MS_LFO1: case MS_LFO2: case MS_LFO3: d_lfo(v); break;
        case MS_PAT: d_pat(v); break;
        case MS_GRID: d_grid(v); break;
        case MS_MIX: d_mix(v); break;
        case MS_GLO: d_glo(v); break;
        case MS_PERF: d_perf(v); break;
        default: break;
        }
        d_knobs(v);
        if (v->touch.k >= 0) d_touched(v, MS_ST_T, MS_ST_B + 1);
        if (v->held.n) d_held(v, MS_ST_T, MS_ST_B + 1);
        break;
    }
    if (v->toast[0]) d_toast(v);
}

int m2k_screen_text_w(const char *s) { return ms_tw(s ? s : ""); }

const uint8_t *m2k_screen(const m2k_scr_t *s)
{
    static ms_view_t v;
    m2k_screen_gather(&v, s);
    m2k_screen_draw(&v, ms_own);
    return ms_own;
}
