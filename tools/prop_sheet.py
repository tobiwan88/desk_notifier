"""Contact sheet of the day-page props, to check the drawings.

    cmake --build build/host --target prop_sheet && uv run python tools/prop_sheet.py [prop ...]

Writes build/props/sheet.png: one row per prop, three frames at scene size, then three enlarged.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 240
ROOT = Path(__file__).resolve().parent.parent
FRAMES = (0, 10, 20)


def frames(path: Path) -> list[Image.Image]:
    raw = path.read_bytes()
    step = SIZE * SIZE * 3
    return [Image.frombytes("RGB", (SIZE, SIZE), raw[i * step : (i + 1) * step]) for i in range(len(raw) // step)]


def main(argv: list[str]) -> int:
    out = ROOT / "build" / "props"
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("*.rgb"):
        old.unlink()
    subprocess.run([str(ROOT / "build" / "host" / "prop_sheet"), str(out), *argv], check=True)
    names = sorted(p.stem for p in out.glob("*.rgb") if not p.stem.endswith("_big"))
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, SIZE - 1, SIZE - 1), fill=255)
    cell, label = SIZE + 10, 110
    sheet = Image.new("RGB", (label + cell * 2 * len(FRAMES), cell * len(names)), (24, 24, 28))
    d = ImageDraw.Draw(sheet)
    for r, name in enumerate(names):
        d.text((4, r * cell + cell // 2), name, fill=(255, 255, 255))
        for k, suffix in enumerate(("", "_big")):
            fs = frames(out / f"{name}{suffix}.rgb")
            for c, i in enumerate(FRAMES):
                sheet.paste(fs[i], (label + (k * len(FRAMES) + c) * cell, r * cell), mask)
    sheet.save(out / "sheet.png")
    print(f"wrote {out / 'sheet.png'} ({len(names)} props)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
