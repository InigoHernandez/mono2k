/* The FM-1 Firmware Hub's pictures (docs/hub/): the engine's own frames in dark mode while A01 CIRCUIT plays.
 * Writes DIR/home.pbm (HOME, a few bars in), DIR/chord.pbm (the chord machine's SYN page) and DIR/f%03d.pbm, the
 * animation's frames (four bars on HOME, then the chord machine's page; 80 ms a frame); tools/hub_pics.py makes the
 * PNGs and GIFs from them.
 *   cc -O2 -w -Ihost -Ifirmware/src -Ifirmware/gen -o build/host/hub_pics tools/hub_pics.c -lm && build/host/hub_pics DIR */
#define main host_main
#include "m2k_host.c"
#undef main

static void save(const char *dir, const char *name)
{
    char p[600];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *o = fopen(p, "wb");
    if (!o) { perror(p); exit(1); }
    fprintf(o, "P4\n240 240\n");
    fwrite(m2k_frame(ux_now), 1, M2K_FB_BYTES, o);
    fclose(o);
}

/* the engine run for ms, the sound rendered alongside so the sequencer moves as it plays */
static void run(int ms) { for (int t = 0; t < ms; t += 20) { ux_audio(M2K_SR / 50); ux_ms(20); } }

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: hub_pics DIR\n"); return 1; }
    ux_init(2000);
    m2k_setting(S_SCREEN, 127);                            /* the screen dark (SET > SCRN DARK), as the hub showed it */
    m2k_pattern(0);                                        /* A01 CIRCUIT */
    int chord = 0;
    for (int t = 0; t < M2K_TRACKS; t++) if (PAT(0)->machine[t] == M_ENS) { chord = t; break; }
    ui.trk = 0; sq.ktrk = 0;
    ui.page = PG_HOME; ui.sub = 0;
    seq_play(1);
    char n[32];
    int f = 0;
    for (int ms = 0; ms < 8000; ms += 80) {                /* four bars of A01 at 126 BPM on HOME */
        run(80);
        snprintf(n, sizeof n, "f%03d.pbm", f++); save(argv[1], n);
        if (ms == 2640) save(argv[1], "home.pbm");    /* (between beats: the header flashes on each) */
    }
    ui.trk = (uint8_t)chord; sq.ktrk = (uint8_t)chord;     /* then the chord machine's page */
    ui.page = PG_SYN; ui.sub = 0;
    for (int ms = 0; ms < 4500; ms += 80) {
        run(80);
        snprintf(n, sizeof n, "f%03d.pbm", f++); save(argv[1], n);
        if (ms == 1840) save(argv[1], "chord.pbm");
    }
    printf("%d frames, the chord machine on T%d\n", f, chord + 1);
    return 0;
}
