#!/usr/bin/env bash
# Hardware gate: ERC/DRC (+ waivers), pin map vs firmware Kconfig. `--bom` adds the live JLC stock check.
set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/kicad-env.sh
if ! command -v kicad-cli >/dev/null; then echo "SKIP: kicad-cli not installed" >&2; exit 0; fi
uv run python hardware/tools/checks.py "$@"
