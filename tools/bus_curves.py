#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The bus's controls (ADPC DRIV DCAY DAMP), every value, measured on the engine (host/m2k_host DIR --bus):
#   ADPC  the error it adds (dB under the mix); DRIV the gain and the distortion (dB under the clean part)
#   DCAY  the reverb's RT60 (the whole; at 250 Hz, 1 kHz, 4 kHz) and its energy (177 Hz .. 5.7 kHz, dB) at DAMP 32;
#   DAMP  the same at DCAY 80 (format 17's reverb)
# Held to the record (tools/ref/bus_m2k.json) and, through the pattern format's conversions, to the engines before:
# T_V9_DRIV and T_V9_ADPC to the one before format 9 (tools/ref/bus_v8.json, measured on 3e57c61), each old value and
# the new one it converts to measuring alike; T_V17_RV as tools/conv17.py measured it (tools/ref/conv17.json). GLCH,
# HISS and TAPE were measured and held here too until 1.5 cut them from the bus (their records' rows kept in the old
# references; their conversions held by the self-test alone).
# --json writes the curves for the page that shows them.
#   .venv/bin/python tools/bus_curves.py [--record] [--json OUT] [--no-render]
import json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))

REF, OLD = os.path.join(HERE, "ref", "bus_m2k.json"), os.path.join(HERE, "ref", "bus_v8.json")
C17 = os.path.join(HERE, "ref", "conv17.json")
CTLS = ("ADPC", "DRIV", "DCAY", "DAMP")


def tables():
    h = open(os.path.join(ROOT, "firmware", "gen", "m2k_tables.h")).read()
    def tab(n):
        b = h[h.index(n + "["):]
        return [int(v) for v in re.findall(r"-?\d+", b[b.index("{") + 1:b.index("}")])]
    t = {n: tab(n) for n in ("T_V9_DRIV", "T_V9_ADPC", "T_V17_GLCH")}
    b = h[h.index("T_V17_RV[81][2]"):]
    t["T_V17_RV"] = [int(v) for v in re.findall(r"-?\d+", b[b.index("{") + 1:b.index(";")])]
    return t


def measure(d):
    rows = {}
    for l in open(os.path.join(d, "bus.tsv")):
        c = l.split("\t")
        if c[0] == "ctl":
            continue
        rows.setdefault(c[0], {})[int(c[1])] = [float(x) for x in c[2:]]
    out = {"mix_rms": rows["DRIV"][0][2],
           "ADPC": {v: [r[1], r[0]] for v, r in rows["ADPC"].items()},
           "DRIV": {v: r[:4] for v, r in rows["DRIV"].items()}}
    for ctl in ("DCAY", "DAMP"):
        out[ctl] = {v: r[:5] for v, r in rows[ctl].items()}
    return out


def keyed(m):
    return {k: ({int(v): x for v, x in c.items()} if isinstance(c, dict) else c) for k, c in m.items()}


def check_record(new, ref):
    bad = []
    for ctl in CTLS:
        for v in range(1 if ctl == "DCAY" else 0, 128):
            if v not in ref.get(ctl, {}) or len(new[ctl][v]) != len(ref[ctl][v]):
                bad.append(f"{ctl} {v}: not in the record (--record)")
                continue
            for i, (a, b) in enumerate(zip(new[ctl][v], ref[ctl][v])):
                if (a is None) != (b is None) or (a is not None and abs(a - b) > 0.05):
                    bad.append(f"{ctl} {v} [{i}]: {b} -> {a}")
    return bad


def check_conversion(new, old, t):
    """each old value against the new one the format converts it to"""
    bad = []
    conv = json.load(open(C17))
    if conv["glch"] != t["T_V17_GLCH"] or [x for nd in conv["nodes"] for x in nd[:2]] != t["T_V17_RV"]:
        bad.append("T_V17_GLCH / T_V17_RV are not tools/ref/conv17.json's (tools/gen_tables.py)")
    for o in range(1, 128):
        v = t["T_V9_DRIV"][o]
        g1, e1 = new["DRIV"][v][:2]
        g0, e0 = old["DRIV"][o][:2]
        # (since pattern format 16 the reference's tracks end their tails a few ms apart from the old engine's: the
        # free-running bass and the noise meet the next notes elsewhere, the mix's peak 1.3 dB lower, the gains the
        # same. Under -25 dB the residual reads those peaks more than the drive: there it may only not grow)
        if abs(g1 - g0) > 0.15 or (max(e0, e1) > -25 and abs(e1 - e0) > 1.5) or (max(e0, e1) > -70 and e1 - e0 > 1.5):
            bad.append(f"DRIV {o} -> {v}: {g0:+.2f} dB ({e0:.1f}) -> {g1:+.2f} dB ({e1:.1f})")
        v = t["T_V9_ADPC"][o]
        if abs(new["ADPC"][v][0] - old["ADPC"][o][0]) > 0.3:
            bad.append(f"ADPC {o} -> {v}: {old['ADPC'][o][0]:.1f} -> {new['ADPC'][v][0]:.1f} dB")
    return bad


def main():
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
    old = keyed({k: v for k, v in json.load(open(OLD)).items() if k in ("ADPC", "DRIV")})
    bad += check_conversion(new, old, t)
    if "--json" in sys.argv:
        json.dump({"new": new, "old": old, "conv": {k: t[k] for k in t if k.startswith("T_V9")}},
                  open(sys.argv[sys.argv.index("--json") + 1], "w"))
    for ctl in CTLS:
        print(f"{ctl} at 1 / 64 / 127: " + " | ".join(" ".join("-" if x is None else f"{x:.2f}" for x in new[ctl][v])
                                                     for v in (1, 64, 127)))
    print("\n".join(bad) if bad else "the bus: every value as recorded; every old value as it sounded, converted")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
