#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Grids' rules as its original firmware has them (Emilie Gillet's grids/pattern_generator.cc, GPL-3.0): ReadDrumMap's
# mix of the four nodes round X and Y (U8Mix, avrlib's op.h), EvaluateDrums' perturbation (a random byte times
# randomness >> 2, over 256, drawn as a cycle starts), its clipping, the density's threshold (~density) and the accent
# (past 192). The nodes are read from firmware/src/m2k_grids.c (the port's data), the random byte is MONO2K's (the
# cycle's and the part's hash, m2k_grids.c's grids_pert): this checks the port's rules, written apart from it. It
# prints the self-test's expected trigs and accents (host/m2k_host.c, "Grids:"), a case a line:
#   python3 tools/ref/grids_ref.py
import os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
src = open(os.path.join(ROOT, "firmware/src/m2k_grids.c")).read()
body = src[src.index("GR_NODE[25][96] = {"):src.index("GR_MAP[5][5]")]
NODE = []
for blk in re.findall(r"\{\s*/\* node (\d+) \*/(.*?)\}", body, re.S):
    NODE.append([int(x) for x in re.findall(r"\d+", blk[1])])
assert len(NODE) == 25 and all(len(n) == 96 for n in NODE)
DRUM_MAP = [[10, 8, 0, 9, 11], [15, 7, 13, 12, 6], [18, 14, 4, 5, 3], [23, 16, 21, 1, 2], [24, 19, 17, 20, 22]]

def u8mix(a, b, balance):                      # (a * (255 - balance) + b * balance) >> 8, in uint8_t
    return ((a * (255 - balance) + b * balance) >> 8) & 255

def read_drum_map(step, instrument, x, y):
    i, j = x >> 6, y >> 6
    a_map, b_map = NODE[DRUM_MAP[i][j]], NODE[DRUM_MAP[i + 1][j]]
    c_map, d_map = NODE[DRUM_MAP[i][j + 1]], NODE[DRUM_MAP[i + 1][j + 1]]
    off = instrument * 32 + step
    xb, yb = (x << 2) & 255, (y << 2) & 255   # x << 2 passed as a uint8_t
    return u8mix(u8mix(a_map[off], b_map[off], xb), u8mix(c_map[off], d_map[off], xb), yb)

def random_byte(cycle, part):                  # MONO2K's: grids_pert's hash
    m = 0xFFFFFFFF
    h = (cycle * 0x9E3779B1 + part * 0x85EBCA77 + 0x2545F491) & m
    h ^= h >> 16; h = (h * 0x7FEB352D) & m; h ^= h >> 15; h = (h * 0x846CA68B) & m; h ^= h >> 16
    return h >> 24

def evaluate(step, cycle, x, y, randomness, density):
    """each part's (trig, accent) at a step, EvaluateDrums' way"""
    out = []
    for i in range(3):
        pert = (random_byte(cycle, i) * (randomness >> 2)) >> 8      # U8U8MulShift8
        level = read_drum_map(step, i, x, y)
        level = level + pert if level < 255 - pert else 255
        threshold = (~density[i]) & 255
        trig = level > threshold
        out.append((trig, trig and level > 192))
    return out

def k8(v):                                     # a MONO2K knob's 0..127 on Grids' 0..255
    return v * 255 // 127

# (X, Y, CHAOS, BD SD HH density, cycle of 16 or 32): knob values
CASES = [(0, 0, 0, (64, 64, 64), 16), (64, 64, 0, (64, 64, 64), 16), (100, 30, 0, (100, 40, 127), 16),
         (37, 90, 127, (80, 80, 80), 16), (127, 127, 60, (127, 127, 127), 32), (20, 110, 100, (50, 90, 110), 32)]

if __name__ == "__main__":
    for x, y, ch, dens, cyc in CASES:
        trig, acc = [0, 0, 0], [0, 0, 0]
        for k in range(32):                    # 32 of the track's steps: two cycles of 16, one of 32
            step = (k % 16) * 2 if cyc == 16 else k % 32
            for p, (t, a) in enumerate(evaluate(step, k // cyc, k8(x), k8(y), k8(ch), [k8(d) for d in dens])):
                trig[p] |= t << k
                acc[p] |= a << k
        print("        {%d, %d, %d, {%d, %d, %d}, %d, {0x%08xu, 0x%08xu, 0x%08xu}, {0x%08xu, 0x%08xu, 0x%08xu}}," %
              ((x, y, ch) + dens + (cyc,) + tuple(trig) + tuple(acc)))
