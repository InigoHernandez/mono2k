#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K's 1-bit fonts: one pixel font of its own, 5 x 7 capitals drawn here a pixel at a time, at four integer
# sizes (1x the labels, values and lines; 2x the stage's names; 3x the big values), so every stroke is whole
# pixels and every size the same design; plus the logotype, rasterized from the brand's paths. Writes
# firmware/gen/m2k_fonts.h.
#   .venv/bin/python tools/gen_font.py [--sheet build/fonts.png]
import os, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
OUT = os.path.join(ROOT, "firmware", "gen", "m2k_fonts.h")

# MONO2K's pixel font: one 5 x 7 design, drawn here a pixel at a time (squared, the round letters chamfered a pixel,
# as the wordmark's rounded blocks; a slashed zero; a flat-middled 2), generated at 1x, 2x, 3x and 4x
GLYPHS = {
    ' ': ["...", "...", "...", "...", "...", "...", "..."],
    '!': ["#", "#", "#", "#", "#", ".", "#"],
    '"': ["#.#", "#.#", "...", "...", "...", "...", "..."],
    '#': [".#.#.", "#####", ".#.#.", ".#.#.", ".#.#.", "#####", ".#.#."],
    '$': ["..#..", ".####", "#.#..", ".###.", "..#.#", "####.", "..#.."],
    '%': ["##...", "##..#", "...#.", "..#..", ".#...", "#..##", "...##"],
    '&': [".##..", "#..#.", "#.#..", ".#...", "#.#.#", "#..#.", ".##.#"],
    "'": ["#", "#", ".", ".", ".", ".", "."],
    '(': [".#", "#.", "#.", "#.", "#.", "#.", ".#"],
    ')': ["#.", ".#", ".#", ".#", ".#", ".#", "#."],
    '*': [".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."],
    '+': ["...", "...", ".#.", "###", ".#.", "...", "..."],
    ',': ["..", "..", "..", "..", "..", ".#", "#."],
    '-': ["...", "...", "...", "###", "...", "...", "..."],
    '.': [".", ".", ".", ".", ".", ".", "#"],
    '/': ["....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."],
    '0': [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    '1': [".#.", "##.", ".#.", ".#.", ".#.", ".#.", "###"],
    '2': ["####.", "....#", "....#", ".###.", "#....", "#....", "#####"],
    '3': ["####.", "....#", "....#", ".###.", "....#", "....#", "####."],
    '4': ["#...#", "#...#", "#...#", "#####", "....#", "....#", "....#"],
    '5': ["#####", "#....", "#....", "####.", "....#", "....#", "####."],
    '6': [".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."],
    '7': ["#####", "....#", "....#", "...#.", "..#..", "..#..", "..#.."],
    '8': [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    '9': [".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."],
    ':': [".", "#", ".", ".", ".", "#", "."],
    ';': ["..", ".#", "..", "..", "..", ".#", "#."],
    '<': ["...#", "..#.", ".#..", "#...", ".#..", "..#.", "...#"],
    '=': ["....", "....", "####", "....", "####", "....", "...."],
    '>': ["#...", ".#..", "..#.", "...#", "..#.", ".#..", "#..."],
    '?': ["####.", "....#", "....#", "..##.", "..#..", ".....", "..#.."],
    '@': [".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".####"],
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'C': [".####", "#....", "#....", "#....", "#....", "#....", ".####"],
    'D': ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    'F': ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    'G': [".####", "#....", "#....", "#..##", "#...#", "#...#", ".###."],
    'H': ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'I': ["###", ".#.", ".#.", ".#.", ".#.", ".#.", "###"],
    'J': ["....#", "....#", "....#", "....#", "....#", "#...#", ".###."],
    'K': ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    'N': ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'Q': [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
    'X': ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    'Z': ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
    '[': ["##", "#.", "#.", "#.", "#.", "#.", "##"],
    '\\': ["#....", "#....", ".#...", "..#..", "...#.", "....#", "....#"],
    ']': ["##", ".#", ".#", ".#", ".#", ".#", "##"],
    '^': ["..#..", ".#.#.", "#...#", ".....", ".....", ".....", "....."],
    '_': [".....", ".....", ".....", ".....", ".....", ".....", "#####"],
}

# name, scale: each size the 1x glyphs blown up whole pixels at a time, its tracking a pixel of its own
FONTS = [("F_S", 1), ("F_M", 2), ("F_L", 3)]
FIRST, LAST = 32, 95


def glyph_image(ch, k):
    rows = GLYPHS[ch]
    w, h = len(rows[0]), len(rows)
    assert h == 7 and all(len(r) == w for r in rows), ch
    im = Image.new("1", (w * k, h * k), 0)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == "#":
                for j in range(k):
                    for i in range(k):
                        im.putpixel((x * k + i, y * k + j), 1)
    return im


def logo_paths():
    """the brand's logotype as web/panel.js holds it: its box and [path, fill rule] pairs"""
    import re, json
    js = open(os.path.join(ROOT, "web", "panel.js")).read()
    m = re.search(r"LOGO: \{ w: (\d+), h: (\d+), paths: (\[\[.*?\]\]) \}", js)
    return int(m.group(1)), int(m.group(2)), json.loads(m.group(3))


def flatten(d):
    """an SVG path's subpaths as polygons: M L H V C Z, absolute (all the logotype uses), cubics in 24 steps"""
    import re
    tok = re.findall(r"[MLHVCZmlhvcz]|-?\d*\.?\d+(?:e-?\d+)?", d)
    polys, cur, i, cmd, x, y = [], [], 0, None, 0.0, 0.0
    num = lambda: float(tok[i])
    while i < len(tok):
        if tok[i].isalpha():
            cmd = tok[i]; i += 1
            if cmd in "Zz":
                if cur: polys.append(cur); cur = []
                continue
        assert cmd in "MLHVC", cmd
        if cmd == "M":
            if cur: polys.append(cur)
            x, y = float(tok[i]), float(tok[i + 1]); i += 2; cur = [(x, y)]; cmd = "L"
        elif cmd == "L":
            x, y = float(tok[i]), float(tok[i + 1]); i += 2; cur.append((x, y))
        elif cmd == "H":
            x = float(tok[i]); i += 1; cur.append((x, y))
        elif cmd == "V":
            y = float(tok[i]); i += 1; cur.append((x, y))
        else:
            p = [float(t) for t in tok[i:i + 6]]; i += 6
            x0, y0 = x, y
            for k in range(1, 25):
                t = k / 24; u = 1 - t
                cur.append((u ** 3 * x0 + 3 * u * u * t * p[0] + 3 * u * t * t * p[2] + t ** 3 * p[4],
                            u ** 3 * y0 + 3 * u * u * t * p[1] + 3 * u * t * t * p[3] + t ** 3 * p[5]))
            x, y = p[4], p[5]
    if cur: polys.append(cur)
    return polys


def logo_mask(width):
    """the logotype, width px wide, 1-bit: a pixel set where most of its 4 x 4 samples fall inside a path (each path
    by its own rule, the paths joined)"""
    W, H, paths = logo_paths()
    sc = width / W
    height = int(round(H * sc))
    S = 4
    acc = [[0] * width for _ in range(height)]
    for d, rule in paths:
        polys = [[(px * sc, py * sc) for px, py in poly] for poly in flatten(d)]
        edges = [(a, b) for poly in polys for a, b in zip(poly, poly[1:] + poly[:1]) if a[1] != b[1]]
        for sy in range(height * S):
            yy = (sy + 0.5) / S
            xs = []
            for (x0, y0), (x1, y1) in edges:
                if (y0 <= yy < y1) or (y1 <= yy < y0):
                    xs.append((x0 + (yy - y0) * (x1 - x0) / (y1 - y0), 1 if y1 > y0 else -1))
            xs.sort()
            for sx in range(width * S):
                xx = (sx + 0.5) / S
                w = n = 0
                for cx, dirn in xs:
                    if cx > xx: break
                    w += dirn; n += 1
                inside = (n & 1) if rule == "evenodd" else (w != 0)
                if inside: acc[sy // S][sx // S] |= 1 << ((sy % S) * S + sx % S)
    img = Image.new("1", (width, height), 0)
    for y in range(height):
        for x in range(width):
            if bin(acc[y][x]).count("1") >= S * S // 2: img.putpixel((x, y), 1)
    return img


def pack(im):
    w, h = im.size
    bpr = (w + 7) // 8
    px = im.load()
    out = []
    for y in range(h):
        for bx in range(bpr):
            b = 0
            for i in range(8):
                x = bx * 8 + i
                if x < w and px[x, y]:
                    b |= 0x80 >> i
            out.append(b)
    return out


def main():
    sheet = None
    if "--sheet" in sys.argv:
        sheet = sys.argv[sys.argv.index("--sheet") + 1]
    lines = ["/* generated by tools/gen_font.py: MONO2K's pixel font at four sizes, and the logotype: do not edit */\n",
             "typedef struct { uint8_t h, base, first, last; int8_t track; const uint16_t *off; const uint8_t *w;"
             " const uint8_t *bits; } m2k_font_t;\n"]
    previews = []
    for name, k in FONTS:
        h = 7 * k
        offs, widths, bits = [], [], []
        prev = Image.new("1", (16 * (6 * k + 4), ((LAST - FIRST) // 16 + 1) * (h + 4)), 0)
        for i, c in enumerate(range(FIRST, LAST + 1)):
            im = glyph_image(chr(c), k)
            offs.append(len(bits))
            widths.append(im.size[0])
            bits.extend(pack(im))
            prev.paste(im, ((i % 16) * (6 * k + 4), (i // 16) * (h + 4)))
        previews.append(prev)
        lines.append("static const uint16_t %s_OFF[%d] = {%s};\n" % (name, len(offs), ",".join(map(str, offs))))
        lines.append("static const uint8_t %s_W[%d] = {%s};\n" % (name, len(widths), ",".join(map(str, widths))))
        lines.append("static const uint8_t %s_BITS[%d] = {\n" % (name, len(bits)))
        for j in range(0, len(bits), 24):
            lines.append("    " + ",".join("0x%02x" % b for b in bits[j:j + 24]) + ",\n")
        lines.append("};\n")
        # the line box is the capitals (h = base: their last row base - 1); a pixel of tracking a scale
        lines.append("static const m2k_font_t %s = {%d, %d, %d, %d, %d, %s_OFF, %s_W, %s_BITS};\n" %
                     (name, h, h, FIRST, LAST, k, name, name, name))
        print("%s: %dx, %d px, %d bytes" % (name, k, h, len(bits)))

    # the logotype: the brand's (web/panel.js, LOGO), 232 px wide, rasterized from its paths (each its own fill rule,
    # sampled 4 x 4 a pixel). LOGO_BITS its fill, LOGO_EDGE its outline (a fill pixel with an empty neighbour), as the
    # web's hero draws it: the outline solid, the inside dithered
    on = logo_mask(232)
    lw, lh = on.size
    edge = Image.new("1", (lw, lh), 0)
    for y in range(lh):
        for x in range(lw):
            if on.getpixel((x, y)) and any(not (0 <= x + dx < lw and 0 <= y + dy < lh) or not on.getpixel((x + dx, y + dy))
                                           for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                edge.putpixel((x, y), 1)
    lb, eb = pack(on), pack(edge)
    lines.append("#define LOGO_W %d\n#define LOGO_H %d\nstatic const uint8_t LOGO_BITS[%d] = {\n" % (lw, lh, len(lb)))
    for j in range(0, len(lb), 24):
        lines.append("    " + ",".join("0x%02x" % b for b in lb[j:j + 24]) + ",\n")
    lines.append("};\nstatic const uint8_t LOGO_EDGE[%d] = {\n" % len(eb))
    for j in range(0, len(eb), 24):
        lines.append("    " + ",".join("0x%02x" % b for b in eb[j:j + 24]) + ",\n")
    lines.append("};\n")
    # the dither's density per column, sparse to solid left to right (the hero's 0.14 + 0.86 u^1.3), of 64
    dens = [round((0.14 + 0.86 * (x / lw) ** 1.3) * 64) for x in range(lw)]
    lines.append("static const uint8_t LOGO_DENS[%d] = {%s};\n" % (lw, ",".join(str(v) for v in dens)))
    previews.append(on)
    previews.append(edge)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("".join(lines))
    print("wrote", os.path.normpath(OUT))
    if sheet:
        W = max(p.size[0] for p in previews)
        H = sum(p.size[1] + 8 for p in previews)
        s = Image.new("1", (W, H), 0)
        y = 0
        for p in previews:
            s.paste(p, (0, y))
            y += p.size[1] + 8
        s = s.point(lambda v: 0 if v else 255).convert("L").resize((W * 2, H * 2), Image.NEAREST)
        os.makedirs(os.path.dirname(sheet), exist_ok=True)
        s.save(sheet)
        print("sheet", sheet)


main()
