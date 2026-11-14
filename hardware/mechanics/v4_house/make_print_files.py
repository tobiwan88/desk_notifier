"""Print files for housing v4 House (A-frame v4b): Bambu Studio plates as 3MF plus per-object STLs.

Plate 1 (main colour, e.g. warm grey): FACE and BODY in print orientation.
Plate 2 (roof colour, e.g. terracotta): the roof CAP in print orientation (rim down, tilted 9.2 deg).
Plate 3 (coupon): the BODY and roof CAP between depths Y 29 and 46 (assembled frame), which holds one
  left/right pair of roof click fingers (centred Y 38) and the matching roof grooves, to try the click.

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
COUPON_Y0, COUPON_Y1 = 29.0, 46.0   # click coupon band: one finger pair (aframe_b.py F_YC 38) and its roof grooves
FRONT_BAND_Y1 = 16.0                 # body front band for the FACE arm coupon (arms reach Y 10.6, grooves behind)


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


def band_slice(tm, y0, y1):
    """Keep the assembled part between depths y0 and y1 (Y is depth, front = 0, rear = max Y).
    It stands on its rear cut face (Y = y1), the same direction the BODY prints (rear face down)."""
    lo, hi = tm.bounds
    box = Manifold.cube([hi[0] - lo[0] + 20, y1 - y0, hi[2] - lo[2] + 20]).translate([lo[0] - 10, y0, lo[2] - 10])
    part = to_trimesh(to_manifold(tm) ^ box)
    # rotate -90 deg about X: z' = -y, so the rear cut face (max Y) becomes the bed
    part.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0]))
    return drop_to_bed(part)


def main():
    os.makedirs(OUT, exist_ok=True)
    face, body, cap = (drop_to_bed(load(n)) for n in ("face", "body", "cap"))

    print("plate 1 (main colour):")
    for n, t in (("face", face), ("body", body)):
        report(n, t)
    write_plate("plate1_house", arrange([("face", face), ("body", body)]))

    print("plate 2 (roof colour):")
    report("roof", cap)
    write_plate("plate2_roof", arrange([("roof", cap)]))

    print("plate 3 (roof click coupon):")
    cb = band_slice(load("body", "assembled"), COUPON_Y0, COUPON_Y1)
    cc = band_slice(load("cap", "assembled"), COUPON_Y0, COUPON_Y1)
    for n, t in (("body_click", cb), ("roof_click", cc)):
        report(n, t)
    write_plate("plate3_click_coupon", arrange([("body_click", cb), ("roof_click", cc)]))

    print("plate 4 (FACE arms + display pocket coupon):")
    # the full FACE carries the display pocket with its crush ribs and the 3 arms (aframe_b.py ARM_Y0..ARM_Y1 = 0.4..10.6);
    # the front band of the BODY holds the floor grooves the arm hooks snap into
    fb = band_slice(load("body", "assembled"), 0.0, FRONT_BAND_Y1)
    for n, t in (("face", face), ("body_front", fb)):
        report(n, t)
    write_plate("plate4_face_coupon", arrange([("face", face), ("body_front", fb)]))


if __name__ == "__main__":
    main()
