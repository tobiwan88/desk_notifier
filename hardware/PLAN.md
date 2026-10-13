# Hardware: AI-driven KiCad pipeline, board v1

Goal: a small board for the desk notifier, fully assembled at JLCPCB, designed by agents
(schematic, placement, routing, checks, fab files, order checklist) with human checkpoints.

**Status (2026-10-13):** notifier-v1 is generated, routed, checked and has a fab package.
Open before ordering: human review in KiCad, JLC placement preview (rotations), display pinout
check against the real module. See `notifier-v1/ORDER.md`.

## One command
```
hardware/tools/pipeline_v1.sh [--bom]   # schematic -> placement -> routing -> pours -> checks -> review pack -> fab package
scripts/hw-check.sh [--bom --qty N]      # the gate alone (also part of scripts/check.sh)
```

## Board v1 (notifier-v1), 50 x 36 mm, 4 layers (In1 GND, In2 3V3)
- **MCU**: ESP32-S3-WROOM-1-N8R2 (C2913204). The firmware needs >= 8 MB flash + PSRAM; C61 and C5
  modules had zero stock at JLC (all variants). N8R8 (C2913201) is the fallback (octal PSRAM:
  `CONFIG_SPIRAM_MODE_OCT=y`, GPIO35-37 are unused anyway). Antenna overhangs the top edge.
  Firmware: `DESK_BOARD_NOTIFIER_V1` (Kconfig choice, default on S3) sets the pin map;
  `sdkconfig.defaults.esp32s3`; build with `idf.py -B build-s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-s3/sdkconfig build`.
- **Power**: USB-C (HRO TYPE-C-31-M-12), 5.1k CC, USBLC6-2SC6 ESD, 500 mA polyfuse, TLV62569 buck
  (2 A, 3.33 V via 100k/22k) with a 2.2 uH Cenker CKCS4030. A linear regulator would burn ~0.6 W
  and warm the sensor; the design review caught it.
- **Sensor**: BME688 at 0x77 on a peninsula (1.2 mm notch facing the module and below, neck at the
  left edge), no pours on the island or neck, far from the module and the buck.
- **Display**: 7-pin GC9A01 module (VCC GND SCL SDA DC CS RST) on a right-angle 1x7 female socket
  (C22436148) whose face sits 0.4 mm past the right edge; pin names on the bottom silkscreen.
  JP1 selects LCD VCC: 3V3 (default, bridged) or 5V.
- **Pins** (S3): LCD SCLK 12, MOSI 11, DC 9, CS 10, RST 14 (FSPI pins); I2C SDA 4, SCL 5;
  button 6; BOOT 0; USB 19/20; UART0 43/44. Checked against Kconfig by `tools/checks.py`.
- **Debug**: pads TX RX GND 3V3 EN IO0 (ESP auto-program order) + VBUS; RESET/BOOT/user buttons.

## Toolchain (what is actually used)
| Step | Tool |
|---|---|
| Schematic | `gen/notifier_v1.py` (PARTS + NETS = source of truth) on kicad-sch-api via `gen/schkit.py` (own pin math, label justification, net-intent check against KiCad's netlist) |
| Placement | `gen/layout_v1.py` (pcbnew SWIG in KiCad's Python): outline, positions, planes, keepouts, rules, silk; writes `.kicad_pro` + `.kicad_dru` |
| Routing | KiCadRoutingTools 0.23 (`tools/route_krt.sh`): planes on In1/In2, USB diff pair, rest on F/B only, `--ordering inside_out`, `--same-net-pad-clearance`, fab floors pinned (`rules/krt-fab.txt`, `--no-fix-drc-settings`) |
| Routing alt | Freerouting 2.5 JAR (`tools/autoroute.py`, `tools/freerouting`): fine on simple boards, left 12 nets open here (planes, island) |
| Finish | `tools/finish.py`: outer GND pours, refill, DRC-driven removal of dangling router leftovers (undone if it opens a connection) |
| Checks | `tools/checks.py`: ERC + DRC + schematic parity (JSON), `waivers.yaml`, pin map vs Kconfig, reserved S3 pins, live JLC BOM (`tools/jlcparts.py`) |
| Review | `tools/review_pack.py` (schematic tiles, layer plots, 3D renders) + `tools/render_review.py` (annotated: no pours, nets named and coloured, mm grid) |
| Fab | `tools/fab_package.py`: gerbers + drill zip, JLC BOM (grouped by LCSC) and CPL with rotation corrections |

## Review loop and agent roles
1. Change the generator, run the pipeline. 2. Gate: `checks.py` must be OK (errors are never waivable
for clearance/short/hole/unconnected/courtyard). 3. Review pack. 4. Haiku agents, one image + one
question each (`agents/visual_check.md`), on the annotated renders; an Opus subagent with fresh context
reviews the whole design. 5. Opus decides fixes; repeat. 6. Human checkpoint.

| Role | Model | Template |
|---|---|---|
| Orchestrator / author / fix decisions | Opus | - |
| Part lookup, datasheet extraction | Haiku | `agents/part_lookup.md`, `agents/datasheet_extract.md` |
| Narrow visual checks | Haiku | `agents/visual_check.md` |
| ERC/DRC triage | Haiku | `agents/drc_triage.md` |
| Fab file check | Haiku | (BOM/CPL/gerber listing) |
| Holistic design review | Opus subagent, fresh context | - |
| Brainstorm when blocked | Sonnet | - |

## Lessons (2026-10-13)
- kicad-sch-api 0.5.x rotates pins the wrong way at 90/270 degrees (an LED came out reversed with
  ERC clean) and writes every label left-justified. `schkit` fixes both; keep the net-intent check.
- Native freerouting-cli 2.5.0 crashes in its router thread and hangs; the JAR has analytics on by
  default (wrapper disables them). kicad-cli 10 has no DSN export (pcbnew SWIG has).
- KiCadRoutingTools routes everything here in seconds, but: it rewrites the project's fab rules
  unless `--no-fix-drc-settings` + `--fab-overrides`; it uses inner layers for signals unless
  `--layers F.Cu B.Cu` (cut the GND plane); its default ordering put vias into EN pads despite
  `--same-net-pad-clearance` (our `.kicad_dru` rule caught it); it leaves dangling stubs.
- Haiku can't read KiCad's own plots (pour and pads same colour, tiny refs): all "unclear". On the
  annotated renders it measures distances and traces nets. The question must encode the exact
  rule (a too-strict antenna question produced a false "fail").
- The fresh-context Opus review found the real design issues (LDO heat into the sensor, weak
  island isolation, vias in pads, plane cuts); Haiku checks are for narrow confirmation.
- No board without a `.kicad_pro`: DRC silently falls back to KiCad defaults.
- KiCad's IPC API did not start from the standalone PCB Editor app (no socket); live editing is not
  part of the pipeline yet.

## Not done / next
- Human review of schematic and layout in KiCad; JLC placement preview; order (`notifier-v1/ORDER.md`).
- MCP server / kicad-happy spike and live IPC placement (blocked on the API socket, see above).
- 3D model for the HRO USB-C is missing in renders; STEP export for an enclosure.
- Placement is hand-written coordinates; try `py_placer` from KiCadRoutingTools on the next board.
- Buck input cap is ~3 mm from VIN; tighten on the next revision.
