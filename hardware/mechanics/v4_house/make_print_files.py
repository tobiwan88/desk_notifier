"""Print file for housing v4 House (A-frame v4d): one Bambu Studio plate with the whole housing (3MF) plus per-object STLs.

housing_v4d.3mf: FACE (front down), BODY (rear down) and the roof CAP (rim down, tilted 9.2 deg so the rim is flat), each a
separate object so the roof can get its own filament (AMS) or be printed after a filament change. Fits a 180 x 180 bed.

Run: uv run --with manifold3d --with trimesh --with numpy --with networkx --with lxml --with shapely --with rtree python -I make_print_files.py
"""
import os

import numpy as np
import trimesh
from manifold3d import Manifold, Mesh

HERE = os.path.dirname(os.path.abspath(__file__))
STL = os.path.join(HERE, "stl")
OUT = os.path.join(HERE, "print")
PLATE = 180.0      # fits the smallest Bambu bed (A1 mini 180 x 180); X1/P1/A1 are 256
GAP = 8.0          # spacing between objects (room for brims)


def load(name, kind="print"):
    tm = trimesh.load(os.path.join(STL, f"aframe_b_{name}_{kind}.stl"), process=True)   # merge STL vertices
    tm.merge_vertices()
    return tm


def to_manifold(tm):
    return Manifold(Mesh(vert_properties=np.asarray(tm.vertices, dtype=np.float32),
                         tri_verts=np.asarray(tm.faces, dtype=np.uint32)))


def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts),
                           process=True)


def drop_to_bed(tm):
    tm = tm.copy()
    tm.apply_translation(-tm.bounds[0])
    return tm


def arrange(objs):
    """Row packing on the plate, then centre the group."""
    x = y = row_h = 0.0
    placed = []
    for name, tm in objs:
        w, d = tm.extents[0], tm.extents[1]
        if x > 0 and x + w > PLATE - 12:
            x, y, row_h = 0.0, y + row_h + GAP, 0.0
        t = tm.copy()
        t.apply_translation([x, y, 0])
        placed.append((name, t))
        x += w + GAP
        row_h = max(row_h, d)
    lo = np.min([t.bounds[0] for _, t in placed], axis=0)
    hi = np.max([t.bounds[1] for _, t in placed], axis=0)
    if hi[0] - lo[0] > PLATE - 6 or hi[1] - lo[1] > PLATE - 6:
        raise SystemExit(f"plate overflow: {hi - lo}")
    shift = np.array([(PLATE - (hi[0] - lo[0])) / 2 - lo[0], (PLATE - (hi[1] - lo[1])) / 2 - lo[1], 0])
    for _, t in placed:
        t.apply_translation(shift)
    return placed


def report(name, tm):
    z0 = tm.bounds[0][2]
    sec = tm.section(plane_origin=[0, 0, z0 + 0.1], plane_normal=[0, 0, 1])
    area = sec.to_2D()[0].area if sec is not None else 0.0
    print(f"  {name:10s} {tm.extents[0]:5.1f} x {tm.extents[1]:5.1f} x {tm.extents[2]:5.1f} mm  "
          f"watertight {tm.is_watertight}  bodies {len(tm.split(only_watertight=False))}  "
          f"first layer {area:6.0f} mm2  volume {tm.volume / 1000:5.2f} cm3")


def write_plate(fname, placed):
    scene = trimesh.Scene()
    for name, t in placed:
        scene.add_geometry(t, node_name=name, geom_name=name)
        t.export(os.path.join(OUT, f"{fname}_{name}.stl"))
    scene.export(os.path.join(OUT, f"{fname}.3mf"))
    print(f"wrote {fname}.3mf and {len(placed)} STLs")


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):                      # old plates (v4b coupons) go, PRINT.md stays
        if f.endswith((".3mf", ".stl")):
            os.remove(os.path.join(OUT, f))
    parts = [(n, drop_to_bed(load(src))) for n, src in (("face", "face"), ("body", "body"), ("roof", "cap"))]
    print("housing plate:")
    for n, t in parts:
        report(n, t)
    write_plate("housing_v4d", arrange(parts))


if __name__ == "__main__":
    main()
