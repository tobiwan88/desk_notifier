---
name: kicad-layout
description: Use for PCB placement and routing in KiCad: placement generators, keepouts, critical blocks, autorouting, DRC failures, layout review from renders, fab outputs (gerbers, drill, zones), and KiCad GUI problems (locks, stray projects, hangs). Read hw-base first; schematic is kicad-schematic, parts and ordering are hw-parts.
---

# KiCad layout

Read `hw-base`, then the board's `BOARD.md`, `board.yaml` and `critical.yaml`.

## Source of truth
- The board is generated. Edit the layout generator (placement, outline, planes, keepouts, hand-drawn copper), not the `.kicad_pcb`.
- Design rules live in files the generator writes (project rules + `.kicad_dru`), not in the GUI. Fab floors appear in the fab rules file and the router rules file: change both or neither.
- One `.kicad_pro` per board in the repo. Intermediates go to temp dirs. Never run DRC on a bare board copy.

## Order of work
1. Fixed parts: connectors, buttons, mounting holes, modules, display sockets.
2. Critical blocks from `critical.yaml`, in this order: `switcher_loop`, `decoupling`, `feedback`, `series_termination`, `diff_pair`. Place them tight and in signal-flow order.
3. Remaining parts by block: power, digital core, sensors, interfaces. Inputs at the edge they enter from.
4. Planes and keepouts (antenna, thermal islands, connector shells) as DRC rule areas.
5. Critical copper drawn by the generator and locked: short, geometric, or what the router keeps failing. Coordinates from the placed pads, never hard-coded; assert the orientation you rely on.
6. Autoroute the rest on the signal layers only; planes stay solid. Router details: `references/routing.md`.
7. Cleanup of router leftovers, zone fill, then the gate.

## Rules (each with its check)
- Every new critical block gets a `critical.yaml` entry before routing (`calc/critical.py`).
- Regulator input loop smallest, inductor at the switch node, FB divider at the pin and away from the switch node (`switcher_loop`, `feedback`).
- Decoupling cap at the pin, power routed through the cap pads, not a stub off a shared trace (`decoupling`).
- Diff pair: net-class width/gap from `calc/impedance.py`, one outer layer next to a GND plane, skew and vias within limits (`diff_pair`).
- Series R at the driving pin (`series_termination`).
- Heat sources away from sensors; no linear regulator dropping large voltage near a sensor (`thermal_distance`).
- Every signal has a continuous reference plane; nothing crosses a split or void (review, `return_path`).
- Nothing in antenna, island or shell keepouts (DRC rule areas).
- No via in an SMD pad; same-net vias keep a mask web to the pad (`.kicad_dru` rules).
- Courtyards never overlap; pin 1 aligned across identical parts (DRC, review).
- Pours: right net per zone, thermal reliefs on hand-soldered pads, islands removed, clearance set per zone (review).
- GND stitching vias 2-3 mm apart along pours and at layer changes, outside keepouts (review).
- A deviation you cannot fix this revision: `known:` on the constraint with the fix for next time. Never loosen the limit to pass.

## Verification
- Gate: zero DRC errors, zero unhandled warnings. Clearance, short, hole, unconnected, via-in-pad and courtyard errors are never waived; waivers are cosmetic and have a reason.
- `calc/critical.py hardware/<board>`: every critical block OK or a `known` WARN.
- Annotated renders (no pours, nets named and coloured, mm grid): read each region yourself against the rules.
- 3D view: height conflicts, connector alignment, parts past the edge.
- Whole-board review by a fresh strong model, then the human review in KiCad.
- Fab package: zones filled before plotting (`--check-zones`), gerbers + drill, CPL rotations confirmed in the fab's placement preview.

## Pitfalls
| Symptom | Cause | Fix |
|---|---|---|
| Plane cut by signals | router used inner layers | restrict signal routing to outer layers |
| Via inside an SMD pad | router ordering | see `references/routing.md`; keep the DRC rule |
| Diff pair partial, snaked through vias | interleaved connector pads (USB-C A6/B6, A7/B7) | draw the pair in the generator, lock it, keep input copper |
| Fine-pitch part won't route | pad gap allows only straight exits | hand-draw escape stubs in the generator |
| Router results differ between runs | non-deterministic routing | restore the committed board after experiments |
| Zones missing in gerbers | zones not filled before plotting | fill zones, then plot |
| Part rotated wrong in fab preview | CPL rotation table is old | add a rotation correction per footprint; do not hand-edit the CPL |
| `pcbnew` import error | system Python | use KiCad's bundled Python (env script in `BOARD.md`) |
| pcbnew SWIG removed (KiCad 11) | SWIG deprecation | read boards with `calc/pcb.py`; write via IPC API or S-expressions |
| KiCad opens a stray project | intermediate saved as a project | keep intermediates in temp dirs; remove it from KiCad's recent list |
| KiCad "already open" | `~*.lck` from a killed instance | delete the lock when no KiCad runs |
| IPC API sees no KiCad | API needs the project manager running and `api.enable_server` | start KiCad (project manager), not the standalone PCB editor |
| Regenerated board: huge diff | new UUIDs each run | commit only real changes |
