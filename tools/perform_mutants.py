#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Mutation tests of PERFORM latched (1.11): FX latched across pages (perf_own, latch_frees, its header tag, its LEDs)
# and the effects latched by a double tap (pf_press, pf_released, pf_unlatch), and a repeat's lap to a 4096th of a
# sample (m2k_fx.c g_lenq). Each mutant is one small change; the self-test (m2k_host --selftest) must fail on it, the
# check the mutant aims at among its failures. Each is made in a copy under build/perform_mut (the tree is never
# touched: a run cut short leaves nothing to restore). The Move's side (its pads, m2k_move_ui.c, and pf_unlatch's own
# letting go, which the FM-1's FX does before it) is held by its bench (move/test/bench.c ctl, through move/proof.sh).
#   .venv/bin/python tools/perform_mutants.py [TREE]
import os, shutil, subprocess, sys
W = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
U = "firmware/src/m2k_ui.c"
X = "firmware/src/m2k_fx.c"
PAGES = "PERFORM latched across pages"
FX = "PERFORM effects latched"
M = [
 ("a page button lets FX's latch go", U,
  "return ui.latch && l != B_FX && b != l &&", "return ui.latch && b != l &&", PAGES),
 ("away, the bus's knobs and banks still", U,
  "    return !ui.paway;\n}", "    return 1;\n}", PAGES),
 ("FX held away: no own view", U,
  "    if (ui.btn[B_FX] || ui.latch != B_FX + 1) return 1;", "    if (ui.latch != B_FX + 1) return 1;", PAGES),
 ("FX tapped away: not let go", U,
  "if (!i && ui.latch == B_FX + 1) { if (!used && hd < 400) latch_off(); return; }",
  "if (!i && ui.latch == B_FX + 1) { return; }", PAGES),
 ("no FX in the header", U,
  "ui.latch == B_FX + 1, sq.arp_hold[ui.trk]", "0, sq.arp_hold[ui.trk]", PAGES),
 ("PERFORM's white keys lit as notes in GRID", U,
  "if (lay && (lay != 2 || ui.kw[k] < 0)) continue;", "if (lay) continue;", PAGES),
 ("an effect never latches", U,
  "    if (!dbl) return 0;", "    if (1) return 0;", FX),
 ("a latched effect not let go by a tap", U,
  "    if (ui.pf_lat & m) {", "    if (0 && (ui.pf_lat & m)) {", FX),
 ("slow taps latch", U,
  "ui.now - ui.pf_tap_t < PF_DT_MS", "ui.now - ui.pf_tap_t < 100000", FX),
 ("FX's latch let go: the effects' latches kept", U,
  "pf_unlatch(); ui.paway = 0; }", "ui.paway = 0; }", PAGES),
 ("FX let go: the effects' latches kept", U,
  "bus_perform(k, 0); pf_unlatch(); }", "bus_perform(k, 0); }", FX),
 ("a move keeps a latched move on", U,
  "    if (pf_ring(e))\n", "    if (0 && pf_ring(e))\n", FX),
 ("a repeat's lap in whole samples (drifting off the beat)", X,
  "    bus.g_lenq = lq;", "    bus.g_lenq = len << 12;", "a latched repeat lands on the beat"),
]


def main():
    base = os.path.join(W, "build", "perform_mut")
    os.makedirs(base, exist_ok=True)
    survived = 0
    for i, (name, f, old, new, aim) in enumerate(M):
        d = os.path.join(base, str(i))
        shutil.rmtree(d, ignore_errors=True)
        shutil.copytree(os.path.join(W, "firmware", "src"), os.path.join(d, "firmware", "src"))
        shutil.copytree(os.path.join(W, "host"), os.path.join(d, "host"))     # (m2k_host.c reaches ../firmware/src)
        path = os.path.join(d, f)
        s = open(path).read()
        if s.count(old) != 1:
            print(f"  MISSING {name}: its line is not in {f} once ({s.count(old)})")
            survived += 1
            continue
        open(path, "w").write(s.replace(old, new))
        exe = os.path.join(d, "m2k_host")
        b = subprocess.run(["cc", "-O2", "-w", "-I" + os.path.join(d, "firmware", "src"), "-I" + os.path.join(W, "firmware", "gen"),
                            "-o", exe, os.path.join(d, "host", "m2k_host.c"), "-lm"], capture_output=True, text=True)
        if b.returncode:
            print(f"  BUILD   {name}: does not build\n{b.stderr[-600:]}")
            survived += 1
            continue
        r = subprocess.run([exe, "--selftest"], capture_output=True, text=True)
        hit = [l for l in r.stdout.splitlines() if l.startswith("FAIL") and aim in l]
        if r.returncode and hit:
            print(f"  caught  {name}: {hit[0].strip()[:110]}")
        else:
            print(f"  SURVIVED {name}")
            survived += 1
        shutil.rmtree(d, ignore_errors=True)
    print(f"perform mutants: {len(M) - survived} of {len(M)} caught" + ("" if not survived else f", {survived} SURVIVED"))
    return 1 if survived else 0


sys.exit(main())
