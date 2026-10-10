"""Turn preview.c output into a PNG contact sheet: python3 host/sheet.py out.rgb sheet.png"""
import sys
from PIL import Image, ImageDraw

STYLES = ["realistic", "flat", "line art", "anime", "cute", "halloween", "christmas"]
SNAPS = ["idle", "wink", "slow blink", "roll", "side-eye", "hunt", "3rd lid", "sleepy",
         "cross", "happy", "special"]
SCALE = 0.5
src, dst = sys.argv[1], sys.argv[2]
data = open(src, "rb").read()
cell = int(240 * SCALE)
gap, top, left = 6, 22, 80
W = left + len(SNAPS) * (2 * cell + gap)
H = top + len(STYLES) * (cell + gap)
sheet = Image.new("RGB", (W, H), (245, 245, 240))
d = ImageDraw.Draw(sheet)
mask = Image.new("L", (240, 240), 0)
ImageDraw.Draw(mask).ellipse((0, 0, 239, 239), fill=255)
frame = 240 * 240 * 3
i = 0
for s, sname in enumerate(STYLES):
    d.text((6, top + s * (cell + gap) + cell // 2 - 6), sname, fill=(40, 40, 40))
    for k, kname in enumerate(SNAPS):
        if s == 0:
            d.text((left + k * (2 * cell + gap) + 4, 6), kname, fill=(40, 40, 40))
        for e in range(2):
            img = Image.frombytes("RGB", (240, 240), data[i * frame:(i + 1) * frame])
            i += 1
            disc = Image.new("RGB", (240, 240), (20, 20, 20))
            disc.paste(img, (0, 0), mask)
            disc = disc.resize((cell, cell), Image.LANCZOS)
            sheet.paste(disc, (left + k * (2 * cell + gap) + e * cell, top + s * (cell + gap)))
sheet.save(dst)
print(dst, sheet.size)
