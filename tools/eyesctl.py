"""eyesctl: command line control of an eyes device.

Env: EYES_HOST (e.g. eyes-xxxxxx.local), EYES_TOKEN.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import random
import sys
import time
from pathlib import Path

from eyes_client import Device, DeviceError

MOODS = ["neutral", "happy", "angry", "sleepy", "scared", "curious", "asleep"]


def soak(dev: Device, hours: float, upload: Path | None, out: Path) -> int:
    end = time.time() + hours * 3600
    next_status, next_upload = 0.0, time.time() + 600
    errors = lat = 0
    lats: list[float] = []
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
                    dev.eye(mood=random.choice(MOODS[:-1]))
                else:
                    dev.eye(blink=random.choice(["both", "left", "right"]))
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
    e = sub.add_parser("eye"); e.add_argument("json", help='JSON body, e.g. \'{"mood":"happy"}\'')
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
    a = ap.parse_args(argv)
    dev = Device(a.host, a.token)
    try:
        if a.cmd == "status":
            print(json.dumps(dev.status(), indent=2))
        elif a.cmd == "eye":
            dev.eye(**json.loads(a.json))
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
        elif a.cmd == "soak":
            return soak(dev, a.hours, a.upload, a.csv)
        elif a.cmd == "smoke":
            st = dev.status()
            for m in MOODS:
                dev.eye(mood=m, transition_ms=100); time.sleep(0.4)
            dev.eye(mood="neutral"); dev.eye(blink="left"); dev.eye(look=[0.5, -0.5]); dev.stop()
            print("smoke ok:", st["hostname"], st["fps"], "fps")
    except DeviceError as ex:
        print(ex, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
