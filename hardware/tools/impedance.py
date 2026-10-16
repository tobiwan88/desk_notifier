"""USB pair impedance check: quasi-static 2D finite-difference solve of edge-coupled microstrip over GND.
Usage: uv run --with numpy --with scipy python hardware/tools/impedance.py W/S [W/S ...]  (mm, JLC04161H-7628 L1/L2)
Cross-check: S=3.0 gives 2x the single-line Z0 (~62 ohm for W 0.27).
"""
# Quasi-static 2D FD solver: edge-coupled microstrip over GND, optional soldermask. Zdiff = 2*Z_odd.
import numpy as np, scipy.sparse as sp, scipy.sparse.linalg as spl, sys
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
for W, S in [tuple(map(float, a.split('/'))) for a in sys.argv[1:]]:
    zd, ee = solve(W, S); zm, _ = solve(W, S, mask=0.015)
    print(f"W {W} S {S}: Zdiff {zd:.1f} ohm (bare), {zm:.1f} (with 15um mask), eeff {ee:.2f}")
