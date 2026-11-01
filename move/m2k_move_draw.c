/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the plain screen. The page state (m2k_move_view.h) drawn into a 1-bit 128 x 64 frame, 16 bytes a
 * row, MSB first, 1 = a lit pixel, in the owner's pixel font at its native size (firmware/gen/m2k_fonts.h's F_S glyphs,
 * one pixel each: capitals 5 rows, a 7-row cell). Functional only: the final design replaces this file, and reads the
 * same page state. It runs on the drawing thread (m2k_move.c), from a copy of the state, touching nothing else.
 *
 *   0..8    a lit bar: the track and its machine; the pattern (and the one queued)
 *   10..16  the page and the bars (a held step's note); the tempo, SYNC or INT, REC and the transport
 *   18..51  the eight knobs, two rows of four (label over value; a lock's value inverted), or what is over them: a
 *           knob's value large, the page list, the catalogue, Shift's and Mute's layers, a message
 *   54..59  the 16 steps (a trig filled, a locked one hollowed, the playhead underlined)
 *   61..63  the bars the track reaches, the one shown filled */

#define MV_W 128
#define MV_H 64
#define MV_FB (MV_W * MV_H / 8)

static void d_px(uint8_t *fb, int x, int y, int on)
{
    if ((unsigned)x >= MV_W || (unsigned)y >= MV_H) return;
    uint8_t *b = &fb[y * 16 + (x >> 3)], m = (uint8_t)(0x80u >> (x & 7));
    if (on) *b |= m; else *b &= (uint8_t)~m;
}

static void d_fill(uint8_t *fb, int x, int y, int w, int h, int on)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) d_px(fb, i, j, on);
}

static void d_frame(uint8_t *fb, int x, int y, int w, int h, int on)
{
    d_fill(fb, x, y, w, 1, on);
    d_fill(fb, x, y + h - 1, w, 1, on);
    d_fill(fb, x, y, 1, h, on);
    d_fill(fb, x + w - 1, y, 1, h, on);
}

/* a glyph of F_S at its native size: its index, its width (without the space after it) */
static int d_glyph(int c, int *w)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < F_S.first || c > F_S.last) c = '?';
    const int i = c - F_S.first;
    *w = F_S.w[i] / F_S.z;
    return i;
}

static int d_textw(const char *s, int z)
{
    int n = 0, w;
    for (; *s; s++) { d_glyph((unsigned char)*s, &w); n += (w + 1) * z; }
    return n ? n - z : 0;
}

/* text at (x, y), y the cell's top (capitals on rows y+1..y+5 at z 1), clipped at xr; z 1 or 2; returns its end */
static int d_text(uint8_t *fb, int x, int y, const char *s, int on, int z, int xr)
{
    const int rows = F_S.h / F_S.z;
    for (; *s; s++) {
        int w;
        const int i = d_glyph((unsigned char)*s, &w);
        if (x + w * z > xr) break;
        const uint8_t *g = F_S.bits + F_S.off[i];
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < w; c++)
                if ((g[r] >> (7 - c)) & 1) d_fill(fb, x + c * z, y + r * z, z, z, on);
        x += (w + 1) * z;
    }
    return x;
}

static int d_text_r(uint8_t *fb, int xr, int y, const char *s, int on)
{
    int w = d_textw(s, 1);
    d_text(fb, xr - w, y, s, on, 1, MV_W);
    return xr - w;
}

static void d_text_c(uint8_t *fb, int cx, int y, const char *s, int on, int z)
{
    d_text(fb, cx - d_textw(s, z) / 2, y, s, on, z, MV_W);
}

static char *d_cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *d_int(char *d, int v) { return s_int(d, v); }

static void d_header(uint8_t *fb, const mv_view_t *v)
{
    char o[32], *e;
    d_fill(fb, 0, 0, MV_W, 9, 1);
    e = d_cat(o, "T");
    e = d_int(e, v->trk + 1);
    e = d_cat(e, " ");
    d_cat(e, v->machine);
    d_text(fb, 2, 1, o, 0, 1, 80);
    e = d_cat(o, v->slot);
    if (v->next[0]) { e = d_cat(e, ">"); d_cat(e, v->next); }
    d_text_r(fb, 126, 1, o, 0);
    /* the page, the bars; the tempo, its source, the transport */
    if (v->held && v->held_note[0]) { e = d_cat(o, "NOTE "); d_cat(e, v->held_note); }   /* a held step's note */
    else {
        e = d_cat(o, v->page_name);
        if (v->npage > 1) { e = d_cat(e, " "); e = d_int(e, v->spage + 1); e = d_cat(e, "/"); d_int(e, v->npage); }
    }
    d_text(fb, 2, 10, o, 1, 1, 60);
    e = d_int(o, v->bpm10 / 10);
    if (v->bpm10 % 10) { *e++ = '.'; *e++ = (char)('0' + v->bpm10 % 10); *e = 0; }
    e = d_cat(e, v->sync == MV_SYNC_MOVE ? " SYNC" : " INT");
    int x = d_text_r(fb, 112, 10, o, 1);
    (void)x;
    if (v->rec) d_frame(fb, 116, 11, 5, 5, 1);                       /* REC: a ring, PLAY: a triangle */
    if (v->playing) for (int i = 0; i < 3; i++) d_fill(fb, 122 + i, 11 + i, 1, 5 - 2 * i, 1);
    else d_fill(fb, 122, 12, 3, 3, 1);
}

static void d_knobs(uint8_t *fb, const mv_view_t *v)
{
    for (int k = 0; k < 8; k++) {
        const mv_knob_t *kn = &v->knob[k];
        const int x = (k & 3) * 32 + 1, y = 19 + (k >> 2) * 17;
        d_text(fb, x, y, kn->label, 1, 1, x + 30);
        if (kn->flags & MV_K_FREE) continue;
        if (kn->flags & MV_K_LOCK) {
            d_fill(fb, x - 1, y + 8, 31, 8, 1);
            d_text(fb, x, y + 8, kn->value, 0, 1, x + 30);
        } else d_text(fb, x, y + 8, kn->value, 1, 1, x + 30);
    }
}

static void d_focus(uint8_t *fb, const mv_view_t *v)
{
    const mv_knob_t *kn = &v->knob[v->focus];
    d_text(fb, 2, 19, kn->name, 1, 1, MV_W);
    if (kn->flags & MV_K_LOCK) d_text_r(fb, 126, 19, "LOCK", 1);
    d_text(fb, 2, 29, kn->value, 1, 2, MV_W);
    d_text(fb, 2, 45, kn->unit, 1, 1, MV_W);
    if (!(kn->flags & (MV_K_CHOICE | MV_K_MOVE))) {                     /* where in its range */
        const int w = 60, x0 = 64, mx = kn->raw > 127 ? 255 : 127, f = kn->raw * w / mx;
        d_frame(fb, x0, 46, w + 2, 5, 1);
        if (kn->flags & MV_K_BIP) {
            const int mid = w / 2;
            if (f >= mid) d_fill(fb, x0 + 1 + mid, 47, f - mid + 1, 3, 1); else d_fill(fb, x0 + 1 + f, 47, mid - f, 3, 1);
        } else d_fill(fb, x0 + 1, 47, f, 3, 1);
    }
}

static void d_pages(uint8_t *fb, const mv_view_t *v)
{
    static const char *const N[MV_PG_COUNT] = {"SYN", "AMP", "FILT", "EFX", "DSTR", "TRK", "LFO1", "LFO2", "LFO3",
                                               "SEQ", "GRID", "PAT"};
    for (int i = 0; i < MV_PG_COUNT; i++) {
        const int x = (i & 3) * 32, y = 19 + (i >> 2) * 11;
        if (i == v->list_sel) { d_fill(fb, x, y - 1, 31, 9, 1); d_text(fb, x + 2, y, N[i], 0, 1, x + 31); }
        else d_text(fb, x + 2, y, N[i], 1, 1, x + 31);
    }
}

static void d_cat_page(uint8_t *fb, const mv_view_t *v)
{
    char o[24], *e;
    e = d_cat(o, "SOUNDS ");
    d_cat(e, v->cat_cat);
    d_text(fb, 2, 19, o, 1, 1, MV_W);
    e = d_cat(o, v->cat_slot);
    e = d_cat(e, " ");
    d_cat(e, v->cat_name);
    d_text(fb, 2, 29, o, 1, 2, MV_W);
    d_text(fb, 2, 45, "CLICK KEEPS, MENU BACK", 1, 1, MV_W);
}

static void d_shift(uint8_t *fb, const mv_view_t *v)
{
    char o[24], *e;
    e = d_cat(o, "BANK ");
    *e++ = (char)('A' + v->bank); *e = 0;
    d_text(fb, 2, 19, o, 1, 1, MV_W);
    d_text(fb, 50, 19, "STEPS: PATTERN", 1, 1, MV_W);
    d_text(fb, 2, 28, "PADS: PERFORM  JOG: MACHINE", 1, 1, MV_W);
    /* the effects held */
    int x = 2;
    for (int k = 0; k < PF_COUNT; k++)
        if (v->perf >> k & 1) x = d_text(fb, x, 38, PF_NAME[k], 1, 1, MV_W) + 4;
    if (x == 2) d_text(fb, 2, 38, "TRACKS 5-8 ON THE BUTTONS", 1, 1, MV_W);
}

static void d_mute(uint8_t *fb, const mv_view_t *v)
{
    d_text(fb, 2, 19, "MUTE: STEPS OR TRACKS", 1, 1, MV_W);
    for (int t = 0; t < v->ntrk; t++) {
        const int x = 2 + t * 15;
        char o[4] = {'T', (char)('1' + t), 0};
        if (v->mutes >> t & 1) d_text(fb, x, 31, o, 1, 1, MV_W);
        else { d_fill(fb, x - 1, 30, 12, 9, 1); d_text(fb, x, 31, o, 0, 1, MV_W); }
    }
}

static void d_toast(uint8_t *fb, const mv_view_t *v)
{
    d_fill(fb, 4, 22, 120, 24, 0);
    d_frame(fb, 4, 22, 120, 24, 1);
    d_text_c(fb, 64, 26, v->toast, 1, 1);
    d_text_c(fb, 64, 36, v->toast2, 1, 1);
}

static void d_steps(uint8_t *fb, const mv_view_t *v)
{
    for (int k = 0; k < 16; k++) {
        const uint8_t f = v->step[k];
        const int x = k * 8 + 1;
        if (f & MV_ST_PAST) continue;
        if (f & MV_ST_TRIG) {
            d_fill(fb, x, 54, 6, 6, 1);
            if (f & MV_ST_LOCK) d_fill(fb, x + 2, 56, 2, 2, 0);
        } else if (f & MV_ST_BEAT) d_frame(fb, x, 54, 6, 6, 1);
        else d_fill(fb, x + 2, 56, 2, 2, 1);
        if (f & MV_ST_HELD) d_frame(fb, x - 1, 53, 8, 8, 1);
        if (f & MV_ST_PLAY) d_fill(fb, x, 61, 6, 2, 1);
    }
    if (v->npage > 1)                                                  /* the bars, the one shown filled */
        for (int b = 0; b < v->npage; b++) d_fill(fb, 128 - 4 * (v->npage - b), 63, 3, 1, 1);
}

static void mv_draw(const mv_view_t *v, uint8_t *fb)
{
    m_set(fb, 0, MV_FB);
    d_header(fb, v);
    if (v->overlay == MV_OV_CAT) d_cat_page(fb, v);
    else if (v->overlay == MV_OV_PAGES) d_pages(fb, v);
    else if (v->overlay == MV_OV_SHIFT) d_shift(fb, v);
    else if (v->overlay == MV_OV_MUTE) d_mute(fb, v);
    else if (v->focus >= 0 && v->focus < 8 && !(v->knob[v->focus].flags & MV_K_FREE)) d_focus(fb, v);
    else d_knobs(fb, v);
    if (v->toast[0] && v->overlay != MV_OV_CAT) d_toast(fb, v);
    d_steps(fb, v);
}
