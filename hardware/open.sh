#!/bin/sh
# Open the notifier-v1 KiCad project (schematic + PCB) from this checkout.
exec open -a /Applications/KiCad/KiCad.app "$(cd "$(dirname "$0")" && pwd)/notifier-v1/notifier-v1.kicad_pro"
