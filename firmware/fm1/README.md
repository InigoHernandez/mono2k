# MONO2K on the M-VAVE FM-1

The FM-1 build: MONO2K's core (`firmware/src`, the same code the browser and the host run) on the open FM-1
platform. `m2k_fm1.c` is MONO2K's own glue; everything else here is the platform, taken from CS-1 (which stands on
Dinghy, SLOOP and Felucca) with the few changes listed below.

## Build

```bash
.venv/bin/python tools/fm1/build.py            # build/fm1/mono2k.fwsc, identity FM-1_980
.venv/bin/python tools/fm1/build.py --diag     # build/fm1/mono2k-test.fwsc: a load meter along the screen's top
.venv/bin/python tools/fm1/build.py --release 0.1   # build/fm1/mono2k-0.1.fwsc, version 0.1, identity FM-1_980
```

It needs JieLi's Linux toolchain (clang 4.0.1 for pi32v2) in `~/.jieli/toolchain` (`JIELI_TOOLCHAIN`), the JieLi
AC79 SDK (`AC79NN_SDK_V1.2.1_2023-12-13`) in `~/fw-AC79_AIoT_SDK` (`AC79_SDK`; three of its files go into the
package) and, on a Mac, a container runtime that runs linux/amd64 (colima with Rosetta: `colima start --vm-type vz
--vz-rosetta`; `DOCKER_CONFIG` defaults to `~/.jieli/docker`). Install a package with the site's installer
(`web/installer.html`: Connect, then a package from this computer). `tools/fm1/gen_sense.py` makes
`gen/sense_tables.h`.

The build checks what Felucca's and CS-1's check: the entry at the XIP address, no code in RAM calling out of it, no
reference to the chip's ROM, no float or 64-bit division helper (the FM-1 has no FPU and the build no libgcc), no
64-bit multiply beside a load (the CPU's fault, felucca-bsp docs/CPU.md: the core's are out of line, `M2K_MUL_OUT`), and
the sizes: the image within the app slot (581 KB), `.data + .bss` within 96 KiB, the pool within 336 KiB.

## How it runs

- Two cores: each 16-sample block the core hands tracks [split, 6) to the second (CS-1's audio worker, its loop in
  RAM) and renders the rest, then the bus; the split keeps every FX machine with the track it hears and shares the
  sounding tracks out. `DUAL=0` in the environment builds one core.
- The core runs in the main loop alone, as it runs in the browser's worklet. The loop renders the sound ahead into a
  ring (six blocks of 128 frames, 17 ms) and the audio interrupt only copies it out, so no interrupt lands inside an
  edit. A pass renders at most half the ring, so a core slower than the sound stutters but never starves the panel
  or the watchdog.
- The screen: the core's 1-bit frame, ink and paper as RGB565, sent in 8-line strips where it changed, one strip a
  pass while the audio renders (a whole screen is 77 ms of SPI at 12 MHz).
- The panel: the platform's buttons, encoders and 27 keys mapped onto the core's (`m2k.h`); the LEDs on from a
  quarter of the core's brightness (the FM-1's are on or off). MASTER is the platform's output level.
- MIDI in: USB-MIDI and the TRS jack, into `m2k_midi_from`: one ring, the jack's messages on cable 1 (`MIDI_TRS`,
  usb.c; USB's forced to cable 0), so GLO > MIDI's SYIN and GLO > IN's NOTE take each from either or both, and THRU
  sends the jack's out over USB.
- The store: a stream (`m2k_fw_store_put` / `_get`) into the last 128 KiB of the data area, `0xC0000`..`0xE0000`,
  a 4 KiB sector at a time, its header (magic, length, CRC-32) last, so a save cut short reads as none. SAVE marks it;
  it is written when the sequencer stands (a sector's erase stops the sound ~50 ms). That region is part of the
  area Felucca keeps its projects and samples in: a save of MONO2K's overwrites whatever another firmware kept there.
- The link (`m2k_link.c`, with the store): the site's Patch Editor reads the store and writes one back over USB-MIDI
  SysEx, `F0 7D 'M' '2' 'K' cmd pack7(body) F7`, a request at a time, each answered with a status first: HELLO (the
  link's version, the counts, the name), INFO (saved first: the store's length and CRC-32), READ (128 bytes a reply),
  BEGIN / WRITE (256 bytes a request, into the store's place) / END (the header once the length and CRC agree, then
  the core reads it as at boot). A write refused, cut or stalled (4 s) writes the FM-1's own state back. Only while
  the sequencer stands. `host/m2k_link_test.c` runs it on a flash in memory, with the browser's store for the FM-1
  and the FM-1's read back in the browser (`tools/check.sh`).
- The update entry, the USB rescue (hold OCT− at power-on) and the boot guard (two failed boots: the rescue) are the
  platform's: an installer reaches the FM-1 whatever the app does.

## What the build leaves out

- One project of 64 patterns in place of four: the core keeps two patterns unpacked (3.5 KB each) and the rest packed
  in a 46 KB working image beside a 46 KB saved one, a blank pattern taking no bytes, so bank A's demos and some 20
  to 30 patterns of one's own fit; a pattern that would not leave room opens no more, and says so. The counts are
  build settings (`M2K_PATTERNS`, `M2K_PAT_SLOTS`, `M2K_PROJECTS`, `M2K_IMG_DATA` in `tools/fm1/build.py`).
- Key velocity (MONO2K plays every note alike), USB audio stays as the platform has it.
- MASTER is the FM-1's level pot, not an encoder: a held step's trig type (GRID, a step held, MASTER on the web) is
  set in the browser for now.
- A save cut short by the power (the header is written last, but the sectors before it are erased as it goes) leaves
  no store: the FM-1 starts on the factory's bank.

## Changed from the platform

- `hal/fm1_lcd_hw.h` `fm1_lcd_done`, `src/lcd.c` `lcd_idle`: a transfer's end without waiting for it.
- `src/platform.c`: `BANK_AT` / `BANK_LEN` and `CV_MAX` can be set by the app; an app's SysEx hook (`APP_SYSEX`)
  sees each frame before the update entry does.
- `src/sense.c`: its tables from `gen/sense_tables.h` (generated here) in place of CS-1's.

## Whose is what

GPL-3.0-only, as MONO2K. The platform: Leo Kuroshita, Hügelton Instruments
([Felucca](https://github.com/hugelton/Felucca)), with [SLOOP](https://github.com/isod89/sloop-fm1)'s changes (isod89);
the wiring `src/platform.c`: zvenson ([Dinghy](https://github.com/zvenson/dinghy)); the second core as an audio worker
(`hal/fm1_multicore.h`, `src/audio_worker.h`): PatttF ([FM-106](https://github.com/patttf/fm-106)); MIDI IN on TRS
(`src/midi_uart.c`): Felucca, as FM-106 adapted it; the keys' sensing (`src/sense.c`, the analog rows in
`hal/fm1_input.h`, `fm1_adc.h`): Charles Vestal ([fm1-velocity](https://github.com/charlesvestal/fm1-velocity)); as
gathered in [CS-1](https://github.com/twochannels/CS-1) (twochannels), whose licensing notes are kept in
`LICENSES/CS-1-LICENSING.md`. The package builder `tools/fm1/fm1pkg_make.py` and `lz4blk.py`: Felucca's, unchanged;
`tools/fm1/build.py` follows CS-1's. Third party: the Terminus font (`gen/felucca_font.h`, SIL OFL 1.1,
`LICENSES/Terminus-OFL-1.1.txt`); the JieLi AC79 SDK's `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`, read from the
SDK at build time into the package (Apache-2.0, `LICENSES/Apache-2.0.txt`; none of them are in this tree).

M-VAVE and FM-1 are their owners' marks; MONO2K is not made or endorsed by M-VAVE, nor by Elektron.
