"""v4b "A-frame" with a lift-off coloured roof.
Three parts: FACE (front board: lower gable + round door boss + name plaque frame + 3 snap arms, prints front face down),
BODY (house up to a split plane 12 mm under the eave: floor, walls, integral rear wall with USB pocket + vents, board cradle,
4 snap fingers that stand up inside the roof skirt; prints rear face down),
ROOF (second colour: the two slopes + rounded ridge + the gable top; lifts off by hand; prints eave rim down, flat, no support).
The split plane P(y) = eave height - SKIRT falls to the back with the eave; the roof edge is the main line of the object.
Frame (brief frame, no tilt): X width, Y depth (front 0, back +), Z up, mm.
v4c (after the first v4b print): the board parts come from the real KiCad board (pcb/export_pcb.py -> pcb/pcb_board_frame.npz) and
are checked here (overlap, gaps, slide-in sweep); USB hole on J1 (X 14.85, not centred); board slot 2.6 high (1.9 over the last 6 mm,
no crush bump), 0.6 side clearance; body +1.5 mm per side (antenna gap 2.2); rear wall inner face 0.5 behind the board so the FACE
seats and the arms latch; FACE press fit by crush ribs on the registration ring (RIB_I); intake and side vents moved to the sensor
side (+X, rear right).
Run: uv run --with manifold3d --with trimesh --with numpy --with matplotlib python -I aframe_b.py
"""
import math
import os
import numpy as np
import trimesh
from manifold3d import Manifold as M, CrossSection as CS, JoinType, OpType, Mesh, Error

OUT = os.path.dirname(os.path.abspath(__file__))
SEG = 64

# ------------------------------------------------------------------ parameters
CX, ZC = 20.5, 33.7
R_MOD, TAB_W, TAB_Z0, CLR = 19.0, 23.0, 7.2, 0.25
R_CAV = R_MOD + CLR
R_DISC = R_CAV + 2.0
R_WIN, WIN_CH = 16.7, 0.5
R_RING = R_CAV + 1.68
Y_FRONT, Y_CAV = -0.25, 1.35            # front face; plate rear face = FACE/BODY joint (plate 1.6 thick)
Y_J = Y_CAV
Y_REAR_OUT, Y_REAR_IN = 63.6, 60.96     # inner face 0.5 behind the PCB rear edge (60.46): the FACE can seat fully, J1 sits in the plug hole
CH_SEAM, CH_BED = 0.6, 1.0
CH_REAR_IN, CH_REAR_Y = 0.7, 1.2
Z_BOT, Z_FI = -1.2, 1.4                 # underside (floor 2.6 thick: ballast low, J2 tails 1.9 -> 0.5 clear)
Z_J2_TOP = 7.76                         # display header top (display model; board parts come from pcb/)
TAB_POCKET_W = TAB_W + 2 * CLR
Y_BOSS = 5.0

HW_F, HW_R = 28.2, 24.9                 # v4c: +1.5 per side (antenna gap 0.67 -> 2.2; the board cannot move in X, J2 centres the display)
Y_T0 = 46.0
PITCH = 46.0                            # >45 so that the slopes lean less than 45 deg from vertical when the roof prints eave down
MARGIN = 3.2
ZE_F = ZC + (R_DISC + MARGIN - HW_F * math.sin(math.radians(PITCH))) / math.cos(math.radians(PITCH))
ZE_R = 31.0
R_BASE, R_EAVE, R_RIDGE = 3.0, 5.0, 9.2   # base corner R3: wider flat foot (side tip angle)
WALL = 1.68                             # body and roof slopes
WALL_SKIRT = 2.1                        # roof skirt (vertical part): holds the finger groove (DFM: 2.1 at snaps)
SKIRT = 15.0                            # split plane P(y) = eave(y) - SKIRT
K_TILT = (ZE_F - ZE_R) / Y_REAR_OUT     # slope of P along Y
GAP_H = 0.1                             # roof sits 0.2 above the body (0.1 each side of P): a real reveal, colours align by geometry
CAP_GAP_Y = 0.2                         # roof front edge 0.2 behind the FACE gable plate

R_B = 21.4                              # door boss disc (FACE) that rises through the roof hole
R_H = R_B + 0.2                         # roof hole

# snap arms FACE -> BODY (C4c numbers), hooks run in a floor rail and snap behind a ridge
ARM_T, ARM_L = 1.26, 10.0
UNDERCUT, DEFL = 0.4, 0.5
ARM_Z0 = 1.65
ARM_Z1 = ARM_Z0 + ARM_T
ARM_Y0, ARM_Y1 = 0.4, 10.6
ARM_M, ARM_TIP_FRONT = 0.8, 7.9
LEAD_DEG, RET_DEG = 25.0, 55.0
HOOK_LEN_X = 1.5
GROOVE_CLR = 0.1
E_PETG, E_PLA = 2100.0, 3000.0
RHO_PETG = 1.27e-3

# roof fingers (BODY, in plane XZ, vertical, flex in X) and grooves in the roof skirt
F_L, F_T = 14.0, 1.26                    # finger height above P, thickness
F_D0 = WALL_SKIRT + 0.2                 # tongue outer face at 2.3 from the outer surface (0.2 clearance to the skirt)
F_BUMP, F_LEAD, F_RET = 0.5, 25.0, 30.0
F_TIPW, F_SLOPE = 3.0, 1.73             # tip width; plan slope: finger height grows 1 per 1 of print height (45 deg)
F_YC = (20.0, 38.0)

Z_SHELF = 3.60                          # cradle shelf top = PCB underside (- 0.01)
PCB_T, PCB_T_MAX = 1.6, 1.76            # board thickness, nominal and fab worst case (+10 %)
Z_RIB = Z_SHELF + PCB_T + 1.0           # jaw underside along the slide: 1.0 free above a 1.6 board (v4b: 0.29, with a 0.08 mm crush bump)
Z_RIB_R = Z_SHELF + 1.9                 # last 6 mm at the rear: 0.3 free, locates the USB end; no bump
Y_RAMP0, Y_RAMP1 = 52.0, 55.0           # jaw ramps down from Z_RIB to Z_RIB_R between these depths
Z_JAW = 7.5
SIDE_CLR = 0.6                          # PCB edge to the cradle side walls (v4b 0.3 / 0.4)

# real PCB (pcb/export_pcb.py, from hardware/notifier-v1): board frame x along the 50 mm edge from the USB edge, y along the
# 36 mm edge from the antenna edge, z up from the underside. Housing X = PCB_X0 - y, Y = PCB_Y0 - x, Z = Z_SHELF + 0.01 + z.
PCB_NPZ = os.path.join(OUT, "pcb", "pcb_board_frame.npz")
PCB_X0, PCB_Y0 = 38.85, 60.46           # J2 centre lands on X 20.47 (window CX 20.5)
_pcb = np.load(PCB_NPZ)
PCB = {k: np.c_[PCB_X0 - _pcb[k][:, 1], PCB_Y0 - _pcb[k][:, 0], Z_SHELF + 0.01 + _pcb[k][:, 2]] for k in _pcb.files}
# SHT40 (U5) has no 3D model in the KiCad library: 1.5 x 1.5 x 0.55 DFN at its footprint (layout_v1.py U5 (3.6, 11.2))
PCB["U5"] = np.array([(PCB_X0 - y, PCB_Y0 - x, Z_SHELF + 0.01 + z) for x in (2.85, 4.35) for y in (10.45, 11.95) for z in (1.51, 2.15)])
_j1lo, _j1hi = PCB["J1"].min(0), PCB["J1"].max(0)
USB_X = float(_j1lo[0] + _j1hi[0]) / 2  # 14.85: J1 is 6 mm left of the board centre (seen from the front)
USB_Z = float(_j1hi[2]) - 1.63          # shell 3.26 high: centre 1.63 under its top
USB_FACE_Y = float(_j1hi[1])            # receptacle face (0.25 out of the board edge)
USB_HOLE = (13.2, 8.0)                  # plug overmold has to enter: J1 face sits 2.9 inside the rear face
USB_FUNNEL = 0.8                        # 45 deg lead-in funnel on the rear face (flat spot)
SLOT_W = 1.8
EXHAUST = dict(xs=[CX + 3.4 * k for k in range(-3, 4)], z0=31.0, z1=40.0)    # in the roof's rear gable
INTAKE = dict(xs=[25.6, 29.0, 32.4, 35.8, 39.2], z0=7.6, z1=14.0)            # in the body's rear wall, right of the plug: BME688/SHT40 corner (X 33-39, Y 49-54)
RIDGE_SLOTS = [(12.0, 22.0), (27.0, 37.0), (42.0, 52.0)]                      # along the ridge, under the roof's top
SIDE_VENT = dict(zs=[7.8, 10.9, 14.0], y0=46.5, y1=58.5, x0=43.0)                # 3 slots in the right wall (sensor side), long axis along Y (vertical in print)
# FACE press fit: crush ribs on the outside of the registration ring, pressing into the BODY side walls
RIB_I = 0.20                            # interference per rib, PLA (stiff, brittle: ribs shave rather than squash); coupon variants 0.10 / 0.30
RIB_W, RIB_ZS = 1.0, (11.0, 15.5, 20.0)  # rib width; centres on each side wall
RIB_Y0, RIB_Y1, RIB_Y2 = Y_J + 0.3, 6.5, 8.0   # full height Y0..Y1, lead-in taper to zero at Y2 (the leading end)
RING_OFF = 1.88                         # ring outer face: offset from the outer section (body inner wall at WALL = 1.68)
BALLAST = dict(x0=6.0, x1=35.0, y0=14.0, y1=44.0, d=1.0)                       # underside recess for an 8 g steel plate, 45 deg ends
PLAQUE = dict(cx=CX, cz=8.5, w=30.0, h=8.0, r=3.0)                           # debossed frame on the front, below the door


def hw_at(y):
    t = min(max((y - Y_T0) / (Y_REAR_OUT - Y_T0), 0.0), 1.0)
    return HW_F - (HW_F - HW_R) * t * t * (3 - 2 * t)


def ze_at(y):
    return ZE_F - (ZE_F - ZE_R) * min(max(y / Y_REAR_OUT, 0.0), 1.0)


def p_at(y):
    return ze_at(y) - SKIRT


def rounded_poly(P, R, n=22):
    out = []
    N = len(P)
    for i in range(N):
        p = np.array(P[i], float)
        a = np.array(P[i - 1], float)
        b = np.array(P[(i + 1) % N], float)
        u = (a - p) / np.linalg.norm(a - p)
        v = (b - p) / np.linalg.norm(b - p)
        phi = math.acos(float(np.clip(u @ v, -1, 1)))
        t = R[i] / math.tan(phi / 2)
        c = p + (u + v) / np.linalg.norm(u + v) * R[i] / math.sin(phi / 2)
        t1, t2 = p + u * t, p + v * t
        a1 = math.atan2(t1[1] - c[1], t1[0] - c[0])
        a2 = math.atan2(t2[1] - c[1], t2[0] - c[0])
        d = (a2 - a1 + math.pi) % (2 * math.pi) - math.pi
        for k in range(n + 1):
            ang = a1 + d * k / n
            out.append((c[0] + R[i] * math.cos(ang), c[1] + R[i] * math.sin(ang)))
    return out


def raw_pts(y):
    hw, ze = hw_at(y), ze_at(y)
    ridge = ze + hw * math.tan(math.radians(PITCH))
    return [(CX - hw, Z_BOT), (CX + hw, Z_BOT), (CX + hw, ze), (CX, ridge), (CX - hw, ze)]


def sec_pts(y):
    return rounded_poly(raw_pts(y), [R_BASE, R_BASE, R_EAVE, R_RIDGE, R_EAVE])


def sec_cs(y, off=0.0):
    cs = poly_cs(sec_pts(y))
    return cs.offset(-off, JoinType.Round, 2.0, 64) if off else cs


BIG = 300.0


def zrect(z0, z1):
    return CS.square((2 * BIG, z1 - z0)).translate((-BIG, z0))


def hull_of(*css):
    pts = np.vstack([np.asarray(p) for c in css for p in c.to_polygons()])
    return CS.hull_points(pts)


def sec_lo(y, e=0.0):
    """BODY (below the split plane) section with a 0.6 chamfer along the top edge; e = extra inward offset (end chamfers)."""
    p = p_at(y) - GAP_H
    return hull_of(sec_cs(y, e) ^ zrect(-BIG, p - CH_SEAM), sec_cs(y, e + CH_SEAM) ^ zrect(-BIG, p))


def sec_up(y, e=0.0):
    """ROOF (above the split plane) section with a 0.6 chamfer along the eave edge."""
    p = p_at(y) + GAP_H
    return hull_of(sec_cs(y, e) ^ zrect(p + CH_SEAM, BIG), sec_cs(y, e + CH_SEAM) ^ zrect(p, BIG))


def loft_secs(entries, fn):
    """entries: (y, e). Union of convex hulls of consecutive sections fn(y, e)."""
    rings = [(y, np.asarray(fn(y, e).to_polygons()[0])) for y, e in entries]
    parts = []
    for (ya, pa), (yb, pb) in zip(rings[:-1], rings[1:]):
        P = np.array([(x, ya, z) for x, z in pa] + [(x, yb, z) for x, z in pb])
        parts.append(M.hull_points(P))
    return union(*parts)


def loft_off(ys, off):
    return loft_secs([(y, 0.0) for y in ys], lambda y, e: sec_cs(y, off))


def grid():
    ys = [Y_J + CH_SEAM] + list(np.linspace(4.0, Y_T0, 15)) + list(np.linspace(Y_T0, Y_REAR_IN, 8)[1:]) + [Y_REAR_OUT - CH_REAR_Y]
    return [round(float(v), 4) for v in ys]


def inner_up_cs(y, dzt):
    """Cavity of the roof: unrounded pentagon offset by the slope wall, vertical skirt wall WALL_SKIRT, apex cut flat at dzt below the virtual apex
    (a flat <= 8 mm bridge instead of an arch)."""
    hw = hw_at(y)
    raw = raw_pts(y)
    cs = poly_cs(raw).offset(-WALL, JoinType.Miter, 4.0, 16)
    cs = cs ^ CS.square((2 * (hw - WALL_SKIRT), 2 * BIG)).translate((CX - (hw - WALL_SKIRT), -BIG))
    return cs ^ zrect(-BIG, raw[3][1] - dzt)


def solve_dzt():
    """Smallest apex cut so that the roof wall is >= 1.2 mm everywhere at the front section; returns (dzt, flat width)."""
    for dz in np.arange(3.0, 10.0, 0.1):
        cav = inner_up_cs(Y_FRONT, float(dz))
        thin = cav - sec_cs(Y_FRONT, 1.2)
        if thin.area() < 1e-4:
            top = cav.bounds()
            b = (cav ^ zrect(raw_pts(Y_FRONT)[3][1] - dz - 0.05, BIG)).bounds()
            return float(dz), float(b[2] - b[0])
    raise RuntimeError("no dzt")


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


# ------------------------------------------------------------------ internals
PLUG_LEN = 16.0


def build_internals():
    I = {}
    I["board"] = pcb_board(PCB_T)
    for r in pcb_refs():
        I[f"pcb_{r}"] = M.hull_points(PCB[r])
    # plug overmold 12 x 7 (most cables), mated: overmold face 0.0 behind the receptacle face
    I["usb_plug"] = box(USB_X - 6.0, USB_X + 6.0, USB_FACE_Y, USB_FACE_Y + PLUG_LEN, USB_Z - 3.5, USB_Z + 3.5)
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
    return C


def pcb_refs():
    return sorted(k for k in PCB if k != "board")


def pcb_board(t):
    """Bare PCB as a slab of thickness t on the shelf (outline hull: the sensor notch is ignored, conservative)."""
    b = PCB["board"]
    return box(b[:, 0].min(), b[:, 0].max(), b[:, 1].min(), b[:, 1].max(), Z_SHELF + 0.01, Z_SHELF + 0.01 + t)


def pcb_assembly(t=PCB_T_MAX):
    """Board (worst-case thickness) + every part hull: the thing that slides into the BODY."""
    return union(pcb_board(t), *[M.hull_points(PCB[r]) for r in pcb_refs()])


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



# ------------------------------------------------------------------ numbers
def arm_numbers(E, xh_off=0.75):
    L = ARM_L
    xh = L - xh_off
    xs = np.linspace(0.0, xh, 600)
    yf0 = ARM_TIP_FRONT - ARM_M * L
    w = ARM_Y1 - np.maximum(yf0 + ARM_M * xs, Y_CAV)
    I = w * ARM_T ** 3 / 12
    comp = np.trapezoid((xh - xs) ** 2 / (E * I), xs)
    F = DEFL * 1.0 / comp
    eps = F * (xh - xs) * ARM_T / (2 * E * I)
    return F, eps.max() * 100, float(w[-1])


def finger_geom():
    ya_off = -6.0
    wroot = F_L / F_SLOPE + F_TIPW
    return ya_off, wroot


def finger_numbers(E, d):
    """Cantilever with plan taper: width w(zeta) = wroot - zeta/slope (slope edge), tip load at the bump centre."""
    _, wroot = finger_geom()
    zc = F_L - 3.2 + F_BUMP / math.tan(math.radians(F_RET)) + 0.15
    xs = np.linspace(0.0, zc, 600)
    w = wroot - xs / F_SLOPE
    I = w * F_T ** 3 / 12
    comp = np.trapezoid((zc - xs) ** 2 / (E * I), xs)
    F = d / comp
    eps = F * (zc - xs) * F_T / (2 * E * I)
    return F, eps.max() * 100, float(w[-1]), zc


# ------------------------------------------------------------------ housing
def prism_yz(poly_yz, x0, x1):
    return M.hull_points(np.array([(x, y, z) for x in (x0, x1) for (y, z) in poly_yz]))


def rrect(cx, cz, w, h, r):
    cs = CS.square((w, h)).translate((cx - w / 2, cz - h / 2))
    return cs.offset(-r, JoinType.Round, 2.0, 24).offset(r, JoinType.Round, 2.0, 24)


def bump_profile(d0, z0):
    """(d, zeta) polygon of the finger bump: d measured inward from the outer surface; bump grows to smaller d (towards the roof skirt)."""
    ret = F_BUMP / math.tan(math.radians(F_RET))
    lead = F_BUMP / math.tan(math.radians(F_LEAD))
    land = 0.3
    return [(d0 + 0.05, z0), (d0 - F_BUMP, z0 + ret), (d0 - F_BUMP, z0 + ret + land), (d0 + 0.05, z0 + ret + land + lead)]


def finger_parts(side, yc):
    """Returns (tongue+pad solid, bump solid, groove cutter for the roof)."""
    xo = CX - HW_F if side < 0 else CX + HW_F
    s = 1.0 if side < 0 else -1.0
    ya = yc + finger_geom()[0]
    yb = ya + finger_geom()[1]
    d0, d1 = F_D0, F_D0 + F_T
    xa, xb = sorted((xo + s * d0, xo + s * d1))
    # tongue: (y, z) trapezoid with the slanted edge on the large-Y side (grows 45 deg per layer in print)
    pr = [(ya, p_at(ya) - 1.0), (yb, p_at(yb) - 1.0), (yb, p_at(yb)),
          (yb - F_L / F_SLOPE, p_at(yb - F_L / F_SLOPE) + F_L), (ya, p_at(ya) + F_L)]
    tongue = prism_yz(pr, xa, xb)
    # pad (thickened wall under the tongue), rear end sloped 45 deg
    y0p, y1p = ya - 1.0, yb + 0.5
    pp = [(y0p, p_at(y0p) - 8.0), (y1p, p_at(y1p) - 8.0), (y1p, p_at(y1p)), (y0p, p_at(y0p))]
    pad = prism_yz(pp, *sorted((xo + s * 0.6, xo + s * d1)))
    C = y1p + WALL
    k = 1 / math.sqrt(2)
    tp = tongue + pad
    if side < 0:
        tp = tp.trim_by_plane((-k, -k, 0), -(C + xo) * k)
    else:
        tp = tp.trim_by_plane((k, -k, 0), (xo - C) * k)
    # bump
    z0 = F_L - 3.2
    yb0, yb1 = ya + 0.3, ya + F_TIPW
    prof = bump_profile(d0, z0)

    def solid_from(profile, y0, y1):
        pts = [(xo + s * d, y, p_at(y) + z) for y in (y0, y1) for (d, z) in profile]
        return M.hull_points(np.array(pts))
    bump = solid_from(prof, yb0, yb1)
    g = poly_cs(prof).offset(GROOVE_CLR, JoinType.Round, 2.0, 16)
    gp = [tuple(p) for p in g.to_polygons()[0]]
    dmax = max(p[0] for p in gp) + 0.5
    zmin, zmax = min(p[1] for p in gp), max(p[1] for p in gp)
    gp = gp + [(dmax, zmin), (dmax, zmax)]
    groove = solid_from(gp, yb0 - 0.3, yb1 + 0.3)
    return tp, bump, groove


def build_housing():
    dzt, flat_w = solve_dzt()
    ys = grid()
    # ---------------- outer solids
    plate = loft_secs([(Y_FRONT, CH_BED), (Y_FRONT + 1.0, 0.0), (Y_J, CH_SEAM)], sec_cs)
    body_out = loft_secs([(Y_J, CH_SEAM)] + [(y, 0.0) for y in ys] + [(Y_REAR_OUT, CH_REAR_IN)], sec_lo)
    cap_f = Y_J + CAP_GAP_Y
    cap_out = loft_secs([(cap_f, CH_SEAM), (cap_f + CH_SEAM, 0.0)] + [(y, 0.0) for y in ys[1:]] + [(Y_REAR_OUT, CH_REAR_IN)], sec_up)
    floor = box(-100, 200, -10, 100, Z_FI, 200)

    # ============ FACE
    face = plate
    tw = TAB_POCKET_W / 2 + 1.68
    face = face + ext_y(circ(CX, ZC, R_RING), Y_J - 0.1, Y_BOSS) + ext_y(sq(CX - tw, CX + tw, TAB_Z0 - CLR - 1.65, ZC), Y_J - 0.1, Y_BOSS)
    face = face - ext_y(circ(CX, ZC, R_CAV), Y_J, Y_BOSS + 0.5)
    face = face - ext_y(sq(CX - TAB_POCKET_W / 2, CX + TAB_POCKET_W / 2, TAB_Z0 - CLR, ZC), Y_J, Y_J + 8.0)
    face = face - cyl_y(CX, ZC, R_WIN + WIN_CH, R_WIN, Y_FRONT - 0.01, Y_FRONT + WIN_CH)
    face = face - cyl_y(CX, ZC, R_WIN, R_WIN, Y_FRONT + WIN_CH - 0.01, Y_J + 0.1)
    # display crush ribs (friction hold before the house is closed): 3 x trapezoid 0.8 base x 0.35 high, 0.10 interference on the 38.0 mm PCB,
    # at 90 / 210 / 330 deg (top, and clear of the glass-tab zone at the bottom), running along Y (vertical in print), lead-in taper at the rear end
    for ang in (90, 210, 330):
        a = math.radians(ang)
        hw_ = 0.4 / R_CAV
        hn_ = 0.15 / R_CAV
        def pt(r, th, y):
            return (CX + r * math.cos(th), y, ZC + r * math.sin(th))
        pts = []
        for y in (Y_J - 0.05, Y_BOSS - 0.9):
            pts += [pt(R_CAV + 0.15, a - hw_, y), pt(R_CAV + 0.15, a + hw_, y), pt(R_CAV - 0.35, a - hn_, y), pt(R_CAV - 0.35, a + hn_, y)]
        pts += [pt(R_CAV + 0.15, a - hw_, Y_BOSS), pt(R_CAV + 0.15, a + hw_, Y_BOSS), pt(R_CAV, a, Y_BOSS)]
        face = face + M.hull_points(np.array(pts))
    # name plaque: debossed rounded frame below the door (3 layer steps, 45 deg, bridges 0.6 mm only)
    base = rrect(PLAQUE["cx"], PLAQUE["cz"], PLAQUE["w"], PLAQUE["h"], PLAQUE["r"])
    for (wd, y0, y1) in [(1.25 - 0.19 * i, Y_FRONT + 0.1 * i - (0.1 if i == 0 else 0.0), Y_FRONT + 0.1 * (i + 1)) for i in range(6)]:
        ring = base.offset(wd / 2, JoinType.Round, 2.0, 24) - base.offset(-wd / 2, JoinType.Round, 2.0, 24)
        face = face - ext_y(ring, y0, y1)
    # registration lip: ring that enters the BODY with 0.2 clearance, only on the walls (above Z 9, below the split)
    yr = [Y_J - 0.2, 4.7, 8.0]
    ring3 = (loft_off(yr, RING_OFF) - loft_off(yr, 3.14)) ^ box(-100, 200, -10, 100, 9.0, p_at(8.0) - 1.0)
    face = face + ring3
    # snap arms (C4c numbers)
    spec = [(3.91, +1), (16.26, +1), (37.89, -1)]
    y_hook0 = Y_J + 7.05
    arm_solids, grooves, tips = [], [], []
    for xr, dr in spec:
        a, g, xt, rl = arm_set(xr, dr, y_hook0)
        arm_solids.append(a)
        grooves.append(g)
        tips.append((xt, dr))
    lip = []
    for (xa, xb) in ((2.65, 3.91 + 0.3), (37.89 - 0.3, 39.15)):
        lip.append(box(xa, xb, ARM_Y0, ARM_Y1, ARM_Z0, 3.5))
        lip.append(box(xa, xb, ARM_Y0, 6.0, ARM_Z0, 8.0))
    lip.append(box(15.0, 16.26, 0.2, ARM_Y1, ARM_Z0, 3.5))
    face = face + union(*lip) + union(*arm_solids)
    face_base = face
    face = face_base + face_ribs(RIB_I)

    # ============ BODY
    cav_ys = [0.5, Y_J + CH_SEAM] + [y for y in ys[1:] if y <= Y_REAR_IN + 1e-9]
    cav_body = loft_off(cav_ys, WALL) ^ floor
    body = body_out - cav_body
    k = 1 / math.sqrt(2)

    def jaw(xa, xb, y0, y1):
        """Hold-down lip over the board edge: Z_RIB high along the slide, ramps to Z_RIB_R over the last mm (USB end); no crush bump.
        45 deg entry chamfer at the front end."""
        pieces = [box(xa, xb, y0, Y_RAMP0, Z_RIB, Z_JAW),
                  prism_yz([(Y_RAMP0, Z_RIB), (Y_RAMP1, Z_RIB_R), (Y_RAMP1, Z_JAW), (Y_RAMP0, Z_JAW)], xa, xb),
                  box(xa, xb, Y_RAMP1, y1, Z_RIB_R, Z_JAW)]
        return union(*pieces).trim_by_plane((0, k, k), (y0 + Z_JAW) * k)

    Y_C0, Y_C1 = 11.2, 61.0
    bx0, bx1 = PCB["board"][:, 0].min(), PCB["board"][:, 0].max()
    xl, xr_ = bx0 - SIDE_CLR, bx1 + SIDE_CLR                    # cradle side walls
    cr_ = [box(xl - 1.6, 4.85, Y_C0, Y_C1, Z_FI - 0.2, Z_SHELF), box(xl - 1.6, xl, Y_C0, Y_C1, Z_SHELF - 0.1, Z_JAW), jaw(xl - 0.05, 4.3, Y_C0, Y_C1),
           box(36.85, xr_ + 1.6, Y_C0, Y_C1, Z_FI - 0.2, Z_SHELF),
           box(xr_, xr_ + 1.6, 45.2, Y_C1, Z_SHELF - 0.1, Z_JAW), jaw(37.9, xr_ + 0.05, 45.2, Y_C1)]   # right lip 0.95 over the board: C5 (X <= 37.4, 1 mm high) passes beside it
    body = body + (union(*cr_) ^ body_out)

    # floor rails for the arm hooks + ridge, then the pocket behind the ridge
    for (xt, dr) in tips:
        hx0, hx1 = (xt - HOOK_LEN_X, xt) if dr > 0 else (xt, xt + HOOK_LEN_X)
        ye = y_hook0 - 1.74
        rail = prism_yz([(0.5, 0.8), (ye, 0.8), (ye + 1.04, 1.4), (ye + 1.04, 1.9), (0.5, 1.9)], hx0 - 0.2, hx1 + 0.2)
        body = body - rail
    for g in grooves:
        body = body - g
    # intake slots (rear wall, beside the sensor corner) and USB plug pocket with a 45 deg funnel on the rear face
    cutters = [box(x - SLOT_W / 2, x + SLOT_W / 2, Y_REAR_IN - 0.3, Y_REAR_OUT + 1, INTAKE["z0"], INTAKE["z1"]) for x in INTAKE["xs"]]
    body = body - union(*cutters)
    hole = rrect(USB_X, USB_Z, USB_HOLE[0], USB_HOLE[1], 1.0)
    body = body - ext_y(hole, Y_REAR_IN - 0.3, Y_REAR_OUT + 2.0)
    fun_o = np.asarray(rrect(USB_X, USB_Z, USB_HOLE[0] + 2 * USB_FUNNEL, USB_HOLE[1] + 2 * USB_FUNNEL, 1.0 + USB_FUNNEL).to_polygons()[0])
    fun_i = np.asarray(hole.to_polygons()[0])
    fpts = [(x, Y_REAR_OUT + 0.02, z) for x, z in fun_o] + [(x, Y_REAR_OUT - USB_FUNNEL, z) for x, z in fun_i]
    body = body - M.hull_points(np.array(fpts))
    # side intake at the sensor corner (right wall) and a ballast recess in the underside
    for zc_ in SIDE_VENT["zs"]:
        ya_, yb_, h_ = SIDE_VENT["y0"], SIDE_VENT["y1"], SLOT_W / 2
        body = body - prism_yz([(yb_, zc_ - h_), (yb_, zc_ + h_), (ya_ + 1.0, zc_ + h_), (ya_, zc_), (ya_ + 1.0, zc_ - h_)], SIDE_VENT["x0"], 80.0)
    by0, by1, bx0, bx1, bd = BALLAST["y0"], BALLAST["y1"], BALLAST["x0"], BALLAST["x1"], BALLAST["d"]
    rl = bd * 1.2
    body = body - prism_yz([(by0, Z_BOT - 0.01), (by1, Z_BOT - 0.01), (by1 - rl, Z_BOT + bd), (by0 + rl, Z_BOT + bd)], bx0, bx1)
    # fingers + pads (tongue without and with the bump)
    fing, bumps, grooves_cap = [], [], []
    for side in (-1, +1):
        for yc in F_YC:
            t, b, g = finger_parts(side, yc)
            fing.append(t)
            bumps.append(b)
            grooves_cap.append(g)
    body_nb = body + union(*fing)
    body = body_nb + union(*bumps)

    # ============ ROOF
    cav_up_ys = [cap_f - 1.0] + [y for y in ys if y <= Y_REAR_IN + 1e-9]
    cav_up = loft_secs([(y, 0.0) for y in cav_up_ys], lambda y, e: inner_up_cs(y, dzt))
    cap = cap_out - cav_up
    for g in grooves_cap:
        cap = cap - g
    cut = []
    for x in EXHAUST["xs"]:
        cut.append(box(x - SLOT_W / 2, x + SLOT_W / 2, Y_REAR_IN - 0.3, Y_REAR_OUT + 1, EXHAUST["z0"], EXHAUST["z1"]))
    for (ya, yb) in RIDGE_SLOTS:
        zl = lambda y: raw_pts(y)[3][1] - 12.0
        cut.append(prism_yz([(ya, zl(ya)), (yb, zl(yb)), (yb, 200.0), (ya, 200.0)], CX - SLOT_W / 2, CX + SLOT_W / 2))
    cap = cap - union(*cut)

    keep = lambda m: union(*[d for d in m.decompose() if d.volume() > 1.0])
    out = {}
    for nm, m in (("face", face), ("body", body), ("cap", cap)):
        out[nm] = finalize(keep(m).simplify(0.004))
    face_var = {i: finalize(keep(face_base + face_ribs(i)).simplify(0.004)) for i in RIB_VARIANTS}
    return out, dict(plate=plate, body_out=body_out, cap_out=cap_out, body_nb=finalize(keep(body_nb).simplify(0.004)), dzt=dzt, flat_w=flat_w,
                     face_base=finalize(keep(face_base).simplify(0.004)), face_var=face_var)


RIB_VARIANTS = (0.10, 0.30)


def face_ribs(interf):
    """Crush ribs on the FACE registration ring, both side walls: radial from inside the ring to `interf` past the BODY inner wall.
    Run along Y (vertical when the FACE prints front down); the leading end (Y2) tapers to the ring surface (lead-in)."""
    out = []
    for side in (-1, 1):
        x_ring = CX + side * (HW_F - RING_OFF)
        x_wall = CX + side * (HW_F - WALL)
        x_in = x_ring - side * 0.3
        x_out = x_wall + side * interf
        for zc in RIB_ZS:
            z0, z1 = zc - RIB_W / 2, zc + RIB_W / 2
            ych = RIB_Y0 + abs(x_out - x_ring)           # 45 deg start (bottom end in print), no mid-air ledge
            pts = [(x, y, z) for y in (ych, RIB_Y1) for x in (x_in, x_out) for z in (z0, z1)]
            pts += [(x, y, z) for y in (RIB_Y0, RIB_Y2) for x in (x_in, x_ring) for z in (z0, z1)]
            out.append(M.hull_points(np.array(pts)))
    return union(*out)


# ------------------------------------------------------------------ main
THETA = math.atan(K_TILT)
PRINT_T = {
    "face": np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]], float),
    "body": np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]], float),
    "cap": np.array([[1, 0, 0], [0, math.cos(THETA), -math.sin(THETA)], [0, math.sin(THETA), math.cos(THETA)]], float),
}
PRINT_ORIENT = {"face": Y_FRONT + 0.1, "body": Y_REAR_OUT - 0.1}


def main():
    parts, aux = build_housing()
    I = build_internals()
    C = build_clearance()
    print(f"section: HW {HW_F} -> {HW_R}, eave Z {ZE_F:.2f} -> {ZE_R}, pitch {PITCH} deg, radii base/eave/ridge {R_BASE}/{R_EAVE}/{R_RIDGE}, split plane P = eave - {SKIRT} "
          f"(Z {p_at(0):.1f} front, {p_at(Y_REAR_OUT):.1f} rear)")
    print(f"roof inner apex cut {aux['dzt']:.1f} mm below the virtual apex: flat bridge {aux['flat_w']:.1f} mm wide; roof wall {WALL}, skirt wall {WALL_SKIRT}")
    print("\npart volumes (mm^3) / connected pieces / genus:")
    for k, v in parts.items():
        print(f"  {k:6s} {v.volume():10.1f}  pieces {len(v.decompose())}  genus {v.genus()}")
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
    print("smallest gap housing -> display / plug (mm):")
    for k in ("display", "tab_glass", "header", "usb_plug"):
        print(f"  {k:12s} {housing.min_gap(I[k], 3.0):.3f}")
    print("real PCB (pcb/pcb_board_frame.npz) vs housing: overlap mm^3 / smallest gap mm (parts closer than 1.0 mm):")
    pcb_bad = 0.0
    for r in pcb_refs():
        h = I[f"pcb_{r}"]
        ov, gp = (housing ^ h).volume(), housing.min_gap(h, 1.0)
        pcb_bad += ov
        if ov > 1e-6 or gp < 1.0:
            lo, hi = PCB[r].min(0), PCB[r].max(0)
            print(f"  {r:4s} overlap {ov:.4f}  gap {gp:.2f}  (X {lo[0]:.1f}..{hi[0]:.1f} Y {lo[1]:.1f}..{hi[1]:.1f} Z {lo[2]:.1f}..{hi[2]:.1f})")
    bb_ = pcb_board(PCB_T_MAX)
    print(f"  board {PCB_T_MAX} thick: overlap {(housing ^ bb_).volume():.4f}  (all parts overlap total {pcb_bad:.4f})")
    b = PCB["board"]
    print(f"  board X {b[:, 0].min():.2f}..{b[:, 0].max():.2f}: side gaps to the cradle walls {SIDE_CLR} / {SIDE_CLR}; slot height {Z_RIB - Z_SHELF:.2f} "
          f"(free {Z_RIB - Z_SHELF - PCB_T:.2f} at 1.6, {Z_RIB - Z_SHELF - PCB_T_MAX:.2f} at {PCB_T_MAX}), rear {Z_RIB_R - Z_SHELF:.2f} from Y {Y_RAMP1}")
    print(f"  rear: PCB edge {b[:, 1].max():.2f} -> rear wall {Y_REAR_IN:.2f} = {Y_REAR_IN - b[:, 1].max():.2f} float; J1 face {USB_FACE_Y:.2f} inside the plug hole")
    u1 = PCB["U1"]
    print(f"  antenna X {u1[:, 0].max():.2f} -> inner wall X {CX + HW_F - WALL:.2f} = {CX + HW_F - WALL - u1[:, 0].max():.2f}")
    print(f"  USB hole centre X {USB_X:.2f} Z {USB_Z:.2f} = J1 shell centre (from the KiCad model); J1 face {Y_REAR_OUT - USB_FACE_Y:.2f} inside the rear face")
    asm = pcb_assembly()
    sweep = [(dy, (asm.translate((0, -dy, 0)) ^ parts["body"]).volume()) for dy in np.arange(0.0, 62.0, 0.5)]
    worst = max(sweep, key=lambda t: t[1])
    print(f"  slide-in (board {PCB_T_MAX} + all parts moved out to the front in 0.5 mm steps, BODY only): max overlap {worst[1]:.4f} mm^3 at {worst[0]:.1f} mm")
    print("part vs part overlap (mm^3):")
    ks = list(parts)
    for i in range(len(ks)):
        for j in range(i + 1, len(ks)):
            print(f"  {ks[i]} x {ks[j]}: {(parts[ks[i]] ^ parts[ks[j]]).volume():.6f}")
    print(f"  face without ribs x body: {(aux['face_base'] ^ parts['body']).volume():.6f}; ribs (interference {RIB_I}) x body: "
          f"{(face_ribs(RIB_I) ^ parts['body']).volume():.3f} (by design)")
    env = union(aux["plate"], aux["body_out"], aux["cap_out"])
    print("internal volume outside the outer envelope (only the plug may stick out):")
    for k, v in I.items():
        print(f"  {k:12s} {(v - env).volume():9.2f} of {v.volume():9.2f}")
    print("roof lift-off sweep (roof moved straight up, fingers without bumps, FACE in place): interference volume mm^3")
    rest = parts["face"] + aux["body_nb"]
    print("  " + "  ".join(f"dz {dz}: {(parts['cap'].translate((0, 0, dz)) ^ rest).volume():.4f}" for dz in (0.3, 1, 3, 6, 12, 25, 50)))
    dzs = np.linspace(0.3, 14.0, 30)
    print(f"  max over 30 steps to 14 mm: {max((parts['cap'].translate((0, 0, float(dz))) ^ rest).volume() for dz in dzs):.4f}")
    print(f"  bumps vs roof wall (the click, by design): {(parts['cap'] ^ parts['body']).volume():.4f}")

    def ceil_over(x0, x1, y0, y1, zt):
        best = (9e9, None)
        for yy in np.arange(y0, y1 + 1e-6, 0.25):
            sec = sect_y(housing, float(yy)) ^ sq(x0, x1, zt + 0.001, 80.0)
            if sec.is_empty():
                continue
            zmin = sec.bounds()[1]
            if zmin - zt < best[0]:
                best = (zmin - zt, float(yy))
        return best
    for r in ("U1", "J1", "J2"):
        lo, hi = PCB[r].min(0), PCB[r].max(0)
        cc, yy = ceil_over(lo[0], hi[0], lo[1], min(hi[1], Y_REAR_IN - 0.01), hi[2])
        print(f"ceiling clearance over {r} (measured on the housing, min along Y): {cc:.2f} mm at Y {yy:.2f}")
    print(f"antenna to housing: {housing.min_gap(I['pcb_U1'], 5.0):.3f} mm (needs 0.5)")
    print(f"walls by construction: slopes {WALL}, skirt {WALL_SKIRT}, body {WALL}, floor {Z_FI - Z_BOT:.2f}, rear wall {Y_REAR_OUT - Y_REAR_IN:.2f}, front plate {Y_J - Y_FRONT:.2f}")

    rear_in = sec_cs(Y_REAR_OUT, 2.5)
    for nm, V in (("exhaust (roof rear gable)", EXHAUST), ("intake (body rear wall)", INTAKE)):
        bad = 0.0
        for x in V["xs"]:
            r = CS.square((SLOT_W, V["z1"] - V["z0"])).translate((x - SLOT_W / 2, V["z0"]))
            bad += (r - rear_in).area()
        a = len(V["xs"]) * SLOT_W * (V["z1"] - V["z0"])
        print(f"vent {nm}: {len(V['xs'])} slots {SLOT_W} x {V['z1'] - V['z0']:.1f} = {a:.0f} mm2, outside rear outline-2.5: {bad:.3f} mm2")
    ridge_a = sum((b - a) * SLOT_W for a, b in RIDGE_SLOTS)
    side_a = len(SIDE_VENT["zs"]) * SLOT_W * (SIDE_VENT["y1"] - SIDE_VENT["y0"])
    print(f"vent side (right wall, sensor corner): {len(SIDE_VENT['zs'])} slots {SLOT_W} x {SIDE_VENT['y1']-SIDE_VENT['y0']:.1f} = {side_a:.0f} mm2")
    sa = side_a + len(INTAKE["xs"]) * SLOT_W * (INTAKE["z1"] - INTAKE["z0"]) + len(EXHAUST["xs"]) * SLOT_W * (EXHAUST["z1"] - EXHAUST["z0"]) + ridge_a
    print(f"vent ridge (roof, high): {len(RIDGE_SLOTS)} slots {SLOT_W} x {RIDGE_SLOTS[0][1]-RIDGE_SLOTS[0][0]:.0f} = {ridge_a:.0f} mm2; total slots {sa:.0f} mm2 + plug pocket ring {USB_HOLE[0]*USB_HOLE[1]-12*7:.0f} mm2")
    print(f"USB: plug pocket {USB_HOLE[0]} x {USB_HOLE[1]} through the {Y_REAR_OUT - Y_REAR_IN:.2f} wall, rear face flat spot with {USB_FUNNEL} x 45 deg funnel (opening {USB_HOLE[0]+2*USB_FUNNEL:.1f} x {USB_HOLE[1]+2*USB_FUNNEL:.1f}); plug gap {housing.min_gap(I['usb_plug'], 3.0):.2f} mm")

    bb = housing.bounding_box()
    size = np.array(bb[3:]) - np.array(bb[:3])
    print(f"\nhousing bbox {np.round(bb, 2).tolist()}  size W {size[0]:.2f} x D {size[1]:.2f} x H {size[2]:.2f}")
    print(f"outer solid volume {env.volume()/1000:.1f} cm3, shell volume {housing.volume()/1000:.1f} cm3, envelope box {np.prod(size)/1000:.1f} cm3")

    print("\nsnap numbers:")
    for nm, E in (("PETG", E_PETG), ("PLA", E_PLA)):
        Fh, eps, wtip = arm_numbers(E)
        print(f"  FACE arm ({nm}): strain {eps:.2f} %  force {Fh:.1f} N  pull-out/arm {Fh*math.tan(math.radians(RET_DEG+17)):.1f} N "
              f"(3 arms {3*Fh*math.tan(math.radians(RET_DEG+17)):.1f} N; DFM formula tan(a-17): {3*Fh*math.tan(math.radians(RET_DEG-17)):.1f} N)  insertion/arm {Fh*math.tan(math.radians(LEAD_DEG+17)):.1f} N (3 arms {3*Fh*math.tan(math.radians(LEAD_DEG+17)):.1f} N)")
    d_f = F_BUMP - (F_D0 - WALL_SKIRT) + 0.1
    for nm, E in (("PETG", E_PETG), ("PLA", E_PLA)):
        Fh, eps, wtip, zc = finger_numbers(E, d_f)
        pull = Fh * math.tan(math.radians(F_RET + 17.0))
        ins = Fh * math.tan(math.radians(F_LEAD + 17.0))
        print(f"  ROOF finger ({nm}): t {F_T} L {F_L} deflection {d_f:.2f}, root width {finger_geom()[1]:.1f} -> {wtip:.1f} at the bump: strain {eps:.2f} % (limit PETG 2.5 % in plane)  "
              f"force {Fh:.2f} N  lift-off per finger {pull:.1f} N, 4 fingers {4*pull:.1f} N; press-on {ins:.1f} N, 4 fingers {4*ins:.1f} N")

    os.makedirs(os.path.join(OUT, "stl"), exist_ok=True)
    meshes, prints = {}, {}
    print("\nSTL checks (float32, welded, collapsed facets dropped, as a slicer imports them; reloaded from the exported file):")
    for k, v in parts.items():
        tm = to_trimesh(v)
        meshes[k] = tm
        pr, R, t = print_mesh(tm, k)
        prints[k] = pr
        for nm, mesh in (("assembled", tm), ("print", pr)):
            cl, bad = clean_stl(mesh)
            fn = os.path.join(OUT, "stl", f"aframe_b_{k}_{nm}.stl")
            cl.export(fn)
            rl = trimesh.load(fn, process=False)
            rl.merge_vertices()
            print(f"  {k:6s} {nm:9s} bad edges {bad}  watertight(reloaded) {rl.is_watertight} winding {rl.is_winding_consistent} "
                  f"shells {n_components(rl)} faces {len(rl.faces)} volume {rl.volume:.1f}  extents {np.round(rl.extents, 1).tolist()}")

    for i, v in aux["face_var"].items():
        pr, _, _ = print_mesh(to_trimesh(v), "face")
        cl, bad = clean_stl(pr)
        fn = os.path.join(OUT, "stl", f"aframe_b_face_rib{int(round(i * 100)):03d}_print.stl")
        cl.export(fn)
        print(f"  face variant rib interference {i}: bad edges {bad} -> {os.path.basename(fn)}")
    full = trimesh.load(os.path.join(OUT, "pcb", "notifier_v1_pcb_board_frame.stl"), process=False)
    v = full.vertices
    trimesh.Trimesh(np.c_[PCB_X0 - v[:, 1], PCB_Y0 - v[:, 0], Z_SHELF + 0.01 + v[:, 2]], full.faces[:, ::-1], process=False).export(
        os.path.join(OUT, "pcb", "notifier_v1_pcb_housing_frame.stl"))

    print("\nprint orientation analysis:")
    for k in parts:
        pr = prints[k]
        tot_o, vis, hid, vis_pts = overhang_report(k, meshes[k], housing)
        cl, bad = clean_stl(pr)
        pm = M(Mesh(vert_properties=cl.vertices.astype(np.float32), tri_verts=cl.faces.astype(np.uint32)))
        fl = pm.slice(0.1).area()
        print(f"  {k:6s} first layer {fl:7.1f} mm2  footprint {pr.extents[0]:.1f} x {pr.extents[1]:.1f}  height {pr.extents[2]:.1f}  "
              f"overhang>45deg total {tot_o:6.1f} mm2: visible {vis:6.1f}, hidden {hid:6.1f}")
        res, isl = layer_check(pm)
        print(f"          layer check: islands (mid-air starts) {len(isl)}; unsupported >45deg: "
              f"{len(res[1.0])} regions {sum(r[1] for r in res[1.0]):.1f} mm2; >55deg: {len(res[1.43])} regions {sum(r[1] for r in res[1.43]):.1f} mm2")
        for z, a, b in res[1.0][:40]:
            print(f"             z {z:5.1f} area {a:6.1f} print x {b[0]:.1f}..{b[2]:.1f} y {b[1]:.1f}..{b[3]:.1f}")
        for z, a, b in isl:
            print(f"             ISLAND z {z:5.1f} area {a:6.1f} x {b[0]:.1f}..{b[2]:.1f} y {b[1]:.1f}..{b[3]:.1f}")
        if len(vis_pts):
            print(f"          visible overhang centroid range x {vis_pts[:,0].min():.1f}..{vis_pts[:,0].max():.1f} "
                  f"y {vis_pts[:,1].min():.1f}..{vis_pts[:,1].max():.1f} z {vis_pts[:,2].min():.1f}..{vis_pts[:,2].max():.1f}")

    mass_shell, mom = 0.0, np.zeros(3)
    for k, tm in meshes.items():
        m = tm.volume * RHO_PETG
        mass_shell += m
        mom += m * tm.center_mass
        print(f"  mass {k}: {m:.1f} g")
    allm = trimesh.util.concatenate(list(meshes.values()))
    z0 = allm.bounds[0][2]
    foot = allm.vertices[allm.vertices[:, 2] < z0 + 0.05][:, :2]
    poly = hull2d(foot)
    print(f"\nshell mass (PETG {RHO_PETG*1e3:.2f} g/cm3, 100 % solid as modelled): {mass_shell:.1f} g, shell CoM {np.round(mom/mass_shell, 1).tolist()}")
    print(f"flat foot polygon x {poly[:,0].min():.1f}..{poly[:,0].max():.1f} y {poly[:,1].min():.1f}..{poly[:,1].max():.1f}  table z {z0:.2f}")
    for label, extra in (("12 g at board centre", [((20.85, 35.46, 4.4), 12.0)]),
                         ("5 g display + 7 g board", [((CX, 3.0, ZC), 5.0), ((20.85, 35.46, 4.4), 7.0)]),
                         ("12 g + 6.8 g steel plate in the ballast recess", [((20.85, 35.46, 4.4), 12.0), ((20.5, 29.0, Z_BOT + 0.5), 6.8)]),
                         ("5 g display + 7 g board + 6.8 g steel plate", [((CX, 3.0, ZC), 5.0), ((20.85, 35.46, 4.4), 7.0), ((20.5, 29.0, Z_BOT + 0.5), 6.8)])):
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
        print(f"[{label}] total {mass:.1f} g, CoM {np.round(com, 1).tolist()}  tip angles deg {({k: round(v, 1) for k, v in worst.items()})}")

    imesh = {k: to_trimesh(v) for k, v in I.items()}
    np.savez(os.path.join(OUT, "meshes_b.npz"),
             **{f"hn_{k}": corner_normals(m).reshape(-1, 9) for k, m in meshes.items()},
             **{f"h_{k}": m.vertices[m.faces].reshape(-1, 9) for k, m in meshes.items()},
             **{f"i_{k}": m.vertices[m.faces].reshape(-1, 9) for k, m in imesh.items()})
    print("\nwrote STL + meshes_b.npz")


if __name__ == "__main__":
    main()
