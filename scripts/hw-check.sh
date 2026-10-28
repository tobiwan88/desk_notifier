#!/usr/bin/env bash
# Hardware gate: board-neutral calc checks (hardware/calc), ERC/DRC (+ waivers), pin map vs firmware Kconfig.
# `--bom` adds the live JLC stock check. Without kicad-cli only the calc checks run (they read the board files directly).
set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/kicad-env.sh
if ! command -v kicad-cli >/dev/null; then
    echo "SKIP: kicad-cli not installed: ERC/DRC/pin map skipped, calc checks only" >&2
    exec uv run --package desk-hardware python hardware/tools/checks.py --calc-only
fi
uv run --package desk-hardware python hardware/tools/checks.py "$@"
