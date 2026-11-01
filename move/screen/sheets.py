#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K on the Move: the screen's sheets as PNGs, from move/screen/sheets.c's frames (128 x 64 PBMs, 1 = lit).
#   .venv/bin/python move/screen/sheets.py build/move/sheets
# Writes, beside the PBMs:
#   png/NAME.png        each frame x6, white on black as the Move's OLED shows it
#   png/NAME_grid.png   the same with a 1 px gap between the pixels (the grid visible)
#   overview.png        every frame x3 on one sheet, labelled underneath (the owner's first look)
#   motion_*.gif        the motions (a knob turned, the pages turned, the playhead), x4, 50 ms a frame
# The labels are set in the owner's pixel font (assets/fonts/mono2k-pixel, CC BY-SA), as the screen is.
import os
import sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
FACE = os.path.join(ROOT, "assets", "fonts", "mono2k-pixel", "mono2k-font.otf")
W, H = 128, 64
INK, PAPER, GAP, BG = 236, 0, 24, 16          # the OLED's lit and dark pixels, the gap grid, the sheet


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    # P4: "P4\n128 64\n" then the rows
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
    out = Image.new("L", (im.width + 2 * pad, im.height + 2 * pad), BG)
    out.paste(im, (pad, pad))
    return out


def main():
    src = sys.argv[1]
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
    Z, cols, pad, lab = 3, 4, 24, 40
    cw, ch = W * Z + 2 * 6, H * Z + 2 * 6
    rows = (len(man) + cols - 1) // cols
    sheet = Image.new("L", (pad + cols * (cw + pad), 70 + rows * (ch + lab + pad)), BG)
    d = ImageDraw.Draw(sheet)
    d.fontmode = "1"
    d.text((pad, 22), "MONO2K ON THE MOVE  128 X 64  THE SCREEN", font=font, fill=200)
    for i, (name, label) in enumerate(man):
        im, _ = frames[name]
        x = pad + (i % cols) * (cw + pad)
        y = 70 + (i // cols) * (ch + lab + pad)
        sheet.paste(framed(scaled(im, Z), 6), (x, y))
        d.text((x, y + ch + 10), label.upper(), font=font, fill=170)
    sheet.save(os.path.join(src, "overview.png"))

    # the motions
    fdir = os.path.join(src, "frames")
    for mo in sorted(os.listdir(fdir)) if os.path.isdir(fdir) else []:
        files = sorted(f for f in os.listdir(os.path.join(fdir, mo)) if f.endswith(".pbm"))
        if not files:
            continue
        ims = [framed(scaled(load(os.path.join(fdir, mo, f)), 4), 8).convert("P") for f in files]
        ims[0].save(os.path.join(src, "motion_%s.gif" % mo), save_all=True, append_images=ims[1:], duration=50,
                    loop=0, optimize=False, disposal=1)
    print("sheets:", len(man), "frames,", os.path.join(src, "overview.png"))


main()
