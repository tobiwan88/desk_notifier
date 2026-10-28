# notifier-v1: board notes

Board-specific facts for the board-neutral skills (`hw-base`, `kicad-schematic`, `kicad-layout`, `hw-parts`).
Numbers the scripts check live in `board.yaml`, `critical.yaml` and `parts/`. Design story and lessons: `../PLAN.md`.
Order checklist and bring-up: `ORDER.md`.

## Board
50 x 36 mm, 4 layers (In1 GND, In2 +3V3), JLC04161H-7628, JLCPCB PCBA. ESP32-S3-WROOM-1-N8R2, USB-C power/data,
TLV62569 buck to 3.3 V, BME688 on a sensor island, SHT40, 7-pin GC9A01 display header (J2, through-hole).

## Commands
- Environment: `source scripts/kicad-env.sh` (kicad-cli, `$KICAD_PYTHON` with pcbnew, symbol/footprint dirs). Anything importing pcbnew runs with `$KICAD_PYTHON`; plain `uv run` does not see it. Hardware tools: `uv run --package desk-hardware ...`.
- Schematic only: `uv run --package desk-hardware python hardware/gen/notifier_v1.py hardware/notifier-v1` (prints `nets OK` or the mismatches).
- Full chain: `hardware/tools/pipeline_v1.sh [--bom]`: schematic + net check and placement (`build_v1.sh`), routing (`route_krt.sh`), cleanup and pours (`finish.py`), gate, review renders, fab package.
- Order step: `hardware/tools/pipeline_v1.sh --fab-only --bom` (committed board as is, clean tree required).
- Gate: `scripts/hw-check.sh [--bom --qty N]` must print `hardware checks: OK`. Without kicad-cli it runs only the `hardware/calc` checks.
- Calc checks: `uv run --package desk-hardware python hardware/calc/{critical,buck,impedance}.py hardware/notifier-v1`.
- Routers: `hardware/tools/setup_tools.sh` (git, uv, curl, Java). KRT in `hardware/.tools/`.

## Generators
- Schematic: `gen/notifier_v1.py` (`PARTS` with LCSC field and datasheet comments, `NETS`, `NO_CONNECT`), helpers `gen/schkit.py` (works around the kicad-sch-api pin rotation and label bugs), sheets `gen/sheets_v1.py`.
- `build_v1.sh` runs `kicad-cli sch upgrade --force` on every sheet (schkit writes the KiCad 9 header, KiCad 10.0.x upgrades). After a generator change check `head -1 hardware/notifier-v1/*.kicad_sch`.
- Layout: `gen/layout_v1.py` (`PLACE`, outline, planes, keepouts, net classes, hand-drawn USB pair and escape stubs); it writes the project and `.kicad_dru`.
- Net names used by `route_krt.sh`, `NETCLASS_PATTERNS`, `finish.py`, `checks.py` and `critical.yaml`: root `GND`, `+3V3`, `VBUS`, `VBUS_IN`, `SW`; hierarchical `/USB_DP`, `/I2C_SDA`, `/LCD_*`, `/MCU/MCU_USB_DP`. Rename together.
- Pin map: module pins vs firmware Kconfig `DESK_BOARD_NOTIFIER_V1` and reserved S3 pins (strapping, USB, PSRAM) are checked by `tools/checks.py`.

## Decisions (why)
- MCU: C61 and C5 modules had zero JLC stock; ESP32-S3-WROOM-1 N8R2 (C2913204). Alternate N8R8 (C2913201) needs `CONFIG_SPIRAM_MODE_OCT=y` (in `board.yaml`).
- Buck (TLV62569) from 5 V, not an LDO: about 0.6 W in an LDO warmed the sensor.
- T/RH from SHT40 (0x44), not BME688 (0x77): the BME688 gas heater warms its own readings. BME688 does gas and pressure; its SDO is tied to 3V3 on purpose (waiver).
- Second 22 uF on the buck output: one 22 uF 0805 keeps about 11 uF at 3.3 V; datasheet sec 8.2.2.5 wants 10-47 uF effective.
- 22 ohm series R on USB D+/D- at the module (Espressif schematic checklist "USB"); 33 ohm on LCD SCLK at the module.
- Display supply JP1 defaults to 3V3 (cut and bridge 2-3 for 5 V).

## Layout specifics
- Sensor island: BME688 peninsula 10.4 x 11.8 mm, neck 3.6 mm at the left edge. No outer/In2 pours on island or neck; In1 GND as a hatched return zone (0.3 mm lines, 1.2 mm gap) added in `finish.py` (KRT's plane router drops zones made in the layout). Island nets leave as tracks through the neck. SHT40 in the island corner away from the BME688.
- Antenna: overhangs the top edge; `ANTENNA_STRIP` keepout with no copper or vias.
- USB pair: drawn and locked in `layout_v1.py`. USB-C D+/D- pads are interleaved (A6/B6, A7/B7): D+ joined above the pad row, D- below with a 1.4 mm B.Cu jumper (2 vias, fine at 12 Mbit/s). ESD USBLC6 rotated 270 for flow-through; its VBUS/GND middle pins leave by via. Nothing but GND under J1's metal shell (DRC rule area).
- SHT40 DFN (0.3 mm pad gap): hand-drawn escape stubs in `layout_v1.py`.
- Copper-to-edge floor is 0.3 mm (`rules/jlc-4layer.json`, `rules/krt-fab.txt`); generic 3 mm advice is for boards with panel handling needs, verify if the outline changes.
- Power tracks 0.4 mm on VBUS_IN/VBUS/SW (DRC min 0.3), +3V3/GND tracks 0.2 mm, planes carry the rest.
- Known deviations (WARN in `critical.yaml`): buck input cap C1 GND side, C10, C7, C8 further from their pins than wanted. Fix in the next revision.

## Fab and order
- Fab floors: clearance 0.127, track 0.127, via 0.45/0.2, hole-to-hole 0.5, edge 0.3 (two files, change both).
- `fab_package.py`: `ROTATION` corrections (USB-C +180, SOT-223 +180, SOT-23 -90, Bosch LGA +90, Sensirion DFN 270). J1 and U2 were wrong in JLC's preview once: check U1-U5, J1, D1, L1, F1, J2 in the preview.
- BME688 (C3664478) stock fluctuates around 200 and costs about $11.7: the usual order blocker. Re-check right before ordering; assemble 2 boards to save if needed.
- J2 is through-hole: Standard PCBA.
- Human steps: KiCad review; display module pin order vs bottom silkscreen (a VCC/GND swap kills it); JLC placement preview; payment.

## Pitfalls (this board and this host)
| Symptom | Cause | Fix |
|---|---|---|
| Router: open EN/GND/VBUS nets left of the module | crowded corner | `pipeline_v1.sh` retries (`KRT_TRIES`) until the gate passes |
| `uv run` can't import the hardware tools | root project only | `uv run --package desk-hardware` |
| macOS: `ld: tapi error: malformed file ... libSystem.B.tbd` | CLT SDK vs linker | `SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk scripts/check.sh` |
| `brew install --cask temurin` fails | cask needs sudo | `brew install openjdk` |
| KiCad hangs at startup (once) | likely macOS privacy prompt during repo discovery | `sample <pid> 5`, `lsof -p <pid>` if it recurs |
| KiCad opens a stray intermediate project | `open_projects` in KiCad's `kicad.json` | remove the entry |
| Freerouting left nets open | last run left 12 open | prefer KRT |
