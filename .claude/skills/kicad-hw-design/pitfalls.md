# Pitfalls: symptom -> cause -> fix

## Schematic
| Symptom | Cause | Fix |
|---|---|---|
| LED/diode reversed, ERC clean | kicad-sch-api 0.5.x rotates pins wrong at 90/270 | `schkit` does its own pin math; keep the net-intent check against the KiCad netlist |
| Labels all left-justified, unreadable | kicad-sch-api default | `schkit` sets justification |

## DRC / project
| Symptom | Cause | Fix |
|---|---|---|
| DRC passes with odd rules | no `.kicad_pro` next to the board: KiCad falls back to defaults | `write_project.py` writes it; never run DRC on a bare board copy |
| New DRC error from a stray project | second `.kicad_pro` in repo | one project only; temp dirs for intermediates |
| Via inside an SMD pad | router ordering | `.kicad_dru` "no via in SMD pad" rule catches it; route with `--ordering inside_out` |

## Routing
| Symptom | Cause | Fix |
|---|---|---|
| Freerouting native CLI crashes/hangs | freerouting-cli 2.5.0 router thread bug | use the JAR via `tools/freerouting` (wrapper disables analytics, on by default); left 12 nets open here anyway |
| No DSN from kicad-cli | kicad-cli 10 has no DSN export | `autoroute.py` uses pcbnew SWIG |
| KRT changed project fab rules | it rewrites them | `--no-fix-drc-settings --fab-overrides rules/krt-fab.txt` |
| GND/3V3 plane cut by signals | KRT uses inner layers | `--layers F.Cu B.Cu` for signals (planes via `route_planes.py` on In1/In2) |
| Vias in pads | default ordering | `--ordering inside_out` + `--same-net-pad-clearance` + dru rule |
| Dangling stubs/vias | KRT leftovers | `finish.py` removes them DRC-driven, one at a time, reverting if a connection opens |
| `finish.py` breaks on 2nd iteration | SWIG frees board/filler when last Python ref goes | keep references (`keep` list) |
| Router results differ run to run | non-deterministic | after experiments restore the committed board (`git checkout hardware/notifier-v1/`); pipeline output is not byte-stable |
| Fine-pitch DFN (SHT4x) won't route | 0.3 mm pad gap + footprint keepout allows only straight exits | hand-drawn escape stubs in `layout_v1.py` (the router extends them) |
| Sensor island nets unconnected | no pours there | all nets go through the neck as tracks; allow tracks in the island keepout |
| Tracks thinner than the netclass near fine pads | Freerouting's automatic neckdown | wrapper passes `--router.automatic_neckdown=false`; prefer KRT |

## JLC / fab
| Symptom | Cause | Fix |
|---|---|---|
| `--bom` fails on stock | C61/C5 modules had zero JLC stock | pick an in-stock module (S3-WROOM N8R2, fallback N8R8 needs `CONFIG_SPIRAM_MODE_OCT=y`); add Kconfig board choice, check pin map in `checks.py` |
| Part rotated wrong in JLC preview | CPL corrections from an old community table | verify every IC in JLC's placement preview; record fixes in `ROTATION` in `fab_package.py` |
| Unexpected cost | each Extended part has a setup fee | swap to Basic when equivalent; TH header is hand-soldered (Standard PCBA) |
| Stock drifts between check and order | BME688 stock fluctuates | rerun `scripts/hw-check.sh --bom --qty N` right before ordering |

## Environment
| Symptom | Cause | Fix |
|---|---|---|
| `uv run` can't import desk-hardware tools | wrong package scope | `uv run --package desk-hardware` |
| `pcbnew` import error | system Python | use `$KICAD_PYTHON` after `source scripts/kicad-env.sh` |
| Router missing | fresh checkout | `hardware/tools/setup_tools.sh` |
| Fresh host build: `ld: tapi error: malformed file ... libSystem.B.tbd: unknown architecture` | Command Line Tools SDK 27.0 vs the linker | `SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk scripts/check.sh` (old build dirs keep working) |
| `brew install --cask temurin` fails | cask needs sudo | `brew install openjdk` (`tools/freerouting` finds it) |
| Regenerated schematic: huge diff | every run assigns new UUIDs | after test runs restore the committed files; only commit a regeneration with a real change |

## KiCad GUI (observed, partly unexplained)
- IPC API (`api.enable_server` in `kicad_common.json`): the standalone "PCB Editor" app created `/tmp/kicad/api.lock` but no socket and hung on a dialog; KiCad.app (project manager) created `api.sock`. The API needs a running GUI until KiCad 11 (headless `kicad-cli api-server`); live editing is not in the pipeline.
- KiCad reopens the last project from `~/Library/Preferences/kicad/10.0/kicad.json` (`open_projects`), ignoring the file passed on the command line: this is how a stray intermediate project (`out/v1-placed.kicad_pro`, no schematic) kept showing up.
- `~*.lck` lock files from killed instances make KiCad show a modal "already open" dialog: delete them when no KiCad runs.
- One startup hang (main thread blocked in `open()` inside libgit2 repo discovery, `PROJECT_TREE_PANE::ReCreateTreePrj`) was sampled once and not reproducible; likely a macOS privacy prompt. Diagnose a hang with `sample <pid> 5` and `lsof -p <pid>`.
- pcbnew SWIG is deprecated and due to be removed in KiCad 11: `layout_v1.py`, `finish.py`, `board_geom.py` will need the IPC API then.
