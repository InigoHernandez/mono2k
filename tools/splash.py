#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The splash for the eye: the host's frames (m2k_host DIR --splash, 40 ms apart) as an animated GIF and a montage.
#   build/host/m2k_host build/splash --splash && .venv/bin/python tools/splash.py build/splash [scale]
# writes DIR/splash.gif (at the FM-1's size times scale, 2 by default) and DIR/splash_sheet.png (every other frame)
import sys, glob, os
from PIL import Image, ImageDraw

src = sys.argv[1]
scale = int(sys.argv[2]) if len(sys.argv) > 2 else 2
files = sorted(glob.glob(os.path.join(src, "splash_[0-9]*.pbm")))
if not files:
    sys.exit("no splash_NNN.pbm in %s: run m2k_host %s --splash" % (src, src))
frames = [Image.open(f).convert("L").resize((240 * scale, 240 * scale), Image.NEAREST) for f in files]
gif = os.path.join(src, "splash.gif")
frames[0].save(gif, save_all=True, append_images=frames[1:] + [frames[-1]] * 25, duration=40, loop=0, optimize=True)
cols, step, th = 8, 2, 120
pick = list(range(0, len(frames), step))
rows = (len(pick) + cols - 1) // cols
sheet = Image.new("L", (cols * (th + 6) + 6, rows * (th + 18) + 6), 160)
d = ImageDraw.Draw(sheet)
for k, i in enumerate(pick):
    x, y = 6 + (k % cols) * (th + 6), 6 + (k // cols) * (th + 18)
    sheet.paste(Image.open(files[i]).convert("L").resize((th, th), Image.LANCZOS), (x, y))
    d.text((x, y + th + 2), "%d ms" % (i * 40), fill=0)
png = os.path.join(src, "splash_sheet.png")
sheet.save(png)
print(gif, png, len(files), "frames")
