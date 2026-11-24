#!/usr/bin/env python3
# Draws the app icon (clay circle, white chat bubble, sparkle) and writes
# icons/<size>/harbour-claudechat.png for all Sailfish sizes.
# Usage: python3 tools/make-icon.py   (needs Pillow)
import math
import os

from PIL import Image, ImageDraw

S = 1024                      # drawing size, scaled down per icon size
SIZES = (86, 108, 128, 172)
TOP = (222, 124, 90)          # clay, light
BOTTOM = (176, 82, 54)        # clay, dark
SPARK = (201, 100, 66)

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

# Round background with a vertical gradient.
grad = Image.new("RGBA", (S, S))
gd = ImageDraw.Draw(grad)
for y in range(S):
    t = y / (S - 1)
    gd.line([(0, y), (S, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(TOP, BOTTOM)) + (255,))
mask = Image.new("L", (S, S), 0)
m = S * 0.02
ImageDraw.Draw(mask).ellipse([m, m, S - m, S - m], fill=255)
img.paste(grad, (0, 0), mask)

d = ImageDraw.Draw(img)

# Chat bubble with a tail at the bottom left.
bx0, by0, bx1, by1 = S * 0.20, S * 0.24, S * 0.80, S * 0.68
d.rounded_rectangle([bx0, by0, bx1, by1], radius=S * 0.11, fill="white")
d.polygon([(S * 0.30, by1 - S * 0.04), (S * 0.27, S * 0.80), (S * 0.44, by1 - S * 0.01)], fill="white")


def sparkle(cx, cy, r, color):
    # Four-pointed star with curved sides (astroid).
    pts = []
    for i in range(240):
        t = 2 * math.pi * i / 240
        pts.append((cx + r * math.cos(t) ** 3, cy + r * math.sin(t) ** 3))
    d.polygon(pts, fill=color)


sparkle(S * 0.47, S * 0.465, S * 0.16, SPARK)
sparkle(S * 0.64, S * 0.35, S * 0.065, SPARK)

for size in SIZES:
    out = os.path.join(root, "icons", "%dx%d" % (size, size), "harbour-claudechat.png")
    img.resize((size, size), Image.LANCZOS).save(out, optimize=True)
    print(out)
