"""Export the assembled notifier-v1 board from KiCad for the housing fit checks.

Writes (in this folder):
  pcb_board_frame.npz  per reference designator: convex-hull points of its 3D model, plus "board" (bare PCB
                       outline, extruded) -- board frame: x along the 50 mm edge from the USB-C edge (left in KiCad),
                       y along the 36 mm edge from the module/antenna edge (top in KiCad), z up from the PCB underside
  notifier_v1_pcb_board_frame.stl  the whole board with all models (for viewing)
aframe_b.py loads the npz, places it in the housing frame and checks the fit.

Needs kicad-cli (source scripts/kicad-env.sh). KiCad's bundled 3D library may lack some models (here the USB-C
receptacle USB_C_Receptacle_HRO_TYPE-C-31-M-12); pass --extra-3d DIR with <Lib>.3dshapes/<name>.step files to fill gaps.
Run: uv run --with trimesh --with numpy --with scipy python -I export_pcb.py [--extra-3d DIR]
"""
import argparse
import os
import re
import subprocess
import tempfile

import numpy as np
import trimesh
from scipy.spatial import ConvexHull

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.normpath(os.path.join(HERE, "..", "..", "..", "notifier-v1", "notifier-v1.kicad_pcb"))
APP_3D = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/3dmodels"
OX, OY = 100.0, 100.0            # board origin on the KiCad page (hardware/gen/layout_v1.py)


def model_dir(extra):
    """A merged 3D library: KiCad's own plus the extra one (extra wins)."""
    d = tempfile.mkdtemp(prefix="kicad3d_")
    for src in [APP_3D] + ([extra] if extra else []):
        if not os.path.isdir(src):
            continue
        for lib in os.listdir(src):
            dst = os.path.join(d, lib)
            if os.path.islink(dst):
                os.unlink(dst)
            if os.path.isdir(dst):
                continue
            if src == extra:
                os.makedirs(dst, exist_ok=True)
                for f in os.listdir(os.path.join(src, lib)):
                    base, ext = os.path.splitext(f)
                    os.symlink(os.path.join(src, lib, f), os.path.join(dst, base + ext.lower()))
            else:
                os.symlink(os.path.join(src, lib), dst)
    return d


def export(out, d3, *args):
    r = subprocess.run(["kicad-cli", "pcb", "export", "stl", "-f", "-D", f"KICAD10_3DMODEL_DIR={d3}", "-o", out, *args, PCB],
                       capture_output=True, text=True)
    missing = re.findall(r"Could not add 3D model for (\S+)\.", r.stdout + r.stderr)
    return os.path.exists(out) and os.path.getsize(out) > 84, missing


def to_board(tm):
    """KiCad page frame (y down on the page = -y in the STL) to the board frame; a mirror, so flip faces when writing a mesh."""
    v = tm.vertices
    return np.c_[v[:, 0] - OX, -v[:, 1] - OY, v[:, 2]]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--extra-3d")
    a = ap.parse_args()
    d3 = model_dir(a.extra_3d)
    refs = sorted(set(re.findall(r'\(property "Reference" "([A-Z]+[0-9]+)"', open(PCB).read())))
    tmp = tempfile.mkdtemp(prefix="pcbstl_")
    data, missing_all = {}, set()
    ok, miss = export(os.path.join(tmp, "all.stl"), d3)
    missing_all |= set(miss)
    full = trimesh.load(os.path.join(tmp, "all.stl"))
    trimesh.Trimesh(to_board(full), full.faces[:, ::-1], process=False).export(os.path.join(HERE, "notifier_v1_pcb_board_frame.stl"))
    export(os.path.join(tmp, "board.stl"), d3, "--board-only")
    data["board"] = to_board(trimesh.load(os.path.join(tmp, "board.stl")))
    for r in refs:
        f = os.path.join(tmp, f"{r}.stl")
        ok, miss = export(f, d3, "--no-board-body", "--component-filter", r)
        missing_all |= set(miss)
        if not ok:
            continue
        p = to_board(trimesh.load(f))
        if len(p) >= 4:
            data[r] = p[ConvexHull(p).vertices]
    np.savez(os.path.join(HERE, "pcb_board_frame.npz"), **data)
    b = data["board"]
    print(f"board {np.ptp(b[:, 0]):.2f} x {np.ptp(b[:, 1]):.2f} x {np.ptp(b[:, 2]):.2f} mm; {len(data) - 1} parts with models; "
          f"missing models: {', '.join(sorted(missing_all)) or 'none'}")
    for r in ("J1", "J2", "U1", "U4", "U5"):
        if r in data:
            lo, hi = data[r].min(0), data[r].max(0)
            print(f"  {r:3s} x {lo[0]:6.2f}..{hi[0]:6.2f}  y {lo[1]:6.2f}..{hi[1]:6.2f}  z {lo[2]:5.2f}..{hi[2]:5.2f}")


if __name__ == "__main__":
    main()
