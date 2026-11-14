#!/usr/bin/env bash
# Flash a built variant (from scripts/build-variants.sh) over USB.
# Usage: scripts/flash-variant.sh <squirrel|bunny|stag|cat|penguin> <serial port>
#   e.g. scripts/flash-variant.sh bunny /dev/cu.usbmodemXXXX
# Writes the parts listed in the build's flash_args (bootloader, partition table,
# OTA data, app), not the merged image: a merged image is gap-filled with 0xFF and
# would erase NVS (Wi-Fi, name, token, bonds).
set -eo pipefail

if [ $# -ne 2 ]; then
  echo "usage: $0 <squirrel|bunny|stag|cat|penguin> <serial port>" >&2
  exit 2
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/firmware/build-variants/$1"
if [ ! -f "$build/flash_args" ]; then
  echo "no build $build: run scripts/build-variants.sh $1 first" >&2
  exit 1
fi

source "$root/scripts/idf-env.sh"
cd "$build"
python -m esptool --chip esp32s3 -p "$2" -b 460800 --before default-reset --after hard-reset \
  write-flash @flash_args
