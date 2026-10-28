"""Read a board profile and a .kicad_pcb without pcbnew (plain Python, survives the KiCad 11 SWIG removal).

Board profile = one folder per board: board.yaml, critical.yaml, parts/*.yaml, the .kicad_pcb named in board.yaml.
Geometry: pad centres in board coordinates (mm), track length per net, via count per net.
"""

import math
import re
from dataclasses import dataclass, field
from pathlib import Path

import yaml

TOKEN = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+')


def parse(text: str) -> list:
    """S-expression -> nested lists of strings."""
    stack: list[list] = [[]]
    for tok in TOKEN.findall(text):
        if tok == "(":
            stack.append([])
        elif tok == ")":
            done = stack.pop()
            stack[-1].append(done)
        else:
            stack[-1].append(tok[1:-1] if tok.startswith('"') else tok)
    return stack[0][0]


def child(node: list, name: str) -> list | None:
    return next((c for c in node[1:] if isinstance(c, list) and c and c[0] == name), None)


def children(node: list, name: str) -> list[list]:
    return [c for c in node[1:] if isinstance(c, list) and c and c[0] == name]


def xy(node: list) -> tuple[float, float, float]:
    return float(node[1]), float(node[2]), float(node[3]) if len(node) > 3 else 0.0


@dataclass
class Board:
    pads: dict[tuple[str, str], tuple[float, float]] = field(default_factory=dict)   # (ref, pad) -> centre
    pad_nets: dict[tuple[str, str], str] = field(default_factory=dict)
    footprints: dict[str, tuple[float, float]] = field(default_factory=dict)          # ref -> position
    values: dict[str, str] = field(default_factory=dict)                              # ref -> Value field
    length: dict[str, float] = field(default_factory=dict)                            # net -> routed length (mm)
    layers: dict[str, set] = field(default_factory=dict)                              # net -> copper layers used
    vias: dict[str, int] = field(default_factory=dict)                                # net -> via count

    def pad(self, ref: str, num: str) -> tuple[float, float]:
        try:
            return self.pads[(ref, num)]
        except KeyError:
            raise KeyError(f"no pad {ref}.{num} on the board") from None

    def dist(self, a: tuple[str, str], b: tuple[str, str]) -> float:
        """Pad centre to pad centre (mm)."""
        return math.dist(self.pad(*a), self.pad(*b))

    def nearest(self, ref: str, net: str, to: tuple[float, float]) -> float:
        """Distance from a point to the nearest pad of `ref` on `net`."""
        pts = [p for (r, _), p in self.pads.items() if r == ref and self.pad_nets.get((r, _)) == net]
        if not pts:
            raise KeyError(f"{ref} has no pad on net {net}")
        return min(math.dist(p, to) for p in pts)


def net_name(node: list) -> str:
    n = child(node, "net")
    return n[-1] if n else ""


def load_pcb(path: Path) -> Board:
    root = parse(path.read_text())
    b = Board()
    for fp in children(root, "footprint"):
        fx, fy, frot = xy(child(fp, "at"))
        props = {p[1]: p[2] for p in children(fp, "property")}
        ref = props["Reference"]
        b.footprints[ref] = (fx, fy)
        b.values[ref] = props.get("Value", "")
        a = math.radians(frot)
        for pad in children(fp, "pad"):
            px, py, _ = xy(child(pad, "at"))
            # KiCad: pad position is footprint-local; rotate by the footprint angle (y axis points down)
            b.pads[(ref, pad[1])] = (round(fx + px * math.cos(a) + py * math.sin(a), 4),
                                     round(fy - px * math.sin(a) + py * math.cos(a), 4))
            b.pad_nets[(ref, pad[1])] = net_name(pad)
    for kind in ("segment", "arc"):
        for t in children(root, kind):
            net = net_name(t)
            pts = [xy(child(t, k))[:2] for k in (("start", "mid", "end") if kind == "arc" else ("start", "end"))]
            seg = sum(math.dist(p, q) for p, q in zip(pts, pts[1:]))   # arc: chord approximation
            b.length[net] = b.length.get(net, 0.0) + seg
            b.layers.setdefault(net, set()).add(child(t, "layer")[1])
    for v in children(root, "via"):
        net = net_name(v)
        b.vias[net] = b.vias.get(net, 0) + 1
    return b


SI = {"p": 1e-12, "n": 1e-9, "u": 1e-6, "µ": 1e-6, "m": 1e-3, "k": 1e3, "M": 1e6, "R": 1.0}


def si(value: str) -> float:
    """Component value -> number: 100k, 4k7, 2.2u, 22uF, 0R1 (unit letters after the prefix are ignored)."""
    m = re.fullmatch(r"(\d+)(?:([.,]\d+)|([pnuµmkMR])(\d*))?\s*([pnuµmkM]?)[A-Za-zΩ]*", value.strip())
    if not m:
        raise ValueError(f"cannot read component value {value!r}")
    whole, frac, mid, tail, pre = m.groups()
    num = float(whole + (frac or "").replace(",", ".") + (f".{tail}" if tail else ""))
    return num * SI[mid or pre or "R"]


def load_profile(board_dir: Path) -> dict:
    """board.yaml + critical.yaml + parts/*.yaml of one board folder."""
    board_dir = Path(board_dir)
    prof = yaml.safe_load((board_dir / "board.yaml").read_text())
    crit = board_dir / "critical.yaml"
    prof["critical"] = yaml.safe_load(crit.read_text()) if crit.exists() else {}
    prof["parts"] = {p.stem: yaml.safe_load(p.read_text()) for p in sorted((board_dir / "parts").glob("*.yaml"))}
    prof["dir"] = board_dir
    return prof


def load(board_dir: Path) -> tuple[dict, Board]:
    prof = load_profile(board_dir)
    return prof, load_pcb(Path(board_dir) / prof["pcb"])


def report(rows: list[tuple[str, str, str]], title: str) -> int:
    """Print `status  check  detail` rows; return the number of FAIL rows."""
    print(title)
    for status, name, detail in rows:
        print(f"  {status:7} {name:34} {detail}")
    return sum(r[0] == "FAIL" for r in rows)
