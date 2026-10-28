---
name: hw-parts
description: Use when choosing, adding or swapping parts, checking a circuit against a datasheet or app note, writing datasheet cards, checking fab stock and price (JLCPCB/LCSC), setting BOM alternates, generating BOM/CPL, or preparing a PCB assembly order. Read hw-base first.
---

# Parts, datasheets, stock, ordering

Read `hw-base`, then the board's `BOARD.md` (decisions, order checklist) and `board.yaml` (alternates, power inputs).

## Choosing a part
- Start from the requirement: function, supply voltage and current, package, temperature, the datasheet's application circuit.
- Collect 2-3 fab-library candidates. Stock and price: `hardware/tools/jlcparts.py <LCSC>` or `-s "keywords" -n 20`.
- Prefer the fab's basic library: each extended part adds a setup fee.
- Stock covers boards x parts per board with margin (gate warns under 2x).
- Compare the footprint's land pattern and pin 1 with the datasheet drawing, not only the package name.
- Add the part to the generator with its fab part number. No number = not in BOM/CPL (holes, test pads, jumpers).
- A module or pin-compatible swap may change firmware config or pin maps: check and note it.

## Datasheet card (every IC and critical passive)
- One file `parts/<MPN>.yaml`: `source.url` (manufacturer site, not a distributor listing or third-party design), `source.revision`, and `values` with a `section` each. Format: see an existing card.
- `null` = not read yet or not stated. Never fill a value from memory or a reference design.
- Extraction: the helper prompt `hardware/agents/datasheet_extract.md` (pins, abs max, circuit, land pattern, page refs). Check each number against the PDF page yourself before it goes into the card.
- Check against the schematic (record the section that confirms each):
  - decoupling per supply pin: count, value, placement
  - enable/reset RC, pull-ups, strapping pins and their levels
  - regulator: feedback divider, input/output caps, inductor (`calc/buck.py`)
  - inductor saturation above peak current with margin; capacitor DC-bias derating at the operating voltage
  - USB-C sink: CC pull-downs; ESD array pinout vs data lines
  - I2C: addresses unique, pull-up sizing for bus load
  - thermal: exposed pad, dissipation, distance to sensors
  - layout notes: turn them into `critical.yaml` entries (kicad-layout)
- A `MISSING` row from a calc script means a card value is still `null`: read the datasheet and fill it.

## Stock and alternates
- `board.yaml` `alternates:` lists substitutes per fab part number, each with a `note` of what else must change.
- When a part runs short the gate names the first in-stock alternate. It never swaps silently: you change the generator and follow the note.
- Parts with unstable stock: re-check right before ordering; consider buying them into the fab's parts inventory ahead.
- Run the stock gate (`--bom --qty N`) right before ordering.

## Fab outputs
- BOM groups by (fab part number, footprint). CPL rotation = KiCad rotation, bottom side mirrored, plus a correction per footprint regex.
- Corrections come from an old community table: confirm every IC, connector and polarised part in the fab's placement preview. Add a correction for each wrong one; never hand-edit the CPL.
- Impedance: the stackup in `board.yaml` is part of the order. Compute with `calc/impedance.py`; cross-check with the fab's calculator (a human runs it) and write its inputs and result into the order notes.

## Before ordering
- Gate OK with `--bom --qty N`, on a clean, committed tree.
- Walk the board's order checklist (`BOARD.md` names it): PCB and assembly options, placement preview, human KiCad review, connector pinouts vs the mating module.

## Pitfalls
| Symptom | Cause | Fix |
|---|---|---|
| Stock check fails | part out of stock | alternate from `board.yaml`, follow its note |
| Stock drops between check and order | volatile part | re-check right before ordering; buy ahead |
| Unexpected cost | extended setup fees | basic-library equivalent |
| Part rotated wrong in preview | old rotation table | correction entry per footprint |
| Pin 1 or centre pad unclear | fab package name vs KiCad footprint differ | compare land patterns with the datasheet drawing |
| Part missing from BOM | no fab part number | add it, or it is hand-placed by design |
| Card value looks plausible but unsourced | filled from memory | set `null`, read the datasheet |
