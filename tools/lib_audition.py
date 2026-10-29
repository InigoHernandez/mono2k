#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The sound library's audition, measured: each render of `m2k_host DIR --library` (its phrase, alone, at 120 BPM) by
# its loudness (BS.1770, tools/sheet/loud.py) and its character (as docs/init-sounds.md measured the INITs: the
# spectrum's shape in 24 bands, the first hit's fall to -30 dB, flatness, pitch clarity, width, how much is left 250 ms
# on, how far the brightness drops in the first 0.1 s and how far it wanders); one distance over them (under 1.0 alike, 1.0-1.5 close), each
# sound's nearest in its category. Adds those, the instructions a sounding sample (DIR/cpu.json, m2k_draw --library)
# and a small file to listen to (MP3 with ffmpeg, else AAC with afconvert) to DIR/index.json.
#   .venv/bin/python tools/lib_audition.py DIR [--no-encode]
import json, math, os, shutil, subprocess, sys, wave
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "sheet"))
from loud import lufs

SR = 44100
# each category's loudness on its phrase, its sounds matched to it within a decibel or so (the INITs sat at -18 LUFS;
# the hats' and the percussion's lower, as their phrases are sparse hits that peak first)
TARGET = {"KICK": -18.0, "SNARE": -20.0, "HATS": -26.0, "PERC": -22.0, "BASS": -17.0, "LEAD": -19.0, "PAD": -20.0,
          "FX": -20.0}
HEAD = -1.5                             # the peak and the track's ceiling kept this far under (dB): a noise peaks
                                        # differently on another track (its own generator)
LIB_C = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "firmware", "src", "m2k_lib.c")
EDGES = np.geomspace(40, 16000, 25)     # the shape's 24 bands
UNIT = {"shape": 6.0, "dec": 1.0, "flat": 0.15, "clar": 0.3, "width": 10.0, "sus": 10.0, "cdrop": 1.0, "move": 0.25}


def load(p):
    w = wave.open(p)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64).reshape(-1, 2) / 32768
    return x


def env_db(m, hop=220, win=1323):        # 5 ms steps, a 30 ms window: steady on a 40 Hz cycle; dB
    w = np.hanning(win)
    n = max(0, (len(m) - win) // hop)
    idx = np.arange(n)[:, None] * hop + np.arange(win)[None, :]
    e = np.sqrt(((m[idx] ** 2) * w).sum(1) / w.sum() + 1e-12)
    return 20 * np.log10(e + 1e-9)


def centroid(seg):
    if len(seg) < 64: return 0.0
    X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / SR)
    k = (f >= 40) & (f <= 16000)
    return float((X[k] * f[k]).sum() / (X[k].sum() + 1e-12))


def features(x, trigs):
    m, s = (x[:, 0] + x[:, 1]) / 2, (x[:, 0] - x[:, 1]) / 2
    e = env_db(m)
    top = e.max()
    fr = lambda t: int(round(t / 0.005))
    on = fr(trigs[0])                                  # the first hit, to the next (or the end)
    nxt = fr(trigs[1]) if len(trigs) > 1 else len(e)
    nxt = min(nxt, len(e))
    pk = on + int(np.argmax(e[on:min(nxt, on + 8)]))
    win = e[pk:nxt]
    below = np.where(win < e[pk] - 30)[0]
    dec = (below[0] if len(below) else len(win)) * 0.005 + 0.005
    i250 = pk + 50
    sus = float(max(-60.0, (e[i250] if i250 < nxt else -120) - e[pk]))
    t0 = int(trigs[0] * SR)
    c1 = centroid(m[t0:t0 + int(0.02 * SR)])
    c2 = centroid(m[t0 + int(0.04 * SR):t0 + int(0.1 * SR)])
    cdrop = math.log2(max(c1, 1) / max(c2, 1)) if c1 and c2 else 0.0
    # the spectrum's shape and flatness over the frames that sound
    N = 2048
    act = [i for i in range(0, len(m) - N, N // 2) if 20 * math.log10(np.sqrt((m[i:i + N] ** 2).mean()) + 1e-9) > top - 30]
    P = np.zeros(N // 2 + 1); fl = []; cs = []
    f = np.fft.rfftfreq(N, 1 / SR)
    band = (f >= 50) & (f <= 16000)
    for i in act:
        X = np.abs(np.fft.rfft(m[i:i + N] * np.hanning(N))) ** 2 + 1e-14
        P += X
        fl.append(np.exp(np.log(X[band]).mean()) / X[band].mean())
        cs.append(math.log2((X[band] * f[band]).sum() / X[band].sum()))
    # motion: how far the brightness wanders over the frames that sound (a sweep, an LFO on the filter), octaves
    move = float(np.std(cs)) if len(cs) > 2 else 0.0
    sh = np.array([P[(f >= a) & (f < b)].sum() for a, b in zip(EDGES[:-1], EDGES[1:])]) + 1e-14
    sh = np.maximum(10 * np.log10(sh / sh.max()), -40.0)
    # pitch clarity: the normalised autocorrelation's peak (40 Hz .. 1.6 kHz) in 46 ms from 20 ms after the hit
    a0 = t0 + int(0.02 * SR); seg = m[a0:a0 + 2048]
    clar = 0.0
    if len(seg) == 2048 and (seg ** 2).sum() > 1e-9:
        seg = seg - seg.mean()
        r = np.correlate(seg, seg, "full")[2047:]
        r = r / (r[0] + 1e-12)
        clar = float(r[int(SR / 1600):int(SR / 40)].max())
    width = 10 * math.log10((s ** 2).sum() / ((m ** 2).sum() + 1e-12) + 1e-6)
    return {"shape": sh.round(1).tolist(), "dec": round(dec, 3), "flat": round(float(np.mean(fl)) if fl else 0.0, 3),
            "clar": round(clar, 2), "width": round(max(width, -40.0), 1), "sus": round(sus, 1), "cdrop": round(cdrop, 2),
            "move": round(move, 2), "centroid": round(centroid(m[t0:t0 + int(0.25 * SR)])), "frames": int(len(m))}


def distance(a, b):
    sh = math.sqrt(np.mean((np.array(a["shape"]) - np.array(b["shape"])) ** 2)) / UNIT["shape"]
    d = math.log2(a["dec"] / b["dec"]) / UNIT["dec"]
    rest = [(a[k] - b[k]) / UNIT[k] for k in ("flat", "clar", "width", "sus", "cdrop", "move")]
    return math.sqrt(sh ** 2 + d ** 2 + sum(v * v for v in rest)), sh


def encode(d, wav):
    src = os.path.join(d, wav)
    if shutil.which("ffmpeg"):
        out = wav[:-4] + ".mp3"
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", src, "-b:a", "96k", os.path.join(d, out)], check=True)
        return out
    if shutil.which("afconvert"):
        out = wav[:-4] + ".m4a"
        subprocess.run(["afconvert", "-f", "m4af", "-d", "aac", "-b", "96000", src, os.path.join(d, out)], check=True)
        return out
    return None


def main():
    d = sys.argv[1]
    ix = json.load(open(os.path.join(d, "index.json")))
    cpu = {}
    if os.path.exists(os.path.join(d, "cpu.json")):
        cpu = {c["slot"]: c for c in json.load(open(os.path.join(d, "cpu.json")))["sounds"]}
    S = ix["sounds"]
    for s in S:
        x = load(os.path.join(d, s["wav"]))
        s["lufs"] = round(lufs(x), 1)
        s["feat"] = features(x, s["trigs"])
        if s["slot"] in cpu:
            s["instr"] = cpu[s["slot"]]["instr"]
            s["instr_peak"] = cpu[s["slot"]]["peak"]
        if "--no-encode" not in sys.argv:
            enc = encode(d, s["wav"])
            if enc: s["audio"] = enc
    cats = []
    for s in S:
        if s["cat"] not in cats: cats.append(s["cat"])
    report = {}
    print(f"{'slot':5} {'cat':5} {'name':7} {'machine':8} {'LUFS':>6} {'peak':>5} {'dec':>5} {'flat':>5} {'clar':>4} "
          f"{'cen':>5}  nearest (distance)  instr")
    for c in cats:
        g = [s for s in S if s["cat"] == c]
        pairs = []
        for i, a in enumerate(g):
            best = None
            for j, b in enumerate(g):
                if i == j: continue
                dist, sh = distance(a["feat"], b["feat"])
                if best is None or dist < best[0]: best = (dist, b["name"], sh)
                if j > i: pairs.append((dist, a["name"], b["name"], sh))
            a["nearest"] = {"name": best[1], "distance": round(best[0], 2)}
        pairs.sort()
        lu = [s["lufs"] for s in g]
        report[c] = {"closest": [{"a": p[1], "b": p[2], "distance": round(p[0], 2), "shape_db": round(p[3] * 6, 1)}
                                 for p in pairs[:3]],
                     "lufs_median": round(float(np.median(lu)), 1), "lufs_spread": round(max(lu) - min(lu), 1)}
        for s in g:
            f = s["feat"]
            print(f"{s['id']:5} {c:5} {s['name']:7} {s['machine']:8} {s['lufs']:6.1f} {s['peak_db']:5.1f} {f['dec']:5.2f} "
                  f"{f['flat']:5.2f} {f['clar']:4.2f} {f['centroid']:5d}  {s['nearest']['name']:7} ({s['nearest']['distance']:.2f})"
                  f"  {s.get('instr', '')}")
        cl = pairs[0]
        print(f"      {c}: closest {cl[1]} ~ {cl[2]} {cl[0]:.2f}; LUFS {min(lu):.1f} .. {max(lu):.1f}"
              f"{'  CLOSE PAIRS: ' + ', '.join(f'{p[1]}~{p[2]} {p[0]:.2f}' for p in pairs if p[0] < 1.5) if pairs[0][0] < 1.5 else ''}")
    ix["categories"] = report
    ix["measure"] = ("distance: the spectrum's shape (24 bands to -40 dB, 6 dB rms a unit), the first hit's fall to -30 dB "
                     "(a doubling a unit), flatness (0.15), pitch clarity (0.3), width and the level 250 ms on (10 dB each), "
                     "the brightness' drop over the first 0.1 s (an octave), its wander over the sound (a quarter octave); "
                     "under 1.0 alike, 1.0-1.5 close")
    json.dump(ix, open(os.path.join(d, "index.json"), "w"), indent=1)
    off = [s for s in S if abs(s["lufs"] - TARGET[s["cat"]]) > 1.0]
    if off: print("off their category's loudness by over 1 LU: " + ", ".join(f"{s['name']} {s['lufs']:.1f}" for s in off))
    if "--level" in sys.argv: level(S)


def level(S):
    """--level: each sound's VOL and LEVL in m2k_lib.c moved to land on its category's loudness, its track 1 dB under
    its ceiling (VOL is before the track's clip, LEVL after it) and its peak under -1 dBFS; render and run again"""
    import re
    src = open(LIB_C).read()
    for s in S:
        a = src.index(f'S("{s["name"]}",')
        b = min(x for x in (src.find('\n    S("', a + 1), src.find("\n};", a)) if x > 0)
        ent = src[a:b]
        v = int(re.search(r"P_VOL, (\d+)", ent).group(1))
        l = int(re.search(r"P_LEVL, (\d+)", ent).group(1))
        T, P, L = s["track_db"], s["peak_db"], s["lufs"]
        gv = 0.0
        if T > HEAD: gv = HEAD - T - (1.5 if T > -0.05 else 0.0)     # (clipped: its true peak is higher)
        gl = TARGET[s["cat"]] - (L + gv)
        if P + gv + gl > HEAD: gl = HEAD - P - gv
        if gv == 0 and abs(gl) < 0.3: continue                       # (near enough: a step of LEVL is ~0.3 dB)
        nl = l * 10 ** (gl / 40)
        if nl > 127:
            more = min(40 * math.log10(nl / 127), HEAD - (T + gv))
            nl = 127
            if more > 0: gv += more
        nv = min(127, max(1, round(v * 10 ** (gv / 40))))
        nl = min(127, max(1, round(nl)))
        if (nv, nl) == (v, l): continue
        ent2 = re.sub(r"P_VOL, \d+", f"P_VOL, {nv}", ent)
        ent2 = re.sub(r"P_LEVL, \d+", f"P_LEVL, {nl}", ent2)
        src = src[:a] + ent2 + src[b:]
        print(f"  {s['name']:7} VOL {v} -> {nv}, LEVL {l} -> {nl}")
    open(LIB_C, "w").write(src)


if __name__ == "__main__":
    main()
