from __future__ import annotations

import argparse
from pathlib import Path

from .budget import Budget
from .pipeline import prepare
from .preview import preview_gif
from .writer import write_clip


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="clipprep", description="Prepare a GIF/PNG sequence/video as an eyes .clip")
    ap.add_argument("src", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--fps", type=float)
    ap.add_argument("--max-frames", type=int)
    ap.add_argument("--region", help="x,y,w,h on the 240x240 screen (default: full screen)")
    ap.add_argument("--fit", choices=["cover", "contain"], default="cover")
    ap.add_argument("--bg", default="#000000")
    ap.add_argument("--max-bytes", type=int, default=Budget.max_bytes)
    ap.add_argument("--max-bps", type=int, default=Budget.max_bps)
    ap.add_argument("--loop", action="store_true")
    ap.add_argument("--preview", type=Path, help="write a preview GIF")
    a = ap.parse_args(argv)
    region = tuple(int(v) for v in a.region.split(",")) if a.region else "full"
    p = prepare(a.src, eye_region=region, fps=a.fps, max_frames=a.max_frames, fit=a.fit, bg=a.bg, loop=a.loop,
                budget=Budget(max_bytes=a.max_bytes, max_bps=a.max_bps))
    write_clip(p, a.out)
    if a.preview:
        preview_gif(p, a.preview)
    print(f"{a.out}: {len(p.frames)} frames, {p.fps:.1f} fps, {p.size} bytes, peak {p.max_bps():.0f} B/s")
    for n in p.notes:
        print("note:", n)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
