"""After routing: GND pours on the outer layers (minus keepouts), refill all zones (run with $KICAD_PYTHON).
Usage: finish.py board.kicad_pcb
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile

import pcbnew

mm = pcbnew.FromMM
board = pcbnew.LoadBoard(sys.argv[1])
gnd = board.FindNet("/GND")
edge = board.GetBoardEdgesBoundingBox()
for z in list(board.Zones()):
    if not z.GetIsRuleArea() and z.GetLayer() in (pcbnew.F_Cu, pcbnew.B_Cu):
        board.Remove(z)
for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(gnd)
    z.SetLocalClearance(mm(0.25))
    z.SetMinThickness(mm(0.2))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)  # thermal spokes on the THT header got starved
    z.SetThermalReliefGap(mm(0.25))
    z.SetThermalReliefSpokeWidth(mm(0.3))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    o = z.Outline()
    o.NewOutline()
    for x, y in ((edge.GetLeft(), edge.GetTop()), (edge.GetRight(), edge.GetTop()),
                 (edge.GetRight(), edge.GetBottom()), (edge.GetLeft(), edge.GetBottom())):
        o.Append(x, y)
    board.Add(z)

# In1 GND under the sensor island + neck: KRT's plane router makes one solid full-board zone, which the old
# keepout left with a hole. Replace it with a solid zone minus island/neck plus a HATCHED zone there
# (thermal isolation: ~35% copper; return path for the I2C tracks that leave through the neck).
# Geometry mirrors layout_v1.py (S, IH, neck 4.5 x 2.8); mm relative to the board's top-left corner.
S, IH, NW, NH = 1.2, 11.8, 4.5, 2.8
x0, y0, x1, y1 = (v / 1e6 for v in (edge.GetLeft(), edge.GetTop(), edge.GetRight(), edge.GetBottom()))
for z in list(board.Zones()):
    if not z.GetIsRuleArea() and z.GetLayer() == pcbnew.In1_Cu:
        board.Remove(z)


def in1(pts, hatch):
    z = pcbnew.ZONE(board)
    z.SetLayer(pcbnew.In1_Cu)
    z.SetNet(gnd)
    z.SetLocalClearance(mm(0.2))
    z.SetMinThickness(mm(0.2))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    if hatch:
        z.SetAssignedPriority(1)
        z.SetFillMode(pcbnew.ZONE_FILL_MODE_HATCH_PATTERN)
        z.SetHatchThickness(mm(0.3))
        z.SetHatchGap(mm(1.2))
        z.SetHatchOrientation(pcbnew.EDA_ANGLE(45, pcbnew.DEGREES_T))
    o = z.Outline()
    o.NewOutline()
    for x, y in pts:
        o.Append(mm(x0 + x), mm(y0 + y))
    board.Add(z)


W, H = x1 - x0, y1 - y0
ov = 0.6  # hatch overlaps the solid plane so it is connected
in1([(10.4 + S, 0), (W, 0), (W, H), (0, H), (0, IH + NH), (NW, IH + NH), (NW, IH + S + ov), (10.4 + S, IH + S + ov)], False)
in1([(0, 0), (10.4 + S, 0), (10.4 + S, IH), (NW, IH), (NW, IH + NH + ov), (0, IH + NH + ov)], True)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(sys.argv[1], board)

# drop router leftovers (dangling tracks/vias) one at a time; keep a removal only if it opens nothing


def drc(path):
    report = os.path.join(tempfile.mkdtemp(), "drc.json")
    subprocess.run(["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all", "-o", report, path],
                   capture_output=True, check=True)
    d = json.load(open(report))
    dangling = [i["uuid"] for v in d["violations"] if v["type"] in ("via_dangling", "track_dangling") for i in v["items"]]
    return dangling, len(d["unconnected_items"])


dangling, open_before = drc(sys.argv[1])
tried = set()
keep = []  # SWIG frees a board/filler the moment its last Python reference goes: later loads then break
while dangling:
    uuid = next((u for u in dangling if u not in tried), None)
    if uuid is None:
        break
    tried.add(uuid)
    backup = sys.argv[1] + ".bak"
    shutil.copy(sys.argv[1], backup)
    board = pcbnew.LoadBoard(sys.argv[1])
    keep.append(board)
    for t in list(board.GetTracks()):
        if t.m_Uuid.AsString() == uuid:
            board.Remove(t)
    keep.append(pcbnew.ZONE_FILLER(board))
    keep[-1].Fill(board.Zones())
    pcbnew.SaveBoard(sys.argv[1], board)
    dangling, open_after = drc(sys.argv[1])
    if open_after > open_before:
        shutil.copy(backup, sys.argv[1])
        dangling, _ = drc(sys.argv[1])
        print(f"kept {uuid}: removing it opens a connection")
    else:
        print(f"removed dangling {uuid}")
    os.remove(backup)
board = pcbnew.LoadBoard(sys.argv[1])
print(f"finished {sys.argv[1]}: {len(board.Zones())} zones")
