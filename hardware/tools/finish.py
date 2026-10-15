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
gnd = board.FindNet("GND")
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
