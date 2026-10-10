"""Load GIF / PNG sequence / video into a list of (RGB array, duration_ms)."""
from __future__ import annotations

import subprocess
import tempfile
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageSequence

Frame = tuple[np.ndarray, int]  # HxWx3 uint8, duration in ms

VIDEO_EXT = {".mp4", ".mov", ".mkv", ".webm", ".avi", ".m4v"}


def _from_pil(img: Image.Image) -> np.ndarray:
    rgba = img.convert("RGBA")
    bg = Image.new("RGBA", rgba.size, (0, 0, 0, 255))
    return np.asarray(Image.alpha_composite(bg, rgba).convert("RGB"))


def load_gif(path: Path) -> list[Frame]:
    with Image.open(path) as im:
        return [(_from_pil(f.copy()), max(int(f.info.get("duration", im.info.get("duration", 100))), 10))
                for f in ImageSequence.Iterator(im)]


def load_images(paths: list[Path], fps: float) -> list[Frame]:
    ms = round(1000 / fps)
    out = []
    for p in sorted(paths):
        with Image.open(p) as im:
            out.append((_from_pil(im), ms))
    return out


def load_video(path: Path, fps: float, max_seconds: float = 30.0) -> list[Frame]:
    with tempfile.TemporaryDirectory() as td:
        subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-t", str(max_seconds), "-vf", f"fps={fps}",
                        f"{td}/f%05d.png"], check=True)
        return load_images(list(Path(td).glob("f*.png")), fps)


def load(src: Path, fps: float | None = None) -> list[Frame]:
    src = Path(src)
    ext = src.suffix.lower()
    if src.is_dir():
        return load_images([p for p in src.iterdir() if p.suffix.lower() in {".png", ".jpg", ".jpeg"}], fps or 15)
    if ext == ".zip":
        with tempfile.TemporaryDirectory() as td, zipfile.ZipFile(src) as z:
            z.extractall(td)
            files = [p for p in Path(td).rglob("*") if p.suffix.lower() in {".png", ".jpg", ".jpeg"}]
            return load_images(files, fps or 15)
    if ext == ".gif":
        return load_gif(src)
    if ext in VIDEO_EXT:
        return load_video(src, fps or 15)
    if ext in {".png", ".jpg", ".jpeg"}:
        return load_images([src], fps or 15)
    raise ValueError(f"unsupported input: {src}")
