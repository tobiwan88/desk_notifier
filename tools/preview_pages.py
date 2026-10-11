"""Preview the screen pages on your computer.

Builds nothing itself: run the host build first (scripts/check.sh --quick or cmake), then

    uv run python tools/preview_pages.py [out_dir] [scene ...]

It runs build/host/page_preview, writes one round, animated GIF per scene and a contact sheet
(`sheet.png`) with the first frame of every scene.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 240
FRAME_MS = 100
ROOT = Path(__file__).resolve().parent.parent


def round_mask(scale: int = 1) -> Image.Image:
    m = Image.new("L", (SIZE * scale, SIZE * scale), 0)
    ImageDraw.Draw(m).ellipse((0, 0, SIZE * scale - 1, SIZE * scale - 1), fill=255)
    return m


def load_frames(path: Path) -> list[Image.Image]:
    raw = path.read_bytes()
    n = len(raw) // (SIZE * SIZE * 3)
    step = SIZE * SIZE * 3
    return [Image.frombytes("RGB", (SIZE, SIZE), raw[i * step : (i + 1) * step]) for i in range(n)]


def on_bezel(frame: Image.Image, mask: Image.Image) -> Image.Image:
    out = Image.new("RGB", (SIZE + 20, SIZE + 20), (24, 24, 28))
    out.paste(frame, (10, 10), mask)
    return out


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "build" / "preview"
    out.mkdir(parents=True, exist_ok=True)
    exe = ROOT / "build" / "host" / "page_preview"
    subprocess.run([str(exe), str(out), *argv[1:]], check=True)
    mask = round_mask()
    firsts: list[tuple[str, Image.Image]] = []
    for raw in sorted(out.glob("*.rgb")):
        frames = [on_bezel(f, mask) for f in load_frames(raw)]
        if not frames:
            continue
        frames[0].save(raw.with_suffix(".gif"), save_all=True, append_images=frames[1:], duration=FRAME_MS, loop=0)
        firsts.append((raw.stem, frames[min(len(frames) - 1, 5)]))
        raw.unlink()
    if firsts:
        cols = 4
        rows = (len(firsts) + cols - 1) // cols
        w = SIZE + 20
        sheet = Image.new("RGB", (cols * w, rows * (w + 18)), (24, 24, 28))
        d = ImageDraw.Draw(sheet)
        for i, (name, img) in enumerate(firsts):
            x, y = (i % cols) * w, (i // cols) * (w + 18)
            sheet.paste(img, (x, y))
            d.text((x + 10, y + w + 2), name, fill=(220, 220, 220))
        sheet.save(out / "sheet.png")
    print(f"wrote {len(firsts)} previews to {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
