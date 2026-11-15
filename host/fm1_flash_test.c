/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's flash driver (firmware/fm1/hal/fm1_flash.h) on a simulated SPI NOR chip, both of the FM-1's parts and
 * the ones it must refuse: the id check (FL_JEDEC_OK), erase and program on a quick chip, a slow one, one that overruns
 * once, one that always overruns and one that never comes ready; then the store (firmware/fm1/m2k_link.c) saved, the
 * power cut and loaded again, and a self-update: the app's update entry (ota.c) staging the new package's loader from a
 * host that serves it, then the update loader (loader/ldr_core.c) installing it over the old one, on each chip. Every
 * run is watched for XIP coming back while the chip is busy, a read while it is busy, the interrupts held off too long,
 * and the watchdog (~8 s) going unfed.
 *
 *   cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/fm1_flash_test host/fm1_flash_test.c
 *   build/host/fm1_flash_test [OLD.fwsc NEW.fwsc]
 *
 * With two packages the update runs (OLD installed, NEW over it: e.g. git show v1.9.1:web/firmware/mono2k.fwsc and
 * tools/fm1/build.py's); without, the rest. The chip model's timing is its own (the UC25HQ80's real erase and program
 * times are not published here): what it holds is the driver's behaviour at, under and past its limits. */
#include <setjmp.h>
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

/* ---- the chip: 1 MiB, its id, an operation in flight, the time (TIMER4's 24 MHz ticks), the watchdog */
#define US(n) ((uint64_t)(n) * 24u)
static struct {
    uint32_t id;
    uint8_t mem[0x100000];
    uint64_t now;
    uint64_t erase_t, prog_t, over_t[3];       /* how long an erase and a page take, and an overrunning one (by kind) */
    uint32_t every[3], count[3];               /* the 1st, (every + 1)th ... erase (1) or page (2) overruns; 0 none */
    int stuck;                                 /* never comes ready, a reset neither */
    int wel, rst_en, xip;
    struct { int kind; uint32_t at; uint64_t done; uint8_t page[256]; uint32_t n; } op;   /* kind 1 erase, 2 program */
    uint64_t reset_done;
    /* the frame on the wire */
    int cs_low, nb;
    uint8_t cmd, buf[260];
    uint32_t addr, rd;
    /* what the driver must never do, and what it did */
    int xip_busy, read_busy, cmd_busy;
    uint64_t enter_t, irq_off_max;
    uint64_t fed, fed_gap_max;
    int wdt_armed;
    jmp_buf wdt;
    int erases, progs, resets_seen;
} chip;

static int busy(void)
{
    if (chip.stuck) return chip.op.kind != 0 || chip.reset_done > 0;
    if (chip.op.kind && chip.now >= chip.op.done) {           /* it finished: its effect now */
        if (chip.op.kind == 1) memset(chip.mem + chip.op.at, 0xFF, 4096);
        else for (uint32_t i = 0; i < chip.op.n; i++) chip.mem[chip.op.at + i] &= chip.op.page[i];
        chip.op.kind = 0;
    }
    return chip.op.kind != 0 || chip.now < chip.reset_done;
}
static void tick(uint64_t t)
{
    chip.now += t;
    if (chip.now - chip.fed > chip.fed_gap_max) chip.fed_gap_max = chip.now - chip.fed;
    if (chip.wdt_armed && chip.now - chip.fed > US(8000000)) longjmp(chip.wdt, 1);   /* the watchdog: a reset */
}
static void fm1_wdt_feed(void) { chip.fed = chip.now; }
static uint64_t op_time(int kind, uint64_t t)
{
    uint32_t n = chip.count[kind]++;
    return chip.every[kind] && n % chip.every[kind] == 0 ? chip.over_t[kind] : t;
}

static void sim_cs(int high)
{
    tick(2);
    if (!high) { chip.cs_low = 1; chip.nb = 0; return; }
    if (!chip.cs_low) return;
    chip.cs_low = 0;
    int b = busy();
    switch (chip.cmd) {
    case 0x06: if (!b) chip.wel = 1; else chip.cmd_busy++; break;
    case 0x66: chip.rst_en = 1; break;
    case 0x99:
        if (chip.rst_en) {                                   /* what was in flight is dropped, half done */
            if (chip.op.kind == 1) memset(chip.mem + chip.op.at, 0xA5, 2048);
            if (chip.op.kind == 2) for (uint32_t i = 0; i < chip.op.n / 2; i++) chip.mem[chip.op.at + i] &= chip.op.page[i];
            chip.op.kind = 0;
            chip.wel = 0;
            chip.reset_done = chip.now + US(30);
            chip.resets_seen++;
        }
        chip.rst_en = 0;
        break;
    case 0x20:
        if (b || !chip.wel || chip.nb != 4) { chip.cmd_busy += b; break; }
        chip.op.kind = 1; chip.op.at = chip.addr & 0xFF000u; chip.op.done = chip.now + op_time(1, chip.erase_t);
        chip.wel = 0; chip.erases++;
        break;
    case 0x02:
        if (b || !chip.wel || chip.nb < 5) { chip.cmd_busy += b; break; }
        chip.op.kind = 2; chip.op.at = chip.addr; chip.op.n = (uint32_t)chip.nb - 4u;
        memcpy(chip.op.page, chip.buf + 4, chip.op.n); chip.op.done = chip.now + op_time(2, chip.prog_t);
        chip.wel = 0; chip.progs++;
        break;
    }
    if (chip.cmd != 0x66 && chip.cmd != 0x99) chip.rst_en = 0;
}
static void sim_tx(uint8_t v)
{
    tick(8);
    if (chip.nb < (int)sizeof chip.buf) chip.buf[chip.nb] = v;
    if (chip.nb == 0) chip.cmd = v;
    if (chip.nb == 3) chip.addr = (uint32_t)chip.buf[1] << 16 | (uint32_t)chip.buf[2] << 8 | chip.buf[3];
    if (chip.cmd == 0x0B && chip.nb == 4) { chip.rd = chip.addr; if (busy()) chip.read_busy++; }
    chip.nb++;
}
static uint8_t sim_rx(void)
{
    tick(8);
    switch (chip.cmd) {
    case 0x9F: { int k = chip.nb++ - 1; return (uint8_t)(chip.id >> (16 - 8 * k)); }
    case 0x05: return (uint8_t)(busy() | chip.wel << 1);
    case 0x35: return 0;
    case 0x0B: return chip.mem[chip.rd++ & 0xFFFFFu];
    }
    return 0xFF;
}

/* ---- the driver's hardware seam (fm1_flash.h's FL_SIM): the chip above in place of the SFRs */
#define FL_SIM 1
#define RAMFN __attribute__((noinline, used))
#define RAMINL static inline
#define FL_FAR(fn) (fn)
#define T4_CNT ((uint32_t)(tick(24), chip.now))
#define FL_XIP(off) ((uintptr_t)(chip.mem + (off)))
#define fl_cs(h) sim_cs(h)
#define fl_tx(v) sim_tx(v)
#define fl_rx() sim_rx()
#define fl_enter(s) ((void)(s), chip.xip = 0, chip.enter_t = chip.now, 0)
#define fl_exit(s) do { (void)(s); if (busy()) chip.xip_busy++;                         \
                        if (chip.now - chip.enter_t > chip.irq_off_max) chip.irq_off_max = chip.now - chip.enter_t; \
                        chip.xip = 1; } while (0)
#define fl_inval(off, n) ((void)0)
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
#define FL_RANGE_OK(off, n) FL_IN(off, n, 0x4000u, 0xFD000u)   /* (the app's and the loader's windows together) */
#include "../firmware/fm1/hal/fm1_flash.h"

/* ---- the platform, as m2k_link.c, ota.c and the loader use it (firmware/fm1/src/platform.c, loader/loader.c) */
#define APP_NAME "MONO2K"
#define APP_VER "0.0"
#define BANK_AT 0x000C0000u
#define BANK_LEN 0x00020000u
#define FELUCCA_ID "FM-1_980"
#define FELUCCA_OTA_DRYRUN 0
static uint8_t flash_ok;
static volatile uint32_t fm1_ms;
static int st_read(uint32_t off, void *dst, uint32_t n)   /* (platform.c's) */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc) return rc;
        off += k; d += k; n -= k;
    }
    return 0;
}

/* the host that serves a package (web/fm1/fm1ota.js's part): a read request answered at once */
static const uint8_t *pkg;
static uint32_t pkg_n;
static uint8_t in7[1100];
static uint32_t in7_n;
static int in_ready, committed, shown_step;
static uint64_t req_last, req_gap_max;                         /* the longest the host waited for a request */
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!in_ready) return 0; *p = in7; *n = in7_n; return 1; }
static void ota_frame_done(void) { in_ready = 0; }
static uint32_t ota_pack7(const uint8_t *in, uint32_t n, uint8_t *out);
static uint32_t ota_unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max);
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t m[64], r[16 + 512];
    uint32_t d = ota_unpack7(p + 1, n - 2, m, sizeof m), i, s = 0;
    if (d < 15u || m[1] != 0x59 || m[2] != 0x30) return 0;           /* (a handshake: nothing to serve) */
    if (req_last && chip.now - req_last > req_gap_max) req_gap_max = chip.now - req_last;
    req_last = chip.now;
    uint32_t addr = (uint32_t)m[7] | (uint32_t)m[8] << 8 | (uint32_t)m[9] << 16 | (uint32_t)m[10] << 24;
    uint32_t len = (uint32_t)m[11] | (uint32_t)m[12] << 8;
    if (len > 512u) return 0;
    r[0] = 0; r[1] = 0x59; r[2] = 0x30;
    r[3] = (uint8_t)(len + 8u); r[4] = (uint8_t)((len + 8u) >> 8); r[5] = 0;
    r[6] = 0;
    r[7] = (uint8_t)addr; r[8] = (uint8_t)(addr >> 8); r[9] = (uint8_t)(addr >> 16); r[10] = (uint8_t)(addr >> 24);
    r[11] = (uint8_t)len; r[12] = (uint8_t)(len >> 8); r[13] = 0;
    for (i = 0; i < len; i++)
        r[14 + i] = addr >= 0xE0000000u ? (i < 8u ? (uint8_t)"success"[i] : 0) : addr + i < pkg_n ? pkg[addr + i] : 0xFF;
    for (i = 6; i < 14u + len; i++) s += r[i];
    r[14 + len] = (uint8_t)~s;
    in7_n = ota_pack7(r, 15u + len, in7);
    in_ready = 1;
    return 0;
}
static uint32_t ota_now_ms(void) { return (uint32_t)(chip.now / 24000u); }
static void ota_idle(void) { fm1_wdt_feed(); }
static int ota_erase(uint32_t off)                              /* (platform.c's, the staging area only) */
{
    uint32_t took;
    if (!FL_IN(off, 0x1000u, 0xE0000u, 0xE5000u) || (off & 0xFFFu)) return -8;
    return fl_erase4k(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!FL_IN(off, n, 0xE0000u, 0xE5000u)) return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code) { (void)code; shown_step = (int)step; }
static void ota_commit(const uint8_t *parm) { (void)parm; committed = 1; }
#include "../firmware/fm1/src/ota.c"

#include "../firmware/fm1/m2k_link.c"

/* the update loader's hooks (loader/loader.c's: the interrupts off throughout, the watchdog fed at each step) */
static int ldr_fread(uint32_t off, void *p, uint32_t n) { return fl_read_ram(off, p, n); }
static int ldr_erase(uint32_t off) { uint32_t took; fm1_wdt_feed(); return fl_erase4k_ram(off, &took); }
static int ldr_prog(uint32_t off, const void *p, uint32_t n) { uint32_t took; fm1_wdt_feed(); return fl_prog_ram(off, p, n, &took); }
static void ldr_record_clear(void) {}
static int ldr_flash_known(void) { return FL_JEDEC_OK(fl_jedec_ram()); }
static void ldr_progress(uint32_t done, uint32_t total) { (void)done; (void)total; fm1_wdt_feed(); }
#include "../firmware/fm1/loader/ldr_core.c"

/* ---- the tests */
static int fails;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

typedef struct {
    const char *name;
    uint32_t id, erase_us, prog_us;
    uint32_t every_e, over_e_us, every_p, over_p_us;          /* overruns: the 1st, (every + 1)th ... take over_us */
    int stuck;
} model_t;
static const model_t PUYA = {"Puya P25Q80, quick", 0x856014u, 45000, 700, 0, 0, 0, 0, 0};
static const model_t UC25 = {"UC25HQ80, slow", 0xB36014u, 1500000, 30000, 0, 0, 0, 0, 0};
static const model_t UC25_EDGE = {"UC25HQ80, at the limit", 0xB36014u, 1990000, 49000, 0, 0, 0, 0, 0};

static void chip_use(const model_t *m)
{
    static uint8_t keep[0x100000];
    memcpy(keep, chip.mem, sizeof keep);
    memset(&chip, 0, sizeof chip);
    memcpy(chip.mem, keep, sizeof keep);
    chip.id = m->id;
    chip.erase_t = US(m->erase_us);
    chip.prog_t = US(m->prog_us);
    chip.every[1] = m->every_e; chip.over_t[1] = US(m->over_e_us);
    chip.every[2] = m->every_p; chip.over_t[2] = US(m->over_p_us);
    chip.stuck = m->stuck;
    chip.xip = 1;
    memset(&fl_stat, 0, sizeof fl_stat);
}
static int clean(void) { return !chip.xip_busy && !chip.read_busy && !chip.cmd_busy; }

/* one sector erased and programmed through the app's wrappers, read back */
static int sector_round(uint32_t at, uint8_t seed, int *rc_out)
{
    static uint8_t w[4096], r[4096];
    uint32_t took;
    for (int i = 0; i < 4096; i++) w[i] = (uint8_t)(seed + i * 7);
    fm1_wdt_feed();
    int rc = fl_erase4k(at, &took);
    if (!rc) rc = fl_write(at, w, 4096);
    *rc_out = rc;
    return !rc && !st_read(at, r, 4096) && !memcmp(w, r, 4096);
}

static void driver_tests(void)
{
    static const uint32_t IDS[] = {0x856014u, 0xB36014u, 0xC84014u, 0xEF4014u, 0x856015u, 0x000000u, 0xFFFFFFu};
    int ids = 1;
    for (unsigned i = 0; i < sizeof IDS / sizeof IDS[0]; i++) {
        model_t m = PUYA;
        m.id = IDS[i];
        chip_use(&m);
        uint32_t got = FL_FAR(fl_jedec_ram)();
        ids &= got == IDS[i] && FL_JEDEC_OK(got) == (i < 2);
    }
    check(ids, "the id (9Fh) is read whole; FL_JEDEC_OK takes 0x856014 and 0xB36014, and no other");

    int rc;
    chip_use(&PUYA);
    check(sector_round(0xC0000u, 1, &rc) && clean() && !fl_stat.late_erase && !fl_stat.late_prog && !fl_stat.resets,
          "the Puya: a sector erased and written as before, nothing late, no reset");
    chip_use(&UC25);
    int ok = sector_round(0xC1000u, 2, &rc);
    printf("     the slow chip: erase %.1f s, page %.0f ms; late erases %u, late pages %u, the slowest erase %u ms\n",
           (double)chip.erase_t / 24e6, (double)chip.prog_t / 24e3, fl_stat.late_erase, fl_stat.late_prog,
           fl_stat.slowest_erase_us / 1000u);
    check(ok && clean() && fl_stat.late_erase == 1 && fl_stat.late_prog == 16 && !fl_stat.resets,
          "a chip slower than the old limits (1.5 s an erase, 30 ms a page; 1.9.1 gave up at 0.5 s and 10 ms): written, "
          "each operation counted late");
    chip_use(&UC25_EDGE);
    check(sector_round(0xC2000u, 3, &rc) && clean() && !fl_stat.resets, "just under the new limits (2 s, 50 ms): written, no reset");

    model_t once = UC25;
    once.every_e = 1000000; once.over_e_us = 2600000;          /* (the first erase only) */
    chip_use(&once);
    ok = sector_round(0xC3000u, 4, &rc);
    printf("     an erase that overran: interrupts off %.2f s at most, resets %u\n", (double)chip.irq_off_max / 24e6,
           fl_stat.resets);
    check(ok && clean() && fl_stat.resets == 1 && chip.resets_seen == 1,
          "an erase past 2 s: the chip reset (66h 99h), the erase done again, the sector right; XIP back only once it was ready");
    check(chip.irq_off_max < US(4300000), "and the interrupts held off well under the watchdog's 8 s");

    model_t prog_once = UC25;
    prog_once.every_p = 1000000; prog_once.over_p_us = 70000;  /* (the first page only) */
    chip_use(&prog_once);
    ok = sector_round(0xC4000u, 5, &rc);
    check(ok && clean() && fl_stat.resets == 1, "a page past 50 ms: the chip reset, the page programmed again, the bytes right");

    model_t always = UC25;
    always.every_e = 1; always.over_e_us = 2600000;
    chip_use(&always);
    ok = sector_round(0xC5000u, 6, &rc);
    check(!ok && rc == -4 && clean() && fl_stat.resets == 2 && chip.irq_off_max < US(5500000),
          "a chip that always overruns: two tries, two resets, an error (-4) to the caller, never XIP while busy");

    model_t stuck = UC25;
    stuck.stuck = 1;
    chip_use(&stuck);
    chip.fed = chip.now;
    chip.wdt_armed = 1;
    int wdt = 0;
    if (setjmp(chip.wdt)) wdt = 1;
    else { uint32_t took; fl_erase4k(0xC6000u, &took); }
    chip.wdt_armed = 0;
    check(wdt && !chip.xip_busy, "a chip that never comes ready: the driver keeps XIP off and the watchdog resets the FM-1");

    model_t unknown = PUYA;
    unknown.id = 0xC84014u;
    chip_use(&unknown);
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
    uint32_t e0 = (uint32_t)chip.erases;
    m2k_init(1);
    store_save();
    check(!flash_ok && chip.erases == (int)e0 && !chip.progs, "an unknown chip: the store writes nothing (saves stay in RAM)");
}

/* the store saved, the power cut (the core from scratch), loaded again; through the real driver */
static uint8_t name_root(const char *s) { unsigned h = 0; while (*s) h = h * 31u + (uint8_t)*s++; return (uint8_t)(24 + h % 72); }
static void mark(const char *pn, const char *sn)
{
    memset(PAT(0)->name, 0, 8); memcpy(PAT(0)->name, pn, strlen(pn));
    memset(snd[5].name, 0, 8); memcpy(snd[5].name, sn, strlen(sn));
    snd_root[5] = name_root(sn);
}
static int marked(const char *pn, const char *sn)
{
    return !strcmp(PAT(0)->name, pn) && !strcmp(snd[5].name, sn) && snd_root[5] == name_root(sn);
}
static void store_tests(const model_t *m)
{
    char what[160];
    chip_use(m);
    memset(chip.mem + BANK_AT, 0xFF, BANK_LEN);
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
    m2k_init(1);
    mark("CHIPA", "SNDA");
    uint32_t f0 = saves_failed;
    chip.fed = chip.now;
    chip.wdt_armed = 1;
    int wdt = 0;
    if (setjmp(chip.wdt)) wdt = 1;
    else store_save();
    chip.wdt_armed = 0;
    m2k_init(1);                                             /* the power cut: the factory's bank in RAM */
    int fresh = !marked("CHIPA", "SNDA");
    int loaded = store_load() && marked("CHIPA", "SNDA");
    snprintf(what, sizeof what, "%s: saved, the power cut, loaded: the patterns and the sounds back (%d erases, %d pages)",
             m->name, chip.erases, chip.progs);
    check(flash_ok && !wdt && saves_failed == f0 && fresh && loaded && clean(), what);
    printf("     the watchdog's longest wait %.2f s, the interrupts off %.2f s at most\n", (double)chip.fed_gap_max / 24e6,
           (double)chip.irq_off_max / 24e6);
}

/* a package (.fwsc) read whole, as the installer serves it */
static uint8_t *slurp(const char *path, uint32_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long k = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)k);
    if (b && fread(b, 1, (size_t)k, f) != (size_t)k) { free(b); b = 0; }
    fclose(f);
    if (!b || k < 20 * 0x30) return 0;
    for (int i = 1; i < 20; i++)                             /* the image the device reads: without the 20 marker bytes */
        memmove(b + i * 0x2F, b + i * 0x30, 0x2F);           /* (byte 0x2F of each 0x30 block: web/fm1/fm1pkg.js */
    memmove(b + 20 * 0x2F, b + 20 * 0x30, (size_t)k - 20 * 0x30);   /* logicalImage) */
    *n = (uint32_t)k - 20u;
    return b;
}

/* OLD installed, a store in it; NEW served: the app stages its loader, the loader installs it */
static void update_test(const model_t *m, const uint8_t *old, uint32_t old_n, const uint8_t *nu, uint32_t nu_n)
{
    static uint8_t hdr[0x400];
    static uint8_t store_before[BANK_LEN];
    uint32_t fo, fl, oo, ol, crc, fo2, fl2;
    char what[200];
    chip_use(m);
    memset(chip.mem, 0xFF, sizeof chip.mem);
    pkg = old; pkg_n = old_n;
    if (ota_ufw(hdr, &fo, &fl, &oo, &ol, &crc) || fl > sizeof chip.mem) { check(0, "the old package reads"); return; }
    memcpy(chip.mem, old + fo, fl);                          /* the old firmware, as installed */
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
    if (flash_ok) {                                          /* a store the player made with it */
        m2k_init(1);
        mark("KEEPA", "KEEPS");
        store_save();
    } else memset(chip.mem + BANK_AT, 0x5A, 64);             /* (something there all the same) */
    memcpy(store_before, chip.mem + BANK_AT, BANK_LEN);

    pkg = nu; pkg_n = nu_n;
    if (ota_ufw(hdr, &fo2, &fl2, &oo, &ol, &crc)) { check(0, "the new package reads"); return; }
    chip.fed = chip.now;
    chip.wdt_armed = 1;
    int wdt = 0, st = -99, lr = -99;
    uint64_t gap_stage = 0;
    committed = 0;
    req_last = req_gap_max = 0;
    if (setjmp(chip.wdt)) wdt = 1;
    else {
        if (flash_ok) st = ota_session();                    /* (platform.c and recovery.c: only on a chip it knows) */
        gap_stage = req_gap_max;
        req_last = req_gap_max = 0;
        ota_deadline = 0;                                    /* (the loader starts in fresh RAM: no session deadline) */
        lr = ldr_session();                                  /* the loader the SPL would start */
        flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());      /* then the new app's boot (platform.c) */
        if (flash_ok) ota_boot_cleanup();
    }
    chip.wdt_armed = 0;
    int app = !memcmp(chip.mem + 0x4000, nu + fo2 + 0x4000, 0x93000 - 0x4000);
    int app_old = !memcmp(chip.mem + 0x4000, old + fo + 0x4000, 0x93000 - 0x4000);
    int head = !memcmp(chip.mem, old + fo, 0x4000);
    int rec = 1;
    for (uint32_t i = 0xE0000u; i < 0xE5000u; i++) rec &= chip.mem[i] == 0xFF;
    int kept = !memcmp(chip.mem + BANK_AT, store_before, BANK_LEN);
    if (FL_JEDEC_OK(m->id)) {
        m2k_init(1);
        int back = store_load() && marked("KEEPA", "KEEPS");
        snprintf(what, sizeof what, "%s: staged (%d), installed (%d): the new app area, the head untouched, the record gone, "
                 "the store kept and loaded", m->name, st, lr);
        check(!wdt && st == 0 && committed && lr == 0 && app && head && rec && kept && back && clean(), what);
        printf("     the host waited %.1f s at most while the loader was staged (web/fm1/fm1ota.js gives up after 8 s), "
               "%.1f s while it installed (180 s)\n", (double)gap_stage / 24e6, (double)req_gap_max / 24e6);
        check(gap_stage < US(8000000) && req_gap_max < US(180000000), "  the host never left waiting past its limits");
    } else {
        snprintf(what, sizeof what, "%s (id %06X): no staging, the loader refuses (%d): the old firmware stays, nothing written",
                 m->name, m->id, lr);
        check(!wdt && st == -99 && lr == LDR_EFLASH && app_old && head && kept && clean(), what);
    }
    printf("     the watchdog's longest wait %.2f s, the interrupts off %.2f s at most, %d erases, %d pages, %.0f s in all\n",
           (double)chip.fed_gap_max / 24e6, (double)chip.irq_off_max / 24e6, chip.erases, chip.progs, (double)chip.now / 24e6);
}

int main(int argc, char **argv)
{
    memset(chip.mem, 0xFF, sizeof chip.mem);
    driver_tests();
    store_tests(&PUYA);
    store_tests(&UC25);
    store_tests(&UC25_EDGE);
    if (argc > 2) {
        uint32_t on, nn;
        uint8_t *o = slurp(argv[1], &on), *n = slurp(argv[2], &nn);
        if (!o || !n) { printf("FAIL the packages (%s, %s) do not read\n", argv[1], argv[2]); return 1; }
        model_t unknown = PUYA;
        unknown.id = 0xC84014u; unknown.name = "an unknown chip";
        model_t once = UC25;
        once.every_e = 37; once.over_e_us = 2600000; once.every_p = 301; once.over_p_us = 70000;
        once.name = "UC25HQ80, an erase in 37 and a page in 301 overrunning";
        update_test(&PUYA, o, on, n, nn);
        update_test(&UC25, o, on, n, nn);
        update_test(&UC25_EDGE, o, on, n, nn);
        update_test(&once, o, on, n, nn);
        update_test(&unknown, o, on, n, nn);
    } else printf("     (no packages given: the update is not run)\n");
    printf("%s (%d failed)\n", fails ? "FLASH TEST FAILED" : "flash test passed", fails);
    return fails != 0;
}
