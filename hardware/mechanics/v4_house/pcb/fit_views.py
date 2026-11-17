"""Section views of the real PCB (pcb/pcb_board_frame.npz, placed as in aframe_b.py) in the BODY: across at the ESP,
in the rear wall (USB hole), plan at Z 6.5. Run after aframe_b.py:
uv run --with manifold3d --with trimesh --with numpy --with matplotlib --with networkx --with shapely --with scipy python -I pcb/fit_views.py"""
import os, sys
import numpy as np, trimesh, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import aframe_b as A                                     # noqa: E402  (parameters + PCB placement only)
body = trimesh.load(os.path.join(HERE, "..", "stl", "aframe_b_body_assembled.stl")); body.merge_vertices()
face = trimesh.load(os.path.join(HERE, "..", "stl", "aframe_b_face_assembled.stl")); face.merge_vertices()
cols = {"board": "green", "J1": "red", "U1": "blue", "J2": "orange", "U4": "purple", "U5": "magenta", "C5": "brown"}


def sec(a, m, o, n, idx, c="k"):
    s = m.section(plane_origin=o, plane_normal=n)
    if s is not None:
        for e in s.discrete:
            a.plot(e[:, idx[0]], e[:, idx[1]], "-", color=c, lw=1)


def rect(a, r, i, j, sel=None, lab=True):
    v = A.PCB[r] if r != "board" else np.vstack([A.PCB["board"][:, :2].min(0).tolist() + [A.Z_SHELF + 0.01], A.PCB["board"][:, :2].max(0).tolist() + [A.Z_SHELF + 0.01 + A.PCB_T]])
    lo, hi = v.min(0), v.max(0)
    a.add_patch(plt.Rectangle((lo[i], lo[j]), hi[i] - lo[i], hi[j] - lo[j], fc=cols[r], alpha=.35, ec=cols[r], label=r if lab else None))


fig, ax = plt.subplots(1, 3, figsize=(21, 7.5))
a = ax[0]; sec(a, body, [0, 35, 0], [0, 1, 0], (0, 2))
for r in ("board", "U1"):
    rect(a, r, 0, 2)
a.set_title(f"Across at Y 35 (ESP): antenna gap {A.CX + A.HW_F - A.WALL - A.PCB['U1'][:, 0].max():.2f} mm")
a = ax[1]; sec(a, body, [0, A.Y_REAR_IN + 1.0, 0], [0, 1, 0], (0, 2))
for r in ("board", "J1"):
    rect(a, r, 0, 2)
a.set_title(f"In the rear wall (Y {A.Y_REAR_IN + 1.0:.1f}): USB hole vs J1")
a = ax[2]; sec(a, body, [0, 0, 6.5], [0, 0, 1], (0, 1)); sec(a, face, [0, 0, 15.5], [0, 0, 1], (0, 1), "tab:gray")
for r in cols:
    rect(a, r, 0, 1)
a.set_title("Plan at Z 6.5 (front = Y 0); grey = FACE at Z 15.5 (ring ribs)")
for k, a in enumerate(ax):
    a.set_aspect("equal"); a.grid(alpha=.3); a.legend(fontsize=8, loc="upper left")
    a.set_xlabel("X"); a.set_ylabel("Y" if k == 2 else "Z")
    if k < 2:
        a.set_xlim(-10, 52); a.set_ylim(-3, 22)
plt.tight_layout(); plt.savefig(os.path.join(HERE, "fit_real_pcb.png"), dpi=80)
print("wrote pcb/fit_real_pcb.png")
