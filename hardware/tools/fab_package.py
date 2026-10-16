"""JLCPCB fab package: gerbers + drill zip, BOM and CPL in JLC's column format.

Parts without an LCSC field (holes, test pads, solder jumper) are left out of BOM and CPL.
Rotation corrections follow the JLCKicadTools cpl_rotations_db (via kicad-jlcpcb-tools); they
date from older KiCad libraries, so always confirm orientation in JLC's placement preview.
Usage: uv run --package desk-hardware python hardware/tools/fab_package.py [--require-clean] hardware/notifier-v1/notifier-v1.kicad_pcb
Only gitignored files are written (hardware/out/, the .net next to the board); the board itself is only read.
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
    # Same pin layout as the community entry for the Bosch 2 x 2.5 mm LGA (pin 1 top left at 0 deg, clockwise,
    # rows along x): applied to the 3 x 3 mm BME688 footprint. Confirm in JLC's preview.
    (r"^Bosch_LGA-8_3x3mm_P0.8mm_ClockwisePinNumbering", 90),
    # Sensirion_DFN-4 (SHT40): pin columns left/right, pin 1 top left, 1-2 down the left column, 3-4 up the right one:
    # the same topology as the KiCad DFN-* footprints that the community entry `^DFN-` (270) corrects.
    (r"^Sensirion_DFN-4", 270),
]


def git_sha(require_clean: bool = False) -> str:
    """Short HEAD SHA for the silkscreen; '-dirty' (and a warning) when hardware/ has uncommitted changes.

    With require_clean (the ORDER step) a dirty tree is an error instead.
    """
    git = lambda *a: subprocess.run(["git", *a], check=True, capture_output=True, text=True).stdout.strip()
    sha = git("rev-parse", "--short", "HEAD")
    if git("status", "--porcelain", "--", "hardware"):
        if require_clean:
            sys.exit("uncommitted changes under hardware/: commit them before building the order package")
        print(f"WARNING: uncommitted changes under hardware/: silk shows {sha}-dirty. Commit, then rerun before ordering.")
        sha += "-dirty"
    return sha


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
    args = [a for a in sys.argv[1:] if a != "--require-clean"]
    pcb = Path(args[0])
    out = Path("hardware/out/fab") / pcb.stem
    gerbers = out / "gerbers"
    gerbers.mkdir(parents=True, exist_ok=True)
    for f in gerbers.iterdir():
        f.unlink()
    sha = git_sha("--require-clean" in sys.argv)
    # JLC's KiCad guide: Protel extensions (default), subtract mask from silk, no X2, no netlist attributes;
    # drill Excellon in mm, decimal zeros, PTH and NPTH in separate files, no map file.
    run("kicad-cli", "pcb", "export", "gerbers", "--layers", LAYERS, "--subtract-soldermask", "--check-zones",
        "--no-x2", "--no-netlist", "-D", f"GIT_SHA={sha}", "-o", str(gerbers) + "/", str(pcb))
    run("kicad-cli", "pcb", "export", "drill", "--format", "excellon", "--excellon-separate-th", "--excellon-units", "mm",
        "--excellon-zeros-format", "decimal", "--drill-origin", "absolute", "-o", str(gerbers) + "/", str(pcb))
    for f in gerbers.glob("*.gbrjob"):
        f.unlink()
    (out / "VERSION.txt").write_text(f"{pcb.stem} {sha}\n")
    zpath = out / f"{pcb.stem}-{sha}-gerbers.zip"
    for old in out.glob(f"{pcb.stem}-*gerbers.zip"):
        old.unlink()
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(gerbers.iterdir()):
            z.write(f, f.name)

    net = pcb.with_suffix(".net")  # gitignored (hardware/notifier-v1/*.net): writing it keeps the tree clean
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
