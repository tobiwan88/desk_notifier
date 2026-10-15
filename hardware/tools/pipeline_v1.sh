#!/bin/sh
# Full notifier-v1 chain: schematic -> placement -> KRT routing -> pours -> checks -> review pack -> fab package.
set -e
cd "$(dirname "$0")/../.."
. scripts/kicad-env.sh
P=hardware/notifier-v1/notifier-v1
hardware/tools/build_v1.sh
cp $P.kicad_pcb hardware/out/v1-placed.kicad_pcb  # unrouted, for comparison (no .kicad_pro: not a project)
hardware/tools/route_krt.sh $P.kicad_pcb $P.kicad_pcb
"$KICAD_PYTHON" hardware/tools/finish.py $P.kicad_pcb 2>&1 | grep -v -e Fontconfig -e stdpbase -e swig
uv run --package desk-hardware python hardware/tools/checks.py "$@"
"$KICAD_PYTHON" hardware/tools/board_geom.py $P.kicad_pcb hardware/out/geom.json 2>&1 | grep -v -e Fontconfig -e stdpbase -e swig
uv run --package desk-hardware python hardware/tools/review_pack.py $P.kicad_pcb 2>&1 | grep -v Fontconfig
uv run --package desk-hardware python hardware/tools/render_review.py hardware/out/geom.json hardware/out/review/notifier-v1 \
    usb=6,21,26,36 power=20,22,40,36 sensor=0,0,16,14 module_left=10,0,20,20 display=32,8,50,30 antenna=8,0,42,8
uv run --package desk-hardware python hardware/tools/fab_package.py $P.kicad_pcb
