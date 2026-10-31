#!/bin/sh
# MONO2K on the Move: dsp.so for the Move (aarch64 Linux, glibc 2.35) built in an aarch64 Debian container, checked,
# and packaged as Schwung installs a module.
#   sh move/build.sh            -> build/move/modules/overtake/mono2k/ and build/move/mono2k-move.tar.gz
# The tarball holds mono2k/ at its top: Schwung Manager's "Install Custom Module" (and its catalogue installs) unpack
# it into modules/overtake/, the overtake component's folder. move/deploy.sh copies the folder over SSH instead.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

IMAGE=mono2k-move-build:bookworm      # debian:bookworm-slim (arm64) + gcc 12; its glibc is 2.36, the Move's 2.35
OPT=${OPT:--O2}
OUT=build/move
MOD=$OUT/modules/overtake/mono2k

# the default Docker config's credential helper is Docker Desktop's; the FM-1 build's config talks to colima alone
if [ -z "${DOCKER_CONFIG:-}" ] && [ -d "$HOME/.jieli/docker" ]; then
    DOCKER_CONFIG="$HOME/.jieli/docker"
    export DOCKER_CONFIG
fi

for h in m2k_tables m2k_fonts m2k_icons m2k_waves64; do
    [ -f "firmware/gen/$h.h" ] || { echo "firmware/gen/$h.h missing: run the generators (CLAUDE.md)"; exit 1; }
done

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    docker build --platform linux/arm64 -t "$IMAGE" - <<'EOF'
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev binutils file && rm -rf /var/lib/apt/lists/*
EOF
fi

rm -rf "$OUT/modules" "$OUT/mono2k-move.tar.gz"
mkdir -p "$MOD"
cp move/module.json move/ui.js "$MOD/"

docker run --rm --platform linux/arm64 -v "$ROOT":/w -w /w -e OPT="$OPT" -e MOD="$MOD" -e OUT="$OUT" "$IMAGE" sh -eu -c '
    [ "$(uname -m)" = aarch64 ] || { echo "not an aarch64 container"; exit 1; }
    # one unit (the core is a unity build); only move_plugin_init_v2 is exported, so nothing of the core can bind to
    # another module'"'"'s symbols or theirs to ours; -mtune keeps the ISA plain ARMv8-A. (gcc -Wextra notes two places
    # in the core that are meant: m2k_factory.c vox_v13'"'"'s unsigned compare with P_SYN 0, m2k_ui.c icon'"'"'s commented
    # fall-through)
    gcc -std=gnu11 $OPT -mtune=cortex-a72 -DM2K_TRACKS=8 -fPIC -shared -fvisibility=hidden \
        -Wall -Wextra -Wno-unused-function -Wno-type-limits -Wno-implicit-fallthrough -Ifirmware/src -Ifirmware/gen \
        -Wl,-z,defs -Wl,-z,now -Wl,--as-needed -s \
        -o "$MOD/dsp.so" move/m2k_move.c
    file -b "$MOD/dsp.so"

    # the Move has glibc 2.35: every versioned symbol must be at or under it
    max=$(objdump -T "$MOD/dsp.so" | grep -o "GLIBC_[0-9][0-9.]*" | sort -uV | tail -1)
    top=$(printf "%s\nGLIBC_2.35\n" "$max" | sort -V | tail -1)
    [ "$top" = GLIBC_2.35 ] || { echo "glibc: $max needs a newer glibc than the Move'"'"'s 2.35"; exit 1; }
    echo "glibc: newest symbol $max (the Move: 2.35)"

    exp=$(nm -D --defined-only "$MOD/dsp.so" | awk "{print \$3}" | grep -v "^_" | tr "\n" " ")
    [ "$exp" = "move_plugin_init_v2 " ] || { echo "exports: $exp(only move_plugin_init_v2 expected)"; exit 1; }
    imp=$(nm -D --undefined-only "$MOD/dsp.so" | awk "{print \$2}" | sed "s/@.*//" | sort -u | tr "\n" " ")
    echo "imports: $imp"
    # nothing that allocates, locks, logs or does I/O can be reached: the audio thread forbids them all
    bad=$(echo "$imp" | tr " " "\n" | grep -E "^(malloc|calloc|realloc|free|posix_memalign|aligned_alloc|memalign|f?open|fopen64|fclose|fread|fwrite|f?printf|puts|vsn?printf|syslog|pthread_mutex_lock|pthread_create|usleep|nanosleep|write|read)$" || true)
    [ -z "$bad" ] || { echo "forbidden on the audio thread: $bad"; exit 1; }

    tar --owner=0 --group=0 --sort=name -czf "$OUT/mono2k-move.tar.gz" -C "$OUT/modules/overtake" mono2k
    tar -tzvf "$OUT/mono2k-move.tar.gz"
'
ls -l "$OUT/mono2k-move.tar.gz" | awk '{print "package: " $NF ", " $5 " bytes"}'
shasum -a 256 "$OUT/mono2k-move.tar.gz" | awk '{print "sha256:  " $1}'
