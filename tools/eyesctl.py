"""eyesctl: command line control of an eyes device.

Env: EYES_HOST (e.g. eyes-xxxxxx.local), EYES_TOKEN.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import random
import re
import sys
import time
from pathlib import Path

import requests

from eyes_client import Device, DeviceError

SMOKE_ANIMS = ["wink", "glance", "happy"]
CLIP_NAME = re.compile(r"[a-z0-9_-]{1,31}")


def print_pool(pool: dict) -> None:
    for it in pool["items"]:
        mark = "x" if it["enabled"] else " "
        print(f"[{mark}] {it['name']:<31} ({it['kind']})")
    lo, hi = pool["interval"]
    print(f"interval {lo:g}-{hi:g} s, auto {'on' if pool['auto'] else 'off'}")


def pool_cmd(dev: Device, a: argparse.Namespace) -> int:
    cur = dev.pool()
    if a.pool_cmd is None:
        print_pool(cur)
        return 0
    names = [it["name"] for it in cur["items"]]
    enabled = [it["name"] for it in cur["items"] if it["enabled"]]
    wanted = list(getattr(a, "names", []) or [])
    unknown = [n for n in wanted if n not in names]
    if unknown:
        print(f"unknown item: {', '.join(unknown)}", file=sys.stderr)
        return 1
    interval = None
    if a.pool_cmd == "set":
        new, interval = wanted, a.interval
    elif a.pool_cmd == "add":
        new = enabled + [n for n in wanted if n not in enabled]
    elif a.pool_cmd == "remove":
        new = [n for n in enabled if n not in wanted]
    elif a.pool_cmd == "all":
        new = names
    else:  # none
        new = []
    print_pool(dev.set_pool(enabled=new, interval=interval))
    return 0


def add_clip(dev: Device, a: argparse.Namespace) -> int:
    from clipprep import Budget, prepare  # lazy: only this command needs clipprep
    from clipprep.writer import encode

    name = a.name or re.sub(r"[^a-z0-9_-]", "_", a.file.stem.lower())[:31]
    if not CLIP_NAME.fullmatch(name):
        print(f"invalid clip name {name!r}: use a-z 0-9 _ - (max 31 chars), or pass --name", file=sys.stderr)
        return 1
    region: tuple[int, int, int, int] | str = "full"
    if a.region:
        parts = a.region.split(",")
        if len(parts) != 4 or not all(p.strip().isdigit() for p in parts):
            print("--region must be x,y,w,h", file=sys.stderr)
            return 1
        region = tuple(int(p) for p in parts)
    st = dev.status()
    budget = Budget(max_bytes=min(1_500_000, max(st["fs_free"] - 65536, 0)), max_bps=int(st["clip_max_bps"] * 0.8))
    try:
        p = prepare(a.file, eye_region=region, fps=a.fps, max_frames=a.max_frames, fit=a.fit, budget=budget)
    except (ValueError, OSError) as ex:
        print(f"cannot prepare {a.file}: {ex}", file=sys.stderr)
        return 1
    data = encode(p.frames, p.loop)
    dev.upload(name, data)
    print(f"uploaded {name}: {len(p.frames)} frames, {p.fps:.1f} fps, {len(data)} bytes")
    for note in p.notes:
        print("note:", note)
    if a.pool:
        enabled = [it["name"] for it in dev.pool()["items"] if it["enabled"]]
        if name not in enabled:
            enabled.append(name)
        print_pool(dev.set_pool(enabled=enabled))
    return 0


def soak(dev: Device, hours: float, upload: Path | None, out: Path) -> int:
    end = time.time() + hours * 3600
    next_status, next_upload = 0.0, time.time() + 600
    errors = lat = 0
    lats: list[float] = []
    anims = dev.animations()["animations"]
    with out.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t", "uptime_s", "heap_internal", "heap_min", "fps", "frame_ms_max", "errors"])
        while time.time() < end:
            t = time.time()
            try:
                c = random.random()
                s = time.time()
                if c < 0.5:
                    dev.eye(look=[random.uniform(-1, 1), random.uniform(-1, 1)])
                elif c < 0.8:
                    dev.eye(play=random.choice(anims))
                else:
                    dev.eye(play="random")
                lats.append(time.time() - s)
                if upload and t >= next_upload:
                    dev.upload("soak", upload); dev.delete("soak"); next_upload = t + 600
                if t >= next_status:
                    st = dev.status()
                    w.writerow([int(t), st["uptime_s"], st["heap_internal"], st["heap_min"], st["fps"], st["frame_ms_max"], errors]); f.flush()
                    next_status = t + 10
            except Exception as e:  # noqa: BLE001
                errors += 1
                print("error:", e, file=sys.stderr)
            time.sleep(random.uniform(0.2, 2.0))
    lats.sort()
    p99 = lats[int(len(lats) * 0.99)] if lats else 0
    print(f"soak done: {len(lats)} requests, {errors} errors, p99 {p99 * 1000:.0f} ms")
    return 0 if errors == 0 and p99 < 0.3 else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="eyesctl")
    ap.add_argument("--host", default=os.environ.get("EYES_HOST", "eyes.local"))
    ap.add_argument("--token", default=os.environ.get("EYES_TOKEN", ""))
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    e = sub.add_parser("eye"); e.add_argument("json", help='JSON body, e.g. \'{"play":"happy","look":[0,0.5,800]}\'')
    an = sub.add_parser("anim"); an.add_argument("name", help="animation name, e.g. wink, glance, random, sparkle")
    st = sub.add_parser("style"); st.add_argument("style", help="realistic|flat|line|anime|cute|halloween|christmas")
    st.add_argument("--iris", help="amber|green|blue|copper|violet|odd"); st.add_argument("--save", action="store_true")
    p = sub.add_parser("play"); p.add_argument("clip", nargs="?"); p.add_argument("--left"); p.add_argument("--right")
    p.add_argument("--loop", action="store_true"); p.add_argument("--hold", action="store_true")
    sub.add_parser("stop")
    sub.add_parser("clips")
    u = sub.add_parser("upload"); u.add_argument("file", type=Path); u.add_argument("--name")
    d = sub.add_parser("delete"); d.add_argument("name")
    o = sub.add_parser("ota"); o.add_argument("bin", type=Path)
    sk = sub.add_parser("soak"); sk.add_argument("--hours", type=float, default=1); sk.add_argument("--upload", type=Path)
    sk.add_argument("--csv", type=Path, default=Path("soak.csv"))
    sm = sub.add_parser("smoke")
    pl = sub.add_parser("pool", help="show or edit the random-animation pool")
    plsub = pl.add_subparsers(dest="pool_cmd")
    pset = plsub.add_parser("set", help="enable exactly these items"); pset.add_argument("names", nargs="+")
    pset.add_argument("--interval", nargs=2, type=float, metavar=("MIN", "MAX"))
    plsub.add_parser("add", help="enable these items too").add_argument("names", nargs="+")
    plsub.add_parser("remove", help="disable these items").add_argument("names", nargs="+")
    plsub.add_parser("all", help="enable every item")
    plsub.add_parser("none", help="disable every item")
    ad = sub.add_parser("add", help="prepare a GIF/PNG zip/dir/video and upload it as a clip")
    ad.add_argument("file", type=Path); ad.add_argument("--name", help="clip name (default: file stem)")
    ad.add_argument("--fps", type=float); ad.add_argument("--region", help="eye region x,y,w,h")
    ad.add_argument("--fit", choices=["cover", "contain"], default="cover")
    ad.add_argument("--max-frames", type=int, dest="max_frames"); ad.add_argument("--pool", action="store_true",
                                                                                 help="also enable it in the pool")
    a = ap.parse_args(argv)
    dev = Device(a.host, a.token)
    try:
        if a.cmd == "status":
            print(json.dumps(dev.status(), indent=2))
        elif a.cmd == "eye":
            dev.eye(**json.loads(a.json))
        elif a.cmd == "anim":
            dev.eye(play=a.name)
        elif a.cmd == "style":
            body: dict = {"style": a.style}
            if a.iris:
                body["iris"] = a.iris
            if a.save:
                body["save"] = True
            dev.eye(**body)
        elif a.cmd == "play":
            dev.play(a.clip, left=a.left, right=a.right, loop=a.loop, then="hold" if a.hold else "eyes")
        elif a.cmd == "stop":
            dev.stop()
        elif a.cmd == "clips":
            print(json.dumps(dev.clips(), indent=2))
        elif a.cmd == "upload":
            print(json.dumps(dev.upload(a.name or a.file.stem, a.file)))
        elif a.cmd == "delete":
            dev.delete(a.name)
        elif a.cmd == "ota":
            dev.ota(a.bin)
            print("uploaded; waiting for reboot")
            time.sleep(5)
            print(json.dumps(dev.wait_up(), indent=2))
        elif a.cmd == "pool":
            return pool_cmd(dev, a)
        elif a.cmd == "add":
            return add_clip(dev, a)
        elif a.cmd == "soak":
            return soak(dev, a.hours, a.upload, a.csv)
        elif a.cmd == "smoke":
            st = dev.status()
            dev.eye(style="realistic")
            for name in SMOKE_ANIMS:
                dev.eye(play=name); time.sleep(0.4)
            dev.eye(play="random"); dev.eye(auto=True)
            anims = dev.animations()
            print("smoke ok:", st["hostname"], st["fps"], "fps,", len(anims["animations"]), "animations")
    except (DeviceError, requests.RequestException) as ex:
        print(ex, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
