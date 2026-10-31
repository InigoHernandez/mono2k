#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The build checks, from the repo's root: sh tools/check.sh
#  1. warnings: the host (and the host of eight tracks, M2K_TRACKS=8) and the reference renderer with clang -Wall
#     -Wextra, and with GCC -Wall -Wextra when a GCC is installed (its -Wstringop-overflow and -Warray-bounds see more
#     than clang's); none allowed
#  2. the device rules: the core built for a 32-bit soft-float target (armv7m) calls no float helper and no 64-bit
#     division (__aeabi_ldivmod / __aeabi_uldivmod, __divdi3 ...); its section sizes are printed
#  3. sanitizers: the self-test, the FM-1's store and link (host/m2k_link_test.c, with the browser's store for it and
#     its store read back in the browser), every screen (and the text audit: on the tour's frames and a sweep of every
#     page, machine and knob value, no two texts touch and none is cut), every bank A track, every reference spec and
#     the power test's loop under ASan + UBSan; any report fails the run
#  3b. eight tracks: the host of eight's self-test, screens and text sweep under the sanitizers; its renders of bank A,
#     DENSE and a pattern of six (--render-hash) the same as the six-track host's
#  4. the pattern format's conversions mutated (tools/ref/mutate_formats.py): the self-test catches every mutant of
#     fm_v9 and sid_v10 but the equivalent one
#  5. the wave banks (tools/wave_check.py); the bus's controls, every value as recorded and every old value as it
#     sounded once converted (tools/bus_curves.py)
set -u
B=build/check
mkdir -p "$B"
FAIL=0
W="-Wall -Wextra -Wno-unused-function -Wno-unused-parameter"
I="-Ifirmware/src -Ifirmware/gen"

echo "== warnings"
for cc in clang $(ls /opt/homebrew/bin/gcc-[0-9]* /usr/local/bin/gcc-[0-9]* 2>/dev/null) ${GCC:-}; do
    case "$($cc --version 2>/dev/null | head -1)" in *clang*) [ "$cc" = clang ] || continue ;; esac
    for src in host/m2k_host.c host/m2k_ref.c host/m2k_link_test.c; do
        if ! out=$($cc -O2 $W $I -o "$B/$(basename "$src" .c)" "$src" -lm 2>&1); then
            echo "  $cc $src: DOES NOT BUILD"; printf "%s\n" "$out" | grep "error:" | head -5; FAIL=1; continue
        fi
        n=$(printf "%s" "$out" | grep -c "warning:")
        echo "  $cc $src: $n warnings"
        [ "$n" -eq 0 ] || { printf "%s\n" "$out"; FAIL=1; }
    done
    if ! out=$($cc -O2 $W -DM2K_TRACKS=8 $I -o "$B/m2k_host8" host/m2k_host.c -lm 2>&1); then   # (eight tracks: the Move's)
        echo "  $cc host/m2k_host.c, eight tracks: DOES NOT BUILD"; printf "%s\n" "$out" | grep "error:" | head -5; FAIL=1; continue
    fi
    n=$(printf "%s" "$out" | grep -c "warning:")
    echo "  $cc host/m2k_host.c, eight tracks: $n warnings"
    [ "$n" -eq 0 ] || { printf "%s\n" "$out"; FAIL=1; }
done
ls /opt/homebrew/bin/gcc-[0-9]* /usr/local/bin/gcc-[0-9]* >/dev/null 2>&1 || [ -n "${GCC:-}" ] ||
    echo "  GCC: SKIPPED (none installed; GCC=path to use one)"

echo "== device rules (armv7m, soft float)"
if clang --target=armv7m-none-eabi -mfloat-abi=soft -ffreestanding -O2 $W $I -c firmware/src/m2k_core.c -o "$B/core_arm.o"; then
    bad=$(nm -u "$B/core_arm.o" | grep -E "__aeabi_(l|ul)divmod|__(u)?(div|mod)di3|__aeabi_[fd](add|sub|mul|div|cmp|2|cmpeq|cmplt)|__aeabi_[iul]2[fd]|__(add|sub|mul|div)[sd]f3|__float|__fix")
    if [ -n "$bad" ]; then echo "  calls a float helper or 64-bit division:"; echo "$bad"; FAIL=1; else echo "  no float helper, no 64-bit division"; fi
    flash=0; ram=0
    for row in $(objdump -h "$B/core_arm.o" | awk '$1 ~ /^[0-9]+$/ && NF >= 3 {print $2 ":" $3}'); do
        name=${row%%:*}; sz=$((0x${row##*:}))
        case "$name" in .text|.rodata*|.data|.ARM.exidx) flash=$((flash + sz)) ;; esac
        case "$name" in .data|.bss) ram=$((ram + sz)) ;; esac
    done
    echo "  flash (code, tables, waves): $((flash / 1024)) KB; RAM (static, POOL included): $((ram / 1024)) KB"
else
    FAIL=1
fi

echo "== sanitizers"
SAN="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
rm -f "$B/m2k_host_san" "$B/m2k_ref_san" "$B/m2k_link_san"      # (a build that fails runs nothing old)
clang $SAN -w $I -o "$B/m2k_host_san" host/m2k_host.c -lm && clang $SAN -w $I -o "$B/m2k_ref_san" host/m2k_ref.c -lm &&
    clang $SAN -w $I -o "$B/m2k_link_san" host/m2k_link_test.c || FAIL=1
mkdir -p "$B/screens" "$B/pitch" "$B/ref" "$B/link"
{
    "$B/m2k_host_san" --selftest
    "$B/m2k_host_san" --web-store "$B/link/web16.bin" && "$B/m2k_link_san" "$B/link/web16.bin" "$B/link/fm1.bin" &&
        "$B/m2k_host_san" --fm1-store "$B/link/fm1.bin" || echo "LINK FAILED"

    "$B/m2k_host_san" "$B/screens"
    "$B/m2k_host_san" --text-sweep
    "$B/m2k_host_san" "$B/pitch" --pitch
    for f in tools/ref/*.spec; do "$B/m2k_ref_san" "$B/ref" < "$f" > /dev/null; done
    "$B/m2k_ref_san" "$B/ref" --loop 88 < tools/ref/power.loop > /dev/null
} > "$B/san.log" 2>&1
h1=$("$B/m2k_host_san" --bank-hash 2>/dev/null); h2=$(grep "bank hash" "$B/san.log" | head -1)
[ -n "$h1" ] && [ "$h1" = "$h2" ] || echo "LINK FAILED: bank A through the cache ($h2) is not the browser's ($h1)" >> "$B/san.log"
rep=$(grep -E "runtime error|ERROR: AddressSanitizer|SELFTEST FAILED|LINK FAILED|^FAIL|^TEXT " "$B/san.log" | sort | uniq -c)
if [ -n "$rep" ]; then echo "$rep"; FAIL=1; else echo "  self-test, the FM-1 link, screens and their text, bank A, $(ls tools/ref/*.spec | wc -l | tr -d ' ') specs, the power loop: clean"; fi

echo "== eight tracks (M2K_TRACKS=8, the Move's build)"
# its self-test (format 21 both ways: the build of six reads its bytes, FMT21; both pack a pattern of six alike), every
# screen's text and a sweep with T8 in view, under ASan + UBSan; bank A, DENSE and a pattern of six from their start
# render as the build of six's, sample for sample (T7 and T8 silent beside them)
rm -f "$B/m2k_host8_san"
clang $SAN -w -DM2K_TRACKS=8 $I -o "$B/m2k_host8_san" host/m2k_host.c -lm || FAIL=1
mkdir -p "$B/screens8"
{
    "$B/m2k_host8_san" --selftest
    "$B/m2k_host8_san" "$B/screens8"
    "$B/m2k_host8_san" --text-sweep
} > "$B/san8.log" 2>&1
rep=$(grep -E "runtime error|ERROR: AddressSanitizer|SELFTEST FAILED|^FAIL|^TEXT " "$B/san8.log" | sort | uniq -c)
r6=$("$B/m2k_host" --render-hash 20 2>&1); r8=$("$B/m2k_host8" --render-hash 20 2>&1)
if [ -n "$rep" ]; then echo "$rep"; FAIL=1;
elif [ -z "$r6" ] || [ "$r6" != "$r8" ]; then echo "  the renders differ from six tracks':"; printf "%s\n%s\n" "$r6" "$r8"; FAIL=1;
else echo "  self-test, screens and their text: clean; bank A, DENSE, a pattern of six: 20 s each as six tracks render them"; fi

echo "== the format's conversions, mutated"
.venv/bin/python tools/ref/mutate_formats.py > "$B/mutate.log" 2>&1 && tail -1 "$B/mutate.log" | sed 's/^/  /' || { grep -E "SURVIVED|MISSING|unmutated" "$B/mutate.log"; FAIL=1; }

echo "== the wave banks"
.venv/bin/python tools/wave_check.py > "$B/waves.log" 2>&1 && tail -1 "$B/waves.log" | sed 's/^/  /' || { cat "$B/waves.log"; FAIL=1; }

echo "== the bus's controls"
.venv/bin/python tools/bus_curves.py > "$B/bus.log" 2>&1 && tail -1 "$B/bus.log" | sed 's/^/  /' || { cat "$B/bus.log"; FAIL=1; }

[ "$FAIL" -eq 0 ] && echo "check: passed" || echo "check: FAILED"
exit "$FAIL"
