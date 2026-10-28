"""Buck converter design check: output voltage, inductor ripple and peak current, effective output capacitance.

Inputs: `power:` in board.yaml, component values from the .kicad_pcb, limits from the datasheet cards (parts/).
A value the card does not have yet prints MISSING (fill in the card from the datasheet); --strict makes it a FAIL.
Usage: uv run --package desk-hardware python hardware/calc/buck.py hardware/<board> [--strict]
Formulas: Vout = Vref * (1 + Rtop/Rbot); dI = Vout * (1 - Vout/Vin) / (L * fsw); Ipeak = Iout + dI/2.
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from pcb import load, report, si  # noqa: E402


def val(card: dict, key: str):
    v = (card.get("values") or {}).get(key) or {}
    return v.get("value")


def check(e: dict, prof: dict, values: dict) -> list[tuple[str, str, str]]:
    card, lcard = prof["parts"][e["card"]], prof["parts"].get(e.get("inductor_card"), {})
    n = e["name"]
    rows = []

    def missing(what: str, where: str):
        rows.append(("MISSING", f"{n}: {what}", f"no value in parts/{where}.yaml"))

    vref = val(card, "vref_v")
    rt, rb = si(values[e["divider"]["top"]]), si(values[e["divider"]["bottom"]])
    vout = e["vout_v"]
    if vref is None:
        missing("Vout", e["card"])
    else:
        vout = vref * (1 + rt / rb)
        err = 100 * (vout - e["vout_v"]) / e["vout_v"]
        rows.append(("OK" if abs(err) <= e["vout_tol_pct"] else "FAIL", f"{n}: Vout",
                     f"{vout:.3f} V from {rt:g}/{rb:g} ohm ({err:+.1f} %, max {e['vout_tol_pct']} %)"))

    imax = val(card, "iout_max_a")
    if imax is None:
        missing("load current", e["card"])
    else:
        rows.append(("OK" if e["iout_a"] <= imax else "FAIL", f"{n}: load current", f"{e['iout_a']} A (part max {imax} A)"))

    fsw, isat = val(card, "fsw_hz"), val(lcard, "isat_a")
    L = si(values[e["inductor"]])
    if fsw is None:
        missing("inductor ripple", e["card"])
    else:
        di = vout * (1 - vout / max(e["vin_v"])) / (L * fsw)
        ipk = e["iout_a"] + di / 2
        rows.append(("OK", f"{n}: ripple", f"{di * 1000:.0f} mA p-p at {max(e['vin_v'])} V in, peak {ipk:.2f} A"))
        if isat is None:
            missing("inductor saturation", e["inductor_card"])
        else:
            rows.append(("OK" if ipk * 1.2 <= isat else "FAIL", f"{n}: Isat margin", f"peak {ipk:.2f} A x1.2 vs Isat {isat} A"))

    rng = val(card, "cout_effective_uf")
    ceff = sum(si(values[c]) for c in e["cout"]) * e["cout_derating"] * 1e6
    if rng is None:
        missing("output capacitance", e["card"])
    else:
        rows.append(("OK" if rng[0] <= ceff <= rng[1] else "FAIL", f"{n}: Cout effective",
                     f"{ceff:.1f} uF after derating x{e['cout_derating']} (datasheet {rng[0]}-{rng[1]} uF)"))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("board_dir", type=Path)
    ap.add_argument("--strict", action="store_true", help="MISSING datasheet values fail")
    a = ap.parse_args()
    prof, b = load(a.board_dir)
    rows = [r for e in prof.get("power", []) for r in check(e, prof, b.values)]
    if a.strict:
        rows = [("FAIL", *r[1:]) if r[0] == "MISSING" else r for r in rows]
    fails = report(rows, f"power: {a.board_dir}")
    print("power:", "FAILED" if fails else "OK")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
