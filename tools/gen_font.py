#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K's 1-bit fonts: the owner's mono2k-font.otf, the FontStruction "Elektron Pixel Font" by savingaurora
# (fontstruct.com/fontstructions/show/70152, CC BY-SA 3.0; assets/fonts/mono2k-pixel, its license and readme beside
# it), adapted: one weight on a 1 px grid, capitals 5 rows at its native 8 px. Every glyph is rendered there, cropped
# to its ink and scaled a whole number of times by nearest neighbour (x2, x3), so every stroke is whole pixels (held:
# each glyph at its native size times z is the font rendered at z times the size, pixel for pixel); set a whole design
# pixel apart. The bitmaps it writes, an adaptation, are under CC BY-SA 4.0 (as CC BY-SA 3.0's section 4(b) allows;
# 4.0 is one-way compatible with the GPL-3.0 the firmware is under). Plus the logotype, rasterized from the
# brand's paths (Orbitron's, the wordmark: unchanged). Writes firmware/gen/m2k_fonts.h.
#   .venv/bin/python tools/gen_font.py [--sheet build/fonts.png]
import os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
FACE = os.path.join(ROOT, "assets", "fonts", "mono2k-pixel", "mono2k-font.otf")
OUT = os.path.join(ROOT, "firmware", "gen", "m2k_fonts.h")
NATIVE = 8                     # the font's own size: a design pixel to a screen pixel

# name, scale: one weight (the owner's: "don't use bold anymore"), a ladder by width (fit_font steps down it)
FONTS = [
    ("F_S", 2),                # every line: labels, values, names, hints, the bars' type (capitals 10 rows, ~10 px a
                               # character)
    ("F_L", 3),                # a page's one big word (capitals 15 rows)
]
FIRST, LAST = 32, 95
SPACE = 2                      # a space's design pixels (with the tracking each side: 4 between two words' ink)


def load(size):
    return ImageFont.truetype(FACE, size)


def ink_box(font, ch):
    """a glyph's ink as rendered, from the pen's top (the font's own bounding box is a row too deep at 8 px)"""
    im = Image.new("1", (NATIVE * 3, NATIVE * 3), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = "1"
    d.text((NATIVE, 0), ch, font=font, fill=1)
    return im.getbbox()


def native(font, ch, cap_top, cap, desc):
    """a glyph at the native size, cropped to its ink sideways, its rows from the capitals' top (0) down to the
    descenders (cap + desc)"""
    pad = 8
    im = Image.new("1", (NATIVE * 2 + 2 * pad, cap + desc + 2 * pad), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = "1"
    d.text((pad, pad - cap_top), ch, font=font, fill=1)
    px = im.load()
    W, H = im.size
    cols = [x for x in range(W) if any(px[x, y] for y in range(H))]
    if not cols:                                         # the space: blank, SPACE wide
        return Image.new("1", (SPACE, cap + desc), 0)
    rows = list(range(pad, pad + cap + desc))
    if ch == "?":                                        # (it rises a row over the capitals: one of its stem's two
        rows = [pad - 1, pad, pad + 1, pad + 3] + list(range(pad + 4, pad + cap + desc))   # rows goes, the arc stays)
    out = Image.new("1", (cols[-1] - cols[0] + 1, cap + desc), 0)
    q = out.load()
    for y, r in enumerate(rows):
        for x in range(out.size[0]):
            if px[cols[0] + x, r]:
                q[x, y] = 1
    return out


def crisp(z, ch, g):
    """the glyph g (native, scaled z times) against the font rendered at z times the size: the same pixels, in the
    same place below the capitals' top (? alone is redrawn: it rose a row over them)"""
    if ch == "?":
        return True
    def ink(f, c, pad):
        im = Image.new("1", (NATIVE * 2 * z + 2 * pad, NATIVE * 3 * z + 2 * pad), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        d.text((pad, pad), c, font=f, fill=1)
        return im
    f = load(NATIVE * z)
    big, hb = ink(f, ch, 8 * z), ink(f, "H", 8 * z).getbbox()
    big = big.crop((0, hb[1], big.size[0], hb[1] + g.size[1]))   # (the line's rows: a glyph above them is cut alike)
    bb, gb = big.getbbox(), g.getbbox()
    if not bb or not gb:
        return not bb and not gb
    return list(big.crop(bb).getdata()) == list(g.crop(gb).getdata()) and bb[1] == gb[1]


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
    lines = ["/* generated by tools/gen_font.py: do not edit. The type's glyphs are adapted from \"Elektron Pixel Font\" "
             "by savingaurora (FontStruct, fontstruct.com/fontstructions/show/70152), the owner's mono2k-font.otf, "
             "CC BY-SA 3.0; this adaptation is licensed CC BY-SA 4.0 (creativecommons.org/licenses/by-sa/4.0, as 3.0's "
             "section 4(b) allows: one-way compatible with GPL-3.0; assets/fonts/mono2k-pixel); the "
             "logotype is MONO2K's wordmark (Orbitron, SIL OFL 1.1) */\n",
             "/* h, base, w in screen pixels; bits a glyph at its native size (rows h / z, (w / z + 7) / 8 bytes "
             "each), drawn z times over */\n",
             "typedef struct { uint8_t h, base, first, last; int8_t track; uint8_t z; const uint16_t *off; "
             "const uint8_t *w; const uint8_t *bits; } m2k_font_t;\n"]
    # a native row's byte widened z times (its bits doubled or tripled, MSB first): drawn a row a byte
    lines.append("static const uint16_t FONT_Z2[256] = {%s};\n" %
                 ",".join("0x%04x" % sum(3 << (14 - 2 * i) for i in range(8) if b >> (7 - i) & 1) for b in range(256)))
    lines.append("static const uint32_t FONT_Z3[256] = {%s};\n" %
                 ",".join("0x%06x" % sum(7 << (21 - 3 * i) for i in range(8) if b >> (7 - i) & 1) for b in range(256)))
    previews = []
    for name, z in FONTS:
        font = load(NATIVE)
        hb = ink_box(font, "H")
        cap_top, cap = hb[1], hb[3] - hb[1]
        desc = max(ink_box(font, chr(c))[3] for c in range(FIRST, LAST + 1) if ink_box(font, chr(c))) - hb[3]
        desc = max(desc, 1)
        track = z                                        # (a design pixel between two glyphs' ink)
        h = (cap + desc) * z
        offs, widths, bits = [], [], []
        prev = Image.new("1", (16 * (8 * z + 4), ((LAST - FIRST) // 16 + 1) * (h + 4)), 0)
        for i, c in enumerate(range(FIRST, LAST + 1)):
            n = native(font, chr(c), cap_top, cap, desc)
            im = n.resize((n.size[0] * z, n.size[1] * z), Image.NEAREST)
            if c != 32 and not crisp(z, chr(c), im):
                raise SystemExit("%s: %r is not whole pixels at x%d" % (name, chr(c), z))
            offs.append(len(bits))
            widths.append(im.size[0])
            bits.extend(pack(n))                         # (native: the draw widens it, FONT_Z2 / FONT_Z3)
            prev.paste(im, ((i % 16) * (8 * z + 4), (i // 16) * (h + 4)))
        previews.append(prev)
        lines.append("static const uint16_t %s_OFF[%d] = {%s};\n" % (name, len(offs), ",".join(map(str, offs))))
        lines.append("static const uint8_t %s_W[%d] = {%s};\n" % (name, len(widths), ",".join(map(str, widths))))
        lines.append("static const uint8_t %s_BITS[%d] = {\n" % (name, len(bits)))
        for j in range(0, len(bits), 24):
            lines.append("    " + ",".join("0x%02x" % b for b in bits[j:j + 24]) + ",\n")
        lines.append("};\n")
        # h: the capitals and the descenders; base: the capitals' rows (y .. y + base - 1), what every layout plans on
        lines.append("static const m2k_font_t %s = {%d, %d, %d, %d, %d, %d, %s_OFF, %s_W, %s_BITS};\n" %
                     (name, h, cap * z, FIRST, LAST, track, z, name, name, name))
        # the full stop on the baseline, the comma from it down (the font's own: held, as at 8 px they sit)
        dot, com = native(font, ".", cap_top, cap, desc), native(font, ",", cap_top, cap, desc)
        if dot.getbbox()[3] != cap or com.getbbox()[1] != cap - 1 or com.getbbox()[3] <= cap:
            raise SystemExit("%s: the full stop or the comma is off the baseline" % name)
        print("%s: mono2k-font x%d, capitals %d, line %d, %d bytes" % (name, z, cap * z, h, len(bits)))

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
