# Source me: puts kicad-cli and freerouting on PATH and points to KiCad's bundled Python.
KICAD_APP=/Applications/KiCad/KiCad.app/Contents
_root="$(cd "$(dirname "${BASH_SOURCE[0]:-${(%):-%x}}")/.." && pwd)"
export PATH="$KICAD_APP/MacOS:$PATH"
export KICAD_PYTHON="$KICAD_APP/Frameworks/Python.framework/Versions/Current/bin/python3"
export FREEROUTING="$_root/hardware/tools/freerouting"
unset _root
export KICAD_SYMBOL_DIR="$KICAD_APP/SharedSupport/symbols"
export KICAD_FOOTPRINT_DIR="$KICAD_APP/SharedSupport/footprints"
