"""Screen geometry shared with the firmware (gfx.hpp): the circle mask and trimmed spans."""
from __future__ import annotations

import numpy as np

SCREEN = 240
CIRCLE_R2X4 = 4 * 14470  # same constant as gfx::kCircleR2x4


def chords() -> list[tuple[int, int]]:
    out = []
    for y in range(SCREEN):
        xs = [x for x in range(SCREEN) if (2 * x - (SCREEN - 1)) ** 2 + (2 * y - (SCREEN - 1)) ** 2 <= CIRCLE_R2X4]
        out.append((xs[0], xs[-1] + 1) if xs else (0, 0))
    return out


CHORDS = chords()


def circle_mask() -> np.ndarray:
    m = np.zeros((SCREEN, SCREEN), dtype=bool)
    for y, (a, b) in enumerate(CHORDS):
        m[y, a:b] = True
    return m


MASK = circle_mask()


def span_of_rows(y: int, h: int) -> tuple[int, int]:
    """Widest chord over rows [y, y+h) -- what the device's Strips(trim=true) uses."""
    spans = [CHORDS[r] for r in range(y, min(y + h, SCREEN)) if CHORDS[r][1] > CHORDS[r][0]]
    if not spans:
        return (0, 0)
    return (min(a for a, _ in spans), max(b for _, b in spans))
