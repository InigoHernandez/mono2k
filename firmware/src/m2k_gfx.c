/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's 1-bit drawing: a 240 x 240 framebuffer (7.2 KB, 1 = ink), dither patterns, text in the owner's pixel font
 * (drawn 2 or 3 times its size) in solid, dithered or outlined-and-dithered fills, the rounded boxes and the 2 px
 * strokes of the knobs' pictures, the machine marks, and the transitions (dissolve, glitch). */

static uint8_t fb[M2K_FB_BYTES];       /* the frame being drawn */
static uint8_t fb_prev[M2K_FB_BYTES];  /* the last frame shown before a transition */
static uint8_t fb_out[M2K_FB_BYTES];   /* what the screen gets */
static int16_t clip_x0 = 0, clip_y0 = 0, clip_x1 = M2K_W, clip_y1 = M2K_H;

#define ROWB (M2K_W / 8)

/* patterns: 0..64 are Bayer levels (64 solid); above are the textures of the references */
enum { PT_SOLID = 64, PT_LINES = 65, PT_VLINES, PT_CHECK, PT_DOTS, PT_SPARSE, PT_CROSS, PT_DIAG, PT_IDOTS, PT_HATCH };

static inline int pat_ink(int p, int x, int y)
{
    if (p <= 64) return T_BAYER[y & 7][x & 7] < p;
    switch (p) {
    case PT_LINES: return !(y & 1);
    case PT_VLINES: return !(x & 1);
    case PT_CHECK: return !((x ^ y) & 1);
    case PT_DOTS: return !(x & 1) && !(y & 1);
    case PT_SPARSE: return !(x & 3) && !(y & 3);
    case PT_CROSS: { int a = x & 3, b = y & 3; return (a == b || a == 3 - b) && a != 3 && b != 3; }
    case PT_DIAG: return ((x + y) & 3) == 0;
    case PT_IDOTS: return (x & 1) || (y & 1);
    case PT_HATCH: return !(x & 3);              /* the screen's one light tone: a hairline every fourth column */
    default: return 1;
    }
}

#ifdef M2K_TEXT_AUDIT
static uint32_t px_calls;              /* (the host counts the pixels drawn one at a time: the drawing's cost) */
#endif

static inline void px(int x, int y, int c)
{
#ifdef M2K_TEXT_AUDIT
    px_calls++;
#endif
    if (x < clip_x0 || y < clip_y0 || x >= clip_x1 || y >= clip_y1) return;
    uint8_t *b = &fb[y * ROWB + (x >> 3)], m = (uint8_t)(0x80u >> (x & 7));
    if (c == 1) *b |= m;
    else if (c == 0) *b &= (uint8_t)~m;
    else *b ^= m;
}

static inline int px_get(const uint8_t *buf, int x, int y)
{
    if ((unsigned)x >= M2K_W || (unsigned)y >= M2K_H) return 0;
    return (buf[y * ROWB + (x >> 3)] >> (7 - (x & 7))) & 1;
}

static void clip(int x, int y, int w, int h) { clip_x0 = (int16_t)x; clip_y0 = (int16_t)y; clip_x1 = (int16_t)(x + w); clip_y1 = (int16_t)(y + h); }
static void noclip(void) { clip(0, 0, M2K_W, M2K_H); }

/* the text audit (a host build, M2K_TEXT_AUDIT): every text's ink as drawn, so a test can find two that touch, one cut
 * by the screen's edge or a clip, one past its cell or its window, one on a rule. A fill of paper (a window, a toast's
 * line) or a broad one of ink hides the texts and the rules it covers. Without the audit the cells and rules are drawn
 * and nothing is kept */
#ifdef M2K_TEXT_AUDIT
typedef struct { int16_t x0, y0, x1, y1; uint8_t cut; char s[14]; } ta_t;
static ta_t ta[512];
static int ta_n;
/* the boxes a text must keep to, the last drawn on top: a window (a paper fill: a text begun in it ends 2 px inside its
 * frame) or a cell (a knob's, the stage's two columns, the header's and the footer's slots: a text whose ink begins
 * anywhere in the cell's box lies wholly in its content, 3 px clear of the rules) */
typedef struct { int16_t x0, y0, x1, y1, cx0, cy0, cx1, cy1; uint8_t win; } ta_box_t;
static ta_box_t ta_box[32];
static int ta_nbox;
/* the rules (the bands' and the cells'): no text's ink within 2 px of one */
typedef struct { int16_t x0, y0, x1, y1; } ta_rule_t;
static ta_rule_t ta_rule[96];                  /* (a card is four: the knobs' four, the stage's, a window's) */
static int ta_nrule;
static void ta_rule_add(int x0, int y0, int x1, int y1)
{
    if (ta_nrule < 96 && x0 <= x1 && y0 <= y1) ta_rule[ta_nrule++] = (ta_rule_t){(int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1};
}
static void ta_cover(int x, int y, int w, int h)
{
    if (ta_nbox < 32 && w * h >= 2000)
        ta_box[ta_nbox++] = (ta_box_t){(int16_t)x, (int16_t)y, (int16_t)(x + w - 1), (int16_t)(y + h - 1), 0, 0, 0, 0, 1};
    int j = 0;
    for (int i = 0; i < ta_n; i++)
        if (!(ta[i].x0 < x + w && x <= ta[i].x1 && ta[i].y0 < y + h && y <= ta[i].y1)) ta[j++] = ta[i];
    ta_n = j;
    /* a rule under the fill is hidden there: what is left of it each side stays */
    for (int i = ta_nrule - 1; i >= 0; i--) {
        ta_rule_t r = ta_rule[i];
        if (!(r.x0 < x + w && x <= r.x1 && r.y0 < y + h && y <= r.y1)) continue;
        ta_rule[i] = ta_rule[--ta_nrule];
        if (r.y0 == r.y1) { ta_rule_add(r.x0, r.y0, x - 1, r.y1); ta_rule_add(x + w, r.y0, r.x1, r.y1); }
        else { ta_rule_add(r.x0, r.y0, r.x1, y - 1); ta_rule_add(r.x0, y + h, r.x1, r.y1); }
    }
}
/* a cell: its box [x0, x1) x [y0, y1), its content [cx0, cx1) x [cy0, cy1) */
static void cell(int x0, int y0, int x1, int y1, int cx0, int cy0, int cx1, int cy1)
{
    if (ta_nbox < 32)
        ta_box[ta_nbox++] = (ta_box_t){(int16_t)x0, (int16_t)y0, (int16_t)(x1 - 1), (int16_t)(y1 - 1), (int16_t)cx0, (int16_t)cy0,
                                       (int16_t)(cx1 - 1), (int16_t)(cy1 - 1), 0};
}
#define CELL(x0, y0, x1, y1, cx0, cy0, cx1, cy1) cell(x0, y0, x1, y1, cx0, cy0, cx1, cy1)
#else
#define CELL(x0, y0, x1, y1, cx0, cy0, cx1, cy1) ((void)0)
#endif

static void cls(void)
{
    m_set(fb, 0, sizeof fb);
#ifdef M2K_TEXT_AUDIT
    ta_n = 0;
    ta_nbox = 0;
    ta_nrule = 0;
#endif
}

/* c: 0 paper, 1 ink, 2 invert. Clipped once (to the clip and the screen), then a byte at a time, the two end bytes
 * masked: a window over the stage was half a million instructions a pixel at a time */
static void rect(int x, int y, int w, int h, int c)
{
#ifdef M2K_TEXT_AUDIT
    if (c == 0 || (c == 1 && w * h >= 600)) {
        int nb = ta_nbox;
        ta_cover(x, y, w, h);
        if (c == 1) ta_nbox = nb;                     /* (an ink fill is no window) */
    }
#endif
    int x0 = x > clip_x0 ? x : clip_x0, x1 = x + w < clip_x1 ? x + w : clip_x1;
    int y0 = y > clip_y0 ? y : clip_y0, y1 = y + h < clip_y1 ? y + h : clip_y1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > M2K_W) x1 = M2K_W;
    if (y1 > M2K_H) y1 = M2K_H;
    if (x0 >= x1 || y0 >= y1) return;
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3;
    uint8_t m0 = (uint8_t)(0xFFu >> (x0 & 7)), m1 = (uint8_t)(0xFF00u >> (((x1 - 1) & 7) + 1));
    if (b0 == b1) {                                  /* within one byte (a vertical line, a cell's side, a bar):
                                                        a byte a row, half the work */
        const uint8_t m = (uint8_t)(m0 & m1);
        const int i0 = y0 * ROWB + b0, n = y1 * ROWB;
        if (c == 1) for (int i = i0; i < n; i += ROWB) fb[i] |= m;
        else if (c == 0) for (int i = i0; i < n; i += ROWB) fb[i] &= (uint8_t)~m;
        else for (int i = i0; i < n; i += ROWB) fb[i] ^= m;
        return;
    }
    for (uint8_t *r = &fb[y0 * ROWB], *e = &fb[y1 * ROWB]; r < e; r += ROWB) {
        uint8_t *a = r + b0, *b = r + b1;
        if (c == 1) { *a |= m0; *b |= m1; while (++a < b) *a = 0xFF; }
        else if (c == 0) { *a &= (uint8_t)~m0; *b &= (uint8_t)~m1; while (++a < b) *a = 0; }
        else { *a ^= m0; if (b > a) *b ^= m1; while (++a < b) *a ^= 0xFF; }   /* (one byte: inverted once) */
    }
}

/* a pattern: ink where the pattern is, paper elsewhere (opaque) or untouched (transparent) */
static void rect_pat(int x, int y, int w, int h, int p, int opaque)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) {
            int k = pat_ink(p, i, j);
            if (k || opaque) px(i, j, k);
        }
}

/* a dithered gradient: Bayer level l0 at one end, l1 at the other (0..64); vertical or horizontal; ink only */
static void rect_grad(int x, int y, int w, int h, int l0, int l1, int vertical)
{
    int n = vertical ? h : w;
    if (n < 1) return;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int k = vertical ? j : i;
            int l = l0 + ((l1 - l0) * k) / (n > 1 ? n - 1 : 1);
            if (T_BAYER[(y + j) & 7][(x + i) & 7] < l) px(x + i, y + j, 1);
        }
}

static void hline(int x, int y, int w, int c) { rect(x, y, w, 1, c); }

/* rows y .. y + h - 1 of the span x .. x + w - 1 slipped dx sideways (a glitch, in place) */
static void row_slip(int x, int y, int w, int h, int dx)
{
    for (int j = y; j < y + h; j++) {
        if (dx > 0) for (int i = x + w - 1; i >= x; i--) px(i, j, i - dx >= x ? px_get(fb, i - dx, j) : 0);
        else for (int i = x; i < x + w; i++) px(i, j, i - dx < x + w ? px_get(fb, i - dx, j) : 0);
    }
}
static void vline(int x, int y, int h, int c) { rect(x, y, 1, h, c); }
static void frame(int x, int y, int w, int h, int c)
{
    hline(x, y, w, c); hline(x, y + h - 1, w, c);
    vline(x, y + 1, h - 2, c); vline(x + w - 1, y + 1, h - 2, c);
}

/* dots on the screen's checker ((x + y) even), so every dotted line, outline and rule meets the others in step: a
 * row's a byte at a time (the checker's byte, 0xAA on an even row), a column's a byte a dot, clipped as px() clips */
static void dots_h(int x, int y, int w)
{
    int x0 = x > clip_x0 ? x : clip_x0, x1 = x + w < clip_x1 ? x + w : clip_x1;
    if (x0 < 0) x0 = 0;
    if (x1 > M2K_W) x1 = M2K_W;
    if (y < clip_y0 || y >= clip_y1 || y < 0 || y >= M2K_H || x0 >= x1) return;
    const uint8_t ck = (y & 1) ? 0x55 : 0xAA;
    uint8_t *r = &fb[y * ROWB];
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3;
    uint8_t m0 = (uint8_t)(0xFFu >> (x0 & 7)), m1 = (uint8_t)(0xFF00u >> (((x1 - 1) & 7) + 1));
    if (b0 == b1) { r[b0] |= (uint8_t)(ck & m0 & m1); return; }
    r[b0] |= (uint8_t)(ck & m0);
    r[b1] |= (uint8_t)(ck & m1);
    for (int b = b0 + 1; b < b1; b++) r[b] |= ck;
}
static void dots_v(int x, int y, int h)
{
    int y0 = y + ((x + y) & 1), y1 = y + h;
    if (x < clip_x0 || x >= clip_x1 || x < 0 || x >= M2K_W) return;
    if (y0 < clip_y0) y0 += (clip_y0 - y0 + 1) & ~1;
    if (y0 < 0) y0 += (-y0 + 1) & ~1;
    if (y1 > clip_y1) y1 = clip_y1;
    if (y1 > M2K_H) y1 = M2K_H;
    const uint8_t m = (uint8_t)(0x80u >> (x & 7));
    for (int i = y0 * ROWB + (x >> 3), n = y1 * ROWB; i < n; i += 2 * ROWB) fb[i] |= m;
}
static void dots_frame(int x, int y, int w, int h)
{
    dots_h(x, y, w); dots_h(x, y + h - 1, w);
    dots_v(x, y + 1, h - 2); dots_v(x + w - 1, y + 1, h - 2);
}

/* the rules: a band's solid across the screen, a cell's dotted. The audit keeps them: no text's ink within 2 px */
static void rule_h(int x, int y, int w, int dotted)
{
    if (dotted) dots_h(x, y, w); else hline(x, y, w, 1);
#ifdef M2K_TEXT_AUDIT
    ta_rule_add(x, y, x + w - 1, y);
#endif
}
static void rule_v(int x, int y, int h, int dotted)
{
    if (dotted) dots_v(x, y, h); else vline(x, y, h, 1);
#ifdef M2K_TEXT_AUDIT
    ta_rule_add(x, y, x, y + h - 1);
#endif
}

/* a card, the library's: a 1 px frame, its edges rules to the audit (no text within 2 px); dotted: a card with nothing
 * in it (a knob a page leaves free) */
static void card(int x, int y, int w, int h, int dotted)
{
    if (dotted) dots_frame(x, y, w, h); else frame(x, y, w, h, 1);
#ifdef M2K_TEXT_AUDIT
    ta_rule_add(x, y, x + w - 1, y); ta_rule_add(x, y + h - 1, x + w - 1, y + h - 1);
    ta_rule_add(x, y, x, y + h - 1); ta_rule_add(x + w - 1, y, x + w - 1, y + h - 1);
#endif
}

/* a window over the stage: a card on paper, 2 px of paper round it so what it covers stays clear of its frame (no
 * shadow: the library's cards have none) */
static void win(int x, int y, int w, int h)
{
    rect(x - 2, y - 2, w + 4, h + 4, 0);
    card(x, y, w, h, 0);
}

/* the bars a picture's area is drawn in (the library's waveform): column x from row y down to y1 (not included:
 * nothing at y1, where its dotted floor shows), two columns on and one off on the picture's own grid (x0, so
 * neighbouring parts mesh), the light state one on and two off */
static void bar_col(int x, int x0, int y, int y1, int light)
{
    int k = (x - x0 + 3 * 256) % 3;
    if (y < y1 && (light ? k == 0 : k != 2)) rect(x, y, 1, y1 - y, 1);
}

/* a picture's handle: 5 x 5 framed on paper, or 7 x 7 solid while its knob turns */
static void handle(int x, int y, int hot)
{
    if (hot) { rect(x - 3, y - 3, 7, 7, 1); return; }
    rect(x - 2, y - 2, 5, 5, 0);
    frame(x - 2, y - 2, 5, 5, 1);
}

/* where a live thing is (a voice's level, an LFO's value): 5 x 5 solid in a ring of paper, crisp over the bars */
static void marker(int x, int y)
{
    rect(x - 3, y - 3, 7, 7, 0);
    rect(x - 2, y - 2, 5, 5, 1);
}

static void line(int x0, int y0, int x1, int y1, int c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

static void disc(int cx, int cy, int r, int c)
{
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r + r) px(cx + i, cy + j, c);
}

/* a rounded box, Model:Cycles' card */
static void rbox(int x, int y, int w, int h, int r, int p)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int dx = i < r ? r - i : i >= w - r ? i - (w - r - 1) : 0;
            int dy = j < r ? r - j : j >= h - r ? j - (h - r - 1) : 0;
            if (dx * dx + dy * dy > r * r) continue;
            px(x + i, y + j, p == PT_SOLID ? 1 : pat_ink(p, x + i, y + j));
        }
}

/* the type's own corners, Elektron's boxes at twice their size: a 2 x 2 design pixel off each corner. rfill a solid
 * box so (c 1 ink, 0 paper); rframe a frame t thick in c (1: the corner pixel off; 2: the 2 x 2 off) */
static void rfill(int x, int y, int w, int h, int c)
{
    rect(x + 2, y, w - 4, 2, c);
    rect(x, y + 2, w, h - 4, c);
    rect(x + 2, y + h - 2, w - 4, 2, c);
}
static void rframe(int x, int y, int w, int h, int t, int c)
{
    const int k = t > 1 ? 2 : 1;
    rect(x + k, y, w - 2 * k, t, c);
    rect(x + k, y + h - t, w - 2 * k, t, c);
    rect(x, y + k, t, h - 2 * k, c);
    rect(x + w - t, y + k, t, h - 2 * k, c);
}

/* a 2 x 2 block of ink from (x, y): its two rows a byte each where it is inside the clip, else a pixel at a time */
static inline void blk2(int x, int y)
{
    if (x < clip_x0 || y < clip_y0 || x + 2 > clip_x1 || y + 2 > clip_y1 || x < 0 || y < 0 || x + 2 > M2K_W ||
        y + 2 > M2K_H) {
        px(x, y, 1); px(x + 1, y, 1); px(x, y + 1, 1); px(x + 1, y + 1, 1);
        return;
    }
    uint8_t *b = &fb[y * ROWB + (x >> 3)];
    const int s = x & 7;
    if (s < 7) { const uint8_t m = (uint8_t)(0xC0u >> s); b[0] |= m; b[ROWB] |= m; }
    else { b[0] |= 1; b[1] |= 0x80; b[ROWB] |= 1; b[ROWB + 1] |= 0x80; }
}

/* a line drawn with a 2 x 2 brush (the type's stroke) from (x0, y0) to (x1, y1), the brush's top left on the line */
static void line2(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        blk2(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

/* a sprite up to 24 px wide (rows of 3 bytes, MSB first) ORed in ink a row at a time; off the screen's edge a pixel at a
 * time */
static void spr24(const uint8_t *b, int h, int x, int y)
{
    if (x < clip_x0 || y < clip_y0 || x + 24 > clip_x1 || y + h > clip_y1 || x < 0 || y < 0 || x + 24 > M2K_W ||
        y + h > M2K_H) {
        for (int j = 0; j < h; j++)
            for (int i = 0; i < 24; i++)
                if ((b[3 * j + (i >> 3)] >> (7 - (i & 7))) & 1) px(x + i, y + j, 1);
        return;
    }
    const int sh = x & 7;
    for (int j = 0; j < h; j++, b += 3) {
        uint32_t r = ((uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8) >> sh;
        uint8_t *o = &fb[(y + j) * ROWB + (x >> 3)];
        o[0] |= (uint8_t)(r >> 24); o[1] |= (uint8_t)(r >> 16); o[2] |= (uint8_t)(r >> 8);
        if (sh) o[3] |= (uint8_t)r;
    }
}

/* a diamond, 7 px: the stats */
static void diamond(int cx, int cy, int full)
{
    for (int j = -3; j <= 3; j++) {
        int w = 3 - (j < 0 ? -j : j);
        for (int i = -w; i <= w; i++) {
            int edge = (i == -w || i == w);
            if (full || edge) px(cx + i, cy + j, 1);
        }
    }
}

/* -------------------------------------------------------------------------------------------------- text */

enum { TX_INK = 1, TX_PAPER = 0, TX_INV = 2, TX_GRAD = 99 };
/* the gradient a TX_GRAD text is filled with: an outline in ink, inside Bayer from tg_l0 (top) to tg_l1 (bottom) */
static int tg_y0, tg_h = 1, tg_l0 = 16, tg_l1 = 64;
static void text_grad(int y0, int h, int l0, int l1) { tg_y0 = y0; tg_h = h > 1 ? h : 1; tg_l0 = l0; tg_l1 = l1; }

static inline int glyph_bit(const m2k_font_t *f, int g, int gx, int gy)
{
    int w = f->w[g];
    if (gx < 0 || gy < 0 || gx >= w || gy >= f->h) return 0;
    const int z = f->z, nx = gx / z;                     /* (the native glyph's pixel under it) */
    const uint8_t *b = f->bits + f->off[g] + (gy / z) * ((w / z + 7) >> 3);
    return (b[nx >> 3] >> (7 - (nx & 7))) & 1;
}

static inline int glyph_of(const m2k_font_t *f, int c)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < f->first || c > f->last) c = ' ';
    return c - f->first;
}

static int text_w(const m2k_font_t *f, const char *s, int track)
{
    int w = 0, n = 0;
    for (; *s; s++, n++) w += f->w[glyph_of(f, (unsigned char)*s)] + f->track + track;
    return n ? w - f->track - track : 0;
}

/* a glyph in one fill (1 ink, 0 paper, else inverted): its native rows widened z times by a table, a screen byte at a
 * time and z rows a native row (a line of the small type was 30 000 instructions a pixel at a time) */
static void glyph_solid(const m2k_font_t *f, int g, int x, int y, int fill)
{
    const int w = f->w[g], z = f->z, bpr = (w / z + 7) >> 3;
    int vx0 = x > clip_x0 ? x : clip_x0, vx1 = x + w < clip_x1 ? x + w : clip_x1;
    int vy0 = y > clip_y0 ? y : clip_y0, vy1 = y + f->h < clip_y1 ? y + f->h : clip_y1;
    if (vx0 < 0) vx0 = 0;
    if (vy0 < 0) vy0 = 0;
    if (vx1 > M2K_W) vx1 = M2K_W;
    if (vy1 > M2K_H) vy1 = M2K_H;
    if (vx0 >= vx1 || vy0 >= vy1) return;
    const uint8_t *src = f->bits + f->off[g];
    if (x >= 0 && bpr == 1 && (z == 2 || z == 3) && vx0 == x && vx1 == x + w && vy0 == y) {
        /* whole across (nearly every glyph; the footer's lose their descenders' last row to the screen's foot): a
         * native row widened into one word, written to its z rows straight, an empty row (the descenders', a gap)
         * skipped. Its bytes past the first only where they have ink, so none past the glyph's last pixel is touched */
        const int sh = x & 7, nr = (vy1 - y) / z, part = (vy1 - y) - nr * z;
        uint8_t *row = &fb[y * ROWB + (x >> 3)];
        if (z == 2) {                                   /* (the small type, most of a frame's: its two rows written
                                                           together, three bytes at most) */
            for (int jn = 0; jn <= nr; jn++, row += 2 * ROWB) {
                const uint8_t nb = jn < nr || part ? src[jn] : 0;
                if (!nb) continue;
                const uint32_t r = (uint32_t)FONT_Z2[nb] << (8 - sh);
                const uint8_t a = (uint8_t)(r >> 16), b = (uint8_t)(r >> 8), c = (uint8_t)r;
                uint8_t *q = row, *q2 = jn < nr ? row + ROWB : row;   /* (a last half row: written twice) */
                if (fill == 1) {
                    q[0] |= a; q2[0] |= a;
                    if (b) { q[1] |= b; q2[1] |= b; }
                    if (c) { q[2] |= c; q2[2] |= c; }
                } else if (fill == 0) {
                    q[0] &= (uint8_t)~a; q2[0] &= (uint8_t)~a;
                    if (b) { q[1] &= (uint8_t)~b; q2[1] &= (uint8_t)~b; }
                    if (c) { q[2] &= (uint8_t)~c; q2[2] &= (uint8_t)~c; }
                } else {
                    q[0] ^= a;
                    if (b) q[1] ^= b;
                    if (c) q[2] ^= c;
                    if (q2 != q) { q2[0] ^= a; if (b) q2[1] ^= b; if (c) q2[2] ^= c; }
                }
            }
            return;
        }
        for (int jn = 0; jn <= nr; jn++, row += z * ROWB) {
            const uint8_t nb = jn < nr || part ? src[jn] : 0;
            if (!nb) continue;
            const uint32_t r = (z == 2 ? (uint32_t)FONT_Z2[nb] << 16 : FONT_Z3[nb] << 8) >> sh;
            const uint8_t a = (uint8_t)(r >> 24), b = (uint8_t)(r >> 16), c = (uint8_t)(r >> 8), d = (uint8_t)r;
            for (uint8_t *q = row, *e = row + (jn < nr ? z : part) * ROWB; q < e; q += ROWB) {
                if (fill == 1) { q[0] |= a; if (b) q[1] |= b; if (c) q[2] |= c; if (d) q[3] |= d; }
                else if (fill == 0) {
                    q[0] &= (uint8_t)~a;
                    if (b) q[1] &= (uint8_t)~b;
                    if (c) q[2] &= (uint8_t)~c;
                    if (d) q[3] &= (uint8_t)~d;
                } else { q[0] ^= a; if (b) q[1] ^= b; if (c) q[2] ^= c; if (d) q[3] ^= d; }
            }
        }
        return;
    }
    if (x >= 0 && bpr == 1 && z >= 1 && z <= 3) {
        /* a native row's byte widened z times by a table into one word, shifted once into the screen's bytes (the
         * first and last masked to the clip) and laid on its z rows: the type is most of a frame */
        const int bx = x >> 3, sh = x & 7, k0 = (vx0 >> 3) - bx, k1 = ((vx1 - 1) >> 3) - bx;
        const uint8_t mlo = (uint8_t)(0xFFu >> (vx0 & 7)), mhi = (uint8_t)(0xFF00u >> (((vx1 - 1) & 7) + 1));
        for (int j = vy0; j < vy1;) {
            const int jn = (j - y) / z, je = y + (jn + 1) * z < vy1 ? y + (jn + 1) * z : vy1;
            const uint8_t nb = src[jn];
            const uint32_t r = z == 2 ? (uint32_t)FONT_Z2[nb] << 16 : z == 3 ? FONT_Z3[nb] << 8 : (uint32_t)nb << 24;
            uint8_t v[5];
            for (int k = k0; k <= k1; k++) {
                v[k] = k < 4 ? (uint8_t)(r >> (24 - 8 * k + sh)) : (uint8_t)(r << (8 - sh));
                if (k == k0) v[k] &= mlo;
                if (k == k1) v[k] &= mhi;
            }
            for (; j < je; j++) {
                uint8_t *row = &fb[j * ROWB + bx];
                for (int k = k0; k <= k1; k++) {
                    if (fill == 1) row[k] |= v[k];
                    else if (fill == 0) row[k] &= (uint8_t)~v[k];
                    else row[k] ^= v[k];
                }
            }
        }
        return;
    }
    for (int j = vy0; j < vy1; j++)                      /* (off the screen's left, or wider: a pixel at a time) */
        for (int i = vx0; i < vx1; i++)
            if (glyph_bit(f, g, i - x, j - y)) {
                uint8_t *b = &fb[j * ROWB + (i >> 3)], m = (uint8_t)(0x80u >> (i & 7));
                if (fill == 1) *b |= m;
                else if (fill == 0) *b &= (uint8_t)~m;
                else *b ^= m;
            }
}

/* fill: 1 ink, 0 paper, 2 invert, or 100 + pattern: an outline in ink, the inside in that pattern */
static int text(const m2k_font_t *f, int x, int y, const char *s, int fill, int track)
{
    int x0 = x;
#ifdef M2K_TEXT_AUDIT
    ta_t r = {32767, 32767, -1, -1, 0, {0}};
    for (int i = 0; i < 13 && s[i]; i++) r.s[i] = s[i];
#endif
    for (; *s; s++) {
        int g = glyph_of(f, (unsigned char)*s), w = f->w[g];
#ifdef M2K_TEXT_AUDIT
        for (int gy = 0; gy < f->h; gy++)
            for (int gx = 0; gx < w; gx++) {
                if (!glyph_bit(f, g, gx, gy)) continue;
                int ax = x + gx, ay = y + gy;
                if (ax < clip_x0 || ay < clip_y0 || ax >= clip_x1 || ay >= clip_y1) r.cut = 1;
                else {
                    if (ax < r.x0) r.x0 = (int16_t)ax;
                    if (ax > r.x1) r.x1 = (int16_t)ax;
                    if (ay < r.y0) r.y0 = (int16_t)ay;
                    if (ay > r.y1) r.y1 = (int16_t)ay;
                }
            }
#endif
        if (fill != TX_GRAD && fill < 100) glyph_solid(f, g, x, y, fill);
        else
            for (int gy = 0; gy < f->h; gy++)          /* an outline and a fill: a pixel at a time */
                for (int gx = 0; gx < w; gx++) {
                    if (!glyph_bit(f, g, gx, gy)) continue;
                    int edge = !glyph_bit(f, g, gx - 1, gy) || !glyph_bit(f, g, gx + 1, gy) ||
                               !glyph_bit(f, g, gx, gy - 1) || !glyph_bit(f, g, gx, gy + 1);
                    if (fill == TX_GRAD) {
                        int yy = y + gy - tg_y0, l = tg_l0 + ((tg_l1 - tg_l0) * CLAMP(yy, 0, tg_h)) / tg_h;
                        px(x + gx, y + gy, edge || T_BAYER[(y + gy) & 7][(x + gx) & 7] < l);
                    } else px(x + gx, y + gy, edge || pat_ink(fill - 100, x + gx, y + gy));
                }
        x += w + f->track + track;
    }
#ifdef M2K_TEXT_AUDIT
    if ((r.x1 >= 0 || r.cut) && ta_n < 512) ta[ta_n++] = r;
#endif
    return x - x0;
}

static int text_r(const m2k_font_t *f, int xr, int y, const char *s, int fill, int track)
{
    int w = text_w(f, s, track);
    text(f, xr - w, y, s, fill, track);
    return w;
}

static int text_c(const m2k_font_t *f, int cx, int y, const char *s, int fill, int track)
{
    int w = text_w(f, s, track);
    text(f, cx - w / 2, y, s, fill, track);
    return w;
}

/* a tag: the small type on a solid box, a pixel of ink above and below its capitals and two each side (on); off, the
 * same type plain in the same place. Its box: rows y .. y + base + 1, its type's capitals y + 1 .. y + base */
static int tag(int x, int y, const char *s, int on)
{
    int w = text_w(&F_S, s, 0) + 4;
    if (on) rfill(x, y, w, F_S.base + 2, 1);
    text(&F_S, x + 2, y + 1, s, !on, 0);
    return w;
}

/* the library's tag: f's type in a 1 px frame, a pixel clear above and below its capitals and two each side; on, the
 * box solid and the type paper. Its box: rows y .. y + base + 3 (OTAG_H), its capitals from y + 2 */
#define OTAG_H(f) ((f)->base + 4)
static int otag_w(const m2k_font_t *f, const char *s) { return text_w(f, s, 0) + 6; }
static int otag(const m2k_font_t *f, int x, int y, const char *s, int on)
{
    int w = otag_w(f, s);
    if (on) rfill(x, y, w, OTAG_H(f), 1); else rframe(x, y, w, OTAG_H(f), 1, 1);
    text(f, x + 3, y + 2, s, !on, 0);
    return w;
}

/* ------------------------------------------------------------------------------------------------ images */

/* 1-bit image, MSB first, rows of ceil(w/8) bytes; mode 1: ink where set, 2: also paper where clear, 3: xor */
static void blit(const uint8_t *bits, int w, int h, int x, int y, int mode)
{
    int bpr = (w + 7) >> 3;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int on = (bits[j * bpr + (i >> 3)] >> (7 - (i & 7))) & 1;
            if (on) px(x + i, y + j, mode == 3 ? 2 : mode == 4 ? 0 : 1);   /* 4: cleared where set (a cut-out) */
            else if (mode == 2) px(x + i, y + j, 0);
        }
}

/* the same, revealed through a Bayer threshold (0..64) */
static void blit_dither(const uint8_t *bits, int w, int h, int x, int y, int level)
{
    int bpr = (w + 7) >> 3;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (((bits[j * bpr + (i >> 3)] >> (7 - (i & 7))) & 1) && T_BAYER[(y + j) & 7][(x + i) & 7] < level)
                px(x + i, y + j, 1);
}

/* ------------------------------------------------------------------------------------------- transitions */

/* out = prev dissolved into fb: level 0 all prev, 64 all new (8 px period: one mask byte per row) */
static void dissolve(int level)
{
    for (int y = 0; y < M2K_H; y++) {
        uint8_t m = 0;
        for (int i = 0; i < 8; i++) if (T_BAYER[y & 7][i] < level) m |= (uint8_t)(0x80u >> i);
        const uint8_t *a = &fb_prev[y * ROWB], *b = &fb[y * ROWB];
        uint8_t *o = &fb_out[y * ROWB];
        for (int i = 0; i < ROWB; i++) o[i] = (uint8_t)((b[i] & m) | (a[i] & ~m));
    }
}

static inline uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

/* the screen change, cyberpunk in one bit: the old screen tears into thin bands that slide sideways and flash
 * inverted; the new one is dealt in band by band (at random, or top down) behind a scan bar; broken 8 x 8 blocks
 * flicker between the two; the last bands slip back into place with a dithered echo. Bands move only sideways
 * and by whole bytes (8 px), so every line of text stays on its line, sharp. p: 0..255 through the change; the
 * same seed, the same change. */
static void cyber(int p, uint32_t seed, int down)
{
    static uint8_t row[ROWB];
    for (int band = 0; band < M2K_H / 4; band++) {
        /* bands of 8 px, on the screen's byte grid, a few of them split in two */
        int pair = band >> 1;
        uint32_t hp = hash32(seed ^ ((uint32_t)pair * 0x9E3779B9u));
        uint32_t h = ((hp >> 28) & 3u) == 0 ? hash32(hp + (uint32_t)band) : hp;
        int y0 = band * 4;
        int at = down ? 36 + y0 * 150 / M2K_H + (int)(h & 15u) : 44 + (int)(h % 156u);   /* when it turns new */
        int old = p < at, k = old ? at - p : p - at;
        int off = (int)((h >> 8) % 7u) - 3;                     /* bytes */
        int sh = old ? (k < 40 ? off * (40 - k) / 40 : 0)       /* torn away as its turn nears */
                     : (k < 48 ? -off * (48 - k) / 48 : 0);     /* in from the other side, then still */
        int flip = ((h >> 16) & 7u) == 0 && k < 12;
        int echo = !old && ((h >> 20) & 3u) == 0 && k < 72;
        const uint8_t *src = old ? fb_prev : fb;
        for (int y = y0; y < y0 + 4; y++) {
            const uint8_t *r = &src[y * ROWB];
            for (int i = 0; i < ROWB; i++) { int j = i - sh; row[i] = (j >= 0 && j < ROWB) ? r[j] : 0; }
            if (echo) { uint8_t d = (y & 1) ? 0xAA : 0x55; for (int i = ROWB - 1; i > 0; i--) row[i] |= row[i - 1] & d; }
            if (flip) for (int i = 0; i < ROWB; i++) row[i] ^= 0xFF;
            m_cpy(&fb_out[y * ROWB], row, ROWB);
        }
    }
    /* broken blocks, densest mid-change, reshuffled every 16 steps of p */
    int dens = p < 128 ? p : 255 - p;
    dens = (dens * dens) >> 10;                                 /* 0..15 */
    uint32_t q = (uint32_t)p >> 4;
    for (int cy = 0; cy < M2K_H / 8; cy++)
        for (int cx = 0; cx < ROWB; cx++) {
            uint32_t h = hash32(seed + q * 7919u + (uint32_t)(cy * ROWB + cx) * 2654435761u);
            if ((int)(h & 255u) >= dens) continue;
            int kind = (int)((h >> 8) & 3u);
            for (int y = cy * 8; y < cy * 8 + 8; y++) {
                uint8_t *o = &fb_out[y * ROWB + cx];
                if (kind == 0) *o = (uint8_t)~fb[y * ROWB + cx];                     /* the new, negative */
                else if (kind == 1) *o = (y & 1) ? 0xFF : 0x00;                     /* scanlines */
                else if (kind == 2) *o = (y & 2) ? 0xAA : 0x55;                     /* a checker */
                else *o = fb_prev[y * ROWB + cx];                                   /* the old, left behind */
            }
        }
    /* the scan bar: a dithered trail, a gap, then three lines of light */
    int scan = p * (M2K_H + 32) / 210 - 16;
    for (int y = scan; y < scan + 12; y++) {
        if (y < 0 || y >= M2K_H) continue;
        int d = y - scan;
        uint8_t m = d >= 9 ? 0xFF : d == 8 ? 0x00 : (d & 1) ? 0x00 : d < 4 ? ((d & 2) ? 0x88 : 0x22) : ((d & 2) ? 0xAA : 0x55);
        uint8_t *o = &fb_out[y * ROWB];
        for (int i = 0; i < ROWB; i++) o[i] ^= m;
    }
}

/* the glitch, only between screens: two beats of 60 ms. A few bands, on the 8-px grid, slip sideways by one
 * step (8 px) and back the other way; nothing moves up or down, so every line of text stays on its line. The
 * same seed gives the same glitch. bands: how many slip in the first beat. */
static void glitch(uint8_t *buf, uint32_t t, uint32_t seed, int bands)
{
    if (t >= 120) return;
    uint32_t s = seed | 1u;
    int beat = (int)(t / 60);
    static uint8_t row[ROWB];
    int n = beat == 0 ? bands : (bands + 1) / 2;
    for (int k = 0; k < n; k++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        int y0 = (int)(s % 27u) * 8 + 16, h = 8 * (1 + (int)((s >> 8) & 1u));
        int sh = ((k + beat) & 1) ? -1 : 1;           /* one byte: 8 px */
        for (int y = y0; y < y0 + h && y < M2K_H; y++) {
            uint8_t *r = &buf[y * ROWB];
            for (int i = 0; i < ROWB; i++) { int j = i - sh; row[i] = (j >= 0 && j < ROWB) ? r[j] : 0; }
            m_cpy(r, row, ROWB);
        }
    }
}
