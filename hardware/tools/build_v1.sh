#!/bin/sh
# Regenerate notifier-v1: schematic (+ net check) -> netlist -> placed board. Routing is separate.
set -e
cd "$(dirname "$0")/../.."
. scripts/kicad-env.sh
P=hardware/notifier-v1
uv run --package desk-hardware python hardware/gen/notifier_v1.py "$P" 2>&1 | grep -v Fontconfig
kicad-cli sch export netlist -o $P/notifier-v1.net $P/notifier-v1.kicad_sch >/dev/null 2>&1
rm -f $P/notifier-v1.kicad_pcb
"$KICAD_PYTHON" hardware/gen/layout_v1.py $P/notifier-v1.net $P/notifier-v1.kicad_pcb 2>&1 | grep -v -e Fontconfig -e stdpbase
