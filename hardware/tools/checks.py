"""Hardware checks for notifier-v1 (the gate in scripts/hw-check.sh).

- ERC/DRC from kicad-cli JSON, minus waivers.yaml (never for clearance/short/hole/unconnected)
- pin map: schematic module pins vs firmware Kconfig defaults for DESK_BOARD_NOTIFIER_V1
- reserved pins: no S3 strapping/USB/PSRAM pin used for anything but its job
- BOM: every LCSC part exists, is in stock for QTY boards, Basic/Extended count and cost
Usage: uv run python hardware/tools/checks.py [--bom] [--qty 5]
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "hardware" / "gen"))
sys.path.insert(0, str(ROOT / "hardware" / "tools"))
import notifier_v1 as design  # noqa: E402

BOARD = ROOT / "hardware/notifier-v1/notifier-v1"
NEVER_WAIVE = {"clearance", "shorting_items", "hole_clearance", "hole_to_hole", "unconnected", "drill_out_of_range",
               "track_width", "courtyards_overlap", "copper_edge_clearance", "annular_width", "via_diameter",
               "tracks_crossing", "zones_intersect", "starved_thermal"}
KCONFIG_NET = {"DESK_LCD_PIN_SCLK": "LCD_SCLK", "DESK_LCD_PIN_MOSI": "LCD_MOSI", "DESK_LCD_PIN_DC": "LCD_DC",
               "DESK_LCD_PIN_RST": "LCD_RST", "DESK_LCD_PIN_CS": "LCD_CS", "DESK_I2C_SDA": "I2C_SDA",
               "DESK_I2C_SCL": "I2C_SCL", "DESK_BTN": "BTN"}
# GPIO -> the only net allowed on it
RESERVED = {0: "BOOT", 3: None, 45: None, 46: None, 19: "USB_DN", 20: "USB_DP", 43: "TXD0", 44: "RXD0",
            **{g: None for g in range(26, 38)}}


def kicad(*args):
    subprocess.run(["kicad-cli", *args], capture_output=True, check=True)


def violations():
    out = ROOT / "hardware/out"
    out.mkdir(exist_ok=True)
    kicad("sch", "erc", "--format", "json", "--severity-all", "-o", str(out / "erc.json"), f"{BOARD}.kicad_sch")
    kicad("pcb", "drc", "--format", "json", "--severity-all", "--schematic-parity", "--refill-zones",
          "-o", str(out / "drc.json"), f"{BOARD}.kicad_pcb")
    erc = json.loads((out / "erc.json").read_text())
    drc = json.loads((out / "drc.json").read_text())
    items = [v for s in erc["sheets"] for v in s["violations"]] + drc["violations"] + drc["schematic_parity"]
    items += [{"type": "unconnected", "severity": "error", "description": u["description"], "items": u["items"]}
              for u in drc["unconnected_items"]]
    return items


def waived(v, waivers):
    if v["type"] in NEVER_WAIVE:
        return None
    for w in waivers:
        if w["type"] == v["type"] and any(w["item"] in i["description"] for i in v["items"]):
            return w
    return None


def check_rules(errors):
    waivers = yaml.safe_load((ROOT / "hardware/waivers.yaml").read_text())
    used = set()
    for v in violations():
        w = waived(v, waivers)
        if w:
            used.add(id(w))
            continue
        errors.append(f"{v['severity']} {v['type']}: {v['description'][:80]} {[i['description'][:40] for i in v['items']]}")
    for w in waivers:
        if id(w) not in used:
            print(f"note: waiver no longer needed: {w['type']} {w['item']}")


def check_pins(errors):
    kconf = (ROOT / "firmware/main/Kconfig.projbuild").read_text()
    net_of_gpio = {}
    for net, pins in design.NETS.items():
        for ref, pin in pins:
            if ref == "U1" and pin in design.U1_GPIO:
                net_of_gpio[design.U1_GPIO[pin]] = net
    for key, net in KCONFIG_NET.items():
        m = re.search(rf"config {key}\n.*?default (\d+) if DESK_BOARD_NOTIFIER_V1", kconf, re.S)
        if not m:
            errors.append(f"pin map: {key} has no DESK_BOARD_NOTIFIER_V1 default")
            continue
        gpio = int(m.group(1))
        if net_of_gpio.get(gpio) != net:
            errors.append(f"pin map: {key}={gpio} but the schematic has {net_of_gpio.get(gpio)} on GPIO{gpio} (want {net})")
    for gpio, net in net_of_gpio.items():
        if gpio in RESERVED and RESERVED[gpio] != net:
            errors.append(f"reserved pin: GPIO{gpio} carries {net}")


def check_bom(errors, qty):
    from jlcparts import lookup
    lines, fee, cost = 0, 0, 0.0
    counts = {}
    for ref, (_, _, _, lcsc, _, _) in design.PARTS.items():
        if lcsc:
            counts[lcsc] = counts.get(lcsc, 0) + 1
    for lcsc, n in sorted(counts.items()):
        p = lookup(lcsc)
        lines += 1
        if not p:
            errors.append(f"BOM: {lcsc} not found at JLC")
            continue
        need = n * qty
        flag = "" if p["stock"] >= need * 2 else "  <-- LOW STOCK"
        if p["stock"] < need:
            errors.append(f"BOM: {lcsc} {p['mpn']} stock {p['stock']} < {need}")
        if p["library"] != "Basic" and not p["preferred"]:
            fee += 1
        cost += (p["price"] or 0) * need
        print(f"  {lcsc:10} {p['mpn'][:28]:28} {p['library']:8} x{n:<2} stock {p['stock']:>8}{flag}")
    print(f"BOM: {lines} parts, {fee} Extended (setup fee each), parts cost about ${cost:.2f} for {qty} boards")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bom", action="store_true", help="also check live JLC stock (network)")
    ap.add_argument("--qty", type=int, default=5)
    a = ap.parse_args()
    errors = []
    check_pins(errors)
    check_rules(errors)
    if a.bom:
        check_bom(errors, a.qty)
    for e in errors:
        print("FAIL", e)
    print("hardware checks:", "FAILED" if errors else "OK")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
