/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's store and the Patch Editor's link (firmware/fm1/m2k_link.c) on a flash in memory, with the core as the
 * FM-1 builds it (64 patterns, two unpacked, one project), and the host's side of the link as web/editor.js speaks it: a store
 * read out and written back, and what a wrong, cut, stalled, refused or busy write leaves; the pattern cache's
 * write-back, saves that rewrite only what changed, settings, DEMO, damaged stores. The flash is NOR: a write only
 * clears bits, and one over a byte not erased is counted as a fault.
 *
 *   cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_link_test host/m2k_link_test.c
 *   build/host/m2k_link_test [WEB_STORE] [FM1_STORE_OUT]
 *
 * WEB_STORE: a store the browser wrote for the FM-1 (build/host/m2k_host --web-store PATH), written through the link;
 * FM1_STORE_OUT: the store read out of the FM-1, for the browser to read (m2k_host --fm1-store PATH). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M2K_FW 1
#define M2K_PATTERNS 64
#define M2K_PAT_SLOTS 2
#define M2K_PROJECTS 1
#define M2K_IMG_DATA 47104
#define POOL
#include "../firmware/src/m2k_core.c"

/* ---- the platform, as m2k_link.c uses it */
#define APP_NAME "MONO2K"
#define APP_VER "0.0"
#define BANK_AT 0x000C0000u
#define BANK_LEN 0x00020000u
#define FELUCCA_ID "FM-1_980"
static uint8_t flash[0x100000];
static int flash_faults, flash_fail_at = -1, flash_writes, flash_erases;
static uint8_t flash_ok = 1;
static volatile uint32_t fm1_ms;
static void fm1_wdt_feed(void) {}
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    *took = 0;
    if (off & 0xFFFu || off + 4096u > sizeof flash) return -1;
    memset(flash + off, 0xFF, 4096);
    flash_erases++;
    return 0;
}
static int fl_write(uint32_t off, const uint8_t *p, uint32_t n)
{
    if (off + n > sizeof flash || flash_writes++ == flash_fail_at) return -1;
    for (uint32_t i = 0; i < n; i++) {
        if (flash[off + i] != 0xFF) flash_faults++;
        flash[off + i] &= p[i];
    }
    return 0;
}
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off + n > sizeof flash) return -1;
    memcpy(dst, flash + off, n);
    return 0;
}

/* the update entry's hooks (ota.c: only its 7-bit packing is used here); a reply is kept for the test */
static uint8_t reply[1024];
static uint32_t reply_n;
static int replies;
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    if (n <= sizeof reply) { memcpy(reply, p, n); reply_n = n; }
    replies++;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) {}
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/fm1/src/ota.c"

#include "../firmware/fm1/m2k_link.c"

/* ---- the editor's side */
static uint8_t ans[512];
static uint32_t ans_n;
static int wire_max;

/* a request through the platform's hook; the status of the one answer, decoded into ans (-1 not taken, -2 no answer
 * or a malformed one) */
static int req(int cmd, const uint8_t *body, uint32_t n)
{
    uint8_t f[640];
    uint32_t w = 0;
    f[w++] = 0x7D; f[w++] = 'M'; f[w++] = '2'; f[w++] = 'K'; f[w++] = (uint8_t)cmd;
    w += ota_pack7(body, n, f + w);
    if (w + 2u > 640u) return -3;                 /* (F0 and F7 around it: the platform's frame) */
    replies = 0;
    if (!app_sysex(f, w)) return -1;
    lk_service();
    if (replies != 1 || reply_n < 8u || reply[0] != 0xF0 || reply[reply_n - 1] != 0xF7 || reply[1] != 0x7D ||
        reply[2] != 'M' || reply[3] != '2' || reply[4] != 'K' || reply[5] != (cmd | 0x40)) return -2;
    for (uint32_t i = 1; i + 1 < reply_n; i++) if (reply[i] & 0x80) return -2;
    if ((int)reply_n > wire_max) wire_max = (int)reply_n;
    ans_n = ota_unpack7(reply + 6, reply_n - 7, ans, sizeof ans);
    return ans[0];
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t crc32(const uint8_t *p, uint32_t n) { return ~crc_more(0xFFFFFFFFu, p, n); }

/* the store read out: INFO, then READ 128 bytes at a time (as large as the region: a full store is ~105 KB) */
static uint8_t rx[BANK_LEN];
static int read_store(uint32_t *len)
{
    if (req(LK_INFO, 0, 0) != LK_OK || ans_n < 9u) return 0;
    uint32_t n = rd32(ans + 1), c = rd32(ans + 5);
    if (n > sizeof rx) return 0;
    for (uint32_t at = 0; at < n;) {
        uint8_t q[5];
        wr32(q, at);
        q[4] = (uint8_t)(n - at > 128u ? 128u : n - at);
        if (req(LK_READ, q, 5) != LK_OK || rd32(ans + 1) != at || ans[5] != q[4]) return 0;
        memcpy(rx + at, ans + 6, ans[5]);
        at += ans[5];
    }
    *len = n;
    return crc32(rx, n) == c;
}

/* a store written: BEGIN, WRITE 256 bytes at a time, END; the END's status (or -10 - the step that failed) */
static int write_store(const uint8_t *b, uint32_t n, uint32_t crc, uint32_t stop_after)
{
    uint8_t q[4 + 256];
    wr32(q, n);
    wr32(q + 4, crc);
    if (req(LK_BEGIN, q, 8) != LK_OK) return -10 - ans[0];
    for (uint32_t at = 0, k = 0; at < n; k++) {
        if (k == stop_after) return -100;
        uint32_t c = n - at > 256u ? 256u : n - at;
        wr32(q, at);
        memcpy(q + 4, b + at, c);
        if (req(LK_WRITE, q, 4 + c) != LK_OK || rd32(ans + 1) != at + c) return -20 - ans[0];
        at += c;
    }
    return req(LK_END, 0, 0);
}

static int fails;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

/* a sound's note by its name, so each state's slot 6 has its own (the store's 'R'; m2k_host --web-store sets it so) */
static uint8_t name_root(const char *s) { unsigned h = 0; while (*s) h = h * 31u + (uint8_t)*s++; return (uint8_t)(24 + h % 72); }

static void mark(const char *pn, const char *sn)   /* a state to tell apart: pattern A01's name, sound slot 6's, its note */
{
    memset(PAT(0)->name, 0, 8);
    memcpy(PAT(0)->name, pn, strlen(pn));
    memset(snd[5].name, 0, 8);
    memcpy(snd[5].name, sn, strlen(sn));
    snd_root[5] = name_root(sn);
}
static int marked(const char *pn, const char *sn)
{
    return !strcmp(PAT(0)->name, pn) && !strcmp(snd[5].name, sn) && snd_root[5] == name_root(sn);
}
static int stored(void) { uint32_t h[4]; return !st_read(BANK_AT, h, 16) && h[0] == STORE_MAGIC && store_load(); }

/* the pattern cache (the FM-1 keeps two of its 64 patterns unpacked, the rest packed in the working image) */
static int trigs_of(int i)
{
    int n = 0;
    for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) n += PAT(i)->step[t][st].flags & ST_TRIG ? 1 : 0;
    return n;
}

static void fill(int i, int seed)                 /* every step of every track a trig, its note by seed */
{
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int st = 0; st < M2K_STEPS; st++) {
            m2k_step_t *x = &PAT(i)->step[t][st];
            x->flags = ST_TRIG;
            x->note = (uint8_t)(24 + (seed * 7 + t * 5 + st) % 72);
            x->cond = 0;
            x->rtrg = 0;
        }
}

static uint32_t bank_hash(void)                   /* every pattern packed, and the sound library (m2k_host --bank-hash) */
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < M2K_PATTERNS; i++) { int n = pat_pack(PAT(i), blob); for (int k = 0; k < n; k++) h = (h ^ blob[k]) * 16777619u; }
    const uint8_t *sb = (const uint8_t *)snd;
    for (unsigned k = 0; k < sizeof snd; k++) h = (h ^ sb[k]) * 16777619u;
    return h;
}

static void cache_tests(void)
{
    static int16_t buf[256];
    memset(flash, 0xFF, sizeof flash);
    m2k_init(3);
    printf("     bank hash %08x\n", bank_hash());
    int names = !strcmp(PAT(0)->name, "CIRCUIT") && !strcmp(PAT(3)->name, "LANTERN");
    for (int i = 16; i < 64; i++) names &= !strcmp(PAT(i)->name, "INIT") && trigs_of(i) == 0;
    check(names && work_img.used < 20000u, "64 patterns: bank A's demos, banks B-D INIT, the blanks taking no room");

    m2k_pattern(16);                              /* B01 */
    PAT(16)->step[2][5].flags = ST_TRIG; PAT(16)->step[2][5].note = 49;
    m2k_pattern(36);                              /* C05 */
    PAT(36)->step[0][0].flags = ST_TRIG; PAT(36)->step[0][0].note = 40;
    m2k_pattern(0);
    m2k_pattern(63);
    int kept = cur_pat == 63 && PAT(16)->step[2][5].note == 49 && trigs_of(16) == 1 && PAT(36)->step[0][0].note == 40;
    store_save();
    m2k_init(4);
    int back = store_load() && PAT(16)->step[2][5].note == 49 && trigs_of(36) == 1 && !strcmp(PAT(0)->name, "CIRCUIT");
    check(kept && back, "patterns edited, left and come back to keep their steps, through a save and a boot");

    m2k_pattern(16);
    seq_play(1);
    m2k_pattern(36);                              /* queued: at the end of the bar */
    int queued = cur_pat == 16 && sq.queued == 36;
    for (int f = 0; f < 44100 * 12 && cur_pat == 16; f += 128) m2k_render(buf, 128);
    int switched = cur_pat == 36;
    for (int f = 0; f < 44100 * 2; f += 128) m2k_render(buf, 128);
    seq_play(0);
    check(queued && switched && trigs_of(16) == 1, "a pattern queued while playing opens at the end of the bar");

    /* the working image fills: a pattern that would not leave the playing one room is refused, and nothing is lost */
    int filled[64] = {0}, last = -1, refused = 0;
    for (int i = 16; i < 64 && !refused; i++) {
        m2k_pattern(i);
        if (cur_pat != i) { refused = 1; break; }
        fill(i, i);
        filled[i] = 1;
        last = i;
    }
    int intact = refused && last > 20;
    for (int i = 16; i <= last; i++) intact &= !filled[i] || (trigs_of(i) == 6 * 64 && PAT(i)->step[3][9].note == (uint8_t)(24 + (i * 7 + 15 + 9) % 72));
    printf("     the working image: %u of %u B, patterns full after %d full ones\n", work_img.used, IMG_DATA, last - 15);
    store_save();
    m2k_init(5);
    int stored = store_load();
    for (int i = 16; i <= last; i++) stored &= trigs_of(i) == 6 * 64;
    check(intact && stored, "patterns full: the next refused, every full one intact, through a save and a boot");

    /* a near-full store: the project saved as well, both images near their room */
    project_save(0);
    uint32_t nf = 0;
    int rd = read_store(&nf);
    static uint8_t big[BANK_LEN];
    memcpy(big, rx, nf);
    memset(flash, 0xFF, sizeof flash);            /* another FM-1 */
    m2k_init(6);
    int wr = write_store(big, nf, crc32(big, nf), ~0u) == LK_OK;
    for (int i = 16; i <= last; i++) wr &= trigs_of(i) == 6 * 64;
    printf("     a near-full store: %u B\n", nf);
    check(rd && nf > 0x10000u && wr, "a near-full store (over 64 KB) read out, written into another FM-1 and played");
}

/* every step a trig and 384 locks: the most a pattern packs to */
static void fill_most(int i, int seed)
{
    fill(i, seed);
    for (int k = 0; k < M2K_LOCKS; k++) PAT(i)->lock[k] = (m2k_lock_t){(uint8_t)(k % 6), (uint8_t)((k / 6) % 64), P_VOL, (uint8_t)((k + seed) & 127)};
}

static uint32_t pat_hash(int i)
{
    int n = pat_pack(PAT(i), blob);
    uint32_t h = 2166136261u;
    for (int k = 0; k < n; k++) h = (h ^ blob[k]) * 16777619u;
    return h;
}

/* the cache writes back both patterns it holds: the one played before is counted with the one playing (an edited
 * pattern left held is never dropped unwritten); and an image filled from elsewhere says PROJECT FULL rather than
 * save without the edits */
static void writeback_tests(void)
{
    static int16_t buf[256];
    int lost = 0, unsaved = 0;
    for (int e = 0; e < 384; e += 32) {          /* the image filled to a different point each run */
        memset(flash, 0xFF, sizeof flash);
        m2k_init(3);
        for (int i = 16; i < 30; i++) { m2k_pattern(i); fill(i, i); }
        m2k_pattern(40);
        for (int k = 0; k < e; k++) { PAT(40)->step[k / 64][k % 64].flags = ST_TRIG; PAT(40)->step[k / 64][k % 64].note = 50; }
        int ed[4] = {51, 52, 53, 54}, opened[4] = {0};
        uint32_t h[4] = {0};
        m2k_pattern(50);
        seq_play(1);
        m2k_pattern(51);                          /* queued: into the slot not holding D03 */
        for (int f = 0; f < 44100 * 20 && cur_pat != 51; f += 128) m2k_render(buf, 128);
        seq_play(0);
        for (int k = 0; k < 4; k++) {             /* D04 .. D07, each opened in turn (or PATTERNS FULL) and filled */
            if (k) m2k_pattern(ed[k]);
            if (cur_pat != ed[k]) break;
            opened[k] = 1;
            fill_most(ed[k], k + 1);
            h[k] = pat_hash(ed[k]);
        }
        unsaved += !project_save(0);
        store_save();
        m2k_init(9);
        store_load();
        for (int k = 0; k < 4; k++) lost += opened[k] && pat_hash(ed[k]) != h[k];
    }
    check(!lost && !unsaved, "the cache: patterns edited one after the other, PROJECT SAVED and a boot: every edit kept");

    /* a store whose working image leaves no room (a browser's, near the FM-1's room): A01 grows past it */
    memset(flash, 0xFF, sizeof flash);
    m2k_init(3);
    for (int i = 16; i < M2K_PATTERNS; i++) {
        pattern_init_blank(&pat_tmp);
        for (int t = 0; t < M2K_TRACKS; t++) for (int s = 0; s < M2K_STEPS; s++) { pat_tmp.step[t][s].flags = ST_TRIG; pat_tmp.step[t][s].note = (uint8_t)(30 + i); }
        int n = pat_pack_img(&pat_tmp);
        if (work_img.used + (uint32_t)n > IMG_DATA - 300u) break;
        pat_drop();
        img_put(&work_img, i, blob, n);
    }
    pat_drop();
    cur_pat = 0;
    project_save(0);
    store_save();
    uint32_t failed = saves_failed;
    for (int t = 4; t < 6; t++) for (int s = 0; s < 64; s++) { PAT(0)->step[t][s].flags = ST_TRIG; PAT(0)->step[t][s].note = 72; }
    int mine = trigs_of(0), refused = !project_save(0);
    store_save();
    int told = refused && saves_failed == failed + 1 && trigs_of(0) == mine;   /* PROJECT FULL; the edits still there */
    for (int i = 16; i < 20; i++) pattern_init_blank(PAT(i));      /* the player INITs four patterns (HOME + SEQ held) */
    int saved = project_save(0);
    store_save();
    m2k_init(4);
    int back = store_load() && trigs_of(0) == mine;
    check(told && saved && back, "a working image with no room: SAVE says PROJECT FULL and keeps the edits; patterns INIT, it saves them");
}

/* a store the core refuses (its CRC right, an image's length wrong): the FM-1 keeps what it had, in the flash too */
static void refused_tests(void)
{
    memset(flash, 0xFF, sizeof flash);
    m2k_init(1);
    mark("MYWORK", "MYSND");
    uint32_t na = 0;
    read_store(&na);
    static uint8_t a[BANK_LEN];
    memcpy(a, rx, na);
    a[10 + S_COUNT + 8] ^= 1;                    /* the working image's length one off */
    int st = write_store(a, na, crc32(a, na), ~0u);
    check(st == LK_REFUSED && marked("MYWORK", "MYSND") && stored() && marked("MYWORK", "MYSND"),
          "a store the core refuses: LK_REFUSED, what the FM-1 played kept, and in its flash");
}

/* while the sequencer plays, a write's pieces and its END are answered PLAYING (a sector's erase stops the sound for
 * ~50 ms): nothing erased, the transfer kept, going on once it stands; one that stalls meanwhile is written back then */
static void playing_tests(void)
{
    memset(flash, 0xFF, sizeof flash);
    m2k_init(1);
    mark("PLAYA", "SNDPA");
    uint32_t na = 0;
    read_store(&na);
    static uint8_t a[BANK_LEN];
    memcpy(a, rx, na);
    mark("PLAYB", "SNDPB");
    store_save();
    uint8_t q[4 + 256];
    wr32(q, na); wr32(q + 4, crc32(a, na));
    int ok = req(LK_BEGIN, q, 8) == LK_OK;
    uint32_t at = 0;
    for (int k = 0; k < 20 && ok; k++, at += 256) { wr32(q, at); memcpy(q + 4, a + at, 256); ok = req(LK_WRITE, q, 260) == LK_OK; }
    sq.playing = 1;
    int e0 = flash_erases, w0 = flash_writes;
    wr32(q, at); memcpy(q + 4, a + at, 256);
    int held = req(LK_WRITE, q, 260) == LK_PLAYING && rd32(ans + 1) == at && req(LK_END, 0, 0) == LK_PLAYING && lk_open &&
               flash_erases == e0 && flash_writes == w0 && marked("PLAYB", "SNDPB");
    sq.playing = 0;
    for (; at < na && ok; at += 256) {
        uint32_t c = na - at > 256u ? 256u : na - at;
        wr32(q, at); memcpy(q + 4, a + at, c);
        ok = req(LK_WRITE, q, 4 + c) == LK_OK;
    }
    ok = ok && req(LK_END, 0, 0) == LK_OK && marked("PLAYA", "SNDPA");
    check(held && ok, "while it plays: a write's pieces and its END answered PLAYING, nothing erased; it goes on once it stands");

    mark("PLAYC", "SNDPC");
    store_save();
    wr32(q, na); wr32(q + 4, crc32(a, na));
    ok = req(LK_BEGIN, q, 8) == LK_OK;
    for (at = 0; at < 20 * 256 && ok; at += 256) { wr32(q, at); memcpy(q + 4, a + at, 256); ok = req(LK_WRITE, q, 260) == LK_OK; }
    sq.playing = 1;
    e0 = flash_erases;
    fm1_ms += 5000;
    lk_service();                                /* stalled: ended, its save waiting */
    uint32_t h[4];
    int waits = ok && !lk_open && save_due && flash_erases == e0 && !st_read(BANK_AT, h, 16) && h[0] != STORE_MAGIC;
    sq.playing = 0;
    if (save_due && !m2k_playing() && !lk_open) { store_save(); save_due = 0; }   /* the glue's loop (m2k_fm1.c) */
    check(waits && stored() && marked("PLAYC", "SNDPC"), "a write that stalls while it plays: the FM-1's state written back once it stands");
}

/* a save rewrites only what changed: one the flash holds already writes nothing, a knob turned the sectors it moved */
static void save_tests(void)
{
    memset(flash, 0xFF, sizeof flash);
    m2k_init(1);
    store_save();
    int e0 = flash_erases, w0 = flash_writes;
    store_save();
    int same = flash_erases == e0 && flash_writes == w0;
    m2k_pattern(3);
    PAT(3)->kit[2][P_PAN] ^= 1;                  /* a knob, a detent: the store as long as it was */
    int pan = PAT(3)->kit[2][P_PAN];
    e0 = flash_erases;
    flash_faults = 0;
    store_save();
    int few = flash_erases - e0;
    m2k_init(2);
    int back = stored() && PAT(3)->kit[2][P_PAN] == pan;
    printf("     a save after a knob turned: %d of %u sectors erased\n", few, (unsigned)(BANK_LEN / 4096u));
    check(same && few >= 1 && few <= 2 && back && !flash_faults, "a save the flash holds already writes nothing; a knob turned rewrites its sector and the header's");
}

/* GLO > MIDI's channel turned on the panel: saved once it has stood 3 s, there after a power cycle */
static void setting_tests(void)
{
    static int16_t buf[256];
    memset(flash, 0xFF, sizeof flash);
    m2k_init(1);
    store_save();
    m2k_dirty();
    ui.page = PG_GLO; ui.sub = 4;
    edit(0, 1);
    edit(0, 1);
    int v = store_.set[S_MCH], early = m2k_dirty();
    for (int f = 0; f < 44100 * 3 + 4410; f += 128) m2k_render(buf, 128);
    if (m2k_dirty()) save_due = 1;               /* the glue's loop */
    if (save_due && !m2k_playing() && !lk_open) { store_save(); save_due = 0; }
    m2k_init(2);
    check(!early && v > 0 && store_load() && store_.set[S_MCH] == v, "a setting turned is saved once it stands, there after a power cycle");
}

/* GLO > SET > DEMO on the FM-1: A01-A04 back, A05 the player's kept */
static void demo_tests(void)
{
    m2k_init(1);
    for (uint32_t ms = 0; ms <= 3000; ms += 500) m2k_frame(ms);
    m2k_pattern(4);
    memset(PAT(4)->name, 0, 8); memcpy(PAT(4)->name, "MINE", 4);
    for (int s = 0; s < 64; s++) PAT(4)->step[0][s].flags = ST_TRIG;
    m2k_pattern(0);
    memset(PAT(0)->name, 0, 8); memcpy(PAT(0)->name, "WIPED", 5);
    ui.page = PG_GLO; ui.sub = 3; ui.now = 3200;
    int dk = knob_param(PG_GLO, 3, 2) == SETTING(23);
    edit(2, 1);
    m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);   /* YES */
    int mine = 0;
    for (int s = 0; s < 64; s++) mine += PAT(4)->step[0][s].flags & ST_TRIG;
    check(dk && !strcmp(PAT(0)->name, "CIRCUIT") && !strcmp(PAT(3)->name, "LANTERN") && !strcmp(PAT(4)->name, "MINE") && mine == 64 &&
          !strcmp(ui.toast2, "A01-A04, THE REST KEPT"), "DEMO: A01-A04 back, A05 the player's kept, the message says so");
}

/* damaged stores in the flash, read as at boot: refused, or played and drawn, the patterns opening one by one */
static void fuzz_tests(void)
{
    static uint8_t good[BANK_LEN];
    static int16_t buf[256];
    uint32_t r = 777;
    memset(flash, 0xFF, sizeof flash);
    m2k_init(1);
    for (int i = 16; i < 20; i++) { m2k_pattern(i); fill(i, i); }
    store_save();
    uint32_t h[4];
    st_read(BANK_AT, h, 16);
    uint32_t n = h[1];
    memcpy(good, flash + BANK_AT + STORE_HDR, n);
    int loaded = 0;
    for (int it = 0; it < 120; it++) {
        memcpy(flash + BANK_AT + STORE_HDR, good, n);
        uint32_t len = n;
        for (int k = 0, f = 1 + (int)((r = r * 1664525u + 1013904223u) >> 8) % 8; k < f; k++) {
            r = r * 1664525u + 1013904223u;
            flash[BANK_AT + STORE_HDR + (r >> 8) % n] ^= (uint8_t)(1u << (r & 7u));
        }
        r = r * 1664525u + 1013904223u;
        if ((r >> 8) % 4 == 0) len = (r >> 12) % n;
        m2k_init(2);
        if (!m2k_store_unpack((int)len)) continue;
        loaded++;
        for (int j = 0; j < 6; j++) {               /* six opened through the cache, the last played (from the boot's
                                                     * state: a track's machine changed under its sound is the DSP's) */
            r = r * 1664525u + 1013904223u;
            m2k_pattern((int)((r >> 8) % 24));
            m2k_frame(4000 + 40u * (uint32_t)(it * 6 + j));
        }
        seq_play(1);
        for (int f = 0; f < 128 * 6; f += 128) m2k_render(buf, 128);
        seq_play(0);
    }
    printf("     damaged stores in the flash: %d of 120 loaded\n", loaded);
    check(loaded > 10, "damaged stores in the flash: refused, or played and drawn with their patterns opening");
}

int main(int argc, char **argv)
{
    memset(flash, 0xFF, sizeof flash);
    m2k_init(1);
    check(!store_load(), "an erased flash holds no store: the factory's bank stays");

    check(crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u, "the store's CRC-32 is the editor's (zlib's): 123456789, CBF43926");
    uint8_t f0[4] = {0x00, 0x59, 0x11, 0x00};
    check(!app_sysex(f0, 4), "the update entry's frames are left to it");
    check(req(LK_HELLO, 0, 0) == LK_OK && ans[1] == 1 && ans[2] == 64 && ans[3] == 1 && ans[4] == M2K_SOUNDS &&
          !memcmp(ans + 8, "MONO2K", 6) && rd32(ans + 24) == M2K_IMG_DATA,
          "HELLO: the link's version, 64 patterns, 1 project, 128 sounds, the name, the image's room");

    mark("LINKA", "SNDA");
    uint32_t na = 0;
    check(read_store(&na) && na > 1000u, "the store read out: saved first, 128 bytes a reply, its CRC-32 agrees");
    printf("     store %u B, the longest reply %d wire bytes\n", na, wire_max);
    check(wire_max <= 3 * 64, "a reply fits the platform's SysEx queue (64 USB-MIDI packets) without waiting");
    static uint8_t a[BANK_LEN];
    memcpy(a, rx, na);
    check(!memcmp(a, flash + BANK_AT + STORE_HDR, na), "what was read is what the flash holds");
    if (argc > 2) {
        FILE *f = fopen(argv[2], "wb");
        if (f) { fwrite(a, 1, na, f); fclose(f); }
    }

    mark("LINKB", "SNDB");
    store_save();
    check(stored() && marked("LINKB", "SNDB"), "(a second state saved)");
    flash_faults = 0;
    int st = write_store(a, na, crc32(a, na), ~0u);
    check(st == LK_OK && marked("LINKA", "SNDA"), "a store written: the core reads it, the patterns and the sounds (their notes too)");
    check(stored() && marked("LINKA", "SNDA"), "and it is the FM-1's store from then on");
    check(flash_faults == 0, "no byte programmed without its erase");

    mark("LINKC", "SNDC");
    check(write_store(a, na, crc32(a, na) ^ 1u, ~0u) == LK_CRC && marked("LINKC", "SNDC"),
          "a wrong CRC is refused: the FM-1 keeps what it plays");
    check(stored() && marked("LINKC", "SNDC"), "and its own state is back in the flash");

    uint8_t q[4 + 8];
    wr32(q, na); wr32(q + 4, crc32(a, na));
    check(req(LK_BEGIN, q, 8) == LK_OK, "(a write begun)");
    wr32(q, 256); memset(q + 4, 0, 8);
    check(req(LK_WRITE, q, 12) == LK_ORDER && rd32(ans + 1) == 0, "a chunk out of order is refused, the next offset said");
    fm1_ms += 5000;
    lk_service();
    check(!lk_open && stored() && marked("LINKC", "SNDC"), "a write that stalls 4 s ends: the FM-1's state back in the flash");

    check(write_store(a, na, crc32(a, na), 5) == -100, "(a write cut after five chunks)");
    fm1_ms += 5000;
    lk_service();
    check(stored() && marked("LINKC", "SNDC"), "a write cut short leaves the FM-1 as it was");

    sq.playing = 1;
    check(req(LK_INFO, 0, 0) == LK_PLAYING && req(LK_BEGIN, q, 8) == LK_PLAYING, "while it plays: neither read nor write");
    sq.playing = 0;

    check(req(LK_END, 0, 0) == LK_IDLE && req(LK_WRITE, q, 12) == LK_IDLE, "an END or a chunk with no write begun: refused");
    wr32(q, BANK_LEN); wr32(q + 4, 0);
    check(req(LK_BEGIN, q, 8) == LK_BIG, "a store larger than the region: refused");
    check(req(0x30, 0, 0) == LK_BAD, "an unknown request: answered, refused");

    flash_fail_at = flash_writes + 1;              /* (the write's second program: the sectors it holds alike are left) */
    st = write_store(a, na, crc32(a, na), ~0u);
    flash_fail_at = -1;
    check(st != LK_OK && stored() && marked("LINKC", "SNDC"), "a flash that fails during a write: refused, the FM-1's state kept");

    if (argc > 1) {                                /* the browser's store for the FM-1 */
        FILE *f = fopen(argv[1], "rb");
        static uint8_t w[BANK_LEN];
        size_t n = f ? fread(w, 1, sizeof w, f) : 0;
        if (f) fclose(f);
        check(n > 0 && write_store(w, (uint32_t)n, crc32(w, (uint32_t)n), ~0u) == LK_OK && marked("WEB16", "WEBSND"),
              "a store the browser wrote for the FM-1 (64 patterns, 1 project): written and read");
    }

    cache_tests();
    writeback_tests();
    refused_tests();
    playing_tests();
    save_tests();
    setting_tests();
    demo_tests();
    fuzz_tests();

    uint8_t pk[10] = {0, 1, 2, 0x7F, 0x80, 0xFF, 0x55, 0xAA, 0x01, 0xFE}, pw[16];
    uint32_t pn = ota_pack7(pk, 10, pw);
    printf("     pack7 of 00 01 02 7F 80 FF 55 AA 01 FE:");
    for (uint32_t i = 0; i < pn; i++) printf(" %02X", pw[i]);
    printf("\n");
    printf("%s (%d failed)\n", fails ? "link test FAILED" : "link test passed", fails);
    return fails != 0;
}
