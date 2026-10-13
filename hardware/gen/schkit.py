"""Thin layer over kicad-sch-api: correct pin positions and a net-intent check.

kicad-sch-api 0.5.x rotates pins clockwise; KiCad rotates symbols counter-clockwise in a
y-up library frame, drawn in a y-down sheet frame. We compute pin positions ourselves and
verify every generated schematic against the exported netlist (`check_nets`).
"""

import math
import re
import subprocess
from pathlib import Path

import kicad_sch_api as ksa


def pin_pos(sch: ksa.Schematic, ref: str, number: str) -> tuple[float, float]:
    comp = sch.components.get(ref)
    sym = ksa.get_symbol_info(comp.lib_id)
    pin = next(p for p in sym.pins if p.number == number)
    r = math.radians(comp.rotation or 0)
    x = pin.position.x * math.cos(r) - pin.position.y * math.sin(r)
    y = pin.position.x * math.sin(r) + pin.position.y * math.cos(r)
    return (round(comp.position.x + x, 2), round(comp.position.y - y, 2))


def pin_away(sch: ksa.Schematic, ref: str, number: str) -> int:
    """Direction (degrees, CCW) pointing from the pin's end away from the symbol body."""
    comp = sch.components.get(ref)
    pin = next(p for p in ksa.get_symbol_info(comp.lib_id).pins if p.number == number)
    return int(pin.rotation + (comp.rotation or 0) + 180) % 360


def connect(sch: ksa.Schematic, nets: dict[str, list[tuple[str, str]]]) -> None:
    """Attach a local label with the net name to every listed pin, text pointing away from the body."""
    for net, pins in nets.items():
        for ref, number in pins:
            sch.add_label(net, position=pin_pos(sch, ref, number), rotation=pin_away(sch, ref, number))


def no_connect(sch: ksa.Schematic, ref: str, numbers: list[str]) -> None:
    for number in numbers:
        sch.no_connects.add(pin_pos(sch, ref, number))


def save(sch: ksa.Schematic, path: Path) -> None:
    """Save, then right-justify labels at 180/270 degrees (kicad-sch-api always writes 'left')."""
    sch.save(path)
    text = re.sub(r'(\(label "[^"]*"\s*\(at [-\d.]+ [-\d.]+ (?:180|270)\)\s*\(effects\s*\(font\s*\(size [\d.]+ [\d.]+\)\s*\)\s*\(justify )left',
                  r"\1right", path.read_text())
    path.write_text(text)


def netlist(sch_file: Path) -> dict[str, set[tuple[str, str]]]:
    net_file = sch_file.with_suffix(".net")
    subprocess.run(["kicad-cli", "sch", "export", "netlist", "-o", str(net_file), str(sch_file)],
                   check=True, capture_output=True)
    text = net_file.read_text().split("(nets", 1)[1]
    out = {}
    for blk in text.split("(net\n")[1:]:
        name = re.search(r'\(name "([^"]+)"', blk).group(1).lstrip("/")
        out[name] = set(re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"', blk))
    return out


def check_nets(sch_file: Path, nets: dict[str, list[tuple[str, str]]]) -> list[str]:
    """Differences between intended nets and KiCad's netlist (power symbols/flags ignored)."""
    actual = netlist(sch_file)
    errors = []
    for net, pins in nets.items():
        want = {p for p in pins if not p[0].startswith("#")}
        got = actual.get(net, set())
        if want != got:
            errors.append(f"{net}: missing {sorted(want - got)} extra {sorted(got - want)}")
    return errors
