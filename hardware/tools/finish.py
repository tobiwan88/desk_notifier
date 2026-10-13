"""After routing: GND pours on the outer layers (minus keepouts), refill all zones (run with $KICAD_PYTHON).
Usage: finish.py board.kicad_pcb
"""

import sys

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
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)
    z.SetThermalReliefGap(mm(0.25))
    z.SetThermalReliefSpokeWidth(mm(0.3))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    o = z.Outline()
    o.NewOutline()
    for x, y in ((edge.GetLeft(), edge.GetTop()), (edge.GetRight(), edge.GetTop()),
                 (edge.GetRight(), edge.GetBottom()), (edge.GetLeft(), edge.GetBottom())):
        o.Append(x, y)
    board.Add(z)
# drop router leftovers: tiny segments with a dangling end
board.BuildConnectivity()
conn = board.GetConnectivity()
for t in list(board.GetTracks()):
    if t.GetClass() == "PCB_TRACK" and t.GetLength() < mm(0.2) and conn.TestTrackEndpointDangling(t, False):
        board.Remove(t)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(sys.argv[1], board)
print(f"finished {sys.argv[1]}: {len(board.Zones())} zones")
