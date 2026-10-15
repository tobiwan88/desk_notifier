#!/bin/sh
# Route a placed board with KiCadRoutingTools: planes (In1 GND, In2 3V3), USB pair, then the rest
# on the outer layers only, so the inner planes stay solid. Fab floors pinned (rules/krt-fab.txt).
# Usage: route_krt.sh placed.kicad_pcb routed.kicad_pcb
# Stage boards live in a temp dir (no stray KiCad projects in the repo); logs go to hardware/out/route/.
set -e
here="$(cd "$(dirname "$0")/.." && pwd)"
K="$here/.tools/KiCadRoutingTools"; P="$K/.venv/bin/python"; F="$here/rules/krt-fab.txt"
[ -x "$P" ] || { echo "KiCadRoutingTools missing: run hardware/tools/setup_tools.sh" >&2; exit 1; }
in="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; out="$2"
work="$(mktemp -d)"; logs="$here/out/route"
trap 'mkdir -p "$logs"; cp "$work"/*.log "$work"/route.json "$logs"/ 2>/dev/null; rm -rf "$work"' EXIT
cd "$work"
pro="${in%.kicad_pcb}.kicad_pro"
cp "$in" placed.kicad_pcb
for f in placed poured diffed; do cp "$pro" "$f.kicad_pro"; done
common="--fab-overrides $F --no-fix-drc-settings --same-net-pad-clearance 0.1"
$P "$K/py_router/route_planes.py" placed.kicad_pcb poured.kicad_pcb --nets "GND" "+3V3" \
    --plane-layers In1.Cu In2.Cu --clearance 0.15 $common > planes.log 2>&1
$P "$K/py_router/route_diff.py" poured.kicad_pcb diffed.kicad_pcb --nets "/USB_DP" "/USB_DN" \
    --layers F.Cu B.Cu $common > diff.log 2>&1
# inside_out: the default ordering put vias into EN pads despite --same-net-pad-clearance
$P "$K/py_router/route.py" diffed.kicad_pcb routed.kicad_pcb --nets "*" --layers F.Cu B.Cu --ordering inside_out \
    --power-nets "VBUS_IN" "VBUS" "SW" "+3V3" "GND" --power-nets-widths 0.4 0.4 0.4 0.2 0.2 --track-width 0.2 \
    --clearance 0.15 --json-out route.json $common > route.log 2>&1
grep -h "JSON_SUMMARY_MIN" route.log | cut -c1-400
cd - >/dev/null
cp "$work/routed.kicad_pcb" "$out"
