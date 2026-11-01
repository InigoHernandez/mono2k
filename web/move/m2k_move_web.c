/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move, in the browser: the Move's build (eight tracks, the controller, the 128 x 64 screen) through the
 * glue the Move's own dsp.so shares (move/m2k_move_glue.c), with a few exports for the site's drawing of the Move
 * (gear.js) and its AudioWorklet (gear-worklet.js). web/build.sh compiles it, with -DM2K_TRACKS=8, to
 * web/move/m2k_move.wasm (no JS glue, no imports: the worklet instantiates it).
 *
 * What differs from the Move: no Move here, so MONO2K plays on its own clock (INT: Play just plays, the tempo knob the
 * pattern's own, nothing asked of anyone) and nothing is saved (each visit opens the factory's bank A). Everything else
 * is the Move's: the same controller, the same screen, the same LEDs, the same samples. */
#include "../../firmware/src/m2k_core.c"
#include "../../move/m2k_move_glue.c"

#define EXPORT __attribute__((used, visibility("default")))

static float mv_host_bpm(void) { return 0.0f; }             /* (no Move: no tempo of its own) */
static void mv_save_request(int reload)
{
    if (reload) toast("NOT SAVED", "A BROWSER KEEPS NOTHING");   /* (Shift + Rec: said; the core's own rule: silent) */
}

static int16_t w_buf[2 * 128];
static mv_view_t w_view;
static uint8_t w_fb[MS_BYTES];
static uint8_t w_st[48];

/* the factory, MONO2K's own clock, the page HOME */
EXPORT void w_init(uint32_t seed)
{
    mv_begin(seed);
    ck.sync = MV_SYNC_INT;             /* (Move's clock is the rule on the Move; here there is none to wait for) */
    mv_ready(-1);
    mv.live = 1;
}

/* the sound: N frames (16..128, a multiple of 16) into w_audio, stereo int16 */
EXPORT int16_t *w_audio(void) { return w_buf; }
EXPORT void w_render(int n) { mv_block(w_buf, n > 128 ? 128 : n); }

/* the controls, into the queue the next block drains in order, as the Move's on_midi and ui.js put them there */
EXPORT void w_step(int k, int on) { if (k >= 0 && k < 16) ev_push(EV_STEP, k, on != 0); }
EXPORT void w_pad(int i, int vel) { if (i >= 0 && i < 32) ev_push(EV_PAD, i, vel > 0 ? CLAMP(vel, 1, 127) : -1); }
EXPORT void w_knob(int k, int d) { if (k >= 0 && k < 8 && d) ev_push(EV_KNOB, k, d); }
EXPORT void w_touch(int k, int on) { if (k >= 0 && k < 8) ev_push(EV_TOUCH, k, on != 0); }
EXPORT void w_jog(int d) { if (d) ev_push(EV_JOG, 0, d); }
EXPORT void w_button(int cc, int v)
{
    if (cc == 51) {                    /* Back: the host's on the Move (it parks MONO2K there, playing on) */
        if (v) toast("BACK", "ON A MOVE: PARKS MONO2K");
        return;
    }
    if (cc >= 0 && cc < 128) ev_push(EV_BTN, cc, v);
}

/* the frame: the page state built now (between blocks, as the Move's audio thread builds it) and the screen drawn from
 * it; 128 x 64, a row 16 bytes, MSB first, 1 lit */
EXPORT const uint8_t *w_frame(void)
{
    mv_view_build(&w_view);
    m2k_screen_draw(&w_view.scr, w_fb);
    return w_fb;
}

/* the LEDs of the frame last built: 16 steps, 32 pads, 4 track buttons (palette indices) and MV_NBTN buttons
 * (MV_BTN_CC's order: palette for Rec, the white buttons' brightness 0..127) */
EXPORT const uint8_t *w_leds(void) { return (const uint8_t *)&w_view.led; }
EXPORT int w_nleds(void) { return (int)sizeof w_view.led; }

/* what the drawing shows beside the LEDs, of the frame last built:
 *   0 playing  1 rec  2 Shift (as the controller has it)  3 the page (MS_*)  4 its family (MS_FAM_*)  5 the track
 *   6 the knob shown large + 1 (0 none)  7 PERFORM's keys on  8 the octave + 3  9 what is over the page (MV_OV_*)
 *   10 steps held  11 the tempo's source (MV_SYNC_*)  12-13 the tempo, tenths (LE)  14 the tempo is shown large
 *   15 PERFORM's moves' page  16-23 how each knob turns (MV_KK_*)  24-31 each knob's flags (MV_K_*)  32-47 each knob's
 *   value (LE, 0..127; the tempo 0..255) */
EXPORT const uint8_t *w_state(void)
{
    const mv_view_t *v = &w_view;
    m_set(w_st, 0, sizeof w_st);
    w_st[0] = v->playing;
    w_st[1] = v->rec;
    w_st[2] = v->shift;
    w_st[3] = v->page;
    w_st[4] = (uint8_t)ms_family(v->page);
    w_st[5] = v->trk;
    w_st[6] = (uint8_t)(v->focus + 1);
    w_st[7] = v->perf;
    w_st[8] = (uint8_t)(v->oct + 3);
    w_st[9] = v->overlay;
    w_st[10] = v->held;
    w_st[11] = v->sync;
    w_st[12] = (uint8_t)v->bpm10;
    w_st[13] = (uint8_t)(v->bpm10 >> 8);
    w_st[14] = v->tempo_focus;
    w_st[15] = v->perf_pg;
    for (int k = 0; k < 8; k++) {
        w_st[16 + k] = v->kkind[k];
        w_st[24 + k] = v->knob[k].flags;
        w_st[32 + 2 * k] = (uint8_t)v->knob[k].raw;
        w_st[33 + 2 * k] = (uint8_t)(v->knob[k].raw >> 8);
    }
    return w_st;
}
