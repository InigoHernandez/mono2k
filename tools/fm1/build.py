#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MONO2K for the FM-1: the app, the update loader and an installable .fwsc package.

  tools/fm1/build.py [--release X.Y]

After CS-1's tools/build.py (on Felucca's, Leo Kuroshita, Hügelton Instruments, GPL-3.0-only): the same steps, the
same checks. Outputs in build/fm1/: mono2k.bin (the app), loader/ota.bin (the update loader), mono2k.fwsc (the
package; mono2k-X.Y.fwsc for a release). The package's identity is FM-1_980 for every build and release (the hub's
FM-1_981 and FM-1_983 are other firmwares'; FM-1_9NN keeps the hub's resume of an interrupted update), as the
installer page reads it; a release sets the version the FM-1 reports (APP_VER) and the file's name.

JieLi's toolchain (clang 4.0.1 for pi32v2) runs on Linux x86-64 only: on a Mac each tool runs in a linux/amd64
container (colima with Rosetta, or Docker). JIELI_TOOLCHAIN (default ~/.jieli/toolchain), AC79_SDK (default
~/fw-AC79_AIoT_SDK: three of its files go into the package) and DOCKER_CONFIG (default ~/.jieli/docker when it
exists) override the places.
"""
import argparse
import bisect
import hashlib
import os
import platform
import re
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FW = ROOT / "firmware" / "fm1"
OUT = ROOT / "build" / "fm1"
LDR = OUT / "loader"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1pkg_make  # noqa: E402
import lz4blk  # noqa: E402

APP_XIP = 0x02000120                # the app's offset 0 in the XIP map: the SPL jumps here
APP_SLOT = fm1pkg_make.APP_SLOT
LOADER_LOAD = 0x01C0A800
LOADER_NAME = b"usb_hid_ota.bin"    # the file name the SPL looks for
DOCKER_IMAGE = os.environ.get("JIELI_DOCKER_IMAGE", "debian:bookworm-slim")
CFLAGS = ["-Os", "-ffunction-sections", "-fno-builtin", "-Wall", "-Wno-unused-function"]
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$")
SDK_SHA256 = {                      # AC79NN_SDK_V1.2.1_2023-12-13, the version the open FM-1 firmwares test
    "uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}
PRODUCT = "FM-1_980"                # MONO2K's, every release (Felucca 9XY, CoralFlower 930, CS-1 95N, zp12 97N, Dinghy 99N)
VERSION = "0.0"
# helpers the core must not need: the chip has no FPU and the build no libgcc (soft float, 64-bit division)
BANNED = re.compile(r"\b__(?:(?:u?(?:div|mod)di3)|(?:add|sub|mul|div)[sd]f3|(?:fix|float)\w*|(?:eq|ne|lt|le|gt|ge|un)[sd]f2|extendsfdf2|truncdfsf2)\b")


def toolchain():
    tc = Path(os.environ.get("JIELI_TOOLCHAIN", Path.home() / ".jieli" / "toolchain"))
    if not (tc / "pi32v2" / "bin" / "clang").exists():
        raise SystemExit(f"no JieLi toolchain in {tc} (JIELI_TOOLCHAIN)")
    return tc.resolve()


def use_docker():
    native = platform.system() == "Linux" and platform.machine() in ("x86_64", "AMD64")
    return os.environ.get("JIELI_DOCKER", "0" if native else "1") == "1"


def env():
    e = dict(os.environ)
    dc = Path.home() / ".jieli" / "docker"
    if "DOCKER_CONFIG" not in e and (dc / "config.json").exists():
        e["DOCKER_CONFIG"] = str(dc)
    return e


def tc(tool, *args):
    """a toolchain binary (pi32v2/bin/..., common/bin/...) run with cwd ROOT; Path arguments relative to it"""
    rel = [str(Path(a).resolve().relative_to(ROOT)) if isinstance(a, Path) else a for a in args]
    if tool == "cc":                # the toolchain's cc wrapper needs python3: clang directly
        tool, rel = "pi32v2/bin/clang", ["-target", "pi32v2", *rel]
    if use_docker():
        cmd = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{ROOT}:/work",
               "-v", f"{toolchain()}:/opt/jieli:ro", "-w", "/work", DOCKER_IMAGE, f"/opt/jieli/{tool}", *rel]
    else:
        cmd = [str(toolchain() / tool), *rel]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, env=env())
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"build: {tool} failed")
    if r.stderr.strip():
        sys.stderr.write(r.stderr)
    return r.stdout


def tc_all(*cmds):
    with ThreadPoolExecutor(min(4, len(cmds))) as ex:
        return list(ex.map(lambda c: tc(*c), cmds))


# ---- the update loader (Felucca's, as CS-1 builds it)

def crc16(d, c=0):
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
        c &= 0xFFFF
    return c


def ldr_head(dcrc, a, b, attr, name):
    body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
    return struct.pack("<H", crc16(body)) + body


def ldr_wrap(image):
    """ota.bin: an outer JLFS head (32 B), an inner one (32 B, the load address), then blocks [clen u32][dlen u32]
    [an LZ4 block], dlen 4096 but the last"""
    blocks = bytearray()
    for i in range(0, len(image), 4096):
        raw = image[i:i + 4096]
        c = lz4blk.compress(raw)
        if len(c) > 4096 or lz4blk.decompress(c) != raw:
            raise SystemExit(f"loader: block {i // 4096} does not compress below 4096 B")
        blocks += struct.pack("<II", len(c), len(raw)) + c
    inner = ldr_head(crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
    body = inner + bytes(blocks)
    return ldr_head(crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def build_loader():
    LDR.mkdir(parents=True, exist_ok=True)
    src = FW / "loader"
    flags = [*CFLAGS, "-Ifirmware/fm1/hal", "-Ifirmware/fm1/src"]
    tc_all(("cc", "-c", src / "crt0_ldr.S", "-o", LDR / "crt0_ldr.o"),
           ("cc", *flags, "-c", src / "loader.c", "-o", LDR / "loader.o"))
    elf = LDR / "loader.elf"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", src / "loader.ld",
       LDR / "crt0_ldr.o", LDR / "loader.o", "-o", elf)
    _, dis, hdr, syms = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, LDR / "loader.bin"),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-h", elf),
                               ("common/bin/objdump", "-t", elf))
    (LDR / "loader.dis").write_text(dis)
    for ln in hdr.splitlines():     # the loader rewrites the flash: nothing of it may run from XIP
        p = ln.split()
        if len(p) > 4 and p[1].startswith(".") and int(p[3], 16) >= 0x02000000 and int(p[2], 16):
            raise SystemExit(f"loader: section {p[1]} at {p[3]} is outside RAM")
    image = (LDR / "loader.bin").read_bytes()
    if not image or len(image) > 0x14000:
        raise SystemExit("loader: image empty or too big")
    ota = ldr_wrap(image)
    (LDR / "ota.bin").write_bytes(ota)
    print(f"loader: image {len(image)} B at {LOADER_LOAD:#x}, ota.bin {len(ota)} B")
    return ota


# ---- the app: the platform and its glue (m2k_fm1.c), and MONO2K's core (firmware/src/m2k_core.c), two units

# the second core renders some tracks of each block (the core's m2k_par_begin; DUAL=0 in the environment: one core)
DUAL = 0 if os.environ.get("DUAL") == "0" else 1
# the FM-1's build: one project of 64 patterns (bank A the demos, B-D INIT), two of them unpacked at a time, the rest
# packed in a 46 KB working image (README: the FM-1 build); the core's 64-bit products out of line (M2K_MUL_OUT,
# m2k_core.c: the CPU's fault beside loads, mul64_check)
CORE = ["-DM2K_FW=1", "-DM2K_PATTERNS=64", "-DM2K_PAT_SLOTS=2", "-DM2K_PROJECTS=1", "-DM2K_IMG_DATA=47104",
        "-DM2K_MUL_OUT=1"]


def build_app():
    flags = [*CORE, *CFLAGS, "-Ifirmware/fm1/hal", "-Ifirmware/fm1/src", "-Ifirmware/fm1/gen", "-Ifirmware/src",
             "-Ifirmware/gen", f'-DFELUCCA_ID="{PRODUCT}"', f'-DAPP_VER="{VERSION}"',
             f"-DDINGHY_DUAL_CORE={DUAL}"]
    tc_all(("cc", f"-DDINGHY_DUAL_CORE={DUAL}", "-c", FW / "crt0.S", "-o", OUT / "crt0.o"),
           ("cc", "-c", FW / "hal" / "fm1_vec.S", "-o", OUT / "fm1_vec.o"),
           ("cc", "-c", FW / "hal" / "fm1_isr.S", "-o", OUT / "fm1_isr.o"),
           ("cc", *flags, "-c", FW / "m2k_fm1.c", "-o", OUT / "mono2k.o"),
           ("cc", *[f for f in CFLAGS if f != "-Os"], "-O2", *CORE, "-Ifirmware/src", "-Ifirmware/gen", "-c",   # the core
            ROOT / "firmware" / "src" / "m2k_core.c",                                                              # for speed
            "-o", OUT / "core.o"))
    undef = tc("common/bin/objdump", "-t", OUT / "mono2k.o") + tc("common/bin/objdump", "-t", OUT / "core.o")
    bad = sorted({m.group(0) for m in BANNED.finditer(undef)})
    if bad:
        raise SystemExit(f"the app needs helpers the FM-1 has not: {', '.join(bad)} (a float or a 64-bit division)")
    elf = OUT / "mono2k.elf"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", FW / "app.ld", OUT / "crt0.o", OUT / "fm1_vec.o",
       OUT / "fm1_isr.o", OUT / "mono2k.o", OUT / "core.o", "-o", elf, "-Map", OUT / "mono2k.map")
    for sect in ("text.bin", "data.bin", "ramtext.bin"):
        (OUT / sect).unlink(missing_ok=True)
    *_, syms, dis, rt = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, OUT / "text.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".data", elf, OUT / "data.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".ram_text", elf, OUT / "ramtext.bin"),
                               ("common/bin/objdump", "-t", elf),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-d", "-j", ".ram_text", elf))
    (OUT / "mono2k.dis").write_text(dis)
    (OUT / "mono2k.sym").write_text(syms)

    def symv(name):
        return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)
    img = bytearray((OUT / "text.bin").read_bytes())
    for sect, lname in (("ramtext.bin", "_rt_load"), ("data.bin", "_data_load")):   # after .text, where crt0
        load = symv(lname)                                                        # copies them from, by words
        if load % 4:
            raise SystemExit(f"{lname} {load:#x} is not word aligned")
        blob = (OUT / sect).read_bytes() if (OUT / sect).exists() else b""
        if blob:
            if load - APP_XIP < len(img):
                raise SystemExit(f"{lname} overlaps the image")
            img += b"\xff" * (load - APP_XIP - len(img))
            img += blob
    img += b"\xff" * (-len(img) % 4)
    (OUT / "mono2k.bin").write_bytes(img)
    return bytes(img), syms, dis, rt


# The CPU's fault (felucca-bsp docs/CPU.md): at the boot loader's 360 MHz, which MONO2K runs at, a 32 x 32 -> 64-bit
# multiply inlined with a load in flight beside it now and then leaves a register a bit off (the firmware crashes or
# computes garbage), most of all beside 16-bit loads; a multiply in a helper of its own that uses the product at once
# is clear of it. fm1-base's rule (tools/build.py mul64_near_loads): a load among the two instructions before it
# (calls aside), or one after it before its product is used (four at most). Here a call, a jump or a return before the
# product is used counts as a load too (the code it goes to loads), so a helper must use its product before it returns.
MUL64 = re.compile(r"(r\d+)_(r\d+) [+-]?= r\d+ \* r\d+")
LOAD = re.compile(r"= [hbd]?\[")
LEAVE = re.compile(r"\b(?:goto|call|rts|rti|tbb|tbh)\b")
# functions allowed a 64-bit multiply beside a load, each with its reason (the compiler's own __* are not looked at)
MUL64_EXEMPT = {}


def mul64_check(dis, syms, window=4):
    """the count of 64-bit multiplies (rN_rM = rA * rB) in the app, and {function: how many} of those beside a load,
    each instruction given to its function by the symbol table's addresses and sizes"""
    fns = []
    for ln in syms.splitlines():
        p = ln.split()
        if len(p) >= 6 and "F" in p[1:-3] and re.fullmatch(r"[0-9a-f]+", p[0]) and re.fullmatch(r"[0-9a-f]+", p[-2]):
            fns.append((int(p[0], 16), int(p[0], 16) + int(p[-2], 16), p[-1]))
    fns.sort()
    starts = [f[0] for f in fns]
    bodies = {}
    for ln in dis.splitlines():
        mm = LINE.match(ln)
        if not mm:
            continue
        a = int(mm.group(1), 16)
        k = bisect.bisect_right(starts, a) - 1
        name = fns[k][2] if k >= 0 and a < fns[k][1] else f"{a:#x}"
        bodies.setdefault(name, []).append(mm.group(3).strip())

    def regs(ins):
        return {t for t in re.split(r"[^A-Za-z0-9]+", ins) if re.fullmatch(r"r\d+", t)}
    total, near = 0, {}
    for name, body in bodies.items():
        for i, ins in enumerate(body):
            m = MUL64.match(ins)
            if not m:
                continue
            total += 1
            dst = {m.group(1), m.group(2)}
            bad = any(LOAD.search(b) for b in body[max(0, i - 2):i] if not re.search(r"\bcall\b", b))
            for b in body[i + 1:i + 1 + window] if not bad else ():
                if regs(b) & dst:
                    break                               # the product used: the multiply has finished
                if LOAD.search(b) or LEAVE.search(b):
                    bad = True
                    break
            else:
                if not bad and i + 1 + window > len(body):
                    bad = True                          # the function ends with the product unused
            if bad and not name.startswith("__"):
                near[name] = near.get(name, 0) + 1
    return total, near


def check(img, syms, dis, rt):
    errors, notes = [], []

    def sym(name):
        mm = re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M)
        return int(mm.group(1), 16) if mm else 0
    if sym("_start") != APP_XIP:
        errors.append(f"_start is not at {APP_XIP:#x}")
    if img[:4] != bytes.fromhex("04818000"):
        errors.append(f"the image starts with {img[:4].hex()}, not the entry stub")
    rt_calls = []                   # code in RAM runs with the flash off: no calls out of RAM (and calls between XIP
    for ln in rt.splitlines():      # and RAM are out of a direct call's reach: through a pointer, FL_FAR)
        mc = re.search(r"\bcall\s+-?\d+\s+<([^:>]*):\s+([0-9a-f]+)\s*>", ln)
        if mc:
            if not 0x01C00000 <= int(mc.group(2), 16) < sym("_rt_end"):
                rt_calls.append(ln)
        elif re.search(r"\bcall\s+-?\d+", ln):
            rt_calls.append(ln)
    if rt_calls:
        errors.append(f".ram_text calls out of RAM: {rt_calls[:3]}")
    for ln in dis.splitlines():     # nothing may call or load an address in the chip's ROM
        mm = LINE.match(ln)
        if not mm:
            continue
        for v in re.findall(r"= (-?\d+) <|call -?\d+ <[^:>]*: ([0-9a-f]+) >", mm.group(3)):
            val = (int(v[0]) & 0xFFFFFFFF) if v[0] else int(v[1], 16) & 0xFFFFFFFF
            if 0xFFC00000 <= val < 0xFFD00000:
                errors.append(f"a reference to ROM address {val:#010x}")
    n, near = mul64_check(dis, syms)
    bad = {f: c for f, c in near.items() if f not in MUL64_EXEMPT}
    for f in sorted(set(near) - set(bad)):
        notes.append(f"64-bit multiplies beside a load in {f}: exempt, {MUL64_EXEMPT[f]}")
    if bad:
        errors.append("64-bit multiplies beside a load (felucca-bsp docs/CPU.md; m2k_core.c's helpers take them out of "
                      "line): " + ", ".join(f"{f} {c}" for f, c in sorted(bad.items(), key=lambda x: -x[1])))
    else:
        notes.append(f"64-bit multiplies: {n}, none beside a load")
    if sym("fm1_core1_start"):      # two cores: CPU1's code in RAM, its own stacks, its loop calling nothing (CS-1's)
        for name in ("fm1_core1_main", "audio_worker_loop"):
            if not 0x01C00000 <= sym(name) < sym("_rt_end"):
                errors.append(f"{name} must be in .ram_text so CPU1 idles with the flash off")
        if sym("_cpu1_stacks_hi") - sym("_cpu1_stacks_lo") != 8192:
            errors.append("CPU1 needs its 4 KiB user + 4 KiB supervisor stacks (.cpu1_stacks)")
        body = dis.split("\naudio_worker_loop:", 1)[-1].split("\n\n", 1)[0]
        if re.search(r"\bcall -?\d+", body):
            errors.append("audio_worker_loop contains a direct call; its helpers must inline (it runs with XIP off)")
        notes.append("CPU1: a polling loop in RAM, its own 8 KiB of stacks, bounded start and join")
    if len(img) > APP_SLOT:
        errors.append(f"the image, {len(img)} B, is bigger than the app slot ({APP_SLOT} B)")
    ram = sym("_bss_end") - 0x01C08000
    pool = sym("_pool_end") - 0x01C20000
    notes.append(f"image {len(img)} B of {APP_SLOT}; RAM .data+.bss {ram} B of 98304; pool {pool} B of 344064; "
                 f"RAM code {sym('_rt_end') - 0x01C00000} B of 24576")
    if sym("_rt_end") > 0x01C06000:
        errors.append(f"RAM code ends at {sym('_rt_end'):#x}, past 0x1c06000")
    if ram > 96 * 1024:
        errors.append(f"RAM .data+.bss {ram} B > 96 KiB")
    if pool > 0x54000:
        errors.append(f"pool {pool} B > {0x54000} B")
    return errors, notes


def main():
    global VERSION
    ap = argparse.ArgumentParser()
    ap.add_argument("--release", metavar="X.Y[.Z]", help="a release: version X.Y[.Z], mono2k-X.Y[.Z].fwsc (identity FM-1_980)")
    ap.add_argument("--sdk", type=Path, help="the JieLi AC79 SDK (default: $AC79_SDK or ~/fw-AC79_AIoT_SDK)")
    ap.add_argument("--diag", action="store_true", help="a test build: the render's load along the top edge")
    a = ap.parse_args()
    name = "mono2k.fwsc"
    if a.diag:
        CORE.extend(["-DM2K_FM1_DIAG=1", "-DM2K_PROF"])   # (the bench's parts: the core's profile)
        name = "mono2k-test.fwsc"
    if a.release:
        if not re.fullmatch(r"\d+\.\d+(\.\d+)?", a.release):
            raise SystemExit(f"--release {a.release}: X.Y or X.Y.Z")
        VERSION, name = a.release, f"mono2k-{a.release}.fwsc"
    CORE.append(f'-DM2K_VER="{VERSION}"')     # the boot screen's version (0.0 for a build that is no release)
    fm1pkg_make.SDK = a.sdk or os.environ.get("AC79_SDK") or str(Path.home() / "fw-AC79_AIoT_SDK")
    for rel, sha in SDK_SHA256.items():
        if hashlib.sha256(fm1pkg_make.sdk_file(rel)).hexdigest() != sha:
            print(f"warning: the SDK's {rel} is not AC79NN_SDK_V1.2.1's")
    OUT.mkdir(parents=True, exist_ok=True)
    ota = build_loader()
    img, syms, dis, rt = build_app()
    errors, notes = check(img, syms, dis, rt)
    for n in notes:
        print("  ok   ", n)
    for e in errors:
        print("  FAIL ", e)
    if errors:
        raise SystemExit("build: the checks failed")
    pkg = fm1pkg_make.ufw(fm1pkg_make.flash_image(img, fm1pkg_make.KEY), ota, PRODUCT)
    (OUT / name).write_bytes(pkg)
    print(f"package  {OUT / name}  {len(pkg)} B, identity {PRODUCT}, sha256 {hashlib.sha256(pkg).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
