#!/bin/sh
# Install the routers into hardware/.tools (gitignored): KiCadRoutingTools 0.23.0 (own venv,
# prebuilt grid_router) and the Freerouting 2.5.0 JAR (needs Java: brew install openjdk).
# Idempotent. Needs git, uv, curl.
set -e
here="$(cd "$(dirname "$0")/.." && pwd)"
T="$here/.tools"; mkdir -p "$T"

K="$T/KiCadRoutingTools"; KRT_REV=caad45cd252f9a50f1fb4b3b15461bdf1b9457a1  # v0.23.0
if [ ! -d "$K/.git" ]; then git clone -q https://github.com/drandyhaas/KiCadRoutingTools.git "$K"; fi
git -C "$K" fetch -q origin && git -C "$K" checkout -q "$KRT_REV"
[ -x "$K/.venv/bin/python" ] || uv venv -q -p 3.13 "$K/.venv"
uv pip install -q -p "$K/.venv/bin/python" -r "$K/requirements.txt"
[ -n "$(ls "$K"/rust_router/grid_router*.so 2>/dev/null)" ] || (cd "$K" && .venv/bin/python build_router.py --tag v0.23.0)

F="$T/freerouting-2.5.0/freerouting-2.5.0.jar"
FR_SHA=f6f51bb02245e8e717f9359bd260cc9c5c0b1bc0acc8b7cb2cd5b8ffeb5de3c7
if [ ! -f "$F" ]; then
  mkdir -p "$(dirname "$F")"
  curl -fsSL -o "$F" https://github.com/freerouting/freerouting/releases/download/v2.5.0/freerouting-2.5.0.jar
fi
echo "$FR_SHA  $F" | shasum -a 256 -c -s || { echo "freerouting jar: sha256 mismatch" >&2; rm -f "$F"; exit 1; }
echo "tools OK: $T"
