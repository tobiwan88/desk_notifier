---
name: kicad-layout
description: Use for PCB placement and routing in KiCad: layout generators, keepouts, critical blocks (critical.yaml, calc/critical.py failures), diff-pair impedance and net-class widths, autorouting, DRC failures, layout review from renders, gerber/drill plotting, and KiCad GUI problems (locks, stray projects, hangs, IPC API). Read hw-base first; schematic is kicad-schematic, BOM/CPL and ordering are hw-parts.
---

# KiCad layout

Read `hw-base`, then the board's `BOARD.md`, `board.yaml` and `critical.yaml`.

## Source of truth
- Edit the layout generator (placement, outline, planes, keepouts, hand-drawn copper), not the `.kicad_pcb` (review).
- Fab floors live in `board.yaml` `fab.rules` and `fab.router_rules`: change both or neither (review).
- Custom DRC rules: edit the file at `board.yaml` `fab.custom_drc`; the generator copies it next to the board (gate).
- One `.kicad_pro` per board in the repo; intermediates go to temp dirs (review).
- Never run DRC on a bare board copy: without its project it uses default rules (review).

## Order of work
1. Fixed parts: connectors, buttons, mounting holes, modules, display sockets.
2. Keepouts and `thermal_distance`: they decide where parts may go.
3. Critical blocks, in this order: `switcher_loop`, `decoupling`, `feedback`, `series_termination`, `diff_pair`. Tight, in signal-flow order.
4. Remaining parts by block: power, digital core, sensors, interfaces. Inputs at the edge they enter from.
5. Planes on the `role: plane` layers of the stackup.
6. Critical copper drawn by the generator and locked: short, geometric, or what the router keeps failing. Coordinates from the placed pads, never hard-coded; assert the orientation you rely on.
7. Autoroute the rest on `role: signal` layers only. Router details: `references/routing.md`.
8. Remove router leftovers, fill zones, run the gate.

## Rules
- Every new critical block gets a `critical.yaml` entry before routing (review).
- Regulator input loop smallest, inductor at the switch node (`switcher_loop`).
- FB divider at the pin, short FB trace (`feedback`); FB away from the switch node and inductor (review).
- Decoupling cap at the pin (`decoupling`); power routed through the cap pads, not a stub off a shared trace (review).
- Diff pair: net-class width/gap from `calc/impedance.py` on the pair's `layer` (gate); skew and vias (`diff_pair`); on one layer next to a GND plane (review).
- A stackup change reruns `calc/impedance.py` and updates the diff-pair net-class widths (gate).
- Series R at the driving pin (`series_termination`).
- Heat sources away from sensors (`thermal_distance`); regulator dissipation near a sensor (review; script candidate).
- Every signal has a continuous return path, also across slots and narrow necks (review).
- Nothing in keepouts (DRC rule areas).
- No via in an SMD pad; same-net vias keep the copper clearance to SMD pads set in the custom DRC file (DRC).
- Courtyards never overlap (DRC); pin 1 aligned across identical parts (review).
- Pours: right net per zone, thermal reliefs from `board.yaml` `layout`, islands removed (review).
- GND stitching vias at `board.yaml` `layout.stitch_pitch_mm` along pours and at layer changes, outside keepouts (review).
- A deviation you cannot fix this revision: `known:` on the constraint with the fix for next time. Never loosen a limit to pass (review).

## Verification
- Gate: zero DRC errors and zero unhandled warnings. Items in the gate's never-waive list are never waived; other waivers are cosmetic and carry a reason.
- `calc/critical.py hardware/<board>`: every block OK or a `known` WARN (gate).
- Annotated renders (no pours, nets named and coloured, mm grid): read each region against the rules (review).
- 3D view: height conflicts, connector alignment, parts past the edge (review).
- Whole-board review by a fresh strong model, then the human review in KiCad (review).
- Gerbers and drill: zones filled before plotting (`kicad-cli --check-zones`).

## Pitfalls
| Symptom | Cause | Fix |
|---|---|---|
| Plane cut by signals | router used plane layers | route signals only on `role: signal` layers |
| Signal over a plane split or void | no return path | move it, or run a GND track beside it |
| Via inside an SMD pad | router ordering | see `references/routing.md`; keep the DRC rule |
| Diff pair partial, snaked through vias | interleaved connector pads (USB-C A6/B6, A7/B7) | draw the pair in the generator, lock it, keep input copper |
| Fine-pitch part won't route | pad gap allows only straight exits | hand-draw escape stubs in the generator |
| Router results differ between runs | non-deterministic routing | pin the seed; restore the committed board after experiments |
| Zones missing in gerbers | zones not filled before plotting | fill zones, then plot |
| `pcbnew` import error | system Python | KiCad's bundled Python (env command in `BOARD.md`) |
| pcbnew SWIG going away | deprecated, removal planned (verify the KiCad version) | read boards with `calc/pcb.py`; write via IPC API or S-expressions |
| KiCad opens a stray project | intermediate saved as a project | keep intermediates in temp dirs; remove it from KiCad's recent list |
| KiCad "already open" | `~*.lck` from a killed instance | delete the lock when no KiCad runs |
| IPC API sees no KiCad | API needs the project manager and `api.enable_server` | start KiCad's project manager, not the standalone PCB editor |
| Regenerated board: huge diff | new UUIDs each run | commit only real changes |
