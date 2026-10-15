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

# ref: (x, y, rotation, side)  side "F" or "B"
PLACE = {
    # module: antenna (local y < -6.75) overhangs the top edge
    "U1": (25.0, 6.75, 0, "F"),
    "C5": (13.8, 2.4, 90, "F"), "C6": (13.7, 4.6, 0, "F"),         # 3V3 pin 2 at (16.25, 2.76)
    "C10": (13.7, 10.6, 0, "F"),                                   # 1 uF for 3V3 pin 2 (no room beside C5/C6; plane vias carry 3V3)
    "R11": (13.5, 16.45, 0, "F"), "R12": (13.5, 17.72, 0, "F"),    # USB 22 ohm at module pins 13/14 (x 16.25)
    "R4": (13.7, 5.9, 0, "F"), "C4": (13.7, 7.2, 0, "F"),          # EN pin 3 at (16.25, 4.03)
    "R6": (13.7, 8.6, 0, "F"),                                     # IO6 pin 6 at (16.25, 7.84)
    "R7": (13.7, 12.6, 0, "F"), "R8": (13.7, 13.9, 0, "F"),        # I2C pull-ups
    # BME688 peninsula (top-left): notch on the module side and below, neck at the left edge
    "U4": (4.5, 4.6, 0, "F"), "C7": (3.3, 7.9, 0, "F"), "C8": (5.3, 7.9, 0, "F"),
    # SHT40 in the island's bottom-right corner, 6 mm from the BME688 (no pour between them)
    "U5": (8.8, 8.8, 90, "F"), "C9": (8.8, 6.9, 180, "F"),
    # buttons on the left under the island
    "SW1": (4.8, 17.8, 0, "F"), "SW2": (4.8, 25.6, 0, "F"),
    # USB-C at the bottom edge, ESD and CC resistors next to it, then the polyfuse
    "J1": (15.0, H - 3.4, 0, "F"),
    "R1": (8.6, H - 5.6, 90, "F"), "R2": (21.4, H - 5.6, 90, "F"),
    "U2": (15.0, 24.5, 0, "F"), "F1": (23.4, 33.8, 0, "F"),
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
S = 1.2
IH = 11.8  # island height
OUTLINE = [(0, 0), (10.4, 0), (10.4, IH), (3.6, IH), (3.6, IH + S), (10.4 + S, IH + S), (10.4 + S, 0),
           (W, 0), (W, H), (0, H)]
ISLAND = [(0, 0), (10.4, 0), (10.4, IH), (0, IH)]
NECK = [(0, IH), (4.5, IH), (4.5, IH + 2.8), (0, IH + 2.8)]          # no pours here: keep plane heat off the neck
ANTENNA_STRIP = [(10.4 + S, 0), (W, 0), (W, 1.8), (10.4 + S, 1.8)]  # no pours/vias beside the antenna

SILK = [  # (text, x, y, size, layer)
    ("RST", 4.8, 14.2, 1.0, "F"), ("BOOT", 4.8, 21.9, 1.0, "F"), ("BTN", 36.4, 35.0, 0.8, "F"),
    ("TX", 36.3, 4.4, 0.8, "F"), ("RX", 38.6, 4.4, 0.8, "F"), ("GND", 40.9, 4.4, 0.8, "F"),
    ("3V3", 43.2, 4.4, 0.8, "F"), ("EN", 45.5, 4.4, 0.8, "F"), ("IO0", 47.8, 4.4, 0.8, "F"),
    ("5V", 33.6, 21.4, 0.8, "F"), ("3V3", 33.4, 25.0, 0.8, "F"), ("BME688", 4.9, 10.3, 0.8, "F"),
    ("desk notifier v1", 25.0, 21.8, 1.0, "F"), ("LCD", 44.5, 7.5, 1.0, "F"), ("1", 39.8, 28.6, 0.8, "F"),
] + [(name, 37.4, 26.0 - 2.54 * i, 0.8, "B") for i, name in enumerate(("VCC", "GND", "SCL", "SDA", "DC", "CS", "RST"))]

NETCLASS_PATTERNS = {"Power": ["VBUS_IN", "VBUS", "+3V3", "GND", "SW"], "USB": ["/USB_DP", "/USB_DN", "/USB_DP_MCU", "/USB_DN_MCU"]}


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
    for name, width in (("Power", 0.4), ("USB", 0.25)):
        cls = pcbnew.NETCLASS(name)
        cls.SetTrackWidth(mm(width))
        cls.SetClearance(mm(0.15))
        cls.SetViaDiameter(mm(0.6))
        cls.SetViaDrill(mm(0.3))
        if name == "USB":
            cls.SetDiffPairWidth(mm(0.25))
            cls.SetDiffPairGap(mm(0.15))
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

    def trk(a, pts, width=0.2):
        for p0, p1 in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(p0), t.SetEnd(p1), t.SetWidth(mm(width)), t.SetLayer(pcbnew.F_Cu), t.SetNet(a.GetNet())
            board.Add(t)

    # SHT40 pads are 0.3 mm apart and its footprint keepout only lets tracks leave a pad straight:
    # hand-draw C9 -> VDD/VSS and short stubs off SDA/SCL, which the router then extends
    for n9, n5 in (("1", "3"), ("2", "4")):
        q = pad("U5", n5).GetPosition()
        trk(pad("U5", n5), [q, pcbnew.VECTOR2I(q.x, q.y - mm(0.6)), pad("C9", n9).GetPosition()])
    for n5 in ("1", "2"):
        q = pad("U5", n5).GetPosition()
        trk(pad("U5", n5), [q, pcbnew.VECTOR2I(q.x, q.y + mm(0.8))])

    full = [(0, 0), (W, 0), (W, H), (0, H)]
    zone(board, pcbnew.In1_Cu, "GND", full)
    zone(board, pcbnew.In2_Cu, "+3V3", full)
    # outer GND pours are added after routing (tools/finish.py), so the router sees free layers
    # no pours on the sensor island: keep the module's and regulator's heat out (tracks still allowed)
    copper = [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
    # In1 stays pourable there: tools/finish.py adds a hatched GND plane under island + neck as the
    # return path for SDA/SCL/3V3 (KRT's plane router replaces the zones made here, so it lives in finish.py)
    outer = [pcbnew.F_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
    keepout(board, outer, ISLAND)
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
