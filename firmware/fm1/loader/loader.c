/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader (device side). Runs from RAM, started by the SPL from
 * an update record, with interrupts off: USB is polled from the main loop.
 * It is a USB-MIDI device "Felucca Update" (1209:0002) that answers the
 * M-UPGRADE update protocol as "ota-FM-1_900" (hosts look for "ota-"), and
 * writes the app area only (ldr_core.c). The UBOOT soft key works here too.
 * MONO2K: the panel shows the write (src/upd_screen.c): the app area's sectors filling, waiting for an installer
 * when there is none, and why it stopped. A panel that does not answer is left alone.
 * Single compilation unit. */
#include <stdint.h>
#define FELUCCA_LOADER 1
#define FELUCCA_CDC 0
#define FELUCCA_OTA 1
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_USB_PID 0x0002
#define FELUCCA_ID "ota-FM-1_900"
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "fm1_time.h"
#include "fm1_sys.h"
/* the loader may write the app area and erase update records; never the head */
#define FL_RANGE_OK(off, n) (FL_IN(off, n, 0x4000u, 0x93000u) || FL_IN(off, n, 0x93000u, 0xFC000u))
#include "fm1_flash.h"
#include "../src/libc.c"
#include "../src/usb.c"
#include "fm1_lcd_hw.h"
#include "../src/lcd.c"
#ifndef APP_VER
#define APP_VER "UPDATE"
#endif
#include "../src/upd_screen.c"

static uint32_t ldr_ms(void)   /* monotonic ms; survives the TIMER4 wrap (178.9 s) */
{
    static uint32_t last, acc, ms;
    uint32_t now = fm1_ticks();
    acc += now - last;
    last = now;
    ms += acc / (1000u * FM1_TICKS_PER_US);
    acc %= 1000u * FM1_TICKS_PER_US;
    return ms;
}

static void ldr_poll(void)                          /* USB at ~2 kHz, watchdog, the screen's blink */
{
    static uint32_t last;
    uint32_t now = fm1_ticks();
    fm1_wdt_feed();
    if (now - last >= 500u * FM1_TICKS_PER_US) {
        last = now;
        usb_poll();
        us_tick(ldr_ms());
    }
}

/* the panel, before USB (its set-up holds the CPU ~0.4 s): one command first, and if that times out nothing more
 * (a full-screen fill on a silent SPI would wait out the watchdog) */
static void ldr_screen(void)
{
    fm1_lcd_hw_init();
    lcd_cmd(0x00);                                  /* NOP */
    fm1_wdt_feed();
    if (fm1_lcd_timeouts)
        return;
    lcd_init();
    fm1_wdt_feed();
    us.ok = fm1_lcd_timeouts == 0;
    us_begin(APP_VER);
    us_wait();
}

static void ldr_hex6(char *t, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 6u; i++)
        t[i] = "0123456789ABCDEF"[v >> (20u - 4u * i) & 15u];
    t[6] = 0;
}

/* ---- ota.c hooks (the SysEx frame hooks are in usb.c) ---- */
static uint32_t ota_now_ms(void) { return ldr_ms(); }
static void ota_idle(void) { ldr_poll(); }
/* step-1 hooks of ota.c: not used by the loader */
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { return fl_read_ram(off, p, n); }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../src/ota.c"

/* ---- ldr_core.c hooks: interrupts are off for the whole loader ---- */
static int ldr_fread(uint32_t off, void *p, uint32_t n) { return fl_read_ram(off, p, n); }
static int ldr_erase(uint32_t off)              /* (fed first: a slow chip's erase, reset and retry take ~4 s) */
{
    uint32_t took;
    fm1_wdt_feed();
    return fl_erase4k_ram(off, &took);
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)
{
    uint32_t took;
    fm1_wdt_feed();
    return fl_prog_ram(off, p, n, &took);
}
static void ldr_record_clear(void)
{
    fm1_updata_parm_clear();                        /* CRC 0: the SPL ignores it on a warm reset */
}
static int ldr_flash_known(void) { return FL_JEDEC_OK(fl_jedec_ram()); }   /* as the app (platform.c) */
static void ldr_progress(uint32_t done, uint32_t total)
{
    us_write(done / 0x1000u, total / 0x1000u);
    ldr_poll();
}
#include "ldr_core.c"

extern uint32_t _bss_start[], _bss_end[];

void ldr_main(void)
{
    fm1_wdt_arm(0x0D);                              /* ~8 s, fed by ldr_poll */
    fm1_time_init();
    ldr_screen();
    usb_start();
    for (;;) {
        ldr_poll();
        usb_retry(ldr_ms());
        if (usb.uboot_req) {                        /* soft key: mask-ROM UBOOT, as in Felucca */
            usb_detach();
            fm1_delay_ms(30);
            fm1_enter_uboot();
        }
        ota_service();                              /* handshake */
        if (usb.ota_req) {
            int rc;
            usb.ota_req = 0;
            us_check();
            rc = ldr_session();
            if (rc == 0) {                          /* MONO2K's isotype in the grid, a moment */
                uint32_t t0 = ldr_ms();
                us_done();
                while (ldr_ms() - t0 < 900u)
                    ldr_poll();
            }
            else if (rc == LDR_EFLASH) {            /* a moment to read it, then the old firmware */
                char id[7];
                uint32_t t0 = ldr_ms();
                ldr_hex6(id, fl_jedec_ram());
                us_stop("FLASH ??", "OLD FIRMWARE AGAIN", id);
                while (ldr_ms() - t0 < 3000u)
                    ldr_poll();
            } else                                  /* the record stays: the installer can finish it */
                us_stop("STOPPED", "PRESS INSTALL AGAIN", "STOPPED");
            if (rc == 0 || rc == LDR_EFLASH) {      /* done, or another flash chip: the old firmware again */
                fm1_delay_ms(200);                  /* the "success" reply leaves */
                usb_detach();
                fm1_delay_ms(30);
                fm1_core_reset();                   /* the SPL boots the new app */
            }
        }
    }
}

void ldr_cstart(void)
{
    uint32_t *p;
    ldr_record_clear();                             /* first thing: a crash below cannot loop through a
                                                     * warm reset; resume after power loss uses the flash record */
    for (p = _bss_start; p < _bss_end; p++)
        *p = 0;
    ldr_main();
}
