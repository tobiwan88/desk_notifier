"""Clip v1 writer (RAW565 only). Layout: header | frames... | index. See docs/spec/clip-format.md."""
from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

MAGIC = b"EYCL"
HEADER = struct.Struct("<4sHHHHBBHHHIII")  # 32 bytes
INDEX = struct.Struct("<IIHH")  # 12 bytes
RECT = struct.Struct("<HHHHBBI")  # 14 bytes
assert HEADER.size == 32 and INDEX.size == 12 and RECT.size == 14

FLAG_LOOP, FLAG_HAS_KEY, FLAG_CIRCLE_TRIMMED = 1, 2, 8
FRAME_KEY = 1


@dataclass
class FrameData:
    duration_ms: int
    rects: list[tuple[int, int, int, int]] = field(default_factory=list)  # x, y, w, h
    pixels: list[np.ndarray] = field(default_factory=list)  # h x w uint16 logical 565
    key: bool = False


def encode(frames: list[FrameData], loop: bool = False) -> bytes:
    body = bytearray()
    index = []
    off = HEADER.size
    for f in frames:
        chunk = bytearray(struct.pack("<HH", len(f.rects), 0))
        for (x, y, w, h), px in zip(f.rects, f.pixels):
            payload = px.astype(">u2").tobytes()
            assert len(payload) == w * h * 2
            chunk += RECT.pack(x, y, w, h, 0, 0, len(payload)) + payload
        index.append((off + len(body), len(chunk), f.duration_ms, FRAME_KEY if f.key else 0))
        body += chunk
    index_offset = HEADER.size + len(body)
    idx = b"".join(INDEX.pack(*e) for e in index)
    data = bytes(body) + idx
    flags = FLAG_CIRCLE_TRIMMED | (FLAG_LOOP if loop else 0)
    header = HEADER.pack(MAGIC, 1, 32, 240, 240, 1, flags, len(frames), 0, 0, index_offset, 0, zlib.crc32(data))
    return header + data


def write_clip(p, out: Path) -> None:
    Path(out).write_bytes(encode(p.frames, p.loop))
