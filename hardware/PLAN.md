# Hardware: AI-driven KiCad pipeline, board v1

Goal: a small 2-layer board for the desk notifier, fully assembled at JLCPCB, designed by agents
(schematic, placement, routing, checks, fab files, order checklist) with human checkpoints.

## Board v1 (notifier-v1)
- **MCU**: ESP32-S3-WROOM-1-N8R2 (LCSC C2913204). Firmware needs >= 8 MB flash and PSRAM
  (1.5 MB app in 2 MB OTA slots, littlefs storage). The C61 module has no LCSC listing, the
  C5 PSRAM variants had no stock. Switch back to C61/C5 if one is stocked at JLC before ordering.
  Firmware port: `idf.py set-target esp32s3`, new pin map behind a Kconfig board option.
- **Power**: USB-C only (HRO TYPE-C-31-M-12, C165948), 5.1k on CC1/CC2, USBLC6-2SC6 ESD (C7519),
  3.3 V LDO AP2112K-3.3 (C51118, 600 mA; check S3 Wi-Fi peak margin in phase 1).
- **Sensor**: BME688 (C3664478), thermally isolated (slots + distance from module and LDO).
  Stock fluctuates: live stock check right before ordering.
- **Display**: 7-pin GC9A01 module (AliExpress: VCC GND SCL SDA DC CS RST, no BL), bought
  separately, plugs into a right-angle 2.54 mm female header (1x8 with one empty position unless
  a 1x7 is found at JLC). VCC from VBUS 5 V if the module has its own regulator.
- **Pins**: display off strapping/JTAG pins; USB on the S3 native USB pins; boot + reset buttons,
  user button. Pin map lives in Kconfig (`DESK_BOARD_NOTIFIER_V1`), breadboard defaults stay.
- **Board**: small rectangle, module at the edge, nothing under the antenna.

## Toolchain
| Step | Tool |
|---|---|
| Schematic | Python generator writing `.kicad_sch` (KiCad 10 IPC has no schematic API) |
| Placement | KiCad IPC API (`kicad-python`) live in the PCB editor; MCP server picked in a spike |
| Routing | Freerouting 2.5 JAR (`hardware/tools/freerouting`, Java 25+), headless: `pcbnew.ExportSpecctraDSN` -> freerouting `-de/-do` -> `pcbnew.ImportSpecctraSES` (kicad-cli 10 has no DSN export); `hardware/tools/autoroute.py` |
| Routing candidate | KiCadRoutingTools (drandyhaas, MIT, Rust A* core): edits `.kicad_pcb` directly, per-net `--nets`, diff pairs, `--json-out`; bake-off against Freerouting on the real board |
| Routing fallback | DeepPCB (cloud, paid, GUI plugin) for a one-off comparison only |
| Checks / outputs | `kicad-cli`: ERC/DRC JSON + `--exit-code-violations`, `--schematic-parity`, gerbers, drill, pos, BOM, PDF/SVG, render, STEP |
| Parts | `easyeda2kicad`, `kicad-jlcpcb-tools` |

Critical nets (USB pair, power, GND pour, antenna area) are placed/routed by rule; Freerouting
routes the rest. Gerbers only from a board with zero DRC errors and zero unrouted nets.

## Review loop
1. Change the design (generator / IPC).
2. Hard checks (`scripts/hw-check.sh`, called from `check.sh`):
   ERC + DRC (JSON), JLC 2-layer `.kicad_dru`, pin map vs Kconfig (flags strapping/USB/PSRAM
   pins), Espressif checklist as code (EN RC 10k + 1 uF, GPIO strapping pulls, decoupling distance,
   antenna keepout, CC resistors), BME688 distance to hot parts, BOM (live stock, Basic/Extended,
   package vs footprint).
3. Review pack (`hardware/tools/review_pack.py`): schematic PDF + per-sheet PNG, per-part crops,
   per-layer PCB images, 3D renders (top/bottom/iso), overlays (antenna keepout, BME688 radius,
   ratsnest, USB/power nets), region crops, diff image against the last revision.
4. Haiku agents in parallel on narrow tasks (below); Opus reviewer with fresh context on the whole pack.
5. Opus triages and fixes; repeat (max N iterations, never more findings than the last pass).
   Accepted violations go to `hardware/waivers.yaml` with a reason, never silent KiCad exclusions.
6. Human checkpoint on the same PDF/images.

## Agent roles
| Role | Model | Task |
|---|---|---|
| Orchestrator / author | Opus | architecture, schematic, placement, critical routing, fix decisions, final review |
| Part lookup | Haiku | one part: LCSC stock, price, Basic/Extended, datasheet URL |
| Datasheet extraction | Haiku | one datasheet -> JSON: pins, abs max, reference circuit, land pattern, page refs |
| Footprint check | Haiku | one part: footprint image vs land-pattern JSON, pin 1 |
| ERC/DRC triage | Haiku | violation batch -> auto-fix / judgment / waiver candidate + reason |
| Narrow visual checks | Haiku | one image + one question (antenna keepout clear? decoupling next to pin? silk overlaps?) |
| Holistic review | Opus subagent | whole review pack + datasheet JSONs, fresh context |
| Fab check | Haiku | gerber/drill images, BOM + CPL vs JLC rules, rotations |

Haiku prompts live in `hardware/agents/` as templates with a fixed JSON output schema. Haiku never
edits the design; findings are confirmed by a script or by Opus before a fix.

## Phases
0. **Setup**: Java 25 + Freerouting, KiCad IPC API on, `kicad-python`; toy board through the full
   chain (generate schematic, ERC, place, route, DRC, review pack, fab outputs); MCP + kicad-happy
   spike; real ERC/DRC JSON samples; JLC 2-layer `.kicad_dru`; Haiku prompt templates.
1. **Parts and pins**: confirm module, S3 USB pins, header, LDO; BOM check; Kconfig board option,
   S3 firmware build.
2. **Schematic** + loop. Checkpoint: human review in KiCad.
3. **Layout**: placement, critical nets, Freerouting, loop. Checkpoint: renders.
4. **Fab package**: gerbers, drill, JLC BOM/CPL; JLC online DFM.
5. **Order checklist**: human orders. Bring-up via `/api/status`.

## Layout
```
hardware/
  PLAN.md
  notifier-v1/   KiCad project (sch generated, pcb edited)
  lib/           project symbols/footprints/3D (LCSC-sourced, pinned)
  gen/           schematic generator
  tools/         checks, review pack, routing, fab package
  agents/        Haiku prompt templates
  rules/         JLC .kicad_dru
  waivers.yaml
  out/           generated outputs (gitignored)
```

## Phase 0 findings (2026-10-13)
- Toy chain works end to end: `spike/gen_toy.py` (schematic + net check) -> netlist ->
  `tools/pcb_from_netlist.py` -> `tools/autoroute.py` -> DRC with `--schematic-parity`.
- kicad-sch-api 0.5.x rotates pins the wrong way at 90/270 degrees (an LED came out reversed,
  ERC clean). `gen/schkit.py` computes pin positions itself and `check_nets()` compares the
  intended nets with KiCad's netlist after every generation. Keep that gate.
- freerouting-cli 2.5.0 (native) crashes in the router thread and hangs; use the JAR.
  The JAR has analytics on by default (BigQuery); the wrapper disables them.
- Freerouting needs the real JLC rules: with 0.2/0.2 mm defaults it necks VBUS down to 0.15 mm
  between SOT-23-5 pins. Next: JLC 2-layer `.kicad_dru` and netclasses (power width).
- Grid placement routes but is poor (decoupling far from pins): placement is the hard part.
- `kicad-cli` prints Homebrew fontconfig warnings on stderr; filter them in tooling.
- The review images (schematic PDF, 3D render PNG) are readable directly by the model;
  the full page is too small, the review pack must crop regions.

## Environment notes
- kicad-cli: `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli` (KiCad 10.0.7); `scripts/kicad-env.sh` puts it on PATH.
- `pcbnew` SWIG only in KiCad's bundled Python 3.9 (`.../Frameworks/Python.framework/Versions/Current/bin/python3`).
- IPC API: `kicad_common.json` -> `api.enable_server: true`; needs the PCB editor running.
