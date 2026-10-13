"""JLCPCB fab package: gerbers + drill zip, BOM and CPL in JLC's column format.

Parts without an LCSC field (holes, test pads, solder jumper) are left out of BOM and CPL.
Rotation corrections follow the JLCKicadTools cpl_rotations_db (via kicad-jlcpcb-tools); they
date from older KiCad libraries, so always confirm orientation in JLC's placement preview.
Usage: uv run python hardware/tools/fab_package.py hardware/notifier-v1/notifier-v1.kicad_pcb
"""

import csv
import re
import subprocess
import sys
import zipfile
from collections import defaultdict
from pathlib import Path

LAYERS = "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"
ROTATION = [  # (regex on footprint name, degrees added)
    (r"^USB_C_Receptacle_HRO_TYPE-C-31-M-12", 180),
    (r"^SOT-223", 180),
    (r"^SOT-23", -90),
]


def run(*args):
    subprocess.run(args, check=True, capture_output=True)


def lcsc_fields(net_file: Path) -> dict[str, dict]:
    text = net_file.read_text().split("(nets", 1)[0]
    parts = {}
    for blk in text.split("(comp\n")[1:]:
        ref = re.search(r'\(ref "([^"]+)"', blk).group(1)
        lcsc = re.search(r'\(name "LCSC"\)\s*"([^"]*)"', blk)
        parts[ref] = {"value": re.search(r'\(value "([^"]*)"', blk).group(1),
                      "footprint": re.search(r'\(footprint "([^"]*)"', blk).group(1),
                      "lcsc": lcsc.group(1) if lcsc else ""}
    return parts


def main():
    pcb = Path(sys.argv[1])
    out = Path("hardware/out/fab") / pcb.stem
    gerbers = out / "gerbers"
    gerbers.mkdir(parents=True, exist_ok=True)
    for f in gerbers.iterdir():
        f.unlink()
    run("kicad-cli", "pcb", "export", "gerbers", "--layers", LAYERS, "--subtract-soldermask", "--check-zones",
        "-o", str(gerbers) + "/", str(pcb))
    run("kicad-cli", "pcb", "export", "drill", "--format", "excellon", "--excellon-separate-th", "--generate-map",
        "--map-format", "gerberx2", "-o", str(gerbers) + "/", str(pcb))
    zpath = out / f"{pcb.stem}-gerbers.zip"
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(gerbers.iterdir()):
            z.write(f, f.name)

    net = pcb.with_suffix(".net")
    run("kicad-cli", "sch", "export", "netlist", "-o", str(net), str(pcb.with_suffix(".kicad_sch")))
    parts = lcsc_fields(net)

    # BOM grouped by LCSC number
    groups = defaultdict(list)
    for ref, p in parts.items():
        if p["lcsc"]:
            groups[(p["lcsc"], p["footprint"])].append(ref)
    with open(out / f"{pcb.stem}-bom.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (lcsc, fp), refs in sorted(groups.items(), key=lambda g: g[1][0]):
            values = sorted({parts[r]["value"] for r in refs})
            w.writerow(["/".join(values), ",".join(sorted(refs)), fp.split(":")[1], lcsc])

    # CPL from KiCad's position file, rotation-corrected
    pos = out / "pos.csv"
    run("kicad-cli", "pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "-o", str(pos), str(pcb))
    rows = list(csv.DictReader(pos.open()))
    with open(out / f"{pcb.stem}-cpl.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for r in rows:
            ref = r["Ref"]
            if not parts.get(ref, {}).get("lcsc"):
                continue
            fpname = parts[ref]["footprint"].split(":")[1]
            rot = float(r["Rot"])
            side = "Top" if r["Side"] == "top" else "Bottom"
            if side == "Bottom":
                rot = 180 - rot
            rot += next((d for pat, d in ROTATION if re.search(pat, fpname)), 0)
            w.writerow([ref, f'{float(r["PosX"]):.4f}mm', f'{float(r["PosY"]):.4f}mm', side, f"{rot % 360:g}"])
    pos.unlink()
    print(f"{out}: {zpath.name}, {len(groups)} BOM lines, {sum(len(v) for v in groups.values())} placements")


if __name__ == "__main__":
    main()
