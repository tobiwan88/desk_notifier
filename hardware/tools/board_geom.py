"""Dump board geometry as JSON for the review renderer (run with $KICAD_PYTHON).
Usage: board_geom.py board.kicad_pcb out.json
"""

import json
import sys

import pcbnew

mm = pcbnew.ToMM
board = pcbnew.LoadBoard(sys.argv[1])
box = board.GetBoardEdgesBoundingBox()
ox, oy = box.GetX(), box.GetY()


def pt(p):
    return [round(mm(p.x - ox), 3), round(mm(p.y - oy), 3)]


geom = {"size": [mm(box.GetWidth()), mm(box.GetHeight())], "edges": [], "pads": [], "tracks": [], "vias": [], "footprints": []}
for d in board.GetDrawings():
    if d.GetLayer() == pcbnew.Edge_Cuts and d.GetShape() == pcbnew.SHAPE_T_SEGMENT:
        geom["edges"].append([pt(d.GetStart()), pt(d.GetEnd())])
for fp in board.GetFootprints():
    bb = fp.GetCourtyard(pcbnew.F_CrtYd if not fp.IsFlipped() else pcbnew.B_CrtYd).BBox()
    geom["footprints"].append({"ref": fp.GetReference(), "value": fp.GetValue(), "pos": pt(fp.GetPosition()),
                               "side": "B" if fp.IsFlipped() else "F",
                               "box": [pt(bb.GetOrigin()), pt(bb.GetEnd())] if bb.GetWidth() else None})
    for p in fp.Pads():
        sz = p.GetBoundingBox()
        geom["pads"].append({"ref": fp.GetReference(), "num": p.GetNumber(), "net": p.GetNetname(),
                             "box": [pt(sz.GetOrigin()), pt(sz.GetEnd())], "tht": p.HasHole(),
                             "front": p.IsOnLayer(pcbnew.F_Cu), "back": p.IsOnLayer(pcbnew.B_Cu)})
for t in board.GetTracks():
    if t.GetClass() == "PCB_VIA":
        geom["vias"].append({"pos": pt(t.GetPosition()), "d": mm(t.GetWidth(pcbnew.F_Cu)), "net": t.GetNetname()})
    else:
        geom["tracks"].append({"a": pt(t.GetStart()), "b": pt(t.GetEnd()), "w": mm(t.GetWidth()),
                               "layer": board.GetLayerName(t.GetLayer()), "net": t.GetNetname()})
json.dump(geom, open(sys.argv[2], "w"))
print(f"{len(geom['pads'])} pads, {len(geom['tracks'])} tracks, {len(geom['vias'])} vias")
