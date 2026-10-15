---
name: hw-layout
description: Use for PCB placement and routing in hardware/ (notifier-v1): editing gen/layout_v1.py PLACE/keepouts/stubs, running tools/route_krt.sh or finish.py, fixing DRC/clearance/short/unconnected failures, reviewing a layout or its annotated renders, exporting the fab package (gerbers, drill, zones), and KiCad GUI problems (lock files, stray projects, hangs). Schematic: hw-schematic. Parts, datasheets, ordering: hw-bom.
---

# PCB layout (hardware/)

- Board: `hardware/notifier-v1/notifier-v1.kicad_pcb` (+ `.kicad_pro`), generated. Edit `hardware/gen/layout_v1.py` (placement, outline, planes, keepouts, stubs), never the KiCad files. Full map: `hardware/README.md`.
- Pipeline: `hardware/tools/pipeline_v1.sh [--bom]`. Order: placement (`build_v1.sh`) -> `route_krt.sh` (KiCadRoutingTools) -> `finish.py` (pours, cleanup) -> `checks.py` -> geometry JSON -> `review_pack.py` / `render_review.py` -> `fab_package.py`.
- Gate: `scripts/hw-check.sh` must print `hardware checks: OK` (also in `scripts/check.sh`). It exits 0 without kicad-cli, so do not read a silent pass as OK.
- Run `$KICAD_PYTHON` for anything importing pcbnew (`source scripts/kicad-env.sh`). Plain `uv run` does not see pcbnew.
- Keep one `.kicad_pro` in the repo. Intermediates go to temp dirs. Never run DRC on a bare board copy.

## Placement (phase 1)
- Order: fixed parts first (USB-C, buttons, display socket, mounting holes, module), then blocks: power (buck, polyfuse, ESD), digital core, sensor island, interfaces.
- Interdependent parts tight. Follow signal flow in the layout (inputs at the edge they come from).
- Decoupling and buck input caps right at the IC power pins. Route power through the cap pads, not stubs off a shared trace. Our v1 buck input cap sits about 3 mm from VIN: tighten next rev.
- Align pin 1 and polarity across identical parts (AOI, assembly). Verify each IC in JLC's placement preview (see `ROTATION` in `fab_package.py`).
- Courtyards must not overlap. Never place parts on the antenna strip, island or neck keepouts.
- Edge keepout: our floor is copper-to-edge 0.3 mm (`rules/jlc-4layer.json`, `board_edge` in `krt-fab.txt`). Generic "3 mm" advice is the default for new boards with panelisation/handling needs, not this board (verify with the JLC panel if the outline changes).
- Thermal: no linear LDO from 5 V (about 0.6 W warms the sensor): use a buck, placed far from the BME688/SHT40.
- Antenna (module): keepout strip with no copper or vias under the antenna (`ANTENNA_STRIP`, `vias=True`); the antenna overhangs the top edge (local y < -6.75).
- Sensor island (BME688 peninsula): notch on the module side and below, neck at the left edge (`NECK`, 3.6 mm wide; island 10.4 x 11.8 mm). No outer/In2 pours on island or neck; In1 GND hatched as return path. All island nets leave as tracks through the neck. SHT40 sits in the island corner, away from the BME688.
- Fine-pitch parts (SHT4x DFN, 0.3 mm pad gap): footprint keepout allows only straight exits. Hand-draw escape stubs in `layout_v1.py` (the router extends them).

## Constraints and classes
- Net classes (`layout_v1.py`): Default track 0.2 / clearance 0.15 / via 0.6/0.3. Power 0.4 mm. USB diff pair 0.27 / gap 0.15 (`USB_W`/`USB_GAP`): 90 ohm on JLC04161H-7628 L1 over In1 GND (check widths in the JLC impedance calculator, https://jlcpcb.com/impedance; KRT's `py_router/impedance.py` without mask gives ~96 ohm for 0.25/0.15, mask lowers ~3 ohm). Diff pairs stay on one outer layer next to a GND plane, never on B.Cu over the 3V3 plane. Generic "1.0 mm power traces" is the default for high-current new boards; here power tracks are 0.4 mm (route_krt uses 0.4 for VBUS_IN/VBUS/SW and 0.2 for +3V3/GND tracks; planes carry the rest).
- Fab floors must match in two files: `rules/jlc-4layer.json` (JLC capabilities) and `rules/krt-fab.txt` (router). Change both or neither. Current floors: clearance 0.127, track 0.127, via 0.45/0.2, hole-to-hole 0.5, edge 0.3.
- `rules/notifier.kicad_dru`: "no via in SMD pad" (open vias wick solder). Do not waive it.
- `layout_v1.py` writes the project and `.kicad_dru`. Edit rules there, not in the GUI.
- Pours: correct net per zone, thermal reliefs (spoke 0.3, gap 0.25: 4 spokes is the hand-solder option), clearance set per zone, islands removed.

## Routing (phase 3)
- Use `tools/route_krt.sh placed.kicad_pcb routed.kicad_pcb`. Its order and flags, and why:
  - Planes first (`route_planes.py`): GND on In1, +3V3 on In2, clearance 0.15. Inner layers stay solid.
  - USB pair: drawn and locked in `layout_v1.py` (USB-C D+/D- pads are interleaved A6/B6 vs A7/B7; route_diff left it partial, the general router then snaked it over B.Cu with ~8 vias and failed DRC 2 of 3 runs). D+ pads joined above the pad row, D- below; ESD USBLC6 rotated 270 for flow-through (4->3, 6->1), VBUS/GND middle pins leave by via. route.py skips complete nets; `--keep-input-copper` keeps it. KRT pairs nets only by suffix (`..._DP`/`_DN`, `_P`/`_N`): name series-R nets `MCU_USB_DP`, not `USB_DP_MCU`.
  - Rest (`route.py`) on `F.Cu B.Cu` only (`--layers`): signals never cross the inner planes.
  - `--ordering inside_out` (default order put vias into EN pads despite the clearance flag).
  - `--same-net-pad-clearance 0.1`: stops same-net vias in pads.
  - `--no-fix-drc-settings --fab-overrides rules/krt-fab.txt`: KRT otherwise rewrites the project's fab rules.
  - `--power-nets-widths` sets the power widths above.
- Freerouting (`tools/freerouting` JAR wrapper) is the fallback: the native CLI 2.5.0 hangs. Automatic neckdown is off in the wrapper. Left 12 nets open last time (verify on re-run).
- Ground reference rule (every signal): each signal needs a continuous GND reference, either a solid plane or an adjacent GND track, including across the island neck and slots. A signal that crosses a plane split or void is a fail: move it or add a GND track beside it. Sensor island/neck: In1 GND is a hatched zone (0.3 mm lines, 1.2 mm gap, ~35 % copper) under island and neck, added in `finish.py` (KRT's plane router drops zones made in the layout); outer and In2 pours stay forbidden there.
- Stitching vias: 2-3 mm apart between top and bottom GND pours, and at high-current layer transitions. Place them outside the antenna strip and island.
- After routing, run `finish.py`: removes dangling stubs and vias, DRC-driven, one at a time, reverting any that open a connection. Keep SWIG references (`keep` list) or the second iteration crashes.
- Hand-draw what the router keeps failing (short, critical, geometric): draw it in `layout_v1.py` with `trk(..., locked=True)` / `via()`, complete the net so `route.py` skips it, keep `--keep-input-copper`. Coordinates from `at(ref, pad)` after placement, not hard-coded; assert the orientation you rely on. USB pair made the pipeline pass 4/4 instead of ~1/3.
- Check a pair after routing: per-net length, layers and widths with `$KICAD_PYTHON` + `pcbnew.LoadBoard` (all `F.Cu`, `USB_W`, no vias; D+/D- skew here 1.8 mm = the connector join stub, fine at full speed).
- Non-determinism: router output differs run to run. After experiments run `git checkout hardware/notifier-v1/` to restore the committed board. Pipeline output is not byte-stable.

## Verification (phase 4)
- DRC: zero errors, zero unhandled warnings. Clearance, short, hole, unconnected, via-in-pad and courtyard errors are never waived (`hardware/waivers.yaml` is for cosmetic items only). Fix the layout.
- Annotated renders: `out/review/notifier-v1/` (no pours, nets named and coloured, mm grid). Add regions in the `render_review.py` call in `pipeline_v1.sh` as `name=x0,y0,x1,y1`. Read them yourself, check each region for the rules above.
- 3D viewer (KiCad GUI): height conflicts, connector alignment, display socket face 0.4 mm past the edge.
- Visual checks: Haiku only for one narrow question per image (see `hardware/agents/visual_check.md`). It cannot read KiCad's own plots (pour and pads same colour, tiny refs make everything "unclear"). Encode the exact rule in the question: a too-strict antenna question gave a false fail.
- Whole-board review: a fresh-context strong model (not Haiku), with the rules above. It found the real problems in v1 (LDO heat, weak island isolation, vias in pads, plane cuts). Run it before the human review.
- Fab package: `tools/fab_package.py` writes gerbers RS-274X + Excellon drill to `out/fab/notifier-v1/`. It runs `kicad-cli --check-zones` (zones must be filled before plotting) and subtracts soldermask. Check the CPL rotations against JLC's preview.
- Then the human steps in `hardware/notifier-v1/ORDER.md`.

## Pitfalls
| Symptom | Cause | Fix |
|---|---|---|
| Via inside an SMD pad | router ordering | `--ordering inside_out`, `--same-net-pad-clearance 0.1`, dru rule |
| GND/3V3 plane cut by signals | KRT routed inner layers | `--layers F.Cu B.Cu` for signals; planes via `route_planes.py` |
| KRT changed project fab rules | no `--no-fix-drc-settings` | rerun with `--no-fix-drc-settings --fab-overrides rules/krt-fab.txt` |
| Dangling stubs and vias after routing | KRT leftovers | `finish.py` (DRC-driven cleanup) |
| `finish.py` breaks on second iteration | SWIG frees board when last Python ref goes | keep references in a `keep` list |
| Diff pair partial, then snaked over B.Cu with vias | USB-C D+/D- pads interleaved (A6 B6 / A7 B7) | hand-draw: D+ joined above the pad row, D- below; ESD rotated for flow-through |
| ESD middle pins (VBUS/GND) trapped between D+ and D- | flow-through orientation | via straight off the pin (drawn with the pair) |
| Router results differ between runs | non-deterministic routing | `git checkout hardware/notifier-v1/` after experiments |
| Fine-pitch DFN (SHT4x) won't route | 0.3 mm pad gap, straight exits only | hand-drawn escape stubs in `layout_v1.py` |
| Island nets unconnected | no pours there, nets must go through the neck | allow tracks in the island keepout, route through the neck |
| Tracks thinner than netclass near fine pads | Freerouting neckdown | wrapper disables it; prefer KRT |
| Signal crossing a plane split or void | no ground reference | move the signal, or add a GND track beside it |
| Part rotated wrong in JLC preview | CPL correction missing | add to `ROTATION` in `fab_package.py` after checking JLC preview |
| Zone-filled gerbers show missing pours | zones not filled before plotting | `fab_package.py --check-zones` (fill first) |
| `pcbnew` import error | system Python | `$KICAD_PYTHON` after `source scripts/kicad-env.sh` |
| Router missing | fresh checkout | `hardware/tools/setup_tools.sh` (needs git, uv, curl, Java) |
| KiCad opens a stray intermediate project | `open_projects` in `~/Library/Preferences/kicad/10.0/kicad.json` | remove the entry; keep intermediates in temp dirs |
| KiCad "already open" dialog | `~*.lck` from a killed instance | delete the lock files when no KiCad runs |
| KiCad hangs at startup | unexplained (once, libgit2 repo discovery in `ReCreateTreePrj`; likely macOS privacy prompt) | `sample <pid> 5` and `lsof -p <pid>` (verify if it recurs) |
| IPC API tool sees no KiCad | API needs a running GUI (`api.enable_server` in `kicad_common.json`) | start KiCad.app (project manager); standalone PCB Editor creates a lock with no socket |
| pcbnew deprecation | SWIG due for removal in KiCad 11 | `layout_v1.py`, `finish.py`, `board_geom.py` move to the IPC API (verify KiCad 11 timing) |
| `uv run` can't import the hardware tools | root project only | `uv run --package desk-hardware` |
| Fresh host build: `ld: tapi error: malformed file ... libSystem.B.tbd` | macOS CLT SDK 27.0 vs linker | `SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk scripts/check.sh` |
| `brew install --cask temurin` fails | cask needs sudo | `brew install openjdk` |
| Regenerated board: huge diff | new UUIDs each run | restore after test runs; commit only real changes |
