# Autorouting (read when routing)

## KiCadRoutingTools (KRT), preferred
- Planes first (`route_planes.py`): one plane net per inner layer; inner layers stay solid.
- Signals (`route.py`) on outer layers only: `--layers F.Cu B.Cu`.
- `--ordering inside_out`: the default order put vias into pads.
- `--same-net-pad-clearance 0.1`: stops same-net vias in pads.
- `--no-fix-drc-settings --fab-overrides <router rules file>`: otherwise KRT rewrites the project's fab rules.
- `--power-nets-widths`: power widths from the board's net classes.
- `--keep-input-copper`: keeps locked, hand-drawn critical copper; `route.py` skips complete nets.
- KRT pairs diff nets by suffix only (`_DP/_DN`, `_P/_N`).
- Output is not deterministic and can depend on object ids: loop "place, route, gate" a few times and keep the first pass.

## Freerouting, fallback
- Use a wrapper that turns automatic neckdown off (tracks thinner than the net class near fine pads).
- The native CLI has hung in the past; run the JAR.

## After routing
- Remove dangling stubs and vias one at a time, DRC-driven, reverting any removal that opens a connection.
- With pcbnew SWIG: keep Python references to items you iterate, or the board is freed under you.
- Check each pair and critical net with `calc/critical.py` (length, skew, vias), not by eye.
