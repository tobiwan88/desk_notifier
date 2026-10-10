"""The browser encoder (firmware/components/web/ui/clip_encoder.js) must match clipprep byte for byte."""
import shutil
import subprocess
from pathlib import Path

import numpy as np
import pytest

from clipprep import read_clip
from clipprep.pipeline import _build
from clipprep.writer import encode

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "js" / "test_clip_encoder.mjs"

pytestmark = pytest.mark.skipif(shutil.which("node") is None, reason="node not on PATH")


def synthetic_frames():
    yy, xx = np.mgrid[0:240, 0:240]
    frames = []
    for i in range(4):
        img = np.zeros((240, 240, 3), dtype=np.uint8)
        img[..., 0] = (xx * 255 // 239).astype(np.uint8)  # horizontal gradient
        img[..., 2] = (yy * 255 // 239).astype(np.uint8)  # vertical gradient
        x0 = 20 + i * 45  # moving square, the last position sticks out of the circle
        img[60:120, x0:x0 + 60] = (255, 200, 30)
        frames.append(img)
    frames.append(frames[-1].copy())  # identical frame -> 0 rects
    return frames


@pytest.mark.parametrize("loop,bg", [(False, (0, 0, 0)), (True, (12, 34, 56))])
def test_js_encoder_matches_python(tmp_path, loop, bg):
    imgs = synthetic_frames()
    py_frames, _ = _build([(img, 80) for img in imgs], None, "cover", bg)
    expected = encode(py_frames, loop)

    raw = tmp_path / "frames.rgba"
    raw.write_bytes(b"".join(np.concatenate([img, np.full((240, 240, 1), 255, np.uint8)], axis=2).tobytes()
                            for img in imgs))
    out = tmp_path / "out.clip"
    res = subprocess.run(["node", str(SCRIPT), str(raw), str(out), '{"loop": %s, "bg": [%d, %d, %d]}'
                          % ("true" if loop else "false", *bg)],
                         capture_output=True, text=True, timeout=120)
    assert res.returncode == 0, res.stderr

    got = out.read_bytes()
    assert got == expected
    screens = read_clip(got)
    assert len(screens) == len(imgs)
    assert [d for _, d in screens] == [80] * len(imgs)
    assert [r.rects for r in py_frames][-1] == []  # the identical frame sends nothing
