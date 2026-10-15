"""Thin layer over kicad-sch-api: correct pin positions and a net-intent check.

kicad-sch-api 0.5.x rotates pins clockwise; KiCad rotates symbols counter-clockwise in a
y-up library frame, drawn in a y-down sheet frame. We compute pin positions ourselves and
verify every generated schematic against the exported netlist (`check_nets`).
"""

import math
import uuid
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


CELL = 2.54  # drawing grid: sheet coordinates are given in cells of 2.54 mm (fractions of 0.5 are on the 1.27 grid)
_uid = [0]


def uid() -> str:
    _uid[0] += 1
    return str(uuid.uuid5(uuid.NAMESPACE_URL, f"notifier-v1/{_uid[0]}"))


def mm(cell: float) -> float:
    return round(cell * CELL, 2)


def q(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


class Sheet:
    """One schematic sheet: parts via kicad-sch-api, everything else written as S-expressions.

    Coordinates are in cells (2.54 mm). Wires are polylines through pins ((ref, pin)) and points (x, y).
    Junctions are derived from the wire geometry when saving.
    """

    pwr_count = 0  # power-symbol references are unique over all sheets

    def __init__(self, name: str, filename: str, parts: dict, paper: str = "A3", note: str = ""):
        self.name, self.filename, self.parts = name, filename, parts
        self.paper, self.note = paper, note
        self.sch = ksa.Schematic.create("notifier-v1")
        self.sch.set_paper_size(paper)
        self.sch.set_title_block(title="desk notifier v1", date="2026-10-15", rev="v1", company="desk_notifier",
                                 comments={1: f"Sheet: {name}", 2: note})
        self.uuid = str(uuid.uuid5(uuid.NAMESPACE_URL, "sheet/" + name))
        self.raw: list[str] = []
        self.segs: list[tuple[tuple[float, float], tuple[float, float]]] = []
        self.pins: set[tuple[float, float]] = set()
        self.refs: list[str] = []
        self.mirror: dict[str, str] = {}
        self.specs: list = []
        self.tags: dict[str, dict[str, tuple[float, float, str]]] = {}
        self.rots: dict[str, int] = {}

    # --- parts -------------------------------------------------------------
    def part(self, ref: str, x: float, y: float, rot: int = 0, value: str | None = None, mirror: bool = False):
        sym, val, fp, lcsc = self.parts[ref][:4]
        rot = rot or 0
        props = {"LCSC": lcsc} if lcsc else {}
        comp = self.sch.components.add(sym, ref, value or val, (mm(x), mm(y)), fp, rotation=rot, **props)
        if not lcsc and not ref.startswith("#"):
            comp.in_bom = False
        self.refs.append(ref)
        self.rots[ref] = rot
        if mirror:
            self.mirror[ref] = "y"  # (mirror y): left-right flip, applied to the file when saving
        self._register(ref)
        return comp

    def _register(self, ref: str) -> None:
        info = ksa.get_symbol_info(self.sch.components.get(ref).lib_id)
        for p in info.pins:
            self.pins.add(self.at(ref, p.number))

    def tag(self, ref: str, reference=None, value=None, just: str = "left") -> None:
        """Place the reference/value texts at (dx, dy) mm from the symbol origin (None keeps the automatic spot)."""
        self.tags[ref] = {"Reference": reference and (*reference, just), "Value": value and (*value, just)}

    def _auto_tags(self) -> None:
        for ref in self.refs:
            if ref in self.tags or ref.startswith("#PWR"):
                continue
            lib = self.sch.components.get(ref).lib_id
            if lib in ("Device:R", "Device:C", "Device:L", "Device:Polyfuse", "Device:LED", "Switch:SW_Push", "Connector:TestPoint"):
                native_h = lib in ("Device:LED", "Switch:SW_Push")
                if (self.rots.get(ref, 0) in (90, 270)) != native_h:
                    self.tags[ref] = {"Reference": (0, -5.2, "center"), "Value": (0, -2.9, "center")}
                else:
                    self.tags[ref] = {"Reference": (2.2, -1.3, "left"), "Value": (2.2, 1.3, "left")}
            elif lib == "power:PWR_FLAG":
                self.tags[ref] = {"Reference": "hide", "Value": "hide"}

    def _apply_tags(self, txt: str) -> str:
        for ref, props in self.tags.items():
            comp = self.sch.components.get(ref)
            for name, spec in props.items():
                if not spec:
                    continue
                if spec == "hide":
                    pat = re.compile(r'(\(property "%s" "[^"]*"\s*\(at [^\n]*\n\s*\(effects\s*\(font\s*\(size [\d. ]+\)\s*\))' % name)
                    blocks = txt.split("\t(symbol\n\t\t(lib_id")
                    for i, blk in enumerate(blocks[1:], 1):
                        if f'(property "Reference" "{ref}"' in blk:
                            blocks[i] = pat.sub(r"\1 (hide yes)", blk, count=1)
                    txt = "\t(symbol\n\t\t(lib_id".join(blocks)
                    continue
                dx, dy, just = spec
                pat = re.compile(r'(\(property "%s" "[^"]*"\s*\(at )[-\d.]+ [-\d.]+ [-\d.]+(\)\s*\(effects\s*\(font\s*\(size [\d. ]+\)\s*\))(\s*\(justify [a-z ]+\))?' % name)
                blocks = txt.split("\t(symbol\n\t\t(lib_id")
                for i, blk in enumerate(blocks[1:], 1):
                    if f'(property "Reference" "{ref}"' in blk:
                        j = f'(justify {just})' if just != "center" else ""
                        x, y = round(comp.position.x + dx, 2), round(comp.position.y + dy, 2)
                        ang = (-self.rots.get(ref, 0)) % 360 if self.rots.get(ref, 0) in (90, 270) else 0  # field angles add to the rotation
                        if self.mirror.get(ref) == "y" and just != "center":
                            j = f"(justify {'right' if just == 'left' else 'left'})"  # mirrored symbols flip the justification
                        blocks[i] = pat.sub(lambda m: f"{m.group(1)}{x} {y} {ang}{m.group(2)} {j}", blk, count=1)
                txt = "\t(symbol\n\t\t(lib_id".join(blocks)
        return txt

    def power(self, net: str, x: float, y: float, flip: bool = False) -> None:
        """Explicit power symbol (GND, +3V3, VBUS) whose pin sits at (x, y); flip draws it pointing down."""
        Sheet.pwr_count += 1
        ref = f"#PWR{Sheet.pwr_count:02d}"
        self.sch.components.add(f"power:{net}", ref, net, (mm(x), mm(y)))
        self.refs.append(ref)
        self.pins.add((mm(x), mm(y)))
        if flip:
            self.mirror[ref] = "x"
            self.rots[ref] = 0
            self.tags[ref] = {"Reference": "hide", "Value": (0, 4.2, "center")}

    def flag(self, ref: str, x: float, y: float) -> None:
        self.part(ref, x, y)

    def at(self, ref: str, pin: str) -> tuple[float, float]:
        x, y = pin_pos(self.sch, ref, pin)
        if self.mirror.get(ref) == "y":
            x = round(2 * self.sch.components.get(ref).position.x - x, 2)
        elif self.mirror.get(ref) == "x":
            y = round(2 * self.sch.components.get(ref).position.y - y, 2)
        return (x, y)

    # --- wires -------------------------------------------------------------
    def _pt(self, p) -> tuple[float, float]:
        if isinstance(p[0], str):
            return self.at(*p)
        return (mm(p[0]), mm(p[1]))

    def wire(self, *pts) -> None:
        self.specs.append(pts)  # resolved in resolve(): wires may name pins of parts placed later

    def resolve(self) -> None:
        for pts in self.specs:
            mmp = [self._pt(p) for p in pts]
            for a, b in zip(mmp, mmp[1:]):
                assert a[0] == b[0] or a[1] == b[1], f"{self.name}: diagonal wire {a}->{b} in {pts}"
                if a != b:
                    self.segs.append((a, b))
        self.specs = []

    # --- labels and text ----------------------------------------------------
    @staticmethod
    def _just(rot: int) -> str:
        return "right" if rot in (180, 270) else "left"

    def label(self, name: str, x: float, y: float, rot: int = 0, kind: str = "local") -> None:
        px, py = mm(x), mm(y)
        eff = f'(effects (font (size 1.27 1.27)) (justify {self._just(rot)}))'
        if kind == "local":
            self.raw.append(f'(label "{q(name)}" (at {px} {py} {rot}) {eff} (uuid "{uid()}"))')
        elif kind == "global":
            self.raw.append(f'(global_label "{q(name)}" (shape bidirectional) (at {px} {py} {rot}) (fields_autoplaced yes) {eff} '
                            f'(uuid "{uid()}") (property "Intersheetrefs" "${{INTERSHEET_REFS}}" (at {px} {py} 0) '
                            f'(effects (font (size 1.27 1.27)) hide)))')
        else:
            self.raw.append(f'(hierarchical_label "{q(name)}" (shape bidirectional) (at {px} {py} {rot}) {eff} (uuid "{uid()}"))')

    def text(self, x: float, y: float, body: str, size: float = 1.27, bold: bool = False) -> None:
        b = " bold" if bold else ""
        self.raw.append(f'(text "{q(body)}" (exclude_from_sim no) (at {mm(x)} {mm(y)} 0) '
                        f'(effects (font (size {size} {size}){b}) (justify left top)) (uuid "{uid()}"))')

    def frame(self, x1: float, y1: float, x2: float, y2: float, title: str) -> None:
        """Group box with a caption (dashed rectangle) to show what belongs together."""
        self.raw.append(f'(rectangle (start {mm(x1)} {mm(y1)}) (end {mm(x2)} {mm(y2)}) '
                        f'(stroke (width 0.2) (type dash)) (fill (type none)) (uuid "{uid()}"))')
        self.text(x1 + 0.5, y1 + 0.4, title, 1.5, bold=True)

    def no_connect(self, ref: str, numbers: list[str]) -> None:
        for number in numbers:
            x, y = self.at(ref, number)
            self.raw.append(f'(no_connect (at {x} {y}) (uuid "{uid()}"))')

    # --- output --------------------------------------------------------------
    @staticmethod
    def _inside(p, seg) -> bool:
        a, b = seg
        return ((a[0] == b[0] == p[0] and min(a[1], b[1]) < p[1] < max(a[1], b[1]))
                or (a[1] == b[1] == p[1] and min(a[0], b[0]) < p[0] < max(a[0], b[0])))

    def _junctions(self) -> list[tuple[float, float]]:
        self.resolve()
        cands = {p for s in self.segs for p in s} | self.pins
        out = []
        for p in sorted(cands):
            if not any(p in seg or self._inside(p, seg) for seg in self.segs):
                continue
            deg = 1 if p in self.pins else 0
            for a, b in self.segs:
                if p in (a, b):
                    deg += 1
                elif a[0] == b[0] == p[0] and min(a[1], b[1]) < p[1] < max(a[1], b[1]):
                    deg += 2
                elif a[1] == b[1] == p[1] and min(a[0], b[0]) < p[0] < max(a[0], b[0]):
                    deg += 2
            if deg >= 3:
                out.append(p)
        return out

    def warnings(self) -> list[str]:
        """Pins lying in the middle of a wire (they would connect) and wires touching foreign pins."""
        out = []
        self.resolve()
        for p in self.pins:
            for a, b in self.segs:
                if p not in (a, b) and ((a[0] == b[0] == p[0] and min(a[1], b[1]) < p[1] < max(a[1], b[1]))
                                        or (a[1] == b[1] == p[1] and min(a[0], b[0]) < p[0] < max(a[0], b[0]))):
                    out.append(f"{self.name}: pin at {p} lies on wire {a}-{b}")
        return out

    def save(self, directory: Path, parent_uuid: str, sheet_uuid: str) -> Path:
        path = directory / self.filename
        self.resolve()
        self.sch.save(path)
        txt = path.read_text()
        self._auto_tags()
        txt = self._apply_tags(txt)
        wires = [f'(wire (pts (xy {a[0]} {a[1]}) (xy {b[0]} {b[1]})) (stroke (width 0) (type default)) (uuid "{uid()}"))'
                 for a, b in self.segs]
        juncs = [f'(junction (at {x} {y}) (diameter 0) (color 0 0 0 0) (uuid "{uid()}"))' for x, y in self._junctions()]
        body = "\n\t".join(wires + juncs + self.raw)
        txt = txt.replace("\t(sheet_instances", "\t" + body + "\n\t(sheet_instances", 1)
        for ref, axis in self.mirror.items():
            blocks = txt.split("\t(symbol\n\t\t(lib_id")
            for i, blk in enumerate(blocks[1:], 1):
                if f'(property "Reference" "{ref}"' in blk:
                    blocks[i] = re.sub(r"(\(at [-\d.]+ [-\d.]+ [-\d.]+\))", r"\1\n\t\t(mirror %s)" % axis, blk, count=1)
            txt = "\t(symbol\n\t\t(lib_id".join(blocks)
        txt = re.sub(r'(\(property "LCSC" "[^"]*"\s*\(at [^\n]*\n\s*\(effects\s*\(font\s*\(size [\d. ]+\)\s*\)\s*\(justify left\))', r"\1 (hide yes)", txt)
        txt = txt.replace(f'(path "/{self.sch.uuid}"', f'(path "/{parent_uuid}/{sheet_uuid}"')
        path.write_text(txt)
        return path


def root_sheet(directory: Path, name: str, sheets: list[dict], texts: list[tuple[float, float, str]], wires, labels) -> tuple[Path, str]:
    """Root page: block diagram of sheet symbols. Each entry of sheets: name, file, x, y, w, h, page, pins=[(edge, y_cell, name)]."""
    root = str(uuid.uuid5(uuid.NAMESPACE_URL, "root"))
    out = ['(kicad_sch (version 20250114) (generator "eeschema") (generator_version "9.0")', f'\t(uuid "{root}")', '\t(paper "A3")',
           '\t(title_block (title "desk notifier v1") (date "2026-10-15") (rev "v1") (company "desk_notifier") '
           '(comment 1 "Sheet: root (block diagram)"))', "\t(lib_symbols)"]
    for w in wires:
        (x1, y1), (x2, y2) = w
        out.append(f'\t(wire (pts (xy {mm(x1)} {mm(y1)}) (xy {mm(x2)} {mm(y2)})) (stroke (width 0) (type default)) (uuid "{uid()}"))')
    for text, x, y in labels:
        out.append(f'\t(label "{text}" (at {mm(x)} {mm(y)} 0) (effects (font (size 1.27 1.27)) (justify left bottom)) (uuid "{uid()}"))')
    for sh in sheets:
        x, y, w, h = (mm(sh[k]) for k in ("x", "y", "w", "h"))
        s = [f'\t(sheet (at {x} {y}) (size {w} {h}) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (fields_autoplaced yes)',
             '\t\t(stroke (width 0.2) (type solid)) (fill (color 255 255 200 0.5))', f'\t\t(uuid "{sh["uuid"]}")',
             f'\t\t(property "Sheetname" "{sh["name"]}" (at {x} {y - 1.0} 0) (effects (font (size 2 2) bold) (justify left bottom)))',
             f'\t\t(property "Sheetfile" "{sh["file"]}" (at {x} {y + h + 1.0} 0) (effects (font (size 1.27 1.27)) (justify left top)))']
        for edge, yc, pname in sh["pins"]:
            px = x if edge == "left" else round(x + w, 2)
            rot, just = (180, "left") if edge == "left" else (0, "right")
            s.append(f'\t\t(pin "{pname}" bidirectional (at {px} {mm(yc)} {rot}) (uuid "{uid()}") '
                     f'(effects (font (size 1.27 1.27)) (justify {just})))')
        s.append(f'\t\t(instances (project "notifier-v1" (path "/{root}" (page "{sh["page"]}"))))')
        s.append("\t)")
        out.append("\n".join(s))
    for x, y, body in texts:
        size = 2.0 if body.startswith("#") else 1.5
        body = body.lstrip("#")
        out.append(f'\t(text "{q(body)}" (exclude_from_sim no) (at {mm(x)} {mm(y)} 0) '
                   f'(effects (font (size {size} {size})) (justify left top)) (uuid "{uid()}"))')
    out.append('\t(sheet_instances (path "/" (page "1")))')
    out.append("\t(embedded_fonts no)\n)")
    path = directory / "notifier-v1.kicad_sch"
    path.write_text("\n".join(out) + "\n")
    return path, root


def netlist(sch_file: Path) -> dict[str, set[tuple[str, str]]]:
    """KiCad's netlist keyed by the last path component: '/Power/CC1', '/USB_DP' and 'GND' -> 'CC1', 'USB_DP', 'GND'."""
    net_file = sch_file.with_suffix(".net")
    subprocess.run(["kicad-cli", "sch", "export", "netlist", "-o", str(net_file), str(sch_file)],
                   check=True, capture_output=True)
    text = net_file.read_text().split("(nets", 1)[1]
    out = {}
    for blk in text.split("(net\n")[1:]:
        name = re.search(r'\(name "([^"]+)"', blk).group(1).split("/")[-1]
        pins = set(re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"', blk))
        if name in out:
            name += "!dup"
        out[name] = pins
    return out


def check_nets(sch_file: Path, nets: dict[str, list[tuple[str, str]]]) -> list[str]:
    """Differences between intended nets and KiCad's netlist (power symbols/flags ignored), also extra nets."""
    actual = netlist(sch_file)
    errors = []
    for net, pins in nets.items():
        want = {p for p in pins if not p[0].startswith("#")}
        got = actual.get(net, set())
        if want != got:
            errors.append(f"{net}: missing {sorted(want - got)} extra {sorted(got - want)}")
    for name, pins in actual.items():
        if name not in nets and not name.startswith("unconnected-") and pins:
            errors.append(f"unexpected net {name}: {sorted(pins)}")
    return errors
