/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the Move: the controller. The Move's controls in, the core's edits out, and the page state for the screen
 * (m2k_move_view.h). Included by m2k_move.c after the core (one unit, as the core is), so it calls the FM-1 UI's own
 * helpers (m2k_ui.c: set_machine, select_pattern, mute_flip, track_to, held_note, the catalogue's cat_*, undo_mark /
 * undo_do, step_remove...) and does to the core what the FM-1's controls do, on the Move's layout (the owner's,
 * 2026-11-01: every page one press away, the sequencer always on the steps, the keyboard at the bottom):
 *   pads 16..31    the pages, a pad each: the top row the sound of the track in view (SYN AMP FILT EFX DSTR LFO1-3),
 *                  the second the rest (HOME TRK PAT GRID MIX GLO, then SOUNDS and PERFORM): MS_PAD_PAGE
 *   pads 0..15     the keyboard, a two-row piano: the white keys C..C on the bottom row, the black over them; with
 *                  PERFORM on, its effects (bank A's on the bottom row, bank B's on the top; Up / Down their 9-11)
 *   8 knobs        the page's eight (m2k_screen_param); a held step's knobs lock it; Shift + knob 1 the tempo on
 *                  every page (Move's and MONO2K's under SYNC: m2k_move.c tq_ask)
 *   steps          the sequencer on every page: trigs (a press places one, a quick tap on one takes it away, held its
 *                  locks); Shift + steps the bank's patterns; Mute + steps the tracks' mutes
 *   jog            the lists: the catalogue in SOUNDS, else the patterns (a click loads one); a held step's note;
 *                  Shift + jog the machine
 *   track buttons  tracks 1-4, Shift 5-8; Mute + a button its mute
 *   Rec, Copy, Delete, Undo, Loop, arrows: README's table.
 * Everything here runs on the audio thread, between blocks (m2k_move.c drains the events before each render). */

/* the glue's (m2k_move.c): the clock, the saver */
static void mv_play_press(void);
static int mv_following(void);
static int mv_sync(void);
static uint32_t mv_bpm10(void);
static void mv_save_request(int why);
static void tq_ask(int d10, int coarse);
static void tq_cancel(void);

#define MV_TAP_MS 300                  /* a step tapped quicker takes its trig away */
#define MV_FOCUS_MS 1200               /* a knob turned shows large so long */
#define MV_TOAST_MS 1600
#define MV_LIST_MS 3000                /* the patterns' list stays so long after its last turn */

enum { SRC_NONE, SRC_STEP, SRC_TRK, SRC_PAT };   /* what Copy held picked first */

static struct {
    uint8_t page, prev, cat_from;      /* the page in view (MS_*); the last plain page; the one SOUNDS was opened on */
    uint8_t shift, mute, copy, del, left, right;
    uint8_t used;                      /* the modifier held (Mute, Copy, Delete, Shift) met another control */
    uint8_t src, src_a;                /* Copy's source (SRC_*) and which */
    uint8_t bank;
    int8_t touch;                      /* the knob touched (-1 none) */
    int8_t focus;
    uint8_t tempo_focus;               /* what is shown large is the tempo (Shift + knob 1) */
    uint32_t focus_t;
    uint8_t perf, perf_pg;             /* PERFORM's effects on the keyboard; their page (each bank's 1-8, its 9-11) */
    uint8_t plist, pat_sel;            /* the patterns' list (the jog's), its cursor */
    uint32_t plist_t;
    uint8_t sdown[16], sstep[16];      /* each step key: held, and the step it holds */
    uint32_t st0[16];
    uint8_t pkind[32], parg[32], ptrk[32];   /* each pad: 1 a note (arg), 2 a PERFORM effect (arg); on which track */
} mc;

static void mv_ctl_init(void)
{
    m_set(&mc, 0, sizeof mc);
    mc.touch = -1;
    mc.focus = -1;
    mc.page = mc.prev = mc.cat_from = MS_HOME;
    mc.bank = (uint8_t)(cur_pat / 16);
    ui.grid = 1;                       /* the steps are always steps (GRID's), the knobs lock what is held */
    ui.follow = 1;
}

/* ------------------------------------------------------------------------------------------------ the pages */

static void focus_off(void) { mc.focus_t = 0; mc.focus = -1; mc.tempo_focus = 0; }

/* a page of knobs that is neither function's: where SOUNDS and PERFORM go back to */
static int page_plain(int pg) { return pg < MS_PAGES && pg != MS_PERF; }

/* the catalogue closed (kept, put back, or left): the page it was opened from, PERFORM's only while its keys are on */
static void cat_left(void)
{
    if (mc.page != MS_CAT) return;
    mc.page = page_plain(mc.cat_from) || (mc.cat_from == MS_PERF && mc.perf) ? mc.cat_from : mc.prev;
}

/* a page pad (16 + j): its page; SOUNDS opens the catalogue (again: the track's own sound back, the page before);
 * PERFORM puts its effects on the keyboard (again: the keyboard back) */
static void page_pad(int j)
{
    const int pg = MS_PAD_PAGE[j & 15];
    mc.plist = 0;
    focus_off();
    if (pg == MS_CAT) {
        if (cat.open) { cat_back(); cat_left(); return; }
        mc.cat_from = mc.page;
        if (page_plain(mc.page)) mc.prev = mc.page;
        cat_open();
        mc.page = MS_CAT;
        return;
    }
    if (cat.open) cat_back();          /* (leaving SOUNDS for a page: the track's own sound, as the FM-1's) */
    if (pg == MS_PERF) {
        if (mc.perf) { mc.perf = 0; if (!page_plain(mc.page)) mc.page = mc.prev; return; }
        if (page_plain(mc.page)) mc.prev = mc.page;
        mc.perf = 1;
        mc.perf_pg = 0;
        mc.page = MS_PERF;
        return;
    }
    mc.page = (uint8_t)pg;
}

/* ----------------------------------------------------------------------------------------------- the knobs */

static int mv_code(int k) { return m2k_screen_param(mc.page, ui.trk, k); }
static int mv_ktrk(int k) { return m2k_screen_knob_track(mc.page, ui.trk, k); }

/* what a knob shows: a setting, a global, a held step's lock (the track in view's), the kit */
static int mv_value(int code, int t)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    if (code == NONE) return 0;
    if (code >= 0xC0) return store_.set[code & 0x3F];
    if (code >= 0x80) return p->glob[code & 0x7F];
    if (ui.nheld && t == ui.trk) {
        int i = lock_find(ui.trk, ui.held[0], code);
        if (i >= 0) return p->lock[i].val;
    }
    return p->kit[t][code];
}

static int mv_locked(int code, int t) { return code < 0x80 && t == ui.trk && ui.nheld && lock_find(ui.trk, ui.held[0], code) >= 0; }

static void mv_focus(int k) { mc.focus = (int8_t)k; mc.focus_t = ui.now | 1u; mc.tempo_focus = 0; }

/* the catalogue's SLOT and CAT (edit()'s SETTING(18) and (21)): each sound landed on heard on the track */
static void mv_cat_slot(int d)
{
    int n = 0, at = snd_list(ui.snd_sel, &n);
    at = CLAMP(at + d, 0, n - 1);
    ui.snd_sel = (uint8_t)snd_nth(at);
    if (cat.open && ui.snd_sel != cat.heard) cat_hear(ui.snd_sel);
}

static void mv_cat_cat(int dir)
{
    ui.snd_cat = (uint8_t)CLAMP(ui.snd_cat + dir, 0, CAT_COUNT - 1);
    int n = 0, at = snd_list(ui.snd_sel, &n);
    if (!snd_shown(ui.snd_sel)) ui.snd_sel = (uint8_t)snd_nth(at < n ? at : 0);
    if (cat.open && ui.snd_sel != cat.heard) cat_hear(ui.snd_sel);
}

static int sgn(int d) { return d > 0 ? 1 : d < 0 ? -1 : 0; }

static void mv_knob(int k, int d);

/* a knob that is the tempo: Shift + knob 1 on every page, and the page's own BPM (HOME's knob 3, PAT's knob 1) */
static int mv_is_tempo(int k) { return (k == 0 && mc.shift) || (!cat.open && mv_code(k) == GL(G_TMPO)); }

/* the tempo turned: d10 tenths of BPM (coarse: whole BPM, landing on one), ui.js's curve for a tempo knob (MV_KK_TEMPO;
 * a "K" turn on it comes here as whole BPM). Shown large, as a touched value is. Under SYNC it is Move's and MONO2K's:
 * MONO2K at it at once, Move asked (tq_ask); Delete + it, Move's tempo again. INT (Move sent no clock): the pattern's
 * own, whole BPM as on the FM-1 (the tenths add up), Delete + it back to its start */
static void mv_tempo_knob(int k, int d10, int coarse)
{
    if (k < 0 || k > 7 || !d10) return;
    if (!mv_is_tempo(k)) { mv_knob(k, sgn(d10)); return; }   /* (ui.js a frame behind a page change) */
    mc.plist = 0;
    mv_focus(k);
    if (k == 0 && mc.shift) { mc.used = 1; mc.tempo_focus = 1; }
    m2k_pattern_t *p = PAT(cur_pat);
    if (mc.del) {
        mc.used = 1;
        if (ck.sync == MV_SYNC_MOVE) {
            if (tq.state != MV_TQ_NONE) { tq_cancel(); toast("TEMPO", "MOVE'S AGAIN"); }
            return;
        }
        undo_mark(N_PAT, 21);
        pat_init(&pat_tmp, "X");
        p->glob[G_TMPO] = pat_tmp.glob[G_TMPO];
        tq.acc10 = 0;
        toast("BPM", "BACK TO ITS START");
        return;
    }
    if (ck.sync == MV_SYNC_MOVE) { tq_ask(d10, coarse); return; }
    int whole;
    if (coarse) { tq.acc10 = 0; whole = d10 / 10; }
    else { tq.acc10 += d10; whole = tq.acc10 / 10; tq.acc10 -= whole * 10; }
    p->glob[G_TMPO] = (uint8_t)CLAMP(p->glob[G_TMPO] + whole, 0, 255);
}

/* knob k turned by d steps (ui.js's curve: a range's values, a short list's options): as the FM-1's edit() does to the
 * code the knob holds, d added once (no acceleration of its own here) */
static void mv_knob(int k, int d)
{
    if (k < 0 || k > 7 || !d) return;
    if (mv_is_tempo(k)) { mv_tempo_knob(k, d * 10, 1); return; }   /* (as whole BPM) */
    if (cat.open) {                    /* the catalogue: KNOB 1 SLOT, KNOB 2 CAT */
        if (k == 0) mv_cat_slot(d);
        else if (k == 1) mv_cat_cat(sgn(d));
        return;
    }
    mc.plist = 0;
    const int code = mv_code(k), t = mv_ktrk(k);
    if (code == NONE) return;
    m2k_pattern_t *p = PAT(cur_pat);
    const pi_t *pi = pinfo(t, code);
    if (pi->s4[0] == '-') return;
    mv_focus(k);
    if (mc.del) {                      /* Delete held (the FM-1's OCT-): a held step's lock away, else the value home */
        mc.used = 1;
        if (code >= 0xC0) return;      /* (a setting has no start of its own) */
        if (ui.nheld && code < 0x80 && t == ui.trk) {
            undo_mark(N_STEP, 20);
            for (int i = 0; i < ui.nheld; i++) {
                int li = lock_find(ui.trk, ui.held[i], code);
                if (li >= 0) p->lock[li].trk = 0xFF;
            }
            lockmap_build();
            trk[ui.trk].eff[code] = p->kit[ui.trk][code];
            ui.held_edit = 1;
            toast(pi->s4, "LOCK REMOVED");
        } else {
            undo_mark(N_PAT, 21);
            if (code >= 0x80) { pat_init(&pat_tmp, "X"); p->glob[code & 0x7F] = pat_tmp.glob[code & 0x7F]; }
            else {
                kit_init(&pat_tmp, t, p->machine[t]);
                p->kit[t][code] = pat_tmp.kit[t][code];
                trk[t].eff[code] = p->kit[t][code];
            }
            toast(pi->s4, "BACK TO ITS START");
        }
        return;
    }
    int v = mv_value(code, t), n = kind_choices(pi->kind), nv;
    if (n) {                           /* a list: d options (ui.js turns a short one's slower: MV_KK_CHOICE) */
        int c = CLAMP(choice_of(pi->kind, v) + d, 0, n - 1);
        nv = pi->kind == K_TRK ? trck_enc(c) : enc(c, n);
    } else nv = CLAMP(v + d, kind_min(pi->kind), kind_max(pi->kind));
    if (code >= 0xC0) m2k_setting(code & 0x3F, nv);   /* (kept with the store: saved once it stands) */
    else if (code >= 0x80) p->glob[code & 0x7F] = (uint8_t)nv;
    else if (ui.nheld && t == ui.trk) {   /* steps held: their locks */
        for (int i = 0; i < ui.nheld; i++) lock_set(ui.trk, ui.held[i], code, nv);
        trk[ui.trk].eff[code] = (uint8_t)nv;
        ui.held_edit = 1;
    } else if (sq.rec && sq.playing && t == ui.trk) {   /* REC playing: the step playing's lock, on a trig */
        int s = track_pos(ui.trk);
        if (p->step[ui.trk][s].flags & ST_TRIG) lock_set(ui.trk, s, code, nv);
        else p->kit[ui.trk][code] = (uint8_t)nv;
        trk[ui.trk].eff[code] = (uint8_t)nv;
    } else {
        p->kit[t][code] = (uint8_t)nv;
        trk[t].eff[code] = (uint8_t)nv;
    }
}

static void mv_touch(int k, int on)
{
    if (k < 0 || k > 7) return;
    if (on) { mc.touch = (int8_t)k; mv_focus(k); mc.tempo_focus = (uint8_t)(k == 0 && mc.shift); mc.plist = 0; }
    else if (mc.touch == k) { mc.touch = -1; mc.focus_t = ui.now | 1u; }   /* (shown a moment more) */
}

/* -------------------------------------------------------------------------------------------- the steps */

static void held_add(int s, int added)
{
    if (ui.nheld >= 8) return;
    if (!ui.nheld) ui.held_edit = 0;
    ui.held[ui.nheld] = (uint8_t)s;
    ui.held_added[ui.nheld] = (uint8_t)added;
    ui.nheld++;
}

/* one step to the clipboard (the FM-1's N_STEP copy of the steps held, here one or those held) */
static void steps_copy(const uint8_t *ss, int n)
{
    m2k_pattern_t *p = PAT(cur_pat);
    int first = ss[0];
    for (int i = 0; i < n; i++) if (ss[i] < first) first = ss[i];
    step_clip.n = 0;
    step_clip.nlk = 0;
    for (int i = 0; i < n && i < 8; i++) {
        int s = ss[i];
        step_clip.off[step_clip.n] = (uint8_t)(s - first);
        step_clip.st[step_clip.n++] = p->step[ui.trk][s];
        for (int k = 0; k < M2K_LOCKS && step_clip.nlk < M2K_LOCKS; k++)
            if (p->lock[k].trk == ui.trk && p->lock[k].step == s) {
                step_clip.lk[step_clip.nlk] = p->lock[k];
                step_clip.lk[step_clip.nlk++].step = (uint8_t)(s - first);
            }
    }
    step_clip.ok = 1;
}

/* the clipboard's steps from step s on, round the track's length (undoable) */
static void steps_paste(int s)
{
    m2k_pattern_t *p = PAT(cur_pat);
    if (!step_clip.ok) return;
    undo_mark(N_STEP, V_PASTE);
    int len = tlen_of(ui.trk), lost = 0;
    for (int i = 0; i < step_clip.n; i++) {
        int d = (s + step_clip.off[i]) % len;
        p->step[ui.trk][d] = step_clip.st[i];
        lock_clear_step(ui.trk, d);
    }
    for (int k = 0; k < step_clip.nlk; k++)
        lost |= !lock_set(ui.trk, (s + step_clip.lk[k].step) % len, step_clip.lk[k].param, step_clip.lk[k].val);
    lockmap_build();
    toast(lost ? "LOCKS FULL" : "PASTE", lost ? "SOME LOCKS LOST" : "UNDO TAKES IT BACK");
}

/* a pattern: Shift + step, or the patterns' list clicked. Picked (at the bar's end while playing, the FM-1's PRESET;
 * again before the bar: now), copied and pasted with Copy held, its steps cleared with Delete held */
static void pattern_key(int n)
{
    char a[16], o[24], *e;
    e = s_pat(a, n);
    if (mc.copy) {
        mc.used = 1;
        if (mc.src != SRC_PAT) { mc.src = SRC_PAT; mc.src_a = (uint8_t)n; toast("COPY", a); return; }
        if (n == mc.src_a) return;
        if (n == cur_pat) undo_mark(N_PAT, V_PASTE);
        m_cpy(PAT(n), PAT(mc.src_a), sizeof(m2k_pattern_t));
        if (n == cur_pat) work_reset_current();
        s_cpy(s_cpy(o, "INTO "), a);
        e = s_cpy(a, "PASTE ");
        s_pat(e, mc.src_a);
        toast(a, o);
        return;
    }
    if (mc.del) {
        mc.used = 1;
        if (n == cur_pat) undo_mark(N_PAT, V_CLEAR);
        steps_clear(PAT(n), -1);
        if (n == cur_pat) lockmap_build();
        s_cpy(e, " CLEARED");
        toast(a, n == cur_pat ? "UNDO TAKES IT BACK" : "ITS STEPS");
        return;
    }
    if (cat.open) { cat_back(); cat_left(); }
    if (sq.playing && sq.queued == n && n != cur_pat) {
        sq.jump = 1;
        s_cpy(e, " NOW");
        toast(a, 0);
        return;
    }
    select_pattern(n);
    if (!s_is(ui.toast, "PATTERNS FULL")) ui.toast_t = 0;   /* (the header names it, the one queued framed: no message) */
}

static void mv_step(int k, int down)
{
    if (k < 0 || k > 15) return;
    m2k_pattern_t *p = PAT(cur_pat);
    if (!down) {                       /* (GRID's: a quick tap on a trig takes it away; held longer it was looked at) */
        if (!mc.sdown[k]) return;
        mc.sdown[k] = 0;
        int s = mc.sstep[k], added = 1, was = 0;
        for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == s) { added = ui.held_added[i]; was = 1; }
        held_remove(s);
        if (was && !added && !ui.held_edit && ui.now - mc.st0[k] < MV_TAP_MS) step_remove(ui.trk, s);
        return;
    }
    if (mc.shift) { mc.used = 1; pattern_key(mc.bank * 16 + k); return; }
    if (mc.mute) { mc.used = 1; if (k < M2K_TRACKS) mute_flip(k); return; }
    const int s = ui.spage * 16 + k;
    if (s >= tlen_of(ui.trk)) return;  /* (past the track's end: nothing placed there unheard) */
    m2k_step_t *st = &p->step[ui.trk][s];
    char o[12], *e;
    if (mc.del) {                      /* Delete + a step: its trig away, with its locks */
        mc.used = 1;
        if (!(st->flags & ST_TRIG)) return;
        undo_mark(N_STEP, V_CLEAR);
        step_remove(ui.trk, s);
        e = s_cpy(o, "STEP ");
        s_int(e, s + 1);
        toast(o, "REMOVED");
        return;
    }
    if (mc.copy) {                     /* Copy + a step: the first is copied, the next ones take it */
        mc.used = 1;
        if (mc.src != SRC_STEP) {
            uint8_t one = (uint8_t)s;
            steps_copy(&one, 1);
            mc.src = SRC_STEP;
            e = s_cpy(o, "STEP ");
            s_int(e, s + 1);
            toast("COPY", o);
        } else steps_paste(s);
        return;
    }
    if (ui.nheld >= 8) return;
    int added = 0;
    if (!(st->flags & ST_TRIG)) {
        st->flags = ST_TRIG;
        st->note = ui.note[ui.trk];
        added = 1;
        step_pop(ui.trk, s);
    }
    held_add(s, added);
    mc.sdown[k] = 1;
    mc.sstep[k] = (uint8_t)s;
    mc.st0[k] = ui.now;
    focus_off();
}

/* --------------------------------------------------------------------------------------------- the tracks */

static void track_key(int b)
{
    const int t = mc.shift ? b + 4 : b;
    if (mc.shift) mc.used = 1;
    if (t >= M2K_TRACKS) return;
    m2k_pattern_t *p = PAT(cur_pat);
    char o[24], a[8] = {'T', (char)('1' + t), 0};
    if (mc.mute) { mc.used = 1; mute_flip(t); return; }
    if (mc.del) {                      /* Delete + a track: its steps cleared */
        mc.used = 1;
        undo_mark(N_TRK, V_CLEAR);
        steps_clear(p, t);
        lockmap_build();
        s_cpy(s_cpy(o, a), " STEPS CLEARED");
        toast(o, "UNDO TAKES IT BACK");
        return;
    }
    if (mc.copy) {                     /* Copy + a track: the first is copied, the next takes its steps and sound */
        mc.used = 1;
        if (mc.src != SRC_TRK) {
            mc.src = SRC_TRK;
            mc.src_a = (uint8_t)t;
            s_cpy(s_cpy(o, a), " STEPS, SOUND");
            toast("COPY", o);
            return;
        }
        const int f = mc.src_a;
        if (f == t) return;
        undo_mark(N_TRK, V_PASTE);
        m_cpy(p->step[t], p->step[f], sizeof p->step[t]);
        for (int i = 0; i < M2K_LOCKS; i++) if (p->lock[i].trk == t) p->lock[i].trk = 0xFF;
        int lost = 0;
        for (int i = 0; i < M2K_LOCKS; i++)
            if (p->lock[i].trk == f) lost |= !lock_set(t, p->lock[i].step, p->lock[i].param, p->lock[i].val);
        p->machine[t] = p->machine[f];
        m_cpy(p->kit[t], p->kit[f], P_COUNT);
        for (int l = 0; l < 3; l++)        /* (its LFOs on itself, as LOAD sets them, where they were on the source) */
            if (trck_of(p->kit[f][P_LFO + 8 * l + L_TRCK]) == f) p->kit[t][P_LFO + 8 * l + L_TRCK] = trck_enc(t);
        m_cpy(trk[t].eff, p->kit[t], P_COUNT);
        lockmap_build();
        o[0] = 'T'; o[1] = (char)('1' + f); o[2] = ' '; s_cpy(s_cpy(o + 3, "INTO "), a);
        toast(lost ? "LOCKS FULL" : "PASTE", lost ? "SOME LOCKS LOST" : o);
        if (t == ui.trk) syn_sub_check();
        return;
    }
    if (cat.open) { cat_back(); cat_left(); }
    if (t != ui.trk) {                 /* (steps held were the last track's: let go, so no knob locks the new one's) */
        ui.nheld = 0;
        m_set(mc.sdown, 0, sizeof mc.sdown);
    }
    track_to(t);
}

/* ----------------------------------------------------------------------------------------------- the pads */

/* the keyboard: a two-row piano on the bottom rows, as an Elektron keyboard lays its trig keys. The bottom row the
 * white keys C D E F G A B C of the track's octave, the row over it the black keys over theirs (C# D# - F# G# A# - -) */
static const int8_t KB_WHITE[8] = {0, 2, 4, 5, 7, 9, 11, 12};
static const int8_t KB_BLACK[8] = {1, 3, -1, 6, 8, 10, -1, -1};
static int key_note(int i)
{
    if (i < 0 || i >= 16) return -1;
    const int o = i < 8 ? KB_WHITE[i] : KB_BLACK[i - 8];
    return o < 0 ? -1 : CLAMP(60 + 12 * ui.oct[ui.trk] + o, 0, 127);
}

/* PERFORM on the keyboard: bank A's eleven on the bottom row, bank B's on the top, eight at a time (Up: their 9-11) */
static int perf_of_key(int i)
{
    const int k = (i & 7) + mc.perf_pg * 8;
    return i >= 0 && i < 16 && k < PF_BANK ? (i >> 3) * PF_BANK + k : -1;
}

static void mv_pad(int i, int vel, int down)
{
    if (i < 0 || i >= 32) return;
    if (!down) {
        int kind = mc.pkind[i], a = mc.parg[i], t = mc.ptrk[i];
        mc.pkind[i] = 0;
        if (kind == 2 && perf.on[a]) bus_perform(a, 0);
        else if (kind == 1)
            for (int k = 0; k < sq.nkeys[t]; k++)
                if (sq.keys[t][k] == a) { live_off_t(t, a); break; }   /* (a track left meanwhile let go of it) */
        return;
    }
    if (mc.pkind[i]) mv_pad(i, 0, 0); /* (a press without its release: let go first) */
    if (i >= MS_PAD0) { page_pad(i - MS_PAD0); return; }
    if (mc.perf) {                     /* PERFORM: held, its effect plays */
        int e = perf_of_key(i);
        if (e < 0) return;
        if (bus_perform(e, 1)) { mc.pkind[i] = 2; mc.parg[i] = (uint8_t)e; }
        else toast(PF_NAME[e], "NO DELAY FREE");
        return;
    }
    const int t = ui.trk, n = key_note(i);
    if (n < 0) return;
    sq.ktrk = (uint8_t)t;
    live_on_t(t, n, vel);
    ui.note[t] = (uint8_t)n;           /* (new trigs take the last note played, as from the FM-1's keys) */
    mc.pkind[i] = 1;
    mc.parg[i] = (uint8_t)n;
    mc.ptrk[i] = (uint8_t)t;
}

/* ----------------------------------------------------------------------------------- the jog and the buttons */

/* the jog: SOUNDS' list; a held step's note (Shift: its condition); Shift + jog the machine; else the patterns' list,
 * opened by the turn at the pattern open, the cursor moved by it (nothing loads until a click) */
static void mv_jog(int d)
{
    if (!d) return;
    if (cat.open) { mv_cat_slot(sgn(d)); return; }
    if (ui.nheld) {
        if (mc.shift) { mc.used = 1; held_step_edit(sgn(d), 0); }
        else held_note(d);
        return;
    }
    if (mc.shift) {                    /* the machine (the FM-1's ALGO) */
        mc.used = 1;
        const m2k_pattern_t *p = PAT(cur_pat);
        set_machine(M_ORDER[(m_place(p->machine[ui.trk]) + M_COUNT + sgn(d)) % M_COUNT]);
        if (mc.page == MS_SYN) ui.toast_t = 0;   /* (SYN names it large itself) */
        return;
    }
    if (!mc.plist) { mc.plist = 1; mc.pat_sel = (uint8_t)cur_pat; }
    mc.pat_sel = (uint8_t)CLAMP(mc.pat_sel + d, 0, M2K_PATTERNS - 1);
    mc.plist_t = ui.now | 1u;
    focus_off();
}

/* a click: SOUNDS keeps the sound shown; the patterns' list loads the one under the cursor; else it opens the list */
static void jog_click(void)
{
    if (cat.open) { cat_keep(); cat_left(); return; }
    if (mc.plist) { mc.plist = 0; pattern_key(mc.pat_sel); return; }
    mc.plist = 1;
    mc.pat_sel = (uint8_t)cur_pat;
    mc.plist_t = ui.now | 1u;
    focus_off();
}

static void bars(int d)
{
    if (mc.left && mc.right) {         /* both: the bars follow the playhead again */
        ui.follow = 1;
        toast("BARS", "FOLLOW THE PLAY");
        return;
    }
    int last = (view_len() - 1) / 16;
    ui.spage = (uint8_t)CLAMP(ui.spage + d, 0, last);
    ui.follow = 0;
}

static void mv_button(int cc, int v)
{
    const int down = v > 0;
    switch (cc) {
    case 49: mc.shift = (uint8_t)down; if (down) mc.used = 0; return;          /* Shift */
    case 88:                                                                   /* Mute: held a layer, a tap the track's */
        if (down) { mc.mute = 1; mc.used = 0; return; }
        if (mc.mute && !mc.used) mute_flip(ui.trk);
        mc.mute = 0;
        return;
    case 60:                                                                   /* Copy */
        if (down) {
            mc.copy = 1; mc.used = 0; mc.src = SRC_NONE;
            if (ui.nheld) { steps_copy(ui.held, ui.nheld); mc.src = SRC_STEP; mc.used = 1; toast("COPY", "THE STEPS HELD"); }
            return;
        }
        if (mc.copy && !mc.used) toast("COPY: HOLD IT", "THEN SOURCE, TARGET");
        mc.copy = 0; mc.src = SRC_NONE;
        return;
    case 119:                                                                  /* Delete */
        if (down) {
            mc.del = 1; mc.used = 0;
            if (ui.nheld) {                                                    /* the held steps' locks */
                undo_mark(N_STEP, V_CLEAR);
                for (int i = 0; i < ui.nheld; i++) lock_clear_step(ui.trk, ui.held[i]);
                lockmap_build();
                ui.held_edit = 1;
                mc.used = 1;
                toast("LOCKS CLEARED", "UNDO TAKES THEM BACK");
            }
            return;
        }
        if (mc.del && !mc.used) toast("DELETE: HOLD IT", "AND A STEP OR TRACK");
        mc.del = 0;
        return;
    case 62: case 63:                                                          /* Left, Right: the bars; Shift the bank */
        if (cc == 62) mc.left = (uint8_t)down; else mc.right = (uint8_t)down;
        if (!down) return;
        if (mc.shift) {
            mc.used = 1;
            mc.bank = (uint8_t)CLAMP(mc.bank + (cc == 63 ? 1 : -1), 0, M2K_BANKS - 1);
            char a[8] = {'B', 'A', 'N', 'K', ' ', (char)('A' + mc.bank), 0};
            toast(a, "STEPS PICK ITS 16");
            return;
        }
        bars(cc == 63 ? 1 : -1);
        return;
    default: break;
    }
    if (!down) return;
    switch (cc) {
    case 3: jog_click(); break;
    case 50:                                                                   /* Menu: a list closed */
        if (cat.open) { cat_back(); cat_left(); }
        mc.plist = 0;
        break;
    case 43: case 42: case 41: case 40: track_key(43 - cc); break;            /* (CC 43 is track 1) */
    case 85: mv_play_press(); break;                                           /* Play: Move's too (passed through) */
    case 86:                                                                   /* Rec: REC; Shift + Rec saves */
        if (mc.shift) { mc.used = 1; mv_save_request(1); break; }
        sq.rec ^= 1;
        if (!sq.rec) sq.ready = 0;
        toast(sq.rec ? "REC" : "REC OFF", 0);
        break;
    case 56: undo_do(mc.shift); if (mc.shift) mc.used = 1; break;             /* Undo; Shift + Undo redoes */
    case 55: case 54: {                                                        /* Up, Down */
        int d = cc == 55 ? 1 : -1;
        if (cat.open) { if (d > 0) cat_keep(); else cat_back(); cat_left(); break; }
        if (ui.nheld) { held_step_edit(0, d); break; }                         /* a held step's ratchet */
        if (mc.perf) {                                                         /* PERFORM's keys: 1-8, 9-11 */
            mc.perf_pg = (uint8_t)(d > 0);
            toast("PERFORM", mc.perf_pg ? "KEYS 9-11" : "KEYS 1-8");
            break;
        }
        int8_t *oc = &ui.oct[ui.trk];
        *oc = (int8_t)CLAMP(*oc + d, -3, 3);
        char o[8], *e = s_cpy(o, "OCT ");
        if (*oc > 0) *e++ = '+';
        s_int(e, *oc);
        toast(o, 0);
        break;
    }
    case 58: {                                                                 /* Loop: the track's arp HOLD */
        arp_hold_set(ui.trk, !sq.arp_hold[ui.trk]);
        char ht[12] = {'T', (char)('1' + ui.trk), ' ', 0};
        s_cpy(ht + 3, sq.arp_hold[ui.trk] ? "ARP HOLD" : "HOLD OFF");
        toast(ht, 0);
        break;
    }
    default: break;
    }
}

/* each block: the bars follow the playhead; the patterns' list closes a while after its last turn; a knob turned
 * shows large a moment */
static void mv_tick(void)
{
    if (ui.follow && sq.playing) {
        int s = track_pos(ui.trk);
        if (s >= 0) ui.spage = (uint8_t)(s / 16);
    }
    if (ui.spage * 16 >= view_len()) ui.spage = 0;
    if (mc.plist && ui.now - (mc.plist_t & ~1u) > MV_LIST_MS) mc.plist = 0;
    if (mc.focus_t && mc.touch < 0 && ui.now - (mc.focus_t & ~1u) > MV_FOCUS_MS) focus_off();
    if (!cat.open) cat_left();         /* (the catalogue closed elsewhere: a pattern, Undo) */
}

/* ------------------------------------------------------------------------------------------- the page state */

/* the Move's palette (Schwung's constants.mjs) and the white buttons' brightness. A family a colour: the sound pages
 * orange (the keyboard's root too), the sequence and mix pages azure (the steps' trigs too), the functions magenta
 * (PERFORM's keys too); the page in view or the function on bright, the rest dim */
enum { C_OFF = 0, C_RED = 1, C_ORANGE = 3, C_AZURE = 16, C_MAGENTA = 26, C_WHITE = 120, C_GREY = 119, C_GREY3 = 123,
       C_DAZURE = 95, C_DORANGE = 69, C_DMAGENTA = 115, C_DPINK = 109, C_DRED = 66 };
enum { W_OFF = 0, W_DIM = 16, W_MID = 64, W_ON = 124 };

static void str_n(char *d, const char *s, int n) { s_cpyn(d, s ? s : "", n); }

static int page_lit(int pg) { return pg == mc.page || (pg == MS_PERF && mc.perf) || (pg == MS_CAT && cat.open); }

static void mv_leds(mv_view_t *v)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = ui.trk;
    mv_leds_t *l = &v->led;
    for (int k = 0; k < 16; k++) {
        uint8_t c;
        if (mc.shift) {                /* the bank's patterns */
            int n = mc.bank * 16 + k;
            c = n == cur_pat ? C_WHITE : n == sq.queued ? C_ORANGE : C_GREY;
        } else if (mc.mute) {
            c = k >= M2K_TRACKS ? C_OFF : mute_will(k) ? C_DRED : C_AZURE;
        } else {
            uint8_t f = v->step[k];
            c = (f & MV_ST_PAST) ? C_OFF : (f & MV_ST_PLAY) || (f & MV_ST_HELD) ? C_WHITE
                : (f & MV_ST_TRIG) ? ((f & MV_ST_LOCK) ? C_DAZURE : C_AZURE) : (f & MV_ST_BEAT) ? C_GREY : C_OFF;
        }
        l->step[k] = c;
    }
    const int root = track_root(p, t) % 12;
    for (int i = 0; i < 16; i++) {     /* the keyboard, or PERFORM's keys */
        uint8_t c;
        if (mc.perf) {
            int e = perf_of_key(i);
            c = e < 0 ? C_OFF : perf.on[e] ? C_WHITE : i < 8 ? C_DMAGENTA : C_DPINK;
        } else {
            int n = key_note(i), lit = 0;
            if (n < 0) { l->pad[i] = C_OFF; continue; }
            for (int k = 0; k < 16; k++) if (mc.pkind[k] == 1 && mc.parg[k] == n && mc.ptrk[k] == t) lit = 1;
            if (trk[t].gate && trk[t].note == n) lit = 1;
            c = lit ? C_WHITE : n % 12 == root ? C_ORANGE : i < 8 ? C_GREY3 : C_GREY;
        }
        l->pad[i] = c;
    }
    for (int j = 0; j < 16; j++) {     /* the page pads */
        const int pg = MS_PAD_PAGE[j], fam = ms_family(pg), on = page_lit(pg);
        l->pad[MS_PAD0 + j] = (uint8_t)(fam == MS_FAM_SOUND ? (on ? C_ORANGE : C_DORANGE)
                                        : fam == MS_FAM_FN ? (on ? C_MAGENTA : C_DMAGENTA) : (on ? C_AZURE : C_DAZURE));
    }
    for (int b = 0; b < 4; b++) {
        int tt = mc.shift ? b + 4 : b;
        l->trk[b] = tt >= M2K_TRACKS ? C_OFF : tt == t ? C_WHITE : mute_will(tt) ? C_DRED : C_DAZURE;
    }
    const int last = v->npage ? v->npage - 1 : 0;
    l->btn[0] = sq.rec ? C_RED : C_OFF;                                       /* Rec */
    l->btn[1] = mc.mute ? W_ON : v->mutes ? W_MID : W_DIM;                     /* Mute */
    l->btn[2] = undo.ok ? W_MID : W_DIM;                                       /* Undo */
    l->btn[3] = mc.copy ? W_ON : W_DIM;                                        /* Copy */
    l->btn[4] = mc.del ? W_ON : W_DIM;                                         /* Delete */
    l->btn[5] = ui.spage > 0 ? W_ON : last ? W_DIM : W_OFF;                   /* Left */
    l->btn[6] = ui.spage < last ? W_ON : last ? W_DIM : W_OFF;                /* Right */
    l->btn[7] = mc.perf ? (mc.perf_pg ? W_ON : W_DIM) : ui.oct[t] > 0 ? W_ON : W_DIM;   /* Up */
    l->btn[8] = mc.perf ? (mc.perf_pg ? W_DIM : W_ON) : ui.oct[t] < 0 ? W_ON : W_DIM;   /* Down */
    l->btn[9] = cat.open || mc.plist ? W_ON : W_OFF;                           /* Menu: a list to close */
    l->btn[10] = sq.arp_hold[t] ? W_ON : W_DIM;                                /* Loop */
}

static void mv_view_build(mv_view_t *v)
{
    const m2k_pattern_t *p = PAT(cur_pat);
    const int t = ui.trk;
    v->page = mc.page;
    v->trk = (uint8_t)t;
    str_n(v->machine, MI[p->machine[t]].name, sizeof v->machine);
    s_pat(v->slot, cur_pat);
    if (sq.queued != cur_pat) s_pat(v->next, sq.queued); else v->next[0] = 0;
    v->bpm10 = (uint16_t)mv_bpm10();
    v->sync = (uint8_t)mv_sync();
    v->following = (uint8_t)mv_following();
    v->playing = sq.playing;
    v->rec = sq.rec;
    v->shift = mc.shift;
    for (int k = 0; k < 8; k++) {
        mv_knob_t *kn = &v->knob[k];
        const int code = mv_code(k), kt = mv_ktrk(k);
        const pi_t *pi = pinfo(kt, code);
        kn->flags = 0;
        kn->raw = 0;
        if (code == NONE || pi->s4[0] == '-' || mc.page >= MS_PAGES) { kn->flags = MV_K_FREE; continue; }
        kn->raw = (uint16_t)mv_value(code, kt);
        if (mv_locked(code, kt)) kn->flags |= MV_K_LOCK;
        if (bipolar(pi->kind)) kn->flags |= MV_K_BIP;
        if (kind_choices(pi->kind)) kn->flags |= MV_K_CHOICE;
        if (code == GL(G_TMPO) && mv_sync() == MV_SYNC_MOVE) kn->flags |= MV_K_MOVE;
    }
    for (int k = 0; k < 8; k++) {      /* how ui.js turns each: the tempo, a short list, a range */
        int kk = MV_KK_NONE;
        if (cat.open) kk = k == 0 ? MV_KK_RANGE : k == 1 ? MV_KK_CHOICE : MV_KK_NONE;
        else if (!(v->knob[k].flags & MV_K_FREE)) {
            const int code = mv_code(k), n = kind_choices(pinfo(mv_ktrk(k), code)->kind);
            kk = code == GL(G_TMPO) ? MV_KK_TEMPO : n && n <= MV_KK_LIST ? MV_KK_CHOICE : MV_KK_RANGE;
        }
        if (k == 0 && mc.shift) kk = MV_KK_TEMPO;
        v->kkind[k] = (uint8_t)kk;
    }
    v->tq_state = tq.state;
    v->tq_seq = tq.seq;
    v->tq_b10 = tq.b10;
    v->focus = mc.touch >= 0 ? mc.touch : mc.focus_t ? mc.focus : -1;
    v->tempo_focus = (uint8_t)(v->focus >= 0 && mc.tempo_focus);
    /* the steps */
    const int len = tlen_of(t), pos = sq.playing ? track_pos(t) : -1;
    v->spage = ui.spage;
    v->npage = (uint8_t)((view_len() + 15) / 16);
    v->follow = ui.follow;
    for (int k = 0; k < 16; k++) {
        const int s = ui.spage * 16 + k;
        uint8_t f = (k & 3) ? 0 : MV_ST_BEAT;
        if (s >= len) f |= MV_ST_PAST;
        else {
            const m2k_step_t *st = &p->step[t][s];
            if (st->flags & ST_TRIG) {
                f |= MV_ST_TRIG;
                if (has_locks(t, s)) f |= MV_ST_LOCK;
                if (st->flags & ST_ACC) f |= MV_ST_ACC;
                if (st->cond || st->rtrg > 1) f |= MV_ST_COND;
            }
            if (s == pos) f |= MV_ST_PLAY;
            for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == s) f |= MV_ST_HELD;
        }
        v->step[k] = f;
    }
    v->held = ui.nheld;
    v->mutes = 0;
    for (int k = 0; k < M2K_TRACKS; k++) if (mute_will(k)) v->mutes |= (uint8_t)(1u << k);
    v->oct = ui.oct[t];
    v->bank = mc.bank;
    v->overlay = cat.open ? MV_OV_CAT : mc.plist ? MV_OV_PATS : mc.shift ? MV_OV_SHIFT : mc.mute ? MV_OV_MUTE : MV_OV_NONE;
    v->pat_sel = mc.pat_sel;
    v->perf = mc.perf;
    v->perf_pg = mc.perf_pg;
    v->cat_slot[0] = 'S';
    s_num3(v->cat_slot + 1, ui.snd_sel + 1);
    str_n(v->cat_name, snd[ui.snd_sel].name[0] ? snd[ui.snd_sel].name : "INIT", sizeof v->cat_name);
    v->perf_on = 0;
    for (int k = 0; k < PF_COUNT && k < 32; k++) if (perf.on[k]) v->perf_on |= 1u << k;
    if (ui.toast_t && ui.now - (ui.toast_t & ~1u) < MV_TOAST_MS) {
        str_n(v->toast, ui.toast, sizeof v->toast);
        str_n(v->toast2, ui.toast2, sizeof v->toast2);
    } else v->toast[0] = v->toast2[0] = 0;
    mv_leds(v);
    /* the screen's view: the moment as the renderer wants it, then everything it reads of the core */
    m2k_scr_t s;
    m_set(&s, 0, sizeof s);
    s.page = mc.plist ? MS_PATS : mc.page;
    s.track = (uint8_t)t;
    s.touch = (int8_t)(mc.plist && !v->tempo_focus ? -1 : v->focus);
    s.tempo = v->tempo_focus;
    s.held = ui.nheld ? ui.held[0] : 0xFF;
    s.spage = ui.spage;
    s.cat_sel = ui.snd_sel;
    s.cat_cat = ui.snd_cat;
    s.pat_sel = mc.pat_sel;
    s.sync = (uint8_t)(mv_sync() == MV_SYNC_MOVE);
    s.bpm10 = (uint16_t)mv_bpm10();
    s.shift = mc.shift;
    s.mute = mc.mute;
    s.bank = mc.bank;
    s.perf = mc.perf;
    s.perf_pg = mc.perf_pg;
    s.toast = v->toast;
    s.toast2 = v->toast2;
    s.ms = ui.now;
    m2k_screen_gather(&v->scr, &s);
}
