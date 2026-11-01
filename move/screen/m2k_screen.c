/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the 128 x 64 1-bit screen (move/screen/DESIGN.md). Part of the core's unity build: included
 * after firmware/src/m2k_core.c, it reads the core's state and names (m2k_ui.c's pinfo, fmt, icon_of; the sequencer;
 * the monitor) and draws them in its own frame, 1 KB. The type is the owner's (assets/fonts/mono2k-pixel: "Elektron
 * Pixel Font" by savingaurora, CC BY-SA 3.0, adapted by MONO2K under CC BY-SA 4.0) at its native size, x1: F_S's
 * bitmaps, which tools/gen_font.py keeps native and the FM-1 widens. One weight. The icons and pictures are MONO2K's
 * own 1 px line drawings.
 *
 * The grid: 2 px, the FM-1's 4 px at the type's x1 (two design pixels). Every box and rule on even rows and columns,
 * text on a baseline (the row under its capitals, 5 rows tall), so a tag round a word is 7 rows, 0..6. Bands:
 *   0..6     header: the page's tag, the track and its machine | the pattern, the tempo or SYNC, the transport
 *   9..11    the playhead strip: the track's 64 steps at 2 px, its trigs, the page the step keys edit, the playhead
 *   14..38   the stage: the page's one live picture; a knob touched, its name and value large
 *   42..53   the knobs' pictures, 8 columns of 16 under the Move's 8 knobs
 *   57..61   their names (baseline 62); row 63 marks the knob touched
 * HOME, the catalogue, PERFORM and the page list lay the middle out their own way under the same header.
 * Integer only, nothing allocated; the meters' fall is the one state kept between frames. */
#include "m2k_screen.h"

static uint8_t ms_fb[MS_BYTES];

#define MS_RB      16                  /* bytes a row */
#define MS_HB      6                   /* the header's baseline */
#define MS_STRIP   9                   /* the playhead strip's top row (9..11) */
#define MS_ST_T    14                  /* the stage */
#define MS_ST_B    38
#define MS_KP      42                  /* the knobs' pictures, 12 rows */
#define MS_KN      62                  /* the knobs' names' baseline */
#define MS_CAP     5                   /* the capitals' rows at x1 */
#define MS_FROWS   7                   /* a glyph's rows with its descenders */
#define MS_LW      15                  /* a knob's name at most (a 16 px column, 1 px clear of the next's) */

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
/* rounded by a pixel: Elektron's title tags, MONO2K's tags (the corners left as they were) */
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

/* a word in a tag: lit, rounded, 7 rows from b - 6 to b, the word dark 1 px in from each side; its width */
static int ms_tag(int x, int b, const char *s)
{
    int w = ms_tw(s);
    ms_rfill(x, b - 6, w + 4, 7, 1);
    ms_text(x + 2, b, s, 0);
    return w + 4;
}

/* ------------------------------------------------------------------------------------------------- icons */

/* MONO2K's line icons, 7 x 5: the capitals' height, so they sit on a baseline as the type does. 1 px strokes on a
 * 1 px grid; drawn for MONO2K, after no one's set */
enum { I_HOME, I_SYN, I_AMP, I_FILT, I_EFX, I_DSTR, I_ARP, I_LFO, I_GLO, I_GRID, I_SET, I_CAT, I_PERF, I_PLAY,
       I_STOP, I_REC, I_SYNC, I_MUTE, I_SPKR, I_LOCK, I_HOLD, I_PERC, I_NOIS, I_SID, I_FM, I_SAW, I_PULS, I_ENS,
       I_WAV, I_VOX, I_KICK, I_SNAR, I_HATS, I_CYMB, I_CHOR, I_RING, I_DYN, I_COUNT };
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

static const char *const MS_PAGE_N[MS_COUNT] = {"HOME", "SYN", "AMP", "FILT", "EFX", "DSTR", "ARP", "LFO1", "LFO2",
                                                "LFO3", "GLO", "GRID", "SET", "SOUNDS", "PERFORM", "PAGES"};
static const uint8_t MS_PAGE_I[MS_COUNT] = {I_HOME, I_SYN, I_AMP, I_FILT, I_EFX, I_DSTR, I_ARP, I_LFO, I_LFO, I_LFO,
                                            I_GLO, I_GRID, I_SET, I_CAT, I_PERF, I_CAT};

const char *m2k_screen_page_name(int page) { return page >= 0 && page < MS_COUNT ? MS_PAGE_N[page] : "?"; }

int m2k_screen_param(int page, int t, int k)
{
    if (k < 0 || k > 7 || t < 0 || t >= M2K_TRACKS) return NONE;
    switch (page) {
    case MS_HOME: { static const uint8_t g[4] = {G_TLEN1, G_SPD1, G_TPRB1, G_LANE1};
                    static const uint8_t m[4] = {P_LEVL, P_PTCH, GL(G_TMPO), GL(G_SWNG)};
                    return k < 4 ? m[k] : GL(g[k - 4] + t); }
    case MS_SYN: return P_SYN + k;
    case MS_AMP: return P_ATK + k;
    case MS_FILT: return P_BASE + k;
    case MS_EFX: return P_EQF + k;
    case MS_DSTR: return k < 4 ? P_BITS + k : P_MODT + k - 4;
    case MS_ARP: return k < 6 ? P_PTCH + k : NONE;
    case MS_LFO1: case MS_LFO2: case MS_LFO3: return P_LFO + 8 * (page - MS_LFO1) + k;
    case MS_GLO: { static const uint8_t g[8] = {G_TMPO, G_SWNG, G_LEN, G_ACNT, G_VERB, G_DAMP, G_ADPC, G_DRIV};
                   return GL(g[k]); }
    case MS_GRID: return k < 7 ? GL(G_GRX + k) : GL(G_LANE1 + t);
    case MS_SET: { static const uint8_t s[8] = {S_MCH, S_SYNI, S_SYNO, S_NOTO, S_NTIN, S_THRU, S_MUTQ, S_MOTN};
                   return SETTING(s[k]); }
    case MS_PERF: return k < 4 ? BUS_K[k] : NONE;     /* (the bus's four, as on the FM-1's PERFORM) */
    default: return NONE;
    }
}

/* the value a knob shows: the kit's, a global, a setting; a step held, its lock where it has one */
static int ms_value(const m2k_scr_t *s, int code)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    if (code >= 0xC0) return store_.set[code & 0x3F];
    if (code >= 0x80) return p->glob[code & 0x7F];
    if (s->held < M2K_STEPS) { int i = lock_find(s->track, s->held, code); if (i >= 0) return p->lock[i].val; }
    return p->kit[s->track][code];
}
static int ms_locked(const m2k_scr_t *s, int code)
{
    return code < 0x80 && s->held < M2K_STEPS && lock_find(s->track, s->held, code) >= 0;
}

/* a knob's name in a 16 px column: its own where it fits (MS_LW), else the Move's short one, three letters of it
 * (the full name shows large when the knob is touched) */
static const char *ms_label(const pi_t *pi)
{
    static const char *const L[][2] = {
        {"ATCK", "ATK"}, {"HOLD", "HLD"}, {"PORT", "POR"}, {"BASE", "BAS"}, {"WDTH", "WID"}, {"B-OF", "BOF"},
        {"W-OF", "WOF"}, {"DTIM", "DTM"}, {"DSND", "DSN"}, {"DBAS", "DBS"}, {"DWID", "DWD"}, {"FOLD", "FLD"},
        {"VERB", "VRB"}, {"PTCH", "PCH"}, {"LEVL", "LEV"}, {"MODE", "ARP"}, {"GATE", "GAT"}, {"TRCK", "TRK"},
        {"DEST", "DES"}, {"WAVE", "WAV"}, {"MULT", "MUL"}, {"PHAS", "PHS"}, {"DPTH", "DPT"}, {"TYPE", "TYP"},
        {"RATE", "SPD"}, {"SWNG", "SW"}, {"ACNT", "ACC"}, {"DCAY", "DCY"}, {"DAMP", "DMP"}, {"ADPC", "ADP"},
        {"DRIV", "DRV"}, {"CHAOS", "CHA"}, {"TLEN", "TLN"}, {"PROB", "PRB"}, {"LANE", "LAN"}, {"CHAN", "CHN"},
        {"SYIN", "SYI"}, {"SYOU", "SYO"}, {"THRU", "THR"}, {"MUTE", "MUT"}, {"MOTN", "MOT"}, {"DIST", "DST"},
        {"BITS", "BIT"}, {"TRIG", "TRG"}, {"1FIN", "1FI"}, {"CLIK", "CLK"}, {"GRIT", "GRT"}, {"RMS", "R/P"},
        {"1ENV", "1EN"}, {"1FEN", "1FE"}, {"1FRQ", "1FR"}, {"1VEN", "1VE"}, {"1VOL", "1VL"}, {"2ENV", "2EN"},
        {"2FRQ", "2FR"}, {"2VOL", "2VL"}, {"3ENV", "3EN"}, {"3FRQ", "3FR"}, {"TONE", "TON"}, {"TUNE", "TUN"},
        {"CARR", "CAR"}, {"CDEC", "CDC"}, {"OPEN", "OPN"}, {"M1FR", "M1F"}, {"M2FR", "M2F"}, {"CHRL", "CHL"},
        {"CHRW", "CHW"}, {"CLAP", "CLP"}, {"CLEN", "CLN"}, {"CVOL", "CVL"}, {"CONS", "CNS"}, {"COLR", "COL"},
        {"CSPC", "CSP"}, {"DRFT", "DRF"}, {"DTUN", "DTN"}, {"FMFR", "FMF"}, {"GAIN", "GN"}, {"HARM", "HRM"},
        {"META", "MET"}, {"MFRQ", "MFR"}, {"MIX2", "MX2"}, {"MSPR", "MSP"}, {"MSRC", "SRC"}, {"NFLT", "NFL"},
        {"NOIS", "NOI"}, {"PCH2", "PC2"}, {"PCH3", "PC3"}, {"PCH4", "PC4"}, {"PCRV", "CRV"}, {"PDEC", "PDC"},
        {"PDEP", "PDP"}, {"PWAD", "SWP"}, {"PWRS", "RST"}, {"SCAN", "SCN"}, {"SNAP", "SNP"}, {"STEP", "STP"},
        {"STIM", "STM"}, {"STON", "STO"}, {"SUB1", "SB1"}, {"SUB2", "SB2"}, {"SUBX", "SBX"}, {"SYNC", "SNC"},
        {"THRS", "THR"}, {"UNIL", "UNL"}, {"UNIW", "UNW"}, {"UNIX", "UNX"}, {"V-STW", "VSW"}, {"VOC1", "VC1"},
        {"VOC2", "VC2"}, {"VOIC", "VOI"}, {"BANK", "BNK"},
    };
    if (pi->kind == K_NTIN) return "NIN";             /* (GLO > IN's NOTE beside > MIDI's) */
    if (pi->kind == K_ONOFF && s_is(pi->name, "NOTES OUT")) return "NOU";
    if (pi->kind == K_SIDM) return "R/S";             /* (SID-6581's MOD: ring and sync) */
    for (unsigned i = 0; i < sizeof L / sizeof L[0]; i++) if (s_is(pi->s4, L[i][0])) return L[i][1];
    return pi->s4;
}

/* the eight names' places: each centred in its column, then pushed right where it comes within 2 px of the one before,
 * then left where the next pushes back, never out of its own column (a 15 px name beside a 14 takes a pixel of the
 * other's room). xs: each one's x */
static void ms_label_x(const char *const *lb, int *xs)
{
    int w[8];
    for (int k = 0; k < 8; k++) w[k] = lb[k] ? ms_tw(lb[k]) : 0;
    /* 3 px between two names where the row has the room (a word's space is 4: 2 reads as a letter's), else 2 */
    for (int g = 3; g >= 2; g--) {
        for (int k = 0; k < 8; k++) xs[k] = k * 16 + (16 - w[k]) / 2;
        for (int k = 1; k < 8; k++)
            if (lb[k] && lb[k - 1] && xs[k] < xs[k - 1] + w[k - 1] + g)
                xs[k] = CLAMP(xs[k - 1] + w[k - 1] + g, k * 16, k * 16 + 16 - w[k]);
        for (int k = 6; k >= 0; k--)
            if (lb[k] && lb[k + 1] && xs[k] + w[k] + g > xs[k + 1])
                xs[k] = CLAMP(xs[k + 1] - g - w[k], k * 16, k * 16 + 16 - w[k]);
        int ok = 1;
        for (int k = 0; k < 7; k++) if (lb[k] && lb[k + 1] && xs[k + 1] - xs[k] - w[k] < 2) ok = 0;
        if (ok) return;
    }
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

/* a parameter as a value names it (an LFO's destination): its knob's name, else its first three letters */
static const char *ms_dest_name(const pi_t *pi, char *o)
{
    const char *lb = ms_label(pi);
    if (ms_tw(lb) <= 14) return lb;
    s_cpyn(o, pi->s4, 4);
    for (int n = 3; n > 1 && ms_tw(o) > 14; n--) o[n - 1] = 0;
    return o;
}

/* a kind writes its value in its column only if every value it takes fits there whole (else its word would come and
 * go as the knob turns): worked out once a kind, through fmt. The ones read off the moment always fit: a destination's
 * name (its label), a track, a channel */
static uint8_t ms_kfit[128];                          /* 0 not yet, 1 written, 2 drawn */
static int ms_writes(const pi_t *pi, int t)
{
    const int k = pi->kind;
    if (k == K_DEST || k == K_TRK || k == K_MCH) return 1;
    if (k >= 128) return 0;
    if (!ms_kfit[k]) {
        int ok = 1;
        for (int v = kind_min(k); v <= kind_max(k) && ok; v++) {
            char o[32];
            const char *u;
            fmt(pi, t, v, o, &u);
            if (ms_vfit(o, 14)) ok = 0;
        }
        ms_kfit[k] = (uint8_t)(ok ? 1 : 2);
    }
    return ms_kfit[k] == 1;
}

/* the column a knob's picture takes (m2k_ui.c's icon_of, at 16 px): what it is, drawn; a value written where its
 * kind's every value fits, else drawn (a choice's segments, a dial) */
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
        return n && n <= 8 ? MP_SEGS : bipolar(k) ? MP_BIDIAL : MP_DIAL;
    }
}

/* the dial: a 12 px ring, 270 degrees in 24 points from the lower left round over the top, and where the pointer
 * starts toward its middle */
static const int8_t MS_DIAL[24][4] = {
    {1, 9, 4, 7}, {1, 8, 4, 7}, {0, 7, 3, 6}, {0, 6, 3, 6}, {0, 5, 3, 5}, {0, 4, 3, 5}, {1, 3, 4, 4}, {1, 2, 4, 4},
    {2, 1, 4, 4}, {3, 1, 4, 4}, {4, 0, 5, 3}, {5, 0, 5, 3}, {6, 0, 6, 3}, {7, 0, 6, 3}, {8, 1, 7, 4}, {9, 1, 7, 4},
    {10, 2, 7, 4}, {10, 3, 7, 4}, {11, 4, 8, 5}, {11, 5, 8, 5}, {11, 6, 8, 6}, {11, 7, 8, 6}, {10, 8, 7, 7},
    {10, 9, 7, 7}};

/* the rows a 14 x 12 picture is drawn in: column x0 + 1 .. x0 + 14, rows y .. y + 11. q: where the value sits, 0..256;
 * o its text, written (fitted) from tx, or centred where tx < 0 */
static void ms_pic(int pic, const pi_t *pi, int x0, int y, int v, int q, const char *o, int tx)
{
    const int kind = pi->kind, n = kind_choices(kind), c = choice_of(kind, v), X = x0 + 1;
    q = CLAMP(q, 0, 256);
    switch (pic) {
    case MP_DIAL: case MP_BIDIAL: {          /* the arc to the value lit, the rest dotted, the pointer in */
        const int dx = x0 + 2, at = (q * 23 + 128) >> 8, from = pic == MP_BIDIAL ? (at < 12 ? at : 12) : 0;
        const int to = pic == MP_BIDIAL ? (at < 12 ? 11 : at) : at;
        for (int i = 0; i < 24; i++) {
            int lit = i >= from && i <= to;
            if (pic == MP_BIDIAL && at == 11) lit = i == 11;     /* (the middle) */
            if (lit || !(i & 1)) ms_px(dx + MS_DIAL[i][0], y + MS_DIAL[i][1], 1);
        }
        ms_line(dx + MS_DIAL[at][2], y + MS_DIAL[at][3], dx + MS_DIAL[at][0], y + MS_DIAL[at][1], 1);
        if (pic == MP_BIDIAL) { ms_px(dx + 5, y + 2, 1); ms_px(dx + 6, y + 2, 1); }   /* (the middle's notch) */
        break;
    }
    case MP_SLIDER: {                        /* a fader: its slot, lit from the floor to the level */
        ms_rframe(x0 + 5, y, 6, 12, 1);
        int f = (q * 10 + 128) >> 8;
        if (f) ms_rect(x0 + 7, y + 11 - f, 2, f, 1);
        break;
    }
    case MP_ENV: {                           /* the segment, its run the value: a rise, a hold, a fall */
        const int top = y + 1, bot = y + 10, L = 1 + ((q * 12) >> 8), x1 = X + 13;
        const int rise = pi->s4[0] == 'A' && pi->s4[1] == 'T', hold = kind == K_AHLD;
        if (hold) { ms_line(X, bot, X, top, 1); ms_line(X, top, X + L, top, 1); ms_line(X + L, top, X + L, bot, 1); }
        else if (rise) { ms_line(X, bot, X + L, top, 1); if (X + L < x1) ms_line(X + L, top, x1, top, 1); }
        else { ms_line(X, top, X + L, bot, 1); if (X + L < x1) ms_line(X + L, bot, x1, bot, 1); }
        ms_dots_h(X, y + 11, 14);
        break;
    }
    case MP_FILT: {                          /* the band: its edges, a resonance's peak */
        const int x1 = X + 13, top = y + 3, bot = y + 10;
        int lo = X + 1, hi = x1 - 1, pk = 0, at = 0;
        if (kind == K_FOCT) lo = X + 1 + ((q * 7) >> 8);
        else if (kind == K_FWID) { lo = X + 2; hi = lo + 2 + ((q * 8) >> 8); }
        else if (pi->s4[0] == 'H') { lo = X + 4; pk = (q * 3) >> 8; at = 1; }
        else { hi = x1 - 4; pk = (q * 3) >> 8; at = 2; }
        if (lo > X + 1) ms_line(lo - 2, bot, lo, top - (at == 1 ? pk : 0), 1); else ms_line(X, top, lo, top, 1);
        ms_line(lo, top - (at == 1 ? pk : 0), lo + 1, top, 1);
        ms_line(lo + 1, top, hi - 1, top, 1);
        ms_line(hi - 1, top, hi, top - (at == 2 ? pk : 0), 1);
        if (hi < x1 - 1) ms_line(hi, top - (at == 2 ? pk : 0), hi + 2 > x1 ? x1 : hi + 2, bot, 1); else ms_line(hi, top, x1, top, 1);
        ms_dots_h(X, y + 11, 14);
        break;
    }
    case MP_WAVE: {                          /* the wave its name says, a cycle */
        int py = 0;
        const int sq = (o[0] == 'S' && o[1] == 'Q') || s_is(o, "PULS");
        for (int i = 0; i < 14; i++) {
            int wy = y + 5 + (wave_at(o, i, 13) * 5) / 7;
            if (i && !sq) ms_line(X + i - 1, py, X + i, wy, 1);
            else if (i) { ms_line(X + i - 1, py, X + i, py, 1); ms_line(X + i, py, X + i, wy, 1); }
            else ms_px(X, wy, 1);
            py = wy;
        }
        break;
    }
    case MP_DWAV: {                          /* DIGI-WAV's wave itself, from the bank it plays */
        const int big = (PAT(cur_pat)->kit[0][P_WBNK] * 2) >> 7, w = big ? (v >> 1 > 63 ? 63 : v >> 1) : (v * 31) / 127;
        const int16_t *wv = big ? &T_WAVES64[w * 256] : &T_WAVES[w * 256];
        int py = 0;
        for (int i = 0; i < 14; i++) {
            int wy = y + 5 - (wv[i * 256 / 14] * 5) / 32768;
            if (i) ms_line(X + i - 1, py, X + i, wy, 1); else ms_px(X, wy, 1);
            py = wy;
        }
        break;
    }
    case MP_PAN: {                           /* a dotted line, its middle, the place a handle */
        ms_dots_h(X, y + 6, 14);
        ms_vline(x0 + 7, y + 4, 5, 1);
        int p = X + ((q * 12 + 128) >> 8);
        ms_rect(p - 1, y + 2, 4, 9, 0);
        ms_rframe(p - 1, y + 2, 4, 9, 1);
        break;
    }
    case MP_SEGS: {                          /* a choice too wide to write: its places, the one chosen standing up */
        const int cw = (15 - (n - 1)) / n, x1 = X + (14 - (n * cw + n - 1)) / 2;
        for (int i = 0; i < n; i++) {
            int a = x1 + i * (cw + 1);
            if (i == c) ms_rect(a, y + 2, cw, 8, 1); else ms_hline(a, y + 9, cw, 1);
        }
        break;
    }
    case MP_TOGGLE: {                        /* a switch: off, a dot at its left; on, lit with a dark dot at its right */
        if (c) { ms_rfill(x0 + 2, y + 3, 12, 7, 1); ms_rect(x0 + 9, y + 5, 3, 3, 0); }
        else { ms_rframe(x0 + 2, y + 3, 12, 7, 1); ms_rect(x0 + 4, y + 5, 3, 3, 1); }
        break;
    }
    case MP_CHOICE: case MP_TEXT: case MP_NUM: {   /* the value's word, and where it is among its kind's */
        char t[16];
        s_cpyn(t, o, sizeof t);
        ms_vfit(t, 14);
        if (tx < 0) ms_text_c(x0 + 8, y + 6, t, 1); else ms_text(tx, y + 6, t, 1);
        if (pic == MP_CHOICE) {
            const int cw = (15 - (n - 1)) / n, x1 = X + (14 - (n * cw + n - 1)) / 2;
            for (int i = 0; i < n; i++) {
                int a = x1 + i * (cw + 1);
                if (i == c) ms_rect(a, y + 9, cw, 3, 1); else ms_hline(a, y + 11, cw, 1);
            }
        } else if (!action(kind)) {
            const int bip = bipolar(kind), f = (q * 13 + 128) >> 8;
            ms_dots_h(X, y + 11, 14);
            if (bip) { int m = X + 7; if (X + f > m) ms_hline(m, y + 11, X + f - m, 1); else ms_hline(X + f, y + 11, m - X - f, 1);
                       ms_vline(m, y + 10, 2, 1); }
            else ms_hline(X, y + 11, f, 1);
            ms_rect(X + f - (f > 12), y + 9, 2, 3, 1);
        }
        break;
    }
    }
}

/* where a knob's value sits, 0..256 (PROB by its chance) */
static int ms_q(const pi_t *pi, int v)
{
    const int mx = kind_max(pi->kind), mn = kind_min(pi->kind);
    if (pi->kind == K_PROB) return PROB_PCT[v & 15] * 256 / 100;
    return CLAMP(((v - mn) * 256) / (mx - mn ? mx - mn : 1), 0, 256);
}

/* a free knob: a dotted square, an unused slot */
static void ms_free(int x0, int y)
{
    for (int i = 0; i < 8; i += 2) { ms_px(x0 + 4 + i, y + 2, 1); ms_px(x0 + 5 + i, y + 9, 1); ms_px(x0 + 4, y + 3 + i, 1); ms_px(x0 + 11, y + 2 + i, 1); }
}

/* the page's eight knobs under the Move's eight: each column its picture (pic 0: the names alone, HOME's), its name;
 * a held step's lock, a line over its name; the knob touched, a line under it */
static const pi_t *ms_knob_pi(const m2k_scr_t *s, int k)
{
    const int code = m2k_screen_param(s->page, s->track, k);
    const pi_t *pi = code == NONE ? 0 : pinfo(s->track, code);
    return pi && pi->s4[0] != '-' ? pi : 0;
}

static void ms_knobs(const m2k_scr_t *s, int pic)
{
    const char *lb[8], *vp[8];
    char val[8][16];
    int xs[8], vx[8], pk[8];
    for (int k = 0; k < 8; k++) {
        const pi_t *pi = ms_knob_pi(s, k);
        lb[k] = pi ? ms_label(pi) : 0;
        vp[k] = 0;
        if (!pi || !pic) continue;
        /* the values the columns write: fitted, then spaced as the names are */
        const int v = ms_value(s, m2k_screen_param(s->page, s->track, k));
        const char *u;
        char dn[8];
        fmt(pi, s->track, v, val[k], &u);
        if (pi->kind == K_DEST && v) s_cpyn(val[k], ms_dest_name(pinfo(s->track, v - 1), dn), sizeof val[k]);
        if (pi->kind == K_MCH) s_cpy(val[k], val[k] + 2);   /* (CH10: the channel's number) */
        pk[k] = ms_pic_of(pi, s->track);
        if (pk[k] == MP_CHOICE || pk[k] == MP_TEXT || pk[k] == MP_NUM) { ms_vfit(val[k], 14); vp[k] = val[k]; }
    }
    ms_label_x(lb, xs);
    ms_label_x(vp, vx);
    for (int k = 0; k < 8; k++) {
        const int x0 = k * 16, code = m2k_screen_param(s->page, s->track, k);
        const pi_t *pi = ms_knob_pi(s, k);
        if (!pi) { if (pic) ms_free(x0, MS_KP); continue; }
        const int v = ms_value(s, code);
        if (pic) ms_pic(pk[k], pi, x0, MS_KP, v, ms_q(pi, v), val[k], vp[k] ? vx[k] : -1);
        /* (a tag round each would run into its neighbour's: the names are 2 or 3 px apart) */
        ms_text(xs[k], MS_KN, lb[k], 1);
        if (ms_locked(s, code)) ms_hline(xs[k], MS_KN - 7, ms_tw(lb[k]), 1);
        if (s->touch == k) ms_hline(xs[k], MS_KN + 1, ms_tw(lb[k]), 1);
    }
}

/* ------------------------------------------------------------------------------------------------- header */

/* a track's tag: T and its number */
static char *ms_tname(char *o, int t) { o[0] = 'T'; o[1] = (char)('1' + t); o[2] = 0; return o; }

/* a machine's model as the header and HOME name it: its voice after the family (DYN of FM+DYN: the icon says FM),
 * the whole name where that is a letter (VOX-6) */
static const char *ms_model(int m)
{
    const char *md = model_of(&MI[m]);
    return md[0] && !md[1] ? MI[m].name : md;
}

/* the page's tag (its title); the track, a mute and an arp holding, its machine's icon and model; at the right the
 * pattern, REC, the tempo (SYNC's link before it when it follows the Move), the transport */
static void ms_header(const m2k_scr_t *s, const char *title, int show_track)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    int x = ms_tag(0, MS_HB, title) + 4;
    int xr = MS_W - 1;
    ms_icon(sq.playing ? I_PLAY : I_STOP, xr - 4, MS_HB, 1);
    xr -= 5 + 4;
    char o[12];
    s_int(o, (int)seq_bpm());
    xr -= ms_text_r(xr, MS_HB, o, 1) + 2;
    if (s->sync) { ms_icon(I_SYNC, xr - 6, MS_HB, 1); xr -= 7; }
    xr -= 4;
    if (sq.rec) { ms_icon(I_REC, xr - 4, MS_HB, 1); xr -= 5 + 4; }
    s_pat(o, cur_pat);
    xr -= ms_text_r(xr, MS_HB, o, 1) + 4;
    if (!show_track) return;
    const int t = s->track, m = p->machine[t];
    char tn[4];
    x += ms_text(x, MS_HB, ms_tname(tn, t), 1) + 3;
    if ((p->mute >> t) & 1) { ms_icon(I_MUTE, x, MS_HB, 1); x += 7 + 3; }
    if (sq.arp_hold[t]) { ms_icon(I_ARP, x, MS_HB, 1); x += 7 + 3; }
    ms_icon(ms_machine_icon(m), x, MS_HB, 1);
    x += 7 + 3;
    if (x + ms_tw(ms_model(m)) <= xr) ms_text(x, MS_HB, ms_model(m), 1);
}

/* ---------------------------------------------------------------------------------------- the playhead strip */

/* the track's steps at 2 px a step, the whole 64 across: each trig a tick 2 rows tall, each beat of its length a dot
 * on the floor, the 16 the step keys edit a line under them; the playhead a block 2 x 4 */
static void ms_strip(const m2k_scr_t *s)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = s->track, len = tlen_of(t), y = MS_STRIP, tp = sq.playing ? track_pos(t) : -1;
    for (int st = 0; st < len && st < 64; st++) {
        const int x = st * 2;
        if (p->step[t][st].flags & ST_TRIG) ms_vline(x, y, 2, 1);
        if (!(st & 3)) ms_px(x, y + 2, 1);
    }
    const int a = s->spage * 16, b = a + 16 < len ? a + 16 : len;
    if (b > a) ms_hline(a * 2, y + 2, (b - a) * 2 - 1, 1);
    if (tp >= 0 && tp < 64) ms_rect(tp * 2, y - 1, 2, 4, 1);
}

/* ------------------------------------------------------------------------------------------------- the stage */

/* a polyline's next point, and its column's fill */
typedef struct { int x, y, on; } ms_pen_t;
static void ms_pen(ms_pen_t *pn, int x, int y)
{
    if (pn->on) ms_line(pn->x, pn->y, x, y, 1); else ms_px(x, y, 1);
    pn->x = x; pn->y = y; pn->on = 1;
}

/* AMP: the envelope, attack hold decay release on the voice's own curves (env_level), the lengths on the FM-1's log
 * scale; its area a tint, the segment touched in hairlines; a handle where each ends; the voice live a dotted cursor
 * and a marker on its level */
static void ms_amp(const m2k_scr_t *s)
{
    const int t = s->track, X = 1, W = 126, top = MS_ST_T + 1, yb = MS_ST_B - 1;
    const int a = ms_value(s, P_ATK), h = ms_value(s, P_HOLD), d = ms_value(s, P_DEC), r = ms_value(s, P_REL);
    const int dec_inf = d == 127, fk = s->touch >= 0 && s->touch < 4 ? s->touch : -1, pk = yb - top;
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
    const int st = mon.astage[t], lv = mon.aenv[t];
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

/* FILT: the chain's response (m2k_ui.c's filt_db: the two-pole high-pass and low-pass at their resonances and the
 * one-poles), one curve: the knobs' band, or while the track sounds where its envelope and LFOs have it now; how far
 * the envelope takes each cutoff dotted; the cutoffs' handles; the decades ticked under the floor */
static void ms_filt(const m2k_scr_t *s)
{
    const int t = s->track, X = 1, W = 126, top = MS_ST_T + 1, yb = MS_ST_B - 2, fk = s->touch;
    const trk_t *tk = &trk[t];
    const int base = ms_value(s, P_BASE), wdth = ms_value(s, P_WDTH), hq = ms_value(s, P_HPQ), lq = ms_value(s, P_LPQ);
    const int db = ms_value(s, P_BOF) - 64, dw = ms_value(s, P_WOF) - 64;
    const int ob = 7 * db * (db < 0 ? -db : db), ow = 7 * dw * (dw < 0 ? -dw : dw);
    const int hp0 = filt_q3(t, base << 8), lp0 = filt_q3(t, (base + wdth) << 8);
    const int live = tk->sounding && (mon.cut_hp[t] != hp0 || mon.cut_lp[t] != lp0);
    const int hp = live ? mon.cut_hp[t] : hp0, lp = live ? mon.cut_lp[t] : lp0;
    ms_pen_t pn = {0, 0, 0};
    for (int i = 0; i < W; i++) {
        int q = i * FQ_MAX / (W - 1), y = filt_row(filt_db(q, hp, lp, hq, lq), top, yb);
        ms_fill(X + i, X, y + 1, yb + 1, fk >= 0 && fk < 4 ? 2 : 1);   /* (the band's knobs touched: hairlines) */
        ms_pen(&pn, X + i, y);
    }
    ms_dots_h(X, filt_row(0, top, yb), W);
    ms_dots_h(X - 1, yb + 1, W + 2);
    static const int16_t DEC[3] = {236, 575, 913};       /* 100 Hz, 1 kHz, 10 kHz */
    for (int d = 0; d < 3; d++) ms_vline(X + DEC[d] * (W - 1) / FQ_MAX, yb + 2, 2, 1);
    if (ob || ow) {
        const int hp1 = filt_q3(t, (base << 8) + ob), lp1 = filt_q3(t, ((base + wdth) << 8) + ob + ow);
        if (hp1 != hp0) ms_dots_v(X + hp1 * (W - 1) / FQ_MAX, top, yb - top);
        if (lp1 != lp0) ms_dots_v(X + lp1 * (W - 1) / FQ_MAX, top, yb - top);
    }
    const int xh = X + hp * (W - 1) / FQ_MAX, xl = X + lp * (W - 1) / FQ_MAX;
    const int yh = filt_row(filt_db(hp, hp, lp, hq, lq), top, yb), yl = filt_row(filt_db(lp, hp, lp, hq, lq), top, yb);
    if (fk == 1) ms_hline(xh, yb + 1, xl - xh + 1, 1);
    ms_handle(xh, yh, fk == 0 || fk == 2);
    ms_handle(xl, yl, fk == 3);
}

/* EFX: under EQF EQG SRR the EQ's band about its 0 dB line and the rate's steps on a sine; under the delay's five
 * the echoes, each as loud as the feedback leaves it, about a middle line when the send is the ping-pong's (each
 * side in turn, as wide as DWID), one after the other DTIM apart */
static void ms_efx(const m2k_scr_t *s)
{
    const int f = ms_value(s, P_EQF), g = ms_value(s, P_EQG) - 64, srr = ms_value(s, P_SRR), fk = s->touch;
    const int X = 1, W = 44, em = MS_ST_T + 6, sm = MS_ST_B - 6;
    const int fx = f * (W - 1) / 127, bw = W / 3;
    ms_pen_t pn = {0, 0, 0};
    for (int i = 0; i < W; i++) {
        int d = i - fx, bump = d > -bw && d < bw ? (g * (bw - (d < 0 ? -d : d))) / bw : 0, y = em - bump * 6 / 64;
        ms_pen(&pn, X + i, y);
    }
    ms_dots_h(X, em, W);
    ms_handle(X + fx, em - g * 6 / 64, fk == 0 || fk == 1);
    const int nst = 3 + (127 - srr) * 20 / 127;
    pn.on = 0;
    for (int i = 0; i < W; i++) {
        int qx = (i * nst / W) * W / nst, y = sm - (sinp((uint32_t)qx * (0xFFFFFFFFu / W)) * 5 >> 15);
        if (pn.on && y != pn.y) ms_line(pn.x, pn.y, X + i, pn.y, 1);
        ms_pen(&pn, X + i, y);
        if (fk == 2) ms_fill(X + i, X, y, sm, 2);
    }
    ms_dots_h(X, sm, W);
    /* the delay */
    const int dt = ms_value(s, P_DTIM), snd = ms_value(s, P_DSND) - 64, fb = ms_value(s, P_DFB), wid = ms_value(s, P_DWID);
    const int DX = 50, DW = 76, mid = (MS_ST_T + MS_ST_B) / 2, H = 11, pp = snd > 0, ghost = !snd;
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

/* DSTR: a sine through the track's destruction (FOLD, XOR, BITS; as the FM-1's DSTR draws it) over the left half; the
 * modulation effect's sweep at its rate, as deep as DPTH, moving, over the right, its mix a line at the foot */
static void ms_dstr(const m2k_scr_t *s)
{
    const int bits = 16 - (ms_value(s, P_BITS) * 14 >> 7), fold = ms_value(s, P_FOLD), xr = ms_value(s, P_XOR);
    const int xm = (xr * xr * 2) & 0x7FFF, mid = (MS_ST_T + MS_ST_B) / 2, X = 1, W = 60, A = 11;
    ms_pen_t pn = {0, 0, 0};
    for (int i = 0; i < W; i++) {
        int32_t v = sinp((uint32_t)i * (0xFFFFFFFFu / W) * 2) * 7 / 8;
        if (fold) v = fold16((v * (256 + fold * 6)) >> 8);
        if (xm) v = (int16_t)((uint16_t)v ^ (uint16_t)xm);
        if (bits < 16) v = (v >> (16 - bits)) * (1 << (16 - bits));
        int y = mid - (int)((v * A) >> 15);
        if (pn.on && bits < 9 && y != pn.y) ms_line(pn.x, pn.y, X + i, pn.y, 1);
        ms_pen(&pn, X + i, y);
    }
    ms_dots_h(X, mid, W);
    const int type = (ms_value(s, P_MODT) * 3) >> 7, dep = ms_value(s, P_MODD), mix = ms_value(s, P_MODM);
    const int MX = 66, MW = 61, amp = 2 + dep * 9 / 127, my = mid - 1;
    const uint32_t ph = (uint32_t)s->ms * (T_MOD_INC[ms_value(s, P_MODR)] * 44u);
    pn.on = 0;
    for (int i = 0; i < MW; i++) {
        int32_t tri = (int32_t)((ph + (uint32_t)i * (0xFFFFFFFFu / MW) * (type == 2 ? 2u : 1u)) >> 16);
        tri = tri < 32768 ? tri : 65535 - tri;
        ms_pen(&pn, MX + i, my + amp - (int)((tri * amp * 2) >> 15));
    }
    ms_dots_h(MX, MS_ST_B, MW);
    ms_hline(MX, MS_ST_B, mix * MW / 127, 1);
}

/* ARP: the notes it plays in its order, a step each rising (its range's octaves up the staircase), the one sounding
 * lit; HOLD in a tag; nothing held: what to do */
static void ms_arp(const m2k_scr_t *s)
{
    const int t = s->track, n = sq.nheld[t], rng = 1 + ((ms_value(s, P_ARNG) * 4) >> 7), tot = n * rng;
    const int on_mode = ms_value(s, P_AMOD) * 6 >> 7;
    if (sq.arp_hold[t]) ms_tag(MS_W - ms_tw("HOLD") - 4, MS_ST_T + 6, "HOLD");
    if (!on_mode) { ms_text_c(64, (MS_ST_T + MS_ST_B) / 2 + 3, "ARP OFF", 1); return; }
    if (!n) { ms_text_c(64, (MS_ST_T + MS_ST_B) / 2 + 3, sq.arp_hold[t] ? "PLAY TO HOLD" : "PLAY THE PADS", 1); return; }
    const int yb = MS_ST_B - 1, cnt = tot < 32 ? tot : 32, pitch = CLAMP(120 / cnt, 3, 16), bw = pitch - 2;
    const int x0 = (MS_W - cnt * pitch + 2) / 2, on = sq.arp_i[t] ? (int)((sq.arp_i[t] - 1) % (uint32_t)tot) : -1;
    for (int i = 0; i < cnt; i++) {
        int x = x0 + i * pitch, y = yb - 3 - i * 16 / (cnt > 1 ? cnt - 1 : 1);
        if (i == on) ms_rect(x, y, bw, 3, 1); else ms_rframe(x, y, bw, 3, 1);
        if (i == on) ms_dots_v(x + bw / 2, y + 4, yb - y - 4);
    }
    ms_dots_h(0, yb + 1, MS_W);
}

/* LFO: its wave over a cycle as deep as DPTH swings it, from a dotted zero line, live (a dotted cursor where its phase
 * is, a marker on what it sends); the three a tag each at the left, this one lit, where it goes beside them, its cycle
 * at the right */
static void ms_lfo(const m2k_scr_t *s)
{
    const int l = s->page - MS_LFO1, t = s->track, base = P_LFO + 8 * l;
    const int dest = ms_value(s, base + L_DEST), wave = (ms_value(s, base + L_WAVE) * 7) >> 7;
    const int32_t dp = T_LDPTH[ms_value(s, base + L_DPTH)];
    const trk_t *tk = &trk[t];
    int x = 0;
    for (int i = 0; i < 3; i++) {
        char n[2] = {(char)('1' + i), 0};
        if (i == l) x += ms_tag(x, MS_ST_T + 5, n) + 2; else x += ms_text(x + 1, MS_ST_T + 5, n, 1) + 4;
    }
    char o[20];
    const char *u;
    fmt(&PI_LFO[8 * l + L_DEST], t, dest, o, &u);
    ms_text(x + 4, MS_ST_T + 5, dest ? ms_label(pinfo(t, dest - 1)) : "OFF", 1);
    s_lfo_cycle(o, &u, ms_value(s, base + L_SPD), choice_of(K_MULT, ms_value(s, base + L_MULT)));
    if (*u && (s_is(u, "BAR") || s_is(u, "BARS") || s_is(u, "HZ"))) {
        char *e = o; while (*e) e++;
        *e++ = ' '; s_cpyn(e, u, (int)(o + sizeof o - e));
    }
    ms_text_r(MS_W - 1, MS_ST_T + 5, o, 1);
    const int X = 1, W = 126, mid = MS_ST_T + 15, A = 8;
    ms_dots_h(X, mid, W);
    if (!dest) return;
    ms_pen_t pn = {0, 0, 0};
    int32_t rr = 0;
    for (int i = 0; i < W; i++) {
        int32_t v = wave == 6 ? (int32_t)(hash32((uint32_t)(i / 9) + 77u) >> 16) - 32768
                              : lfo_shape(wave, (uint32_t)i * (0xFFFFFFFFu / (uint32_t)(W - 1)), &rr);
        int y = mid - (int)((((v * dp) >> 14) * A) >> 15);
        if (pn.on && wave == 6 && y != pn.y) ms_line(pn.x, pn.y, X + i, pn.y, 1);
        ms_pen(&pn, X + i, y);
        ms_fill(X + i, X, y < mid ? y + 1 : mid + 1, y < mid ? mid : y, s->touch == 7 ? 2 : 1);
    }
    int cx = X + (int)(((tk->lph[l] >> 16) * (uint32_t)(W - 1)) >> 16);
    int cy = mid - (int)((((tk->lval[l] * dp) >> 14) * A) >> 15);
    ms_dots_v(cx, mid - A - 1, 2 * A + 3);
    ms_marker(cx, cy);
}

/* GLO: the tempo large (SYNC's link over BPM when it is the Move's, followed), the bar's four beats under it, lit as
 * they pass; under the bus's knobs the reverb's tail over time (0.1 .. 30 s on a log scale, 60 dB down the stage), DAMP's
 * top dotted, DRIV lifting it, ADPC stepping it */
static void ms_glo(const m2k_scr_t *s)
{
    char o[12];
    s_int(o, (int)seq_bpm());
    const int b = MS_ST_T + 15;
    int x = 2 + ms_text_z(2, b, o, 3, 1) + 4;
    ms_text(x, b, "BPM", 1);
    if (s->sync) ms_icon(I_SYNC, x, b - 9, 1);         /* (the Move's: followed) */
    const int beat = sq.playing ? (sq.step ? sq.step - 1 : 0) / 4 % 4 : -1;
    for (int i = 0; i < 4; i++) { int bx = 2 + i * 14; if (i == beat) ms_rect(bx, MS_ST_B - 5, 12, 5, 1); else ms_rframe(bx, MS_ST_B - 5, 12, 5, 1); }
    const m2k_pattern_t *p = PAT(cur_pat);
    const int v = p->glob[G_VERB], ad = p->glob[G_ADPC], X = 66, W = 61, top = MS_ST_T + 1, ty = MS_ST_B - 1, dh = ty - top;
    if (!v) { ms_text_c(X + W / 2, (top + ty) / 2 + 3, "OFF", 1); return; }
    const uint32_t t60 = T_RV_MS[v], th = (t60 * 256u) / (256u + T_RV_DA[p->glob[G_DAMP]]);
    const int dg = (ilog2_q8(T_DRIV[p->glob[G_DRIV]]) - 8 * 256) * dh / 2560;
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

/* GRID: Grids' map, 25 nodes a dot each, the four round X and Y larger, the place a marker on its dotted cross; the
 * three lanes' cycles at the right as they play, a bar a step as tall as its level, lit where it trigs, the density a
 * dotted line, the step playing a mark under it */
static void ms_grid_lane(int x, int b, int w, int h, int part)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int cyc = grids_cyc(p), kn = sq.playing ? (int)sq.n1x - 1 : -1, base = kn < 0 ? 0 : kn - kn % cyc, pw = w / cyc;
    const int th = 255 - gr8(p->glob[G_GRBD + part]);
    for (int i = 0; i < cyc; i++) {
        int lv, hit = grids_hit(p, part, (uint32_t)(base + i), &lv), bh = (lv * h + 254) / 255, bx = x + i * pw;
        if (hit) ms_rect(bx, b - bh, pw > 2 ? pw - 1 : 1, bh, 1);
        else if (bh) ms_vline(bx, b - bh, bh, 1);
        if (base + i == kn) ms_hline(bx, b + 1, pw > 2 ? pw - 1 : 1, 1);
    }
    ms_dots_h(x, b - (th * h + 127) / 255, cyc * pw);
}

static void ms_grid(const m2k_scr_t *s)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int X = 2, Y = MS_ST_T + 1, S = 5;
    const int x8 = gr8(p->glob[G_GRX]), y8 = gr8(p->glob[G_GRY]), i0 = x8 >> 6, j0 = y8 >> 6;
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
    for (int l = 0; l < 3; l++) {
        int b = MS_ST_T + 6 + l * 9;
        if (lane_of(p, s->track) == l + 1) ms_tag(32, b, N[l]); else ms_text(34, b, N[l], 1);
        ms_grid_lane(48, b - 1, 80, 6, l);
    }
    (void)s;
}

/* SET: what comes in and goes out, a tag lit while it is on */
static void ms_set(const m2k_scr_t *s)
{
    static const char *const F[6] = {"CLOCK IN", "CLOCK OUT", "NOTES OUT", "NOTES IN", "THRU", "MUTES AT BAR"};
    const int on[6] = {clk_from() != 0, store_.set[S_SYNO] >= 64, store_.set[S_NOTO] >= 64, notes_from() != 0,
                       set_on(S_THRU), !set_on(S_MUTQ)};
    for (int i = 0; i < 6; i++) {
        int x = (i % 2) * 64 + 2, b = MS_ST_T + 6 + (i / 2) * 9;
        if (on[i]) ms_tag(x - 2, b, F[i]); else ms_text(x, b, F[i], 1);
    }
    (void)s;
}

/* SYN: the machine (the header names it): its class and style, its voice's model three times the type; punch, metal
 * and plastic five cells each at the right (the FM-1's machine card, Hortator's) */
static void ms_syn(const m2k_scr_t *s)
{
    const int m = PAT(cur_pat)->machine[s->track];
    const mi_t *mi = &MI[m];
    int x = 2 + ms_text(2, MS_ST_T + 5, mi->cls, 1) + 4;
    if (!s_is(mi->style, mi->cls)) ms_text(x, MS_ST_T + 5, mi->style, 1);
    const char *md = ms_model(m);
    ms_text_z(2, MS_ST_B, md, ms_tw_z(md, 3) <= 74 ? 3 : 2, 1);
    static const char *const ST[3] = {"PNCH", "METL", "PLST"};
    for (int r = 0; r < 3; r++) {
        const int rb = MS_ST_T + 10 + r * 7, sx = MS_W - 5 * 5 + 1;
        ms_text_r(sx - 4, rb, ST[r], 1);
        for (int g = 0; g < 5; g++) { if (g < mi->stat[r]) ms_rect(sx + g * 5, rb - 4, 4, 4, 1); else ms_frame(sx + g * 5, rb - 4, 4, 4, 1); }
    }
}

/* ---------------------------------------------------------------------------------------- a knob touched */

/* the stage gives way to the knob: its full name (A..H before a machine's), LOCK when a held step's lock is what it
 * shows; its value three times the type, its unit beside it; where it sits along the foot */
static void ms_touched(const m2k_scr_t *s, int y0, int y1)
{
    const int code = m2k_screen_param(s->page, s->track, s->touch);
    if (code == NONE) return;
    const pi_t *pi = pinfo(s->track, code);
    if (pi->s4[0] == '-') return;
    ms_rect(0, y0, MS_W, y1 - y0 + 1, 0);
    const int v = ms_value(s, code), b0 = y0 + 6;
    char o[32], val[16], *e = o;
    const char *u;
    if (code < 8) { *e++ = (char)('A' + code); *e++ = ' '; }
    s_cpy(e, pi->name);
    int xr = MS_W - 1;
    if (ms_locked(s, code)) xr -= ms_tag(xr - ms_tw("LOCK") - 3, b0, "LOCK") + 4;
    if (s->held < M2K_STEPS) { char st[12]; s_int(s_cpy(st, "STEP "), s->held + 1); xr -= ms_text_r(xr, b0, st, 1) + 6; }
    if (ms_tw(o) > xr - 2) s_cpy(o, pi->s4);
    ms_text(2, b0, o, 1);
    fmt(pi, s->track, v, val, &u);
    const int vb = y1 - 4;
    int z = 3;
    if (ms_tw_z(val, 3) + (*u ? ms_tw(u) + 4 : 0) > MS_W - 4) z = 2;
    int w = ms_text_z(2, vb, val, z, 1);
    if (*u) ms_text(2 + w + 4, vb, u, 1);
    const int q = ms_q(pi, v), bip = bipolar(pi->kind), fx = 2 + (q * 123 + 128) / 256;
    ms_dots_h(2, y1 - 1, 124);
    if (bip) { if (fx > 64) ms_hline(64, y1 - 1, fx - 64, 1); else ms_hline(fx, y1 - 1, 64 - fx, 1); ms_vline(64, y1 - 2, 3, 1); }
    else ms_hline(2, y1 - 1, fx - 2, 1);
    ms_rect(CLAMP(fx - 1, 2, 124), y1 - 3, 2, 4, 1);
}

/* ------------------------------------------------------------------------------------------------- HOME */

/* the eight tracks as lanes: each its number (the track in view lit in a tag), its machine's icon, the 16 steps of
 * the page the step keys edit grouped by the beat, its level live. A trig 4 x 5, an accent's a notch, on a condition
 * framed, with locks a checker, under its track's PROB filled to its chance; a rest a dash on the floor; muted, the
 * trigs framed and the rests gone; the playhead its step's cell inverted */
static int32_t ms_meter[M2K_TRACKS];
static uint32_t ms_last;

static void ms_lane_step(const m2k_scr_t *s, int x, int y, int t, int st)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    if (st >= tlen_of(t)) return;
    const int muted = (p->mute >> t) & 1, pr = PROB_PCT[p->glob[G_TPRB1 + t] & 15];
    int hit = 0, acc = 0, cond = 0, lk = 0;
    if (lane_of(p, t)) {
        uint32_t len = (uint32_t)seq_len(), n = !sq.playing ? 0 : spd_of(t) == 4 ? sq.n1x : sq.tk[t];
        hit = grids_hit(p, lane_of(p, t) - 1, (n ? (n - 1) / len * len : 0) + (uint32_t)st, 0);
        acc = hit == 2;
    } else {
        const m2k_step_t *sp = &p->step[t][st];
        hit = sp->flags & ST_TRIG;
        acc = sp->flags & ST_ACC;
        cond = sp->cond != C_ALWAYS;
        lk = has_locks(t, st);
    }
    if (!hit) { if (!muted) ms_hline(x + 1, y + 4, 2, 1); }
    else if (muted || cond) ms_frame(x, y, 4, 5, 1);
    else if (lk) { ms_frame(x, y, 4, 5, 1); ms_px(x + 1, y + 2, 1); ms_px(x + 2, y + 1, 1); ms_px(x + 2, y + 3, 1); }
    else if (pr < 100) { int f = (5 * pr + 50) / 100; ms_frame(x, y, 4, 5, 1); if (f) ms_rect(x, y + 5 - f, 4, f, 1); }
    else ms_rect(x, y, 4, 5, 1);
    if (hit && acc && !muted) ms_px(x + 1, y, 0), ms_px(x + 2, y, 0);
    if (sq.playing && track_pos(t) == st) ms_rect(x - 1, y - 1, 6, 7, 2);   /* (the playhead: its cell inverted) */
    (void)s;
}

static void ms_home(const m2k_scr_t *s)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    for (int t = 0; t < M2K_TRACKS && t < 8; t++) {
        const int y = 8 + t * 6, b = y + 5;
        char n[2] = {(char)('1' + t), 0};
        if (t == s->track) { ms_rfill(0, y - 1, 7, 7, 1); ms_text_c(3, b, n, 0); }
        else if (s->shift && t >= 4) { ms_rframe(0, y - 1, 7, 7, 1); ms_text_c(3, b, n, 1); }
        else ms_text_c(3, b, n, 1);
        ms_icon(ms_machine_icon(p->machine[t]), 10, b, 1);
        for (int i = 0; i < 16; i++) ms_lane_step(s, 20 + i * 6 + (i >> 2) * 2, y, t, s->spage * 16 + i);
        const int w = ms_meter[t] * 8 >> 15;            /* its level, live; its floor dotted */
        if (w) ms_rect(119, y + 1, w, 3, 1);
        if (w < 8) ms_dots_h(119 + w, y + 4, 8 - w);
        if (sq.arp_hold[t] && sq.nheld[t]) ms_px(17, y, 1), ms_px(17, y + 2, 1), ms_px(17, y + 4, 1);
    }
}

/* ------------------------------------------------------------------------------------------- the catalogue */

/* the library's rows (Hortator's, the FM-1's EDIT held): the slots of the category shown, the one landed on lit in a
 * bar (it plays as it is landed on: a speaker in it), its number, name, and machine's icon at the right; the scroll a
 * dotted track at the edge; at the foot the sound's machine, note and category */
static void ms_cat(const m2k_scr_t *s)
{
    const int sel = s->cat_sel % M2K_SOUNDS, cat = s->cat_cat < CAT_COUNT ? s->cat_cat : 0, rows = 5;
    int list[M2K_SOUNDS], n = 0, at = 0;
    for (int i = 0; i < M2K_SOUNDS; i++) {
        if (cat && snd[i].name[0] && snd_cat[i] != cat) continue;
        if (i == sel) at = n;
        list[n++] = i;
    }
    const int first = CLAMP(at - rows / 2, 0, n > rows ? n - rows : 0);
    for (int r = 0; r < rows && first + r < n; r++) {
        const int i = list[first + r], b = 16 + r * 8, on = i == sel;
        char o[4];
        s_num3(o, i + 1);
        if (on) ms_rfill(0, b - 6, 123, 7, 1);
        ms_text(2, b, o, !on);
        if (!snd[i].name[0]) { ms_text(20, b, "INIT", !on); continue; }
        ms_text(20, b, snd[i].name, !on);
        if (on) ms_icon(I_SPKR, 88, b, 0);
        ms_icon(ms_machine_icon(snd[i].machine < M_COUNT ? snd[i].machine : 0), 113, b, !on);
    }
    const int top = 9, h = rows * 8 - 1, sh = n > rows ? CLAMP(rows * h / n, 3, h) : h, sy = n > rows ? first * (h - sh) / (n - rows) : 0;
    ms_dots_v(126, top, h);
    ms_rect(125, top + sy, 3, sh, 1);
    ms_dots_h(0, 50, MS_W);
    const m2k_sound_t *sd = &snd[sel];
    if (!sd->name[0]) { ms_text(2, 60, "AN EMPTY SLOT", 1); return; }
    const mi_t *mi = &MI[sd->machine < M_COUNT ? sd->machine : 0];
    char o[20], *e = s_cpy(o, mi->name);
    *e++ = ' '; *e++ = ' ';
    s_note(e, snd_root[sel]);
    ms_text(2, 60, o, 1);
    if (snd_cat[sel]) ms_text_r(MS_W - 1, 60, CAT_N[snd_cat[sel]], 1);
}

/* --------------------------------------------------------------------------------------------- PERFORM */

/* the pads as the Move has them, four rows of eight from the top: bank B's eleven over bank A's, the effects playing
 * lit, the rest framed, the pads PERFORM leaves free a dot; what plays named at the left, large while one alone */
static void ms_perf(const m2k_scr_t *s)
{
    const int X = 64, Y = MS_ST_T - 2;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 8; c++) {
            const int pad = (3 - r) * 8 + c, k = pad < 11 ? pad : pad >= 16 && pad < 27 ? 11 + pad - 16 : -1;
            const int x = X + c * 8, y = Y + r * 7;
            if (k < 0) ms_px(x + 3, y + 2, 1);
            else if (perf.on[k]) ms_rect(x, y, 6, 5, 1);
            else ms_frame(x, y, 6, 5, 1);
        }
    int n = 0, last = -1;
    for (int k = 0; k < PF_COUNT; k++) if (perf.on[k]) { n++; last = k; }
    if (!n) { ms_text(2, MS_ST_T + 5, "HOLD A PAD", 1); return; }
    if (n == 1) { ms_text_z(2, MS_ST_T + 12, PF_NAME[last], ms_tw_z(PF_NAME[last], 2) <= 58 ? 2 : 1, 1); return; }
    for (int k = 0, r = 0; k < PF_COUNT && r < 3; k++) if (perf.on[k]) ms_text(2, MS_ST_T + 5 + 8 * r++, PF_NAME[k], 1);
    (void)s;
}

/* ------------------------------------------------------------------------------------------- the page list */

/* the pages of knobs and the views, a row each (the MNM-1's menu): its icon and name, the one under the cursor lit in a
 * bar; what each holds at the right in the light of the moment (the pattern, the machine, the LFO's way) */
static void ms_list(const m2k_scr_t *s)
{
    const int rows = 6, n = MS_LIST, at = s->list < n ? s->list : 0, first = CLAMP(at - rows / 2, 0, n - rows);
    const m2k_pattern_t *p = PAT(cur_pat);
    for (int r = 0; r < rows; r++) {
        const int pg = first + r, b = 16 + r * 8, on = pg == at;
        if (on) ms_rfill(0, b - 6, 123, 7, 1);
        ms_icon(MS_PAGE_I[pg], 2, b, !on);
        ms_text(12, b, MS_PAGE_N[pg], !on);
        char o[16];
        const char *h = "";
        if (pg == MS_SYN) h = MI[p->machine[s->track]].name;
        else if (pg >= MS_LFO1 && pg <= MS_LFO3) {
            int d = p->kit[s->track][P_LFO + 8 * (pg - MS_LFO1) + L_DEST];
            h = d ? ms_label(pinfo(s->track, d - 1)) : "OFF";
            if (d) { s_cpy(s_cpy(o, "TO "), h); h = o; }
        } else if (pg == MS_HOME) h = pat_name(cur_pat);
        else if (pg == MS_CAT) h = "THE LIBRARY";
        if (*h) ms_text_r(120, b, h, !on);
    }
    const int top = 9, hh = rows * 8 - 1, sh = rows * hh / n, sy = first * (hh - sh) / (n - rows);
    ms_dots_v(126, top, hh);
    ms_rect(125, top + sy, 3, sh, 1);
}

/* --------------------------------------------------------------------------------------------- the frame */

const uint8_t *m2k_screen(const m2k_scr_t *s)
{
    m_set(ms_fb, 0, sizeof ms_fb);
    /* the meters: the track's peak since the last frame, falling an eighth a 33 ms (the FM-1's) */
    const uint32_t dt = ms_last ? s->ms - ms_last : 33u;
    ms_last = s->ms ? s->ms : 1u;
    for (int t = 0; t < M2K_TRACKS; t++) {
        int32_t pk = mon.peak[t] * 2;
        mon.peak[t] = 0;
        ms_meter[t] = pk > ms_meter[t] ? CLAMP(pk, 0, 32767) : ms_meter[t] - ms_meter[t] * (int32_t)(dt > 132 ? 132 : dt) / 264;
    }
    mon.lfo_view = s->track < M2K_TRACKS ? s->track : 0;
    const int pg = s->page < MS_COUNT ? s->page : MS_HOME;
    switch (pg) {
    case MS_HOME: {
        ms_header(s, "HOME", 0);
        /* (the pattern's name after the tag; the bars it has, the one the step keys edit lit) */
        int x = ms_tw("HOME") + 8;
        x += ms_text(x, MS_HB, pat_name(cur_pat), 1) + 6;
        for (int b = 0; b < 4; b++) {
            if (b * 16 >= seq_len()) { ms_px(x + b * 5 + 1, MS_HB - 2, 1); continue; }
            if (b == s->spage) ms_rect(x + b * 5, MS_HB - 4, 3, 3, 1); else ms_frame(x + b * 5, MS_HB - 4, 3, 3, 1);
        }
        ms_home(s);
        ms_knobs(s, 0);
        if (s->touch >= 0) ms_touched(s, 8, 54);
        break;
    }
    case MS_CAT: {
        char o[12];
        int x = ms_tag(0, MS_HB, "SOUNDS") + 4;
        x += ms_text(x, MS_HB, ms_tname(o, s->track), 1) + 4;
        const int cat = s->cat_cat < CAT_COUNT ? s->cat_cat : 0;
        ms_rframe(MS_W - ms_tw(CAT_N[cat]) - 4, 0, ms_tw(CAT_N[cat]) + 4, 7, 1);
        ms_text_r(MS_W - 3, MS_HB, CAT_N[cat], 1);
        ms_cat(s);
        break;
    }
    case MS_LIST:
        ms_header(s, "PAGES", 1);
        ms_list(s);
        break;
    case MS_PERF:
        ms_header(s, "PERFORM", 0);
        ms_perf(s);
        ms_knobs(s, 1);
        if (s->touch >= 0) ms_touched(s, MS_ST_T - 3, MS_ST_B + 1);
        break;
    default:
        ms_header(s, MS_PAGE_N[pg], 1);
        ms_strip(s);
        switch (pg) {
        case MS_SYN: ms_syn(s); break;
        case MS_AMP: ms_amp(s); break;
        case MS_FILT: ms_filt(s); break;
        case MS_EFX: ms_efx(s); break;
        case MS_DSTR: ms_dstr(s); break;
        case MS_ARP: ms_arp(s); break;
        case MS_LFO1: case MS_LFO2: case MS_LFO3: ms_lfo(s); break;
        case MS_GLO: ms_glo(s); break;
        case MS_GRID: ms_grid(s); break;
        case MS_SET: ms_set(s); break;
        default: break;
        }
        ms_knobs(s, 1);
        if (s->touch >= 0) ms_touched(s, MS_ST_T - 1, MS_ST_B + 1);
        break;
    }
    return ms_fb;
}
