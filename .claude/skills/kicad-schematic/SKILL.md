---
name: kicad-schematic
description: Use when creating or changing a KiCad schematic, editing a schematic generator (parts, nets, no-connects), reviewing a schematic for readability, or fixing ERC and net-intent failures. Read hw-base first; parts and datasheets are hw-parts, layout is kicad-layout.
---

# KiCad schematic

Read `hw-base`, then the board's `BOARD.md` and `board.yaml` (generator paths, gate command).

## Source of truth
- The schematic is generated. Edit the generator (parts, nets, no-connects), never the `.kicad_sch`: GUI edits are overwritten. Review in KiCad, change the generator.
- The generator's net list is the intent. A net-intent check compares it with `kicad-cli sch export netlist` after every run (gate).
- Stock KiCad symbols and footprints only, unless `BOARD.md` names a project library.

## Readability (review)
- Hierarchy: a root block diagram plus one sheet per function (power, MCU, sensors, connectors).
- Signal flow left to right: power and inputs left, controller middle, outputs and connectors right. Rails up, GND down.
- Group by function. Decoupling caps beside the pins they serve.
- Wires for tightly coupled parts; labels only for buses and far nets.
- Explicit power symbols, no hidden power pins.
- Net names: descriptive, UPPERCASE, active-low ends in `_N`. Diff pairs end in their polarity (`_DP`/`_DN` or `_P`/`_N`): routers pair nets by suffix only.
- Connectors carry a function name (`PWR_IN`, `DEBUG_UART`), not only `J1`.
- IC pins grouped by function, not package order. Polarity and pin 1 visible.
- A short note on the sheet for every non-obvious constraint (thermal, pull level, strap).
- Title block on every sheet: project, revision, sheet n of m, date.

## Net names and the rest of the chain
- A net's full name includes its sheet path (`/Power/FB`). Power symbols and global labels give root names. Router scripts, net classes and `critical.yaml` use full names: rename everywhere together, then run the gate.
- Parts without a fab part number are kept out of the BOM (test points, holes, jumpers): mark them, do not delete the field silently.

## Verification
1. Generator run prints the net-intent result: fix the generator, not the output.
2. ERC through the gate. Waive only cosmetic items, each with a reason; never a pin conflict or an unconnected pin.
3. Export to PDF, convert to PNG, look at every page: no overlaps, no wire through a part, every power pin tied (review).
4. Every IC: application circuit against its datasheet card (hw-parts).

## Pitfalls
| Symptom | Cause | Fix |
|---|---|---|
| LED or diode reversed, ERC clean | kicad-sch-api 0.5.x rotates pins wrong at 90/270 deg | compute pin positions yourself; net-intent check |
| Labels left-justified and unreadable | kicad-sch-api writes all labels left-justified | right-justify labels at 180/270 deg on save |
| Net renamed after adding a sheet | sheet path is part of the name | global label or power symbol, or rename downstream |
| "global label only once" ERC | single global label per net | place it twice or use a power symbol |
| KiCad: "created by an older version" | sheets written in an old format | `kicad-cli sch upgrade --force` on every sheet, not only the root |
| Huge diff after regenerating | new UUIDs each run | restore after test runs; commit only real changes |
| Diff pair routed as two single nets | names lack the polarity suffix | rename to `_DP/_DN` or `_P/_N` |
| Stray `.kicad_sch` or `~*.lck` in the repo | GUI session or test run | delete it (locks only when no KiCad runs) |
