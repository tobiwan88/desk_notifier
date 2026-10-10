"""Preview GIF: exactly what the device shows (circle mask, RGB565 quantisation)."""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

from .frames import from_565
from .geometry import MASK


def preview_gif(p, out: Path) -> None:
    imgs = []
    for s in p.screens:
        rgb = from_565(s)
        rgb[~MASK] = 0
        imgs.append(Image.fromarray(rgb))
    imgs[0].save(out, save_all=True, append_images=imgs[1:], duration=[f.duration_ms for f in p.frames], loop=0)
