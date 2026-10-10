"""Resize/crop to the screen, apply the circle mask, convert to RGB565 (big-endian values)."""
from __future__ import annotations

import numpy as np
from PIL import Image

from .geometry import MASK, SCREEN


def parse_color(s: str) -> tuple[int, int, int]:
    s = s.lstrip("#")
    return int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)


def fit_image(rgb: np.ndarray, w: int, h: int, fit: str, bg: tuple[int, int, int]) -> np.ndarray:
    im = Image.fromarray(rgb)
    sw, sh = im.size
    scale = max(w / sw, h / sh) if fit == "cover" else min(w / sw, h / sh)
    nw, nh = max(1, round(sw * scale)), max(1, round(sh * scale))
    im = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGB", (w, h), bg)
    canvas.paste(im, ((w - nw) // 2, (h - nh) // 2))
    return np.asarray(canvas)


def to_565(rgb: np.ndarray) -> np.ndarray:
    """HxWx3 uint8 -> HxW uint16 holding the logical RGB565 value (swap to big-endian on write)."""
    r = rgb[..., 0].astype(np.uint16)
    g = rgb[..., 1].astype(np.uint16)
    b = rgb[..., 2].astype(np.uint16)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def from_565(v: np.ndarray) -> np.ndarray:
    r = ((v >> 11) & 31) << 3
    g = ((v >> 5) & 63) << 2
    b = (v & 31) << 3
    return np.stack([r | (r >> 5), g | (g >> 6), b | (b >> 5)], axis=-1).astype(np.uint8)


def compose(rgb: np.ndarray, region: tuple[int, int, int, int] | None, fit: str, bg: tuple[int, int, int]) -> np.ndarray:
    """Return a SCREEN x SCREEN uint16 565 image (outside the circle forced to bg)."""
    if region is None:
        img = fit_image(rgb, SCREEN, SCREEN, fit, bg)
    else:
        x, y, w, h = region
        canvas = np.zeros((SCREEN, SCREEN, 3), dtype=np.uint8)
        canvas[:] = bg
        canvas[y:y + h, x:x + w] = fit_image(rgb, w, h, fit, bg)
        img = canvas
    out = to_565(img)
    out[~MASK] = to_565(np.array([[bg]], dtype=np.uint8))[0, 0]
    return out
