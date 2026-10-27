#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The bus's controls (GLCH HISS ADPC DRIV TAPE DCAY DAMP), every value, measured on the engine (host/m2k_host DIR --bus):
#   GLCH  repeats started per bar, the share of the time one sounds (repeats of a sixteenth, at 120), and the glitch's
#         energy on the reference mix (dB against it; format 17's law: quiet and rare low, wild at the top)
#   HISS  the hiss's level against the reference mix (dB; its rms away from the clicks; 12 s), the crackle's rate (/s)
#         and level (the median click's peak, dBFS). The hiss is a one-pole lowpass of a bounded white noise, so
#         whitening the output leaves the hiss inside a bound its gain sets and the crackle outside it: counted where
#         the hiss leaves the crackle room, not counted (None) where it buries it
#   ADPC  the error it adds (dB under the mix); DRIV the gain and the distortion (dB under the clean part)
#   TAPE  wow and flutter (cents, a held sine), the loss at 8.4 kHz (dB)
#   DCAY  the reverb's RT60 (the whole; at 250 Hz, 1 kHz, 4 kHz) and its energy (177 Hz .. 5.7 kHz, dB) at DAMP 32;
#   DAMP  the same at DCAY 80 (format 17's reverb)
# Held to the record (tools/ref/bus_m2k.json) and, through the pattern format's conversions, to the engines before:
# T_V9_* to the one before format 9 (tools/ref/bus_v8.json, measured on 3e57c61), each old value and the new one it
# converts to measuring alike (GLCH against format 9..16's engine, tools/ref/bus_v16.json, as 9 made it); T_V17_GLCH
# to the one before format 17 (bus_v16.json, f1f2708): each old value to the one nearest by tools/conv17.py's
# measure (the glitch's energy, its events a bar); T_V17_RV as tools/conv17.py measured it (tools/ref/conv17.json).
# --json writes the curves for the page that shows them.
#   .venv/bin/python tools/bus_curves.py [--record] [--record-old DIR] [--json OUT] [--no-render]
import json, math, os, re, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(HERE, "ref"))
from sweep import SR

REF, OLD = os.path.join(HERE, "ref", "bus_m2k.json"), os.path.join(HERE, "ref", "bus_v8.json")
V16, C17 = os.path.join(HERE, "ref", "bus_v16.json"), os.path.join(HERE, "ref", "conv17.json")
A_HISS = 9000 / 32768                              # the hiss's one pole (m2k_fx.c)


def tables():
    h = open(os.path.join(ROOT, "firmware", "gen", "m2k_tables.h")).read()
    def tab(n):
        b = h[h.index(n + "["):]
        return [int(v) for v in re.findall(r"-?\d+", b[b.index("{") + 1:b.index("}")])]
    t = {n: tab(n) for n in ("T_HISS_G", "T_CRK_A", "T_V9_GLCH", "T_V9_HISS", "T_V9_DRIV", "T_V9_TAPE", "T_V9_ADPC",
                             "T_V17_GLCH")}
    b = h[h.index("T_V17_RV[81][2]"):]
    t["T_V17_RV"] = [int(v) for v in re.findall(r"-?\d+", b[b.index("{") + 1:b.index(";")])]
    return t


def hiss(path, gain, crk_min):
    """(hiss rms dBFS, crackles a second or None, their median peak dBFS or None)"""
    x = np.frombuffer(open(path, "rb").read()[44:], np.int16).astype(np.float64)   # the host's mono 16-bit file
    w = (x[1:] - (1 - A_HISS) * x[:-1]) / A_HISS
    bound = gain * 1.05 + 60
    idx = np.nonzero(np.abs(w) > bound)[0]
    groups = np.split(idx, np.nonzero(np.diff(idx) > 8)[0] + 1) if len(idx) else []
    quiet = np.ones(len(x), bool)                  # the hiss: the rms away from the clicks
    for g in groups:
        quiet[max(0, g[0] - 4):g[-1] + 12] = False
    rms = np.sqrt(np.mean(x[quiet] ** 2)) if quiet.any() else 0.0
    h = 20 * math.log10(rms / 32768) if rms > 0 else None
    if 2 * crk_min < bound * 1.3:                  # the crackle hides in the hiss
        return h, None, None
    peaks = [np.abs(x[g[0]:g[-1] + 3]).max() for g in groups]
    level = 20 * math.log10(np.median(peaks) / 32768) if peaks else None
    return h, len(peaks) / (len(x) / SR), level


def measure(d, old=False):
    t = tables()
    rows = {}
    for l in open(os.path.join(d, "bus.tsv")):
        c = l.split("\t")
        if c[0] == "ctl":
            continue
        rows.setdefault(c[0], {})[int(c[1])] = [float(x) for x in c[2:]]
    mix = rows["DRIV"][0][2]
    out = {"mix_rms": mix,
           "GLCH": {v: r[:2] + ([rows["GLCE"][v][0]] if "GLCE" in rows else []) for v, r in rows["GLCH"].items()},
           "ADPC": {v: [r[1], r[0]] for v, r in rows["ADPC"].items()},
           "DRIV": {v: r[:4] for v, r in rows["DRIV"].items()},
           "TAPE": {v: r[:3] for v, r in rows["TAPE"].items()},
           "HISS": {}}
    for ctl in ("DCAY", "DAMP"):
        if ctl in rows:
            out[ctl] = {v: r[:5] for v, r in rows[ctl].items()}
    for v in range(128):
        gain = v * v if old else t["T_HISS_G"][v]
        a = (16 + v) * 16 if old else t["T_CRK_A"][v]
        h, rate, lev = hiss(os.path.join(d, f"hiss_{v:03d}.wav"), gain, 1500 * a / 1024)
        out["HISS"][v] = [h - mix if v and h is not None else None, rate, lev]
    return out


def keyed(m):
    return {k: ({int(v): x for v, x in c.items()} if isinstance(c, dict) else c) for k, c in m.items()}


def check_record(new, ref):
    bad = []
    for ctl in ("GLCH", "ADPC", "DRIV", "TAPE", "HISS", "DCAY", "DAMP"):
        for v in range(1 if ctl == "DCAY" else 0, 128):
            if v not in ref.get(ctl, {}) or len(new[ctl][v]) != len(ref[ctl][v]):
                bad.append(f"{ctl} {v}: not in the record (--record)")
                continue
            for i, (a, b) in enumerate(zip(new[ctl][v], ref[ctl][v])):
                if (a is None) != (b is None) or (a is not None and abs(a - b) > 0.05):
                    bad.append(f"{ctl} {v} [{i}]: {b} -> {a}")
    return bad


def smooth(c):
    """over five values, on a log scale (the events) and in dB (the energy), as tools/conv17.py"""
    out = {}
    for v in range(1, 128):
        win = [c[u] for u in range(max(1, v - 2), min(127, v + 2) + 1)]
        out[v] = (math.exp(sum(math.log(max(r, 1e-4)) for r, _ in win) / len(win)), sum(e for _, e in win) / len(win))
    return out


def check_conversion(new, old, t, v16):
    """each old value against the new one the format converts it to"""
    bad = []
    # format 17's GLCH: each old value to the new one nearest by the glitch's energy and its events (conv17.py's
    # measure, its 512 bars against the 64 here: within their noise of the nearest)
    o = smooth({v: (v16["GLCH"][v][0], v16["GLCH"][v][2]) for v in range(1, 128)})
    n = smooth({v: (new["GLCH"][v][0], new["GLCH"][v][2]) for v in range(1, 128)})
    d = lambda a, b: ((a[1] - b[1]) / 6) ** 2 + math.log2(a[0] / b[0]) ** 2
    for v in range(1, 128):
        c = t["T_V17_GLCH"][v]
        best = min(d(n[u], o[v]) for u in range(1, 128))
        if d(n[c], o[v]) > 1.4 * best + 0.5:                # (64 bars here: the low values few events)
            bad.append(f"GLCH (17) {v} -> {c}: {d(n[c], o[v]):.2f} from the old, the nearest {best:.2f}")
    conv = json.load(open(C17))
    if conv["glch"] != t["T_V17_GLCH"] or [x for nd in conv["nodes"] for x in nd[:2]] != t["T_V17_RV"]:
        bad.append("T_V17_GLCH / T_V17_RV are not tools/ref/conv17.json's (tools/gen_tables.py)")
    for o in range(1, 128):
        v = t["T_V9_GLCH"][o]
        (s1, d1), (s0, d0) = v16["GLCH"][v][:2], old["GLCH"][o]
        n1, n0 = s1 * 256, s0 * 256
        if abs(n1 - n0) > 3 * math.sqrt(n0 + n1) + 3 or abs(d1 - d0) > 3 * max(d0, d1) * math.sqrt(2 / max(n0, 1)) + 0.3:
            bad.append(f"GLCH {o} -> {v}: {s0:.3f}/bar {d0:.1f} % -> {s1:.3f}/bar {d1:.1f} %")
        v = t["T_V9_DRIV"][o]
        g1, e1 = new["DRIV"][v][:2]
        g0, e0 = old["DRIV"][o][:2]
        # (since pattern format 16 the reference's tracks end their tails a few ms apart from the old engine's: the
        # free-running bass and the noise meet the next notes elsewhere, the mix's peak 1.3 dB lower, the gains the
        # same. Under -25 dB the residual reads those peaks more than the drive: there it may only not grow)
        if abs(g1 - g0) > 0.15 or (max(e0, e1) > -25 and abs(e1 - e0) > 1.5) or (max(e0, e1) > -70 and e1 - e0 > 1.5):
            bad.append(f"DRIV {o} -> {v}: {g0:+.2f} dB ({e0:.1f}) -> {g1:+.2f} dB ({e1:.1f})")
        v = t["T_V9_TAPE"][o]
        (w1, f1, l1), (w0, f0, l0) = new["TAPE"][v], old["TAPE"][o]
        if (o > 3 and (abs(w1 - w0) > max(0.03 * w0, 0.1) or abs(f1 - f0) > max(0.06 * f0, 0.15))) or (o <= 3 and w1 > 1.1) \
                or abs(l1 - l0) > 0.5:                     # the old head loss stepped every 4 values (0.4 dB)
            bad.append(f"TAPE {o} -> {v}: +-{w0:.2f} c ({f0:.2f}, {l0:.2f} dB) -> +-{w1:.2f} c ({f1:.2f}, {l1:.2f} dB)")
        v = t["T_V9_ADPC"][o]
        if abs(new["ADPC"][v][0] - old["ADPC"][o][0]) > 0.3:
            bad.append(f"ADPC {o} -> {v}: {old['ADPC'][o][0]:.1f} -> {new['ADPC'][v][0]:.1f} dB")
        v = t["T_V9_HISS"][o]
        (h1, r1, c1), (h0, r0, c0) = new["HISS"][v], old["HISS"][o]
        if h1 is not None and h0 is not None and h1 < h0 - 0.5:
            bad.append(f"HISS {o} -> {v}: the hiss {h0:.1f} -> {h1:.1f} dB, quieter")
        if r0 is not None and r1 is not None and abs(r1 - r0) * 12 > 3 * math.sqrt((r0 + r1) * 12) + 3:
            bad.append(f"HISS {o} -> {v}: crackle {r0:.2f}/s -> {r1:.2f}/s")
        # a click's level is drawn over 7.5 dB: the median of n of them carries about 2.75 / sqrt(n) dB
        if c0 is not None and c1 is not None and abs(c1 - c0) > 3 * 2.75 * math.sqrt(1 / max(r0 * 12, 1) + 1 / max(r1 * 12, 1)):
            bad.append(f"HISS {o} -> {v}: crackle {c0:.1f} -> {c1:.1f} dBFS")
    return bad


def main():
    if "--record-old" in sys.argv:
        d = sys.argv[sys.argv.index("--record-old") + 1]
        json.dump(measure(d, old=True), open(OLD, "w"), indent=0)
        print(f"the old engine's curves from {d} into {os.path.relpath(OLD, ROOT)}")
        return 0
    out = os.path.join(ROOT, "build", "bus")
    os.makedirs(out, exist_ok=True)
    if "--no-render" not in sys.argv:              # (the last render's, as it is)
        subprocess.run(["cc", "-O2", "-Wno-unused-function", "-Ifirmware/src", "-Ifirmware/gen", "-o", "build/host/m2k_host",
                        "host/m2k_host.c", "-lm"], cwd=ROOT, check=True)
        subprocess.run([os.path.join(ROOT, "build/host/m2k_host"), out, "--bus"], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    new = keyed(json.loads(json.dumps(measure(out))))
    t = tables()
    bad = []
    if "--record" in sys.argv:
        json.dump(new, open(REF, "w"), indent=0)
        print(f"recorded into {os.path.relpath(REF, ROOT)}")
    elif os.path.exists(REF):
        bad += check_record(new, keyed(json.load(open(REF))))
    old = keyed(json.load(open(OLD)))
    v16 = keyed({k: v for k, v in json.load(open(V16)).items() if k == "GLCH"})
    bad += check_conversion(new, old, t, v16)
    if "--json" in sys.argv:
        json.dump({"new": new, "old": old, "conv": {k: t[k] for k in t if k.startswith("T_V9")}},
                  open(sys.argv[sys.argv.index("--json") + 1], "w"))
    for ctl, mid in (("GLCH", 64), ("DRIV", 64), ("TAPE", 64), ("ADPC", 64), ("HISS", 64), ("DCAY", 64), ("DAMP", 64)):
        print(f"{ctl} at 1 / {mid} / 127: " + " | ".join(" ".join("-" if x is None else f"{x:.2f}" for x in new[ctl][v])
                                                       for v in (1, mid, 127)))
    print("\n".join(bad) if bad else "the bus: every value as recorded; every old value as it sounded, converted")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
