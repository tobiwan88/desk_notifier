"""C4 "Moon on a plinth", aesthetic pass: the disc and the plinth are one form.
The front plate (disc + plinth front block) is one flat face whose outline flows from the plinth side through an
S-curve into the disc (no neck), a quarter-circle cove (saddle) sweeps up the back of the plate, the plinth is a
pillow with elliptical edges and a symmetric plan, the vents hide in the saddle and underneath.
(C3c = C3b after the independent review; everything inside is unchanged.)

Frame (brief frame, no tilt): X width, Y depth (front 0, back +), Z up, mm.  The table is at
Z = -RAIL (the two skid rails under the plinth are the lowest points).
Parts: FACE (disc + neck + plinth front block + snap arms, prints front face down),
       PLINTH (hollow slab, prints seam face down, standing on its edge),
       BACK (disc back plate with 3 tangential arms, prints rear face down).
Run: uv run --with manifold3d --with trimesh --with numpy --with matplotlib python -I moon3c.py
"""
import math
import os
import numpy as np
import trimesh
from manifold3d import Manifold as M, CrossSection as CS, JoinType, OpType, Mesh, Error

OUT = os.path.dirname(os.path.abspath(__file__))
SEG = 64

# ------------------------------------------------------------------ parameters
CX, ZC = 20.5, 33.7                     # display / window centre
R_MOD, TAB_W, TAB_Z0, CLR = 19.0, 23.0, 7.2, 0.25
R_CAV = R_MOD + CLR                     # 19.25 disc cavity
R_DISC = R_CAV + 2.0                    # 21.25 -> 42.5 mm
R_WIN, WIN_CH = 16.7, 0.5               # window 33.4 (DFM 27) with 0.5 chamfer
WALL = 1.68                             # 4 perimeters (DFM 18)
Y_FRONT, Y_CAV = -0.25, 1.35            # front face, cavity front (glass at 1.6)
Y_SEAM = 7.75                           # disc/neck depth 8.0 (DFM 42)
Y_REAR_OUT, Y_REAR_IN = 63.6, 60.61
CH = 0.6                                # bed-side / seam chamfer (DFM 20)
Z_BOT, RAIL = 0.0, 1.2                  # slab underside, two feet rails below it (visible shadow gap)
Z_FI = 1.4                              # cavity floor (J2 tails bottom 1.9 -> 0.5)
Z_ESP_TOP, Z_J2_TOP, Z_TAIL, Z_USB_TOP = 8.32, 7.76, 1.9, 8.45
Z_CEIL = 10.32                          # 2.0 over the ESP shield (DFM 32), 1.87 over the USB receptacle
Z_TOP = Z_CEIL + WALL                   # 12.0 (flat top over the board)
DROP_R = 0.5                            # the top falls by this much towards the rear (gentle taper in profile)
Y_T0, Y_T1 = 44.5, 63.6                 # taper starts behind the ESP shield
RT_H, RT_V = 6.0, 4.0                   # top edge: ellipse 6 wide x 4 high (pillow)
RB_H, RB_V = 3.0, 3.0                   # bottom edge round
HW, HW_R = 26.7, 25.5                   # plan half-width (symmetric about CX): antenna side sets 26.7, rear corners R11 to 25.5
R_REAR = 11.0
R_CLOSE = 11.0                          # fillet of the front outline between plinth and disc (2D closing radius)
R_COVE = 30.0                           # saddle: quarter-circle cove up the back of the plate
FIN_HW, FIN_FLARE, FIN_R = 17.5, 3.0, 2.5   # cove width (half), flare at its foot, edge radius
Z_USB_CEIL, Y_USB_X0, Y_USB_X1 = 9.65, 14.6, 27.1   # receptacle top 8.45 -> 1.20 clearance
RAIL_X0, RAIL_X1, RAIL_Y1 = 12.5, 21.0, 52.0
CAV_X0, CAV_X1 = 2.55, 39.25            # board 2.85..38.85 + 0.3 / 0.4
ANT_X1, ANT_Y0, ANT_Y1 = 45.45, 25.86, 45.06   # antenna bay (0.6 clearance)
Z_SHELF, Z_RIB = 3.60, 5.50             # board slot 1.9 mm (DFM 28)
Y_SHELF0 = 11.2                         # shelves start behind the snap bars
TAB_Y_POCKET = Y_CAV                    # C4c: tab pocket starts at the cavity front (1.35), so a tab carrying full glass (Y 1.6..3.1) fits
TAB_POCKET_W = TAB_W + 2 * CLR          # 23.5


# snap arms FACE -> PLINTH (DFM 33, 35)
ARM_T, ARM_L, ARM_W = 1.26, 10.0, 6.0
UNDERCUT, DEFL = 0.5, 0.6
ARM_Z0 = 1.65                           # arm underside (0.25 over the floor)
ARM_Z1 = ARM_Z0 + ARM_T                 # 2.91
ARM_Y0, ARM_Y1 = 1.25, 10.6            # bars grow out of the front wall (no mid-air start)
ARM_M, ARM_TIP_FRONT = 0.8, 7.9         # arm front edge: 51 deg from vertical in plan, from the wall to the hook
LEAD_DEG, RET_DEG = 30.0, 60.0
HOOK_LEN_X = 1.5
GROOVE_CLR = 0.1

E_PETG, E_PLA = 2100.0, 3000.0
RHO_PETG = 1.27e-3                      # g/mm3


# ------------------------------------------------------------------ helpers
def box(x0, x1, y0, y1, z0, z1):
    return M.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def ext_y(cs, y0, y1):
    return M.extrude(cs, y1 - y0).rotate((90, 0, 0)).translate((0, y1, 0))


def sect_y(m, y0):
    return m.translate((0, -y0, 0)).rotate((-90, 0, 0)).slice(0)


def cyl_y(cx, cz, r0, r1, y0, y1):
    return M.cylinder(y1 - y0, r0, r1, SEG).rotate((-90, 0, 0)).translate((cx, y0, cz))


def sq(x0, x1, z0, z1):
    return CS.square((x1 - x0, z1 - z0)).translate((x0, z0))


def circ(cx, cz, r):
    return CS.circle(r, SEG).translate((cx, cz))


def sector(cx, cz, r0, r1, a0_deg, a1_deg, n=14):
    ts = np.radians(np.linspace(a0_deg, a1_deg, n))
    pts = [(cx + r1 * math.cos(t), cz + r1 * math.sin(t)) for t in ts] + \
          [(cx + r0 * math.cos(t), cz + r0 * math.sin(t)) for t in ts[::-1]]
    return CS([np.array(pts)])


def union(*ms):
    ms = list(ms)
    return (CS if isinstance(ms[0], CS) else M).batch_boolean(ms, OpType.Add)


def poly_cs(pts):
    p = np.array(pts, float)
    a = 0.5 * np.sum(p[:, 0] * np.roll(p[:, 1], -1) - np.roll(p[:, 0], -1) * p[:, 1])
    if a < 0:
        p = p[::-1]
    return CS([p])


def prism_x(profile_yz, x0, x1):
    """Convex prism: (y, z) polygon extruded along X from x0 to x1."""
    return M.hull_points(np.array([(x, y, z) for x in (x0, x1) for (y, z) in profile_yz]))


def revolve_y(profile_ry, cx, cz):
    """Solid of revolution about the Y axis through (cx, cz); profile = (radius, y) polygon."""
    r = M.revolve(poly_cs(profile_ry), SEG, 360.0)
    return r.rotate((-90, 0, 0)).translate((cx, 0, cz))


def finalize(mm, q=1.0 / 1024):
    """Snap vertices to a 2^-10 mm grid (exact in float32), merge, drop collapsed facets, rebuild the Manifold.
    Removes the micro-slivers of the boolean ops so that the float32 STL stays watertight."""
    me = mm.to_mesh64()
    V = np.asarray(me.vert_properties)[:, :3]
    F = np.asarray(me.tri_verts)
    u, inv = np.unique(np.round(V / q) * q, axis=0, return_inverse=True)
    G = inv[F]
    G = G[(G[:, 0] != G[:, 1]) & (G[:, 1] != G[:, 2]) & (G[:, 0] != G[:, 2])]
    out = M(Mesh(vert_properties=u.astype(np.float32), tri_verts=G.astype(np.uint32)))
    assert out.status() == Error.NoError and abs(out.volume() - mm.volume()) < 0.002 * mm.volume(), \
        f"finalize failed: {out.status()} vol {out.volume():.2f} vs {mm.volume():.2f}"
    return out


def strict_stl_check(mm):
    """Float32 STL as a slicer reads it: vertices merged exactly, zero-area facets dropped."""
    me = mm.to_mesh()
    V = np.asarray(me.vert_properties)[:, :3].astype(np.float32)
    F = np.asarray(me.tri_verts)
    u, inv = np.unique(V[F].reshape(-1, 3), axis=0, return_inverse=True)
    G = inv.reshape(-1, 3)
    G = G[(G[:, 0] != G[:, 1]) & (G[:, 1] != G[:, 2]) & (G[:, 0] != G[:, 2])]
    e = np.sort(np.concatenate([G[:, [0, 1]], G[:, [1, 2]], G[:, [2, 0]]]), axis=1)
    _, c = np.unique(e, axis=0, return_counts=True)
    P = u[G]
    ar = 0.5 * np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1)
    return dict(faces=len(G), bad_edges=int((c != 2).sum()), zero_area=int((ar < 1e-9).sum()))


def moments(cs):
    """Area, centroid, centroidal Ixx (about the x axis, uses y) and Iyy of a CrossSection."""
    A = cx = cy = ixx = iyy = 0.0
    for poly in cs.to_polygons():
        p = np.asarray(poly)
        x, y = p[:, 0], p[:, 1]
        x1, y1 = np.roll(x, -1), np.roll(y, -1)
        a = x * y1 - x1 * y
        A += a.sum() / 2
        cx += ((x + x1) * a).sum() / 6
        cy += ((y + y1) * a).sum() / 6
        ixx += ((y * y + y * y1 + y1 * y1) * a).sum() / 12
        iyy += ((x * x + x * x1 + x1 * x1) * a).sum() / 12
    gx, gy = cx / A, cy / A
    return A, gx, gy, ixx - A * gy * gy, iyy - A * gx * gx


def pick_neck():
    return dict(ws=1.68, W=0.0)


def neck_scan(face):
    """Bending section modulus of the FACE plate between the plinth top and the disc cavity (tab pocket section, U open at the rear),
    scanned over height; returns the minimum S_front (front fibre in tension when the disc is pushed back), S_rear, S_side."""
    best = None
    for z in np.arange(Z_TOP + 0.3, ZC - R_CAV - 0.05, 0.25):
        cs = face.slice(float(z))
        A, gx, gy, I, Iy = moments(cs)
        s_front = I / (gy - Y_FRONT)
        s_rear = I / (Y_SEAM - gy)
        xs = np.concatenate([np.asarray(p)[:, 0] for p in cs.to_polygons()])
        s_side = Iy / max(gx - xs.min(), xs.max() - gx)
        row = dict(z=float(z), A=A, s_front=s_front, s_rear=s_rear, s_side=s_side, W=float(xs.max() - xs.min()))
        if best is None or row["s_front"] < best["s_front"]:
            best = row
    return best


# ------------------------------------------------------------------ plan and soft body
def plan_shape():
    """Symmetric about CX: straight sides (half-width HW) up to the antenna bay, then rear corners R_REAR (half-width HW_R)."""
    circles = [(CX - (HW_R - R_REAR), Y_REAR_OUT - R_REAR, R_REAR), (CX + (HW_R - R_REAR), Y_REAR_OUT - R_REAR, R_REAR)]
    pts = []
    for cx, cy, r in circles:
        for t in np.linspace(0, 2 * math.pi, 96, endpoint=False):
            pts.append((cx + r * math.cos(t), cy + r * math.sin(t)))
    pts += [(CX - HW, -15.0), (CX + HW, -15.0), (CX - HW, 46.0), (CX + HW, 46.0)]
    return CS.hull_points(np.array(pts))


def densify(poly, step=2.0, ys=()):
    p = np.asarray(poly)
    out = []
    n = len(p)
    for i in range(n):
        a, b = p[i], p[(i + 1) % n]
        ts = {0.0}
        L = float(np.hypot(*(b - a)))
        for k in range(1, int(L / step)):
            ts.add(k * step / L)
        for y in ys:
            if (a[1] - y) * (b[1] - y) < 0:
                ts.add((y - a[1]) / (b[1] - a[1]))
        for t in sorted(ts):
            out.append(a + t * (b - a))
    return out


def top_shear(y):
    """Fall of the top surface towards the rear (0 up to Y_T0, linear to -DROP_R at the rear), a plane: stays convex."""
    f = min(max((y - Y_T0) / (Y_T1 - Y_T0), 0.0), 1.0)
    return -DROP_R * f


def rounded_z(plan, z0, z1, rt_h, rt_v, rb_h, rb_v, nt=9, nb=5, shear=None):
    """Convex body over `plan`: elliptical bottom edge (rb_h x rb_v) and top edge (rt_h x rt_v), hull of offset levels."""
    levels = []
    if rb_h > 0:
        for k in range(nb + 1):
            t = (k / nb) * math.pi / 2
            levels.append((z0 + rb_v * (1 - math.cos(t)), rb_h * (1 - math.sin(t)), False))
    else:
        levels.append((z0, 0.0, False))
    for k in range(nt + 1):
        t = (k / nt) * math.pi / 2
        levels.append((z1 - rt_v + rt_v * math.sin(t), rt_h * (1 - math.cos(t)), True))
    pts = []
    for z, ins, top in levels:
        cs = plan if ins < 1e-6 else plan.offset(-ins, JoinType.Round, 2.0, 96)
        for poly in cs.to_polygons():
            for x, y in densify(poly, 2.0, (Y_T0, Y_T1) if shear else ()):
                pts.append((x, y, z + (shear(y) if (shear and top) else 0.0)))
    return M.hull_points(np.array(pts))


def chamfer_cut_front(sil, y0, d):
    """Material to remove so that the edge at plane y0 (bed side, grows towards +Y) gets a d x 45 chamfer."""
    ring = sil.offset(2.0, JoinType.Round, 2.0, SEG) - sil.offset(-0.01, JoinType.Round, 2.0, SEG)
    cone = M.cylinder(d, d, 0.0, 23).rotate((-90, 0, 0))
    return ext_y(ring, y0 - 0.05, y0).minkowski_sum(cone)


def chamfer_cut_back(sil, y1, d):
    ring = sil.offset(2.0, JoinType.Round, 2.0, SEG) - sil.offset(-0.01, JoinType.Round, 2.0, SEG)
    cone = M.cylinder(d, 0.0, d, 23).rotate((-90, 0, 0)).translate((0, -d, 0))
    return ext_y(ring, y1, y1 + 0.05).minkowski_sum(cone)


# ------------------------------------------------------------------ internals
PLUG_LEN = 16.0


def build_internals():
    I = {}
    I["board"] = box(2.85, 38.85, 10.46, 60.46, 3.61, 5.12)
    I["low_parts"] = box(4.4, 37.3, 11.0, 59.0, 5.12, 6.6)
    I["esp_shield"] = box(19.35, 38.85, 26.46, 44.46, 5.12, Z_ESP_TOP)
    I["antenna"] = box(38.85, 44.85, 26.46, 44.46, 5.2, 6.2)
    I["j2_socket"] = box(12.5, 28.5, 10.46, 14.5, 5.12, Z_J2_TOP)
    I["j2_tails"] = box(14.0, 27.0, 11.0, 14.0, Z_TAIL, 3.61)
    I["usb_recept"] = box(16.35, 25.35, 54.0, 60.46, 5.15, Z_USB_TOP)
    I["usb_shell"] = box(20.85 - 4.47, 20.85 + 4.47, 60.46, 61.91, 6.8 - 1.63, 6.8 + 1.63)
    I["usb_plug"] = box(20.85 - 6.0, 20.85 + 6.0, 61.91, 61.91 + PLUG_LEN, 6.8 - 3.5, 6.8 + 3.5)
    glass = ext_y(circ(CX, ZC, R_MOD), 1.6, 3.1)
    pcb = ext_y(union(circ(CX, ZC, R_MOD), sq(CX - TAB_W / 2, CX + TAB_W / 2, TAB_Z0, ZC)), 3.1, 4.7)
    I["display"] = union(glass, pcb)
    I["header"] = box(13.5, 27.5, 4.7, 10.46, 7.2, Z_J2_TOP)
    # worst case: the lower tab (23 x 11) carries full glass thickness
    I["tab_glass"] = box(CX - TAB_W / 2, CX + TAB_W / 2, 1.6, 3.1, TAB_Z0, TAB_Z0 + 11.0)
    return I


def build_clearance():
    C = {}
    r = R_MOD + CLR
    glass = ext_y(circ(CX, ZC, r), 1.6 - CLR, 3.1)
    pcb = ext_y(union(circ(CX, ZC, r),
                      sq(CX - TAB_W / 2 - CLR, CX + TAB_W / 2 + CLR, TAB_Z0 - CLR, ZC)), 3.1, 4.7 + CLR)
    C["display+0.25"] = union(glass, pcb)
    C["tab_glass+0.25"] = box(CX - TAB_W / 2 - CLR, CX + TAB_W / 2 + CLR, 1.6, 3.1, TAB_Z0 - CLR, TAB_Z0 + 11.0 + CLR)
    C["antenna+0.5"] = box(38.35, 45.35, 25.96, 44.96, 4.7, 6.7)
    C["esp_shield+0.5"] = box(19.35, 38.85, 25.96, 44.96, 5.12, Z_ESP_TOP + 0.5)
    C["j2_socket+0.3"] = box(12.2, 28.8, 10.46, 14.8, 5.12, Z_J2_TOP + 0.3)
    C["j2_tails+0.3"] = box(13.7, 27.3, 10.7, 14.3, Z_TAIL - 0.3, 3.61)
    C["usb_recept+0.3"] = box(16.05, 25.65, 54.0, 60.46, 5.15, Z_USB_TOP + 0.3)
    return C


# ------------------------------------------------------------------ snap details
def hook_profile():
    """(y, z) polygon of the arm hook relative to y0 = 0, z = arm underside = 0, depth down = negative."""
    h = UNDERCUT + (ARM_Z0 - Z_FI)               # hook height below the arm: undercut + gap to the floor
    ret = h / math.tan(math.radians(RET_DEG))
    lead = h / math.tan(math.radians(LEAD_DEG))
    land = 0.3
    return [(0.0, 0.0), (ret, -h), (ret + land, -h), (ret + land + lead, 0.0)], h


def arm_plan_pts(x_root, direction):
    """Plan (x, y) outline of an arm: rear edge straight, front edge a diagonal that starts in the front wall at the
    root and reaches the hook at the tip, so every print layer grows from the previous one (<= 51 deg from vertical)."""
    xt = x_root + direction * ARM_L
    yf0 = ARM_TIP_FRONT - ARM_M * ARM_L
    return [(x_root, yf0), (xt, ARM_TIP_FRONT), (xt, ARM_Y1), (x_root, ARM_Y1)]


def prism_z(pts_xy, z0, z1):
    return M.extrude(poly_cs(pts_xy), z1 - z0).translate((0, 0, z0))


def arm_set(x_root, direction, y_hook0):
    """One in-plane cantilever arm (long axis X, bends in Z) with hook and groove.
    x_root = inner face of the bar the arm grows from; direction +1 / -1.
    Returns (arm solid, groove solid, tip x, relief solid under the arm in the FACE floor)."""
    xt = x_root + direction * ARM_L
    xa, xb = sorted((x_root, xt))
    pl = arm_plan_pts(x_root, direction)
    arm = prism_z(pl, ARM_Z0, ARM_Z1)
    # root taper on the compression (top) side: t -> 1.4 t over 3 mm (DFM 36)
    g = poly_cs([(x_root - direction * 0.01, ARM_Z1 - 0.01), (x_root - direction * 0.01, ARM_Z1 + 0.5),
                 (x_root + direction * 3.0, ARM_Z1 - 0.01)])
    arm = arm + (ext_y(g, -1.0, ARM_Y1) ^ prism_z(pl, ARM_Z0, ARM_Z1 + 1.0))
    prof, h = hook_profile()
    hx0, hx1 = (xt - HOOK_LEN_X, xt) if direction > 0 else (xt, xt + HOOK_LEN_X)     # hook at the free end, on the arm
    hook_pts = [(y_hook0 + y, ARM_Z0 + z) for (y, z) in prof]
    hook_pts += [(hook_pts[0][0], ARM_Z0 + 0.05), (hook_pts[-1][0], ARM_Z0 + 0.05)]
    arm = arm + prism_x(hook_pts, hx0, hx1)
    # relief in the FACE floor under the arm: the plan grown by 0.25, starting at the root
    rel = poly_cs(pl).offset(0.25, JoinType.Round, 2.0, 16) ^ CS.square((ARM_L + 1, 40)).translate(
        (x_root if direction > 0 else x_root - ARM_L - 1, -5))
    relief = M.extrude(rel, 0.31).translate((0, 0, Z_FI - 0.3))
    # groove in the plinth floor: hook profile grown by 0.1, 0.6 deep, +-0.2 in X
    gp = poly_cs([(y_hook0 + y, ARM_Z0 + z) for (y, z) in prof] +
                 [(y_hook0 + prof[-1][0], Z_FI + 0.3), (y_hook0 + prof[0][0], Z_FI + 0.3)])
    gp = gp.offset(GROOVE_CLR, JoinType.Round, 2.0, 16)
    gpts = [tuple(p) for p in gp.to_polygons()[0]]
    groove = prism_x(gpts, hx0 - 0.2, hx1 + 0.2) ^ box(hx0 - 1, hx1 + 1, 0, 30, Z_FI - UNDERCUT - 0.1, Z_FI + 0.2)
    return arm, groove, xt, relief


def fin_d(h):
    """Depth of the cove behind the plate at height h above the plinth top: quarter circle, tangent to the top and to the plate."""
    h = min(max(h, 0.0), R_COVE)
    return R_COVE - math.sqrt(max(R_COVE ** 2 - (R_COVE - h) ** 2, 0.0))


def fin_hw(h):
    h = min(max(h, 0.0), FIN_FLARE)
    return FIN_HW + FIN_FLARE - math.sqrt(max(FIN_FLARE ** 2 - (FIN_FLARE - h) ** 2, 0.0)) if FIN_FLARE > 0 else FIN_HW


def build_fin():
    """The saddle: a loft of rounded rectangles (each convex) whose depth follows the cove circle. Clipped at the seam."""
    hs = [R_COVE * (1 + math.sin(math.radians(a))) for a in np.linspace(180.0, 270.0, 30)]
    hs += [0.15, 0.4, 0.8, 1.3, 1.9, 2.6]
    hs = sorted(set(round(h, 4) for h in hs))
    hs = [-1.2] + hs
    secs = []
    for h in hs:
        d = fin_d(h)
        hw = fin_hw(h)
        y0, y1 = Y_SEAM - 3.0, Y_SEAM + d
        r = min(FIN_R, 0.45 * (y1 - y0), 0.9 * hw)
        cs = CS.square((2 * hw, y1 - y0)).translate((CX - hw, y0))
        cs = cs.offset(-r, JoinType.Round, 2.0, 24).offset(r, JoinType.Round, 2.0, 24)
        poly = np.asarray(cs.to_polygons()[0])
        secs.append((Z_TOP + h, poly))
    parts = []
    for (za, pa), (zb, pb) in zip(secs[:-1], secs[1:]):
        pts = [(x, y, za) for x, y in pa] + [(x, y, zb) for x, y in pb]
        parts.append(M.hull_points(np.array(pts)))
    return union(*parts) ^ box(-50, 100, Y_SEAM, 100, -10, 100)


# ------------------------------------------------------------------ housing
def build_housing(neck):
    ws, NW = neck["ws"], neck["W"]
    plan = plan_shape()
    H = rounded_z(plan, Z_BOT, Z_TOP, RT_H, RT_V, RB_H, RB_V, shear=top_shear) ^ box(-50, 100, Y_FRONT, 100, -10, 30)
    plan_in = plan.offset(-WALL, JoinType.Round, 2.0, 96)
    Hin = rounded_z(plan_in, Z_FI, Z_CEIL, RT_H - WALL, RT_V - WALL, RB_H - WALL, RB_V - WALL, nt=9, nb=4, shear=top_shear)

    # underside: two feet rails (visible 1.2 mm shadow gap), symmetric about CX
    rail_l = box(CX - RAIL_X1, CX - RAIL_X0, Y_FRONT, RAIL_Y1, -RAIL, 0.3)
    rail_r = box(CX + RAIL_X0, CX + RAIL_X1, Y_FRONT, RAIL_Y1, -RAIL, 0.3)
    base = H + rail_l + rail_r

    # ---- saddle: quarter-circle cove up the back of the plate, loft of rounded sections (convex per level)
    fin = build_fin()
    body = base + fin
    face_blk = base ^ box(-50, 100, -10, Y_SEAM, -10, 30)
    plin_blk = body ^ box(-50, 100, Y_SEAM, 100, -10, 100)

    # ---- plinth cavity (shared by FACE block and PLINTH)
    cav_main = box(CAV_X0, CAV_X1, Y_CAV, Y_REAR_IN, Z_FI, Z_CEIL)
    cav_ant = box(CAV_X0, ANT_X1, ANT_Y0, ANT_Y1, Z_SHELF, Z_CEIL)
    cav_chan = box(CX - HW + 1.0, CAV_X0 + 0.05, 14.5, 61.0, Z_FI, Z_CEIL)   # hollow left flank behind the lip zone: air for the sensors
    cav_raw = cav_main + cav_ant + cav_chan
    cav = cav_raw ^ Hin
    # C4b: local pocket over the USB receptacle (inner top round + rear fall pinched the ceiling to 8.9): ceiling Z_USB_CEIL
    cav = cav + box(Y_USB_X0, Y_USB_X1, 52.0, Y_REAR_IN, Z_FI, Z_USB_CEIL)
    # (C3c rear-roof wedge dropped: the ceiling already falls 1.2 mm to the rear, cavity height 7.9 at the rear wall = bridge < 8 mm)

    # ---- board shelves with 45 deg front ramps, hold-down ribs with crush bead
    def ramp_shelf(x0, x1, y0, y1):
        s = box(x0, x1, y0, y1, Z_FI - 0.2, Z_SHELF)
        k = 1 / math.sqrt(2)
        return s.trim_by_plane((0, k, -k), (y0 - Z_FI) * k)

    shelves = ramp_shelf(CAV_X0 - 0.05, 4.85, Y_SHELF0, 44.5) + ramp_shelf(36.85, CAV_X1 + 0.05, Y_SHELF0, 60.2)

    def rib(xa, xb, y0, y1):
        xm = 0.5 * (xa + xb)
        body_r = box(xa, xb, y0, y1, Z_RIB, Z_CEIL + 0.3)
        # triangular crush bead, apex 5.12 (touches the nominal board top), base 0.8 wide
        pts = [(x, y, z) for (x, z) in ((xm - 0.4, Z_RIB + 0.01), (xm + 0.4, Z_RIB + 0.01), (xm, Z_RIB - 0.38)) for y in (y0, y1)]
        bead = M.hull_points(np.array(pts))
        r = body_r + bead
        k = 1 / math.sqrt(2)
        return r.trim_by_plane((0, k, k), (y0 + Z_CEIL) * k)     # 45 deg lead-in from the ceiling
    ribs = rib(3.2, 4.3, 10.5, 44.0) + rib(37.5, 38.5, 10.5, 25.7) + rib(37.5, 38.5, 45.2, 60.2)
    ribs = ribs ^ H

    # ---- PLINTH
    plinth = plin_blk - cav
    plinth = plinth + (shelves ^ box(-50, 100, Y_SEAM, 100, -10, 30) ^ H)
    plinth = plinth + (ribs ^ box(-50, 100, Y_SEAM, 100, -10, 30))
    # seam chamfer (bed side of the plinth)
    sil_p = sect_y(plin_blk, Y_SEAM + 0.001)
    plinth = plinth - chamfer_cut_front(sil_p, Y_SEAM, CH)

    # vents, hidden: (1) ESP exhaust = 8 slots 1.8 x 12.8 in the saddle (behind the plate, chimney through the cove),
    # (2) intake / sensor air = floor slots between the feet rails, (3) no top slots at all.
    for i in range(8):
        x = CX - 11.9 + 3.4 * i
        plinth = plinth - box(x - 0.9, x + 0.9, 12.0, 28.0, Z_CEIL - 0.4, 60.0)
    for x in (11.5, 15.1, 18.7, 22.3, 25.9, 29.5):
        plinth = plinth - box(x - 0.9, x + 0.9, 47.0, 58.0, -RAIL - 1, Z_FI + 0.2)
    for x in (15.1, 18.7, 22.3):
        plinth = plinth - box(x - 0.9, x + 0.9, 16.0, 24.0, -RAIL - 1, Z_FI + 0.2)     # front intake under the board
    # USB-C: 10.0 x 4.2 opening, 14.0 wide open notch (no roof), R1 corners
    ux = 20.85
    plinth = plinth - box(ux - 5.0, ux + 5.0, Y_REAR_IN - 0.2, 80, 6.8 - 2.1, 6.8 + 2.1)
    notch = CS.square((14.0, 25.0)).translate((ux - 7.0, 61.81)).offset(-1.0, JoinType.Round, 2.0, 16)\
        .offset(1.0, JoinType.Round, 2.0, 16)
    plinth = plinth - M.extrude(notch, 20.0).translate((0, 0, 2.9))

    # arms' floor grooves
    arms = []
    spec = [(3.91, +1), (16.26, +1), (37.89, -1)]
    y_hook0 = Y_SEAM + 0.65
    arm_solids, grooves, tips, reliefs = [], [], [], []
    for xr, dr in spec:
        a, g, xt, rl = arm_set(xr, dr, y_hook0)
        reliefs.append(rl)
        arm_solids.append(a)
        grooves.append(g)
        tips.append(xt)
    for g in grooves:
        plinth = plinth - g

    # ---- FACE
    # front plate outline: the plinth's front section, plus disc, with an S-curve (2D closing R_CLOSE) between them
    base_sec = sect_y(base, 3.0)
    up = union(circ(CX, ZC, R_DISC), base_sec ^ sq(-50, 100, 3.0, 20.0))
    F_up = up.offset(R_CLOSE, JoinType.Round, 2.0, 128).offset(-R_CLOSE, JoinType.Round, 2.0, 128) ^ sq(-50, 100, 3.0, 100)
    F = base_sec + F_up
    face_outer = union(face_blk, ext_y(F, Y_FRONT, Y_SEAM))
    sil_f = sect_y(face_outer, Y_FRONT + 0.001)
    sil_b = sect_y(face_outer, Y_SEAM - 0.001)
    face = face_outer - chamfer_cut_front(sil_f, Y_FRONT, CH) - chamfer_cut_back(sil_b, Y_SEAM, CH)

    face = face - (cav ^ box(-50, 100, Y_CAV, Y_SEAM + 1.0, -10, 30))
    face = face - ext_y(circ(CX, ZC, R_CAV), Y_CAV, Y_SEAM + 1.0)
    face = face - ext_y(sq(CX - TAB_POCKET_W / 2, CX + TAB_POCKET_W / 2, TAB_Z0 - CLR, ZC), TAB_Y_POCKET, Y_SEAM + 1.0)
    # window 33.4, 0.5 chamfer on the visible (bed) side
    face = face - cyl_y(CX, ZC, R_WIN + WIN_CH, R_WIN, Y_FRONT - 0.01, Y_FRONT + WIN_CH)
    face = face - cyl_y(CX, ZC, R_WIN, R_WIN, Y_FRONT + WIN_CH - 0.01, Y_CAV + 0.1)

    # floor relief under the arms inside the FACE block (keeps a 0.55 gap, DFM 31)
    for rl in reliefs:
        face = face - (rl ^ box(-50, 100, -10, Y_SEAM + 0.01, -10, 30))

    # lip bands + snap bars + root plates (root overlaps the wall by >= 1.0 mm, DFM A)
    lip = []
    for (xa, xb, xr0, xr1) in ((CAV_X0 + 0.1, CAV_X0 + 0.1 + 1.26, 1.5, CAV_X0 + 0.15),
                               (CAV_X1 - 0.1 - 1.26, CAV_X1 - 0.1, CAV_X1 - 0.15, CAV_X1 + 1.05)):
        lip.append(box(xa, xb, ARM_Y0, ARM_Y1, ARM_Z0, 3.5))              # bar (free of the plinth floor)
        lip.append(box(xa, xb, ARM_Y0, Y_SEAM, 1.2, 3.5))                 # ... fused to the FACE floor inside the block
        lip.append(box(xa, xb, ARM_Y0, 10.2, 3.4, 7.8))                   # lip band, grows out of the front wall
        lip.append(box(xr0, xr1, ARM_Y0, Y_SEAM, ARM_Z0, 8.0))            # root plate into the wall
    lip.append(box(15.0, 16.26, 0.2, Y_SEAM, 1.2, 3.5))                   # centre web from the front wall
    lip.append(box(15.0, 16.26, Y_SEAM - 0.5, ARM_Y1, ARM_Z0, 3.5))
    face = face + union(*lip) + union(*arm_solids)

    # groove in the disc wall for the BACK bumps, 3 windows
    BUMP_ANG = [(90 + 8 + 37.2), (210 + 8 + 37.2), (330 + 8 + 37.2)]
    bp, gp = back_profiles()
    ring_g = revolve_y(gp, CX, ZC)
    for a_end in BUMP_ANG:
        win = ext_y(sector(CX, ZC, R_CAV - 0.5, R_CAV + 1.0, a_end - 5.0, a_end + 0.8), Y_SEAM - 3.0, Y_SEAM + 0.1)
        face = face - (ring_g ^ win)

    # ---- BACK plate
    back = build_back(bp)
    keep = lambda m: union(*[d for d in m.decompose() if d.volume() > 1.0])   # drop slivers
    face, plinth, back = [finalize(keep(p).simplify(0.004)) for p in (face, plinth, back)]
    return {"face": face, "plinth": plinth, "back": back}, dict(H=H, Hin=Hin, cav=cav, cav_raw=cav_raw, fin=fin, F=F, face_outer=face_outer,
                                                                  tips=tips, body=body)


def back_profiles():
    R_BO = R_CAV - 0.1
    ry0, lead = 5.95, UNDERCUT / math.tan(math.radians(LEAD_DEG))
    ret = UNDERCUT / math.tan(math.radians(RET_DEG))
    land = 0.3
    bump = [(R_BO - 0.05, ry0), (R_BO + UNDERCUT, ry0 + lead), (R_BO + UNDERCUT, ry0 + lead + land),
            (R_BO - 0.05, ry0 + lead + land + ret)]
    g = poly_cs(bump).offset(GROOVE_CLR, JoinType.Round, 2.0, 16)
    gpts = [tuple(p) for p in g.to_polygons()[0]]
    gpts = [(max(r, R_CAV - 0.02), y) for (r, y) in gpts]
    return bump, gpts


BACK_Y0, BACK_Y1 = 5.4, Y_SEAM - 0.2      # 7.55
BACK_ARM_T, BACK_ARM_ARC, BACK_SLOT = 1.26, 12.0, 1.4


def build_back(bump_profile):
    R_BO = R_CAV - 0.1                         # 19.15
    r_in = R_BO - BACK_ARM_T                   # 17.89
    r_body = r_in - BACK_SLOT                  # 16.49
    r_m = 0.5 * (r_in + R_BO)
    a_len = math.degrees(BACK_ARM_ARC / r_m)   # 37.2 deg
    a_root = 8.0                               # root block 8 deg
    b2d = union(circ(CX, ZC, r_body),
                sq(CX - TAB_POCKET_W / 2 + 0.1, CX + TAB_POCKET_W / 2 - 0.1, 9.3, ZC))
    for a in (90, 210, 330):
        b2d = b2d + sector(CX, ZC, r_body - 0.6, R_BO, a, a + a_root)        # root block
        b2d = b2d + sector(CX, ZC, r_in, R_BO, a + a_root - 0.5, a + a_root + a_len)   # tangential arm
    back = ext_y(b2d, BACK_Y0, BACK_Y1)
    # bumps at the free ends (1.2 mm along the arc), lead-in on the -Y (insertion) side
    ring_b = revolve_y(bump_profile, CX, ZC)
    for a in (90, 210, 330):
        a_end = a + a_root + a_len
        a_w = math.degrees(1.2 / (R_BO + 0.25))
        win = ext_y(sector(CX, ZC, R_BO - 0.5, R_BO + 1.0, a_end - a_w - 0.4, a_end - 0.4), BACK_Y0, BACK_Y1)
        back = back + (ring_b ^ win)
    # pads 0.4 mm high on the display side (DFM 37)
    for a in (90, 210, 330):
        px, pz = CX + 15 * math.cos(math.radians(a)), ZC + 15 * math.sin(math.radians(a))
        back = back + ext_y(circ(px, pz, 2.0), BACK_Y0 - 0.4, BACK_Y0 + 0.05)
    return back


# ------------------------------------------------------------------ IO
def to_trimesh(m):
    me = m.to_mesh()
    return trimesh.Trimesh(np.asarray(me.vert_properties)[:, :3], np.asarray(me.tri_verts), process=False)


def corner_normals(tm, angle_deg=32.0):
    fn = tm.face_normals
    F = len(tm.faces)
    vid = tm.faces.ravel()
    fid = np.repeat(np.arange(F), 3)
    order = np.argsort(vid, kind="stable")
    vs, fs = vid[order], fid[order]
    cnt = np.bincount(vid, minlength=len(tm.vertices))
    start = np.concatenate([[0], np.cumsum(cnt)[:-1]])
    rank = np.arange(len(vs)) - start[vs]
    vf = -np.ones((len(tm.vertices), cnt.max()), dtype=int)
    vf[vs, rank] = fs
    area = tm.area_faces
    cosl = math.cos(math.radians(angle_deg))
    out = np.zeros((len(tm.faces), 3, 3))
    for c in range(3):
        nb = vf[tm.faces[:, c]]
        valid = nb >= 0
        nbn = fn[np.where(valid, nb, 0)]
        ok = valid & ((nbn @ fn[:, :, None])[..., 0] > cosl)
        w = np.where(ok, area[np.where(valid, nb, 0)], 0.0)[..., None]
        v = (nbn * w).sum(axis=1)
        out[:, c] = v / np.maximum(np.linalg.norm(v, axis=1, keepdims=True), 1e-12)
    return out


PRINT_T = {
    "face": np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]], float),     # front face down: (x,y,z) -> (x,-z,y)
    "plinth": np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]], float),   # seam face down
    "back": np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]], float),     # rear face down: (x,y,z) -> (x,z,-y)
}


def print_mesh(tm, key):
    R = PRINT_T[key]
    pr = tm.copy()
    T = np.eye(4)
    T[:3, :3] = R
    pr.apply_transform(T)
    t = -np.floor(pr.bounds[0] * 1024) / 1024      # grid-exact shift keeps float32 vertices welded
    pr.apply_translation(t)
    return pr, R, t


def clean_stl(tm):
    """float32-quantise, weld identical vertices, drop collapsed facets (what a slicer does on import).
    Returns (mesh, bad_edge_count)."""
    V = tm.vertices.astype(np.float32)
    u, inv = np.unique(V, axis=0, return_inverse=True)
    G = inv[tm.faces]
    G = G[(G[:, 0] != G[:, 1]) & (G[:, 1] != G[:, 2]) & (G[:, 0] != G[:, 2])]
    out = trimesh.Trimesh(u.astype(np.float64), G, process=False)
    e = np.sort(np.concatenate([G[:, [0, 1]], G[:, [1, 2]], G[:, [2, 0]]]), axis=1)
    _, c = np.unique(e, axis=0, return_counts=True)
    return out, int((c != 2).sum())


def n_components(tm):
    """Connected shells of a mesh by shared vertices (after the vertex merge of trimesh)."""
    parent = np.arange(len(tm.vertices))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for f in tm.faces:
        r0, r1, r2 = find(f[0]), find(f[1]), find(f[2])
        parent[r1] = r0
        parent[find(r2)] = r0
    used = np.unique(tm.faces)
    return len({find(v) for v in used})


def hull2d(pts):
    pts = sorted(set(map(tuple, np.round(pts, 4))))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0:
            up.pop()
        up.append(p)
    return np.array(lo[:-1] + up[:-1])


def tip_angles(com, poly, z0):
    res = []
    n = len(poly)
    h = com[2] - z0
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        e = b - a
        nrm = np.array([e[1], -e[0]]) / np.linalg.norm(e)
        d = -np.dot(com[:2] - a, nrm)
        mid = 0.5 * (a + b)
        res.append((math.degrees(math.atan2(d, h)), nrm))
    return res


def overhang_report(key, tm_assembled, union_m):
    """Faces facing down more than 45 deg from vertical in print orientation (not on the bed)."""
    pr, R, t = print_mesh(tm_assembled, key)
    n = pr.face_normals
    a = pr.area_faces
    zmax = pr.triangles[:, :, 2].max(axis=1)
    cand = (n[:, 2] < -0.72) & (zmax > 0.05)
    idx = np.nonzero(cand)[0]
    tot = a[idx].sum()
    cen = pr.triangles_center[idx]
    nn = n[idx]
    # back to assembled frame
    p0 = (cen - t) @ R           # R orthogonal: inverse = R^T, row-vector form
    n0 = nn @ R
    vis_area, hid_area = 0.0, 0.0
    vis_items = []
    for i in range(len(idx)):
        o = p0[i] + 0.04 * n0[i]
        e = p0[i] + 90.0 * n0[i]
        hits = union_m.ray_cast(tuple(o), tuple(e))
        if len(hits) == 0:
            vis_area += a[idx[i]]
            vis_items.append(p0[i])
        else:
            hid_area += a[idx[i]]
    return tot, vis_area, hid_area, np.array(vis_items)


def arm_numbers(E, xh_off=0.75):
    """Euler-Bernoulli cantilever of varying width (plan taper); tip load at the hook. Returns (force N, max strain %, width at hook)."""
    L = ARM_L
    xh = L - xh_off
    xs = np.linspace(0.0, xh, 600)
    yf0 = ARM_TIP_FRONT - ARM_M * L
    w = ARM_Y1 - np.maximum(yf0 + ARM_M * xs, Y_CAV)           # the part inside the front wall does not count
    I = w * ARM_T ** 3 / 12
    comp = np.trapezoid((xh - xs) ** 2 / (E * I), xs)
    F = DEFL / comp
    eps = F * (xh - xs) * ARM_T / (2 * E * I)
    return F, eps.max() * 100, float(w[-1])


def layer_check(pm, dz=0.2, slopes=(1.0, 1.43), min_area=0.05):
    """Print-frame layer check. A region of layer k not covered by layer k-1 grown by dz*slope is unsupported;
    slope 1.0 = 45 deg from vertical, 1.43 = 55 deg. An island is a part of a layer that touches nothing below it
    (mid-air start). Returns {slope: [(z, area, bbox)]} and the islands."""
    zb = pm.bounding_box()
    zs = np.arange(zb[2] + dz / 2, zb[5], dz)
    secs = [pm.slice(float(z)) for z in zs]
    res = {sl: [] for sl in slopes}
    isl = []
    for k in range(1, len(zs)):
        s, prev = secs[k], secs[k - 1]
        if s.is_empty():
            continue
        for comp in s.decompose():
            if prev.is_empty() or (comp ^ prev).area() < 1e-6:
                isl.append((float(zs[k]), comp.area(), comp.bounds()))
        for sl in slopes:
            if prev.is_empty():
                continue
            un = s - prev.offset(dz * sl + 0.02, JoinType.Round, 2.0, 16)
            for comp in un.decompose():
                if comp.area() > min_area:
                    res[sl].append((float(zs[k]), comp.area(), comp.bounds()))
    return res, isl


def main():
    neck = pick_neck()
    parts, aux = build_housing(neck)
    I = build_internals()
    C = build_clearance()

    print("\npart volumes (mm^3) / connected pieces / genus:")
    for k, v in parts.items():
        print(f"  {k:7s} {v.volume():10.1f}  pieces {len(v.decompose())}  genus {v.genus()}")

    housing = union(*parts.values())
    print("\nintersection volumes housing vs internals (mm^3):")
    tot = 0.0
    for k, v in I.items():
        iv = (housing ^ v).volume()
        tot += iv
        print(f"  {k:12s} {iv:.6f}")
    print(f"  TOTAL housing x internals = {tot:.6f}")
    print("with clearances grown:")
    for k, v in C.items():
        print(f"  {k:16s} {(housing ^ v).volume():.6f}")
    print("smallest gap housing -> internal part (mm):")
    for k in ("board", "esp_shield", "antenna", "display", "tab_glass", "header", "j2_socket", "j2_tails", "usb_recept",
              "usb_shell", "usb_plug", "low_parts"):
        print(f"  {k:12s} {housing.min_gap(I[k], 3.0):.3f}")
    print("part vs part overlap (mm^3):")
    ks = list(parts)
    for i in range(len(ks)):
        for j in range(i + 1, len(ks)):
            print(f"  {ks[i]} x {ks[j]}: {(parts[ks[i]] ^ parts[ks[j]]).volume():.6f}")
    env = union(aux["face_outer"], aux["H"] ^ box(-50, 100, Y_SEAM, 200, -10, 30), aux["body"])
    print("internal volume outside the outer envelope (only the plug may stick out):")
    for k, v in I.items():
        print(f"  {k:12s} {(v - env).volume():9.2f} of {v.volume():9.2f}")
    def ceil_over(x0, x1, y0, y1, zt):
        """Measured on the final housing: lowest material above zt inside the X range, per 0.25 mm Y step (slice of the union)."""
        best = (9e9, None)
        for yy in np.arange(y0, y1 + 1e-6, 0.25):
            sec = sect_y(housing, float(yy)) ^ sq(x0, x1, zt + 0.001, 40.0)
            if sec.is_empty():
                continue
            zmin = sec.bounds()[1]
            if zmin - zt < best[0]:
                best = (zmin - zt, float(yy))
        return best
    for k, zt, xr, yr in (("esp_shield", Z_ESP_TOP, (19.35, 38.85), (26.46, 44.46)), ("usb_recept", Z_USB_TOP, (16.35, 25.35), (54.0, 60.46)),
                          ("j2_socket", Z_J2_TOP, (12.5, 28.5), (10.46, 14.5))):
        c, yy = ceil_over(xr[0], xr[1], yr[0], yr[1], zt)
        print(f"ceiling clearance over {k} (measured on the housing, min along Y): {c:.2f} mm at Y {yy:.2f}")
    # wall thickness of the soft shell, sampled: distance from the cavity surface vertices to the outer envelope surface
    cm = to_trimesh(aux["cav"])
    hm = to_trimesh(aux["H"])
    pts = cm.vertices
    pts = pts[(pts[:, 1] > Y_SEAM + 0.5)]
    ok = hm.area_faces > 0.05
    fn = hm.face_normals[ok]
    d0 = (hm.triangles_center[ok] * fn).sum(axis=1)         # H is convex: distance to the boundary = min over face planes
    dist = np.min(d0[None, :] - pts @ fn.T, axis=1)
    print(f"wall thickness (cavity vertices to outer surface, plinth side): min {dist.min():.2f} mm, 1st percentile {np.percentile(dist, 1):.2f}")
    pr = aux["fin"].rotate((-90, 0, 0)).project()
    print(f"saddle fin outside the front-plate outline (area of its front projection beyond it): {(pr - aux['F']).area():.4f} mm2")
    esp_a = 8 * 1.8 * 16.0
    sens_a = 6 * 1.8 * 11.0 + 3 * 1.8 * 8.0
    c3_esp, c3_all = 6 * 1.8 * 17.0, 6 * 1.8 * 17.0 + 3 * 1.8 * 11.0 + 2 * 1.8 * 11.0 + 2 * 1.8 * 8.0
    print(f"vent open area (nominal): ESP exhaust {esp_a:.0f} mm2 (C3c {c3_esp:.0f}), sensor/intake {sens_a:.0f}, total {esp_a + sens_a:.0f} (C3c {c3_all:.0f})")

    bb = housing.bounding_box()
    size = np.array(bb[3:]) - np.array(bb[:3])
    print(f"\nhousing bbox {np.round(bb, 2).tolist()}  size W {size[0]:.2f} x D {size[1]:.2f} x H {size[2]:.2f}")
    print(f"plinth top above table {Z_TOP + RAIL:.2f}, slab {Z_TOP:.2f}; disc top {ZC + R_DISC + RAIL:.2f}")

    # ---------------- snap strain / force
    print("\nsnap numbers:")
    d = DEFL
    for nm, E in (("PETG", E_PETG), ("PLA", E_PLA)):
        Fh, eps, wtip = arm_numbers(E)
        print(f"  FACE arm ({nm}): t {ARM_T} L {ARM_L} d {d}, plan taper (width {ARM_Y1 - (ARM_TIP_FRONT - ARM_M * ARM_L):.1f} at the root, "
              f"{wtip:.1f} at the hook): strain {eps:.2f} %  force {Fh:.1f} N  pull-out/arm {Fh*math.tan(math.radians(RET_DEG-17)):.1f} N "
              f"(3 arms {3*Fh*math.tan(math.radians(RET_DEG-17)):.1f} N)  insertion/arm {Fh*math.tan(math.radians(LEAD_DEG+17)):.1f} N "
              f"(3 arms {3*Fh*math.tan(math.radians(LEAD_DEG+17)):.1f} N)")
    w6 = 3 * E_PETG * (6.0 * ARM_T ** 3 / 12) * d / ARM_L ** 3
    print(f"  (reference: constant width 6 at L {ARM_L}: strain {1.5*ARM_T*d/ARM_L**2*100:.2f} %, {w6:.1f} N)")
    r_m = R_CAV - 0.1 - BACK_ARM_T / 2
    epsb = 1.5 * BACK_ARM_T * d / BACK_ARM_ARC ** 2
    Ib = (BACK_Y1 - BACK_Y0) * BACK_ARM_T ** 3 / 12
    Wb = 3 * E_PETG * Ib * d / BACK_ARM_ARC ** 3
    print(f"  BACK arm: t {BACK_ARM_T} L {BACK_ARM_ARC} d {d}: strain {epsb*100:.2f} %  force/arm {Wb:.2f} N (PETG), "
          f"insertion {Wb*math.tan(math.radians(LEAD_DEG+17)):.2f} N")
    nk = neck_scan(parts["face"])
    print(f"  neck (weakest section z {nk['z']:.2f}, width {nk['W']:.1f}, A {nk['A']:.1f} mm2): S_front {nk['s_front']:.1f} S_rear {nk['s_rear']:.1f} "
          f"S_side {nk['s_side']:.1f} mm3; stress at 15 N, 41 mm lever: {15*41/nk['s_front']:.1f} / {15*41/nk['s_rear']:.1f} MPa")

    # ---------------- meshes
    os.makedirs(os.path.join(OUT, "stl"), exist_ok=True)
    meshes, prints = {}, {}
    print("\nSTL checks (float32, welded, collapsed facets dropped, as a slicer imports them):")
    for k, v in parts.items():
        tm = to_trimesh(v)
        meshes[k] = tm
        pr, R, t = print_mesh(tm, k)
        prints[k] = pr
        for nm, mesh in (("assembled", tm), ("print", pr)):
            cl, bad = clean_stl(mesh)
            cl.export(os.path.join(OUT, "stl", f"moon4c_{k}_{nm}.stl"))
            rl = trimesh.load(os.path.join(OUT, "stl", f"moon4c_{k}_{nm}.stl"), process=False)
            rl.merge_vertices()
            print(f"  {k:7s} {nm:9s} bad edges {bad}  watertight(reloaded) {rl.is_watertight} winding {rl.is_winding_consistent} "
                  f"shells {n_components(rl)} faces {len(rl.faces)} volume {rl.volume:.1f}  extents {np.round(rl.extents, 1).tolist()}")

    # ---------------- first layer, overhangs
    print("\nprint orientation analysis:")
    for k in parts:
        pr = prints[k]
        yl = {"face": Y_FRONT + 0.1, "plinth": Y_SEAM + 0.1, "back": BACK_Y1 - 0.1}[k]
        fl = sect_y(parts[k], yl).area()
        tot, vis, hid, vis_pts = overhang_report(k, meshes[k], housing)
        print(f"  {k:7s} first layer {fl:7.1f} mm2  footprint {pr.extents[0]:.1f} x {pr.extents[1]:.1f}  height {pr.extents[2]:.1f}  "
              f"overhang>45deg total {tot:6.1f} mm2: visible {vis:6.1f}, hidden {hid:6.1f}")
        cl, bad = clean_stl(pr)
        pm = M(Mesh(vert_properties=cl.vertices.astype(np.float32), tri_verts=cl.faces.astype(np.uint32)))
        res, isl = layer_check(pm)
        print(f"          layer check: islands (mid-air starts) {len(isl)}; unsupported >45deg: "
              f"{len(res[1.0])} regions {sum(r[1] for r in res[1.0]):.1f} mm2; >55deg: {len(res[1.43])} regions "
              f"{sum(r[1] for r in res[1.43]):.1f} mm2")
        for sl in (1.0,):
            for z, a, b in res[sl]:
                print(f"             z {z:5.1f} area {a:6.1f} print x {b[0]:.1f}..{b[2]:.1f} y {b[1]:.1f}..{b[3]:.1f}")
        for z, a, b in isl:
            print(f"             ISLAND z {z:5.1f} area {a:6.1f} x {b[0]:.1f}..{b[2]:.1f} y {b[1]:.1f}..{b[3]:.1f}")
        if len(vis_pts):
            print(f"          visible overhang centroid range x {vis_pts[:,0].min():.1f}..{vis_pts[:,0].max():.1f} "
                  f"y {vis_pts[:,1].min():.1f}..{vis_pts[:,1].max():.1f} z {vis_pts[:,2].min():.1f}..{vis_pts[:,2].max():.1f}")

    # ---------------- centre of mass, tip angles
    mass_shell, mom = 0.0, np.zeros(3)
    for k, tm in meshes.items():
        m = tm.volume * RHO_PETG
        mass_shell += m
        mom += m * tm.center_mass
    allm = trimesh.util.concatenate(list(meshes.values()))
    z0 = allm.bounds[0][2]
    foot = allm.vertices[allm.vertices[:, 2] < z0 + 0.05][:, :2]
    poly = hull2d(foot)
    print(f"\nshell mass (PETG {RHO_PETG*1e3:.2f} g/cm3, 100 % solid as modelled): {mass_shell:.1f} g, "
          f"shell CoM {np.round(mom/mass_shell, 1).tolist()}")
    print(f"foot polygon x {poly[:,0].min():.1f}..{poly[:,0].max():.1f} y {poly[:,1].min():.1f}..{poly[:,1].max():.1f}  table z {z0:.2f}")
    for label, extra in (("12 g at board centre", [((20.85, 35.46, 4.4), 12.0)]),
                         ("5 g display + 7 g board", [((CX, 3.0, ZC), 5.0), ((20.85, 35.46, 4.4), 7.0)])):
        mass, mm = mass_shell, mom.copy()
        for pos, g in extra:
            mass += g
            mm += g * np.asarray(pos)
        com = mm / mass
        worst = {}
        for ang, nrm in tip_angles(com, poly, z0):
            side = ("front" if nrm[1] < -0.7 else "rear" if nrm[1] > 0.7 else "left" if nrm[0] < -0.7 else "right" if nrm[0] > 0.7 else None)
            if side:
                worst[side] = min(worst.get(side, 999), ang)
        print(f"[{label}] total {mass:.1f} g, CoM {np.round(com, 1).tolist()}  tip angles deg "
              f"{ {k: round(v, 1) for k, v in worst.items()} }")

    imesh = {k: to_trimesh(v) for k, v in I.items()}
    np.savez(os.path.join(OUT, "meshes4c.npz"),
             **{f"hn_{k}": corner_normals(m).reshape(-1, 9) for k, m in meshes.items()},
             **{f"h_{k}": m.vertices[m.faces].reshape(-1, 9) for k, m in meshes.items()},
             **{f"i_{k}": m.vertices[m.faces].reshape(-1, 9) for k, m in imesh.items()})
    print("\nwrote STL + meshes.npz")


if __name__ == "__main__":
    main()
