---
name: kicad-hw-design
description: Use when designing or changing a PCB in hardware/ (notifier-v1), running the KiCad pipeline, adding or swapping parts, routing, fixing DRC/ERC failures, preparing a JLCPCB order (BOM/CPL/gerbers), or reviewing schematics/layouts with agents. Pitfalls table in pitfalls.md.
---

# KiCad hardware design (hardware/)

One KiCad project: `hardware/notifier-v1/notifier-v1.kicad_pro`. Everything in it is generated. Edits made in the KiCad GUI are overwritten by the next pipeline run: review in KiCad, then change the generators.

## Source of truth
- `hardware/gen/notifier_v1.py`: `PARTS` + `NETS` (schematic, via `gen/schkit.py`).
- `hardware/gen/layout_v1.py`: `PLACE`, outline, island/neck, planes, keepouts, hand-drawn stubs, silk. Writes `.kicad_pro` + `.kicad_dru`.
- `hardware/rules/`: `jlc-4layer.json` (fab floors), `krt-fab.txt` (same floors for the router, keep in sync), `notifier.kicad_dru`.
- `hardware/waivers.yaml`: only for cosmetic ERC/DRC items.

## Environment
- Once per checkout: `hardware/tools/setup_tools.sh` (routers into gitignored `hardware/.tools`; needs git, uv, curl, Java for the Freerouting JAR).
- `source scripts/kicad-env.sh` (kicad-cli on PATH, `$KICAD_PYTHON`, `$KICAD_SYMBOL_DIR`, `$KICAD_FOOTPRINT_DIR`).
- Pure-Python tools: `uv run --package desk-hardware python hardware/tools/X.py` (plain `uv run` does not see the package).
- Anything importing `pcbnew` (SWIG) runs only under `"$KICAD_PYTHON"`: `layout_v1.py`, `finish.py`, `board_geom.py`, `autoroute.py`, `pcb_from_netlist.py`.
- Keep exactly one `.kicad_pro` in the repo. Pipeline intermediates go to temp dirs (`route_krt.sh` does this); never leave a stray project or `~*.lck` next to a board.

## Workflow
1. Edit the generators (never the `.kicad_*` files).
2. `hardware/tools/pipeline_v1.sh [--bom]`: schematic + net-intent check -> netlist -> placement -> KRT routing -> `finish.py` pours/cleanup -> `checks.py` -> geometry JSON -> review pack -> annotated renders -> fab package. `build_v1.sh` alone does schematic + placement.
3. Gate: `scripts/hw-check.sh [--bom --qty N]` must print `hardware checks: OK` (also part of `scripts/check.sh`). Never waive clearance, short, hole, unconnected, via-in-pad or courtyard errors; fix the layout. It skips (exit 0) when kicad-cli is missing.
4. Review the annotated renders in `hardware/out/review/notifier-v1/` (`render_review.py`: no pours, nets named/coloured, mm grid). Add regions in the `pipeline_v1.sh` render call (`name=x0,y0,x1,y1`).
5. Visual checks and a fresh-context design review (see Agents), fix in the generators, back to 2.
6. `fab_package.py` output in `hardware/out/fab/notifier-v1/`, then the human steps in `hardware/notifier-v1/ORDER.md`.

`hardware/out/` is not in git. Router logs: `out/route/`. Open the board with `hardware/open.sh`.

## Adding or swapping a part
1. Stock check: `uv run --package desk-hardware python hardware/tools/jlcparts.py C123456` or `-s "keyword"`. Prefer Basic parts (each Extended part costs a setup fee). Check stock for the order quantity, not just >0.
2. Use a KiCad stock symbol and footprint (no custom libs). Compare the footprint's land pattern and pin 1 with the datasheet.
3. Add the entry to `PARTS` with an `LCSC` field (parts without LCSC are left out of BOM/CPL: holes, test pads, jumpers), then its `NETS` entries.
4. Add a position in `PLACE` in `layout_v1.py`. Respect keepouts (antenna strip, sensor island, neck).
5. Re-run the pipeline; the gate must be OK. Fine-pitch parts may need hand-drawn escape stubs.
6. Check the rotation applied in `out/fab/.../cpl.csv` against `ROTATION` in `fab_package.py` and add an entry if JLC's preview shows it wrong.
7. A module swap changes the firmware pin map: update `DESK_BOARD_*` Kconfig and `KCONFIG_NET`/`RESERVED` in `checks.py` (the gate compares schematic pins with Kconfig defaults).

## Design rules learned (notifier-v1)
- Thermal: no linear LDO from 5 V (about 0.6 W warms the sensor): use a buck, far from the sensor.
- BME688's gas heater warms itself: T/RH come from a separate SHT40, the BME688 does gas/pressure.
- Sensor on a peninsula: notch toward the module and buck, no pours on island and neck (all its nets leave as tracks through the neck).
- Antenna: keepout strip (no copper, no vias) under the module's antenna; the antenna overhangs the board edge.
- No vias in SMD pads (`.kicad_dru` rule "no via in SMD pad"): open vias wick solder.
- Inner layers: In1 GND, In2 3V3 stay solid; signals on F/B only.
- Cap placement: decoupling and buck input caps right at the pins (v1's buck input cap is about 3 mm from VIN; tighten next rev).

## Agents (see `hardware/agents/*.md`)
- Haiku: narrow single-question checks on the annotated renders (`visual_check.md`), part lookup, datasheet extraction, DRC triage. It cannot read KiCad's own plots (pour and pads same colour, tiny refs: everything "unclear"). The question must encode the exact rule (a too-strict antenna question gave a false fail), one image per question.
- Sonnet: real tasks: routing iteration, debugging, research.
- Strong model, fresh context, whole design: found the real problems (LDO heat, weak island isolation, vias in pads, plane cuts). Run it before the human review.
- Subagents may edit generators in the worktree; the orchestrator reviews the diff, re-runs the gate and commits.

## Human steps that remain
KiCad review of schematic and layout; display module pin order vs the bottom silkscreen (VCC/GND swap kills it); JLC placement preview for every IC (pin 1, see ORDER.md checklist); payment and ordering; bring-up (shorts check, current-limited power, flash, `/api/status` I2C scan).

## Pitfalls
Symptom -> cause -> fix tables are in `pitfalls.md` in this folder. Read it when a tool misbehaves.
