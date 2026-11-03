# Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Low | Reviewed by: None
#
# Draws the PWA icons (the mushroom of icon.svg) as PNG: Android needs 192 and 512 px
# for "Install app". Needs Pillow (KiCad's Python has it):
#   "C:\Program Files\KiCad\10.0\bin\python.exe" web\make_icons.py

import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
BLUE = (42, 120, 214, 255)
WHITE = (255, 255, 255, 255)


def icon(size, maskable):
    s = size / 64.0
    img = Image.new("RGBA", (size, size), BLUE if maskable else (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    if not maskable:
        d.rounded_rectangle([0, 0, size - 1, size - 1], radius=int(14 * s), fill=BLUE)
    # Maskable icons keep the drawing inside the central 80 % safe zone.
    k = 0.8 if maskable else 1.0
    o = size * (1 - k) / 2
    t = lambda v: o + v * s * k
    d.pieslice([t(12), t(14), t(52), t(54)], 180, 360, fill=WHITE)              # cap
    d.rounded_rectangle([t(26), t(34), t(38), t(50)], radius=int(5 * s * k), fill=WHITE)  # stem
    return img


for size in (192, 512):
    icon(size, False).save(os.path.join(HERE, "icon-%d.png" % size))
icon(512, True).save(os.path.join(HERE, "icon-maskable-512.png"))
print("icons written")
