"""Generate golden clips for the C++ clip tests: uv run python tools/make_goldens.py"""
from pathlib import Path

import numpy as np
from PIL import Image

from clipprep import prepare, write_clip

out = Path(__file__).resolve().parent.parent / "firmware" / "test" / "golden"
out.mkdir(parents=True, exist_ok=True)
tmp = out / "_src.gif"
frames = []
for i in range(5):
    a = np.full((60, 60, 3), (10, 20, 30), np.uint8)
    a[10 + 6 * i:30 + 6 * i, 10:40] = (250, 120, 0)
    frames.append(Image.fromarray(a))
frames[0].save(tmp, save_all=True, append_images=frames[1:], duration=80)
p = prepare(tmp, eye_region=(60, 60, 120, 120))
write_clip(p, out / "good.clip")
full = prepare(tmp)
write_clip(full, out / "full.clip")
data = (out / "good.clip").read_bytes()
(out / "bad_crc.clip").write_bytes(data[:100] + bytes([data[100] ^ 0xFF]) + data[101:])
(out / "truncated.clip").write_bytes(data[:-7])
(out / "bad_magic.clip").write_bytes(b"XXXX" + data[4:])
tmp.unlink()
print("goldens:", sorted(f.name for f in out.iterdir()))
