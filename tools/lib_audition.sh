#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The sound library's audition, whole, from the repo's root (a redo is this one command): sh tools/lib_audition.sh [DIR]
#   the host and the cost counter built (the counter as the FM-1 builds the core: a Mac's instruction count), every
#   library sound rendered in its phrase (m2k_host DIR --library), its cost counted (m2k_draw --library), each measured
#   and made small to listen to (tools/lib_audition.py): DIR/index.json, its WAVs and MP3s (or AAC without ffmpeg)
set -e
D=${1:-build/lib_audition}
mkdir -p build/host "$D"
cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm
cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen \
    -o build/host/m2k_draw host/m2k_draw.c
build/host/m2k_host "$D" --library > "$D/render.txt" || { grep -E "OVER|CLIPS" "$D/render.txt"; exit 1; }
build/host/m2k_draw --library "$D" > "$D/cpu.txt"
.venv/bin/python tools/lib_audition.py "$D"
