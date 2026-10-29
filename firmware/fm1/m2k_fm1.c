/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the M-VAVE FM-1: the core (firmware/src) on the open FM-1 platform (src/platform.c: CS-1's, after
 * Dinghy, SLOOP and Felucca; README.md says whose is what): this unit, the platform and the glue; the core is
 * its own unit (both carry statics of the same names).
 *
 * The core runs in the main loop alone, as it runs in the browser's worklet: the loop renders the sound ahead into a
 * ring (RING blocks of M2K_SUB * 8 frames) and the audio interrupt only copies out of it, so no interrupt ever lands
 * in the middle of an edit. Between two blocks the loop takes the panel (keys, buttons, knobs), MIDI, draws a frame
 * when the last has gone out, and sends the screen one changed strip at a time (a whole screen is 77 ms of SPI: the
 * sound would starve if the loop waited for it), and MIDI out as the sound it belongs to plays. */
#define APP_NAME "MONO2K"
#ifndef APP_VER
#define APP_VER "0.0"
#endif
/* the store's region: the last 128 KiB of the family's data area before the update's staging (0x93000..0xE0000;
 * Felucca's projects and samples live in that area too) */
#define BANK_AT 0x000C0000u
#define BANK_LEN 0x00020000u
#define CV_MAX (240u * 18u)                     /* the platform's text: a line of FONT_S at a time (crash, update, rescue) */
#define APP_SYSEX app_sysex                     /* the Patch Editor's link (below) */
#include "platform.c"
#include "m2k.h"                                 /* the core: its own unit (core.o), its API alone */

/* ---- the sound: the ring the main loop fills and the audio interrupt empties */
#define BLK 128u                                /* frames a render */
#define RING 6u                                 /* blocks: 17 ms ahead of the interrupt's own 5.8 ms */
static int16_t ring[RING][BLK * 2u];           /* (RAM, not the pool: the core's buffers fill that) */
static volatile uint32_t ring_w, ring_r;        /* blocks written / read, free-running */
static volatile uint32_t ring_short;            /* halves the interrupt found the ring short for */

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t i, f = 0;
    while (f < n) {
        if (ring_w == ring_r) {                 /* the loop fell behind: silence, counted */
            ring_short++;
            return;
        }
        const int16_t *b = ring[ring_r % RING];
        for (i = 0; i < BLK * 2u; i++)
            out[f * 2u + i] = b[i];
        ring_r++;
        f += BLK;
    }
}

/* at most half the ring a pass: were the core slower than the sound, the loop still reaches the panel and the
 * watchdog (the sound would stutter, the FM-1 would not reset) */
static uint32_t load_ticks, load_frames;        /* render time against the frames rendered, the meter's */
static uint32_t busy_ticks, busy_t0;            /* and the same over each quarter second, for the screen's pace */
static uint32_t frame_gap = 33u;                /* ms between frames: 33 (30 a second), further while the render is busy */

static void render_ahead(void)
{
    uint32_t k;
    for (k = 0; k < RING / 2u && ring_w - ring_r < RING; k++) {
        uint32_t t0 = fm1_ticks(), dt;
        m2k_render(ring[ring_w % RING], (int)BLK);
        dt = fm1_ticks() - t0;
        load_ticks += dt;
        busy_ticks += dt;
        load_frames += BLK;
        RING_PUBLISH();
        ring_w++;
    }
    uint32_t span = fm1_ticks() - busy_t0;
    if (span >= 250u * 1000u * FM1_TICKS_PER_US) {       /* the render's share: under 70 % 30 frames a second, to 85 % 20, */
        uint32_t pct = busy_ticks / (span / 100u);         /* over it 15 */
        frame_gap = pct < 70u ? 33u : pct < 85u ? 50u : 66u;
        busy_ticks = 0;
        busy_t0 += span;
    }
}

/* ---- the second core: the core hands it some tracks of each block (m2k_par_begin), CS-1's audio worker runs them */
#if DINGHY_DUAL_CORE
static void (*par_fn)(void);
static void par_tramp(void *c) { (void)c; par_fn(); }
static int par_begin(void (*job)(void)) { par_fn = job; return audio_worker_submit(par_tramp, 0); }
static uint32_t job_ticks;                      /* the second core's time on jobs, for the meter */
static void par_end(void) { audio_worker_join(); job_ticks += audio_worker.last_job_ticks; }
#endif

/* ---- MIDI out (GLO > MIDI's clock and notes, IN's thru): each message once the interrupt has taken the sound it
 * belongs to (the ring runs ahead of the speaker; sent at render time it would run 17 ms ahead of the sound). USB only:
 * the FM-1's TRS jack is an input */
static void midi_drain(void)
{
    uint8_t m[3];
    int n;
    while ((n = m2k_midi_out(m, ring_r * BLK)) > 0) midi_send(m[0], n > 1 ? m[1] : 0u, n > 2 ? m[2] : 0u);
}

/* ---- the panel: the platform's ids (platform.c) to the core's (m2k.h) */
static int button_of(uint32_t id) { return id == BTN_OCT_DOWN ? B_OCTD : id == BTN_OCT_UP ? B_OCTU : (int)id - 2; }
static const uint8_t ENC_OF[7] = {E_SELECT, E_ALGO, E_K1, E_K2, E_K3, E_K4, E_PRESET};
static uint32_t notes_prev;

static void take_panel(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), notes = fm1_in.notes, ch = notes ^ notes_prev, i;
    for (i = 0; i < 14u; i++) {
        if (pr >> i & 1u) m2k_button(button_of(i), 1);
        if (rel >> i & 1u) m2k_button(button_of(i), 0);
    }
    for (i = 0; i < M2K_KEYS; i++)
        if (ch >> i & 1u) m2k_key((int)i, (int)(notes >> i & 1u));
    notes_prev = notes;
    for (i = 0; i < 7u; i++) {
        int32_t d = fm1_enc_take(i);
        if (d) m2k_encoder(ENC_OF[i], d);
    }
    uint8_t src, st, d1, d2;
    while (midi_get(&src, &st, &d1, &d2)) m2k_midi_from(src == MIDI_TRS ? M2K_TRS : M2K_USB, st, d1, d2);
}

/* ---- a test build's meter (M2K_FM1_DIAG): over the top 16 lines, in red, shares of real time in %: CPU the first
 * core rendering the sound (waiting for the second included: what must stay under 100), W the second core's work,
 * F drawing the screen; a block at the top right while the sound ran short in the last second */
#ifndef M2K_FM1_DIAG
#define M2K_FM1_DIAG 0
#endif
static uint32_t diag_short_ms, diag_last_short, frame_ticks;
#if M2K_FM1_DIAG
static uint32_t diag_t0;
#endif
static char diag_txt[40];

static void diag_num(char *o, uint32_t v)
{
    char b[8];
    int n = 0;
    do { b[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 7);
    while (n) *o++ = b[--n];
    *o = 0;
}

static void diag_tick(void)
{
#if M2K_FM1_DIAG
    uint32_t now = fm1_ticks(), span = now - diag_t0;
    if (span >= 500u * 1000u * FM1_TICKS_PER_US) {      /* every half second */
        uint32_t w = 0;
#if DINGHY_DUAL_CORE
        w = job_ticks;
        job_ticks = 0;
#endif
        char *o = diag_txt;
        *o++ = 'C'; *o++ = 'P'; *o++ = 'U'; *o++ = ' ';
        diag_num(o, load_ticks / (span / 100u)); while (*o) o++;
        *o++ = ' '; *o++ = ' '; *o++ = 'W'; *o++ = ' ';
        diag_num(o, w / (span / 100u)); while (*o) o++;
        *o++ = ' '; *o++ = ' '; *o++ = 'F'; *o++ = ' ';
        diag_num(o, frame_ticks / (span / 100u)); while (*o) o++;
        load_ticks = load_frames = frame_ticks = 0;
        diag_t0 = now;
    }
#endif
    if (ring_short != diag_last_short) { diag_last_short = ring_short; diag_short_ms = fm1_ms; }
}

/* a 3 x 5 font, doubled: digits, the parts' letters and a space (each glyph: 5 rows of 3 bits) */
static const uint16_t *diag_glyph(char c)
{
    static const uint16_t D[10][5] = {{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},
                                      {7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
    static const uint16_t A[][5] = {{7,4,4,4,7} /* C */, {7,5,7,4,4} /* P */, {5,5,5,5,7} /* U */, {5,7,7,5,5} /* M */,
                                    {6,5,5,5,6} /* D */, {6,5,6,5,6} /* B */, {7,4,7,1,7} /* S */, {7,4,6,4,4} /* F */,
                                    {5,5,7,7,5} /* W */};
    static const uint16_t SP[5] = {0, 0, 0, 0, 0};
    if (c >= '0' && c <= '9') return D[c - '0'];
    switch (c) { case 'C': return A[0]; case 'P': return A[1]; case 'U': return A[2]; case 'M': return A[3];
                 case 'D': return A[4]; case 'B': return A[5]; case 'S': return A[6]; case 'F': return A[7];
                 case 'W': return A[8]; }
    return SP;
}

/* ---- the screen: the core's 1-bit frame, ink and paper as RGB565 (the panel takes it big-endian), sent in strips
 * of ROWS lines where they changed since the last sent */
#define ROWS 8u
#define NSTRIP (240u / ROWS)
static uint8_t shown[M2K_FB_BYTES];             /* what the panel shows */
static uint16_t strip[2][240u * ROWS];          /* two: one fills while the other goes out */
static uint8_t todo[NSTRIP], pending, sbuf;
static const uint8_t *frame;

static uint16_t be16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

static uint32_t frame_ms;

static void screen_frame(void)
{
    uint32_t s, i, t0 = fm1_ticks();
    frame_ms = fm1_ms;
    frame = m2k_frame(fm1_ms);
    frame_ticks += fm1_ticks() - t0;
    for (s = 0; s < NSTRIP; s++) {
        const uint8_t *a = frame + s * ROWS * 30u, *b = shown + s * ROWS * 30u;
        for (i = 0; i < ROWS * 30u && a[i] == b[i]; i++)
            ;
        todo[s] = i < ROWS * 30u;
        pending += todo[s];
    }
}

/* a nibble of the frame as its four pixels (ink and paper: pure black and white, the panel's most contrast), so a
 * strip is turned a byte at a time, not a pixel (it runs on the first core, beside the sound) */
static uint16_t nib[16][4];

static void screen_init(void)
{
    uint32_t v, k;
    for (v = 0; v < 16u; v++)
        for (k = 0; k < 4u; k++)
            nib[v][k] = v >> (3u - k) & 1u ? be16(0x0000u) : be16(0xFFFFu);
}

static void screen_send(void)                   /* one strip, if the last has gone out */
{
    uint32_t s, x, y, i;
    if (!pending || !lcd_idle())
        return;
    for (s = 0; s < NSTRIP && !todo[s]; s++)
        ;
    if (s == NSTRIP) { pending = 0; return; }
    lcd_sync();
    uint16_t *px = strip[sbuf];
    const uint8_t *src = frame + s * ROWS * 30u;
    for (i = 0; i < ROWS * 30u; i++) {
        const uint16_t *a = nib[src[i] >> 4], *b = nib[src[i] & 15u];
        uint16_t *o = px + i * 8u;
        o[0] = a[0]; o[1] = a[1]; o[2] = a[2]; o[3] = a[3];
        o[4] = b[0]; o[5] = b[1]; o[6] = b[2]; o[7] = b[3];
    }
    memcpy(shown + s * ROWS * 30u, src, ROWS * 30u);
    if (M2K_FM1_DIAG && s < 2u) {               /* the meter over the first two strips (resent each frame) */
        uint16_t red = be16(RGB(255, 0, 0));
        uint32_t c, gx = 2;
        for (c = 0; diag_txt[c] && gx + 6u < 230u; c++, gx += 8u) {
            const uint16_t *g = diag_glyph(diag_txt[c]);
            for (y = 0; y < ROWS; y++) {
                uint32_t row = (s * ROWS + y) / 2u - 1u;   /* glyph rows from line 2, doubled */
                if (s * ROWS + y < 2u || row >= 5u) continue;
                for (x = 0; x < 6u; x++) if (g[row] >> (2u - x / 2u) & 1u) px[y * 240u + gx + x] = red;
            }
        }
        if (fm1_ms - diag_short_ms < 1000u && diag_last_short)
            for (y = 0; y < ROWS; y++) for (x = 232; x < 240u; x++) px[y * 240u + x] = red;
        shown[s * ROWS * 30u] ^= 0x01u;         /* so the next frame sends this strip again */
    }
    lcd_blit(0, s * ROWS, 240, ROWS, px);
    sbuf ^= 1u;
    todo[s] = 0;
    pending--;
}

/* ---- the lights: the core's 27 keys and 14 buttons, 0..255, on from a quarter up (the panel's LEDs are on / off) */
static void lights(void)
{
    const uint8_t *l = m2k_leds();
    uint32_t i;
    for (i = 0; i < M2K_KEYS; i++) fm1_led_key(KEY_LED(i), l[i] >= 64u);
    for (i = 0; i < 14u; i++) fm1_led_key(i, l[M2K_KEYS + button_of(i)] >= 64u);
}

#include "m2k_link.c"                          /* the store and the Patch Editor's link */

static void app_init(void)
{
#if DINGHY_DUAL_CORE
    if (audio_worker_online) { m2k_par_begin = par_begin; m2k_par_end = par_end; }
#endif
    screen_init();
    m2k_init(fm1_ticks());
    store_load();                               /* the factory's bank stays when there is none */
    lcd_fill(0, 0, 240, 240, 0xFFFFu);
    render_ahead();
    screen_frame();
}

static void app_loop(void)
{
    if (M2K_FM1_DIAG) diag_tick();
    render_ahead();
    take_panel();
    midi_drain();
    render_ahead();
    midi_drain();
    /* a frame once the last has gone out, 30 a second at most (as the browser draws them); the sound first: while
     * the ring is under half full the frame waits for the render to catch up (a frame is drawn whole, the sound not
     * rendered meanwhile), and while the render is busy (frame_gap) they come further apart; never more than 150 ms */
    uint32_t since = fm1_ms - frame_ms;
    if (!pending && since >= frame_gap && (ring_w - ring_r >= RING / 2u || since >= 150u)) {
        lights();
        screen_frame();
    }
    screen_send();
    if (m2k_dirty()) save_due = 1;
    lk_service();
    if (save_due && !m2k_playing() && !lk_open) {   /* SAVE kept it: into the flash once the sequencer stands */
        store_save();
        save_due = 0;
    }
}
