/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K in the browser: the firmware's core, unchanged, with a few exports for the AudioWorklet and the page.
 * web/build.sh compiles it to web/m2k.wasm. */
#include "../firmware/src/m2k_core.c"

#define EXPORT __attribute__((used, visibility("default")))

static int16_t w_buf[2 * 128];
static uint8_t w_inf[32];

EXPORT int16_t *w_audio(void) { return w_buf; }
EXPORT void w_render(int n) { m2k_render(w_buf, n); }
EXPORT const uint8_t *w_frame(uint32_t ms) { return m2k_frame(ms); }
EXPORT const uint8_t *w_leds(void) { return m2k_leds(); }
EXPORT void w_init(uint32_t seed) { m2k_init(seed); }
EXPORT void w_key(int k, int d) { m2k_key(k, d); }
EXPORT void w_button(int b, int d) { m2k_button(b, d); }
EXPORT void w_encoder(int e, int d) { m2k_encoder(e, d); }
EXPORT void w_midi(int s, int a, int b) { m2k_midi(s, a, b); }   /* Web MIDI in: the FM-1's USB */
EXPORT uint8_t *w_store_buf(void) { int m; return m2k_store_buf(&m); }
EXPORT int w_store_max(void) { int m; m2k_store_buf(&m); return m; }
EXPORT int w_store_pack(void) { return m2k_store_pack(); }
EXPORT int w_store_pack_as(int patterns, int projects) { return m2k_store_pack_as(patterns, projects); }   /* the FM-1's: 16, 1 */
EXPORT int w_store_unpack(int n) { return m2k_store_unpack(n); }
EXPORT int w_proj_export(int slot) { return m2k_proj_export(slot); }
EXPORT int w_proj_import(int slot, int n) { return m2k_proj_import(slot, n); }
EXPORT void w_project(int slot, int save) { m2k_project(slot, save); }
EXPORT const char *w_project_name(int slot) { return m2k_project_name(slot); }
EXPORT int w_projects(void) { return M2K_PROJECTS; }
EXPORT int w_dirty(void) { return m2k_dirty(); }
EXPORT void w_setting(int i, int v) { m2k_setting(i, v); }
EXPORT void w_pattern(int n) { m2k_pattern(n); }
EXPORT int w_patterns(void) { return M2K_PATTERNS; }
EXPORT void w_track(int t) { m2k_track(t); }
EXPORT void w_mute(int t) { m2k_mute(t); }
EXPORT void w_tempo(int d) { m2k_tempo(d); }
/* what knob k turns on a page, for machine m: the panel's labels follow the screen */
EXPORT const char *w_knob(int page, int sub, int m, int k)
{
    int code = knob_param(page % PG_COUNT, sub, k & 3);
    if (code == NONE) return "";
    return code < 8 ? MI[m % M_COUNT].syn[code].s4 : pinfo(0, code)->s4;
}

/* every page and subpage the panel edits, for the page's list of every parameter */
EXPORT int w_pages(void) { return PG_COUNT; }
EXPORT int w_subs(int page) { return NSUB[page % PG_COUNT]; }
EXPORT const char *w_page_label(int page, int sub) { static char o[12]; page_label(page % PG_COUNT, sub, o); return o; }
EXPORT int w_kcode(int page, int sub, int k) { return knob_param(page % PG_COUNT, sub, k & 3); }
EXPORT const char *w_kname(int page, int sub, int m, int k)
{
    int code = knob_param(page % PG_COUNT, sub, k & 3);
    if (code == NONE) return "";
    return code < 8 ? MI[m % M_COUNT].syn[code].name : pinfo(0, code)->name;
}

/* the patch editor: the open pattern's tracks read and set as the panel sets them (kept, dirty, saved with the store),
 * each value formatted as the screen shows it, its range, and the Monomachine's CC for it (-1 for MONO2K's own) */
EXPORT int w_cur_pattern(void) { return cur_pat; }
EXPORT int w_machine_of(int t) { return pat[cur_pat].machine[t % M2K_TRACKS]; }
EXPORT const uint8_t *w_kit(int t) { return pat[cur_pat].kit[t % M2K_TRACKS]; }
EXPORT int w_pcount(void) { return P_COUNT; }
EXPORT void w_param(int t, int code, int v)
{
    if (code < 0 || code >= P_COUNT) return;
    t %= M2K_TRACKS;
    pat[cur_pat].kit[t][code] = (uint8_t)CLAMP(v, 0, 255);
    trk[t].eff[code] = pat[cur_pat].kit[t][code];
    dirty = 1;
}
EXPORT void w_set_machine(int t, int m)
{
    t %= M2K_TRACKS;
    if (m < 0 || m >= M_COUNT) return;
    machine_to(t, m);                                    /* as ALGORITHM: an INIT sound takes m's whole INIT */
    dirty = 1;
}
/* in the page's own copy of the core: the machines the edited pattern has, so the labels and the formats follow them */
EXPORT void w_mirror_machine(int t, int m) { if (m >= 0 && m < M_COUNT) pat[cur_pat].machine[t % M2K_TRACKS] = (uint8_t)m; }
EXPORT const char *w_fmt(int t, int code, int v)
{
    static char o[32];
    const char *u = "";
    fmt(pinfo(t % M2K_TRACKS, code), t % M2K_TRACKS, v, o, &u);
    if (*u) { char *e = o; while (*e) e++; *e++ = ' '; s_cpy(e, u); }
    return o;
}
EXPORT int w_kmin(int t, int code) { return kind_min(pinfo(t % M2K_TRACKS, code)->kind); }
EXPORT int w_kmax(int t, int code) { return kind_max(pinfo(t % M2K_TRACKS, code)->kind); }
EXPORT int w_kchoices(int t, int code) { return kind_choices(pinfo(t % M2K_TRACKS, code)->kind); }
EXPORT int w_enc(int i, int n) { return enc(i, n); }
EXPORT int w_choice(int t, int code, int v) { return choice_of(pinfo(t % M2K_TRACKS, code)->kind, v); }
EXPORT int w_cc_of(int code)
{
    for (int cc = 0; cc < 128; cc++) if (mm_cc_param(cc) == code) return cc;
    return -1;
}

/* what the page shows next to the panel */
/* MIDI out: the next message due once the sound reached PLAYED samples (the worklet sends what it rendered) */
static uint8_t w_mo[3];
EXPORT int w_midi_out(uint32_t played) { return m2k_midi_out(w_mo, played); }
EXPORT const uint8_t *w_mout(void) { return w_mo; }
EXPORT uint32_t w_samples(void) { return m2k_samples(); }

EXPORT const uint8_t *w_info(void)
{
    w_inf[0] = ui.trk;
    w_inf[1] = ui.page;
    w_inf[2] = ui.sub;
    w_inf[3] = (uint8_t)cur_pat;
    w_inf[4] = sq.playing;
    w_inf[5] = (uint8_t)(seq_bpm() - 30);
    w_inf[6] = pat[cur_pat].machine[ui.trk];
    w_inf[7] = ui.grid;
    w_inf[8] = sq.rec;
    w_inf[9] = sq.fill;
    w_inf[10] = (uint8_t)play_step();
    w_inf[11] = (uint8_t)seq_len();
    w_inf[12] = sq.queued;
    w_inf[13] = pat[cur_pat].mute;
    w_inf[14] = (uint8_t)ui.oct[ui.trk];     /* the track in view's keyboard octave */
    w_inf[15] = master_vol;
    w_inf[16] = ui.nheld;
    for (int t = 0; t < M2K_TRACKS; t++) w_inf[17 + t] = pat[cur_pat].machine[t];
    w_inf[23] = (uint8_t)m2k_project_current();
    w_inf[24] = (clk_from() & 1) != 0;       /* GLO > MIDI: the clock taken from USB, where the page's MIDI comes in */
    w_inf[25] = store_.set[S_SYNO] >= 64;
    w_inf[26] = store_.set[S_NOTO] >= 64;
    w_inf[27] = ui.pads;                     /* the keys' mode with [7]: KEYS, PADS or GRID, for the page's status line */
    w_inf[28] = ui.latch;                    /* a latched layer: its button + 1 */
    w_inf[29] = sq.ready;                    /* Ready: the first note starts it */
    w_inf[30] = ui.spage;                    /* GRID's page of steps */
    return w_inf;
}

/* the site's machine sheet, in a copy of the core of its own (never the one that plays): the open pattern made INIT and
 * stopped, machine m at its INIT on track 1, in view on SYN (an FX machine on track 2, after a plain SWAV-SAW at its
 * loudest on track 1, as its INIT was voiced). Returns the track; w_hit then trigs it at its home note as a pad does (a
 * step's trig and gate), an FX's source with it */
static int w_aud_t = 0, w_aud_m = 0;
EXPORT int w_audition(int m)
{
    if (m < 0 || m >= M_COUNT) return -1;
    m2k_pattern_t *p = PAT(cur_pat);
    seq_play(0);
    pattern_init_blank(p);
    int fx = m == M_FXC || m == M_FXR || m == M_FXD, t = fx ? 1 : 0;
    if (fx) {
        kit_default(p, 0, M_SAW);
        feel_kit_v15(p->kit[0]);
        p->kit[0][P_LEVL] = 127;
        p->kit[0][P_VOL] = 127;
    }
    kit_init(p, t, m);
    track_home(t, mach_home(m));
    if (fx) track_home(0, mach_home(m));
    lockmap_build();
    for (int i = 0; i < M2K_TRACKS; i++) m_cpy(trk[i].eff, p->kit[i], P_COUNT);
    track_to(t);
    ui.page = PG_SYN;
    ui.sub = 0;
    ui.focus_t = 0;
    w_aud_t = t;
    w_aud_m = m;
    return t;
}
EXPORT int w_home_note(int m) { return mach_home(m % M_COUNT); }
EXPORT void w_hit(void)
{
    int n = mach_home(w_aud_m);
    if (w_aud_t) pad_hit(0, n);
    pad_hit(w_aud_t, n);
}

/* the machines, for the page's machine sheet */
EXPORT const uint8_t *w_mark(int m) { return MARK_BITS[m % ICON_COUNT]; }
EXPORT const uint8_t *w_icon(int m, int f) { return ICON_BITS[m % ICON_COUNT][f % ICON_FRAMES]; }
EXPORT int w_machines(void) { return M_COUNT; }
EXPORT int w_morder(int i) { return M_ORDER[(unsigned)i % M_COUNT]; }   /* the machine at place i, as ALGORITHM steps */
EXPORT const char *w_mi(int m, int field)
{
    const mi_t *mi = &MI[m % M_COUNT];
    switch (field) {
    case 0: return mi->name;
    case 1: return mi->badge;
    case 2: return mi->style;
    case 3: return mi->cls;
    default: return "";
    }
}
EXPORT int w_stat(int m, int i) { return MI[m % M_COUNT].stat[i % 3]; }
EXPORT const char *w_syn(int m, int i, int longname) { const pi_t *p = &MI[m % M_COUNT].syn[i & 7]; return longname ? p->name : p->s4; }
EXPORT const char *w_pat_name(int i) { return pat[i % M2K_PATTERNS].name; }
