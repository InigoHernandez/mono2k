#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K's machine marks. Each machine has a brand mark, a slanted, chamfered letterform (after the Y2K techno
# faces) drawn on a module grid. It is used twice: flat (64 px, the machine card, as Model:Cycles shows its
# engines) and extruded in 3D, ray-marched, its face solid and its sides dithered to 1 bit with screen-fixed
# patterns (after Kloroform's dithered letters), in 12 frames that move as the machine does (the sine travels, the
# noise flickers, the FM's modulators swing, the saw detunes ...) while it sways. Writes firmware/gen/m2k_icons.h.
# --move writes the Move's set instead, move/screen/m2k_icons_move.h (the FM-1 never includes it): the same marks and
# motions at 36 px (about 31 rows of ink: the SYN page's stage, beside the model, on the 128 x 64 OLED), cropped to the
# ink of every frame; no flat marks.
#   .venv/bin/python tools/gen_icons.py [--move] [--sheet build/icons.png]
import os, sys, math
import numpy as np
from PIL import Image, ImageDraw, ImageChops

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOVE = "--move" in sys.argv
OUT = os.path.join(ROOT, "move", "screen", "m2k_icons_move.h") if MOVE else os.path.join(ROOT, "firmware", "gen", "m2k_icons.h")
N = 36 if MOVE else 64      # pixels of a frame
FRAMES = 12
G = 128         # the design grid


# The marks are isotypes (after Fors' plugin marks): each a small symbol for what its machine does, all of one family:
#   a 12-unit grid, every mark inside the same box (1..11); solid ink only; one weight: strokes and bars of 2 units,
#   tiles of 2, square ends; circles and their arcs, triangles and tiles; and one signature, THE SLIT: every piece is
#   set apart from the next by half a unit.
SS = 4                      # supersampling while drawing
U = G * SS / 12.0           # one unit, in drawing pixels
SL = 0.5                    # the slit


def mark(name, ph=None, tooth=None, level=0, halo=False):
    """the mark, flat (ph None), or at a phase 0..1 of its own motion, one loop seamless; SAW's one tooth alone at a
    level 0..7 (tooth), or that tooth's footprint with its halo (halo: what it hides of the teeth behind)"""
    im = Image.new("L", (G * SS, G * SS), 0)
    a = ph is not None
    w2 = 2 * math.pi * (ph or 0.0)                    # the loop's angle
    fr = int(round((ph or 0.0) * FRAMES)) % FRAMES    # the frame, for the stepped motions
    d = ImageDraw.Draw(im)
    W = 255

    def R(x0, y0, x1, y1, c=W):
        d.rectangle([x0 * U, y0 * U, x1 * U - 1, y1 * U - 1], fill=c)

    def P(pts, c=W):
        d.polygon([(x * U, y * U) for x, y in pts], fill=c)

    def DISC(cx, cy, r, c=W):
        d.ellipse([(cx - r) * U, (cy - r) * U, (cx + r) * U - 1, (cy + r) * U - 1], fill=c)

    def ARC(cx, cy, ro, ri, a0, a1):                 # a ring sector, degrees clockwise from 3 o'clock; adds ink only
        t = Image.new("L", im.size, 0)
        td = ImageDraw.Draw(t)
        td.pieslice([(cx - ro) * U, (cy - ro) * U, (cx + ro) * U - 1, (cy + ro) * U - 1], a0, a1, fill=255)
        if ri > 0:
            td.pieslice([(cx - ri) * U, (cy - ri) * U, (cx + ri) * U - 1, (cy + ri) * U - 1], a0, a1, fill=0)
        im.paste(255, mask=t)

    def STROKE(pts, w, c=W):                          # a curve of width w, its ends cut square
        L, Rr = [], []
        for i, (x, y) in enumerate(pts):
            x0, y0 = pts[max(i - 1, 0)]
            x1, y1 = pts[min(i + 1, len(pts) - 1)]
            tx, ty = x1 - x0, y1 - y0
            n = math.hypot(tx, ty) or 1
            nx, ny = -ty / n * w / 2, tx / n * w / 2
            L.append((x + nx, y + ny))
            Rr.append((x - nx, y - ny))
        P(L + Rr[::-1], c)

    def SLIT(cx, cy, ang, length):                    # a cut across a form, half a unit wide
        ux, uy = math.cos(ang), math.sin(ang)
        vx, vy = -uy * SL / 2, ux * SL / 2
        hx, hy = ux * length / 2, uy * length / 2
        P([(cx - hx - vx, cy - hy - vy), (cx + hx - vx, cy + hy - vy), (cx + hx + vx, cy + hy + vy),
           (cx - hx + vx, cy - hy + vy)], 0)

    if name == "GSIN":          # the sine: one cycle, a slit where it crosses zero
        pts = [(1.2 + 9.6 * k / 80, 6 - 3.2 * math.sin(2 * math.pi * k / 80 - w2)) for k in range(81)]
        STROKE(pts, 2.0)
        slope = 3.2 * 2 * math.pi / 9.6
        if not a:
            SLIT(6.0, 6.0, math.atan2(slope, 1) + math.pi / 2, 4)
        else:                   # moving: travelling, the slits ride its zero crossings
            for c, sg in ((0.5, 1), (0.0, -1), (1.0, -1)):
                x = 1.2 + 9.6 * ((c + ph) % 1.0 if c < 1 else 1.0 + ph)
                if 2.2 < x < 9.8:
                    SLIT(x, 6.0, math.atan2(sg * slope, 1) + math.pi / 2, 4)
    elif name == "GNOI":        # the Monomachine's noise: the whole N of tiles, split down the middle (its stereo)
        cells = [list(r) for r in ["X..X", "XX.X", "X.XX", "X..X"]]
        if a:                   # moving: noise, one tile of the N gone at a time, a new one each frame (the N kept)
            on = [(j, i) for j in range(4) for i in range(4) if cells[j][i] == "X"]
            j, i = on[(fr * 7) % len(on)]
            cells[j][i] = "."
        for j, row in enumerate(cells):
            for i, c in enumerate(row):
                x0, y0 = 1.25 + i * 2.5, 1.25 + j * 2.5
                if c == "X":
                    R(x0, y0, x0 + 2, y0 + 2)
        SLIT(6.0, 6.0, math.pi / 2, 10.5)
    elif name == "GPRC":        # MONO2K's noise percussion: tiles forming an N, one gone, some halved
        cells = ["X..X", "Xb.X", "..cX", "X.Xa"]
        if a:                   # moving: each halved tile held on a new corner every other frame (S&H)
            turn = {"a": "bcda", "b": "cdab", "c": "dabc", "d": "abcd"}
            cells = ["".join(turn[c][(fr // 2) % 4] if c in "abcd" else c for c in r) for r in cells]
        for j, row in enumerate(cells):
            for i, c in enumerate(row):
                x0, y0 = 1.25 + i * 2.5, 1.25 + j * 2.5
                if c == "X":
                    R(x0, y0, x0 + 2, y0 + 2)
                elif c == "a":                                      # the upper left half
                    P([(x0, y0), (x0 + 2, y0), (x0, y0 + 2)])
                elif c == "b":                                      # the upper right
                    P([(x0, y0), (x0 + 2, y0), (x0 + 2, y0 + 2)])
                elif c == "c":                                      # the lower left
                    P([(x0, y0), (x0, y0 + 2), (x0 + 2, y0 + 2)])
                elif c == "d":                                      # the lower right
                    P([(x0 + 2, y0), (x0 + 2, y0 + 2), (x0, y0 + 2)])
    elif name == "SID":         # the chip: a body, its notch, three pins a side
        R(3.75, 1.5, 8.25, 10.5)
        DISC(6, 1.5, 1.0, 0)
        lit = (fr // 2) % 6 if a else -1                         # moving: the pins lit in turn, round the chip
        for k in range(3):
            y = 2.75 + k * 2.5
            R(0.5 if lit == k else 1.25, y, 3.25, y + 1.25)
            R(8.75, y, 11.5 if lit == 5 - k else 10.75, y + 1.25)
    elif name == "FMS":         # static FM: a carrier, two modulators held round it
        DISC(6.8, 6.8, 2.9)
        s1, s2 = (22 * math.sin(w2), -16 * math.sin(w2)) if a else (0, 0)   # moving: the two held modulators swing
        ARC(6.8, 6.8, 4.9 + 0.0, 3.4, 180 + s1, 270 + s1)
        ARC(6.8, 6.8, 6.4, 5.4, 190 + s2, 260 + s2)
    elif name == "FMD":         # dynamic FM: the carrier, and its modulators streaking in
        for k, (y, x0) in enumerate([(3.4, 1.0), (5.4, 2.2), (7.4, 1.6)]):
            if a:               # moving: each streak runs in from the left and starts again, out of step
                x0 = 1.0 + 3.2 * ((ph * 2 + k * 0.37) % 1.0)
            R(x0, y, 8, y + 1.2)
        DISC(8, 6, 3.6, 0)
        DISC(8, 6, 3.1)
    elif name == "FMP":         # parallel FM: the carrier, three modulators standing side by side over it
        DISC(6, 7.9, 3.1)
        for k, x in enumerate((1.9, 5.3, 8.7)):
            top = 0.6 + (1.6 * (1 - abs(math.sin(w2 + k * math.pi / 3))) if a else 0)   # moving: in turn
            R(x, top, x + 1.4, 4.0)
    elif name == "SAW":         # the supersaw: three teeth, detuned, each in front of the last and cut out of it
        off, T, B = 2.75, 2.0, 10.0
        w = 10 - 2 * off
        for k in range(3):
            if tooth is not None and k != tooth:
                continue
            Tk = T
            if tooth is not None:   # one tooth at a level (the screen composes them, each as tall as the music)
                Tk = 6.2 - 5.6 * level / 7
            elif a:                 # moving: the peaks rippling, each a sixth of a cycle behind the one before
                Tk = 6.2 - 5.6 * (0.5 + 0.5 * math.sin(w2 - k * math.pi / 3))
            pts = [(1 + k * off, B), (1 + k * off + w, Tk), (1 + k * off + w, B)]
            if halo:
                P(pts)
                d.line([(x * U, y * U) for x, y in pts + [pts[0]]], fill=W, width=int(2 * SL * U), joint="curve")
                continue
            P(pts, 0)
            d.line([(x * U, y * U) for x, y in pts + [pts[0]]], fill=0, width=int(2 * SL * U), joint="curve")
            P(pts)
        R(0, B, 12, 12, 0)                            # the halo's foot, off the baseline
    elif name == "PUL":         # the pulse: a train on its low line, each pulse narrower than the last (its width swept)
        R(0.5, 8.6, 11.5, 10.2)
        x = 0.5
        for k, w in enumerate((3.4, 2.4, 1.4)):
            w0 = w
            if a:               # moving: the widths swept, each a third of the way on
                w = w * (1 + 0.18 * math.sin(w2 + k * 2 * math.pi / 3))
            R(x, 1.8, x + w, 10.2)
            x += w + 2.2 - (3.4 - w0) * 0.3
    elif name == "FXC":         # the chorus: one wave and its copy, bent apart
        for k, dy in enumerate((-1.3, 1.3)):
            sp = (0.9 + 0.5 * math.sin(w2)) if a else 0.9        # moving: the copy's offset swept, both travelling
            pts = [(1 + 10 * q / 40, 6 + dy * (1 if k else 1) + 3.2 * math.sin(2 * math.pi * q / 40 + k * sp - w2)) for q in range(41)]
            STROKE(pts, 1.5)
    elif name == "FXR":         # the ring: a ring, a wave through it
        ARC(6, 6, 5.2, 3.6, 0, 360)
        STROKE([(1 + 10 * q / 40, 6 + 2.0 * math.sin(2 * math.pi * 2 * q / 40 - w2)) for q in range(41)], 1.2, 0)   # moving: it runs
        STROKE([(1 + 10 * q / 40, 6 + 2.0 * math.sin(2 * math.pi * 2 * q / 40 - w2)) for q in range(41)], 0.8)
    elif name == "FXD":         # the dynamics: a level, squeezed flat over its line
        R(1, 8.5, 11, 10)
        for k, h in enumerate((5.5, 7.5, 6.5, 6.5, 6.5)):
            if a:               # moving: the level pumping into the ceiling, bar after bar
                h = 5.0 + 2.2 * (0.5 + 0.5 * math.sin(w2 - k * 2 * math.pi / 5))
            R(1.2 + k * 2.0, 10 - min(h, 6.5) - 1.5, 2.6 + k * 2.0, 8.5)
        R(0.5, 2.0, 11.5, 2.8)
    elif name == "ENS":         # the ensemble: one tone, three voices swirled apart
        cx, cy, r = 6.0, 6.0, 4.9
        DISC(cx, cy, r)
        for k in range(3):
            a0 = math.radians(-90 + 120 * k + (120 * ph if a else 0))   # moving: it turns, a third a loop
            pts = [(cx + math.cos(a0 + 1.4 * (q / 40 * (r + 0.4) / r) ** 1.3) * q / 40 * (r + 0.4),
                    cy + math.sin(a0 + 1.4 * (q / 40 * (r + 0.4) / r) ** 1.3) * q / 40 * (r + 0.4)) for q in range(41)]
            STROKE(pts, SL, 0)
    elif name == "WAV":         # the wavetable: its frames stacked into a D, thick to thin
        st = Image.new("L", im.size, 0)
        sd = ImageDraw.Draw(st)
        y = 1.0
        H6 = [2.0, 1.6, 1.3, 1.05, 0.85, 0.7]
        if a:                   # moving: the frames scanned, each height passing to the one under it
            q = ph * 6
            i0, t6 = int(q) % 6, q - int(q)
            H6 = [H6[(i - i0) % 6] + (H6[(i - i0 - 1) % 6] - H6[(i - i0) % 6]) * t6 for i in range(6)]
        for h in H6:                                                # six frames and five slits: ten units
            sd.rectangle([0, y * U, G * SS, (y + h) * U - 1], fill=255)
            y += h + SL
        dm = Image.new("L", im.size, 0)
        dd = ImageDraw.Draw(dm)
        dd.rectangle([1.5 * U, 1 * U, 5.5 * U, 11 * U - 1], fill=255)  # the D: a flat back, a half disc
        dd.pieslice([0.5 * U, 1 * U, 10.5 * U - 1, 11 * U - 1], -90, 90, fill=255)
        im.paste(255, mask=ImageChops.multiply(st, dm))
    elif name == "VOX":         # the voice: a mouth, its lips parted by the slit
        bow = lambda t: 1.0 * math.exp(-((t - 0.5) / 0.1) ** 2)    # the dip of the upper lip
        n = 60
        op = 0.55 * (1 - math.cos(w2)) if a else 0.0             # moving: it talks, the lips apart and together
        top = [(1.2 + 9.6 * k / n, 5.75 - op - 3.0 * math.sin(math.pi * k / n) ** 0.75 + bow(k / n)) for k in range(n + 1)]
        mid = [(1.2 + 9.6 * k / n, 5.75 - op + 0.5 * math.sin(math.pi * k / n)) for k in range(n + 1)]
        bot = [(1.2 + 9.6 * k / n, 6.25 + op + 3.2 * math.sin(math.pi * k / n) ** 0.9) for k in range(n + 1)]
        P(top + mid[::-1])                            # the upper lip
        P([(x, y + SL + 2 * op * math.sin(math.pi * (x - 1.2) / 9.6)) for x, y in mid] + bot[::-1])  # the lower lip
    elif name == "DKIK":        # the drum voice: a head struck, its ring around it; the slit across
        k = ph % 1.0 if a else 0.35
        r = 2.3 + 1.3 * (1 - k) ** 2                  # moving: each loop a hit, the head thumping down (its pitch drop)
        ARC(6, 6, 5.0, 4.0, 0, 360)
        DISC(6, 6, r)
        SLIT(6, 6, 0, 2 * r + 0.2)
    elif name == "DSNR":        # the snare: a head over its wires
        ARC(6, 6.6, 5.0, 0, 180, 360)
        for j in range(4):
            x0 = 1.0 + j * 2.6
            up = 0.6 if a and (fr + j * 3) % 4 == 0 else 0.0     # moving: the wires buzz, one lifted in turn
            R(x0, 8.0 - up, x0 + 2.0, 10.0 - up)
    elif name == "DHAT":        # the hats: two plates, opening and closing
        op = 1.3 * abs(math.sin(w2)) if a else 0.6
        P([(1.0, 5.2 - op), (11.0, 5.2 - op), (9.0, 3.4 - op), (3.0, 3.4 - op)])     # the top plate, lifted
        P([(1.0, 6.8), (11.0, 6.8), (9.0, 8.6), (3.0, 8.6)])                          # the bottom one
        R(5.0, 9.1, 7.0, 11.0)                                                         # the stand
    elif name == "DCYM":        # the cymbal: one plate and its bell, the shimmer over it
        P([(1.0, 8.6), (11.0, 8.6), (7.4, 6.4), (4.6, 6.4)])
        DISC(6, 6.3, 1.4)
        for k, x in enumerate((2.0, 5.3, 8.6)):
            top = 1.0 + (1.8 * (1 - abs(math.sin(w2 + k * math.pi / 3))) if a else 0.6 * k % 1.2)   # moving: in turn
            R(x, top, x + 1.4, 4.0)
        R(5.0, 9.1, 7.0, 11.0)
    return im                   # G * SS square, 8 bit


NAMES = ["GSIN", "GPRC", "SID", "FMS", "FMD", "SAW", "ENS", "WAV", "VOX", "FMP", "PUL", "FXC", "FXR", "FXD", "GNOI",
         "DKIK", "DSNR", "DHAT", "DCYM"]


GS = 256        # the distance field's grid: finer than the mark, for clean normals


def sdf2d(m):
    """signed distance to the mark's edge, in grid pixels (negative inside)"""
    G = m.shape[0]
    edge = m & ~(np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1))
    edge[0, :] |= m[0, :]; edge[-1, :] |= m[-1, :]; edge[:, 0] |= m[:, 0]; edge[:, -1] |= m[:, -1]
    ey, ex = np.nonzero(edge)
    ys, xs = np.mgrid[0:G, 0:G]
    pts = np.stack([xs.ravel(), ys.ravel()], 1).astype(np.float32)
    e = np.stack([ex, ey], 1).astype(np.float32)
    best = np.full(len(pts), 1e9, np.float32)
    for i in range(0, len(e), 256):
        d = ((pts[:, None, :] - e[None, i:i + 256, :]) ** 2).sum(-1)
        best = np.minimum(best, d.min(1))
    dist = np.sqrt(best).reshape(G, G)
    return np.where(m, -dist, dist)


def make_sdf(m):
    G = m.shape[0]
    d2 = sdf2d(m) / (G / 2)          # in object units: the mark spans -1..1
    def f(p):
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        gx = (x + 1) * (G / 2) - 0.5
        gy = (1 - y) * (G / 2) - 0.5
        cx, cy = np.clip(gx, 0, G - 1.001), np.clip(gy, 0, G - 1.001)
        x0, y0 = cx.astype(int), cy.astype(int)
        fx, fy = cx - x0, cy - y0
        v = (d2[y0, x0] * (1 - fx) * (1 - fy) + d2[y0, x0 + 1] * fx * (1 - fy) +
             d2[y0 + 1, x0] * (1 - fx) * fy + d2[y0 + 1, x0 + 1] * fx * fy)
        out = np.sqrt(np.maximum(np.abs(gx - cx), np.abs(gy - cy)) ** 2) / (G / 2)
        v = v + out
        dz = np.abs(z) - 0.15
        q0, q1 = np.maximum(v + 0.05, 0), np.maximum(dz, 0)
        return np.sqrt(q0 ** 2 + q1 ** 2) + np.minimum(np.maximum(v + 0.05, dz), 0) - 0.05
    return f


def length(v):
    return np.sqrt((v * v).sum(-1))


def rot(p, ax, ay):
    cy, sy = math.cos(ay), math.sin(ay)
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    x, z = cy * x + sy * z, -sy * x + cy * z
    cx, sx = math.cos(ax), math.sin(ax)
    y, z = cx * y - sx * z, sx * y + cx * z
    return np.stack([x, y, z], -1)


# screen-fixed 1-bit patterns, dark (6) to light (2); hit pixels never go lighter than dots
def pattern(level, x, y):
    if level >= 6:
        return np.ones_like(x, bool)
    if level == 5:
        return ~((x % 2 == 0) & (y % 2 == 0))
    if level == 4:
        return (x + y) % 2 == 0
    if level == 3:
        return y % 2 == 0
    return (x % 2 == 0) & (y % 2 == 0)


def render(sdf, ay, ax, full=False):
    ys, xs = np.mgrid[0:N, 0:N]
    u = (xs + 0.5) / N * 2.1 - 1.05
    v = -((ys + 0.5) / N * 2.1 - 1.05)
    ro = np.stack([u, v, np.full_like(u, -3.0)], -1)
    rd = np.array([0, 0, 1.0])
    t = np.zeros(u.shape)
    hit = np.zeros(u.shape, bool)
    f = lambda q: sdf(rot(q, ax, ay))
    for _ in range(140):
        p = ro + rd * t[..., None]
        d = f(p)
        hit |= (d < 0.003) & (t < 6)
        t = np.where(hit, t, t + d * 0.6)
    p = ro + rd * t[..., None]
    e = 0.01
    n = np.stack([f(p + [e, 0, 0]) - f(p - [e, 0, 0]), f(p + [0, e, 0]) - f(p - [0, e, 0]),
                  f(p + [0, 0, e]) - f(p - [0, 0, e])], -1)
    n /= np.maximum(length(n)[..., None], 1e-9)
    L = np.array([-0.5, 0.6, -0.62])
    L /= np.linalg.norm(L)
    dif = np.clip((n * L).sum(-1), 0, 1)
    # few textures: the face solid (the flat mark), the sides dithered by their light
    po = rot(p, ax, ay)                               # the hit, in the object's own space
    no = np.stack([sdf(po + [e, 0, 0]) - sdf(po - [e, 0, 0]), sdf(po + [0, e, 0]) - sdf(po - [0, e, 0]),
                   sdf(po + [0, 0, e]) - sdf(po - [0, 0, e])], -1)
    no /= np.maximum(length(no)[..., None], 1e-9)
    front = np.abs(no[..., 2]) > 0.9                  # the letter's face, whatever the angle
    level = np.where(dif > 0.5, 3, 4)                 # the sides, dithered: lines in the light, checks in shadow
    level = np.where(front, 6, level).astype(int)       # the face: solid, the flat mark itself
    ink = np.zeros(u.shape, bool)
    for l in range(2, 7):
        ink |= (level == l) & pattern(l, xs, ys)
    return hit if full else ink & hit


def pack(ink):
    h, w = ink.shape
    out = []
    for y in range(h):
        for bx in range(w // 8):
            b = 0
            for i in range(8):
                if ink[y, bx * 8 + i]:
                    b |= 0x80 >> i
            out.append(b)
    return out


def move_out(frames3d, teeth, masks, sheet):
    """the Move's header: every frame, tooth and footprint cropped to the one box that holds the ink of all the frames
    and teeth (its place in the N square kept, so the screen knows where the marks' middle is); rows of whole bytes"""
    seen = np.zeros((N, N), bool)
    for ink in [i for fr in frames3d for i in fr] + [i for tk in teeth for i in tk]:
        seen |= ink
    ys, xs = np.nonzero(seen)
    y0, x0 = int(ys.min()), int(xs.min())
    h, w = int(ys.max()) + 1 - y0, int(xs.max()) + 1 - x0
    rb = (w + 7) // 8

    def crop(ink):
        c = np.zeros((h, rb * 8), bool)
        c[:, :w] = ink[y0:y0 + h, x0:x0 + w]
        return c

    def rows(ink):
        return "{" + ",".join("0x%02x" % x for x in pack(crop(ink))) + "}"

    lines = ["/* generated by tools/gen_icons.py --move: do not edit */\n",
             "/* MONO2K's machine marks for the Move's screen (move/screen/m2k_screen.c): the FM-1's marks and motions drawn\n"
             " * in a %d px square, cropped to the ink of every frame: MK_W x MK_H from (MK_X0, MK_Y0) of the square, rows of\n"
             " * MK_RB bytes, MSB first, 1 = lit. MK_BITS[machine][frame]; SWAV-SAW's teeth and their footprints by level */\n" % N,
             "#define MK_N %d\n#define MK_X0 %d\n#define MK_Y0 %d\n#define MK_W %d\n#define MK_H %d\n#define MK_RB %d\n"
             "#define MK_FRAMES %d\n#define MK_COUNT %d\n" % (N, x0, y0, w, h, rb, FRAMES, len(NAMES)),
             "static const uint8_t MK_BITS[%d][%d][%d] = {\n" % (len(NAMES), FRAMES, h * rb)]
    for fr in frames3d:
        lines.append("  {\n" + "".join("    %s,\n" % rows(ink) for ink in fr) + "  },\n")
    lines.append("};\n")
    for nm, arr in (("MK_TEETH", teeth), ("MK_MASK", masks)):
        lines.append("static const uint8_t %s[3][8][%d] = {\n" % (nm, h * rb))
        for k in range(3):
            lines.append("  {\n" + "".join("    %s,\n" % rows(ink) for ink in arr[k]) + "  },\n")
        lines.append("};\n")
    with open(OUT, "w") as f:
        f.write("".join(lines))
    print("wrote", os.path.normpath(OUT), "%d x %d at (%d, %d) of %d" % (w, h, x0, y0, N))
    if sheet:                   # lit on the OLED's black: a row a machine, its 12 frames; SWAV-SAW's teeth last
        P = 4
        rws = [[crop(i) for i in fr] for fr in frames3d] + [[crop(teeth[k][lv]) for k in range(3) for lv in (0, 3, 7)]]
        img = np.full((len(rws) * (h + P) + P, FRAMES * (rb * 8 + P) + P), 24, np.uint8)
        for r, row in enumerate(rws):
            for k, ink in enumerate(row):
                y, x = P + r * (h + P), P + k * (rb * 8 + P)
                img[y:y + h, x:x + rb * 8] = np.where(ink, 236, 0)
        Image.fromarray(img).resize((img.shape[1] * 4, img.shape[0] * 4), Image.NEAREST).save(sheet)
        print("sheet", sheet)


def main():
    sheet = sys.argv[sys.argv.index("--sheet") + 1] if "--sheet" in sys.argv else None
    lines = ["/* generated by tools/gen_icons.py: do not edit */\n",
             "#define ICON_N %d\n#define ICON_FRAMES %d\n#define ICON_COUNT %d\n" % (N, FRAMES, len(NAMES))]
    frames3d, flats = [], []
    for name in NAMES:
        big = mark(name)
        flat = np.array(big.resize((N, N), Image.BOX)) > 127
        flats.append(flat)
        # each frame its own mark, moving as its machine does, swaying less than it used to so the motion reads
        fr = []
        for k in range(FRAMES):
            sdf = make_sdf(np.array(mark(name, k / FRAMES).resize((GS, GS), Image.BOX)) > 127)
            fr.append(render(sdf, 0.32 * math.sin(2 * math.pi * k / FRAMES), 0.22 + 0.06 * math.cos(2 * math.pi * k / FRAMES)))
        frames3d.append(fr)
        print(name)
    lines.append("static const uint8_t ICON_BITS[%d][%d][%d] = {\n" % (len(NAMES), FRAMES, N * N // 8))
    for fr in frames3d:
        lines.append("  {\n")
        for ink in fr:
            lines.append("    {" + ",".join("0x%02x" % x for x in pack(ink)) + "},\n")
        lines.append("  },\n")
    lines.append("};\n")
    # SWAV-SAW's teeth one by one at 8 levels, and each one's footprint, from the first frame's view: the screen stacks
    # them back to front, each as tall as the music
    teeth, masks = [], []
    for k in range(3):
        teeth.append([]); masks.append([])
        for lv in range(8):
            sd = make_sdf(np.array(mark("SAW", 0.0, tooth=k, level=lv).resize((GS, GS), Image.BOX)) > 127)
            teeth[k].append(render(sd, 0.0, 0.28))
            sh = make_sdf(np.array(mark("SAW", 0.0, tooth=k, level=lv, halo=True).resize((GS, GS), Image.BOX)) > 127)
            masks[k].append(render(sh, 0.0, 0.28, full=True))
    if MOVE:
        move_out(frames3d, teeth, masks, sheet)
        return
    for nm, arr in (("SAW_TEETH", teeth), ("SAW_MASK", masks)):
        lines.append("static const uint8_t %s[3][8][%d] = {\n" % (nm, N * N // 8))
        for k in range(3):
            lines.append("  {\n" + "".join("    {" + ",".join("0x%02x" % x for x in pack(ink)) + "},\n" for ink in arr[k]) + "  },\n")
        lines.append("};\n")
    lines.append("static const uint8_t MARK_BITS[%d][%d] = {\n" % (len(NAMES), N * N // 8))
    for fl in flats:
        lines.append("    {" + ",".join("0x%02x" % x for x in pack(fl)) + "},\n")
    lines.append("};\n")
    with open(OUT, "w") as f:
        f.write("".join(lines))
    print("wrote", os.path.normpath(OUT))
    if sheet:
        cols = FRAMES + 1
        img = np.ones((len(NAMES) * (N + 6), cols * (N + 6)), np.uint8) * 255
        for i in range(len(NAMES)):
            y = i * (N + 6)
            img[y:y + N, 0:N] = np.where(flats[i], 0, 255)
            for k, ink in enumerate(frames3d[i]):
                x = (k + 1) * (N + 6)
                img[y:y + N, x:x + N] = np.where(ink, 0, 255)
        Image.fromarray(img).resize((img.shape[1] * 2, img.shape[0] * 2), Image.NEAREST).save(sheet)
        print("sheet", sheet)


main()
