---
name: hw-bom
description: Use when choosing, adding or swapping parts for hardware/ (notifier-v1), verifying a circuit against a datasheet or app note, checking JLCPCB/LCSC stock and price, generating BOM/CPL, or preparing a JLCPCB order.
---

# hw-bom: parts, datasheets, JLC stock, fab outputs

Scope: parts and ordering. Schematic: `hw-schematic`. Layout, routing, environment: `hw-layout`.

## 1. Part selection
- Start from the requirement: function, supply voltage and current, package, temperature, the datasheet's application circuit.
- Collect 2-3 LCSC candidates per requirement.
- Stock and price: `uv run --package desk-hardware python hardware/tools/jlcparts.py C123456`, or `... -s "keyword" -n 20` for a search. Read `library`, `stock`, `price`.
- Prefer Basic parts. Each Extended part costs a setup fee (v1: 9 of 21 BOM lines are Extended).
- Stock must cover the order quantity (boards x parts per board) with margin, not just be >0. Re-check before ordering.
- KiCad stock symbol and footprint only, no custom libraries.
- Compare the footprint's land pattern and pin 1 with the datasheet, not only the package name. Example: SHT40 is "DFN-4-EP" at JLC, the KiCad footprint has no centre pad (verify which the board needs).
- Add the part to `PARTS` in `hardware/gen/notifier_v1.py` with an `LCSC` field, then its `NETS`. No `LCSC` = left out of BOM/CPL (holes, test pads, jumpers).
- Add its position to `PLACE` in `hardware/gen/layout_v1.py`; respect keepouts.
- A module swap also changes the firmware pin map (`DESK_BOARD_*` Kconfig, `checks.py`).

## 2. Datasheet rule (every IC, user guidance D)
- Take the typical application circuit, recommended values and layout guidelines from the OFFICIAL datasheet or app note (manufacturer site). Not from JLC listings or third-party reference designs.
- Record the source URL and section (e.g. "sec 8.2") as a comment next to the part in `PARTS`.
- Extraction: Haiku with `hardware/agents/datasheet_extract.md` (pins, abs max, circuit, land pattern, page refs). The orchestrator decides. Check each extracted number against the PDF page yourself.
- Check at least:
  - decoupling per supply pin: count, value, placed at the pin
  - EN/RESET RC and pull-ups
  - strapping pins (boot mode, flash/PSRAM voltage) and their pull levels
  - feedback divider and Vref: compute the output with the datasheet formula
  - inductor saturation current above peak current with margin; capacitor DC-bias derating at the operating voltage (verify)
  - USB-C: CC1/CC2 5.1 k pull-downs for a sink (verify against the connector datasheet); ESD array pinout vs D+/D-
  - I2C: addresses (SHT40 0x44, BME688 0x77 on this board), pull-up sizing for bus load
  - thermal: exposed pad, dissipation, heat distance to sensors
  - layout notes: keepouts, short loops, GND under the part
- If a datasheet is silent, write "not stated". Do not invent a value.

## 3. Learned choices (why)
- MCU: C61 and C5 modules had zero JLC stock. Swapped to ESP32-S3-WROOM-1 N8R2 (C2913204, about 1.7k). Fallback N8R8 (C2913201) needs `CONFIG_SPIRAM_MODE_OCT=y` (octal PSRAM).
- Buck (TLV62569) from 5 V, not a linear LDO: about 0.6 W in an LDO warmed the sensor.
- T/RH from SHT40, not BME688: the BME688 gas heater warms its own readings. BME688 does gas and pressure.
- BME688 (C3664478) stock fluctuates (about 200): the usual order blocker.
- J2 header (C22436148, about 680) is through-hole: Standard PCBA, hand/wave soldered.

## 4. Fab outputs
- `hardware/tools/pipeline_v1.sh --bom` writes `hardware/out/fab/notifier-v1/`: gerbers zip, BOM CSV, CPL CSV.
- `fab_package.py` groups the BOM by (LCSC, footprint). CPL rotation = KiCad position, bottom side as 180 - rot, plus `ROTATION` (regex on footprint name: USB-C +180, SOT-223 +180, SOT-23 -90).
- `ROTATION` comes from an old community table. Verify every IC in JLC's placement preview (ORDER.md checklist). For each wrong part add a `ROTATION` entry and note it. Do not hand-edit the CPL.
- Parts without an LCSC field are excluded from BOM and CPL.

## 5. Before ordering
- `scripts/hw-check.sh --bom --qty N` must print `hardware checks: OK` (fails on missing stock). Run it right before ordering.
- Walk the `hardware/notifier-v1/ORDER.md` checklist: PCB and assembly options, placement preview of U1-U5, J1, D1, L1, F1, J2.
- Human steps: KiCad review; display module pin order vs bottom silkscreen (a VCC/GND swap kills it; JP1 bridged to 3V3); JLC payment and order; JLC placement preview; upload gerbers, BOM, CPL.
- Quantity: assemble all boards or 2 to save on the BME688 (about $11.7 each).

## 6. Pitfalls (JLC/fab)
| Symptom | Cause | Fix |
|---|---|---|
| `--bom` fails on stock | part or module out of stock | in-stock equivalent (section 3) |
| Part rotated wrong in preview | old rotation table | fix in JLC editor, add `ROTATION` entry |
| Unexpected cost | Extended setup fee | Basic equivalent |
| Stock drops between check and order | BME688 fluctuates | rerun the check right before ordering |
| SHT40 pin 1 unclear | JLC "DFN-4-EP" vs KiCad footprint | compare land pattern with datasheet, check preview |
| Part missing from BOM | no `LCSC` field | add LCSC, or it is hand-placed by design |
