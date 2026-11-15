/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's update screen on the FM-1, drawn straight to the panel (lcd.c) in MONO2K's own type (m2k_fonts.h, the
 * small size's glyphs at any scale). The app area's 143 sectors of 4 KB (0x04000..0x93000) stand as a grid of cells
 * that fill as the update loader writes them, the sector being written blinking as a cursor; under them how far it
 * is and a word for the state. While the loader waits for an installer (also after a power cut in the middle of a
 * write) a box says so over the grid and a cell runs through the others.
 *
 * The app draws it while it stages the loader (platform.c, ota_show: the panel is already set up); the update
 * loader while it writes (loader.c, after its own lcd_init). It calls lcd.c and nothing else, and keeps everything in
 * RAM: in the loader the flash's XIP is off. If the panel does not answer (lcd.c counted a timeout) it draws nothing
 * more, so the update itself never waits on the screen.
 *
 *   us_begin(right)        the frame: MONO2K, `right` (the version, or UPDATE), the grid to come
 *   us_wait()              waiting for an installer: the box, the scan
 *   us_check()             an installer came: the box away, 0 %, CHECKING (the package's head and key)
 *   us_write(n, total)     n of total sectors written (n going back: a new pass, the cells after it cleared)
 *   us_done()              written: 100 %, DONE, and the grid gives way to MONO2K's isotype in a wave (us_tick)
 *   us_stop(a, b, word)    it stopped: the cursor off, a box with a and b, word on the right
 *   us_bottom(l, r)        the line under the grid: l large on the left, r on the right (each only if it changed)
 *   us_box(a, b)           two lines in a box over the grid (us_unbox() takes it away)
 *   us_tick(ms)            the cursor's blink and the waiting scan, called often
 * Between two sectors the loader redraws a cell or two and, when the number changes, the number: about a
 * millisecond, with no read outstanding. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#include "m2k_fonts.h"
#pragma GCC diagnostic pop

#define US_INK 0xFFFFu
#define US_BG 0x0000u
#define US_COLS 13u
#define US_ROWS 11u
#define US_N (US_COLS * US_ROWS)             /* 143 = (0x93000 - 0x4000) / 0x1000 */
#define US_CELL 13u
#define US_PITCH 15u
#define US_X0 24u                            /* 24 .. 217 across, the type's edges too */
#define US_Y0 38u                            /* 38 .. 201 down */
#define US_X1 (US_X0 + (US_COLS - 1u) * US_PITCH + US_CELL)
#define US_BOX_R0 4u                         /* the box covers rows 4..6 */
#define US_BOX_R1 6u

enum { US_IDLE, US_WAIT, US_WRITE, US_DONE };
enum { UC_TO_COME, UC_DONE, UC_CURSOR, UC_SCAN, UC_DOT };
/* MONO2K's isotype (brand/mono2k-iso.svg) on the grid, a row a word, the left column the top bit */
static const uint16_t US_ISO[US_ROWS] = {
    0x0318, 0x0318, 0x0318, 0x1FBF, 0x1DF7, 0x1DF7, 0x1DF7, 0x1CE7, 0x1C07, 0x1C07, 0x1C07,
};

static struct {
    uint8_t ok, mode, boxed, blink, wave;
    uint16_t done, scan, lw, rw;             /* lw, rw: the widths of the line's two texts as drawn */
    uint32_t t;
    char l[8], r[20];
} us;
static uint16_t us_px[15u * 21u];           /* one glyph (up to 3 times), one cell or a rule, as the panel takes it */

static uint32_t us_glyph(uint32_t c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32u;
    return c < 32u || c > 95u ? 0u : c - 32u;
}

static uint32_t us_text_w(const char *s, uint32_t z)
{
    uint32_t w = 0;
    for (; *s; s++)
        w += (F_S_W[us_glyph((uint8_t)*s)] / 2u + 1u) * z;
    return w ? w - z : 0u;
}

/* the type's native glyphs (7 rows, a byte each) drawn z times over, a glyph a transfer */
static void us_text(uint32_t x, uint32_t y, const char *s, uint32_t z)
{
    if (!us.ok)
        return;
    for (; *s; s++) {
        uint32_t g = us_glyph((uint8_t)*s), w = F_S_W[g] / 2u * z, h = 7u * z, i, j;
        const uint8_t *b = F_S_BITS + F_S_OFF[g];
        if (x + w > 240u || w * h > sizeof us_px / sizeof us_px[0])
            return;
        lcd_sync();                                       /* the last glyph may still be going out of us_px */
        for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
                us_px[j * w + i] = b[j / z] >> (7u - i / z) & 1u ? US_INK : US_BG;
        lcd_blit(x, y, w, h, us_px);
        x += w + z;
    }
}

static void us_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    if (us.ok)
        lcd_fill(x, y, w, h, c);
}

/* a dotted rule across the grid, every other pixel */
static void us_rule(uint32_t y)
{
    uint32_t i;
    if (!us.ok)
        return;
    lcd_sync();
    for (i = 0; i < US_X1 - US_X0 && i < sizeof us_px / sizeof us_px[0]; i++)
        us_px[i] = i & 1u ? US_BG : US_INK;
    lcd_blit(US_X0, y, US_X1 - US_X0, 1, us_px);
}

static void us_cell(uint32_t c, uint32_t kind)
{
    uint32_t x, y, r = c / US_COLS;
    if (!us.ok || c >= US_N || (us.boxed && r >= US_BOX_R0 && r <= US_BOX_R1))
        return;
    lcd_sync();
    for (y = 0; y < US_CELL; y++)
        for (x = 0; x < US_CELL; x++) {
            uint32_t edge = !x || !y || x == US_CELL - 1u || y == US_CELL - 1u, on;
            if (kind == UC_DONE)
                on = 1;
            else if (kind == UC_CURSOR)                   /* a block cursor, a pixel inside the outline */
                on = edge || (x >= 2u && y >= 2u && x <= US_CELL - 3u && y <= US_CELL - 3u);
            else if (kind == UC_SCAN)                     /* the waiting scan: a coarse checker */
                on = edge || ((x >> 1) + (y >> 1)) % 2u == 0u;
            else if (kind == UC_DOT)                      /* round the isotype: a dot */
                on = x == US_CELL / 2u && y == US_CELL / 2u;
            else
                on = edge;
            us_px[y * US_CELL + x] = on ? US_INK : US_BG;
        }
    lcd_blit(US_X0 + c % US_COLS * US_PITCH, US_Y0 + r * US_PITCH, US_CELL, US_CELL, us_px);
}

static uint32_t us_kind(uint32_t c) { return c < us.done ? UC_DONE : UC_TO_COME; }

static int us_same(char *had, const char *s, uint32_t n)       /* (and keeps s, cut to n - 1) */
{
    uint32_t i;
    for (i = 0; i + 1u < n && s[i] && had[i] == s[i]; i++)
        ;
    if ((i + 1u == n || !s[i]) && !had[i])
        return 1;
    for (i = 0; i + 1u < n && s[i]; i++)
        had[i] = s[i];
    had[i] = 0;
    return 0;
}

static void us_bottom(const char *l, const char *r)
{
    if (!us_same(us.l, l ? l : "", sizeof us.l)) {
        us_rect(US_X0, 213, us.lw, 21, US_BG);
        us.lw = (uint16_t)us_text_w(us.l, 3u);
        us_text(US_X0, 213, us.l, 3u);                    /* (21 high: its baseline at 228) */
    }
    if (!us_same(us.r, r ? r : "", sizeof us.r)) {
        us_rect(US_X1 - us.rw, 218, us.rw, 14, US_BG);
        us.rw = (uint16_t)us_text_w(us.r, 2u);
        us_text(US_X1 - us.rw, 218, us.r, 2u);
    }
}

static void us_box(const char *a, const char *b)
{
    uint32_t y0 = US_Y0 + US_BOX_R0 * US_PITCH, h = (US_BOX_R1 - US_BOX_R0) * US_PITCH + US_CELL;
    us.boxed = 1;
    us_rect(US_X0, y0, US_X1 - US_X0, h, US_INK);
    us_rect(US_X0 + 1u, y0 + 1u, US_X1 - US_X0 - 2u, h - 2u, US_BG);
    us_text(120u - us_text_w(a, 2u) / 2u, y0 + 4u, a, 2u);
    us_text(120u - us_text_w(b, 2u) / 2u, y0 + 23u, b, 2u);
}

static void us_unbox(void)
{
    uint32_t c;
    if (!us.boxed)
        return;
    us.boxed = 0;
    us_rect(US_X0, US_Y0 + US_BOX_R0 * US_PITCH, US_X1 - US_X0, (US_BOX_R1 - US_BOX_R0 + 1u) * US_PITCH, US_BG);
    for (c = US_BOX_R0 * US_COLS; c < (US_BOX_R1 + 1u) * US_COLS; c++)
        us_cell(c, us_kind(c));
}

static void us_begin(const char *right)
{
    uint32_t c;
    us.mode = US_IDLE; us.boxed = 0; us.done = 0; us.scan = 0; us.blink = 0;
    us.lw = us.rw = 0; us.l[0] = us.r[0] = 0;
    us_rect(0, 0, 240, 240, US_BG);
    us_text(US_X0, 10, "MONO2K", 2u);
    us_text(US_X1 - us_text_w(right, 2u), 10, right, 2u);
    us_rule(30);
    for (c = 0; c < US_N; c++)
        us_cell(c, UC_TO_COME);
    us_rule(207);
}

static void us_wait(void)
{
    us.mode = US_WAIT;
    us_box("UPDATE MODE", "OPEN THE INSTALLER");
    us_bottom(0, "WAITING");
}

static void us_pct(uint32_t n, uint32_t total, const char *word)
{
    char t[5];
    uint32_t p = total ? n * 100u / total : 0u, k = 0;
    if (p >= 100u) t[k++] = '1';
    if (p >= 10u) t[k++] = (char)('0' + p / 10u % 10u);
    t[k++] = (char)('0' + p % 10u);
    t[k++] = '%';
    t[k] = 0;
    us_bottom(t, word);
}

static void us_leave_wait(void)
{
    if (us.mode == US_WAIT)
        us_cell(us.scan, us_kind(us.scan));
    us.mode = US_IDLE;
    us_unbox();
}

static void us_check(void)
{
    us_leave_wait();
    us_bottom("0%", "CHECKING");
}

static void us_write(uint32_t n, uint32_t total)
{
    uint32_t c;
    if (n > US_N)
        n = US_N;
    if (us.mode != US_WRITE) {
        us_leave_wait();
        us.mode = US_WRITE;
    }
    if (n < us.done) {                                    /* a pass again: from n on, to come */
        for (c = n; c < US_N; c++)
            us_cell(c, UC_TO_COME);
        us.done = (uint16_t)n;
    }
    for (c = us.done; c < n; c++)
        us_cell(c, UC_DONE);
    us.done = (uint16_t)n;
    if (n < US_N)
        us_cell(n, UC_CURSOR);
    us.blink = 0;
    us_pct(n, total, "WRITING");
}

static void us_done(void)
{
    us_leave_wait();
    us.done = US_N;
    us.mode = US_DONE;
    us.wave = 0;
    us_bottom("100%", "DONE");
}

static void us_stop(const char *a, const char *b, const char *word)
{
    if (us.mode == US_WRITE && us.done < US_N)
        us_cell(us.done, UC_TO_COME);
    us_leave_wait();
    us_box(a, b);
    us_bottom(us.l, word);
}

static void us_tick(uint32_t ms)
{
    if (!us.ok || ms - us.t < (us.mode == US_WAIT ? 70u : us.mode == US_DONE ? 28u : 180u))
        return;
    us.t = ms;
    if (us.mode == US_DONE) {                             /* a diagonal a step: the isotype, dots round it */
        uint32_t c;
        if (us.wave >= US_COLS + US_ROWS - 1u)
            return;
        for (c = 0; c < US_N; c++)
            if (c % US_COLS + c / US_COLS == us.wave)
                us_cell(c, US_ISO[c / US_COLS] >> (US_COLS - 1u - c % US_COLS) & 1u ? UC_DONE : UC_DOT);
        us.wave++;
        return;
    }
    if (us.mode == US_WRITE && us.done < US_N) {          /* the cursor blinks where the loader writes */
        us.blink ^= 1u;
        us_cell(us.done, us.blink ? UC_TO_COME : UC_CURSOR);
    } else if (us.mode == US_WAIT) {                      /* a cell runs through the grid, round the box */
        uint32_t c = us.scan;
        us_cell(c, us_kind(c));
        do
            c = (c + 1u) % US_N;
        while (c / US_COLS >= US_BOX_R0 && c / US_COLS <= US_BOX_R1);
        us.scan = (uint16_t)c;
        us_cell(c, UC_SCAN);
    }
}
