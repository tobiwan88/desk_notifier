"""notifier-v1 board: outline, placement, 4-layer planes, keepouts (run with $KICAD_PYTHON).

Builds hardware/notifier-v1/notifier-v1.kicad_pcb from the schematic netlist. Positions are in mm
from the board's top-left corner. Routing is a separate step (tools/autoroute.py or KRT).
Usage: layout_v1.py notifier-v1.net notifier-v1.kicad_pcb
"""

import math
import os
import sys

import pcbnew

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from pcb_from_netlist import parse  # noqa: E402

W, H = 50.0, 36.0
OX, OY = 100.0, 100.0  # board origin on the page
mm = pcbnew.FromMM

S, SM = 1.2, 1.8  # slot below the island / on the module side (wider where it faces the heat: TI SNOAA01)
IH = 14.8  # island height (11.8 + 3: the M2 hole takes the top-left corner)
HOLE_C, HOLE_KEEPOUT = 3.0, 2.7  # M2 holes: centre this far from both edges, copper keepout radius
J2_PIN_X = 10.0  # J2 pin column this far from the right edge: the socket face (fab outline 49.88 + 0.2 pin offset) lands at W + 0.03

J2_X = W - J2_PIN_X

# ref: (x, y, rotation, side)  side "F" or "B"
PLACE = {
    # module: antenna (local y < -6.75) overhangs the top edge
    "U1": (25.0, 6.75, 0, "F"),
    "C5": (13.8, 2.4, 90, "F"), "C6": (13.7, 4.6, 0, "F"),         # 3V3 pin 2 at (16.25, 2.76)
    "C10": (13.7, 10.6, 0, "F"),                                   # 1 uF for 3V3 pin 2 (no room beside C5/C6; plane vias carry 3V3)
    "R13": (24.36, 21.3, 0, "F"),                                  # LCD SCLK 33 ohm right below module pin 20
    # USB 22 ohm: rotated, pad 2 on the module pin row (13: y 16.73, 14: y 18.0); R11 under the D- line, R12 right of it
    "R11": (12.74, 17.24, 90, "F"), "R12": (14.7, 18.51, 90, "F"),
    "R4": (13.7, 5.9, 0, "F"), "C4": (13.7, 7.2, 0, "F"),          # EN pin 3 at (16.25, 4.03)
    # I2C pull-ups in the corridor between the slot and the module (x 12.9..14.5, y 12.3..14.2); the I2C bus reaches
    # their pad 2 over hand-drawn vias west of the USB pair (see "I2C" below)
    "R7": (13.7, 12.6, 0, "F"), "R8": (13.7, 13.9, 0, "F"),        # I2C pull-ups next to the module
    # sensor island (top-left peninsula, 10.4 x 14.8). M2 hole H3 in its top-left corner (keepout radius 2.7).
    # BME688 top-right, SHT40 bottom-left: opposite corners, 8.9 mm apart. Hand-routed below.
    "U4": (7.9, 3.4, 180, "F"), "C7": (5.2, 7.4, 270, "F"), "C8": (9.5, 6.3, 270, "F"),
    "U5": (3.6, 11.2, 90, "F"), "C9": (3.6, 9.3, 180, "F"),
    "C12": (1.9, 7.3, 0, "F"),                                     # bulk on the 3V3 line, next to the bus
    # USB-C on the left (back) edge, opening out of the edge; ESD, CC resistors and polyfuse next to it
    "J1": (3.4, 24.0, 270, "F"),
    "U2": (11.6, 24.2, 180, "F"),   # D- row = B7 row (y 23.25), flow-through to the right
    "R1": (9.7, 21.4, 90, "F"), "R2": (9.7, 27.0, 270, "F"), "F1": (12.0, 30.6, 0, "F"),
    # buck converter bottom centre, far from the sensor
    "U3": (30.0, 30.5, 180, "F"), "C1": (26.6, 28.2, 90, "F"), "L1": (34.8, 30.5, 0, "F"),
    "R9": (27.0, 31.6, 0, "F"), "R10": (27.0, 32.9, 0, "F"),
    "C2": (34.0, 26.8, 0, "F"), "C3": (37.0, 27.0, 90, "F"), "C11": (30.2, 26.9, 0, "F"),
    "R3": (28.0, 24.3, 0, "F"), "D1": (30.6, 24.3, 180, "F"),
    # BOOT pull-up near IO0 (pin 27 at (33.75, 18.0))
    "R5": (36.3, 19.0, 90, "F"),
    # display socket on the right (front) edge: fab-layer face at x = W + 0.03 (body to +x, pins at W - 10)
    "J2": (J2_X, 26.0, 180, "F"), "JP1": (36.0, 23.2, 90, "F"),
    # debug pads, TX RX 3V3 EN GND IO0 at 2.54 mm: tweezers EN-GND = reset, GND-IO0 = boot
    "TP1": (36.2, 6.8, 0, "F"), "TP2": (38.74, 6.8, 0, "F"), "TP4": (41.28, 6.8, 0, "F"),
    "TP5": (43.82, 6.8, 0, "F"), "TP3": (46.36, 6.8, 0, "F"), "TP6": (48.9, 6.8, 0, "F"),
    "TP7": (24.5, 27.0, 0, "F"),
    # M2 mounting holes, one per corner
    "H1": (W - HOLE_C, H - HOLE_C, 0, "F"), "H2": (HOLE_C, H - HOLE_C, 0, "F"),
    "H3": (HOLE_C, HOLE_C, 0, "F"), "H4": (W - HOLE_C, HOLE_C, 0, "F"),
}

# BME688 peninsula: notch (slot width 1.2 mm) along the module side and below, open to the top edge;
# the island hangs on a 3.6 mm wide neck at the left board edge, away from the module and the buck
OUTLINE = [(0, 0), (10.4, 0), (10.4, IH), (3.6, IH), (3.6, IH + S), (10.4 + SM, IH + S), (10.4 + SM, 0),
           (W, 0), (W, H), (0, H)]
ISLAND = [(0, 0), (10.4, 0), (10.4, IH), (0, IH)]
NECK = [(0, IH), (4.5, IH), (4.5, IH + 2.8), (0, IH + 2.8)]          # no pours here: keep plane heat off the neck
# Neck bus, hand-drawn on F.Cu, left to right: 3V3 | GND | SCL | SDA | GND, 0.15 mm (thin copper = less heat into
# the island; Bosch BME68x HSMI 7.3). The GND guards beside the I2C pair are its return path (no plane under the
# neck or island); each guard is stitched to In1 by a via just below the neck. SDA/SCL/3V3 start at island
# pads (U5, C7): the router keeps copper that ends on a pad, extends the free main-board ends, and reroutes free
# stubs. Only F.Cu crosses the neck.
BUS_X = {"+3V3": 0.5, "GND_L": 1.1, "I2C_SCL": 1.7, "I2C_SDA": 2.3, "GND_R": 2.9}
BUS_W = 0.15
STITCH_Y = IH + S + 0.3                                           # guard vias to In1, just below the neck
BUS_Y1 = IH + 2.8                                                 # free main-board ends of 3V3/SDA/SCL
LOOP_Y = IH + 0.25                                                # guards joined on B.Cu under SDA/SCL, in the neck
ANTENNA_STRIP = [(10.4 + SM, 0), (W, 0), (W, 1.8), (10.4 + SM, 1.8)]  # no pours/vias beside the antenna

SILK = [  # (text, x, y, size, layer). 1.0 mm text where it fits (JLC minimum 0.8 mm high / 0.15 mm stroke). GND and IO0 sit
    # below their pads at 0.8 mm: the M2 screw head of H4 (3.8 mm) covers the area above them and 2.54 mm pitch leaves no
    # room for two 1.0 mm three-letter labels in a row. JP1 labels (0.8 mm) go left of its pads (no room above/below).
    ("TX", 36.2, 5.05, 0.8, "F"), ("RX", 38.74, 5.05, 0.8, "F"), ("3V3", 41.28, 5.05, 0.8, "F"),
    ("EN", 43.82, 5.05, 0.8, "F"), ("GND", 46.0, 8.62, 0.8, "F"), ("IO0", 48.8, 8.62, 0.8, "F"),
    ("v1 ${GIT_SHA}", 22.5, 22.8, 1.0, "F"),  # worst case "v1 0000000-dirty" (16 chars, 12.5 mm) ends at x 28.8, JP1 "5V" starts at 33.0; DRC runs with that SHA
    ("JLCJLCJLCJLC", 20.0, 33.2, 1.0, "F"),  # JLC order number goes here: select "Specify a location"
    ("LCD", 37.7, 11.0, 1.0, "F"), ("1", J2_X, 28.6, 1.0, "F"),
    ("5V", 33.9, 21.9, 0.8, "F"), ("3V3", 33.6, 24.5, 0.8, "F"),  # JP1: pad 3 (VBUS) on top, pad 1 (3V3) below; ships bridged to 3V3
] + [(name, J2_X - 2.4, 26.0 - 2.54 * i, 1.0, "B") for i, name in enumerate(("RST", "CS", "DC", "SDA", "SCL", "GND", "VCC"))]

USB_W, USB_GAP = 0.27, 0.15  # sized for ~91 ohm differential with mask where coupled (calc/impedance.py); the pair is only coupled ~0.5 mm, see the USB block in main()
NETCLASS_PATTERNS = {"Power": ["VBUS_IN", "VBUS", "+3V3", "GND", "SW"], "USB": ["/USB_DP", "/USB_DN", "/MCU/MCU_USB_DP", "/MCU/MCU_USB_DN"]}


def v(x, y):
    return pcbnew.VECTOR2I(mm(OX + x), mm(OY + y))


def edge_poly(board, pts):
    for (x1, y1), (x2, y2) in zip(pts, pts[1:] + pts[:1]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(v(x1, y1))
        seg.SetEnd(v(x2, y2))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(mm(0.05))
        board.Add(seg)


def zone(board, layer, net, pts, priority=0, clearance=0.25):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(board.FindNet(net))
    z.SetAssignedPriority(priority)
    z.SetLocalClearance(mm(clearance))
    z.SetMinThickness(mm(0.2))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    z.SetThermalReliefGap(mm(0.25))
    z.SetThermalReliefSpokeWidth(mm(0.3))
    outline = z.Outline()
    outline.NewOutline()
    for x, y in pts:
        outline.Append(mm(OX + x), mm(OY + y))
    board.Add(z)
    return z


def keepout(board, layers, pts, tracks=False, vias=False, pours=True, name=""):
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    if name:
        z.SetZoneName(name)  # referenced by rules/notifier.kicad_dru
    ls = pcbnew.LSET()
    for layer in layers:
        ls.AddLayer(layer)
    z.SetLayerSet(ls)
    z.SetDoNotAllowTracks(tracks)
    z.SetDoNotAllowVias(vias)
    z.SetDoNotAllowZoneFills(pours)
    z.SetDoNotAllowPads(False)
    z.SetDoNotAllowFootprints(False)
    outline = z.Outline()
    outline.NewOutline()
    for x, y in pts:
        outline.Append(mm(OX + x), mm(OY + y))
    board.Add(z)


def rules(board):
    ds = board.GetDesignSettings()
    ds.m_TrackMinWidth = mm(0.127)
    ds.m_MinClearance = mm(0.127)
    ds.m_ViasMinSize = mm(0.45)
    ds.m_MinThroughDrill = mm(0.2)
    ds.m_HoleToHoleMin = mm(0.5)
    ds.m_CopperEdgeClearance = mm(0.3)
    ds.m_SilkClearance = mm(0.15)
    nc = ds.m_NetSettings
    default = nc.GetDefaultNetclass()
    default.SetTrackWidth(mm(0.2))
    default.SetClearance(mm(0.15))
    default.SetViaDiameter(mm(0.6))
    default.SetViaDrill(mm(0.3))
    for name, width in (("Power", 0.4), ("USB", USB_W)):
        cls = pcbnew.NETCLASS(name)
        cls.SetTrackWidth(mm(width))
        cls.SetClearance(mm(0.15))
        cls.SetViaDiameter(mm(0.6))
        cls.SetViaDrill(mm(0.3))
        if name == "USB":
            cls.SetDiffPairWidth(mm(USB_W))
            cls.SetDiffPairGap(mm(USB_GAP))
        nc.SetNetclass(name, cls)
        for pattern in NETCLASS_PATTERNS[name]:
            nc.SetNetclassPatternAssignment(pattern, name)


def main():
    net_file, pcb_file = sys.argv[1], sys.argv[2]
    comps, pads = parse(open(net_file).read())
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(4)
    rules(board)
    edge_poly(board, OUTLINE)

    nets = {}
    for name in sorted(set(pads.values())):
        net = pcbnew.NETINFO_ITEM(board, name)
        board.Add(net)
        nets[name] = net
    fp_dir = os.environ["KICAD_FOOTPRINT_DIR"]
    for ref, (value, fpid, uuid, fields, no_bom) in sorted(comps.items()):
        lib, name = fpid.split(":")
        fp = pcbnew.FootprintLoad(os.path.join(fp_dir, lib + ".pretty"), name)
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPath(pcbnew.KIID_PATH("/" + uuid))
        board.Add(fp)
        x, y, rot, side = PLACE[ref]
        fp.SetPosition(v(x, y))
        fp.SetOrientationDegrees(rot)
        if side == "B":
            fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_TOP_BOTTOM)
        for pad in fp.Pads():
            name = pads.get((ref, pad.GetNumber()))
            if name:
                pad.SetNet(nets[name])
        fp.Value().SetVisible(False)
        # references live on the fab layer; the silkscreen carries the user-facing labels (SILK)
        fp.Reference().SetLayer(pcbnew.F_Fab if side == "F" else pcbnew.B_Fab)
        for name, text in fields.items():
            if name not in ("Datasheet", "Description"):
                fp.SetField(name, text)
                fp.GetField(name).SetVisible(False)
        attrs = fp.GetAttributes() & ~pcbnew.FP_EXCLUDE_FROM_BOM
        fp.SetAttributes(attrs | (pcbnew.FP_EXCLUDE_FROM_BOM if no_bom else 0))
    missing = sorted(set(PLACE) - set(comps))
    if missing:
        sys.exit(f"placement for unknown refs: {missing}")

    def pad(ref, num):
        return next(q for q in board.FindFootprintByReference(ref).Pads() if q.GetNumber() == num)

    def trk(a, pts, width=0.2, locked=False, layer=pcbnew.F_Cu):
        for p0, p1 in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(p0), t.SetEnd(p1), t.SetWidth(mm(width)), t.SetLayer(layer), t.SetNet(a.GetNet())
            t.SetLocked(locked)
            board.Add(t)

    def path(net_pad, pts, width=USB_W, layer=pcbnew.F_Cu):
        trk(net_pad, [v(x, y) for x, y in pts], width, locked=True, layer=layer)

    def via(net_pad, x, y, dia=0.6, drill=0.3):
        t = pcbnew.PCB_VIA(board)
        t.SetPosition(v(x, y)), t.SetWidth(mm(dia)), t.SetDrill(mm(drill)), t.SetNet(net_pad.GetNet())
        t.SetLocked(True)
        board.Add(t)

    def at(ref, num):
        q = pad(ref, num).GetPosition()
        return round(pcbnew.ToMM(q.x) - OX, 3), round(pcbnew.ToMM(q.y) - OY, 3)

    # USB D+/D- by hand: 0.27 mm / 0.15 mm gap on F.Cu over the In1 GND plane = about 91 ohm differential on
    # JLC04161H-7628 (prepreg 0.2104 mm, er 4.4, 1 oz, mask) where the tracks are coupled (only ~0.5 mm; elsewhere
    # they are ~2 mm apart: a short, ~13 mm full-speed pair, coupling is not critical). J1 sits on the left edge (pads
    # in a column, interior to the right). Nothing runs under the J1 shell (x < 7.1). Pad rows from the top: B7 D-,
    # A6 D+, A7 D-, B6 D+ (0.5 mm pitch), so the two joins cannot both be planar on the right of the pads:
    #  - D+ A6-B6 is an F.Cu loop (0.2 mm stubs, x = XA) around A7's via,
    #  - D- A7 goes through a small via inside that loop (0.45/0.2: fits between the 0.2 mm D+ stubs, 0.5 mm pitch),
    #    a 1.4 mm B.Cu jumper (over In2 3V3 / In1 GND, harmless at 12 Mbit/s) and a second via on the B7 track.
    # B7 runs straight through the ESD U2 (flow-through, D- row on top) to R11; D+ goes through U2 (row y 25.15) and
    # around the GND via, coupled to D- (0.42 mm pitch) before it hooks to R12. Locked: the router keeps it.
    px = at("J1", "A6")[0]
    jy = {n: at("J1", n)[1] for n in ("B7", "A6", "A7", "B6")}
    assert jy["B7"] < jy["A6"] < jy["A7"] < jy["B6"], "J1 orientation"
    u2 = {n: at("U2", n) for n in "123456"}
    assert u2["3"][1] == u2["4"][1] == jy["B7"] and u2["1"][1] == u2["6"][1] and u2["3"][0] > u2["4"][0], "U2 orientation"
    r11, r11b, r12, r12b = at("R11", "1"), at("R11", "2"), at("R12", "1"), at("R12", "2")
    assert abs(r11[0] - u2["3"][0]) < 0.01 and abs(r11b[1] - at("U1", "13")[1]) < 0.01 and abs(r12b[1] - at("U1", "14")[1]) < 0.01 and abs(r12b[0] - r12[0]) < 0.01, ("R11/R12 alignment", r11, r11b, r12, r12b, at("U1", "13"), u2["3"])
    xa, va_x, vb_x, vbus_x = 9.1, 8.6, 9.37, 11.55           # D+ loop, D- via in the loop, D- via on the B7 track, VBUS_IN via (right of U2 pin 5)
    # every via keeps >= 0.2 mm copper-to-pad from SMD pads, also on the same net (rule 'no via in SMD pad')
    dpn = 0.2                                                # D+ stubs next to the D- via (via 0.45 needs 0.2 mm tracks at 0.5 mm pitch)
    path(pad("J1", "A6"), [(px, jy["A6"]), (xa, jy["A6"]), (xa, jy["B6"]), (px, jy["B6"])], dpn)
    path(pad("J1", "A6"), [(xa, jy["B6"]), (xa + 0.4, u2["6"][1]), u2["6"], u2["1"]])
    path(pad("J1", "B7"), [(px, jy["B7"]), u2["4"], u2["3"], r11])
    path(pad("J1", "A7"), [(px, jy["A7"]), (va_x, jy["A7"])])
    path(pad("J1", "A7"), [(va_x, jy["A7"]), (vb_x, jy["B7"])], USB_W, pcbnew.B_Cu)
    via(pad("J1", "A7"), va_x, jy["A7"], 0.45, 0.2)
    via(pad("J1", "A7"), vb_x, jy["B7"], 0.45, 0.2)
    xd = u2["3"][0] + USB_W + USB_GAP + 0.01                       # D+ coupled to the D- line
    xo = r12[0]                                              # D+ column right of the GND via, over R12
    dd = xo - xd
    path(pad("R12", "1"), [u2["1"], (xo, u2["1"][1]), (xo, r12[1] + 2.0 + dd), (xd, r12[1] + 2.0),
                           (xd, r12[1] + dd), (xo, r12[1])])
    for r, mod in (("R11", "13"), ("R12", "14")):
        path(pad(r, "2"), [at(r, "2"), at("U1", mod)])
    # ESD middle pins: VBUS_IN via to the left (between the pair, right of the D+ loop), GND via to the right
    # (between D- and the D+ column)
    u5, u2g = u2["5"], u2["2"]
    path(pad("U2", "5"), [u5, (vbus_x, u5[1])], 0.4)
    via(pad("U2", "5"), vbus_x, u5[1], 0.45, 0.2)
    path(pad("U2", "2"), [u2g, (u2g[0] + 1.2, u2g[1])], 0.4)
    via(pad("U2", "2"), u2g[0] + 1.2, u2g[1])
    # VBUS_IN of J1 (A4/B9 top, A9/B4 bottom): F.Cu stubs to vias right of the pads (outside the shell), joined on B.Cu
    # with the ESD via; the F.Cu link to the polyfuse F1 is left to the router. 0.4 mm.
    vt, vb = (8.7, at("J1", "A4")[1]), (8.7, at("J1", "A9")[1])
    path(pad("J1", "A4"), [(px, vt[1]), vt], 0.4)
    path(pad("J1", "A9"), [(px, vb[1]), vb], 0.4)
    via(pad("J1", "A4"), *vt)
    via(pad("J1", "A9"), *vb)
    path(pad("J1", "A4"), [vt, (10.2, vt[1]), (10.2, u5[1]), (vbus_x, u5[1])], 0.4, pcbnew.B_Cu)
    path(pad("J1", "A9"), [vb, (10.2, vb[1] - 0.75), (10.2, u5[1])], 0.4, pcbnew.B_Cu)

    # ---- sensor island: everything by hand, deterministic (the router would box in SCL or cut the SHT40 keepout) ----
    # Layout (island 10.4 x 14.8, M2 hole keepout circle at the top-left corner):
    #   BME688 top-right (rot 180: bottom row SCL SDA 3V3 GND, top row 3V3 3V3 GND 3V3). 3V3 comes up the left edge,
    #   turns right at y 6.2 (0.4 mm below the H3 keepout circle: radius 2.7 around (3, 3) reaches y 5.7) and climbs to pin 5; pins 6/8/2 join in a ring around the package. SCL/SDA drop
    #   straight down to two horizontals along the island bottom, which turn into the neck bus (SCL | SDA, SCL turns
    #   first so nothing crosses). SHT40 bottom-left: SCL taps the upper horizontal, SDA reaches the lower one over a
    #   B.Cu jumper. GND is a B.Cu tree (spine at y 9.2) with vias at every cap and at the BME688 centre.
    bx, bw = BUS_X, BUS_W
    u = {n: at("U4", n) for n in "12345678"}
    vdd, gnd, sda, scl = pad("C7", "1"), pad("C7", "2"), pad("U5", "1"), pad("U5", "2")
    tail = [IH + S + 0.3 * i for i in range(int((BUS_Y1 - IH - S) / 0.3) + 1)]   # 0.3 mm pieces on the main board
    SDA_Y0, SCL_Y0 = 16.6, 17.2                                                  # where the hand-drawn I2C routes below take over
    y_h, ring_y, ring_x = 6.2, u["8"][1] - 1.01, u["8"][0] + 0.85               # 3V3 horizontal, ring top / right
    ring_b = u["2"][1] + 0.71
    x_up = u["5"][0] - 0.75                                                      # 3V3 climbs left of the BME688
    # 3V3: bus -> up the left edge -> right along y 6.2, below the H3 keepout circle (y <= 5.7) -> pin 5; ring around the BME688
    path(vdd, [u["5"], (x_up, u["5"][1]), (x_up, y_h), (bx["+3V3"], y_h)] + [(bx["+3V3"], y) for y in tail], bw)
    path(vdd, [u["5"], u["6"], (u["6"][0], ring_y), (u["8"][0], ring_y), u["8"]], bw)
    path(vdd, [u["8"], (ring_x, u["8"][1]), (ring_x, ring_b), (u["2"][0], ring_b), u["2"]], bw)
    c7, c8, c9, c12, u5p = at("C7", "1"), at("C8", "1"), at("C9", "1"), at("C12", "1"), at("U5", "3")
    path(vdd, [c7, (c7[0], y_h)], bw)
    path(vdd, [c8, (c8[0], ring_b)], bw)
    path(vdd, [c12, (bx["+3V3"], c12[1])], bw)
    path(vdd, [c9, (4.5, c9[1]), (4.5, y_h)], bw)                               # SHT40 decoupling to the 3V3 line
    path(vdd, [u5p, (u5p[0], u5p[1] - 0.6), c9], bw)
    # SCL: pin 4 straight down to the upper horizontal, left to the bus (45 deg), SHT40 taps in at its x
    y_c, y_d = IH - 1.6, IH - 0.7
    u5c, u5d = at("U5", "2"), at("U5", "1")
    path(scl, [u["4"], (u["4"][0], y_c), (u5c[0], y_c), (bx["I2C_SCL"] + 0.3, y_c), (bx["I2C_SCL"], y_c + 0.3)]
         + [(bx["I2C_SCL"], y) for y in tail if y <= SCL_Y0 + 1e-6], bw)
    path(scl, [u5c, (u5c[0], y_c)], bw)
    # SDA: pin 3 straight down to the lower horizontal; SHT40 pin 1 over a B.Cu jumper below its pad
    path(sda, [u["3"], (u["3"][0], y_d), (u5d[0], y_d), (bx["I2C_SDA"] + 0.3, y_d), (bx["I2C_SDA"], y_d + 0.3)]
         + [(bx["I2C_SDA"], y) for y in tail if y <= SDA_Y0 + 1e-6], bw)
    va = (u5d[0], u5d[1] + 0.7)
    path(sda, [u5d, va], bw)
    via(sda, *va, 0.45, 0.2)                                                  # >= 0.2 mm from the pad (0.3 to the SCL line below)
    path(sda, [va, (u5d[0], y_d)], bw, pcbnew.B_Cu)
    via(sda, u5d[0], y_d)
    # GND: BME688 centre via (pins 1 and 7), B.Cu spine to the caps, SHT40 pin 4 via C9
    vg = (u["7"][0], (u["1"][1] + u["7"][1]) / 2)
    path(gnd, [u["7"], vg], bw)
    path(gnd, [u["1"], (u["1"][0], vg[1]), vg], bw)
    spine_y = 9.2
    gc7, gc8, gc9, gc12, u5g = at("C7", "2"), at("C8", "2"), at("C9", "2"), at("C12", "2"), at("U5", "4")
    vc7, vc8, vc129 = (gc7[0], gc7[1] + 0.72), (gc8[0], gc8[1] + 0.72), (2.9, 8.5)
    vg1 = (bx["GND_L"], 11.5)
    path(gnd, [gc7, vc7], bw)
    path(gnd, [gc8, vc8], bw)
    path(gnd, [gc12, vc129], bw)
    path(gnd, [gc9, vc129], bw)
    path(gnd, [u5g, (u5g[0], u5g[1] - 0.6), gc9], bw)
    for p_ in (vg, vg1):
        via(gnd, *p_)
    for p_ in (vc7, vc8, vc129):
        via(gnd, *p_, 0.45, 0.2)                                              # >= 0.2 mm to the C7/C8/C9 pads
    b_ = lambda pts: path(gnd, pts, bw, pcbnew.B_Cu)
    b_([vc8, (vc8[0], spine_y), (vg1[0], spine_y), vg1])
    b_([vg, (vg[0], spine_y)])
    b_([vc7, (vc7[0], spine_y)])
    b_([vc129, (vc129[0], spine_y)])
    # GND guards beside SCL/SDA in the neck: left guard from the island GND via, right guard only through the neck;
    # joined on B.Cu inside the neck, stitched to In1 just below it
    path(gnd, [vg1, (bx["GND_L"], STITCH_Y)], bw)
    path(gnd, [(bx["GND_R"], LOOP_Y), (bx["GND_R"], STITCH_Y)], bw)
    for name in ("GND_L", "GND_R"):
        via(gnd, bx[name], STITCH_Y)
        via(gnd, bx[name], LOOP_Y)
    path(gnd, [(bx["GND_L"], LOOP_Y), (bx["GND_R"], LOOP_Y)], bw, pcbnew.B_Cu)

    # ---- I2C from the neck to the pull-ups R7/R8 (hand-drawn, locked) ----
    # Both lines run on F.Cu along y 17.0 (SDA) / 17.9 (SCL) and dive to B.Cu at x = 10.9, 1.2 mm clear of R11's pads and
    # more than 1.8 mm from the D- track (x 12.74), then cross under R11/D- on B.Cu (In1 GND and In2 3V3 lie between)
    # and come up beside the pull-up pad 2 (R7 SDA at y 12.6, R8 SCL at y 13.9). The router continues from there to the
    # module pins.
    xv = 10.9
    sda_p, scl_p = pad("R7", "2"), pad("R8", "2")
    r7b, r8b = at("R7", "2"), at("R8", "2")
    vs, vc, vsd, vcl = (xv, 17.0), (xv, 17.9), (15.0, r7b[1]), (14.8, 14.6)
    path(sda, [(bx["I2C_SDA"], SDA_Y0), (bx["I2C_SDA"] + 0.4, 17.0), vs], 0.2)
    path(scl, [(bx["I2C_SCL"], SCL_Y0), (bx["I2C_SCL"] + 0.7, 17.9), vc], 0.2)
    via(sda, *vs), via(scl, *vc), via(sda, *vsd), via(scl, *vcl)
    path(sda, [vs, (xv + 0.5, 16.6), (12.6, 16.6), vsd], 0.2, pcbnew.B_Cu)
    path(scl, [vc, (vcl[0], vc[1]), vcl], 0.2, pcbnew.B_Cu)
    path(sda_p, [r7b, vsd], 0.2)
    path(scl_p, [r8b, vcl], 0.2)

    full = [(0, 0), (W, 0), (W, H), (0, H)]
    zone(board, pcbnew.In1_Cu, "GND", full)
    zone(board, pcbnew.In2_Cu, "+3V3", full)
    # outer GND pours are added after routing (tools/finish.py), so the router sees free layers
    # no pours on the sensor island: keep the module's and regulator's heat out (tracks still allowed)
    copper = [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
    # no In1 GND under the island or the neck either (tools/finish.py redraws In1; KRT's plane router replaces the
    # zones made here): sensor GND reaches the main board by the bus guards
    outer = [pcbnew.F_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
    keepout(board, outer, ISLAND)
    keepout(board, [pcbnew.In1_Cu], [(0, 0), (10.4, 0), (10.4, IH), (3.6, IH), (3.6, IH + S), (0, IH + S)])
    # no other B.Cu track across the neck: a band below the guards' B.Cu link (every crossing passes through it)
    keepout(board, [pcbnew.B_Cu], [(0, LOOP_Y + 0.35), (3.6, LOOP_Y + 0.35), (3.6, IH + S), (0, IH + S)], tracks=True, pours=False)
    keepout(board, outer, NECK)
    keepout(board, copper, ANTENNA_STRIP, vias=True)
    # J1's metal shell (fab outline x -0.3..7.1, y 19.48..28.52) sits on the board: only GND may run under it
    # (rule "no signal copper under the J1 shell" in notifier.kicad_dru; the area itself restricts nothing)
    keepout(board, [pcbnew.F_Cu, pcbnew.B_Cu], [(-0.3, 19.48), (7.1, 19.48), (7.1, 28.52), (-0.3, 28.52)], pours=False, name="j1_shell")
    # M2 holes: nothing but the screw within 2.7 mm (no tracks, vias or pours on any copper layer)
    r_ko = HOLE_KEEPOUT / math.cos(math.pi / 32)
    for ref in ("H1", "H2", "H3", "H4"):
        hx, hy = PLACE[ref][:2]
        keepout(board, copper, [(hx + r_ko * math.cos(math.pi * k / 16), hy + r_ko * math.sin(math.pi * k / 16)) for k in range(32)],
                tracks=True, vias=True)

    filler = pcbnew.ZONE_FILLER(board)
    filler.Fill(board.Zones())
    for text, x, y, size, side in SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetPosition(v(x, y))
        t.SetLayer(pcbnew.F_SilkS if side == "F" else pcbnew.B_SilkS)
        t.SetMirrored(side == "B")
        t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
        t.SetTextThickness(mm(0.15))
        board.Add(t)
    if not pcbnew.SaveBoard(pcb_file, board):
        sys.exit("save failed")
    from write_project import write
    from pathlib import Path
    here = Path(__file__).resolve().parent.parent
    write(Path(pcb_file).with_suffix(".kicad_pro"), here / "rules" / "jlc-4layer.json", NETCLASS_PATTERNS,
          {"USB": {"track_width": USB_W, "diff_pair_width": USB_W, "diff_pair_gap": USB_GAP}})
    import shutil
    shutil.copy(here / "rules" / "notifier.kicad_dru", Path(pcb_file).with_suffix(".kicad_dru"))
    print(f"{pcb_file}: {len(comps)} footprints, {len(nets)} nets, 4 layers, {W}x{H} mm")


main()
