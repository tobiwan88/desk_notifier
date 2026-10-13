"""Run kicad-cli DRC (with schematic parity) and print a grouped summary. Exit 1 on errors."""

import collections
import json
import subprocess
import sys
from pathlib import Path

pcb = Path(sys.argv[1])
out = Path(sys.argv[2] if len(sys.argv) > 2 else "hardware/out/drc.json")
out.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all", "--schematic-parity",
                "--refill-zones", "-o", str(out), str(pcb)], capture_output=True, check=True)
d = json.loads(out.read_text())
items = d["violations"] + d["schematic_parity"]
groups = collections.Counter((v["severity"], v["type"]) for v in items)
for (sev, typ), n in sorted(groups.items()):
    ex = next(v for v in items if v["type"] == typ)
    print(f"{n:3} {sev:7} {typ:28} e.g. {ex['description'][:70]} | {[i['description'][:40] for i in ex['items']]}")
print(f"unconnected: {len(d['unconnected_items'])}")
sys.exit(1 if any(s == "error" for s, _ in groups) else 0)
