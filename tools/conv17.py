#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The bus's conversions for pattern format 17, by measurement: each old value to the new one that sounds nearest.
# The old engine (f1f2708) was measured into tools/ref/bus_v16.json; today's is measured here; the result goes to
# tools/ref/conv17.json, which tools/gen_tables.py makes T_V17_RV and T_V17_GLCH from.
#   The reverb (the old SPU-style combs to today's feedback delay network): the old one on a 9 x 9 grid (DCAY 1, 16 ..
#   112, 127 by DAMP 0, 16 .. 112, 127) and at bank A's pairs; today's (m2k_host --verb-grid) at every other DCAY and
#   every fourth DAMP. A measure: the RT60 of the whole and of the octaves at 250 Hz .. 4 kHz (interrupted noise), the
#   impulse response's energy in 177 Hz .. 5.7 kHz, in all and in each octave. Nearest: the octaves' RT60s at 250, 500, 1k and 2k
#   (weights 1, 0.7, 1, 0.4) on a log scale, each held up to 0.22 s (under it the old reverb's octaves are its input's
#   smear, not a tail); the level (each send's) the mean difference in the energy of the octaves at 250, 500 and 1k (where
#   the parts' sounds are: the old reverb's top was its first pass, the new one's early part darker).
#   GLCH (m2k_host --glch-curve BARS, the reference mix): the glitch's energy (its output less GLCH 0's) and its events
#   a bar, both curves smoothed over five values; nearest by (dE / 6 dB)^2 + (log2 of the events' ratio)^2: 6 dB of
#   glitch weighs as twice as many events (the old low values were rare and loud, the new ones rare and quiet: neither
#   alone is the old sound), kept rising.
#     .venv/bin/python tools/conv17.py [--cached] [--bars N]
import json, math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
OLD = os.path.join(HERE, "ref", "bus_v16.json")
OUT = os.path.join(HERE, "ref", "conv17.json")
CACHE_V, CACHE_G = os.path.join(ROOT, "build", "verb_grid.txt"), os.path.join(ROOT, "build", "glch_curve.txt")
NV = [1, 16, 32, 48, 64, 80, 96, 112, 127]
ND = [0, 16, 32, 48, 64, 80, 96, 112, 127]
FLOOR = 0.22
W = [1.0, 0.7, 1.0, 0.4]
HOST = os.path.join(ROOT, "build/host/m2k_host")


def rt60(v):
    return 0.3 * 80 ** ((v - 1) / 126.0)


def level(o, n):
    """what the new reverb's sends make up: the old impulse response's energy against the new one's, its octaves at 250,
    500 and 1k on average (dB)"""
    return sum(o[k] - n[k] for k in (8, 9, 10)) / 3


def dist(a, b):
    return sum(w * math.log(max(x, FLOOR) / max(y, FLOOR)) ** 2 for w, x, y in zip(W, a[1:5], b[1:5]))


def run(args, cache, stdin=None):
    if "--cached" in sys.argv and os.path.exists(cache):
        return open(cache).read()
    text = subprocess.run([HOST] + args, cwd=ROOT, input=stdin, text=True, capture_output=True, check=True).stdout
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    open(cache, "w").write(text)
    return text


def verb(old):
    lines = "".join(f"{v} {d} {rt60(v) * 0.6 + 1.5:.2f}\n" for v in range(1, 128, 2) for d in list(range(0, 128, 4)) + [127])
    new = {}
    for l in run(["--verb-grid"], CACHE_V, lines).splitlines():
        c = l.split()
        new[(int(c[0]), int(c[1]))] = [float(x) for x in c[2:]]
    best = lambda o: min(new, key=lambda k: (dist(new[k], o), k[1], k[0]))   # ties: the brighter, the shorter
    nodes, worst = [], 0.0
    for v in NV:
        for d in ND:
            o = old[(v, d)]
            b = best(o)
            nodes.append([b[0], b[1], round(level(o, new[b]), 2)])
            worst = max(worst, dist(new[b], o))
    points = {}
    for k in sorted(old):
        if k[0] in NV and k[1] in ND:
            continue
        b = best(old[k])
        points[f"{k[0]},{k[1]}"] = [b[0], b[1], round(level(old[k], new[b]), 2), round(dist(new[b], old[k]), 4)]
    print("the reverb: new DCAY, DAMP and level (dB) at old DCAY (rows) by old DAMP 0 16 .. 127 (columns)")
    for i, v in enumerate(NV):
        print(f"{v:4d}: " + "  ".join(f"{n[0]:3d} {n[1]:3d} {n[2]:+5.1f}" for n in nodes[i * 9:i * 9 + 9]))
    for k, p in points.items():
        print(f"  old {k}: new {p[0]} {p[1]}, level {p[2]:+.1f} dB (distance {p[3]})")
    print(f"  the worst node's distance {worst:.3f}")
    return {"verb": NV, "damp": ND, "nodes": nodes, "points": points}


def smooth(c):
    """over five values, on a log scale (the events) and in dB (the energy); 0 stays 0"""
    out = {}
    for v in range(1, 128):
        win = [c[u] for u in range(max(1, v - 2), min(127, v + 2) + 1)]
        out[v] = (math.exp(sum(math.log(max(r, 1e-4)) for r, _ in win) / len(win)), sum(e for _, e in win) / len(win))
    return out


def glch(old_rows):
    bars = sys.argv[sys.argv.index("--bars") + 1] if "--bars" in sys.argv else "256"
    new_rows = {}
    for l in run(["--glch-curve", bars], CACHE_G).splitlines():
        c = l.split()
        new_rows[int(c[0])] = [float(x) for x in c[1:]]
    o = smooth({v: (old_rows[v][0], old_rows[v][2]) for v in range(1, 128)})
    n = smooth({v: (new_rows[v][0], new_rows[v][2]) for v in range(1, 128)})
    d = lambda a, b: ((a[1] - b[1]) / 6) ** 2 + math.log2(a[0] / b[0]) ** 2
    conv, lo = [0], 1
    for v in range(1, 128):
        b = min(range(lo, 128), key=lambda u: (d(n[u], o[v]), u))
        conv.append(b)
        lo = b
    print("GLCH: old -> new (old events a bar, energy dB -> new)")
    for v in (1, 5, 10, 15, 20, 30, 40, 50, 60, 64, 70, 80, 90, 100, 110, 120, 127):
        print(f"  {v:3d} ({old_rows[v][0]:.3f}, {old_rows[v][2]:+6.1f}) -> {conv[v]:3d} ({new_rows[conv[v]][0]:.3f}, {new_rows[conv[v]][2]:+6.1f})")
    return conv


def main():
    subprocess.run(["cc", "-O2", "-Wno-unused-function", "-Ifirmware/src", "-Ifirmware/gen", "-o", "build/host/m2k_host",
                    "host/m2k_host.c", "-lm"], cwd=ROOT, check=True)
    ref = json.load(open(OLD))
    old_v = {tuple(int(x) for x in k.split(",")): v for k, v in ref["VERB"].items()}
    old_g = {int(k): v for k, v in ref["GLCH"].items()}
    out = verb(old_v)
    out["glch"] = glch(old_g)
    json.dump(out, open(OUT, "w"), indent=0)
    print(f"into {os.path.relpath(OUT, ROOT)}")


if __name__ == "__main__":
    main()
