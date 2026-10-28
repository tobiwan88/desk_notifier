# Autorouting (read when routing)

## KiCadRoutingTools (KRT), preferred
- Planes first (`route_planes.py`): one plane net per `role: plane` layer; plane layers stay solid.
- Signals (`route.py`) on `role: signal` layers only (`--layers`).
- `--ordering inside_out`: the default order put vias into pads.
- `--same-net-pad-clearance`: keep it at or above the same-net via clearance in the custom DRC file (stops same-net vias in pads).
- `--no-fix-drc-settings --fab-overrides <fab.router_rules>`: otherwise KRT rewrites the project's fab rules.
- `--power-nets-widths`: keep equal to the board's net classes and `BOARD.md` notes (review).
- `--keep-input-copper`: keeps locked, hand-drawn critical copper; `route.py` skips complete nets.
- Diff pairs: KRT pairs nets by name suffix (see kicad-schematic).
- Pin the hash seed (`PYTHONHASHSEED`) for repeatable runs; output still depends on object ids, so loop place, route, gate and keep the first pass.

## Freerouting, fallback
- Use a wrapper that turns automatic neckdown off (tracks thinner than the net class near fine pads).
- The native CLI has hung in the past; run the JAR.

## After routing
- Remove dangling stubs and vias one at a time, DRC-driven, reverting any removal that opens a connection.
- With pcbnew SWIG: keep Python references to items you iterate, or the board is freed under you.
- Check pairs and critical nets with `calc/critical.py` (skew, vias, FB length); layer and width per pair: script candidate (review until then).
