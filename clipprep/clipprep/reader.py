"""Reference clip reader: validates and reconstructs frames (used by tests and preview)."""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

import numpy as np

from .geometry import SCREEN, span_of_rows
from .writer import HEADER, INDEX, MAGIC, RECT, FLAG_CIRCLE_TRIMMED


class ClipError(ValueError):
    pass


def read_clip(path) -> list[tuple[np.ndarray, int]]:
    """Returns [(screen 240x240 uint16 logical 565 after applying the frame, duration_ms)]."""
    data = Path(path).read_bytes() if not isinstance(path, (bytes, bytearray)) else bytes(path)
    if len(data) < HEADER.size:
        raise ClipError("truncated header")
    (magic, version, hsize, w, h, pf, flags, nframes, _key, pal, index_off, pal_off, crc) = HEADER.unpack_from(data)
    if magic != MAGIC or version != 1 or hsize != 32 or (w, h) != (SCREEN, SCREEN) or pf != 1:
        raise ClipError("bad header")
    if not flags & FLAG_CIRCLE_TRIMMED or pal or pal_off:
        raise ClipError("unsupported flags")
    if zlib.crc32(data[32:]) != crc:
        raise ClipError("crc mismatch")
    if nframes == 0 or nframes > 4096 or index_off + nframes * INDEX.size != len(data):
        raise ClipError("bad index")
    screen = np.zeros((SCREEN, SCREEN), dtype=np.uint16)
    out = []
    for i in range(nframes):
        off, length, dur, _fl = INDEX.unpack_from(data, index_off + i * INDEX.size)
        if off < 32 or off + length > index_off or length < 4:
            raise ClipError("frame out of range")
        n, _ = struct.unpack_from("<HH", data, off)
        if n > 16:
            raise ClipError("too many rects")
        p = off + 4
        for _ in range(n):
            x, y, rw, rh, enc, _r, plen = RECT.unpack_from(data, p)
            p += RECT.size
            if enc != 0 or plen != rw * rh * 2 or rw == 0 or rh == 0 or x + rw > SCREEN or y + rh > SCREEN:
                raise ClipError("bad rect")
            sx0, sx1 = span_of_rows(y, rh)
            if x < sx0 or x + rw > sx1:
                raise ClipError("rect outside circle")
            if p + plen > off + length:
                raise ClipError("payload out of range")
            screen[y:y + rh, x:x + rw] = np.frombuffer(data, dtype=">u2", count=rw * rh, offset=p).reshape(rh, rw)
            p += plen
        out.append((screen.copy(), dur))
    return out
