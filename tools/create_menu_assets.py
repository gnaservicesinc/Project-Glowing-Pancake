#!/usr/bin/env python3
"""Reproduce the original menu art, icon, UI sound and silent motion loop.

Development-only: Pillow and the macOS SDK/iconutil. No downloaded source art.
The game's runtime needs none of these tools.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import math
import random
import struct
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "menu"
OUT.mkdir(parents=True, exist_ok=True)
W, H = 1920, 1080
art = Image.new("RGB", (W, H))
pixels = art.load()
for y in range(H):
    for x in range(W):
        glow = math.exp(-(((x - 1410) / 580) ** 2 + ((y - 500) / 400) ** 2))
        horizon = math.exp(-((y - 700) / 110) ** 2) * (x / W) ** 2
        pixels[x, y] = (int(7 + 48 * glow + 24 * horizon),
                        int(15 + 36 * glow + 12 * horizon),
                        int(23 + 28 * glow + 8 * horizon))
layer = Image.new("RGBA", art.size)
d = ImageDraw.Draw(layer)
rng = random.Random(2014)
for _ in range(160):
    x, y = rng.randrange(650, W), rng.randrange(40, 640)
    opacity = rng.randrange(30, 115)
    d.ellipse((x, y, x + 2, y + 2), fill=(225, 222, 198, opacity))
# An abstract orbital sunrise; deliberately no office, lab or world scene.
cx, cy, radius = 1400, 410, 210
for r in range(radius, 0, -1):
    t = r / radius
    d.ellipse((cx-r, cy-r, cx+r, cy+r), fill=(int(226 + 20*(1-t)), int(137 + 54*(1-t)), 80, 255))
for y in range(cy+55, cy+radius, 11):
    half = int(math.sqrt(max(0, radius*radius-(y-cy)**2)))
    d.rectangle((cx-half-1, y, cx+half+1, y+4), fill=(28, 32, 35, 210))
d.ellipse((cx-284, cy-284, cx+284, cy+284), outline=(234, 180, 110, 42), width=2)
d.arc((cx-350, cy-350, cx+350, cy+350), 210, 335, fill=(234, 180, 110, 60), width=2)
d.line((cx-370, cy+245, cx+360, cy-230), fill=(230, 184, 125, 65), width=2)
d.ellipse((cx+332, cy-215, cx+340, cy-207), fill=(255, 212, 145, 255))
for yy in (708, 740, 792, 870, 990):
    d.line((700, yy, W, yy), fill=(207, 151, 86, 24), width=1)
for bottom_x in range(-800, W+1800, 200):
    d.line((1400, 680, bottom_x, H), fill=(207, 151, 86, 18), width=1)
art = Image.alpha_composite(art.convert("RGBA"), layer)
# Keep the title and choices readable over every frame.
shade = Image.new("RGBA", art.size)
sd = ImageDraw.Draw(shade)
for x in range(1200):
    sd.line((x, 0, x, H), fill=(4, 11, 18, int(190 * (1-x/1200)**1.5)))
art = Image.alpha_composite(art, shade).convert("RGB")
art.save(OUT / "horizon.jpg", quality=95, subsampling=0)
subprocess.run(["xcrun", "swift", str(ROOT / "tools/create_menu_video.swift"),
    str(OUT / "horizon.jpg"), str(OUT / "horizon.mp4")], check=True)
with wave.open(str(OUT / "select.wav"), "wb") as audio:
    audio.setparams((1, 2, 44100, 0, "NONE", "not compressed"))
    frames = []
    for i in range(4410):
        t = i / 44100
        envelope = math.sin(math.pi * min(1, t / .008)) if t < .004 else math.exp(-60 * t)
        sample = .18 * envelope * (math.sin(2*math.pi*660*t) + .3*math.sin(2*math.pi*990*t))
        frames.append(struct.pack("<h", int(32767 * sample)))
    audio.writeframes(b"".join(frames))

icon = Image.new("RGBA", (1024, 1024))
idraw = ImageDraw.Draw(icon)
idraw.rounded_rectangle((35, 35, 989, 989), radius=210, fill="#0b1926")
idraw.ellipse((170, 170, 854, 854), outline="#4a473d", width=4)
for y, color in ((630, "#a6693e"), (500, "#d49857"), (370, "#f6ce89")):
    idraw.rounded_rectangle((238, y-36, 786, y+66), radius=50, fill=color)
    idraw.ellipse((238, y-90, 786, y+36), fill=color)
    idraw.arc((270, y-68, 754, y+10), 188, 348, fill="#ffe5b3", width=7)
idraw.ellipse((474, 210, 550, 286), fill="#ffe7ae")
icon.save(OUT / "icon.png")
with tempfile.TemporaryDirectory() as temporary:
    iconset = Path(temporary) / "Pancake.iconset"
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        for scale, suffix in ((1, ""), (2, "@2x")):
            icon.resize((size*scale, size*scale), Image.Resampling.LANCZOS).save(iconset / f"icon_{size}x{size}{suffix}.png")
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(OUT / "Pancake.icns")], check=True)
