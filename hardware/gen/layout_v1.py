"""notifier-v1 board: outline, placement, 4-layer planes, keepouts (run with $KICAD_PYTHON).

Builds hardware/notifier-v1/notifier-v1.kicad_pcb from the schematic netlist. Positions are in mm
from the board's top-left corner. Routing is a separate step (tools/autoroute.py or KRT).
Usage: layout_v1.py notifier-v1.net notifier-v1.kicad_pcb
"""

import os
import sys

import pcbnew

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from pcb_from_netlist import parse  # noqa: E402

W, H = 50.0, 36.0
OX, OY = 100.0, 100.0  # board origin on the page
mm = pcbnew.FromMM

S, SM = 1.2, 1.8  # slot below the island / on the module side (wider where it faces the heat: TI SNOAA01)
IH = 11.8  # island height

# ref: (x, y, rotation, side)  side "F" or "B"
PLACE = {
    # module: antenna (local y < -6.75) overhangs the top edge
    "U1": (25.0, 6.75, 0, "F"),
    "C5": (13.8, 2.4, 90, "F"), "C6": (13.7, 4.6, 0, "F"),         # 3V3 pin 2 at (16.25, 2.76)
    "C10": (13.7, 10.6, 0, "F"),                                   # 1 uF for 3V3 pin 2 (no room beside C5/C6; plane vias carry 3V3)
    "R13": (24.36, 21.3, 0, "F"),                                  # LCD SCLK 33 ohm right below module pin 20
    "R11": (13.5, 16.45, 0, "F"), "R12": (13.5, 17.72, 0, "F"),    # USB 22 ohm at module pins 13/14 (x 16.25)
    "R4": (13.7, 5.9, 0, "F"), "C4": (13.7, 7.2, 0, "F"),          # EN pin 3 at (16.25, 4.03)
    "R6": (13.7, 8.6, 0, "F"),                                     # IO6 pin 6 at (16.25, 7.84)
    # I2C pull-ups just below the sensor neck: the neck bus ends on their pads (pad 2 under SDA/SCL, see BUS_X)
    "R7": (13.7, 12.6, 0, "F"), "R8": (13.7, 13.9, 0, "F"),        # I2C pull-ups next to the module
    # BME688 peninsula (top-left): notch on the module side and below, neck at the left edge
    "U4": (4.5, 4.6, 0, "F"), "C7": (3.3, 7.9, 0, "F"), "C8": (5.3, 7.9, 0, "F"),
    # SHT40 in the island's bottom-right corner, 6 mm from the BME688 (no pour between them)
    "U5": (8.8, 8.8, 90, "F"), "C9": (8.8, 6.9, 180, "F"),
    "C12": (1.2, 6.6, 90, "F"),                                    # bulk next to C7, where 3V3 enters the island
    # buttons on the left under the island
    "SW1": (4.8, 17.8, 0, "F"), "SW2": (4.8, 25.6, 0, "F"),
    # USB-C at the bottom edge, ESD and CC resistors next to it, then the polyfuse
    "J1": (15.0, H - 3.4, 0, "F"),
    "R1": (8.6, H - 5.6, 90, "F"), "R2": (21.4, H - 5.6, 90, "F"),
    "U2": (15.0, 24.0, 270, "F"), "F1": (23.4, 33.8, 0, "F"),     # U2 rot 270: D- flows 4->3, D+ 6->1
    # buck converter bottom centre, far from the sensor
    "U3": (30.0, 30.5, 180, "F"), "C1": (26.6, 28.2, 90, "F"), "L1": (34.8, 30.5, 0, "F"),
    "R9": (27.0, 31.6, 0, "F"), "R10": (27.0, 32.9, 0, "F"),
    "C2": (34.0, 26.8, 0, "F"), "C3": (37.0, 27.0, 90, "F"), "C11": (30.2, 26.9, 0, "F"),
    "R3": (28.0, 24.3, 0, "F"), "D1": (31.0, 24.3, 0, "F"),
    # BOOT pull-up near IO0 (pin 27 at (33.75, 18.0))
    "R5": (36.3, 19.0, 90, "F"),
    # display socket on the right edge, its face 0.4 mm past the edge (body to +x)
    "J2": (W - 10.2, 26.0, 180, "F"), "JP1": (36.0, 23.2, 90, "F"),
    "SW3": (41.3, 32.3, 0, "F"),
    # debug pads (ESP auto-program order) and mounting holes
    "TP1": (36.3, 2.8, 0, "F"), "TP2": (38.6, 2.8, 0, "F"), "TP3": (40.9, 2.8, 0, "F"),
    "TP4": (43.2, 2.8, 0, "F"), "TP5": (45.5, 2.8, 0, "F"), "TP6": (47.8, 2.8, 0, "F"),
    "TP7": (24.5, 27.0, 0, "F"),
    "H1": (47.8, H - 2.2, 0, "F"), "H2": (3.0, H - 3.0, 0, "F"),
}

# BME688 peninsula: notch (slot width 1.2 mm) along the module side and below, open to the top edge;
# the island hangs on a 2.6 mm neck at the left board edge, away from the module and the buck
OUTLINE = [(0, 0), (10.4, 0), (10.4, IH), (3.6, IH), (3.6, IH + S), (10.4 + SM, IH + S), (10.4 + SM, 0),
           (W, 0), (W, H), (0, H)]
ISLAND = [(0, 0), (10.4, 0), (10.4, IH), (0, IH)]
NECK = [(0, IH), (4.5, IH), (4.5, IH + 2.8), (0, IH + 2.8)]          # no pours here: keep plane heat off the neck
# Neck bus, hand-drawn on F.Cu, left to right: 3V3 | GND | SDA | SCL | GND, 0.15 mm (thin copper = less heat into
# the island; Bosch BME68x HSMI 7.3). The GND guards beside the I2C pair are its return path (no plane under the
# neck or island); each guard is stitched to In1 by a via just below the neck. SDA/SCL/3V3 start at island
# pads (U5, C7): the router keeps copper that ends on a pad, extends the free main-board ends, and reroutes free
# stubs. Only F.Cu crosses the neck.
BUS_X = {"+3V3": 0.6, "GND_L": 1.15, "I2C_SDA": 1.7, "I2C_SCL": 2.25, "GND_R": 2.8}
BUS_W = 0.15
STITCH_Y = IH + S + 0.3                                           # guard vias to In1, just below the neck
BUS_Y1 = IH + 2.8                                                 # free main-board ends of 3V3/SDA/SCL
LOOP_Y = IH + 0.25                                                # guards joined on B.Cu under SDA/SCL, in the neck
ANTENNA_STRIP = [(10.4 + SM, 0), (W, 0), (W, 1.8), (10.4 + SM, 1.8)]  # no pours/vias beside the antenna

SILK = [  # (text, x, y, size, layer)
    ("RST", 4.8, 14.2, 1.0, "F"), ("BOOT", 4.8, 21.9, 1.0, "F"), ("BTN", 36.4, 35.0, 0.8, "F"),
    ("TX", 36.3, 4.4, 0.8, "F"), ("RX", 38.6, 4.4, 0.8, "F"), ("GND", 40.9, 4.4, 0.8, "F"),
    ("3V3", 43.2, 4.4, 0.8, "F"), ("EN", 45.5, 4.4, 0.8, "F"), ("IO0", 47.8, 4.4, 0.8, "F"),
    ("5V", 33.6, 21.4, 0.8, "F"), ("3V3", 33.4, 25.0, 0.8, "F"), ("BME688", 4.9, 10.3, 0.8, "F"),
    ("desk notifier v1", 25.0, 22.8, 1.0, "F"), ("LCD", 44.5, 7.5, 1.0, "F"), ("1", 39.8, 28.6, 0.8, "F"),
] + [(name, 37.4, 26.0 - 2.54 * i, 0.8, "B") for i, name in enumerate(("VCC", "GND", "SCL", "SDA", "DC", "CS", "RST"))]

USB_W, USB_GAP = 0.27, 0.15  # 90 ohm differential, see the USB block in main()
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


def keepout(board, layers, pts, tracks=False, vias=False, pours=True):
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
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

    # SHT40 pads are 0.3 mm apart and its footprint keepout only lets tracks leave a pad straight:
    # hand-draw C9 -> VDD/VSS (SDA/SCL leave through the neck bus, see BUS_X)
    for n9, n5 in (("1", "3"), ("2", "4")):
        q = pad("U5", n5).GetPosition()
        trk(pad("U5", n5), [q, pcbnew.VECTOR2I(q.x, q.y - mm(0.6)), pad("C9", n9).GetPosition()])

    # USB D+/D- by hand (the router left them partial with ~8 vias): 0.27 mm / 0.15 mm gap on F.Cu over
    # the In1 GND plane = 90 ohm differential on JLC04161H-7628 (prepreg 0.2104 mm, er 4.4, 1 oz, mask).
    # Connector D+ (A6/B6) joined above the pad row, D- (A7/B7) below it; straight through the ESD
    # (flow-through), coupled up to R11/R12, short stubs to module pins 13/14. Locked: the router keeps it.
    def path(net_pad, pts, width=USB_W, layer=pcbnew.F_Cu):
        trk(net_pad, [v(x, y) for x, y in pts], width, locked=True, layer=layer)

    def via(net_pad, x, y):
        t = pcbnew.PCB_VIA(board)
        t.SetPosition(v(x, y)), t.SetWidth(mm(0.6)), t.SetDrill(mm(0.3)), t.SetNet(net_pad.GetNet())
        t.SetLocked(True)
        board.Add(t)

    def at(ref, num):
        q = pad(ref, num).GetPosition()
        return round(pcbnew.ToMM(q.x) - OX, 3), round(pcbnew.ToMM(q.y) - OY, 3)

    dn, dp = at("U2", "4"), at("U2", "6")                     # bottom row (connector side)
    assert dn[0] < dp[0] and at("U2", "3")[0] == dn[0] and at("U2", "3")[1] < dn[1], "U2 orientation"
    gap = USB_W + USB_GAP                                    # centre to centre
    d45 = gap * 2 ** 0.5                                     # horizontal offset of parallel 45 deg lines
    (b7x, py), (a6x, _), (a7x, _), (b6x, _) = at("J1", "B7"), at("J1", "A6"), at("J1", "A7"), at("J1", "B6")
    top, bot = py - 1.25, py + 1.05                          # join rows above / below the pad row
    path(pad("J1", "A6"), [(a6x, py), (a6x, top), (b6x, top), (b6x, py)])
    path(pad("J1", "A6"), [(b6x, top), (dp[0], top - 0.2), at("U2", "6"), at("U2", "1")])
    path(pad("J1", "B7"), [(a7x, py), (a7x, bot), (b7x, bot), (b7x, py)])
    path(pad("J1", "B7"), [(b7x, py), (b7x, top), (dn[0], top - 0.2), at("U2", "4"), at("U2", "3")])
    r12, r11 = at("R12", "1"), at("R11", "1")
    turn_p = (dp[0], r12[1] + 0.92 + (dp[0] - r12[0]))      # 45 deg up-left from here ends below R12.1
    end_p = (r12[0], r12[1] + 0.92)
    path(pad("R12", "1"), [at("U2", "1"), turn_p, end_p, r12])
    x_n = r12[0] - d45                                       # D- parallel left of D+, passes R12 on the left
    turn_n = (dn[0], end_p[1] + (dn[0] - x_n))
    path(pad("R11", "1"), [at("U2", "3"), turn_n, (x_n, end_p[1]), (x_n, r11[1] + (r11[0] - x_n)), r11])
    for r, mod in (("R11", "13"), ("R12", "14")):
        a, b_ = at(r, "2"), at("U1", mod)
        path(pad(r, "2"), [a, (b_[0] - 1.25, a[1]), (b_[0] - 1.25 + (b_[1] - a[1]), b_[1]), b_])
    # ESD middle pins: VBUS_IN via below (it cannot leave between the pair), GND via above
    u5, u2 = at("U2", "5"), at("U2", "2")
    path(pad("U2", "5"), [u5, (u5[0], u5[1] + 1.25)], 0.4)
    via(pad("U2", "5"), u5[0], u5[1] + 1.25)
    path(pad("U2", "2"), [u2, (u2[0], u2[1] - 1.2)], 0.4)
    via(pad("U2", "2"), u2[0], u2[1] - 1.2)

    # neck bus (see BUS_X): 3V3 and the left guard come down from C7 at 45 deg, SDA/SCL run along the island's
    # bottom edge from U5, the right guard starts below SCL
    bx, c7v, c7g, sda, scl = BUS_X, at("C7", "1"), at("C7", "2"), at("U5", "1"), at("U5", "2")
    turn = c7v[1] + 0.3
    # the main-board tails come in 0.3 mm pieces: where the router joins one mid-way, finish.py drops the leftover
    tail = [IH + S + 0.3 * i for i in range(int((BUS_Y1 - IH - S) / 0.3) + 1)]
    path(pad("C7", "1"), [c7v, (c7v[0], turn), (bx["+3V3"], turn + c7v[0] - bx["+3V3"])] + [(bx["+3V3"], y) for y in tail], BUS_W)
    path(pad("C7", "2"), [c7g, (c7g[0], turn), (bx["GND_L"], turn + c7g[0] - bx["GND_L"]), (bx["GND_L"], STITCH_Y)], BUS_W)
    ys, yc = IH - 0.9, IH - 0.45                                  # SDA / SCL along the island edge (0.35 mm to it)
    path(pad("U5", "1"), [sda, (sda[0], ys), (bx["I2C_SDA"] + 0.3, ys), (bx["I2C_SDA"], ys + 0.3)] + [(bx["I2C_SDA"], y) for y in tail], BUS_W)
    path(pad("U5", "2"), [scl, (scl[0], yc), (bx["I2C_SCL"] + 0.3, yc), (bx["I2C_SCL"], yc + 0.3)] + [(bx["I2C_SCL"], y) for y in tail], BUS_W)
    path(pad("C7", "2"), [(bx["GND_R"], IH - 0.05), (bx["GND_R"], STITCH_Y)], BUS_W)
    # island by hand (the router kept boxing in SCL or cutting the SHT40 keepout): 3V3 and I2C on F.Cu,
    # GND as a B.Cu tree between four layer-change vias (no plane on the island)
    def b_path(net_pad, pts):
        path(net_pad, pts, BUS_W, pcbnew.B_Cu)
    u = {n: at("U4", n) for n in "12345678"}
    gnd, vdd = pad("C7", "2"), pad("C7", "1")
    # 3V3: U4 pin 2 over the top, down the left to C12; one rung at y 6.8 to pins 8 and 6 (pin 5 joins 6), C7, C8
    path(vdd, [u["2"], (u["2"][0], 2.6), (2.1, 2.6), (2.1, 7.375), at("C12", "1")], BUS_W)
    path(vdd, [(2.1, 6.8), (u["6"][0], 6.8)], BUS_W)
    path(vdd, [u["8"], (u["8"][0], 7.4), at("C7", "1")], BUS_W)
    path(vdd, [u["6"], (u["6"][0], 7.5), at("C8", "1")], BUS_W)
    path(vdd, [u["5"], (u["5"][0], 6.2), (u["6"][0] + 0.4, 6.6), (u["6"][0], 6.6)], BUS_W)
    # SHT40 3V3 (C9) to U4 pin 5 under the SDA line: via, B.Cu at y 5.0, via
    path(vdd, [at("C9", "1"), (9.4, 6.0)], BUS_W)
    via(vdd, 9.4, 6.0)
    b_path(vdd, [(9.4, 6.0), (9.4, 5.0), (6.3, 5.0)])
    via(vdd, 6.3, 5.0)
    path(vdd, [(6.3, 5.0), (u["5"][0], 5.6), u["5"]], BUS_W)
    # SDA: U4 pin 3 over the top, down at x 7.6 onto the SDA line from U5
    path(pad("U4", "3"), [u["3"], (u["3"][0], 2.6), (7.6, 2.6), (7.6, ys)], BUS_W)
    # SCL: U4 pin 4 right to a via, B.Cu along the top and down the right edge, via onto the U5 SCL stub
    path(pad("U4", "4"), [u["4"], (6.6, u["4"][1])], BUS_W)
    via(pad("U4", "4"), 6.6, u["4"][1])
    b_path(pad("U4", "4"), [(6.6, u["4"][1]), (9.95, u["4"][1]), (9.95, 10.3), (scl[0], 10.3)])
    via(pad("U4", "4"), scl[0], 10.3)
    # GND: U4 pins 1 and 7 joined under the package, vias V1 (left of U4), V2 (C12), V3 (C7/C8), V4 (C9/U5)
    v1, v2, v3, v4 = (2.75, 4.25), (1.2, 4.75), (4.33, 8.75), (8.3, 6.0)
    path(gnd, [u["1"], (u["1"][0], v1[1]), v1], BUS_W)
    path(gnd, [(u["1"][0], v1[1]), (u["7"][0], v1[1]), u["7"]], BUS_W)
    path(gnd, [at("C12", "2"), v2], BUS_W)
    path(gnd, [(c7g[0], turn), v3], BUS_W)
    path(gnd, [at("C8", "2"), (at("C8", "2")[0], v3[1]), v3], BUS_W)
    path(gnd, [at("C9", "2"), v4], BUS_W)
    for x, y in (v1, v2, v3, v4):
        via(gnd, x, y)
    b_path(gnd, [v2, (v2[0] + 0.5, v1[1]), v1, (v1[0], 6.0), v4])
    b_path(gnd, [(v3[0], 6.0), v3])

    for name in ("GND_L", "GND_R"):
        via(pad("C7", "2"), bx[name], STITCH_Y)
        via(pad("C7", "2"), bx[name], LOOP_Y)
    # the right guard cannot reach island GND on F.Cu (SDA/SCL run between), so the guards join on B.Cu: a GND loop
    # around the pair, grounded at both ends
    path(pad("C7", "2"), [(bx["GND_L"], LOOP_Y), (bx["GND_R"], LOOP_Y)], BUS_W, pcbnew.B_Cu)

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
    write(Path(pcb_file).with_suffix(".kicad_pro"), here / "rules" / "jlc-4layer.json", NETCLASS_PATTERNS)
    import shutil
    shutil.copy(here / "rules" / "notifier.kicad_dru", Path(pcb_file).with_suffix(".kicad_dru"))
    print(f"{pcb_file}: {len(comps)} footprints, {len(nets)} nets, 4 layers, {W}x{H} mm")


main()
