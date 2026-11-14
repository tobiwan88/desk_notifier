"""Print files for housing v3 Moon (C4c): Bambu Studio plates as 3MF plus per-object STLs.

Plate 1: FACE, PLINTH, BACK in print orientation.
Plate 2: coupons cut from the real PLINTH in its print orientation, so layers run as in the final part:
  C6  rear end of the plinth (USB-C opening and plug pocket), the top 14 mm of the standing plinth;
  C7  20 mm plinth slice with shelves, hold-down ribs and the board slot.
The heat test (C8) uses the full PLINTH from plate 1.

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
GAP = 8.0          # spacing between objects (brim 5 mm on the plinth needs room)


def load(name):
    tm = trimesh.load(os.path.join(STL, f"moon4c_{name}_print.stl"), process=True)   # merge STL vertices
    tm.merge_vertices()
    return tm


def to_manifold(tm):
    return Manifold(Mesh(vert_properties=np.asarray(tm.vertices, dtype=np.float32),
                         tri_verts=np.asarray(tm.faces, dtype=np.uint32)))


def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts),
                           process=True)


def z_band(tm, z0, z1):
    """Keep the part between print heights z0 and z1; the cut at z0 becomes the new bed face."""
    lo, hi = tm.bounds
    box = Manifold.cube([hi[0] - lo[0] + 20, hi[1] - lo[1] + 20, z1 - z0]).translate([lo[0] - 10, lo[1] - 10, z0])
    return to_trimesh(to_manifold(tm) ^ box)


def drop_to_bed(tm):
    tm = tm.copy()
    tm.apply_translation(-np.array([tm.bounds[0][0], tm.bounds[0][1], tm.bounds[0][2]]))
    return tm


def arrange(objs):
    """Simple row packing on the plate, origin at the plate corner."""
    x = y = row_h = 0.0
    placed = []
    for name, tm in objs:
        w, d = tm.extents[0], tm.extents[1]
        if x > 0 and x + w > PLATE:
            x, y, row_h = 0.0, y + row_h + GAP, 0.0
        t = tm.copy()
        t.apply_translation([x + 6, y + 6, 0])     # 6 mm margin from the plate edge
        placed.append((name, t))
        x += w + GAP
        row_h = max(row_h, d)
    used = max(t.bounds[1][0] for _, t in placed), max(t.bounds[1][1] for _, t in placed)
    if used[0] > PLATE or used[1] > PLATE:
        raise SystemExit(f"plate overflow: {used}")
    # centre the group on the plate
    lo = np.min([t.bounds[0] for _, t in placed], axis=0)
    hi = np.max([t.bounds[1] for _, t in placed], axis=0)
    shift = np.array([(PLATE - (hi[0] - lo[0])) / 2 - lo[0], (PLATE - (hi[1] - lo[1])) / 2 - lo[1], 0])
    for _, t in placed:
        t.apply_translation(shift)
    return placed


def report(name, tm):
    z0 = tm.bounds[0][2]
    sec = tm.section(plane_origin=[0, 0, z0 + 0.1], plane_normal=[0, 0, 1])
    area = sec.to_2D()[0].area if sec is not None else 0.0
    print(f"  {name:8s} {tm.extents[0]:5.1f} x {tm.extents[1]:5.1f} x {tm.extents[2]:5.1f} mm  "
          f"watertight {tm.is_watertight}  first layer {area:6.0f} mm2  volume {tm.volume / 1000:5.2f} cm3")


def write_plate(fname, placed):
    scene = trimesh.Scene()
    for name, t in placed:
        scene.add_geometry(t, node_name=name, geom_name=name)
        t.export(os.path.join(OUT, f"{fname}_{name}.stl"))
    scene.export(os.path.join(OUT, f"{fname}.3mf"))
    print(f"wrote {fname}.3mf and {len(placed)} STLs")


def main():
    os.makedirs(OUT, exist_ok=True)
    face, plinth, back = (drop_to_bed(load(n)) for n in ("face", "plinth", "back"))

    print("plate 1 (parts):")
    parts = [("face", face), ("plinth", plinth), ("back", back)]
    for n, t in parts:
        report(n, t)
    write_plate("plate1_parts", arrange(parts))

    print("plate 2 (coupons):")
    h = plinth.extents[2]
    c6 = drop_to_bed(z_band(plinth, h - 14.0, h + 1.0))      # rear end: USB opening, plug pocket, rear roof
    c7 = drop_to_bed(z_band(plinth, 15.0, 35.0))             # mid slice: shelves, ribs, board slot
    coupons = [("C6_usb", c6), ("C7_slice", c7)]
    for n, t in coupons:
        report(n, t)
    write_plate("plate2_coupons", arrange(coupons))


if __name__ == "__main__":
    main()
