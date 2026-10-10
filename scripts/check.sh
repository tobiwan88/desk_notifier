#!/usr/bin/env bash
# Single local gate. `--quick` skips the IDF build.
set -euo pipefail
cd "$(dirname "$0")/.."
quick=0; [[ "${1:-}" == "--quick" ]] && quick=1

# core/ must stay free of ESP-IDF includes
if grep -rEn '#include *[<"](esp_|freertos/)' firmware/components/*/core 2>/dev/null; then
  echo "FAIL: ESP-IDF include under core/" >&2; exit 1
fi

cmake -S firmware/test/host -B build/host -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build build/host -j
./build/host/host_tests

uv run pytest -q

if [[ $quick -eq 0 ]]; then
  . scripts/idf-env.sh
  if command -v idf.py >/dev/null; then (cd firmware && idf.py build); else echo "SKIP: idf.py not installed" >&2; fi
fi
echo "check OK"
