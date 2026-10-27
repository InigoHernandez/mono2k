#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The fidelity regression suite. Each case names a line of a tools/ref spec, a metric and a tolerance. `capture` reads
# the Monomachine's renders of those lines (tools/ref/mm_ref, an owner's OS image) and stores only the measured
# numbers in tools/ref/regress_ref.json: no audio, nothing from the OS image. `check` renders MONO2K with the same
# lines (host/m2k_ref), measures it the same way and prints PASS / FAIL / INFO per case, with a JSON result.
#   .venv/bin/python tools/ref/regress.py capture            from build/ref/mm_c* (the renders the chain specs made)
#   .venv/bin/python tools/ref/regress.py check [GROUP ...]  needs no OS image; exits 1 on a FAIL
#   .venv/bin/python tools/ref/regress.py gaps [GROUP ...] [--widen]   records the XFAIL cases' gaps (they only narrow)
# INFO cases are measured and reported, not asserted (below MONO2K's 16-bit floor, or past its RAM). XFAIL marks a
# known gap, with its reason. Its size is held in tools/ref/regress_gaps.json (MONO2K's error against the reference,
# per value, nothing else): an XFAIL whose error grows past that plus the tolerance FAILs, and so does one without a
# recorded gap. XPASS (the gap closed) fails too: the case is to be re-marked PASS. Recording gaps is a decision, as
# a capture is: the diff of regress_gaps.json shows what it accepted.
import json, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
from sweep import load, SR
from chain_anal import harm, psd

REF_JSON = os.path.join(HERE, "regress_ref.json")
GAPS_JSON = os.path.join(HERE, "regress_gaps.json")
SPECS = ["chain_levels", "chain_dist2", "chain_filt2", "chain_map3", "chain_clip3", "chain_eq2", "chain_srr2",
         "chain_delay", "chain_delay2", "chain_ftime5", "chain_res6", "chain_drive12", "chain_tune13", "chain_sidbend14", "chain_ens20", "chain_ensacc", "chain_dist21",
         "chain_fm26", "chain_sid36", "chain_pul38", "chain_kick40", "chain_ens41", "chain_gnoi42", "chain_fmstep43",
         "chain_fmstep44", "ring45", "chain_hold46", "chain_vo47"]
# loop specs (NAME.loops): a two-track loop per case, "= name" then its lines (mm_ref ROM loop / m2k_ref --loop), each
# render kept as name.wav beside the others; `regress.py mmloops ROM` renders the Monomachine's
LOOPS = {"ring45"}
# panel specs: the Monomachine's side from NAME.panel (mm_ref ROM panel, a "= part" a process), MONO2K's from NAME.spec
PANELS = {"chain_hold46"}
MM_DIR = {"chain_levels": "mm_clevels", "chain_dist2": "mm_cdist2", "chain_filt2": "mm_cfilt2", "chain_map3": "mm_cmap3",
          "chain_clip3": "mm_cclip3", "chain_eq2": "mm_ceq2", "chain_srr2": "mm_csrr2", "chain_delay": "mm_cdelay",
          "chain_delay2": "mm_cdelay2", "chain_ftime5": "mm_cftime5", "chain_res6": "mm_cres6",
          "chain_drive12": "mm_cdrive12", "chain_tune13": "mm_ctune13",
          "chain_sidbend14": "mm_csidbend14", "chain_ens20": "mm_cens20", "chain_ensacc": "mm_censacc",
          "chain_dist21": "mm_cdist21", "chain_fm26": "mm_cfm26", "chain_sid36": "mm_csid36", "chain_pul38": "mm_cpul38",
          "chain_kick40": "mm_ckick40", "chain_ens41": "mm_cens41", "chain_gnoi42": "mm_cgnoi42",
          "chain_fmstep43": "mm_cfmstep43", "chain_fmstep44": "mm_cfmstep44", "ring45": "mm_lring45",
          "chain_hold46": "mm_chold46", "chain_vo47": "mm_cvo47"}


def db(v):
    return float(20 * np.log10(max(abs(v), 1e-12)))


def f0_of(note):
    return 440.0 * 2 ** ((note - 69) / 12)


# ---------------------------------------------------------------------------------------------------- metrics
# each takes (render, args, loader) and returns a list of floats; loader(name) gives another render of the same set

def m_rms(x, a, ld):                      # dB per channel over the held part
    t0, t1 = a.get("t", (0.3, 1.1))
    s = x[int(t0 * SR):int(t1 * SR)]
    return [db(np.sqrt(np.mean(s[:, 0] ** 2))), db(np.sqrt(np.mean(s[:, 1] ** 2)))]


def m_h1(x, a, ld):                       # the fundamental, dB
    return [db(harm(x, f0_of(a["note"]), 1, 0.3, 16384)[0])]


def m_h1_rel(x, a, ld):                   # the fundamental against another render's (the open chain)
    f0 = f0_of(a["note"])
    return [db(harm(x, f0, 1, 0.3, 16384)[0]) - db(harm(ld(a["ref"]), f0, 1, 0.3, 16384)[0])]


def m_harm(x, a, ld):                     # h1 dB, then h3 and h5 against it (floored at -60: clean is clean)
    h = harm(x, f0_of(a["note"]), 5, a.get("t", 0.3), 16384)
    return [db(h[0]), max(-60.0, db(h[2]) - db(h[0])), max(-60.0, db(h[4]) - db(h[0]))]


def m_tone_rel(x, a, ld):                 # a sine wherever it sits (parabolic peak), dB against another render's
    def tone(y):
        s = y[int(0.4 * SR):int(0.9 * SR), 0]
        sp = np.abs(np.fft.rfft(s * np.hanning(len(s)), 8 * len(s)))
        k = int(np.argmax(sp))
        l0, l1, l2 = np.log(sp[k - 1]), np.log(sp[k]), np.log(sp[k + 1])
        return l1 - 0.25 * (l0 - l2) ** 2 / (l0 - 2 * l1 + l2) / 2
    return [float((tone(x) - tone(ld(a["ref"]))) * 20 / np.log(10))]


def m_pitch_c(x, a, ld):                  # the sine's pitch, cents from the note
    s = x[int(0.4 * SR):int(0.9 * SR), 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s)), 8 * len(s)))
    k = int(np.argmax(sp))
    l0, l1, l2 = np.log(sp[k - 1]), np.log(sp[k]), np.log(sp[k + 1])
    fq = (k + 0.5 * (l0 - l2) / (l0 - 2 * l1 + l2)) * SR / (8 * len(s))
    return [float(1200 * np.log2(fq / f0_of(a["note"])))]


def m_bend_c(x, a, ld):                  # a short note's pitch over its audible part (harmonic comb), cents from the note
    sys.path.insert(0, os.path.join(HERE, ".."))
    import demo_pitch as D
    y = x[:, 0]
    e = np.convolve(np.abs(y), np.ones(64) / 64, "same")
    o = int(np.argmax(e > e.max() * 0.05))
    seg = y[o + int(0.015 * SR):o + int(0.35 * SR)]
    sm = np.convolve(np.abs(seg), np.ones(256) / 256, "same")
    k = np.nonzero(sm > sm.max() * 0.03)[0]
    sp, df = D.spectrum(seg[k[0]:k[-1]])
    return [D.comb(sp, df, f0_of(a["note"]))[0]]


def m_ens_voice(x, a, ld):               # SWAV-ENS: the interval PCH2 adds over the root alone, cents (NaN: none)
    from ens15 import spec as espec, voices as evoices, peak_c as epeak, F0 as EF0
    ref, df = espec(ld(a["ref"])[:, 0])
    sp, _ = espec(x[:, 0])
    v = evoices(sp, df, ref, 1)
    if not v or v[0][2] < epeak(ref, df, EF0)[1] - 40:
        return [float("nan")]
    return [float(100 * v[0][0] + v[0][1] - epeak(ref, df, EF0)[0])]


def m_c3_partials(x, a, ld):             # level, then partials 1..6 of C3 (a voice is two saws an octave down), dB,
    from ens15 import spec as espec, peak_c as epeak, F0 as EF0      # floored at -75 (a cancelled one is cancelled)
    sp, df = espec(x[:, 0])
    lv = 20 * np.log10(np.sqrt(np.mean(x[int(0.3 * SR):int(1.1 * SR), 0] ** 2)) + 1e-12)
    return [max(-75.0, float(lv))] + [max(-75.0, float(epeak(sp, df, k * EF0 / 2, 20)[1])) for k in range(1, 7)]


def m_chorus_mix(x, a, ld):              # the dry line's power and the moving copy's at partial 8, dB
    from ens15 import spec as espec, F0 as EF0
    sp, df = espec(x[:, 0], 0.3, 3.9, 4)
    fr = np.arange(len(sp)) * df
    cents = 1200 * np.log2(np.maximum(fr, 1) / (8 * EF0))
    p = sp ** 2
    dry = p[np.abs(cents - 0.5) < 0.8].sum()
    tot = p[np.abs(cents) < 25].sum()
    return [max(-70.0, db(np.sqrt(dry))), max(-70.0, db(np.sqrt(max(tot - dry, 1e-30))))]   # -70: no copy


def m_lr_corr(x, a, ld):                 # the channels' correlation over the held note
    s = x[int(0.3 * SR):int(3.9 * SR)]
    return [float(np.corrcoef(s[:, 0], s[:, 1])[0, 1])]


def _chorus_track(c, f0):                 # one channel's copy pitch around partial 8, cents, every 200 samples
    n = len(c)
    X = np.fft.fft(c)
    f = np.fft.fftfreq(n, 1 / SR)
    H = np.zeros(n)
    H[(f > f0 - 60) & (f < f0 + 60)] = 2
    ph = np.unwrap(np.angle(np.fft.ifft(X * H)))
    fi = np.diff(ph) * SR / (2 * np.pi)
    k = int(0.02 * SR)
    fi = np.convolve(fi, np.ones(k) / k, "same")
    tr = 1200 * np.log2(np.maximum(fi, 1) / f0)[2000:-2000][::200]
    return tr - tr.mean()


def m_chorus_stereo(x, a, ld):            # SWAV-ENS's chorus held: its LFO (Hz), the left's swing (c), the right's
    from ens15 import F0 as EF0          # phase on the left's (deg), L/R correlation, the copy's side against its mid (dB)
    s = x[int(0.5 * SR):int(11.5 * SR)]
    f8 = 8 * EF0
    tl, tr = _chorus_track(s[:, 0], f8), _chorus_track(s[:, 1], f8)
    t = np.arange(len(tl)) * 200 / SR
    best = (0.0, 0.0, 0.0)
    for fq in np.arange(0.10, 1.0, 0.002):
        A = np.stack([np.sin(2 * np.pi * fq * t), np.cos(2 * np.pi * fq * t)], 1)
        co = np.linalg.lstsq(A, tl, rcond=None)[0]
        if np.hypot(*co) > best[1]:
            best = (fq, float(np.hypot(*co)), float(np.arctan2(co[1], co[0])))
    A = np.stack([np.sin(2 * np.pi * best[0] * t), np.cos(2 * np.pi * best[0] * t)], 1)
    cr = np.linalg.lstsq(A, tr, rcond=None)[0]
    dphi = float((np.degrees(np.arctan2(cr[1], cr[0]) - best[2]) + 180) % 360 - 180)
    corr = float(np.corrcoef(s[:, 0], s[:, 1])[0, 1])
    from ens15 import spec as espec
    def copy(c):
        sp, df = espec(c, 0, 11, 4)
        fr = np.arange(len(sp)) * df
        cents = 1200 * np.log2(np.maximum(fr, 1) / f8)
        p = sp ** 2
        return p[np.abs(cents) < 40].sum() - p[np.abs(cents - 0.5) < 0.8].sum()
    side = db(np.sqrt(max(copy((s[:, 0] - s[:, 1]) / 2), 1e-30) / max(copy((s[:, 0] + s[:, 1]) / 2), 1e-30)))
    return [best[0], best[1], dphi, corr, side]


def m_ens_voices(x, a, ld):              # each voice of a SWAV-ENS chord, cents from its intended pitch (ens_voices.py)
    import ens_voices as EV
    return EV.measure(x, EV.intended(a["note"], {int(k): v for k, v in a["cc"].items()}))


def m_thd(x, a, ld):
    h = harm(x, f0_of(a["note"]), 7, 0.3, 16384)
    return [db(np.sqrt(np.sum(h[1:] ** 2))) - db(h[0])]


def m_peak_hz(x, a, ld):                  # the resonance's peak on noise, parabolic between bins
    f, p = psd(x, 0.3, 2.2, 32768)
    k = int(np.argmax(np.where(f > 15, p, 0)))
    y0, y1, y2 = np.log(p[k - 1] + 1e-30), np.log(p[k] + 1e-30), np.log(p[k + 1] + 1e-30)
    return [float((k + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)) * SR / 32768)]


def _ratio(x, r, t1, smooth):
    f, p = psd(x, 0.3, t1, 16384)
    _, q = psd(r, 0.3, t1, 16384)
    k = np.ones(smooth) / smooth
    return f, 10 * np.log10(np.convolve(p, k, "same") / np.convolve(q, k, "same"))


def m_res_db(x, a, ld):                   # the response's peak against the open chain, noise with headroom
    f, r = _ratio(x, ld(a["ref"]), 3.2, 9)
    m = (f > 100) & (f < 16000)
    return [float(np.max(np.where(m, r, -999)))]


def m_resp_at(x, a, ld):                  # the response against the open chain at given frequencies
    f, r = _ratio(x, ld(a["ref"]), 3.2, 15)
    return [float(r[np.argmin(np.abs(f - q))]) for q in a["hz"]]


def m_srr_image(x, a, ld):                # the sine's first image (R - f0) near where it should be: Hz, dB under the sine
    s = x[int(0.15 * SR):int(0.15 * SR) + 16384, 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    f = np.fft.rfftfreq(len(s), 1 / SR)
    sel = np.abs(f / a["near"] - 1) < 0.03
    k = int(np.argmax(np.where(sel, sp, 0)))
    return [float(f[k]), db(sp[k] / sp.max())]


def m_level_at(x, a, ld):                 # dB under the strongest component, at given Hz (+-3 bins)
    s = x[int(0.15 * SR):int(0.15 * SR) + 32768, 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    f = np.fft.rfftfreq(len(s), 1 / SR)
    out = []
    for q in a["hz"]:
        k = int(np.argmin(np.abs(f - q)))
        out.append(max(-80.0, db(sp[k - 3:k + 4].max() / sp.max())))
    return out


def _env(x, n=44):
    return np.sqrt(np.convolve(x[:, 0] ** 2 + x[:, 1] ** 2, np.ones(n) / n, "same") / 2)


def m_echo_ms(x, a, ld):                  # from the dry onset to the echo's, both at 10 % of their peaks
    e = _env(x)
    on = int(np.argmax(e > 0.1 * e.max()))
    lo, hi = int((a["at"] - 20) / 1000 * SR), int((a["at"] + 60) / 1000 * SR)
    seg = e[lo:hi]
    return [float((int(np.argmax(seg > 0.1 * seg.max())) + lo - on) / SR * 1000)]


def m_echoes(x, a, ld):                   # each echo's level against the dry blip, per channel, at its expected time
    n = 220
    el = np.sqrt(np.convolve(x[:, 0] ** 2, np.ones(n) / n, "same"))
    er = np.sqrt(np.convolve(x[:, 1] ** 2, np.ones(n) / n, "same"))
    e = np.sqrt((el ** 2 + er ** 2) / 2)
    dry = e[:int(0.05 * SR)].max()
    out = []
    for t in a["at"]:
        lo, hi = int((t - 20) / 1000 * SR), int((t + 30) / 1000 * SR)
        j = lo + int(np.argmax(e[lo:hi]))
        out += [max(-60.0, db(el[j] / dry)), max(-60.0, db(er[j] / dry))]     # silence floored
    return out


def m_echo_bands(x, a, ld):               # the first echo's spectrum against the dry blip's, in octave bands
    def spec(s):
        n = 4096
        w = np.hanning(n)
        return np.mean([np.abs(np.fft.rfft(s[i:i + n] * w)) ** 2 for i in range(0, len(s) - n, n // 4)], axis=0)
    s = x[:, 0]
    d, e = spec(s[:int(0.22 * SR)]), spec(s[int(a["at"] * SR):int((a["at"] + 0.22) * SR)])
    f = np.fft.rfftfreq(4096, 1 / SR)
    out = []
    for c in a["hz"]:
        m = (f >= c / 2 ** 0.5) & (f < c * 2 ** 0.5)
        out.append(float(10 * np.log10(e[m].mean() / d[m].mean())))
    return out


def m_fenv_ms(x, a, ld):                  # when the sine crosses -3.8 dB as the swept cutoff passes it
    n = 441
    e = 20 * np.log10(np.maximum(np.sqrt(np.convolve(x[:, 0] ** 2, np.ones(n) / n, "same")), 1e-9))
    if a["kind"] == "dec":
        lvl, start = e - e[int(0.003 * SR):int(0.02 * SR)].max(), int(0.02 * SR)
        idx = np.where(lvl[start:int(5.95 * SR)] < -3.8)[0]
    else:
        lvl, start = e - e[int(5.5 * SR):int(5.9 * SR)].mean(), int(0.005 * SR)
        idx = np.where(lvl[start:int(5.95 * SR)] > -3.8)[0]
    return [float((idx[0] + start) / SR * 1000) if len(idx) else float("nan")]


def m_res_fit(x, a, ld):                  # the resonance resolved (resfit.py): its peak against the cutoff, %, and height, dB
    from resfit import psd as rpsd
    f, p = rpsd(x)
    _, r = rpsd(ld(a["ref"]))
    ratio = p / np.maximum(r, 1e-30)
    fc = f0_of(a["note"]) * 2 ** ((a["step"] - 16) / 8)
    fp, q, g, e = _resfit(f, ratio, fc, a["hp"])
    return [(fp / fc - 1) * 100, g]


def _resfit(f, ratio, fc, hp):
    from resfit import N
    k = int(np.argmax(np.where(np.abs(f / fc - 1) < 0.15, np.convolve(ratio, np.ones(5) / 5, "same"), 0)))
    best = None
    for q in np.exp(np.linspace(np.log(2), np.log(3000), 160)):
        bw = f[k] / q
        sel = np.abs(f - f[k]) < max(6 * bw, 40)
        for fp in f[k] + np.linspace(-3, 3, 61) * (SR / N):
            m = 1 / (1 + (2 * q * (f[sel] - fp) / fp) ** 2)
            aa = np.dot(m, ratio[sel]) / np.dot(m, m)
            e = np.mean((np.log(np.maximum(aa * m, 1e-30)) - np.log(np.maximum(ratio[sel], 1e-30))) ** 2)
            if best is None or e < best[0]:
                best = (e, fp, q, aa)
    e, fp, q, aa = best
    pb = ratio[(f > fc * 1.6) & (f < min(fc * 2.1, 17000))] if hp else ratio[(f > fc / 4) & (f < fc / 2.5)]
    return fp, q, 10 * np.log10(aa / np.median(pb)), e


def m_fm_side(x, a, ld):                  # FM+: partials at given Hz, dB under the window's strongest (floored at -50)
    t0, n = a.get("t", 0.3), a.get("n", 16384)
    s = x[int(t0 * SR):int(t0 * SR) + n].mean(axis=1)
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    df = SR / len(s)
    r = max(2, int(round(8 / df)))
    out = []
    for q in a["hz"]:
        k = int(round(q / df))
        out.append(max(a.get("floor", -50.0), db(sp[max(0, k - r):k + r + 1].max() / sp.max())))
    return out


def m_ring(x, a, ld):                    # FX-RING: the right channel's level at given Hz (dB of full scale, floored),
    t0, n = a.get("t", 0.5), int(a.get("n", 1.0) * SR)  # and with "pk" its peak against the first, dB. A flat-top
    s = x[int(t0 * SR):int(t0 * SR) + n, 1]            # window: no scalloping, so a component a hair off its bin (the
    u = np.arange(len(s)) * 2 * np.pi / len(s)          # two instruments' carriers differ by 0.04 Hz) reads its level
    w = 0.21557895 - 0.41663158 * np.cos(u) + 0.277263158 * np.cos(2 * u) - 0.083578947 * np.cos(3 * u) + 0.006947368 * np.cos(4 * u)
    sp = np.abs(np.fft.rfft(s * w)) * 2 / w.sum()
    df = SR / len(s)
    out = []
    for q in a["hz"]:
        k = int(round(q / df))
        out.append(max(a.get("floor", -150.0), db(sp[max(0, k - 3):k + 4].max())))
    if a.get("pk"):
        out.append(db(np.abs(s).max()) - out[0])
    return out


def m_hold_ms(x, a, ld):                 # the amp's hold: ms from the onset to the first fall 0.5 dB under the level
    s = x.mean(axis=1) if x.ndim == 2 else x   # just after the attack (5..12 ms on, past a click), RMS over a period
    n = int(SR / f0_of(a.get("note", 60)))
    c = np.concatenate([[0], np.cumsum(s * s)])
    e = 20 * np.log10(np.sqrt(np.maximum(c[n:] - c[:-n], 0) / n) + 1e-9)
    on = int(np.argmax(e > e.max() - 30))
    pl = np.median(e[on + int(0.005 * SR):on + int(0.012 * SR)])
    after = np.nonzero(e[on + int(0.012 * SR):] < pl - 0.5)[0]
    return [float((after[0] + int(0.012 * SR)) * 1000 / SR) if len(after) else float("nan")]


def vo_mono(x):
    return x.mean(axis=1) if x.ndim == 2 else x


def m_vo_f0(x, a, ld):                   # VO-6: its pitch, the peak near the note (+-20 %), cents from the note
    s = vo_mono(x)[int(0.4 * SR):int(0.95 * SR)]
    n = len(s); S = np.abs(np.fft.rfft(s * np.hanning(n))); df = SR / n; L = np.log(S + 1e-12); g = f0_of(a["note"])
    i = int(g * 0.8 / df) + int(np.argmax(S[int(g * 0.8 / df):int(g * 1.2 / df)]))   # (TUNE's +-2 semitones inside)
    return [float(1200 * np.log2((i + 0.5 * (L[i - 1] - L[i + 1]) / (L[i - 1] - 2 * L[i] + L[i + 1])) * df / g))]


def m_vo_formant(x, a, ld):              # a whispered VO-6's spectral peaks in given ranges, Hz (Welch, 16384 points,
    s = vo_mono(x)[int(0.3 * SR):int(0.95 * SR)]; n = 16384; w = np.hanning(n); acc = np.zeros(n // 2 + 1); k = 0   # the
    for i in range(0, len(s) - n, n // 2):                                    # power smoothed over 60 Hz)
        acc += np.abs(np.fft.rfft(s[i:i + n] * w)) ** 2; k += 1
    f = np.fft.rfftfreq(n, 1 / SR); m = int(60 / (SR / n))
    d = 10 * np.log10(np.convolve(acc / k, np.ones(2 * m + 1) / (2 * m + 1), mode="same") + 1e-20)
    out = []
    for lo, hi in a["ranges"]:                 # the power's centre within 3 dB of the top: a broad formant's flat top
        sel = (f > lo) & (f < hi); i = np.nonzero(sel)[0][np.argmax(d[sel])]   # does not move it (its argmax would)
        j, k = i, i
        while j > 0 and d[j - 1] > d[i] - 3: j -= 1
        while k < len(d) - 1 and d[k + 1] > d[i] - 3: k += 1
        pw = 10 ** (d[j:k + 1] / 10); out.append(float(np.sum(f[j:k + 1] * pw) / np.sum(pw)))
    return out


def m_vo_split(x, a, ld):                # a voice's harmonic power and the rest (its noise), dB
    s = vo_mono(x)[int(0.35 * SR):int(0.95 * SR)]; n = len(s); w = np.hanning(n); t = np.arange(n) / SR
    f0 = f0_of(a["note"]) * 2 ** (m_vo_f0(x, a, ld)[0] / 1200)
    hp = sum(abs(np.sum(s * w * np.exp(-2j * np.pi * k * f0 * t)) * 2 / w.sum()) ** 2 / 2 for k in range(1, int(8000 / f0)))
    tot = np.mean(s * s)
    return [10 * np.log10(hp + 1e-20), 10 * np.log10(max(tot - hp, 1e-12))]


def m_vo_onset(x, a, ld):                # the fade-in: dB against the held level, 10 ms windows from given times
    s = vo_mono(x); ss = np.mean(s[int(0.6 * SR):int(0.9 * SR)] ** 2)
    return [10 * np.log10(np.mean(s[int(t * SR):int((t + 0.01) * SR)] ** 2) / ss + 1e-12) for t in a["at"]]


_JT = {}


def m_fm_beta(x, a, ld):                  # FM+: a modulator's index (rad), fitted to its sidebands' |J_k| (a ratio whose
    if not _JT:                           # sidebands fall apart from each other); Bessel J_n by its integral
        tau = np.linspace(0, np.pi, 2001)
        b = np.arange(0, 24, 0.01)
        _JT["b"] = b
        _JT["j"] = np.stack([np.abs(getattr(np, "trapezoid", getattr(np, "trapz", None))(np.cos(k * tau[None, :] - b[:, None] * np.sin(tau)[None, :]), tau, axis=1) / np.pi)
                             for k in range(29)], axis=1)
    fc, fm = f0_of(a["note"]), f0_of(a["note"]) * a["ratio"]
    t0, n, K = a.get("t", 0.3), a.get("n", 32768), a.get("k", 12)
    s = x[int(t0 * SR):int(t0 * SR) + n].mean(axis=1)
    w = np.hanning(len(s))
    N = max(1 << 15, len(s))
    X = np.abs(np.fft.rfft(s * w, N))
    df = SR / N
    def at(q):
        k, r = int(round(q / df)), max(2, int(round(8 / df)))
        return X[max(0, k - r):k + r + 1].max()
    m = np.array([at(fc)] + [0.5 * (at(fc + k * fm) + at(abs(fc - k * fm))) for k in range(1, K + 1)])
    J = _JT["j"][:, :K + 1]
    sc = (J @ m) / np.maximum((J * J).sum(axis=1), 1e-20)
    e = ((m[None, :] - sc[:, None] * J) ** 2).sum(axis=1)
    return [float(_JT["b"][int(np.argmin(e))])]


def m_fen_hz(x, a, ld):                   # FM+DYN's first operator's frequency at given times: the J1 pair (fc +- f1)
    fc = f0_of(a["note"])                 # that carries the most, the carrier's own lobe left out; or ("peak", for one
    y, N = x.mean(axis=1), 1 << 16        # that sweeps up through 0) the strongest sideband's distance from the carrier
    f = np.fft.rfftfreq(N, 1 / SR)
    cand = np.arange(0.0, 4000.0, 1.0)
    L = 2048 if a.get("mode") == "peak" else 1024
    out = []
    for t in a["at"]:
        s = y[int(t * SR):int(t * SR) + L]
        X = np.abs(np.fft.rfft(s * np.hanning(len(s)), N))
        if a.get("mode") == "peak":
            X[(np.abs(f - fc) < 2.2 * SR / L) | (f < 25)] = 0
            out.append(float(abs(f[int(np.argmax(X))] - fc)))
            continue
        X[np.abs(f - fc) < 1.6 * SR / L] = 0
        sc = np.interp(fc + cand, f, X) + np.interp(np.abs(fc - cand), f, X)
        sc[cand < 1.6 * SR / L] = 0
        out.append(float(cand[int(np.argmax(sc))]))
    return out


def _hit(x):                             # a one-shot from its onset, 300 ms, both channels' mean
    m = x.mean(axis=1)
    on = int(np.argmax(np.abs(m) > 0.05 * np.abs(m).max()))
    return m[on:on + int(0.3 * SR)]


def _hit_spec(s):
    X = np.abs(np.fft.rfft(s, 1 << 16)) ** 2
    return X, np.fft.rfftfreq(1 << 16, 1 / SR)


def m_kick(x, a, ld):                     # a one-shot: rms over 300 ms, its 60-120 and 200-500 Hz shares (dB), its body
    s = _hit(x)                           # (ms from its peak to -20 dB under 150 Hz)
    X, f = _hit_spec(s)
    tot = X.sum()
    sh = lambda lo, hi: float(10 * np.log10(X[(f >= lo) & (f < hi)].sum() / tot + 1e-12))
    Y = np.fft.rfft(s, 1 << 16)
    Y[f >= 150] = 0
    lo = np.fft.irfft(Y, 1 << 16)[:len(s)]
    e = np.sqrt(np.convolve(lo ** 2, np.ones(128) / 128, "same"))
    k = int(np.argmax(e))
    after = np.where(e[k:] < 0.1 * e[k])[0]
    return [db(np.sqrt(np.mean(s ** 2))), sh(60, 120), sh(200, 500), 1000.0 * (after[0] if len(after) else len(e) - k) / SR]


def m_onset_hf(x, a, ld):                 # a one-shot's first 4 ms over 2 kHz against the whole hit, dB (its click)
    m = x.mean(axis=1)
    on = int(np.argmax(np.abs(m) > 1e-4 * np.abs(m).max()))
    s = m[on:on + int(0.3 * SR)]
    n4 = int(0.004 * SR)
    X = np.abs(np.fft.rfft(s[:n4], 1 << 16)) ** 2
    f = np.fft.rfftfreq(1 << 16, 1 / SR)
    return [float(10 * np.log10(X[f > 2000].sum() / (np.sum(s ** 2) * (1 << 16) / 2) + 1e-15))]


def m_noise_bands(x, a, ld):              # held noise, 0.2-0.9 s: its level (dB), then octaves 125 Hz .. 16 kHz against
    s = x[int(0.2 * SR):int(0.9 * SR)].astype(np.float64)       # 250 Hz .. 8 kHz's (a steadier yardstick than one octave)
    m = s.mean(axis=1)
    X = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
    f = np.fft.rfftfreq(len(m), 1 / SR)
    def oc(c):
        sel = (f >= c / np.sqrt(2)) & (f < c * np.sqrt(2))
        return X[sel].mean()
    r = np.mean([oc(c) for c in (250, 500, 1000, 2000, 4000, 8000)])
    return [db(np.sqrt(np.mean(s ** 2)))] + [float(10 * np.log10(oc(c) / r)) for c in (125, 250, 500, 1000, 2000, 4000, 8000, 16000)]


def m_noise_stereo(x, a, ld):             # held noise, 0.2-0.9 s: L/R correlation, the side against the mid (dB, floored at -40)
    s = x[int(0.2 * SR):int(0.9 * SR)].astype(np.float64)
    md, sd = (s[:, 0] + s[:, 1]) / 2, (s[:, 0] - s[:, 1]) / 2
    return [float(np.corrcoef(s[:, 0], s[:, 1])[0, 1]), max(-40.0, float(10 * np.log10(np.mean(sd ** 2) / np.mean(md ** 2) + 1e-20)))]


def m_grit(x, a, ld):                     # the energy off the note's harmonics, dB under the whole (character.py)
    import character
    return [character.analyse_x(x, a["note"])["grit"]]


METRICS = {"ring": m_ring, "hold_ms": m_hold_ms, "vo_f0": m_vo_f0, "vo_formant": m_vo_formant, "vo_split": m_vo_split,
           "vo_onset": m_vo_onset, "res_fit": m_res_fit, "rms": m_rms, "h1": m_h1, "h1_rel": m_h1_rel, "harm": m_harm, "thd": m_thd, "peak_hz": m_peak_hz,
           "res_db": m_res_db, "resp_at": m_resp_at, "srr_image": m_srr_image, "level_at": m_level_at, "echo_ms": m_echo_ms,
           "echoes": m_echoes, "echo_bands": m_echo_bands, "fenv_ms": m_fenv_ms, "tone_rel": m_tone_rel, "pitch_c": m_pitch_c, "bend_c": m_bend_c,
           "ens_voice": m_ens_voice, "c3_partials": m_c3_partials, "chorus_mix": m_chorus_mix, "lr_corr": m_lr_corr,
           "ens_voices": m_ens_voices, "fm_side": m_fm_side, "fm_beta": m_fm_beta, "fen_hz": m_fen_hz, "grit": m_grit,
           "kick": m_kick, "onset_hf": m_onset_hf, "noise_bands": m_noise_bands, "noise_stereo": m_noise_stereo,
           "chorus_stereo": m_chorus_stereo}


# ------------------------------------------------------------------------------------------------------ cases
# (group, spec, render, metric, args, tolerance per value or one for all, "rel" for a fraction, info: not asserted)

def cases():
    C = []
    def add(group, spec, name, metric, args, tol, rel=False, info=False, xfail=None):
        C.append(dict(group=group, spec=spec, name=name, metric=metric, args=args, tol=tol, rel=rel, info=info, xfail=xfail))
    sat = ("the reference's HP saturates inside its resonance: from h1 ~-16 dBFS out (DIST <= 64, any Q) its output is a"
           " square through a high-pass, scaled by the DIST ceiling; MONO2K's clamped SVF stays near clean (chain_drive12)")
    vary = "the reference's Q at 96 / 127 varies with the cutoff (C6 against C4): one table fits C6"
    for v in (8, 32, 64, 96, 120, 127):
        add("levels", "chain_levels", f"vol_{v:03d}", "rms", {}, 0.1)
    for v in (16, 64, 127):
        add("levels", "chain_levels", f"lev_{v:03d}", "rms", {}, 0.1)
    for v in (0, 16, 32, 48, 64, 80, 96, 112, 127):
        add("levels", "chain_levels", f"pan_{v:03d}", "rms", {}, 0.1)
    for d in range(32, 64, 8):                          # the headroom, down to -60 dB
        add("dist", "chain_dist2", f"dd_{d:03d}", "h1", {"note": 36}, 0.3)
    for d in (8, 16, 24):                               # below: MONO2K's machines are 16-bit, a step is ~-90 dB
        add("dist", "chain_dist2", f"dd_{d:03d}", "h1", {"note": 36}, 0.3, info=True)
    for d in (64, 68, 72):                              # clean
        add("dist", "chain_dist2", f"dd_{d:03d}", "h1", {"note": 36}, 0.1)
    for d in range(76, 128, 4):                         # the clip
        add("dist", "chain_dist2", f"dd_{d:03d}", "harm", {"note": 36}, [0.4, 1.5, 1.5])
    # DIST on loud and complex signals (chain_dist21): the Monomachine clips each machine against a ceiling of its own
    # (SWAVE-SAW's ~10 dB under GND-SIN's), whatever the level going in. The output's rms, past where it clips
    for sig in ("saw", "saw_s1_040", "saw_s1_127", "saw_subs", "saw_uni", "ens1", "ens4", "nois"):
        for d in (80, 88, 104, 127):
            add("dist_loud", "chain_dist21", f"d21_{sig}_d{d:03d}", "rms", {"t": (0.3, 0.95)}, 1.0)
    for d in (104, 127):            # a sine cut 15 dB going in reaches the same ceiling (lower, the EQ's own 1.4 dB)
        add("dist_loud", "chain_dist21", f"d21_sin_eq32_d{d:03d}", "rms", {"t": (0.3, 0.95)}, 1.0)
    for sig in ("sid", "fms", "fmd", "fmp", "wav", "vox"):
        for d in (112, 127):
            add("dist_loud", "chain_dist21", f"d21m_{sig}_d{d:03d}", "rms", {"t": (0.3, 0.95)}, 1.0)
    # each machine's own level, clean (DIST 64, chain_dist21): the machines measured so far match; the rest are the
    # calibration still to do (README, the next steps), each gap held at its size
    for sig in ("sin", "saw", "saw_s1_040", "saw_s1_080", "saw_uni", "ens1", "ens4"):
        add("machine_level", "chain_dist21", f"d21_{sig}_d064", "rms", {"t": (0.3, 0.95)}, 1.0)
    sub = "SWAV-SAW's sine sub at C1 (33 Hz) at SUB1 127: MONO2K 1.1-1.2 dB under (0.6 at SUB1 80)"
    for sig in ("saw_s1_127", "saw_subs"):
        add("machine_level", "chain_dist21", f"d21_{sig}_d064", "rms", {"t": (0.3, 0.95)}, 1.0, xfail=sub)
    add("machine_level", "chain_dist21", "d21_nois_d064", "rms", {"t": (0.3, 0.95)}, 1.0)   # GND-NOIS on its page: its level
    for sig in ("sid", "sid48"):                   # on SID-6581's page
        add("machine_level", "chain_dist21", f"d21m_{sig}_d064", "rms", {"t": (0.3, 0.95)}, 1.0)
    for sig in ("fms", "fmd", "fmp"):              # on the Monomachine's pages: the carrier at half GND-SIN's
        add("machine_level", "chain_dist21", f"d21m_{sig}_d064", "rms", {"t": (0.3, 0.95)}, 1.0)
    add("machine_level", "chain_dist21", "d21m_vox_d064", "rms", {"t": (0.3, 0.95)}, 1.0)   # (VO-6 rebuilt: closed)
    add("machine_level", "chain_dist21", "d21m_wav_d064", "rms", {"t": (0.3, 0.95)}, 1.0, info=True)   # not the
    # same waves: DIGI-WAV plays MONO2K's own, the Monomachine's DPRO-WAVE its DigiPro set (never copied)
    for v in range(0, 31, 2):
        add("filter_mag", "chain_filt2", f"hp_{v:03d}", "h1_rel", {"note": 48, "ref": "hp_000"}, 0.5)
    for v in range(32, 45, 2):                          # the HP's far stopband: MONO2K's 1-2 dB steeper
        add("filter_mag", "chain_filt2", f"hp_{v:03d}", "h1_rel", {"note": 48, "ref": "hp_000"}, 0.5,
            xfail="HP stopband below -40 dB is 0.5-2 dB steeper than the reference")
    for v in range(46, 57, 2):
        add("filter_mag", "chain_filt2", f"hp_{v:03d}", "h1_rel", {"note": 48, "ref": "hp_000"}, 0.5, info=True)
    for v in range(0, 65, 2):
        add("filter_mag", "chain_filt2", f"lp_{v:03d}", "h1_rel", {"note": 48, "ref": "lp_064"}, 0.5)
    for v in range(0, 57, 8):
        add("res_map", "chain_map3", f"m3_lp_{v:03d}", "peak_hz", {}, 0.01, rel=True)
        add("res_map", "chain_map3", f"m3_hp_{v:03d}", "peak_hz", {}, 0.01, rel=True)
    for n in (24, 36, 48, 72, 96):
        add("res_map", "chain_map3", f"m3_note_{n:03d}", "peak_hz", {}, 0.01, rel=True)
        add("res_map", "chain_map3", f"m3_hnote_{n:03d}", "peak_hz", {}, 0.01, rel=True)
    for q in range(16, 121, 8):
        add("res_level", "chain_filt2", f"qn_lp_{q:03d}", "res_db", {"ref": "qn_open"}, 2.5,
            xfail="LPQ 120 on C4 noise: MONO2K ~4 dB under (" + vary + ")" if q == 120 else None)
    for q in range(32, 121, 8):
        add("res_level", "chain_filt2", f"qn_hp_{q:03d}", "res_db", {"ref": "qn_open"}, 2.5,
            xfail="HPQ 88 on C4 noise: MONO2K 2.5 dB over, where the reference's curve dips (30.7 33.2 37.4 dB at 80 88"
                  " 96; on the edge since the start, over it once GND-NOIS's noise turned the Monomachine's)" if q == 88 else None)
    for kind, step in (("hp", 40), ("lp", 40)):          # the resonance resolved: where its peak sits, how high
        for q in (64, 80, 96, 112, 127):
            add("res_fit", "chain_res6", f"r6_{kind}_n84_{q:03d}", "res_fit",
                {"ref": "r6_open_n84", "note": 84, "step": step, "hp": kind == "hp"}, [0.05, 1.5] if q >= 96 else [0.6, 2.0],
                xfail="HPQ 96's peak 0.07 % over the reference's (C6)" if (kind, q) == ("hp", 96) else None)
    for kind in ("hp", "lp"):
        for q in (96, 127):
            add("res_fit", "chain_res6", f"r6_{kind}_n60_{q:03d}", "res_fit",
                {"ref": "r6_open_n60", "note": 60, "step": 48, "hp": kind == "hp"}, [0.05, 1.5],
                xfail=None if (kind, q) in (("hp", 127), ("lp", 96)) else vary)     # LPQ 96 at C4: closed on GND-NOIS's noise
    for d in (32, 48, 64, 72, 80, 96, 127):              # resonance and drive: the clip holds the LP's resonance; at 32
        # LPQ 127 at 131 Hz is still building at 0.8 s, and the reference's run carried it from the line before
        add("clip", "chain_clip3", f"c3_res_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0], info=d == 32)
    for d in (80, 96, 126):
        add("clip", "chain_clip3", f"c3_lpf_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0])
    for d in (80, 96, 124):
        add("clip", "chain_clip3", f"c3_lp24_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0])
    for d in (80, 96, 127):
        add("clip", "chain_dist2", f"dclip_lp_{d:03d}", "harm", {"note": 36}, [0.7, 1.5, 2.0])
        add("clip", "chain_dist2", f"dclip_hp_{d:03d}", "harm", {"note": 36}, [0.7, 1.5, 2.0], xfail=sat)
    for d in (16, 32, 40, 48, 64, 72, 80, 96, 127):   # 0.8 s into a 2.9 s build-up, and rendered one after another:
        # the reference's resonance carries from the line before (chain_order11), so these are not comparable
        add("hp_drive", "chain_clip3", f"c3_hres_{d:03d}", "harm", {"note": 48}, [1.0, 3.0, 3.0], info=True)
    floor = dict(info=True)                           # h3 / h5 near -105 dBFS: under MONO2K's 16-bit floor
    def d12(name, note=48, **kw):
        add("hp_sat", "chain_drive12", name, "harm", {"note": note, "t": 1.5}, [1.0, 2.0, 2.0], **kw)
    for q, d in ((0, 32), (32, 32)):
        d12(f"d12_b16_q{q:03d}_n48_d{d:03d}", **floor)
    d12("d12_b16_q000_n48_d064")
    for q in (16,):
        for d in (32, 64):
            d12(f"d12_b16_q{q:03d}_n48_d{d:03d}", xfail="HPQ 16 at the cutoff: MONO2K 1.3-1.6 dB quieter")
    d12("d12_b16_q032_n48_d056", xfail="HPQ 32 at the cutoff, C3: MONO2K 1.35 dB quieter (C4 noise wants no more Q)")
    for q, d in ((48, 32), (48, 48), (64, 32), (64, 40)):               # linear: the level at the cutoff
        d12(f"d12_b16_q{q:03d}_n48_d{d:03d}")
    for d in (32, 48):
        d12(f"d12_b16_q048_n72_d{d:03d}", note=72)
    d12("d12_b16_q096_n48_d016")
    d12("d12_b16_q096_n48_d032", xfail="HPQ 96 at the cutoff, C3, DIST 32: MONO2K 2 dB louder, near the saturation")
    for q, ds in ((32, (60, 64)), (48, (52, 56, 64)), (64, (48, 56, 64)), (96, (40, 48, 64))):   # saturated
        for d in ds:
            d12(f"d12_b16_q{q:03d}_n48_d{d:03d}", xfail=sat)
    for d in (56, 64):
        d12(f"d12_b16_q048_n72_d{d:03d}", note=72, xfail=sat)
    for q in (48, 96):                                # the cutoff an octave under the note (passband), over it (stopband)
        d12(f"d12_b08_q{q:03d}_n48_d064", xfail="the passband at HPQ 48 / 96, DIST 64: MONO2K 1.8 dB louder")
        d12(f"d12_b24_q{q:03d}_n48_d064", xfail="an octave under a resonant HP's cutoff the reference falls 7.6-9.4 dB further")
    for kind in ("lp", "hp"):                         # the filter follows GND-SIN's TUNE (chain_tune13); the reference
        for t in (32, 48, 56, 72, 80, 95):            # itself scatters +-0.5 dB, a fixed filter loses 2 dB at 25 c
            add("tune", "chain_tune13", f"t13_{kind}_{t:03d}", "tone_rel", {"ref": f"t13_{kind}_064"}, 1.0)
        for t in (32, 48, 80, 95):
            add("tune", "chain_tune13", f"t13_{kind}_{t:03d}", "pitch_c", {"note": 60}, 1.0)
    firsts = [0, 2, 6, 10, 14, 18, 22, 26, 30, 33, 37, 41, 45, 49, 53, 57, 61, 64, 68, 72, 76, 80, 84, 88, 92, 95, 99,
              103, 107, 111, 115, 119, 123, 128]                 # SWAV-ENS (chain_ens20): each PCH group's ends
    for v in sorted({v for a, b in zip(firsts, firsts[1:]) for v in (a, b - 1)}):
        add("ens", "chain_ens20", f"e20_pch2_{v:03d}", "ens_voice", {"ref": "e20_pch2_061"}, 1.0)
    for w in (0, 32, 64, 96, 127):                                  # WAVE x PW: the level and C3's partials
        for p in (0, 16, 32, 48, 64, 96, 127):
            add("ens", "chain_ens20", f"e20_w{w:03d}_p{p:03d}", "c3_partials", {}, [1.5] + [3.0] * 6,
                xfail="WAVE 127 at PW 0 / 127: the reference keeps a residue -66 / -63 dB where the two saws cancel"
                if w == 127 and p in (0, 127) else None)
    for v in (0, 32, 63, 64, 65, 96, 127):
        add("ens", "chain_ens20", f"e20_tune_{v:03d}", "pitch_c", {"note": 60}, 1.0)
    for n in ("e20_v2", "e20_v3", "e20_v4"):                        # no level shared between the voices
        add("ens", "chain_ens20", n, "rms", {}, 0.5)
    for v in (0, 16, 32, 64, 96, 127):                              # the chorus, mono here: its mix by CHRL
        add("ens", "chain_ens20", f"e20_chrl_{v:03d}", "chorus_mix", {}, 1.5)
    for v in (0, 64, 127):                                           # CHRW: the mix holds; the stereo is the gap
        add("ens", "chain_ens20", f"e20_chrw_{v:03d}", "chorus_mix", {}, 1.5,
            xfail="CHRW 0: the reference's copy moves faster (~0.66 Hz) and stands 2 dB further off the dry line" if v == 0 else None)
        add("ens", "chain_ens20", f"e20_chrw_{v:03d}", "lr_corr", {}, 0.02)
    # its chorus across the stereo, held 12 s (chain_ens41): one LFO, 0.2 Hz and +-16.8 c whatever CHRW, two copies a
    # quarter turn apart, CHRW setting them apart (L/R correlation, the copy's side against its mid); the channels' phase
    # on each other from CHRW 64. Below it the reference's channels already swing a quarter turn apart, MONO2K's less
    apart = ("CHRW 32: the reference's channels swing a quarter turn apart (+-16.7 c each) while 0.99 alike; MONO2K's"
             " blend of the two copies keeps them closer (28 degrees, +-11.6 c); their correlation and width match")
    for w in (0, 32, 64, 96, 127):
        add("ens_stereo", "chain_ens41", f"e41_l127_w{w:03d}", "chorus_stereo", {}, [0.02, 3.0, 20.0, 0.02, 1.5],
            xfail=apart if w == 32 else None)
    add("ens_stereo", "chain_ens41", "e41_l064_w127", "lr_corr", {}, 0.05)
    # SWAV-ENS's acceptance: bank A's eight parts (ea69d53, converted to format 7), each voice against the
    # Monomachine's, held and as the part's own trig; a stab of ~0.1 s cannot resolve a cluster's voices: INFO
    import ens_voices as EV
    for name, (note, cc) in EV.spec_lines(os.path.join(HERE, "chain_ensacc.spec")).items():
        stab = name.endswith("_trig") and any(k in name for k in ("a01_t4", "a07_t4", "a13_t4"))
        add("ens_accept", "chain_ensacc", name, "ens_voices", {"note": note, "cc": {str(k): cc[k] for k in (48, 49, 50, 55)}},
            3.0 if name.endswith("_held") else 5.0, info=stab)
    for kind, tol in (("res", 6.0), ("nores", 1.5), ("open", 1.5)):   # a resonant sweep bends a short note flat:
        for n in (45, 43, 41, 40, 52, 48, 57, 55):                    # the Monomachine's too (chain_sidbend14)
            add("bend", "chain_sidbend14", f"s14_{kind}_n{n}", "bend_c", {"note": n}, tol)
    hz = [60, 150, 400, 800, 1600, 3200, 6400]
    top = "the EQ's top band: the reference's skirts stay wide toward Nyquist, MONO2K's warp narrow"
    for v in (0, 16, 32, 48, 64, 80, 96, 112, 127):
        add("eq", "chain_eq2", f"e2_f_{v:03d}_up", "resp_at", {"ref": "e2_open", "hz": hz}, 2.5, xfail=top if v >= 112 else None)
    for v in (12, 28, 44, 60, 76, 92, 108):
        add("eq", "chain_eq2", f"e2_f_{v:03d}_dn", "resp_at", {"ref": "e2_open", "hz": hz}, 2.5, xfail=top if v >= 108 else None)
    for v in (0, 16, 32, 48, 80, 96, 112, 127):
        add("eq", "chain_eq2", f"e2_g_{v:03d}", "resp_at", {"ref": "e2_open", "hz": hz}, 2.5)
    period = {32: 4.706, 48: 6.589, 64: 8.471, 80: 10.182, 96: 12.003, 112: 14.0, 127: 15.677}   # measured (README)
    for v, p in period.items():
        add("srr", "chain_srr2", f"s2_{v:03d}", "srr_image", {"near": round(SR / p - f0_of(84))}, ["1%", 1.5])
    add("srr", "chain_srr2", "s2_032", "level_at", {"hz": [1709]}, 3.0,
        xfail="the hold's fractional period leaves a spur (1709 Hz on a C6 at SRR 32) 10 dB stronger than the reference's")
    for v, T in ((8, 62.5), (16, 125), (32, 250), (48, 375), (64, 500), (96, 750), (127, 992.2)):
        add("delay_time", "chain_delay", f"d_tim_{v:03d}", "echo_ms", {"at": T}, 1.0)
    for v in (0, 16, 32, 48, 80, 96, 112, 127):
        add("delay_send", "chain_delay", f"d_snd_{v:03d}", "echoes", {"at": [258]}, 0.5,
            xfail="DSND straight in level since format 17 (the owner's, 2026-10-26: a third of the knob was near"
                  " silent); the Monomachine's knob is the square, |d / 64|^2: an old pattern converts to it"
            if 0 < v < 127 else None)
    for v in (56, 64):
        add("delay_fb", "chain_delay2", f"d2_fb_{v:03d}", "echoes", {"at": [132, 257]}, 0.5)
    add("delay_fb", "chain_delay2", "d2_fb_068", "echoes", {"at": [132, 257]}, 0.5)   # the "growth" was a control
                                                                                     # carried from the line before
    # (ping-pong's leak to the wrong side at -32 dB, a known gap, was its one-poles stalled off zero: gone with them)
    for s in (0, 32, 96, 127):
        add("delay_pan", "chain_delay2", f"d2_pan_s{s:03d}_f048", "echoes", {"at": [258, 515]}, 1.0,
            xfail="DSND straight in level since format 17: a send half way 6 dB louder than the Monomachine's square"
            if s in (32, 96) else None)
    for n in ("d2_bas_032", "d2_bas_064", "d2_bas_096", "d2_wid_032", "d2_wid_064", "d2_wid_096", "d2_bw_032_032", "d2_default"):
        add("delay_filter", "chain_delay2", n, "echo_bands", {"at": 0.379, "hz": [125, 250, 500, 1000, 2000, 4000]}, 3.0,
            xfail="375 ms ping-pong runs past MONO2K's half line: the slowed clock darkens 2-4 kHz" if n == "d2_bas_096" else None)
        add("delay_filter", "chain_delay2", n, "echo_bands", {"at": 0.379, "hz": [8000, 16000]}, 3.0, info=True)
    for d in (32, 48, 64, 80, 96):
        add("fenv", "chain_ftime5", f"ft_dec_{d:03d}", "fenv_ms", {"kind": "dec"}, 0.05, rel=True)
    for v in (80, 96, 112):
        add("fenv", "chain_ftime5", f"ft_atk_{v:03d}", "fenv_ms", {"kind": "atk"}, 0.08, rel=True)
    add("fenv", "chain_ftime5", "ft_atk_064", "fenv_ms", {"kind": "atk"}, 0.08, rel=True,
        xfail="the reference's attack starts slower than a straight line (crossing 15 % of it at 26 ms, MONO2K 19 ms)")
    # FM+STAT and FM+DYN on the Monomachine's pages (chain_fm26): partials against the strongest, so each depth, ratio,
    # decay, lowpass and feedback reads as the spectrum it makes
    def odd(fc, top=9):
        return [fc * k for k in range(1, top + 1, 2)]
    fc = f0_of(48)
    for m in ("s", "d"):
        add("fm", "chain_fm26", f"f26{m}_carrier", "rms", {}, 0.3)
    for v in (16, 32, 48, 64):
        add("fm", "chain_fm26", f"f26s_i1_{v:03d}", "fm_side", {"hz": odd(fc)}, 1.0)
    for v in (80, 112):                            # high depths: their index (the partials swing through J's zeros)
        add("fm", "chain_fm26", f"f26s_i2_{v:03d}", "fm_beta", {"note": 48, "ratio": 4}, "3%")
    for n in (60, 72, 84):                         # a modulator at 1, 2, 4 kHz: FM+STAT's lowpass takes its depth
        add("fm", "chain_fm26", f"f26s_lp_n{n}", "fm_side", {"hz": odd(f0_of(n), 5)}, 1.0)
    R24 = {6: 3 / 16, 13: 3 / 4, 17: 1.5, 22: 3.5}
    for i, r in R24.items():
        add("fm", "chain_fm26", f"f26s_r1_{i:02d}", "fm_side", {"hz": [abs(1 - r) * fc, (1 + r) * fc, (1 + 2 * r) * fc]}, 1.5)
    add("fm", "chain_fm26", "f26s_fin_096", "fm_side", {"hz": [fc, 2.125 * fc, 0.125 * fc, 3.25 * fc]}, 1.5)
    split = ("high feedback: the Monomachine's carrier moves off the note (fc + 0.14 fc at 48, a quarter at 56..72, then"
             " noise), MONO2K's stays harmonic until it aliases")
    for m, top in (("s", 48), ("d", 48)):
        for v in (16, 32, 48):
            add("fm", "chain_fm26", f"f26{m}_fb_{v:03d}", "fm_side", {"hz": [fc * k for k in range(1, 9)]}, 1.5,
                xfail=split if v == 48 else None)
    add("fm", "chain_fm26", "f26s_fb_064", "grit", {"note": 48}, 3.0, xfail=split)
    add("fm", "chain_fm26", "f26d_fb_096", "grit", {"note": 48}, 3.0, xfail=split)
    for v in (88, 104):
        for t in (0.02, 0.15, 0.4):
            add("fm", "chain_fm26", f"f26s_env_{v:03d}", "fm_side", {"hz": [3 * fc, 5 * fc], "t": t, "n": 2048}, 2.0)
    for v in (16, 32):
        add("fm", "chain_fm26", f"f26s_tone_{v:03d}", "fm_side", {"hz": odd(fc, 7)}, 1.5)
    for v in (32, 64):                             # DYN's 1VOL on a 1.5 ratio
        add("fm", "chain_fm26", f"f26d_v1_{v:03d}", "fm_side", {"hz": [k * fc for k in (0.5, 2, 2.5, 3.5, 4, 5.5)]}, 1.0)
    for v in (96, 120):
        add("fm", "chain_fm26", f"f26d_v1_{v:03d}", "fm_beta", {"note": 48, "ratio": 1.5, "k": 20}, "3%")
    for v in (32, 64):
        add("fm", "chain_fm26", f"f26d_e2_{v:03d}", "fm_side", {"hz": odd(fc)}, 1.0)
    for t in (0.02, 0.15, 0.4):
        add("fm", "chain_fm26", "f26d_d2_096", "fm_side", {"hz": [3 * fc, 5 * fc], "t": t, "n": 2048}, 2.0)
    for v, r in ((16, 0.25), (112, 1.75)):
        add("fm", "chain_fm26", f"f26d_r1_{v:03d}", "fm_side", {"hz": [abs(1 - r) * fc, (1 + r) * fc, (1 + 2 * r) * fc]}, 1.5)
    for v, r in ((48, 0.5625), (111, 111 * 111 / 4096)):
        add("fm", "chain_fm26", f"f26d_r2_{v:03d}", "fm_side", {"hz": [abs(1 - r) * fc, (1 + r) * fc, (1 + 2 * r) * fc]}, 1.5)
    for name, ts in (("f26d_ven_088", (0.3, 0.9)), ("f26d_ven_032", (0.05, 0.2, 0.6))):   # the push; the cut turning
        for t in ts:                                                                             # over (a ratio-2 operator:
            add("fm", "chain_fm26", name, "fm_side", {"hz": odd(fc, 7), "t": t, "n": 2048}, 3.0)    # its phase shows too)
    for name, ts in (("f26d_vp_088", (0.0, 0.1, 0.3, 0.7, 1.2)), ("f26d_vn_032", (0.0, 0.06, 0.15, 0.4, 0.8))):
        for t in ts:
            add("fm", "chain_fm26", name, "fm_beta", {"note": 48, "ratio": 1.5, "t": t, "n": 1102, "k": 20}, 0.15)
    add("fm", "chain_fm26", "f26d_fen_096", "fen_hz", {"note": 48, "at": [0.03, 0.1, 0.3, 0.6]}, "3%")
    add("fm", "chain_fm26", "f26d_fen60_096", "fen_hz", {"note": 60, "at": [0.03, 0.1, 0.3, 0.6]}, "3%")
    add("fm", "chain_fm26", "f26d_fen_032", "fen_hz", {"note": 48, "at": [0.03, 0.08, 0.6, 1.2], "mode": "peak"}, "4%")
    for v in (0, 127):
        add("fm", "chain_fm26", f"f26d_tune_{v:03d}", "pitch_c", {"note": 48}, 1.0)
    # FM+'s 24 ratio steps where the Monomachine's sound begins each and the value under (chain_fmstep43): the sidebands
    # of the step it plays, (v * 24 + 8) >> 7 (its screen labels these eight values with the step before)
    R24 = [1 / 64, 1 / 32, 1 / 16, 3 / 32, 1 / 8, 5 / 32, 3 / 16, 1 / 4, 5 / 16, 3 / 8, 7 / 16, 1 / 2, 5 / 8, 3 / 4, 7 / 8,
           1, 1.25, 1.5, 1.75, 2, 2.5, 3, 3.5, 4]
    fc6 = f0_of(84)
    for b in (5, 21, 37, 53, 69, 85, 101, 117):
        for v in (b - 1, b):
            r = R24[(v * 24 + 8) >> 7]
            hz = [(1 + r) * fc6, (1 + 2 * r) * fc6] + ([abs(1 - r) * fc6] if abs(1 - r) > 0.05 else [])
            for k in ("s1", "p1", "p2", "p3"):
                add("fmstep", "chain_fmstep43", f"fs43{k}_{v:03d}", "fm_side", {"hz": hz}, 1.5)
            add("fmstep", "chain_fmstep44", f"fs44s2_{v:03d}", "fm_side", {"hz": hz}, 1.5)   # FM+STAT's 2FRQ alike
    # FX-RING on the Monomachine's RINGMOD (ring45.loops): the input after the source's VOL, clip and LEV, INP's square
    # law into its clip, WAVE's carrier, MIX, EXT's fade; the products' levels at the right channel
    fs_, fc_ = f0_of(60), f0_of(66)
    up, lo, c3p, c3m = fc_ + fs_, fc_ - fs_, fc_ + 3 * fs_, abs(fc_ - 3 * fs_)
    for n in ("inp016", "inp040", "inp064", "vol032", "lev040", "mix064", "ext064"):
        add("ring", "ring45", f"r45_{n}", "ring", {"hz": [up, lo]}, 0.5)
    for n in ("d48i088", "d48i127", "d64i127", "vol127"):           # clipped: the input's own odd products with them
        add("ring", "ring45", f"r45_{n}", "ring", {"hz": [up, c3p, c3m]}, [0.5, 1.5, 1.5])
    add("ring", "ring45", "r45_lev000", "ring", {"hz": [up]}, 1.0)                  # at the source's LEV 0, nothing
    add("ring", "ring45", "r45_mix000", "ring", {"hz": [fs_, up], "floor": -110.0}, 0.5)   # the dry source, no product
    for n in ("wav032", "wav064", "wav096", "wav127"):              # the carrier's odd harmonics, k c + s
        add("ring", "ring45", f"r45_{n}", "ring", {"hz": [up] + [k * fc_ + fs_ for k in (3, 5, 7)]}, 0.7)
    # the amp's HOLD in beats, v / 32 of one at the pattern's tempo (chain_hold46: 60, 120, 150 BPM)
    for b in (60, 120, 150):
        for h in (2, 8, 32, 96):
            add("hold", "chain_hold46", f"h46_t{b:03d}_h{h:03d}", "hold_ms", {"note": 60}, 2.5)
    # VO-6 on the Monomachine's page (chain_vo47): F1 and F2 whispered (Hz, within 3 %), its level over the vowels
    # voiced and whispered (dB, within 1.5), VOIC's voice and noise (within 1.5 dB), its pitch in whole hertz (cents),
    # its fade-in from the trig (dB at 20 .. 150 ms against the held level, within 3)
    for v in (0, 24, 48, 72, 96, 127):
        add("vo6", "chain_vo47", f"vo47_f1_{v:03d}", "vo_formant", {"ranges": [[120, 900]]}, "3%")
    for v in (8, 40, 72, 104):
        add("vo6", "chain_vo47", f"vo47_f2_{v:03d}", "vo_formant", {"ranges": [[420 if v < 30 else 480, 2400]]}, "3%")
    for ab in ("000_000", "064_064", "127_127", "024_100", "100_024", "088_120", "040_116"):
        add("vo6", "chain_vo47", f"vo47_lv_{ab}", "rms", {}, 1.5)
        add("vo6", "chain_vo47", f"vo47_lw_{ab}", "rms", {}, 1.5)
    for v in (40, 72, 100):
        add("vo6", "chain_vo47", f"vo47_vc_{v:03d}", "vo_split", {"note": 48}, 1.5)
    for n in (30, 48, 66, 90):
        add("vo6", "chain_vo47", f"vo47_nt_{n}", "vo_f0", {"note": n}, 2.0 if n < 36 else 1.0)   # (low: a faint h1)
        add("vo6", "chain_vo47", f"vo47_nt_{n}", "rms", {}, 1.5)
    for t in (0, 40, 127):
        add("vo6", "chain_vo47", f"vo47_tn_{t:03d}", "vo_f0", {"note": 48}, 1.0)
    add("vo6", "chain_vo47", "vo47_lv_064_064", "vo_onset", {"at": [0.02, 0.05, 0.08, 0.11, 0.15]}, 3.0)
    peak = ("FX-RING's carrier: the Monomachine's is peakier above WAVE 96 (its peak 4.4 dB further over the fundamental"
            " at 127): harmonics past the 15th, which T_RINGW has not")
    for n in ("wav096", "wav127"):
        add("ring", "ring45", f"r45_{n}", "ring", {"hz": [up], "pk": 1}, [0.5, 0.7], xfail=peak)
    # SID-6581 on the Monomachine's page (chain_sid36)
    mix = "SID's MIX: MONO2K's is the chip's triangle AND saw; the Monomachine's combines them otherwise (its level matches)"
    top = ("SID's top: the Monomachine's dips ~2.5 dB around 4-5 kHz and stops at 15 kHz (its noise quieter as it"
           " climbs); MONO2K's is flat to its band-limiting's roll-off")
    for w in ("tri", "saw", "pul", "mix", "noi"):
        add("sid", "chain_sid36", f"s36_w_{w}_n48", "rms", {}, 0.5)
        if w == "noi":
            continue
        for n in (36, 72):
            add("sid", "chain_sid36", f"s36_w_{w}_n{n}", "fm_side", {"hz": [f0_of(n) * k for k in range(1, 9)]}, 1.5,
                xfail=mix if w == "mix" else top if n == 72 else None)
    for n in (36, 72):
        add("sid", "chain_sid36", f"s36_w_noi_n{n}", "rms", {}, 1.0, xfail=top if n == 72 else None)
    for v in (16, 32, 48, 56, 96):                 # the duty: |PW - 64| / 128
        add("sid", "chain_sid36", f"s36_pw_{v:03d}", "rms", {}, 0.5)
        add("sid", "chain_sid36", f"s36_pw_{v:03d}", "fm_side", {"hz": [fc * k for k in range(1, 9)]}, 1.5)
    for v in (16, 64, 127):                        # PWAD's sweep: the level a pulse's duty gives, over time
        for a in (0.1, 0.35, 0.75, 1.3):
            add("sid", "chain_sid36", f"s36_pwad_{v:03d}", "rms", {"t": (a, a + 0.06)}, 1.5)
    for f in (48, 80, 112):                        # MFRQ: C3 at 0, an octave every 32
        fm = fc * 2 ** (f / 32)
        add("sid", "chain_sid36", f"s36_ring_{f:03d}", "fm_side", {"hz": [fc, fm - fc, fm, fm + fc]}, 2.0)
        near = sorted(range(1, 40), key=lambda k: abs(k * fc - fm))[:3]
        for m in ("sync", "rs"):
            add("sid", "chain_sid36", f"s36_{m}_{f:03d}", "fm_side", {"hz": [k * fc for k in sorted(near)]}, 3.0)
    add("sid", "chain_sid36", "s36_ringtri_080", "fm_side", {"hz": [fc, 740 - fc, 740, 740 + fc]}, 2.0)
    for n in (36, 60):                             # the same MFRQ at every note
        f0n = f0_of(n)
        near = sorted(range(1, 40), key=lambda k: abs(k * f0n - fc * 2 ** 2.5))[:2]
        add("sid", "chain_sid36", f"s36_mfrq_n{n}", "fm_side", {"hz": [k * f0n for k in sorted(near)]}, 3.0)
    for v in (0, 127):
        add("sid", "chain_sid36", f"s36_tune_{v:03d}", "pitch_c", {"note": 48}, 1.0)
    # GND-SIN kicks (chain_kick40): the Monomachine's GND-SIN is a sine with TUNE alone (CC 55); its MIDI notes play
    # alike at any velocity; a sine through decays 40 and 60, clean and driven, keeps its level, body and bands. A clean
    # kick's 200-500 Hz share follows the Monomachine's start (its two renders of one setting differ by 5 dB): 6 dB
    onset = ("the Monomachine's sine starts each note where its free-running phase is, after a ~0.5 ms burst (up to"
             " +-0.7 of the peak): 14-28 dB more over 2 kHz in a kick's first 4 ms than MONO2K's, which starts at zero."
             " Not modelled (it may be the emulator's): MONO2K's CLIK is its kick's click")
    # the amp's DEC on MONO2K's own law from pattern format 16 (the owner's choice, gen_tables.py): the same knob decays
    # faster (40: 0.22 s to -60 dB, the Monomachine's 0.36 s; a pattern converted takes 49 for its 40), so each kick's
    # level over 300 ms and its body (ms to -20 dB) are the shorter decay's: held at their size, the machine's rest as it was
    amp = ("the amp's DEC is MONO2K's own law since pattern format 16, by the owner's choice: DEC 20 / 40 / 60 fall to"
           " -60 dB in 61 / 218 / 650 ms, the Monomachine's 94 / 355 / 1341 ms (a format-15 pattern's DEC converts to the"
           " value that times as it did: 26 / 49 / 74), so the kick's level over 300 ms and its body are shorter")
    clean, driven = [0.5, 1.0, 6.0, 10.0], [0.5, 1.0, 1.5, 10.0]
    for v in (64, 100, 127):
        add("kick", "chain_kick40", f"k40_vel_{v:03d}", "kick", {}, clean, xfail=amp)
    add("kick", "chain_kick40", "k40_tune_064", "level_at", {"hz": [2 * f0_of(60)]}, 3.0)
    add("kick", "chain_kick40", "k40_init", "kick", {}, clean, xfail=amp)
    for d in (20, 40, 60):
        for x in (64, 100, 127):
            add("kick", "chain_kick40", f"k40_d{d:03d}_x{x:03d}", "kick", {}, clean if x == 64 else driven,
                xfail=onset + "; and " + amp if d == 20 else amp)
    for c in ("k40_d040_x064", "k40_d040_x100", "k40_d060_x127"):
        add("kick", "chain_kick40", c, "onset_hf", {}, 3.0, xfail=onset)
    # SWAVE-PULS (chain_pul38): a pulse, its duty PW / 128, the sweep, the unison, the sine subs, its DIST ceiling
    for n in (36, 48, 72):
        add("pul", "chain_pul38", f"p38_lev_n{n}", "rms", {}, 0.5)
        add("pul", "chain_pul38", f"p38_lev_n{n}", "fm_side", {"hz": [f0_of(n) * k for k in range(1, 9)]}, 1.5)
    for v in (8, 32, 48, 96):
        add("pul", "chain_pul38", f"p38_pw_{v:03d}", "rms", {}, 0.5 if v > 8 else 1.5)
        add("pul", "chain_pul38", f"p38_pw_{v:03d}", "fm_side", {"hz": [fc * k for k in range(1, 9)], "floor": -30.0}, 2.0)   # its
                                                   # zeros (the duty's) are sharp: floored
    for v in (16, 64):
        for a in (0.1, 0.4, 0.7, 1.0, 1.3):
            add("pul", "chain_pul38", f"p38_pwad_{v:03d}", "rms", {"t": (a, a + 0.06)}, 1.5)
    for l, w in ((64, 64), (127, 16), (127, 127)):
        add("pul", "chain_pul38", f"p38_uni_l{l:03d}_w{w:03d}", "rms", {}, 0.5)
    for k in (1, 2):
        for v in (64, 127):
            add("pul", "chain_pul38", f"p38_sub{k}_{v:03d}", "rms", {}, 0.5)
    for v in (0, 127):
        add("pul", "chain_pul38", f"p38_tune_{v:03d}", "pitch_c", {"note": 48}, 1.0)
    for d in (64, 96, 112, 127):
        add("pul", "chain_pul38", f"p38_drv_d{d:03d}", "rms", {"t": (0.3, 0.95)}, 1.0)
    for m in ("p",):                               # FM+PAR: three of FM+STAT's operators side by side
        add("fm", "chain_fm26", f"f26{m}_carrier", "rms", {}, 0.3)
    for j in (1, 2, 3):
        for v in (32, 64):
            add("fm", "chain_fm26", f"f26p_i{j}_{v:03d}", "fm_side", {"hz": odd(fc)}, 1.0)
        add("fm", "chain_fm26", f"f26p_r{j}_13", "fm_side", {"hz": [0.25 * fc, 1.75 * fc, 2.5 * fc]}, 1.5)
    for v in (88, 104):                            # ENV over 64: 2VOL's depth there, falling
        for t in (0.0, 0.02, 0.15, 0.4):
            add("fm", "chain_fm26", f"f26p_e3_{v:03d}", "fm_beta", {"note": 48, "ratio": 4, "t": t, "n": 1102, "k": 16}, 0.25)
    add("fm", "chain_fm26", "f26p_sum", "fm_side", {"hz": [fc * k for k in range(1, 9)]}, 1.5)   # two at ratio 1: one depth
    for v in (16, 48, 96):                         # TONE: the lowpass's cutoff
        add("fm", "chain_fm26", f"f26p_tone_{v:03d}", "fm_side", {"hz": odd(fc, 7)}, 1.5)
    for v in (56, 80):
        add("fm", "chain_fm26", f"f26s_tone_{v:03d}", "fm_side", {"hz": odd(f0_of(72), 5)}, 1.5)
    # GND-NOIS on the Monomachine's page (chain_gnoi42): RED's colour from white to red, ST's second noise (alone in the
    # middle, across the stereo with STON), TUNE moving the filter's key, the note. Bands within 1 dB, the level 1 dB
    for r in (0, 16, 32, 48, 64, 80, 96, 112, 127):
        add("gnoi", "chain_gnoi42", f"g42_red_{r:03d}", "noise_bands", {}, 1.0)
    for on in ("off", "on"):
        for v in (0, 32, 64, 96, 127):
            add("gnoi", "chain_gnoi42", f"g42_st_{v:03d}_{on}", "noise_bands", {}, 1.0)
            add("gnoi", "chain_gnoi42", f"g42_st_{v:03d}_{on}", "noise_stereo", {}, [0.03, 1.0])
    for v in (64, 127):
        add("gnoi", "chain_gnoi42", f"g42_st_{v:03d}_on_r064", "noise_stereo", {}, [0.03, 1.0])
        add("gnoi", "chain_gnoi42", f"g42_st_{v:03d}_on_r064", "noise_bands", {}, 1.0)
    for v in (0, 32, 96, 127):
        add("gnoi", "chain_gnoi42", f"g42_tune_{v:03d}", "noise_bands", {}, 1.0)
    for n_ in (36, 84):
        for r in (0, 64):
            add("gnoi", "chain_gnoi42", f"g42_note_{n_:03d}_r{r:03d}", "noise_bands", {}, 1.0)
    return C


def key(c):
    return f'{c["group"]}:{c["name"]}:{c["metric"]}:{json.dumps(c["args"], sort_keys=True)}'


def measure(c, d):
    cache = {}
    def ld(n):
        if n not in cache:
            cache[n] = load(os.path.join(d, n + ".wav"))
        return cache[n]
    return METRICS[c["metric"]](ld(c["name"]), c["args"], ld)


def capture():
    out = {"source": "Monomachine OS 1.32B in gearmulator-md-mm (tools/ref/mm_ref), MIDI only; measured numbers only",
           "cases": {}}
    for c in cases():
        d = os.path.join(ROOT, "build", "ref", MM_DIR[c["spec"]])
        out["cases"][key(c)] = [round(v, 4) for v in measure(c, d)]
    with open(REF_JSON, "w") as f:
        json.dump(out, f, indent=0, sort_keys=True)
    print(f"captured {len(out['cases'])} cases into {os.path.relpath(REF_JSON, ROOT)}")


def loops(spec):
    """a loop spec's loops: name -> its lines"""
    out, name = {}, None
    for l in open(os.path.join(HERE, spec + ".loops")):
        if l.startswith("#") or not l.strip():
            continue
        if l.startswith("= "):
            name = l[2:].strip(); out[name] = ""
        else:
            out[name] += l
    return out


def render_loops(exe, spec, names, d, mm=False):
    """each named loop of a spec rendered on its own (in parallel), kept as d/name.wav"""
    import shutil, tempfile
    from concurrent.futures import ThreadPoolExecutor
    L = loops(spec)
    os.makedirs(d, exist_ok=True)
    def one(n):
        with tempfile.TemporaryDirectory() as t:
            subprocess.run(exe + [t] + ([] if mm else ["--loop"]), input=L[n], text=True, cwd=ROOT, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            shutil.copy(os.path.join(t, "loop_mm.wav" if mm else "loop_m2k_driv0.wav"), os.path.join(d, n + ".wav"))
    with ThreadPoolExecutor(6) as ex:
        list(ex.map(one, sorted(names)))


def render_m2k(cs, d):
    os.makedirs(d, exist_ok=True)
    want = {(c["spec"], c["name"]) for c in cs} | {(c["spec"], c["args"]["ref"]) for c in cs if "ref" in c["args"]}
    lines = []
    for spec in sorted({s for s, _ in want} - LOOPS):       # (a panel spec's MONO2K side is a plain spec)
        for l in open(os.path.join(HERE, spec + ".spec")):
            if l.strip() and not l.startswith("#") and (spec, l.split()[0]) in want:
                lines.append(l)
    subprocess.run(["cc", "-O2", "-Ifirmware/src", "-Ifirmware/gen", "-o", "build/host/m2k_ref", "host/m2k_ref.c", "-lm"],
                   cwd=ROOT, check=True)
    subprocess.run([os.path.join(ROOT, "build/host/m2k_ref"), d], input="".join(lines), text=True, cwd=ROOT,
                   check=True, stdout=subprocess.DEVNULL)
    for spec in sorted({s for s, _ in want} & LOOPS):
        render_loops([os.path.join(ROOT, "build/host/m2k_ref")], spec, {n for s_, n in want if s_ == spec}, d)


def errors(c, r, m):
    """MONO2K's error against the reference, one per value (absolute, or a fraction where the case is relative);
    NaN where both are NaN, inf where only one is"""
    tol = c["tol"] if isinstance(c["tol"], list) else [c["tol"]] * len(r)
    out = []
    for a, b, t in zip(r, m, tol):
        if np.isnan(a) or np.isnan(b):
            out.append(float("nan") if np.isnan(a) and np.isnan(b) else float("inf"))
            continue
        rel = c["rel"] or isinstance(t, str)
        out.append(float(abs(b - a) / abs(a) if rel else abs(b - a)))
    return out, [float(t.rstrip("%")) / 100 if isinstance(t, str) else t for t in tol]


def run(groups):
    ref = json.load(open(REF_JSON))["cases"]
    cs = [c for c in cases() if not groups or c["group"] in groups]
    d = os.path.join(ROOT, "build", "ref", "regress_m2k")
    render_m2k(cs, d)
    out = []
    for c in cs:
        r = ref.get(key(c))
        out.append((c, r, None if r is None else measure(c, d)))
    return out


def gaps(groups, widen=False):
    """a new known gap is recorded as it stands; a recorded one only narrows, unless widen (--widen) says otherwise"""
    old = json.load(open(GAPS_JSON))["cases"] if os.path.exists(GAPS_JSON) else {}
    new = {k: v for k, v in old.items()}
    n, kept = 0, []
    for c, r, m in run(groups):
        if not c["xfail"] or r is None:
            continue
        e, tol = errors(c, r, m)
        g = [round(v, 4) if np.isfinite(v) else None for v in e]
        was = old.get(key(c))
        if was is not None and not widen:
            g = [w if w is not None and (v is None or v > w) else v for v, w in zip(g, was)]
            if g != [round(v, 4) if np.isfinite(v) else None for v in e]:
                kept.append(c["name"])
        new[key(c)] = g
        n += 1
    if kept:
        print("held at their recorded size (grown since; --widen to accept):", " ".join(kept))
    live = {key(c) for c in cases() if c["xfail"]}
    new = {k: v for k, v in new.items() if k in live}
    with open(GAPS_JSON, "w") as f:
        json.dump({"note": "MONO2K's error against the reference per known gap (regress.py gaps); differences only",
                   "cases": new}, f, indent=0, sort_keys=True)
    print(f"recorded {n} gaps into {os.path.relpath(GAPS_JSON, ROOT)} ({len(new)} held)")


def check(groups):
    known = json.load(open(GAPS_JSON))["cases"] if os.path.exists(GAPS_JSON) else {}
    rows, fails = [], 0
    for c, r, m in run(groups):
        if r is None:
            rows.append((c, None, None, "BLOCKED", ""))
            continue
        e, tol = errors(c, r, m)
        ok = all(np.isnan(v) or v <= t for v, t in zip(e, tol))
        why = ""
        if c["info"]:
            st = "INFO"
        elif c["xfail"]:
            g = known.get(key(c))
            if ok:
                st, why = "XPASS", "the known gap closed: re-mark the case PASS"
            elif g is None:
                st, why = "FAIL", "a known gap without a recorded size (regress.py gaps)"
            elif any(not np.isnan(v) and v > (float("inf") if gv is None else gv) + t for v, gv, t in zip(e, g, tol)):
                st, why = "FAIL", "the known gap grew: " + " ".join(f"{v:.3g}>{gv}+{t:.3g}" for v, gv, t in zip(e, g, tol)
                                                                     if not np.isnan(v) and gv is not None and v > gv + t)
            else:
                st = "XFAIL"
        else:
            st = "PASS" if ok else "FAIL"
        fails += st in ("FAIL", "XPASS")
        rows.append((c, r, m, st, why))
    res = []
    last = None
    for c, r, m, st, why in rows:
        if c["group"] != last:
            print(f"== {c['group']}")
            last = c["group"]
        if r is None:
            print(f"  {st:5s} {c['name']:18s} no reference measurement")
        else:
            print(f"  {st:5s} {c['name']:18s} {c['metric']:10s} MM " + " ".join(f"{v:8.2f}" for v in r) +
                  "   M2K " + " ".join(f"{v:8.2f}" for v in m) + (f"   ({why})" if why else ""))
        res.append(dict(group=c["group"], name=c["name"], metric=c["metric"], args=c["args"], tol=c["tol"],
                        rel=c["rel"], mm=r, m2k=m, status=st, xfail=c["xfail"], why=why))
    ST = ("PASS", "FAIL", "XFAIL", "XPASS", "INFO", "BLOCKED")
    summary = {s: sum(1 for x in res if x["status"] == s) for s in ST}
    by_group = {}
    for x in res:
        g = by_group.setdefault(x["group"], {s: 0 for s in ST})
        g[x["status"]] += 1
    with open(os.path.join(ROOT, "build", "ref", "regress_result.json"), "w") as f:
        json.dump({"summary": summary, "groups": by_group, "cases": res}, f, indent=1)
    print("groups:", ", ".join(f"{g} {v['PASS']}/{v['PASS'] + v['FAIL']}" + "".join(f" {v[s]} {s.lower()}" for s in ("XFAIL", "XPASS", "INFO") if v[s])
                                for g, v in by_group.items()))
    for x in res:
        if x["status"] == "XPASS":
            print(f"XPASS {x['group']}:{x['name']}: the known gap no longer shows, re-mark it PASS ({x['xfail']})")
        elif x["status"] == "FAIL" and x["why"]:
            print(f"FAIL  {x['group']}:{x['name']}: {x['why']} ({x['xfail']})")
    print("summary:", summary)
    return 1 if fails else 0


if __name__ == "__main__":
    if sys.argv[1:2] == ["mmloops"]:                       # the Monomachine's renders of the loop specs
        for spec in sorted(LOOPS):
            render_loops([os.path.join(ROOT, "build/ref/mm_ref"), sys.argv[2], "loop"], spec, set(loops(spec)),
                         os.path.join(ROOT, "build", "ref", MM_DIR[spec]), mm=True)
        for spec in sorted(PANELS):                        # each part on its own emulator, all into the spec's folder
            from concurrent.futures import ThreadPoolExecutor
            d = os.path.join(ROOT, "build", "ref", MM_DIR[spec]); os.makedirs(d, exist_ok=True)
            parts, cur = [], None
            for l in open(os.path.join(HERE, spec + ".panel")):
                if l.startswith("#"): continue
                if l.startswith("= "): cur = []; parts.append(cur)
                elif cur is not None and l.strip(): cur.append(l)
            run1 = lambda p: subprocess.run([os.path.join(ROOT, "build/ref/mm_ref"), sys.argv[2], "panel", d], input="".join(p),
                                            text=True, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            with ThreadPoolExecutor(6) as ex: list(ex.map(run1, parts))
    elif sys.argv[1:2] == ["capture"]:
        capture()
    elif sys.argv[1:2] == ["check"]:
        sys.exit(check(set(sys.argv[2:])))
    elif sys.argv[1:2] == ["gaps"]:
        gaps({a for a in sys.argv[2:] if a != "--widen"}, "--widen" in sys.argv[2:])
    else:
        print("regress.py capture | check [GROUP ...] | gaps [GROUP ...]")
