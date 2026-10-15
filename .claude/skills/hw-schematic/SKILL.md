---
name: hw-schematic
description: Use when creating or changing the KiCad schematic in hardware/ (notifier-v1), editing the generators gen/notifier_v1.py (PARTS, NETS, NO_CONNECT) or gen/schkit.py, reviewing a schematic for readability, or triaging schematic ERC or net-intent failures.
---

# Schematic (hardware/notifier-v1)

The schematic is generated. Edit `hardware/gen/notifier_v1.py` (`PARTS`, `NETS`, `NO_CONNECT`) and `hardware/gen/schkit.py`, never the `.kicad_sch` file. KiCad GUI edits are overwritten by the next run: review in KiCad, change the generator.

## Where things are, how to run
- Generator: `hardware/gen/notifier_v1.py`; helpers `hardware/gen/schkit.py`; netlist export and placement chain in `hardware/tools/build_v1.sh` (schematic + net check, then `layout_v1.py`) and `hardware/tools/pipeline_v1.sh` (full chain).
- Run: `source scripts/kicad-env.sh`, then `uv run --package desk-hardware python hardware/gen/notifier_v1.py hardware/notifier-v1` (prints `nets OK` or the mismatches).
- Gate: `scripts/hw-check.sh` must print `hardware checks: OK` (ERC, waivers, pin map vs Kconfig).
- Everything else (layout, routing, fab): `hardware/README.md`.

## Readability rules
- Split into hierarchical sheets: a root block diagram plus Power, MCU, Sensors, Display (verify: the generator currently writes one A3 sheet, so hierarchy is a planned change).
- Signal flow left to right: inputs and power entry left, MCU middle, connectors and outputs right. Positive rails at the top, GND at the bottom.
- Group by function. Decoupling caps sit beside the IC power pins they serve.
- Draw full wires for tightly coupled connections; use labels only for buses and far nets.
- Power symbols explicit (3V3, 5V, GND). No hidden power pins.
- Net names descriptive and UPPERCASE: `3V3_MCU`, `SPI_MOSI`, `UART_TX`. Active-low names end in `_N` (`RESET_N`).
- Connectors with functional names (`PWR_IN`, `DEBUG_UART`), not only J1/J2.
- IC pins grouped by function, not package order. Polarity visible (diode, LED, polarised cap, pin 1).
- Short notes on the sheet for non-obvious constraints (keep short, thermal, pull-up choice).
- Title block on every sheet: project, revision, sheet n of m and title, designer, date.

## Generating with kicad-sch-api
- The library builds the file: `ksa.Schematic.create(...)`, `sch.components.add(lib_id, ref, value, pos, footprint, rotation=...)`, `sch.add_label(...)`, `set_title_block`. Symbols come from KiCad stock libraries only.
- Known bugs, handled in `schkit`:
  - kicad-sch-api 0.5.x rotates pins wrong at 90 and 270 degrees (LEDs and diodes come out reversed with ERC clean). `schkit.pin_pos` and `pin_away` do the pin math; do not trust the library's pin positions.
  - Labels are always written left-justified. `schkit.save` right-justifies labels at 180 and 270 degrees.
- Keep `check_nets` (net intent) in step with the netlist: it compares the generator's NETS against `kicad-cli sch export netlist`.
- Hierarchy: a net's full name changes with its sheet path (`/Power/SW` instead of `SW`). Nets referenced by the router, netclasses or checks must use global labels or power symbols, or be updated together with them (verify before adding sheets).
- Parts without an `LCSC` field are kept out of the BOM (`in_bom = False`): test points, holes, jumpers.

## Verification
1. `build_v1.sh` prints `nets OK` (net-intent check against the KiCad netlist). Fix the generator, not the output.
2. ERC through `scripts/hw-check.sh`. Waive only cosmetic items in `hardware/waivers.yaml`; never waive a real pin conflict or unconnected pin.
3. Export the schematic to PDF (`kicad-cli sch export pdf`), convert to PNG, and look at every page: no overlaps, no wires through parts, labels readable, every power pin tied.
4. Visual review by a strong model or yourself. Haiku only for narrow single questions with the exact rule spelled out, one image per question (it cannot read KiCad plots reliably; everything comes back "unclear").
5. BOM parts: check the datasheet application circuit; see the hw-bom skill.

## Pitfalls (schematic)
| Symptom | Cause | Fix |
|---|---|---|
| LED or diode reversed, ERC clean | kicad-sch-api 0.5.x pin rotation at 90/270 | use `schkit.pin_pos`/`pin_away`; net-intent check |
| Labels all left-justified, unreadable | library default | `schkit.save` justification |
| Net name changed after adding a sheet | full net name includes sheet path | global labels / power symbols, or update router and checks together (verify) |
| Huge schematic diff, every UUID changed | regeneration assigns new UUIDs | restore committed files after test runs; commit only real changes |
| `nets OK` fails after an edit | NETS does not match PARTS pins | fix NETS; the netlist is the truth |
| Pin conflict or unconnected pin in ERC | missing NETS entry or wrong pin number | check the symbol's pin numbers in the datasheet and the stock symbol |
| Stray `.kicad_sch` or lock file in repo | GUI session or test run | delete the stray file, `~*.lck` only when no KiCad runs |
