"""Fit the humidity coefficient k of the VOC index from a trace recorded with tools/iaq_log.py.

Usage: uv run python tools/iaq_fit.py trace.csv [--skip-min 30] [--quantile 0.9]

Model (core/vocindex.cpp):  ln Rc = ln R + k * (AH - 10)  should not follow the humidity, so over the trace
ln R falls by k per g/m3 of absolute humidity (AH) when nothing else changes. VOC events only push R down, so
we use an upper envelope: per hour, the high quantile of ln R against the mean AH, with one intercept per day
(the baseline creeps from day to day). Slope of that regression = -k. Needs several days with some humidity
variation; it says so when it does not have them. The result is a starting point to check by eye, not an oracle.
"""
import csv
import math
import sys
from dataclasses import dataclass

import numpy as np


def absolute_humidity(temp_c: np.ndarray, rh: np.ndarray) -> np.ndarray:
    """g/m3, same Magnus form as sensor::absolute_humidity in the firmware."""
    rh = np.clip(rh, 0.0, 100.0)
    return 216.7 * (rh / 100.0) * 6.112 * np.exp(17.62 * temp_c / (243.12 + temp_c)) / (273.15 + temp_c)


@dataclass
class Fit:
    k: float
    stderr: float
    r2: float
    hours: int
    days: int
    ah_range: float
    warnings: list[str]


def load(path: str) -> dict[str, np.ndarray]:
    cols: dict[str, list[float]] = {"unix_s": [], "temp_c": [], "humidity": [], "gas_ohm": [], "gas_valid": []}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            try:
                vals = {c: float(row[c]) for c in cols}
            except (KeyError, ValueError):
                continue
            for c, v in vals.items():
                cols[c].append(v)
    return {c: np.asarray(v) for c, v in cols.items()}


def fit_k(d: dict[str, np.ndarray], skip_min: float = 30.0, quantile: float = 0.9, min_per_hour: int = 150) -> Fit:
    warnings: list[str] = []
    t, gas, valid = d["unix_s"], d["gas_ohm"], d["gas_valid"]
    keep = (valid > 0) & (gas > 0) & (t >= (t.min() + skip_min * 60 if len(t) else 0))
    if keep.sum() < 3 * min_per_hour:
        return Fit(float("nan"), float("nan"), 0.0, 0, 0, 0.0, ["too few valid samples"])
    t, lnr = t[keep], np.log(gas[keep])
    ah = absolute_humidity(d["temp_c"][keep], d["humidity"][keep])
    hour = np.floor(t / 3600).astype(np.int64)
    xs, ys, ds = [], [], []
    for h in np.unique(hour):
        m = hour == h
        if m.sum() < min_per_hour:  # a full hour at 10.8 s is 333 samples
            continue
        xs.append(ah[m].mean())
        ys.append(np.quantile(lnr[m], quantile))
        ds.append(int(h // 24))
    xs, ys, ds = np.asarray(xs), np.asarray(ys), np.asarray(ds)
    if len(xs) < 6:
        return Fit(float("nan"), float("nan"), 0.0, len(xs), len(set(ds)), 0.0, ["fewer than 6 usable hours"])
    # one intercept per day: subtract each day's mean
    xd, yd = xs.copy(), ys.copy()
    for day in np.unique(ds):
        m = ds == day
        xd[m] -= xs[m].mean()
        yd[m] -= ys[m].mean()
    sxx = float((xd * xd).sum())
    ah_range = float(xs.max() - xs.min())
    if sxx < 1e-9:
        return Fit(float("nan"), float("nan"), 0.0, len(xs), len(np.unique(ds)), ah_range, ["no humidity variation within the days"])
    slope = float((xd * yd).sum() / sxx)
    resid = yd - slope * xd
    dof = max(1, len(xs) - len(np.unique(ds)) - 1)
    stderr = math.sqrt(float((resid * resid).sum()) / dof / sxx)
    r2 = 1.0 - float((resid * resid).sum()) / max(float((yd * yd).sum()), 1e-12)
    k = -slope
    if ah_range < 2.0:
        warnings.append(f"absolute humidity only varied {ah_range:.1f} g/m3: the fit is weak")
    if len(np.unique(ds)) < 3:
        warnings.append("fewer than 3 days: record at least a week")
    if k < 0:
        warnings.append("negative k: resistance rose with humidity, check the trace (heater, location, events)")
    if not 0.0 <= k <= 0.25:
        warnings.append("k outside the 0..0.25 range from the research: do not trust it")
    return Fit(k, stderr, r2, len(xs), len(np.unique(ds)), ah_range, warnings)


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    skip, q = 30.0, 0.9
    if "--skip-min" in sys.argv:
        skip = float(sys.argv[sys.argv.index("--skip-min") + 1])
        args = [a for a in args if a != sys.argv[sys.argv.index("--skip-min") + 1]]
    if "--quantile" in sys.argv:
        q = float(sys.argv[sys.argv.index("--quantile") + 1])
        args = [a for a in args if a != sys.argv[sys.argv.index("--quantile") + 1]]
    f = fit_k(load(args[0]), skip, q)
    print(f"k = {f.k:.4f} per g/m3 (+/- {f.stderr:.4f}), R2 {f.r2:.2f}, {f.hours} hours over {f.days} days, AH range {f.ah_range:.1f} g/m3")
    for w in f.warnings:
        print("warning:", w)
    return 0


if __name__ == "__main__":
    sys.exit(main())
