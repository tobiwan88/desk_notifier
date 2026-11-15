import numpy as np

from iaq_fit import absolute_humidity, fit_k


def synthetic(k: float, days: int = 6, seed: int = 1, events: bool = True) -> dict[str, np.ndarray]:
    """ln R = base(day) - k*AH + noise, with VOC events that only push R down."""
    rng = np.random.default_rng(seed)
    n = days * 8000  # ~24 h at 10.8 s is 8000 samples
    t = 1.7e9 + np.arange(n) * 10.8
    hours = (t / 3600) % 24
    temp = 21 + 1.5 * np.sin(2 * np.pi * (hours - 14) / 24)
    rh = 48 + 14 * np.sin(2 * np.pi * (hours - 5) / 24) + 6 * np.sin(np.arange(n) / 20000.0) + rng.normal(0, 0.5, n)
    ah = absolute_humidity(temp, rh)
    day = np.floor((t - t[0]) / 86400)
    lnr = 11.5 + 0.03 * day - k * (ah - 10) + rng.normal(0, 0.01, n)
    if events:
        for i in rng.integers(0, n - 300, size=days * 6):
            lnr[i:i + rng.integers(30, 300)] -= rng.uniform(0.1, 0.5)  # cooking, a spray: only downwards
    return {"unix_s": t, "temp_c": temp, "humidity": rh, "gas_ohm": np.exp(lnr), "gas_valid": np.ones(n)}


def test_recovers_k_despite_events():
    for k in (0.03, 0.10, 0.20):
        f = fit_k(synthetic(k))
        assert abs(f.k - k) < 0.02, (k, f)
        assert f.days >= 5


def test_flat_humidity_is_reported_not_fitted():
    d = synthetic(0.1)
    d["humidity"] = np.full_like(d["humidity"], 45.0)
    d["temp_c"] = np.full_like(d["temp_c"], 21.0)
    f = fit_k(d)
    assert np.isnan(f.k)
    assert f.warnings


def test_too_little_data():
    d = synthetic(0.1, days=1)
    short = {k: v[:100] for k, v in d.items()}
    assert np.isnan(fit_k(short).k)


def test_invalid_samples_are_ignored():
    d = synthetic(0.1)
    d["gas_valid"][::2] = 0
    d["gas_ohm"][::2] = 1.0  # garbage where invalid
    assert abs(fit_k(d).k - 0.1) < 0.03
