"""Record air-quality traces from the board's /api/sensor into a CSV (for tuning the score offline).

Usage: uv run python tools/iaq_log.py HOST [out.csv] [--period 10.8] [--hours N]

HOST is the board's host name or IP (device identifiers stay out of the repo: pass it on the command line).
Rows are appended, so a run can be stopped and resumed. Mark events while it runs by typing a label and Enter
(for example `cooking`, `window open`, `window closed`, `people 3`); the label is stored on the next row.
Columns: unix_s, temp_c, humidity, pressure_hpa, gas_ohm, gas_valid, score, level, temp_src, bme_temp_c, label.
"""
import csv
import json
import queue
import sys
import threading
import time
import urllib.request

FIELDS = ["unix_s", "temp_c", "humidity", "pressure_hpa", "gas_ohm", "gas_valid", "score", "level", "temp_src", "bme_temp_c", "label"]


def fetch(host: str) -> dict | None:
    try:
        with urllib.request.urlopen(f"http://{host}/api/sensor", timeout=5) as r:
            return json.load(r)
    except (OSError, ValueError):
        return None


def labels_from_stdin(q: "queue.Queue[str]") -> None:
    for line in sys.stdin:
        line = line.strip()
        if line:
            q.put(line)


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opts = {a.split("=")[0]: a.split("=")[1] for a in sys.argv[1:] if a.startswith("--") and "=" in a}
    if "--period" in sys.argv:
        opts["--period"] = sys.argv[sys.argv.index("--period") + 1]
        args = [a for a in args if a != opts["--period"]]
    if "--hours" in sys.argv:
        opts["--hours"] = sys.argv[sys.argv.index("--hours") + 1]
        args = [a for a in args if a != opts["--hours"]]
    if not args:
        print(__doc__)
        return 2
    host = args[0]
    out = args[1] if len(args) > 1 else "iaq_trace.csv"
    period = float(opts.get("--period", 10.8))  # the board's gas cadence
    poll = period / 3
    end = time.time() + float(opts["--hours"]) * 3600 if "--hours" in opts else None
    labels: "queue.Queue[str]" = queue.Queue()
    threading.Thread(target=labels_from_stdin, args=(labels,), daemon=True).start()
    last_ts = 0.0
    with open(out, "a", newline="") as f:
        w = csv.DictWriter(f, FIELDS)
        if f.tell() == 0:
            w.writeheader()
        while end is None or time.time() < end:
            t0 = time.time()
            s = fetch(host)
            ts = t0 - s["age_ms"] / 1000 if s else 0.0
            # poll faster than the gas cadence and keep only samples we have not stored yet (by their own timestamp)
            if s and s.get("ok") and ts - last_ts > period * 0.5:
                last_ts = ts
                label = []
                while not labels.empty():
                    label.append(labels.get())
                w.writerow({"unix_s": f"{ts:.1f}", "temp_c": s["temp_c"], "humidity": s["humidity"],
                            "pressure_hpa": s["pressure_hpa"], "gas_ohm": s["gas_ohm"], "gas_valid": int(s["gas_valid"]),
                            "score": s["score"], "level": s["level"], "temp_src": s["temp_src"],
                            "bme_temp_c": s["bme_temp_c"], "label": ";".join(label)})
                f.flush()
            time.sleep(max(0.5, poll - (time.time() - t0)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
