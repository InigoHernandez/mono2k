#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K on the Move, the proof (after move/build.sh):
#   1. the host build on the Mac (clang, as CLAUDE.md builds it) and in the Move's container (gcc 12, aarch64 Linux):
#      the self-test on both, and bank A's renders compared bit for bit: m2k_host --demo's WAVs of A01..A04 and
#      m2k_draw --hash (A01..A04, DENSE and FXS on one core and two); the eight-track host (m2k_host8) too;
#   2. dsp.so itself, driven through the plugin API as Schwung drives it (move/test/bench.c): A01..A04's renders against
#      m2k_draw's hashes; the audio thread's rules with every control, Move's clock and saves (each allocation, file,
#      thread and sleep call made there counted); every control; Move's clock followed through jittered streams; saving
#      and loading; the screen in a few states (build/move/proof/screens/*.pbm); the render's time a block;
#   3. ui.js against a mock of the host (node).
#   sh move/proof.sh [SECONDS]      (SECONDS: the timing's length a pattern, 20 by default)
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
SEC=${1:-20}
IMAGE=mono2k-move-build:bookworm
DSP=build/move/modules/overtake/mono2k/dsp.so
P=build/move/proof
[ -f "$DSP" ] || { echo "no $DSP: run sh move/build.sh first"; exit 1; }
if [ -z "${DOCKER_CONFIG:-}" ] && [ -d "$HOME/.jieli/docker" ]; then
    DOCKER_CONFIG="$HOME/.jieli/docker"
    export DOCKER_CONFIG
fi
rm -rf "$P"
mkdir -p "$P/mac" "$P/linux" "$P/save" "$P/screens"

echo "== the Mac: $(uname -m), $(cc --version | head -1)"
cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o "$P/mac/m2k_host" host/m2k_host.c -lm
cc -O2 -Wall -Wno-unused-function -DM2K_TRACKS=8 -Ifirmware/src -Ifirmware/gen -o "$P/mac/m2k_host8" host/m2k_host.c -lm
cc -O2 -fno-builtin -fno-vectorize -fno-slp-vectorize -DM2K_MUL_OUT -Ifirmware/src -Ifirmware/gen \
    -o "$P/mac/m2k_draw" host/m2k_draw.c
set +e
"$P/mac/m2k_host" --selftest > "$P/mac/selftest.txt" 2>&1; echo "exit $?" >> "$P/mac/selftest.txt"
"$P/mac/m2k_host8" --selftest > "$P/mac/selftest8.txt" 2>&1; echo "exit $?" >> "$P/mac/selftest8.txt"
set -e
"$P/mac/m2k_host" "$P/mac" --demo 30 1 2 3 4 > "$P/mac/demo.txt"
"$P/mac/m2k_draw" --hash > "$P/mac/hash.txt"
"$P/mac/m2k_draw" --cpu > "$P/mac/cpu.txt"
(cd "$P/mac" && shasum -a 256 a0*.wav) > "$P/mac/wav.sha256"

echo "== the container: aarch64 Linux, gcc 12, glibc 2.36"
docker run --rm --platform linux/arm64 -v "$ROOT":/w -w /w -e P="$P" -e DSP="$DSP" -e SEC="$SEC" "$IMAGE" sh -eu -c '
    L=$P/linux
    gcc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o $L/m2k_host host/m2k_host.c -lm 2>/dev/null
    gcc -O2 -fno-builtin -fno-tree-vectorize -DM2K_MUL_OUT -Imove/test -Ifirmware/src -Ifirmware/gen \
        -o $L/m2k_draw host/m2k_draw.c 2>/dev/null
    gcc -O2 -Wall -Wextra -Wno-unused-parameter -rdynamic -o $L/bench move/test/bench.c -ldl -lm -lpthread
    set +e
    $L/m2k_host --selftest > $L/selftest.txt 2>&1; echo "exit $?" >> $L/selftest.txt
    set -e
    $L/m2k_host $L --demo 30 1 2 3 4 > $L/demo.txt
    $L/m2k_draw --hash > $L/hash.txt
    (cd $L && sha256sum a0*.wav) > $L/wav.sha256
    $L/bench $DSP hash > $L/dsp_hash.txt
    for m in rt ctl clock; do $L/bench $DSP $m > $L/dsp_$m.txt || true; done
    $L/bench $DSP save $P/save > $L/dsp_save.txt || true
    $L/bench $DSP draw $P/screens > $L/dsp_draw.txt || true
    $L/bench $DSP time $SEC > $L/dsp_time.txt
'

fail=0
echo
echo "== self-test"
for s in mac linux; do printf "  %-6s %s, %s\n" "$s" "$(grep -c "" "$P/$s/selftest.txt") lines" "$(tail -1 "$P/$s/selftest.txt")"; done
grep -q "^exit 0$" "$P/mac/selftest.txt" && grep -q "^exit 0$" "$P/linux/selftest.txt" || { echo "  a self-test failed"; fail=1; }
if diff -q "$P/mac/selftest.txt" "$P/linux/selftest.txt" >/dev/null; then echo "  the two outputs are identical"; else echo "  the outputs differ:"; diff "$P/mac/selftest.txt" "$P/linux/selftest.txt" | head -20; fi
printf "  %-6s %s, %s\n" "host8" "$(grep -c "" "$P/mac/selftest8.txt") lines" "$(tail -1 "$P/mac/selftest8.txt")"
grep -q "^exit 0$" "$P/mac/selftest8.txt" || { echo "  the eight-track self-test failed"; fail=1; }

echo "== bank A, m2k_host --demo 30 s (A01..A04 WAVs, sha256)"
awk '{print $1}' "$P/mac/wav.sha256" > "$P/mac.w"
awk '{print $1}' "$P/linux/wav.sha256" > "$P/linux.w"
paste "$P/mac/wav.sha256" "$P/linux.w" | awk '{printf "  %-20s %s %s\n", $2, substr($1, 1, 16), ($1 == $3 ? "= linux" : "!= linux " substr($3, 1, 16))}'
cmp -s "$P/mac.w" "$P/linux.w" || { echo "  WAVs differ"; fail=1; }

echo "== m2k_draw --hash (FNV-1a, 10 s; one core, two cores): mac | linux"
paste -d'|' "$P/mac/hash.txt" "$P/linux/hash.txt" | sed 's/^/ /'
cmp -s "$P/mac/hash.txt" "$P/linux/hash.txt" || { echo "  hashes differ"; fail=1; }

echo "== dsp.so through the plugin API (bench hash), against the Mac's m2k_draw one-core column"
cat "$P/linux/dsp_hash.txt"
for p in A01 A02 A03 A04; do
    want=$(awk -v p="$p" '$1 == p {print $2}' "$P/mac/hash.txt")
    got=$(awk -v p="$p" '$1 == p {print $2}' "$P/linux/dsp_hash.txt")
    if [ "$want" = "$got" ]; then echo "  $p dsp.so $got = mac $want"; else echo "  $p dsp.so $got != mac $want"; fail=1; fi
done

for m in rt ctl clock save draw; do
    case $m in
        rt) echo "== dsp.so on the audio thread's rules, every control and Move's clock with it (bench rt)" ;;
        ctl) echo "== every control (bench ctl)" ;;
        clock) echo "== Move's clock followed (bench clock): each step against the pulse that made it, in samples" ;;
        save) echo "== saving and loading (bench save)" ;;
        draw) echo "== the screen (bench draw): $P/screens/*.pbm" ;;
    esac
    if [ "$m" = ctl ]; then grep -v "^  ok" "$P/linux/dsp_$m.txt" | sed 's/^\(  view at power-on: .\{60\}\).*/\1.../'
    else cat "$P/linux/dsp_$m.txt"; fi
    grep -q FAIL "$P/linux/dsp_$m.txt" && fail=1
done
[ "$(ls "$P/screens" | grep -c pbm)" -ge 9 ] || { echo "  screens missing"; fail=1; }

echo "== dsp.so's render a block, in the container on this Mac's cores (the Move's Cortex-A72 is slower)"
cat "$P/linux/dsp_time.txt"
echo "== the work a sample, counted on the Mac (the container cannot count instructions): m2k_draw --cpu"
sed 's/^/  /' "$P/mac/cpu.txt"

if command -v node >/dev/null 2>&1; then
    echo "== ui.js against a mock of the host (node)"
    node move/test/ui_test.mjs || fail=1
fi

[ "$fail" = 0 ] && echo "== proof: all equal, all checks passed" || { echo "== proof: FAILED"; exit 1; }
