"""Create or update a .kicad_pcb from a KiCad netlist (run with $KICAD_PYTHON, needs pcbnew).

New footprints are placed on a grid inside the outline; existing ones keep their position.
Usage: pcb_from_netlist.py design.net board.kicad_pcb [width_mm height_mm]
"""

import os
import re
import sys

import pcbnew

FP_DIR = os.environ["KICAD_FOOTPRINT_DIR"]
mm = pcbnew.FromMM


def parse(net_text):
    comps = {}
    for blk in net_text.split("(nets", 1)[0].split("(comp\n")[1:]:
        ref = re.search(r'\(ref "([^"]+)"', blk).group(1)
        value = re.search(r'\(value "([^"]*)"', blk).group(1)
        fpid = re.search(r'\(footprint "([^"]*)"', blk).group(1)
        uuid = re.findall(r'\(tstamps "([^"]+)"\)', blk)[-1]
        fields = dict(re.findall(r'\(field\s*\(name "([^"]+)"\)\s*"([^"]*)"\)', blk.split("(libsource", 1)[0]))
        fields.pop("Footprint", None)
        no_bom = '(name "exclude_from_bom")' in blk
        comps[ref] = (value, fpid, uuid, fields, no_bom)
    pads = {}
    for blk in net_text.split("(nets", 1)[1].split("(net\n")[1:]:
        name = re.search(r'\(name "([^"]+)"', blk).group(1)
        for ref, pin in re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"', blk):
            pads[(ref, pin)] = name
    return comps, pads


def outline(board, w, h):
    pts = [(0, 0), (w, 0), (w, h), (0, h)]
    for (x1, y1), (x2, y2) in zip(pts, pts[1:] + pts[:1]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
        seg.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(mm(0.1))
        board.Add(seg)


def main():
    net_file, pcb_file = sys.argv[1], sys.argv[2]
    w, h = (float(sys.argv[3]), float(sys.argv[4])) if len(sys.argv) > 4 else (30.0, 20.0)
    comps, pads = parse(open(net_file).read())
    new = not os.path.exists(pcb_file)
    board = pcbnew.BOARD() if new else pcbnew.LoadBoard(pcb_file)
    if new:
        outline(board, w, h)
    nets = {}
    for name in sorted(set(pads.values())):
        net = board.FindNet(name)
        if net is None:
            net = pcbnew.NETINFO_ITEM(board, name)
            board.Add(net)
        nets[name] = net
    existing = {fp.GetReference(): fp for fp in board.GetFootprints()}
    x, y = 4.0, 4.0
    for ref, (value, fpid, uuid, fields, no_bom) in sorted(comps.items()):
        fp = existing.get(ref)
        if fp is None:
            lib, name = fpid.split(":")
            fp = pcbnew.FootprintLoad(os.path.join(FP_DIR, lib + ".pretty"), name)
            fp.SetFPID(pcbnew.LIB_ID(lib, name))
            fp.SetReference(ref)
            fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
            board.Add(fp)
            x += 6
            if x > w - 4:
                x, y = 4.0, y + 6
        fp.SetValue(value)
        fp.SetPath(pcbnew.KIID_PATH("/" + uuid))
        for pad in fp.Pads():
            name = pads.get((ref, pad.GetNumber()))
            if name:
                pad.SetNet(nets[name])
    board.Save(pcb_file)
    print(f"{pcb_file}: {len(comps)} footprints, {len(nets)} nets")


if __name__ == "__main__":
    main()
