"""Write <board>.kicad_pro with the fab rules and net classes (KiCad keeps both in the project file)."""

import json
import sys
from pathlib import Path


def write(pro: Path, rules_file: Path, patterns: dict[str, list[str]]) -> None:
    fab = json.loads(rules_file.read_text())
    base = {"line_style": 0, "pcb_color": "rgba(0, 0, 0, 0.000)", "schematic_color": "rgba(0, 0, 0, 0.000)",
            "priority": 2147483647, "wire_width": 6, "bus_width": 12, "microvia_diameter": 0.3, "microvia_drill": 0.1,
            "diff_pair_via_gap": 0.25}
    classes = []
    for i, c in enumerate(fab["classes"]):
        classes.append({**base, **c, "priority": 2147483647 if c["name"] == "Default" else i})
    doc = {
        "board": {"design_settings": {"rules": fab["rules"], "defaults": {}, "diff_pair_dimensions": [],
                                      "drc_exclusions": [], "track_widths": [], "via_dimensions": []}},
        "boards": [],
        "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "meta": {"filename": pro.name, "version": 3},
        "net_settings": {"classes": classes, "meta": {"version": 4},
                         "netclass_patterns": [{"netclass": n, "pattern": p} for n, ps in patterns.items() for p in ps]},
        "pcbnew": {"page_layout_descr_file": ""},
        "sheets": [],
        "text_variables": {},
    }
    pro.write_text(json.dumps(doc, indent=2) + "\n")


if __name__ == "__main__":
    write(Path(sys.argv[1]), Path(sys.argv[2]), json.loads(sys.argv[3]))
