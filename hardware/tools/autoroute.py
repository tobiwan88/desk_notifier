"""Autoroute a board with Freerouting (run with $KICAD_PYTHON).

board -> Specctra DSN (pcbnew) -> freerouting-cli -> SES -> board. kicad-cli 10 has no DSN
export, so this stays on the pcbnew bindings until KiCad 11 drops them.
Usage: autoroute.py board.kicad_pcb [max_passes]
"""

import os
import subprocess
import sys

import pcbnew

pcb_file = sys.argv[1]
passes = sys.argv[2] if len(sys.argv) > 2 else "100"
base = os.path.splitext(pcb_file)[0]
dsn, ses = base + ".dsn", base + ".ses"

board = pcbnew.LoadBoard(pcb_file)
if not pcbnew.ExportSpecctraDSN(board, dsn):
    sys.exit("DSN export failed")
run = subprocess.run([os.environ["FREEROUTING"], "-de", dsn, "-do", ses, "--router.autorouter.max_passes=" + passes],
                     capture_output=True, text=True, timeout=900)
with open(base + ".freerouting.log", "w") as f:
    f.write(run.stdout + run.stderr)
if run.returncode != 0 or not os.path.exists(ses):
    sys.exit(f"freerouting failed ({run.returncode}), see {base}.freerouting.log")
if not pcbnew.ImportSpecctraSES(board, ses):
    sys.exit("SES import failed")
board.Save(pcb_file)
print(f"routed {pcb_file}: {len(board.GetTracks())} track/via items")
