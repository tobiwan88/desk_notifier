"""Build the visual review pack for a board: schematic tiles, per-layer copper, 3D views, region crops.

Writes PNGs plus index.json (image -> what to look at) to hardware/out/review/<board>/.
Region crops come from REGIONS (board mm from the top-left corner); the Haiku checks
(hardware/agents/) each get one image and one question.
Usage: uv run python hardware/tools/review_pack.py hardware/notifier-v1/notifier-v1.kicad_pcb
"""

import json
import subprocess
import sys
from pathlib import Path

import pymupdf

DPI = 300
REGIONS = {  # name: (x0, y0, x1, y1) in board mm, question focus
    "antenna": ((8, 0, 42, 6), "antenna edge: no copper, parts or tracks next to the module antenna at the top edge"),
    "usb": ((6, 21, 26, 36), "USB-C: CC resistors, ESD next to the connector, D+/D- routed as a short pair"),
    "power": ((20, 22, 40, 36), "regulator: input/output caps next to VI/VO, wide VBUS/3V3 tracks"),
    "sensor": ((0, 0, 16, 14), "BME688 island: slot, decoupling caps next to VDD/VDDIO, no pour on the island"),
    "module_left": ((10, 0, 20, 20), "module 3V3/EN pins: 22u+100n next to pin 2, EN RC next to pin 3"),
    "display": ((32, 8, 50, 30), "display socket: pin 1 marking, LCD_VCC jumper, signal routing"),
}
LAYER_SETS = {
    "top": "F.Cu,F.SilkS,F.Fab,Edge.Cuts",
    "in1_gnd": "In1.Cu,Edge.Cuts",
    "in2_3v3": "In2.Cu,Edge.Cuts",
    "bottom": "B.Cu,B.SilkS,Edge.Cuts",
    "copper_outer": "F.Cu,B.Cu,Edge.Cuts",
}


def run(*args):
    subprocess.run(args, check=True, capture_output=True)


def board_box(pcb: Path) -> tuple[float, float, float, float]:
    """Edge.Cuts bounding box in page mm (from the board file, no pcbnew needed)."""
    import re
    xs, ys = [], []
    text = pcb.read_text()
    for m in re.finditer(r'\(gr_line\s*\(start ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)(?:(?!\(gr_).)*?\(layer "Edge\.Cuts"\)', text, re.S):
        xs += [float(m.group(1)), float(m.group(3))]
        ys += [float(m.group(2)), float(m.group(4))]
    return min(xs), min(ys), max(xs), max(ys)


def render_pdf(pdf: Path, png: Path, clip_mm=None, dpi=DPI):
    page = pymupdf.open(pdf)[0]
    scale = dpi / 25.4  # px per mm
    clip = None
    if clip_mm:
        k = 72 / 25.4
        clip = pymupdf.Rect(clip_mm[0] * k, clip_mm[1] * k, clip_mm[2] * k, clip_mm[3] * k)
    page.get_pixmap(matrix=pymupdf.Matrix(scale * 25.4 / 72, scale * 25.4 / 72), clip=clip).save(png)


def main():
    pcb = Path(sys.argv[1])
    sch = pcb.with_suffix(".kicad_sch")
    out = Path("hardware/out/review") / pcb.stem
    out.mkdir(parents=True, exist_ok=True)
    index = []

    # schematic: full page and 2x2 tiles
    pdf = out / "schematic.pdf"
    run("kicad-cli", "sch", "export", "pdf", "-o", str(pdf), str(sch))
    page = pymupdf.open(pdf)[0]
    r = page.rect
    for i in range(2):
        for j in range(2):
            png = out / f"sch_{j}{i}.png"
            clip = pymupdf.Rect(r.x0 + i * r.width / 2, r.y0 + j * r.height / 2, r.x0 + (i + 1) * r.width / 2, r.y0 + (j + 1) * r.height / 2)
            page.get_pixmap(matrix=pymupdf.Matrix(2.5, 2.5), clip=clip).save(png)
            index.append({"image": png.name, "kind": "schematic", "focus": f"schematic tile row {j} col {i}"})

    # PCB layers, cropped to the board (plots keep page coordinates)
    x0, y0, x1, y1 = board_box(pcb)
    for name, layers in LAYER_SETS.items():
        lpdf = out / f"{name}.pdf"
        run("kicad-cli", "pcb", "export", "pdf", "--layers", layers, "--mode-single", "-o", str(lpdf), str(pcb))
        png = out / f"layer_{name}.png"
        render_pdf(lpdf, png, (x0 - 1, y0 - 8, x1 + 1, y1 + 1), dpi=250)
        index.append({"image": png.name, "kind": "layer", "focus": layers})
        if name == "top":
            for region, ((rx0, ry0, rx1, ry1), focus) in REGIONS.items():
                png = out / f"region_{region}.png"
                render_pdf(lpdf, png, (x0 + rx0, y0 + ry0, x0 + rx1, y0 + ry1), dpi=600)
                index.append({"image": png.name, "kind": "region", "focus": focus})

    # 3D views
    for side, extra in (("top", []), ("bottom", []), ("iso", ["--rotate", "-45,0,30", "--perspective"])):
        png = out / f"3d_{side}.png"
        run("kicad-cli", "pcb", "render", "--side", "bottom" if side == "bottom" else "top", "--quality", "high",
            "-w", "1600", "-h", "1200", *extra, "-o", str(png), str(pcb))
        index.append({"image": png.name, "kind": "3d", "focus": f"3D {side}"})

    (out / "index.json").write_text(json.dumps(index, indent=1))
    print(f"{out}: {len(index)} images")


if __name__ == "__main__":
    main()
