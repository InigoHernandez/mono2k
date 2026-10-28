"""The FM-1 Firmware Hub's pictures from tools/hub_pics.c's frames: docs/hub/screen.png (240), screen-4x.png (960),
screens-wide.png (HOME and the chord machine's page, 3x), mono2k.gif (240) and mono2k-2x.gif (480), 80 ms a frame,
looping. Usage: .venv/bin/python tools/hub_pics.py DIR [OUT=docs/hub]"""
import glob, os, sys
from PIL import Image

src = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else "docs/hub"


def frame(path):
    return Image.open(path).convert("1").convert("RGB")


home, chord = frame(os.path.join(src, "home.pbm")), frame(os.path.join(src, "chord.pbm"))
home.save(os.path.join(out, "screen.png"), optimize=True)
home.resize((960, 960), Image.NEAREST).save(os.path.join(out, "screen-4x.png"), optimize=True)
wide = Image.new("RGB", (1512, 720), (0, 0, 0))
wide.paste(home.resize((720, 720), Image.NEAREST), (0, 0))
wide.paste(chord.resize((720, 720), Image.NEAREST), (792, 0))
wide.save(os.path.join(out, "screens-wide.png"), optimize=True)
frames = [Image.open(p).convert("1").convert("L") for p in sorted(glob.glob(os.path.join(src, "f*.pbm")))]
for name, z in (("mono2k.gif", 1), ("mono2k-2x.gif", 2)):
    fs = [f if z == 1 else f.resize((240 * z, 240 * z), Image.NEAREST) for f in frames]
    fs[0].save(os.path.join(out, name), save_all=True, append_images=fs[1:], duration=80, loop=0, optimize=True)
print(len(frames), "frames;", ", ".join(sorted(os.listdir(out))))
