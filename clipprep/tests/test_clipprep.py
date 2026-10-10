import struct

import numpy as np
import pytest
from PIL import Image

from clipprep import Budget, prepare, read_clip, write_clip
from clipprep.frames import compose, from_565, parse_color, to_565
from clipprep.geometry import CHORDS, MASK, span_of_rows
from clipprep.reader import ClipError
from clipprep.writer import HEADER


def make_gif(path, n=6, size=(100, 100)):
    frames = []
    for i in range(n):
        a = np.zeros((size[1], size[0], 3), np.uint8)
        a[:, :] = (10, 20, 30)
        a[20 + i * 5:40 + i * 5, 20:60] = (250, 100, 0)
        frames.append(Image.fromarray(a))
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=100, loop=0)


def test_geometry_matches_firmware_constants():
    assert int(MASK.sum()) == 45476  # circle_pixels() in gfx.hpp tests
    assert sum((span_of_rows(y, 20)[1] - span_of_rows(y, 20)[0]) * 20 for y in range(0, 240, 20)) == 48880


def test_565_roundtrip_quantises():
    rgb = np.array([[[255, 128, 0], [0, 0, 0]]], np.uint8)
    back = from_565(to_565(rgb))
    assert tuple(back[0, 0]) == (255, 130, 0) or abs(int(back[0, 0, 1]) - 128) <= 4
    assert tuple(back[0, 1]) == (0, 0, 0)


def test_roundtrip_bit_exact(tmp_path):
    gif = tmp_path / "a.gif"
    make_gif(gif)
    p = prepare(gif)
    out = tmp_path / "a.clip"
    write_clip(p, out)
    frames = read_clip(out)
    assert len(frames) == len(p.frames) == 6
    for (screen, dur), shown, f in zip(frames, p.screens, p.frames):
        assert dur == f.duration_ms
        assert np.array_equal(screen, shown)
    # first frame is the full circle, later ones are small diffs
    assert sum(r[2] * r[3] for r in p.frames[0].rects) > 40000
    assert sum(r[2] * r[3] for r in p.frames[2].rects) < 8000


def test_static_frames_become_hold_frames():
    from clipprep.pipeline import _build
    a = np.full((50, 50, 3), 7, np.uint8)
    frames, _ = _build([(a, 50), (a, 50), (a, 50)], None, "cover", (0, 0, 0))
    assert frames[1].rects == [] and frames[2].rects == []


def test_region_clip_only_touches_region(tmp_path):
    gif = tmp_path / "r.gif"
    make_gif(gif, size=(30, 20))
    p = prepare(gif, eye_region=(90, 100, 60, 40))
    for f in p.frames:
        for x, y, w, h in f.rects:
            assert 90 <= x and x + w <= 150 and 100 <= y and y + h <= 140


def test_budget_halves_frame_rate(tmp_path):
    gif = tmp_path / "b.gif"
    make_gif(gif, n=16)
    p = prepare(gif, budget=Budget(max_bytes=150_000, max_bps=10**9, min_fps=1))
    assert p.size <= 150_000 and any("halving" in n for n in p.notes)


def test_rect_count_limit(tmp_path):
    frames = []
    for i in range(2):
        a = np.zeros((240, 240, 3), np.uint8)
        if i:
            a[::6, :] = 255  # many separate bands
        frames.append(Image.fromarray(a))
    gif = tmp_path / "m.gif"
    frames[0].save(gif, save_all=True, append_images=frames[1:], duration=100)
    p = prepare(gif)
    assert all(len(f.rects) <= 16 for f in p.frames)
    write_clip(p, tmp_path / "m.clip")
    read_clip(tmp_path / "m.clip")


@pytest.mark.parametrize("mutate", ["magic", "crc", "trunc", "version"])
def test_reader_rejects_corrupt(tmp_path, mutate):
    gif = tmp_path / "c.gif"
    make_gif(gif, n=3)
    write_clip(prepare(gif), tmp_path / "c.clip")
    data = bytearray((tmp_path / "c.clip").read_bytes())
    if mutate == "magic":
        data[0] = 0
    elif mutate == "crc":
        data[100] ^= 0xFF
    elif mutate == "trunc":
        data = data[:-5]
    else:
        data[4] = 9
    with pytest.raises(ClipError):
        read_clip(bytes(data))


def test_preview_gif(tmp_path):
    from clipprep.preview import preview_gif
    gif = tmp_path / "p.gif"
    make_gif(gif, n=3)
    p = prepare(gif)
    preview_gif(p, tmp_path / "p_out.gif")
    with Image.open(tmp_path / "p_out.gif") as im:
        assert im.size == (240, 240) and im.n_frames == 3


def test_cli(tmp_path):
    from clipprep.cli import main
    gif = tmp_path / "x.gif"
    make_gif(gif, n=4)
    assert main([str(gif), str(tmp_path / "x.clip"), "--preview", str(tmp_path / "x.gif.png")]) == 0
    assert (tmp_path / "x.clip").stat().st_size > 1000
