"""Annotated board renders for review agents: no pours, nets coloured and named, big reference labels.

Reads the JSON from board_geom.py. Writes full-board and region PNGs with a mm grid, so a
reviewer can read distances and follow nets. Colours: F.Cu tracks red, B.Cu blue, inner
green; pads by net (GND grey, +3V3 orange, VBUS purple, others teal); vias black rings.
Usage: render_review.py geom.json out_dir [region=x0,y0,x1,y1 ...]
"""

import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

LAYER = {"F.Cu": (215, 40, 40), "B.Cu": (40, 90, 220), "In1.Cu": (40, 160, 60), "In2.Cu": (230, 150, 0)}
NET = {"GND": (120, 120, 120), "+3V3": (240, 150, 30), "VBUS": (150, 60, 190)}


def font(px):
    for name in ("/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            pass
    return ImageFont.load_default()


def render(geom, out: Path, region=None, scale=40, labels=True):
    x0, y0, x1, y1 = region or (0, 0, *geom["size"])
    x0, y0, x1, y1 = x0 - 1, y0 - 1, x1 + 1, y1 + 1
    img = Image.new("RGB", (int((x1 - x0) * scale), int((y1 - y0) * scale)), "white")
    d = ImageDraw.Draw(img)
    P = lambda p: ((p[0] - x0) * scale, (p[1] - y0) * scale)  # noqa: E731

    for gx in range(int(x0), int(x1) + 1):  # 1 mm grid, labelled every 5 mm
        d.line([P((gx, y0)), P((gx, y1))], fill=(235, 235, 235) if gx % 5 else (205, 205, 205))
        if gx % 5 == 0:
            d.text(P((gx + 0.1, y0 + 0.1)), str(gx), fill=(150, 150, 150), font=font(int(scale * 0.35)))
    for gy in range(int(y0), int(y1) + 1):
        d.line([P((x0, gy)), P((x1, gy))], fill=(235, 235, 235) if gy % 5 else (205, 205, 205))
        if gy % 5 == 0:
            d.text(P((x0 + 0.1, gy + 0.1)), str(gy), fill=(150, 150, 150), font=font(int(scale * 0.35)))
    for a, b in geom["edges"]:
        d.line([P(a), P(b)], fill=(0, 0, 0), width=max(2, scale // 12))
    for fp in geom["footprints"]:
        if fp["box"]:
            (a, b) = fp["box"]
            d.rectangle([P(a), P(b)], outline=(190, 190, 255))
    for p in geom["pads"]:
        (a, b) = p["box"]
        col = NET.get(p["net"], (0, 150, 150) if p["net"] else (210, 210, 210))
        d.rectangle([P(a), P(b)], fill=col + (0,), outline=(0, 0, 0))
    for layer in ("B.Cu", "In2.Cu", "In1.Cu", "F.Cu"):
        for t in geom["tracks"]:
            if t["layer"] == layer:
                d.line([P(t["a"]), P(t["b"])], fill=LAYER[layer], width=max(1, int(t["w"] * scale)))
    for v in geom["vias"]:
        cx, cy = P(v["pos"])
        r = v["d"] * scale / 2
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255), outline=(0, 0, 0), width=max(1, scale // 15))
    if labels:
        small = font(max(9, int(scale * 0.22)))
        for p in geom["pads"]:
            if p["net"] and scale >= 60:
                (a, b) = p["box"]
                c = P(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2))
                d.text(c, p["net"].lstrip("/").replace("unconnected-", "nc"), fill=(0, 0, 0), font=small, anchor="mm")
        big = font(max(12, int(scale * 0.5)))
        for fp in geom["footprints"]:
            d.text(P(fp["pos"]), fp["ref"], fill=(0, 0, 160), font=big, anchor="mm", stroke_width=2, stroke_fill="white")
    img.save(out)


def main():
    geom = json.loads(Path(sys.argv[1]).read_text())
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    render(geom, out / "annot_board.png", scale=30)
    for arg in sys.argv[3:]:
        name, box = arg.split("=")
        render(geom, out / f"annot_{name}.png", tuple(float(v) for v in box.split(",")), scale=80)


if __name__ == "__main__":
    main()
