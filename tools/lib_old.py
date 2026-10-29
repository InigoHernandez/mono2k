#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The factory libraries before m2k_lib.c's: every commit since the library came in (b081cbe) builds its own factory,
# packs its store, and today's core reads it back (its sounds converted as an old store's are); each slot's sound is
# hashed as a store loaded today holds it (FNV-1a of its 80 bytes: the name, the machine, the kit). The reader is the
# last core before the library (LAST: today's conversions, no refresh). The distinct hashes are LIB_OLD in
# firmware/src/m2k_lib.c (--c prints them): a slot of an older store still holding one of them is the factory's, and
# takes the new library (lib_refresh). Run from the repo's root:  .venv/bin/python tools/lib_old.py [--c]
import os, subprocess, sys, tarfile, io

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = os.path.join(ROOT, "build", "libold")
FIRST = "b081cbe"                  # the library's first commit (store version 3)
LAST = "e7bedad"                   # the last commit before m2k_lib.c (its core: the newest that touched it)

PACK = r'''
#include <stdio.h>
#include "m2k_core.c"
int main(int argc, char **argv)
{
    m2k_init(1);
    int max, n = m2k_store_pack();
    uint8_t *b = m2k_store_buf(&max);
    FILE *f = fopen(argv[1], "wb");
    fwrite(b, 1, (size_t)n, f);
    fclose(f);
    return n <= 0;
}
'''

READ = r'''
#include <stdio.h>
#include "m2k_core.c"
int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        int max;
        uint8_t *b = m2k_store_buf(&max);
        m2k_init(1);
        FILE *f = fopen(argv[a], "rb");
        int n = (int)fread(b, 1, (size_t)max, f);
        fclose(f);
        int ver = b[4], rev = b[7];
        if (!m2k_store_unpack(n)) { printf("# %s: not read\n", argv[a]); continue; }
        for (int i = 0; i < M2K_SOUNDS; i++)
            if (snd[i].name[0]) printf("%s %d %d %d %08x %s\n", argv[a], ver, rev, i, fnv((const uint8_t *)&snd[i], sizeof snd[i]), snd[i].name);
    }
    return 0;
}
'''


def sh(*a, **k):
    return subprocess.run(a, cwd=ROOT, check=True, capture_output=True, **k)


def main():
    os.makedirs(B, exist_ok=True)
    revs = sh("git", "rev-list", "--reverse", f"{FIRST}^..{LAST}", "--", "firmware/src", "firmware/gen").stdout.decode().split()
    with open(os.path.join(B, "pack.c"), "w") as f: f.write(PACK)
    with open(os.path.join(B, "read.c"), "w") as f: f.write(READ)
    stores = []
    for c in revs:
        d = os.path.join(B, c[:7])
        st = os.path.join(B, c[:7] + ".m2ks")
        if not os.path.exists(st):
            tar = sh("git", "archive", c, "firmware/src", "firmware/gen").stdout
            tarfile.open(fileobj=io.BytesIO(tar)).extractall(d)
            exe = os.path.join(d, "pack")
            sh("cc", "-O1", "-w", "-I" + os.path.join(d, "firmware/src"), "-I" + os.path.join(d, "firmware/gen"),
               "-o", exe, os.path.join(B, "pack.c"), "-lm")
            sh(exe, st)
        stores.append(st)
    d = os.path.join(B, revs[-1][:7])                  # the last core before the library
    exe = os.path.join(B, "read")
    sh("cc", "-O1", "-w", "-I" + os.path.join(d, "firmware/src"), "-I" + os.path.join(d, "firmware/gen"), "-o", exe,
       os.path.join(B, "read.c"), "-lm")
    out = sh(exe, *stores).stdout.decode().splitlines()
    pairs, names = {}, {}
    for ln in out:
        if ln.startswith("#"): print(ln, file=sys.stderr); continue
        _, ver, rev, i, h, nm = ln.split()
        pairs.setdefault((int(i), h), set()).add(os.path.basename(_)[:7])
        names[(int(i), h)] = nm
    keys = sorted(pairs)
    print(f"{len(stores)} commits, {len(keys)} distinct factory sounds over {len(set(i for i, _ in keys))} slots", file=sys.stderr)
    if "--c" in sys.argv:                              # the hashes alone, sorted (a sound is the factory's at any slot)
        row = [f"0x{h}u" for h in sorted(set(h for _, h in keys), key=lambda x: int(x, 16))]
        print(f"{len(row)} hashes", file=sys.stderr)
        for k in range(0, len(row), 8): print("    " + ", ".join(row[k:k + 8]) + ",")
    else:
        for i, h in keys: print(i, h, names[(i, h)], " ".join(sorted(pairs[(i, h)])))


if __name__ == "__main__":
    main()
