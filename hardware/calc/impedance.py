"""Differential impedance of edge-coupled microstrip over a GND plane: quasi-static 2D finite-difference solve.

Checks every diff_pair in critical.yaml against its net class (board.yaml) and the outer layer of the stackup:
  impedance.py hardware/<board>                 OK/FAIL per pair (Zdiff with soldermask vs z_diff +- tol_ohm)
  impedance.py hardware/<board> --try 0.25/0.15 explore widths/gaps (mm) on the same stackup
Self-test (--selftest): wide gap (S=3.0) gives 2x the single-line Z0 (~62 ohm for W 0.27 on JLC04161H-7628).
Fab calculators (e.g. jlcpcb.com/impedance) are the cross-check: write their inputs and result into the order notes.
"""
# Zdiff = 2*Z_odd. Odd mode: mirror plane at x=0 between the lines.
import argparse
import sys
from pathlib import Path

import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spl

sys.path.insert(0, str(Path(__file__).parent))
from pcb import load_profile, report  # noqa: E402


def solve(W, S, h=0.2104, er=4.4, t=0.035, mask=0.0, erm=3.8, d=0.005):
    X, Y = 4.0, 2.0                       # half-width (odd symmetry plane at x=0), height (mm)
    nx, ny = int(X/d), int(Y/d)
    x = (np.arange(nx)+0.5)*d; y = np.arange(ny)*d
    eps = np.ones((ny, nx)); eps[y < h, :] = er
    x0, x1 = S/2, S/2+W
    cond = (x[None,:] >= x0) & (x[None,:] <= x1) & (y[:,None] >= h) & (y[:,None] <= h+t)
    if mask:
        near = (x[None,:] >= x0-mask) & (x[None,:] <= x1+mask) & (y[:,None] >= h) & (y[:,None] <= h+t+mask)
        eps[(y[:,None] >= h) & (y[:,None] < h+mask) & ~cond] = erm; eps[near & ~cond] = erm
    def cap(epsm):
        N = nx*ny; idx = np.arange(N).reshape(ny, nx)
        rows, cols, vals = [], [], []; b = np.zeros(N)
        fixed = cond.copy(); fixed[0, :] = True   # ground plane at y=0
        V = np.where(cond, 1.0, 0.0)
        for j in range(ny):
            for i in range(nx):
                k = idx[j, i]
                if fixed[j, i]: rows.append(k); cols.append(k); vals.append(1.0); b[k] = V[j, i]; continue
                diag = 0.0
                for dj, di in ((1,0),(-1,0),(0,1),(0,-1)):
                    jj, ii = j+dj, i+di
                    if ii < 0: e = epsm[j, i]; diag += e; continue   # odd: mirror is -V -> V=0 at x=-d/2 ... approx antisym
                    if jj >= ny or ii >= nx: continue                # Neumann at far boundary
                    e = 0.5*(epsm[j, i]+epsm[jj, ii]); diag += e
                    rows.append(k); cols.append(idx[jj, ii]); vals.append(-e)
                rows.append(k); cols.append(k); vals.append(diag)
        A = sp.csr_matrix((vals, (rows, cols)), shape=(N, N))
        phi = spl.spsolve(A, b).reshape(ny, nx)
        # charge via energy: C = sum eps |grad|^2 (per unit V^2)
        gx = np.diff(phi, axis=1); ex = 0.5*(epsm[:, 1:]+epsm[:, :-1])
        gy = np.diff(phi, axis=0); ey = 0.5*(epsm[1:, :]+epsm[:-1, :])
        E = (ex*gx**2).sum() + (ey*gy**2).sum() + (epsm[:, 0]*(2*phi[:, 0])**2).sum()*0.5
        return E  # proportional to C (eps0 dropped)
    c = cap(eps); c0 = cap(np.ones_like(eps))
    eps0 = 8.854e-12; cl = 3e8
    C, C0 = c*eps0, c0*eps0
    Z = 1/(cl*np.sqrt(C*C0))
    return 2*Z, C/C0
def zdiff(W, S, layer, stack):
    """(bare, with soldermask) differential impedance in ohm."""
    h, er, t = layer["dielectric_mm"], layer["er"], stack["copper_mm"]
    m = stack.get("soldermask") or {}
    bare, _ = solve(W, S, h=h, er=er, t=t)
    masked, _ = solve(W, S, h=h, er=er, t=t, mask=m.get("thickness_mm", 0.0), erm=m.get("er", 3.8))
    return bare, masked


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("board_dir", type=Path)
    ap.add_argument("--try", dest="tries", nargs="*", default=[], metavar="W/S")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    prof = load_profile(a.board_dir)
    stack = prof["stackup"]
    layers = {l["name"]: l for l in stack["layers"]}
    outer = stack["layers"][0]
    if a.selftest:
        z_wide, _ = zdiff(0.27, 3.0, outer, stack)
        ok = 55 < z_wide / 2 < 70
        print(f"selftest: S=3.0 gives Z0 {z_wide / 2:.1f} ohm per line (expect ~62)", "OK" if ok else "FAILED")
        sys.exit(0 if ok else 1)
    for t in a.tries:
        W, S = map(float, t.split("/"))
        bare, masked = zdiff(W, S, outer, stack)
        print(f"W {W} S {S} on {outer['name']}: Zdiff {bare:.1f} ohm bare, {masked:.1f} with mask")
    if a.tries:
        return
    rows = []
    for e in prof["critical"].get("diff_pair", []):
        nc = prof["net_classes"][e["net_class"]]
        layer = layers[e.get("layer", outer["name"])]
        bare, masked = zdiff(nc["track_mm"], nc["gap_mm"], layer, stack)
        ok = abs(masked - e["z_diff"]) <= e["tol_ohm"]
        rows.append(("OK" if ok else "FAIL", f"{e['name']}: Zdiff",
                     f"{masked:.1f} ohm with mask ({bare:.1f} bare), W {nc['track_mm']} S {nc['gap_mm']}, "
                     f"{layer['name']}, target {e['z_diff']} +- {e['tol_ohm']}"))
    fails = report(rows, f"impedance: {a.board_dir} ({stack['name']})")
    print("impedance:", "FAILED" if fails else "OK")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
