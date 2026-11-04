"""Regenerate original procedural M0 image fixtures (requires Pillow).

No source imagery, remote services or production art are used. Not a build step.
"""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1] / "assets/probe/images"
root.mkdir(parents=True, exist_ok=True)
image = Image.new("RGB", (900, 600), "#252938")
draw = ImageDraw.Draw(image)
for y in range(600):
    draw.line((0, y, 900, y), fill=(35+y//32, 40+y//36, 56+y//30))
# Quiet geometric room: window, distant city, desk, lamp and plant.
draw.rounded_rectangle((75, 60, 540, 370), radius=12, fill="#687b99")
draw.rectangle((90, 75, 525, 355), fill="#c7b7a7")
for x, height in ((95, 92), (165, 130), (240, 66), (315, 115), (405, 154), (475, 80)):
    draw.rectangle((x, 355-height, x+49, 355), fill="#7d8596")
draw.line((305, 75, 305, 355), fill="#252e42", width=12)
draw.line((90, 218, 525, 218), fill="#252e42", width=10)
draw.polygon(((80, 476), (645, 476), (810, 600), (0, 600)), fill="#d0a981")
draw.line((20, 500, 680, 500), fill="#e6bf96", width=5)
draw.rounded_rectangle((188, 371, 420, 481), radius=9, fill="#141c2b")
draw.rounded_rectangle((200, 383, 408, 463), radius=4, fill="#526c81")
for n in range(5):
    draw.line((220, 405+n*10, 300+n*12, 405+n*10), fill="#9ec5c1", width=3)
draw.polygon(((178, 482), (432, 482), (463, 510), (148, 510)), fill="#323f52")
draw.ellipse((580, 449, 687, 469), fill="#1a2435")
draw.line((632, 453, 632, 320, 586, 275), fill="#172133", width=9)
draw.polygon(((555, 245), (617, 266), (588, 305), (535, 283)), fill="#e0b16b")
draw.rounded_rectangle((735, 408, 810, 500), radius=9, fill="#ab7c67")
for points in (((770, 421), (708, 310), (759, 326)), ((774, 418), (822, 290), (789, 327)), ((771, 409), (758, 280), (783, 320))):
    draw.polygon(points, fill="#6f9b87")
image.save(root / "office.png", optimize=True)
# A second codec exercises the JPEG plugin; a simple paper grain swatch.
paper = Image.new("RGB", (160, 100), "#d8c5aa")
grain = paper.load()
for y in range(100):
    for x in range(160):
        shade = ((x*17+y*31) % 9)-4
        grain[x, y] = (216+shade, 197+shade, 170+shade)
paper.save(root / "paper.jpg", quality=91, subsampling=0)
