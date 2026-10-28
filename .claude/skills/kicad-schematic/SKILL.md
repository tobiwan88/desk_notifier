---
name: kicad-schematic
description: Use when creating or changing a KiCad schematic, editing a schematic generator (parts, nets, no-connects), reviewing a schematic for readability, or fixing ERC and net-intent failures. Read hw-base first; parts and datasheets are hw-parts, layout and KiCad GUI problems are kicad-layout.
---

# KiCad schematic

Read `hw-base`, then the board's `BOARD.md` and `board.yaml`.

## Source of truth
- Edit the generator, never the `.kicad_sch`: GUI edits are overwritten. Review in KiCad, change the generator.
- Run the net-intent check after every generator run (the generator prints it; command in `BOARD.md`). Fix the generator, not the output.
- Stock KiCad symbols and footprints, unless `BOARD.md` names a project library (review).

## Readability (review)
- Hierarchy: a root block diagram plus one sheet per function (power, controller, sensors, connectors).
- Signal flow left to right: power and inputs left, controller middle, outputs and connectors right. Rails up, GND down.
- Group by function. Decoupling caps beside the pins they serve.
- Wires for tightly coupled parts; labels only for buses and far nets.
- Explicit power symbols, no hidden power pins.
- Net names: descriptive, UPPERCASE, active-low ends in `_N`.
- Diff pair names end in their polarity (`_DP`/`_DN` or `_P`/`_N`): routers pair nets by suffix only.
- Connectors carry a function name (`PWR_IN`, `DEBUG_UART`), not only `J1`.
- IC pins grouped by function, not package order. Polarity and pin 1 visible.
- A short note on the sheet for every non-obvious constraint (thermal, pull level, strap).
- Title block on every sheet: project, revision, sheet n of m, date.

## Net names and the rest of the chain
- A net's full name includes its sheet path (`/Power/FB`); power symbols and global labels give root names. Router scripts, net classes and `critical.yaml` use full names: rename everywhere together, then run the gate.
- Parts without a fab part number (test points, holes, jumpers): leave the number empty and keep them out of the BOM (review).

## Verification
1. Net-intent check prints OK.
2. ERC through the gate. Never waive an unconnected pin; waive a pin conflict only when the design intends it, with the datasheet reason (gate, review).
3. Export to PDF, convert to PNG, look at every page: no overlaps, no wire through a part, every power pin tied (review).
4. Every IC: application circuit against its datasheet card (hw-parts).

## Pitfalls
| Symptom | Cause | Fix |
|---|---|---|
| Diode or LED reversed, ERC clean | generator library pin math (rotation) | net-intent check; verify pin positions in the generator |
| Net-intent check fails after an edit | generator nets do not match the parts' pins | fix the generator nets; the KiCad netlist is the truth |
| ERC pin conflict or unconnected pin | wrong pin number in the generator | compare the symbol's pin numbers with the datasheet |
| Net renamed after adding a sheet | sheet path is part of the name | global label or power symbol, or rename downstream |
| "global label only once" ERC | single global label per net | place it twice or use a power symbol |
| KiCad: "created by an older version" | sheets written in an old format | `kicad-cli sch upgrade --force` on every sheet, not only the root |
| Huge diff after regenerating | new UUIDs each run | restore after test runs; commit only real changes |
| Diff pair routed as two single nets | names lack the polarity suffix | rename to `_DP/_DN` or `_P/_N` |
