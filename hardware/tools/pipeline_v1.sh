#!/bin/sh
# Full notifier-v1 chain: schematic -> placement -> KRT routing -> pours -> checks -> review pack -> fab package.
#   pipeline_v1.sh [--bom]             regenerate and re-route everything (the router output changes between runs)
#   pipeline_v1.sh --fab-only [--bom]  ORDER step: checks + fab package on the committed board as is. Nothing is
#                                      regenerated or routed, nothing tracked is written; refuses a dirty hardware/ tree.
set -e
cd "$(dirname "$0")/../.."
. scripts/kicad-env.sh
P=hardware/notifier-v1/notifier-v1
if [ "$1" = "--fab-only" ]; then
    shift
    before="$(git status --porcelain -- hardware)"
    if [ -n "$before" ]; then
        echo "hardware/ has uncommitted changes: commit them first, the silkscreen SHA must match the board you order." >&2
        echo "$before" >&2
        exit 1
    fi
    uv run --package desk-hardware python hardware/tools/checks.py "$@"
    uv run --package desk-hardware python hardware/tools/fab_package.py --require-clean $P.kicad_pcb
    after="$(git status --porcelain -- hardware)"
    if [ -n "$after" ] || grep -q dirty hardware/out/fab/notifier-v1/VERSION.txt; then
        echo "fab-only run changed tracked files or the package says -dirty: do not order." >&2
        echo "$after" >&2
        exit 1
    fi
    echo "fab package: $(cat hardware/out/fab/notifier-v1/VERSION.txt) (clean tree, board not regenerated)"
    exit 0
fi
hardware/tools/build_v1.sh
cp $P.kicad_pcb hardware/out/v1-placed.kicad_pcb  # unrouted, for comparison (no .kicad_pro: not a project)
# The router is not robust on the crowded corner left of the module (open EN/GND/VBUS nets, a via in a pad) and its
# result depends on the (random) object ids of the placed board, not on the hash seed. So: regenerate the placement
# step (new ids, same design) and route again until the gate (ERC + DRC, no JLC stock lookup) passes; KRT_TRIES attempts.
tries="${KRT_TRIES:-12}"; n=1
while :; do
    if [ "$n" -gt 1 ]; then
        rm -f $P.kicad_pcb
        "$KICAD_PYTHON" hardware/gen/layout_v1.py $P.net $P.kicad_pcb 2>&1 | grep -v -e Fontconfig -e stdpbase -e swig
        cp $P.kicad_pcb hardware/out/v1-placed.kicad_pcb
    fi
    hardware/tools/route_krt.sh $P.kicad_pcb $P.kicad_pcb
    "$KICAD_PYTHON" hardware/tools/finish.py $P.kicad_pcb 2>&1 | grep -v -e Fontconfig -e stdpbase -e swig
    if uv run --package desk-hardware python hardware/tools/checks.py > hardware/out/attempt.log 2>&1; then
        echo "routing attempt $n: gate OK"
        break
    fi
    echo "routing attempt $n: gate FAILED ($(grep -c '^FAIL' hardware/out/attempt.log) findings)"
    [ "$n" -ge "$tries" ] && break
    n=$((n + 1))
done
uv run --package desk-hardware python hardware/tools/checks.py "$@"
"$KICAD_PYTHON" hardware/tools/board_geom.py $P.kicad_pcb hardware/out/geom.json 2>&1 | grep -v -e Fontconfig -e stdpbase -e swig
uv run --package desk-hardware python hardware/tools/review_pack.py $P.kicad_pcb 2>&1 | grep -v Fontconfig
uv run --package desk-hardware python hardware/tools/render_review.py hardware/out/geom.json hardware/out/review/notifier-v1 \
    usb=6,21,26,36 power=20,22,40,36 sensor=0,0,16,14 module_left=10,0,20,20 display=32,8,50,30 antenna=8,0,42,8
uv run --package desk-hardware python hardware/tools/fab_package.py $P.kicad_pcb
