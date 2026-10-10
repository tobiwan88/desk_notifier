from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Literal

import numpy as np

from .bands import Rect, changed_bands
from .budget import Budget
from .frames import compose, parse_color
from .load import load
from .writer import FrameData, encode


@dataclass
class PreparedClip:
    frames: list[FrameData]
    screens: list[np.ndarray]  # what the device shows after each frame (for preview)
    loop: bool = False
    notes: list[str] = field(default_factory=list)

    @property
    def size(self) -> int:
        return len(encode(self.frames, self.loop))

    @property
    def duration_ms(self) -> int:
        return sum(f.duration_ms for f in self.frames)

    @property
    def fps(self) -> float:
        return len(self.frames) * 1000 / max(self.duration_ms, 1)

    def max_bps(self) -> float:
        """Worst 1-second window of payload bytes."""
        sizes = [sum(p.size * 2 for p in f.pixels) for f in self.frames]
        durs = [f.duration_ms for f in self.frames]
        worst, t0, acc, j = 0.0, 0, 0, 0
        for i in range(len(sizes)):
            acc += sizes[i]
            while sum(durs[j:i + 1]) > 1000 and j < i:
                acc -= sizes[j]
                j += 1
            span = max(sum(durs[j:i + 1]), 1)
            worst = max(worst, acc * 1000 / max(span, 200))
        return worst


def _resample(frames, fps: float):
    """Pick frames at the target rate (time-based), preserving total duration."""
    step = 1000 / fps
    out, t, nxt = [], 0.0, 0.0
    cum = 0
    for img, d in frames:
        if cum >= nxt - 1e-6:
            out.append([img, 0])
            nxt += step
        out[-1][1] += d
        cum += d
    return [(i, int(max(d, 10))) for i, d in out]


def _build(sources, region: Rect | None, fit: str, bg) -> tuple[list[FrameData], list[np.ndarray]]:
    frames, screens = [], []
    prev = None
    for n, (img, dur) in enumerate(sources):
        cur = compose(img, region, fit, bg)
        rects = changed_bands(prev, cur, region, first=(n == 0))
        fd = FrameData(duration_ms=dur, rects=rects, pixels=[cur[y:y + h, x:x + w].copy() for x, y, w, h in rects], key=(n == 0))
        frames.append(fd)
        shown = cur.copy() if prev is None else prev.copy()
        for (x, y, w, h), px in zip(rects, fd.pixels):
            shown[y:y + h, x:x + w] = px
        screens.append(shown)
        prev = shown
    return frames, screens


def prepare(src: Path, *, eye_region: Literal["full"] | tuple[int, int, int, int] = "full", fps: float | None = None,
            max_frames: int | None = None, budget: Budget = Budget(), fit: Literal["cover", "contain"] = "cover",
            bg: str = "#000000", loop: bool = False) -> PreparedClip:
    region = None if eye_region == "full" else tuple(eye_region)
    bgc = parse_color(bg)
    notes: list[str] = []
    sources = load(Path(src), fps)
    if not sources:
        raise ValueError("no frames in input")
    if fps:
        sources = _resample(sources, fps)
    if max_frames and len(sources) > max_frames:
        notes.append(f"truncated {len(sources)} -> {max_frames} frames")
        sources = sources[:max_frames]
    while True:
        frames, screens = _build(sources, region, fit, bgc)
        p = PreparedClip(frames, screens, loop, notes)
        rate = len(sources) * 1000 / max(sum(d for _, d in sources), 1)
        if (p.size <= budget.max_bytes and p.max_bps() <= budget.max_bps) or rate / 2 < budget.min_fps or len(sources) < 2:
            if p.size > budget.max_bytes or p.max_bps() > budget.max_bps:
                notes.append("could not meet budget (size %d B, %.0f B/s)" % (p.size, p.max_bps()))
            return p
        notes.append(f"over budget ({p.size} B, {p.max_bps():.0f} B/s): halving frame rate {rate:.1f} -> {rate / 2:.1f} fps")
        sources = _resample(sources, rate / 2)
