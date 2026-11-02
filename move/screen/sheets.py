#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K on the Move: the sheets as PNGs and GIFs, from move/screen/sheets.c's frames (128 x 64 PBMs, 1 = lit), its
# LEDs and its pad maps.
#   .venv/bin/python move/screen/sheets.py build/move/sheets
# Writes, beside them:
#   png/NAME.png        each frame x6, white on black as the Move's OLED shows it
#   png/NAME_grid.png   the same with a 1 px gap between the pixels (the grid visible)
#   overview.png        every frame x3 on one sheet, labelled underneath (the owner's first look)
#   padmap.png          the Move's 32 pads, 16 steps and 4 track buttons, labelled and lit as the controller lights
#                       them, beside the screen, in three states (HOME in view; keys held; PERFORM on)
#   motion_*.gif        the motions (a knob touched, the page pads pressed, the playhead), the screen beside the pads
#                       and the steps as they light, at the screen's pace (34.8 ms a frame)
#   knob_rows.png       every page's knob row (SYN on every machine, a step's locks, a knob touched), rows 45..63 x6
#   knobs_before_after.png   with --before OLD (another build's sheets): each row as it was over as it is
#   .venv/bin/python move/screen/sheets.py build/move/sheets [--before build/move/sheets_before]
# The labels are set in the owner's pixel font (assets/fonts/mono2k-pixel, CC BY-SA), as the screen is. The LED colours
# are the Move's palette entries MONO2K uses (Schwung's constants.mjs, MIT).
import os
import sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
FACE = os.path.join(ROOT, "assets", "fonts", "mono2k-pixel", "mono2k-font.otf")
W, H = 128, 64
INK, PAPER, GAP, BG = 236, 0, 24, 16          # the OLED's lit and dark pixels, the gap grid, the sheet
SHEET = (16, 16, 16)
PAD_EDGE = (44, 44, 44)                       # an unlit pad's rim, so the hardware reads where it is dark
PALETTE = {                                   # the Move's palette, the entries MONO2K lights
    0: (0, 0, 0), 1: (0xFF, 0x40, 0x32), 3: (0xC9, 0x3C, 0x00), 16: (0x31, 0xAD, 0xFF), 17: (0x36, 0x63, 0xFC),
    26: (0xFF, 0x2B, 0xD4), 66: (0x21, 0x08, 0x06), 69: (0x5D, 0x17, 0x00), 95: (0x13, 0x45, 0x66),
    109: (0x3C, 0x11, 0x66), 115: (0x66, 0x11, 0x54), 118: (0x59, 0x59, 0x59), 119: (0x1A, 0x1A, 0x1A),
    120: (0xFF, 0xFF, 0xFF), 123: (0x40, 0x40, 0x40), 124: (0x14, 0x14, 0x14),
}


def rgb(i):
    return PALETTE.get(i, (0x80, 0x00, 0x80))  # (an entry not listed: loud, so it is noticed)


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(b"\n", 2)
    assert parts[0] == b"P4", path
    w, h = map(int, parts[1].split())
    bits = parts[2]
    im = Image.new("L", (w, h), PAPER)
    px = im.load()
    rb = (w + 7) // 8
    for y in range(h):
        for x in range(w):
            if bits[y * rb + x // 8] >> (7 - x % 8) & 1:
                px[x, y] = INK
    return im


def scaled(im, z, grid=False):
    big = im.resize((im.width * z, im.height * z), Image.NEAREST)
    if grid:
        d = ImageDraw.Draw(big)
        for x in range(0, big.width, z):
            d.line([(x, 0), (x, big.height)], fill=GAP)
        for y in range(0, big.height, z):
            d.line([(0, y), (big.width, y)], fill=GAP)
    return big


def framed(im, pad):
    """the screen in its bezel: a margin of the sheet's grey round it"""
    out = Image.new(im.mode, (im.width + 2 * pad, im.height + 2 * pad), BG if im.mode == "L" else SHEET)
    out.paste(im, (pad, pad))
    return out


def leds_of(path):
    """63 LEDs: 16 steps, 32 pads, 4 track buttons, 11 buttons"""
    s = open(path).read().strip()
    v = [int(s[i:i + 2], 16) for i in range(0, len(s), 2)]
    return {"step": v[0:16], "pad": v[16:48], "trk": v[48:52]}


def text_fit(d, xy, s, fonts, width, fill, anchor="mm"):
    for f in fonts:
        if d.textlength(s, font=f) <= width:
            d.text(xy, s, font=f, fill=fill, anchor=anchor)
            return


def device(d, x0, y0, leds, labels=None, pad=64, gap=8, fonts=None):
    """the Move's track buttons, pads (row 0 at the bottom) and steps, as lit; labelled where labels are given"""
    rows = 4
    tb = pad // 3
    for r in range(rows):                     # the track buttons, one beside each pad row (T1 the top)
        y = y0 + r * (pad + gap) + (pad - tb) // 2
        c = rgb(leds["trk"][r])
        d.rounded_rectangle([x0, y, x0 + tb, y + tb], radius=tb // 3, fill=c, outline=PAD_EDGE)
        if labels:
            text_fit(d, (x0 + tb // 2, y + tb + 10), labels["trk"][r], fonts, pad, (150, 150, 150))
    px0 = x0 + tb + gap * 2
    for i in range(32):
        r, c = i // 8, i % 8
        x = px0 + c * (pad + gap)
        y = y0 + (rows - 1 - r) * (pad + gap)
        col = rgb(leds["pad"][i])
        d.rounded_rectangle([x, y, x + pad, y + pad], radius=max(2, pad // 10), fill=col, outline=PAD_EDGE)
        if labels and labels["pad"][i]:
            lum = 0.3 * col[0] + 0.59 * col[1] + 0.11 * col[2]
            text_fit(d, (x + pad // 2, y + pad // 2), labels["pad"][i], fonts, pad - 6,
                     (16, 16, 16) if lum > 140 else (225, 225, 225))
    sy = y0 + rows * (pad + gap) + gap
    sw = (8 * pad + 7 * gap - 15 * gap) // 16
    sh = max(6, pad // 3)
    for k in range(16):
        x = px0 + k * (sw + gap)
        col = rgb(leds["step"][k])
        d.rounded_rectangle([x, sy, x + sw, sy + sh], radius=max(1, sh // 4), fill=col, outline=PAD_EDGE)
        if labels:
            text_fit(d, (x + sw // 2, sy + sh // 2), labels["step"][k], fonts, sw - 2,
                     (16, 16, 16) if sum(col) > 400 else (200, 200, 200))
    return px0 + 8 * pad + 7 * gap - x0, sy + sh - y0


def padmap_of(path):
    leds = {"pad": [0] * 32, "step": [0] * 16, "trk": [0] * 4}
    labels = {"pad": [""] * 32, "step": [""] * 16, "trk": [""] * 4}
    screen = ""
    for line in open(path):
        f = line.rstrip("\n").split("\t")
        if f[0] == "screen":
            screen = f[1]
            continue
        k, i, lb, c = f[0], int(f[1]), f[2], int(f[3])
        leds[k][i] = c
        labels[k][i] = lb
    return leds, labels, screen


ROW0, ROW1 = 45, 64                           # the knob row's band: a lock's line, the boxes, the touched line


def rows_of(src):
    """sheets.c's knob rows: (name, label, the frame's rows 45..63) in its order"""
    d = os.path.join(src, "rows")
    if not os.path.exists(os.path.join(d, "manifest.tsv")):
        return []
    out = []
    for l in open(os.path.join(d, "manifest.tsv")):
        f = l.rstrip("\n").split("\t")
        if len(f) == 2:
            out.append((f[0], f[1], load(os.path.join(d, f[0] + ".pbm")).crop((0, ROW0, W, ROW1))))
    return out


def strips(path, title, blocks, font, z=6, cols=2):
    """blocks of labelled strips (each a list of (tag, image)), in columns, x z, white on black in a bezel"""
    sw, sh, tagw, lab, pad = W * z + 12, (ROW1 - ROW0) * z + 12, 120, 34, 30
    bh = lab + max(len(b[1]) for b in blocks) * (sh + 8) + pad
    rows = (len(blocks) + cols - 1) // cols
    sheet = Image.new("L", (pad + cols * (tagw + sw + pad), 80 + rows * bh), BG)
    d = ImageDraw.Draw(sheet)
    d.fontmode = "1"
    d.text((pad, 26), title, font=font, fill=200)
    for i, (label, items) in enumerate(blocks):
        x = pad + (i % cols) * (tagw + sw + pad)
        y = 80 + (i // cols) * bh
        d.text((x, y), label.upper(), font=font, fill=170)
        for j, (tag, im) in enumerate(items):
            yy = y + lab + j * (sh + 8)
            d.text((x, yy + sh // 2), tag, font=font, fill=120, anchor="lm")
            sheet.paste(framed(scaled(im, z), 6), (x + tagw, yy))
    sheet.save(path)


def knob_sheets(src, before):
    font = ImageFont.truetype(FACE, 16)
    rows = rows_of(src)
    if not rows:
        return
    strips(os.path.join(src, "knob_rows.png"), "MONO2K ON THE MOVE  THE KNOB ROWS, ROWS 45-63, X6",
           [(lb, [("", im)]) for _, lb, im in rows], font)
    if before:
        old = {n: im for n, _, im in rows_of(before)}
        blocks = [(lb, [("BEFORE", old[n]), ("AFTER", im)]) for n, lb, im in rows if n in old]
        strips(os.path.join(src, "knobs_before_after.png"),
               "MONO2K ON THE MOVE  THE KNOB ROW, BEFORE AND AFTER (ROWS 45-63, X6)", blocks, font)


def main():
    src = sys.argv[1]
    before = sys.argv[sys.argv.index("--before") + 1] if "--before" in sys.argv else None
    knob_sheets(src, before)
    man = [l.rstrip("\n").split("\t") for l in open(os.path.join(src, "manifest.tsv")) if "\t" in l]
    os.makedirs(os.path.join(src, "png"), exist_ok=True)
    frames = {}
    for name, label in man:
        im = load(os.path.join(src, name + ".pbm"))
        frames[name] = (im, label)
        scaled(im, 6).save(os.path.join(src, "png", name + ".png"))
        scaled(im, 6, grid=True).save(os.path.join(src, "png", name + "_grid.png"))

    # the overview: x3, four across, each labelled underneath in the owner's type (x2: 16 px)
    font = ImageFont.truetype(FACE, 16)
    small = ImageFont.truetype(FACE, 8)
    Z, cols, pad, lab = 3, 4, 24, 40
    cw, ch = W * Z + 2 * 6, H * Z + 2 * 6
    rows = (len(man) + cols - 1) // cols
    sheet = Image.new("L", (pad + cols * (cw + pad), 70 + rows * (ch + lab + pad)), BG)
    d = ImageDraw.Draw(sheet)
    d.fontmode = "1"
    d.text((pad, 22), "MONO2K ON THE MOVE  128 X 64  THE SCREEN, CALMER", font=font, fill=200)
    for i, (name, label) in enumerate(man):
        im, _ = frames[name]
        x = pad + (i % cols) * (cw + pad)
        y = 70 + (i // cols) * (ch + lab + pad)
        sheet.paste(framed(scaled(im, Z), 6), (x, y))
        d.text((x, y + ch + 10), label.upper(), font=font, fill=170)
    sheet.save(os.path.join(src, "overview.png"))

    # the pad map: three states, each the screen beside the device
    states = [("home", "HOME IN VIEW: THE PAGES, THE KEYBOARD"), ("keys", "TRK, THREE KEYS HELD (C E G), THE ARP"),
              ("perform", "PERFORM ON: THE KEYS ARE ITS EFFECTS")]
    states = [s for s in states if os.path.exists(os.path.join(src, "padmap_%s.tsv" % s[0]))]
    PADP, GAPP, SZ = 72, 10, 3
    dev_w = PADP // 3 + 2 * GAPP + 8 * PADP + 7 * GAPP
    dev_h = 4 * (PADP + GAPP) + GAPP + PADP // 3
    panel_h = max(dev_h, H * SZ + 12) + 70
    sheet = Image.new("RGB", (40 + W * SZ + 12 + 60 + dev_w + 220, 90 + len(states) * panel_h), SHEET)
    d = ImageDraw.Draw(sheet)
    d.fontmode = "1"
    d.text((40, 30), "MONO2K ON THE MOVE  THE PADS", font=font, fill=(210, 210, 210))
    rowlab = ["THE SOUND", "THE REST", "BLACK KEYS", "WHITE KEYS"]
    for si, (st, title) in enumerate(states):
        leds, labels, screen = padmap_of(os.path.join(src, "padmap_%s.tsv" % st))
        y0 = 90 + si * panel_h
        d.text((40, y0), title, font=font, fill=(170, 170, 170))
        scr = framed(scaled(load(os.path.join(src, "padmap_%s.pbm" % st)), SZ), 6).convert("RGB")
        sheet.paste(scr, (40, y0 + 34))
        dx = 40 + scr.width + 60
        device(d, dx, y0 + 34, leds, labels, PADP, GAPP, [font, small])
        for r in range(4):
            lab = rowlab[r]
            if st == "perform" and r >= 2:
                lab = ["BANK B", "BANK A"][r - 2]
            d.text((dx + dev_w + 20, y0 + 34 + r * (PADP + GAPP) + PADP // 2), lab, font=font, fill=(150, 150, 150),
                   anchor="lm")
        d.text((dx + dev_w + 20, y0 + 34 + 4 * (PADP + GAPP) + GAPP + PADP // 6), "THE SEQUENCER", font=font,
               fill=(150, 150, 150), anchor="lm")
    sheet.save(os.path.join(src, "padmap.png"))

    # the motions: the screen x3 beside the pads and the steps as they light, a frame 34.8 ms (35)
    fdir = os.path.join(src, "frames")
    pal = Image.new("P", (1, 1))
    cols_ = [SHEET, (INK,) * 3, (PAPER,) * 3, (BG,) * 3, PAD_EDGE] + list(PALETTE.values())
    flat = [c for col in cols_ for c in col]
    pal.putpalette(flat + [0] * (768 - len(flat)))
    for mo in sorted(os.listdir(fdir)) if os.path.isdir(fdir) else []:
        files = sorted(f[:-4] for f in os.listdir(os.path.join(fdir, mo)) if f.endswith(".pbm"))
        if not files:
            continue
        ims = []
        for f in files:
            scr = framed(scaled(load(os.path.join(fdir, mo, f + ".pbm")), 3), 6).convert("RGB")
            ledp = os.path.join(fdir, mo, f + ".led")
            im = Image.new("RGB", (scr.width + 30 + 30 // 3 + 2 * 5 + 8 * 30 + 7 * 5 + 14, max(scr.height, 200) + 20), SHEET)
            im.paste(scr, (10, 10))
            if os.path.exists(ledp):
                device(ImageDraw.Draw(im), scr.width + 30, 14, leds_of(ledp), None, 30, 5)
            ims.append(im.quantize(palette=pal, dither=Image.Dither.NONE))
        ims[0].save(os.path.join(src, "motion_%s.gif" % mo), save_all=True, append_images=ims[1:], duration=35,
                    loop=0, optimize=False, disposal=1)
    print("sheets:", len(man), "frames,", os.path.join(src, "overview.png"), os.path.join(src, "padmap.png"))


main()
