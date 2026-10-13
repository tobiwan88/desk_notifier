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
    "C5": (13.1, 2.2, 0, "F"), "C6": (13.1, 4.0, 0, "F"),          # 3V3 pin 2 at (16.25, 2.76)
    "R4": (13.1, 5.4, 0, "F"), "C4": (13.1, 6.7, 0, "F"),          # EN pin 3 at (16.25, 4.03)
    "R6": (13.1, 8.6, 0, "F"),                                     # IO6 pin 6 at (16.25, 7.84)
    "R7": (13.1, 12.6, 0, "F"), "R8": (13.1, 13.9, 0, "F"),        # I2C pull-ups
    # BME688 island (top-left), cut free by the L slot below
    "U4": (4.5, 5.0, 0, "F"), "C7": (8.2, 3.6, 90, "F"), "C8": (8.2, 6.4, 90, "F"),
    # buttons on the left under the island
    "SW1": (4.8, 17.5, 0, "F"), "SW2": (4.8, 25.0, 0, "F"),
    # USB-C at the bottom edge, ESD and CC resistors next to it
    "J1": (15.0, H - 3.4, 0, "F"),
    "R1": (8.6, H - 5.6, 90, "F"), "R2": (21.4, H - 5.6, 90, "F"),
    "U2": (15.0, 24.5, 0, "F"),
    # regulator bottom centre, far from the sensor
    "U3": (29.5, 30.0, 0, "F"), "C1": (23.4, 30.5, 90, "F"), "C2": (35.6, 30.0, 90, "F"), "C3": (37.4, 30.0, 90, "F"),
    "R3": (28.0, 24.3, 0, "F"), "D1": (31.0, 24.3, 0, "F"),
    # BOOT pull-up near IO0 (pin 27 at (33.75, 18.0))
    "R5": (36.3, 19.0, 90, "F"),
    # display socket on the right edge, opening facing out (body to +x)
    "J2": (W - 10.6, 26.0, 180, "F"), "JP1": (36.0, 23.2, 90, "F"),
    "SW3": (44.5, 31.5, 0, "F"),
    # debug UART pads and mounting holes
    "TP1": (37.0, 2.2, 0, "F"), "TP2": (39.6, 2.2, 0, "F"), "TP3": (42.2, 2.2, 0, "F"),
    "H1": (46.8, 3.2, 0, "F"), "H2": (3.0, H - 3.0, 0, "F"),
}

# L-shaped slot around the BME688 island: vertical along x=10.5..11.3 and horizontal along y=10.5..11.3
SLOT = [(10.5, 3.5), (11.3, 3.5), (11.3, 11.3), (1.5, 11.3), (1.5, 10.5), (10.5, 10.5)]
ISLAND = [(0, 0), (10.5, 0), (10.5, 10.5), (0, 10.5)]

SILK = [  # (text, x, y, size)
    ("RST", 4.8, 14.0, 1.0), ("BOOT", 4.8, 21.5, 1.0), ("BTN", 44.5, 35.0, 0.8),
    ("TX RX GND", 39.6, 4.4, 0.8), ("1", 37.3, 26.0, 0.8), ("LCD", 44.5, 7.5, 1.0),
    ("5V", 33.6, 21.4, 0.8), ("3V3", 33.4, 25.0, 0.8), ("BME688", 5.0, 9.3, 0.8),
    ("desk notifier v1", 25.0, 21.8, 1.0),
]

NETCLASS_PATTERNS = {"Power": ["/VBUS", "/+3V3", "/GND"], "USB": ["/USB_DP", "/USB_DN"]}


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
    edge_poly(board, [(0, 0), (W, 0), (W, H), (0, H)])
    edge_poly(board, SLOT)

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

    full = [(0, 0), (W, 0), (W, H), (0, H)]
    zone(board, pcbnew.In1_Cu, "/GND", full)
    zone(board, pcbnew.In2_Cu, "/+3V3", full)
    # outer GND pours are added after routing (tools/finish.py), so the router sees free layers
    # no pours on the sensor island: keep the module's and regulator's heat out (tracks still allowed)
    keepout(board, [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu], ISLAND)

    filler = pcbnew.ZONE_FILLER(board)
    filler.Fill(board.Zones())
    for text, x, y, size in SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetPosition(v(x, y))
        t.SetLayer(pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
        t.SetTextThickness(mm(0.15))
        board.Add(t)
    if not pcbnew.SaveBoard(pcb_file, board):
        sys.exit("save failed")
    from write_project import write
    from pathlib import Path
    here = Path(__file__).resolve().parent.parent
    write(Path(pcb_file).with_suffix(".kicad_pro"), here / "rules" / "jlc-4layer.json", NETCLASS_PATTERNS)
    print(f"{pcb_file}: {len(comps)} footprints, {len(nets)} nets, 4 layers, {W}x{H} mm")


main()
