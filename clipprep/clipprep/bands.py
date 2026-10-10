"""Frame diff -> row bands (disjoint in y, so never overlapping), each trimmed to the circle."""
from __future__ import annotations

import numpy as np

from .geometry import MASK, SCREEN, span_of_rows

MAX_RECTS = 16
CHUNK = 20
Rect = tuple[int, int, int, int]  # x, y, w, h


def changed_bands(prev: np.ndarray | None, cur: np.ndarray, region: Rect | None, first: bool) -> list[Rect]:
    """prev=None -> everything inside `region` (or the full circle) is considered changed."""
    if prev is None or first:
        changed = MASK.copy() if region is None else _region_mask(region)
    else:
        changed = (prev != cur) & MASK
        if region is not None:
            changed &= _region_mask(region)
    rows = np.flatnonzero(changed.any(axis=1))
    if rows.size == 0:
        return []
    # group consecutive rows (gap <= 2 merged)
    runs: list[list[int]] = []
    for y in rows:
        if runs and y - runs[-1][1] <= 3:
            runs[-1][1] = int(y) + 1
        else:
            runs.append([int(y), int(y) + 1])
    # split very tall runs for the 20-line strip granularity? not needed: strips are made on device
    rects = []
    for y0, y1 in runs:  # chunks of <= 20 rows keep rects tight to the circle (same as the device strips)
        for c0 in range(y0, y1, CHUNK):
            if changed[c0:min(c0 + CHUNK, y1)].any():
                rects.append(_run_rect(changed, c0, min(c0 + CHUNK, y1)))
    while len(rects) > MAX_RECTS:  # merge the two closest neighbours (still disjoint in y)
        gaps = [rects[i + 1][1] - (rects[i][1] + rects[i][3]) for i in range(len(rects) - 1)]
        i = int(np.argmin(gaps))
        rects[i:i + 2] = [_union_rows(rects[i], rects[i + 1])]
    return rects


def _region_mask(region: Rect) -> np.ndarray:
    m = np.zeros((SCREEN, SCREEN), dtype=bool)
    x, y, w, h = region
    m[y:y + h, x:x + w] = True
    return m & MASK


def _run_rect(changed: np.ndarray, y0: int, y1: int) -> Rect:
    cols = np.flatnonzero(changed[y0:y1].any(axis=0))
    x0, x1 = int(cols[0]), int(cols[-1]) + 1
    sx0, sx1 = span_of_rows(y0, y1 - y0)  # stay inside the device's trim span
    x0, x1 = max(x0, sx0), min(x1, sx1)
    return (x0, y0, x1 - x0, y1 - y0)


def _union_rows(a: Rect, b: Rect) -> Rect:
    x0, x1 = min(a[0], b[0]), max(a[0] + a[2], b[0] + b[2])
    y0, y1 = a[1], b[1] + b[3]
    sx0, sx1 = span_of_rows(y0, y1 - y0)
    x0, x1 = max(x0, sx0), min(x1, sx1)
    return (x0, y0, x1 - x0, y1 - y0)
