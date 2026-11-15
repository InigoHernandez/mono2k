/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's update screen (firmware/fm1/src/upd_screen.c) on a computer: the same code drawn into a 240 x 240 panel
 * in memory, each state saved as a PPM (upd_screen_draw DIR). lcd.c's three calls stand in for the panel; nothing
 * else of the platform is needed. tools/fm1/upd_screens.sh turns them into PNGs. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint16_t panel[240 * 240];
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    for (uint32_t j = y; j < y + h && j < 240; j++)
        for (uint32_t i = x; i < x + w && i < 240; i++) panel[j * 240 + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            if (x + i < 240 && y + j < 240) {
                uint16_t c = px[j * w + i];
                panel[(y + j) * 240 + x + i] = (uint16_t)(c >> 8 | c << 8);   /* the panel takes big-endian */
            }
}
#include "../firmware/fm1/src/upd_screen.c"

static const char *dir = ".";
static void save(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n240 240\n255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = panel[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)((c >> 5 & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    uint32_t t = 0;
    if (argc > 1) dir = argv[1];
    us.ok = 1;
    /* the app, staging the loader */
    us_begin("UPDATE");
    us_bottom("3/5", "LOADER");
    save("1-app-staging");
    /* the loader: waiting for an installer, the scan a few cells on */
    us_begin("1.9.3");
    us_wait();
    for (int i = 0; i < 18; i++) us_tick(t += 80);
    save("2-loader-waiting");
    /* checking, then writing */
    us_check();
    save("3-loader-checking");
    for (uint32_t n = 0; n <= 67; n++) us_write(n, 143);
    save("4-loader-writing");
    us_tick(t += 200);
    save("5-loader-writing-blink");
    for (uint32_t n = 68; n <= 143; n++) us_write(n, 143);
    us_done();
    for (int i = 0; i < 12; i++) us_tick(t += 30);
    save("6a-loader-done-wave");
    for (int i = 0; i < 20; i++) us_tick(t += 30);
    save("6-loader-done");
    /* stopped half way, and a chip it does not know */
    us_begin("1.9.3");
    for (uint32_t n = 0; n <= 67; n++) us_write(n, 143);
    us_stop("STOPPED", "PRESS INSTALL AGAIN", "STOPPED");
    save("7-loader-stopped");
    us_begin("1.9.3");
    us_wait();
    us_check();
    us_stop("FLASH ??", "OLD FIRMWARE AGAIN", "B36015");
    save("8-loader-unknown-chip");
    us_begin("UPDATE");
    us_bottom("3/5", "LOADER");
    us_stop("FAILED -7", "NOTHING CHANGED", "FAILED");
    save("9-app-failed");
    return 0;
}
