#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Mutation tests of the FM-1's CPU guard (1.11): each mutant is one small change to the guard (the core's m2k_guard in
# m2k_ui.c, its fades in m2k_dsp.c and m2k_fx.c, POLY's side of it in m2k_seq.c, the glue's pacing in
# firmware/fm1/m2k_pace.h); the guard bench
# (host/m2k_guard_bench.c) or the self-test's GUARD (no traces: m2k_host --selftest) must fail on every one, the check
# the mutant aims at among its failures. A survivor is a rule neither holds. Each mutant is made in a copy under
# build/guard_mut (the tree itself is never touched: a run cut short leaves nothing to restore).
#   .venv/bin/python tools/guard_mutants.py [TREE]
import os, shutil, subprocess, sys
W = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
U = "firmware/src/m2k_ui.c"
D = "firmware/src/m2k_dsp.c"
X = "firmware/src/m2k_fx.c"
S = "firmware/src/m2k_seq.c"
P = "firmware/fm1/m2k_pace.h"
M = [
 ("bank A sheds: danger from half the real time", U,
  "const int over = eff > 100 && ahead * 100 < (eff - 100) * (GRD_SEC / 8)",
  "const int over = eff > 50 && ahead * 100 < (eff - 50) * (GRD_SEC / 8)",
  ["A01", "A02", "A03", "A04", "A04+15"], "bank A never sheds"),
 ("no track ever rests", U,
  "if (t < 0 && sound > 1 && rs != REST_NONE && (t = grd_rest(rs)) >= 0) what = GR_REST;",
  "if (t < 0 && sound > 2 && rs != REST_NONE && (t = grd_rest(rs)) >= 0) what = GR_REST;",
  ["dense110"], "runs short no more often"),
 ("a stall's short at 70 % taken for the render's", U,
  "if (eff > 90 || (eff > 80 && grd.ev_t && now - grd.ev_t < GRD_SEC)) sound = 2;",
  "if (eff > 60 || (eff > 80 && grd.ev_t && now - grd.ev_t < GRD_SEC)) sound = 2;",
  ["hiccup80"], "a short at 70-85 %"),
 ("the screen never asked to give way (the core)", U,
  "int m2k_guard_gap(void) { return (int32_t)(grd.wide_t - grd.tick) > 0 ? GRD_GAP : 0; }",
  "int m2k_guard_gap(void) { return 0; }",
  ["hiccup80"], "a short at 70-85 %"),
 ("the screen does not give way (the glue)", P,
  "const uint32_t g = (uint32_t)m2k_guard_gap(), gap",
  "const uint32_t g = 0u * (uint32_t)m2k_guard_gap(), gap",
  ["hiccup80"], "a short at 70-85 %"),
 ("a cascade: a step's saving not credited", U,
  "    grd.credit += grd_saving(t, dly);",
  "    grd.credit += 0 * grd_saving(t, dly);",
  ["dense"], "no cascade"),
 ("a cascade: no wait after a step", U,
  "#define GRD_SETTLE 4 ",
  "#define GRD_SETTLE 0 ",
  ["rest-none"], "no cascade"),
 ("bars heavy and light: no longer wait after one came back only to rest", U,
  "if (grd.back_t && now - grd.back_t < 4 * GRD_SEC) grd.calm =",
  "if (0 && now - grd.back_t < 4 * GRD_SEC) grd.calm =",
  ["waves70"], "no more rests than"),
 ("back on the load's last value, not held a while", U,
  "if (grd.slow + cores_q8(cost_q8(trk[t].park_cost)) >= (85 << 8)) grd.calm_t = now;",
  "if (0 && grd.slow + cores_q8(cost_q8(trk[t].park_cost)) >= (85 << 8)) grd.calm_t = now;",
  ["waves70"], "no more rests than"),
 ("a rested track back only far under the line", U,
  "if (grd.slow + cores_q8(cost_q8(trk[t].park_cost)) >= (85 << 8)) grd.calm_t = now;",
  "if (grd.slow + cores_q8(cost_q8(trk[t].park_cost)) >= (40 << 8)) grd.calm_t = now;",
  ["recover"], "room again"),
 ("no tail first: a track rests at once", U,
  "int dly = 0, what = GR_TAIL, t = grd_tail(&dly);",
  "int dly = 0, what = GR_TAIL, t = -1 + 0 * grd_tail(&dly);",
  ["spike"], "a tail is cut first"),
 ("REST's track ignored", U,
  "if (rs >= 1 && rs <= M2K_TRACKS6 && rs <= M2K_TRACKS && !trk[rs - 1].held) return rs - 1;",
  "if (rs >= 99 && rs <= M2K_TRACKS6 && rs <= M2K_TRACKS && !trk[rs - 1].held) return rs - 1;",
  ["rest-t3"], "REST T3"),
 ("REST NONE rests anyway", U,
  "if (t < 0 && sound > 1 && rs != REST_NONE && (t = grd_rest(rs)) >= 0) what = GR_REST;",
  "if (t < 0 && sound > 1 && (t = grd_rest(rs)) >= 0) what = GR_REST;",
  ["rest-none", "dense110"], "REST NONE"),
 ("AUTO on the moment's level (a hit a beat taken for quiet between two)", U,
  "        grd.lv[t] += (grd.hold[t] - grd.lv[t]) / 512;",
  "        grd.lv[t] = h;",
  None, "GUARD"),
 ("a tail's fade kills the note that comes in it", D,
  "        if (t->astage == 1 && !t->held) t->park = 0;",
  "        if (t->astage == 9 && !t->held) t->park = 0;",
  None, "GUARD"),
 ("a tail cut rests the track", D,
  "    t->held = (uint8_t)(hold != 0);",
  "    t->held = 1; (void)hold;",
  None, "GUARD"),
 ("the echoes cut never wiped (the line stays awake)", X,
  "    if (t->dcut == 1 && !dio) {",
  "    if (t->dcut == 99 && !dio) {",
  None, "GUARD"),
 ("CPU FULL says nothing of a tail", U,
  "else if (cpu.shed) s_cpy(s_trk(e, cpu.faded), cpu.what == GR_VOICE ? \" VOICE CUT\" : \" TAIL CUT\");",
  "else if (cpu.shed) s_cpy(s_trk(e, cpu.faded), \" FADED\");",
  None, "GUARD"),
 ("the track in view kept resting", U,
  "    if (trk[ui.trk].held && rs != ui.trk + 1) grd_back(ui.trk);",
  "    if (0 && trk[ui.trk].held && rs != ui.trk + 1) grd_back(ui.trk);",
  None, "GUARD"),
 # POLY: a chord's voice cut (guard_voice_steal) between a tail and a rest; poly_pick and the guard agreeing
 ("POLY's voices never cut: the hook not called", U,
  "    if (t < 0 && (t = guard_voice_steal()) >= 0) what = GR_VOICE;",
  "    if (t < 0 && 0 && (t = guard_voice_steal()) >= 0) what = GR_VOICE;",
  ["poly6"], "a POLY chord from the steps"),
 ("POLY's voices never cut: the hook finds none", U,
  "    if (!pv.on) return -1;\n    int best = -1;",
  "    if (pv.on) return -1;\n    int best = -1;",
  ["poly6keys"], "a POLY chord held on the keys"),
 ("voices that do not hold: no track rests for them", U,
  "if (t < 0 && sound > 1 && rs != REST_NONE && grd.vfail_t && now - grd.vfail_t < 2 * GRD_SEC && (t = grd_rest(rs)) >= 0)",
  "if (t < 0 && 0 && sound > 1 && rs != REST_NONE && grd.vfail_t && now - grd.vfail_t < 2 * GRD_SEC && (t = grd_rest(rs)) >= 0)",
  ["poly6x"], "a POLY chord at 190 %"),
 ("a cut voice rests", U,
  "    else trk_park(t, trk_cost(t), what == GR_REST);",
  "    else trk_park(t, trk_cost(t), what != GR_TAIL);",
  None, "GUARD POLY"),
 ("a cut voice keeps its key", S,
  "    if (pv.key[v] == 0xFF) return;\n    pv.key[v] = 0xFF;",
  "    if (pv.key[v] == 0xFF) return;\n    pv.key[v] = pv.key[v];",
  None, "GUARD POLY"),
 ("the lead's own note cut from a chord of steps", U,
  "            for (int i = l + 1; i < l + pv.nv[l]; i++) {",
  "            for (int i = l; i < l + pv.nv[l]; i++) {",
  None, "GUARD POLY"),
 ("a chord's last voice cut", U,
  "            if (n < 2) continue;",
  "            if (n < 1) continue;",
  None, "GUARD POLY"),
 ("a key takes the voice the guard is cutting", S,
  "            if (pass < 2 && (trk[v].held || trk[v].park)) continue;",
  "            if (pass < 2 && trk[v].held) continue;",
  None, "GUARD POLY"),
 ("a key takes a voice the guard rests", S,
  "            if (pass < 2 && (trk[v].held || trk[v].park)) continue;",
  "            if (pass < 2 && trk[v].park) continue;",
  None, "GUARD POLY"),
]

def main():
    base = os.path.join(W, "build", "guard_mut")
    os.makedirs(base, exist_ok=True)
    survived = 0
    for i, (name, f, old, new, traces, aim) in enumerate(M):
        d = os.path.join(base, str(i))
        shutil.rmtree(d, ignore_errors=True)
        shutil.copytree(os.path.join(W, "firmware", "src"), os.path.join(d, "firmware", "src"))
        os.makedirs(os.path.join(d, "firmware", "fm1"))
        shutil.copy(os.path.join(W, "firmware", "fm1", "m2k_pace.h"), os.path.join(d, "firmware", "fm1"))
        shutil.copytree(os.path.join(W, "host"), os.path.join(d, "host"))     # (m2k_host.c reaches ../firmware/src)
        path = os.path.join(d, f)
        s = open(path).read()
        if s.count(old) != 1:
            print(f"  MISSING {name}: its line is not in {f} once ({s.count(old)})")
            survived += 1
            continue
        open(path, "w").write(s.replace(old, new))
        exe = os.path.join(d, "bench")
        b = subprocess.run(["cc", "-O2", "-w", "-I" + os.path.join(d, "firmware", "src"), "-I" + os.path.join(W, "firmware", "gen"),
                            "-I" + os.path.join(d, "firmware", "fm1"), "-o", exe,
                            os.path.join(d, "host", "m2k_guard_bench.c" if traces else "m2k_host.c"), "-lm"],
                           capture_output=True, text=True)
        if b.returncode:
            print(f"  BUILD   {name}: does not build\n{b.stderr[-600:]}")
            survived += 1
            continue
        r = subprocess.run([exe, "--only", *traces] if traces else [exe, "--selftest"], capture_output=True, text=True)
        hit = [l for l in r.stdout.splitlines() if l.strip().startswith("FAIL") and aim in l]
        if r.returncode and hit:
            print(f"  caught  {name}: {hit[0].strip()[:110]}")
        else:
            print(f"  SURVIVED {name} ({' '.join(traces) if traces else 'the self-test'})")
            survived += 1
        shutil.rmtree(d, ignore_errors=True)
    print(f"guard mutants: {len(M) - survived} of {len(M)} caught" + ("" if not survived else f", {survived} SURVIVED"))
    return 1 if survived else 0

sys.exit(main())
