"""After routing: GND pours on the outer layers (minus keepouts), refill all zones (run with $KICAD_PYTHON).
Usage: finish.py board.kicad_pcb
"""

import json
import math
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

# In1 GND: KRT's plane router makes one full-board zone. Replace it with a solid zone that leaves out the sensor
# island and the neck (thermal isolation; the neck bus carries its own GND guards).
# Geometry mirrors layout_v1.py (S, SM, IH); mm relative to the board's top-left corner.
S, SM, IH = 1.2, 1.8, 14.8
x0, y0, x1, y1 = (v / 1e6 for v in (edge.GetLeft(), edge.GetTop(), edge.GetRight(), edge.GetBottom()))
for z in list(board.Zones()):
    if not z.GetIsRuleArea() and z.GetLayer() == pcbnew.In1_Cu:
        board.Remove(z)
W, H = x1 - x0, y1 - y0
z = pcbnew.ZONE(board)
z.SetLayer(pcbnew.In1_Cu)
z.SetNet(gnd)
z.SetLocalClearance(mm(0.2))
z.SetMinThickness(mm(0.2))
z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
o = z.Outline()
o.NewOutline()
for x, y in ((10.4 + SM, 0), (W, 0), (W, H), (0, H), (0, IH + S), (10.4 + SM, IH + S)):
    o.Append(mm(x0 + x), mm(y0 + y))
board.Add(z)
for z in board.Zones():  # KRT's In2 plane keeps unconnected slivers between the module's thermal vias
    if not z.GetIsRuleArea() and z.GetLayer() == pcbnew.In2_Cu:
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)


# Router vias too close to same-net SMD pads (rule "via to same-net SMD pad", >= 0.2 mm copper to pad: solder wicks into
# vias beside a pad). The router sometimes drops them into the pad or at 0.1 mm; nudge each such via to the nearest
# free spot (other copper 0.2 mm, other vias 0.6 mm apart, edge) and drag the track ends with it. Locked (hand-drawn)
# vias are left alone: layout_v1.py places them right. checks.py's DRC has the last word.
SAME_NET_GAP = 0.2


def too_close(v, pos):
    c = pcbnew.SHAPE_CIRCLE(pos, v.GetWidth() // 2)
    return any(q.GetNetCode() == v.GetNetCode() and q.GetAttribute() == pcbnew.PAD_ATTRIB_SMD
               and q.GetEffectiveShape(pcbnew.F_Cu).Collide(c, mm(SAME_NET_GAP + 0.01))
               for f in board.GetFootprints() for q in f.Pads() if q.IsOnLayer(pcbnew.F_Cu))


def spot_ok(v, p):
    rx, ry = p.x / 1e6 - x0, p.y / 1e6 - y0
    if not (0.45 < rx < W - 0.45 and 0.45 < ry < H - 0.45) or (rx < 10.4 + SM + 0.45 and ry < IH + S + 0.45):
        return False
    if any(z.GetIsRuleArea() and z.GetDoNotAllowVias() and z.Outline().Contains(p) for z in board.Zones()):
        return False
    circle = pcbnew.SHAPE_CIRCLE(p, v.GetWidth() // 2)
    if too_close(v, p):
        return False
    if any(o is not v and (o.GetPosition() - p).EuclideanNorm() < mm(0.6) for o in board.GetTracks() if o.GetClass() == "PCB_VIA"):
        return False
    if any(t.GetNetCode() != v.GetNetCode() and t.GetEffectiveShape(t.GetLayer()).Collide(circle, mm(0.2))
           for t in board.GetTracks() if t.GetClass() != "PCB_VIA"):
        return False
    return not any(q.GetNetCode() != v.GetNetCode() and q.GetEffectiveShape(layer).Collide(circle, mm(0.2))
                   for f in board.GetFootprints() for q in f.Pads() for layer in (pcbnew.F_Cu, pcbnew.B_Cu) if q.IsOnLayer(layer))


for v in [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and not t.IsLocked()]:
    old = v.GetPosition()
    if not too_close(v, old):
        continue
    spot = None
    for r in (0.15, 0.25, 0.35, 0.45, 0.6, 0.8, 1.0):
        cands = [pcbnew.VECTOR2I(old.x + mm(r * math.cos(k * math.pi / 12)), old.y + mm(r * math.sin(k * math.pi / 12))) for k in range(24)]
        cands = [p for p in cands if spot_ok(v, p)]
        if cands:
            spot = cands[0]
            break
    if spot is None:
        print(f"no spot for the {v.GetNetname()} via near a same-net pad")
        continue
    pad_hit = next((q for f in board.GetFootprints() for q in f.Pads() if q.GetNetCode() == v.GetNetCode()
                    and q.IsOnLayer(pcbnew.F_Cu) and q.HitTest(old)), None)
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA" or t.GetNetCode() != v.GetNetCode():
            continue
        if t.GetStart() == old:
            t.SetStart(spot)
        if t.GetEnd() == old:
            t.SetEnd(spot)
    v.SetPosition(spot)
    if pad_hit is not None:  # the via sat in the pad: connect pad and via with a short F.Cu track
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pad_hit.GetPosition()), t.SetEnd(spot), t.SetWidth(mm(0.2)), t.SetLayer(pcbnew.F_Cu), t.SetNet(v.GetNet())
        board.Add(t)
    print(f"moved a {v.GetNetname()} via away from a same-net pad")

# GND via beside every layer change of the display SPI lines (40-80 MHz): F.Cu references In1 (GND), B.Cu In2
# (3V3), so the return current needs a nearby path between the GND layers. First free spot 0.9-1.5 mm from the
# signal via, checked against other copper (0.2 mm), SMD pads (no via in pad), vias (hole spacing) and the edge;
# checks.py's DRC has the last word.
def free_spot(pos):
    for r in (0.9, 1.1, 1.3, 1.5):
        for k in range(16):
            a = k * math.pi / 8
            p = pcbnew.VECTOR2I(pos.x + mm(r * math.cos(a)), pos.y + mm(r * math.sin(a)))
            rx, ry = p.x / 1e6 - x0, p.y / 1e6 - y0
            if not (0.6 < rx < W - 0.6 and 0.6 < ry < H - 0.6) or (rx < 10.4 + SM + 0.6 and ry < IH + S + 0.6):
                continue  # board edge, sensor island/neck/slot
            if any(z.GetIsRuleArea() and z.GetDoNotAllowVias() and z.Outline().Contains(p) for z in board.Zones()):
                continue
            circle = pcbnew.SHAPE_CIRCLE(p, mm(0.3))
            if any((v.GetPosition() - p).EuclideanNorm() < mm(0.9) for v in board.GetTracks() if v.GetClass() == "PCB_VIA"):
                continue
            if any(t.GetNetCode() != gnd.GetNetCode() and t.GetEffectiveShape(t.GetLayer()).Collide(circle, mm(0.2))
                   for t in board.GetTracks() if t.GetClass() != "PCB_VIA"):
                continue
            if any(q.GetEffectiveShape(layer).Collide(circle, mm(0.2) if q.GetNetCode() != gnd.GetNetCode() else mm(0.1))
                   for f in board.GetFootprints() for q in f.Pads() for layer in (pcbnew.F_Cu, pcbnew.B_Cu) if q.IsOnLayer(layer)):
                continue
            return p
    return None


for sig in [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname().split("/")[-1] in ("LCD_SCLK", "MCU_LCD_SCLK", "LCD_MOSI")]:
    p = free_spot(sig.GetPosition())
    if p is None:
        print(f"no room for a GND via next to the {sig.GetNetname()} via")
        continue
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(p), v.SetWidth(mm(0.6)), v.SetDrill(mm(0.3)), v.SetNet(gnd)
    board.Add(v)
    print(f"GND return via next to the {sig.GetNetname()} via")
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
