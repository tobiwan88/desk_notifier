# Hardware: AI-driven KiCad pipeline, board v1

Goal: a small board for the desk notifier, fully assembled at JLCPCB, designed by agents
(schematic, placement, routing, checks, fab files, order checklist) with human checkpoints.

**Status (2026-10-16, after the review fixes):** notifier-v1 is generated, routed, checked and has a fab package.
Open before ordering: human review in KiCad, JLC placement preview (rotations; J1 and U2 were rotated in the
2026-10-16 rework), display pinout check against the real module (now mirrored, see Display). See `notifier-v1/ORDER.md`.

See `README.md` for where to look during a review.

## One command
```
hardware/tools/setup_tools.sh             # once: routers into hardware/.tools (KRT 0.23.0, Freerouting 2.5.0 JAR)
hardware/tools/pipeline_v1.sh [--bom]   # schematic -> placement -> routing -> pours -> checks -> review pack -> fab package
hardware/tools/pipeline_v1.sh --fab-only [--bom]  # ORDER step: gate + fab package on the committed board, no regeneration/routing
scripts/hw-check.sh [--bom --qty N]      # the gate alone (also part of scripts/check.sh)
```

## Board v1 (notifier-v1), 50 x 36 mm, 4 layers (In1 GND, In2 3V3)
- **MCU**: ESP32-S3-WROOM-1-N8R2 (C2913204). The firmware needs >= 8 MB flash + PSRAM; C61 and C5
  modules had zero stock at JLC (all variants). N8R8 (C2913201) is the fallback (octal PSRAM:
  `CONFIG_SPIRAM_MODE_OCT=y`, GPIO35-37 are unused anyway). Antenna overhangs the top edge.
  Firmware: `DESK_BOARD_NOTIFIER_V1` (Kconfig choice, default on S3) sets the pin map;
  `sdkconfig.defaults.esp32s3`; build with `idf.py -B build-s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-s3/sdkconfig build`.
  Silkscreen: the board carries no reference designators (no space; only user labels, pin names and the order number). Refs live on F.Fab/B.Fab; the review pack's `layer_top.png` plots `F.Cu,F.SilkS,F.Fab,Edge.Cuts`, so the small refs in it come from F.Fab. The assembly drawing for debugging is the review pack or the KiCad file. The title is `v1 ${GIT_SHA}`; the project text variable is the worst case `0000000-dirty` so DRC (`silk_overlap`, `silk_over_copper`) gates the widest title (16 chars, ends at x about 28.8, JP1 `5V` starts at 33.0); `fab_package.py` plots with the real SHA. The review pack therefore shows `0000000-dirty`.
  Datasheet fixes: 22 ohm series R11/R12 (C25092) on USB D+/D- at the module (nets `MCU_USB_DP`/`MCU_USB_DN`), 1 uF C10 at 3V3, second 22 uF C11 on the buck output (Espressif checklist, TLV62569 sec 8.2.2.5).
- **Enclosure orientation**: the board lies flat. The round GC9A01 plugs into the right-angle socket J2 on the RIGHT edge
  (front of the case). The LEFT edge (x = 0) is the back of the case and its only opening: USB-C J1 (receptacle
  opening out of the left edge, centre y 24, front (fab outline) at x = -0.25, 0.25 mm past the edge) next to the sensor vent.
  The module antenna overhangs the top edge. See "Enclosure" below for all positions.
- **Enclosure** (mm from the board's top-left corner, x right, y down; measured on the fab layer, not the courtyard):
  - Outline 50 x 36 mm, 1.6 mm FR-4, sensor slot cut into the top-left (island x 0..10.4, y 0..14.8, slot 1.2/1.8 mm).
  - M2 mounting holes (2.2 mm, unplated), centres (3, 3) H3, (47, 3) H4, (3, 33) H2, (47, 33) H1; each has a 2.7 mm
    copper keepout radius, a M2 screw head is about 3.8 mm in diameter (silkscreen must stay clear of it).
  - USB-C J1: opening out of the LEFT edge, centre line y = 24.0 (shell fab outline y 19.48..28.52, 9.04 mm wide), connector front
    at x = -0.25 (0.25 mm past the edge), metal shell body to x = 7.1. This is the back of the case: **the back wall in front of
    J1 should be thin (<= 1.2 mm) or recessed** so bulky plugs seat (a USB-C plug overmould is wider than the receptacle opening).
  - Display socket J2: on the RIGHT edge, pin column at x = 40.0, socket face (fab outline) at x = 50.03 (0.03 mm past the
    edge), body y 9.4..27.3, pins at y 10.76..26.0 (2.54 pitch, pin 1 at the bottom). The display plugs in from the right.
  - Antenna: the module (x 16..34) has its antenna end at y -6.0..0, overhanging the TOP edge by 6.0 mm. Keep metal, batteries
    and the case's conductive parts away from it; no rail, V-cut or screw there.
  - Debug pads TP1..TP6 at y = 6.8, x 36.2..48.9 (2.54 pitch), TP7 (VBUS) at (24.5, 27.0): reachable with tweezers; leave an opening
    or a service hole in the case if they are needed after assembly.
- **Mounting**: four M2 holes (unplated, MountingHole_2.2mm_M2, H1..H4), one per corner, centre 3 mm from both edges,
  each with a 2.7 mm radius keepout on all copper layers (no tracks, vias or pours). H3 sits in the sensor island's
  top-left corner: the screw braces the thin neck mechanically.
- **Power**: USB-C (HRO TYPE-C-31-M-12), 5.1k CC, USBLC6-2SC6 ESD, 500 mA polyfuse, TLV62569 buck
  (2 A, 3.33 V via 100k/22k) with a 2.2 uH Cenker CKCS4030. ESD U2, CC resistors R1/R2 and polyfuse F1 sit next to
  J1 on the left; VBUS runs from F1 along the bottom to the buck (bottom middle). A linear regulator would burn ~0.6 W
  and warm the sensor; the design review caught it.
- **USB pair**: hand-drawn in `gen/layout_v1.py` for the left-edge connector: D+ pads joined on the interior side, D-
  pads on the outer side, straight through U2 (flow-through), D- straight up to R11, D+ around the U2 GND via,
  coupled to D- for only about 0.5 mm (0.27 mm tracks / 0.15 mm gap are sized for about 91 ohm differential with mask on JLC04161H-7628, prepreg 0.2104 mm, field solver `calc/impedance.py`; elsewhere the tracks are about 2 mm apart; measured minimum same-layer copper gap I2C_SCL / I2C_SDA to USB_DN is 1.65 mm (pcbnew, 2026-10-16); no impedance control ordered; a short (about 13 mm) full-speed pair, coupling is not critical, see ORDER.md) before it hooks to R12. Nothing but GND runs under J1's metal shell (x < 7.1; DRC rule `no signal copper under the J1 shell`). The 0.5 mm pad pitch (B7 D-, A6 D+, A7 D-, B6 D+) forbids two planar joins, so the D- A7/B7 join takes two 0.45/0.2 vias right of the pads and a 1.4 mm B.Cu jumper (over In2/In1, harmless at 12 Mbit/s); the D+ A6/B6 join is an F.Cu loop of 0.2 mm stubs around the A7 via. J1's VBUS_IN pads (top and bottom) go through vias just right of the pads and join on B.Cu to the ESD pin.
- **Sensor**: BME688 at 0x77 on a peninsula: milled slot 1.8 mm on the module side (TI SNOAA01: a wider gap reads
  closer to ambient), 1.2 mm below, neck 3.6 x 1.2 mm at the left edge, far from the module and the buck. No copper
  pour or plane under the island or the neck on any layer (Bosch BME68x HSMI 7.3, Sensirion SHTxx design guide,
  TI HDC302x: thin interconnects, no copper under the sensor). The island is routed by hand in `gen/layout_v1.py`
  (3V3/I2C on F.Cu, GND as a B.Cu tree), so it is identical on every run. The island is 10.4 x 14.8 mm (grown 3 mm to make
  room for the M2 hole in its top-left corner): BME688 top-right, SHT40 bottom-left, 8.9 mm apart; 3V3 climbs the left
  edge and passes under the hole keepout; SCL/SDA drop from the BME688 to two horizontals along the island bottom
  (SDA reaches the SHT40 over a B.Cu jumper); C12 4.7 uF bulk sits on the 3V3 line next to the bus.
  The neck carries one F.Cu bus at 0.15 mm (`BUS_X`): 3V3 | GND | SCL | SDA | GND. The two GND guards are the
  return path for SDA/SCL (at 400 kHz a guard is enough); they are joined on B.Cu inside the neck and stitched to
  In1 just below it. A B.Cu keepout band keeps other tracks out of the neck. The bus starts at island pads and ends
  in 0.3 mm pieces on the main board: the router extends free ends and reroutes stubs that touch no pad, and
  `finish.py` drops the unused pieces. Replaced an earlier hatched In1 zone under the whole island.
- **Display SPI**: 33 ohm R13 in series with SCLK at module pin 20 (net `MCU_LCD_SCLK` -> `LCD_SCLK`).
  `finish.py` adds a GND via beside each SPI layer-change via where there is room: on B.Cu the lines reference the
  3V3 plane, on F.Cu the GND plane.
- **Temperature/humidity**: SHT40-AD1B (C2909890, 0x44, ±1.8 %RH / ±0.2 °C) on the same island, in the
  corner away from the BME688: its gas heater warms the BME688, so T/RH come from the SHT40 and the
  BME688 is used for gas/air quality (and pressure).
- **Display**: 7-pin GC9A01 module on a right-angle 1x7 female socket, pinout mirrored for the right-edge socket:
  J2 pin 1 LCD_RST, 2 LCD_CS, 3 LCD_DC, 4 LCD_MOSI (SDA), 5 LCD_SCLK (SCL), 6 GND, 7 LCD_VCC (the module's own
  header order is VCC GND SCL SDA DC CS RST; it plugs in rotated), the socket
  (C22436148) whose face (fab outline) sits at x = 50.03, 0.03 mm past the right edge (moved 0.2 mm right so the module seats at the
  edge); pin names on the bottom silkscreen.
  JP1 selects LCD VCC: 3V3 (default, bridged) or 5V.
- **Pins** (S3): LCD SCLK 12, MOSI 11, DC 9, CS 10, RST 14 (FSPI pins); I2C SDA 4, SCL 5;
  BOOT 0; USB 19/20; UART0 43/44. GPIO6 is unused: the notifier-v1 board has no user button (`DESK_BTN` defaults to -1 = none on this board; the
  firmware skips the button when it is negative); ack / next page go through the web UI / API (`POST /api/ack`, `/api/page`). Checked against Kconfig by `tools/checks.py`.
- **Debug**: no buttons. Six 1.5 mm round pads at 2.54 mm pitch, top right, in the order TX RX 3V3 EN GND IO0 (GND
  between EN and IO0): tweezers across EN-GND = reset, across GND-IO0 = boot mode; the ESP auto-program order is not
  needed with the USB-Serial/JTAG port. VBUS pad TP7 stays. (Refs: TP1 TX, TP2 RX, TP4 3V3, TP5 EN, TP3 GND, TP6 IO0.)

## Toolchain (what is actually used)
| Step | Tool |
|---|---|
| Schematic | `gen/notifier_v1.py` (PARTS + NETS = source of truth) on kicad-sch-api via `gen/schkit.py` (own pin math, label justification, net-intent check against KiCad's netlist) |
| Placement | `gen/layout_v1.py` (pcbnew SWIG in KiCad's Python): outline, positions, planes, keepouts, rules, silk; writes `.kicad_pro` + `.kicad_dru` |
| Routing | KiCadRoutingTools 0.23 (`tools/route_krt.sh`): planes on In1/In2, rest on F/B only (USB pair hand-drawn in `gen/layout_v1.py`, 0.27/0.15, about 90 ohm differential), `--ordering inside_out`, `--same-net-pad-clearance`, fab floors pinned (`rules/krt-fab.txt`, `--no-fix-drc-settings`) |
| Routing alt | Freerouting 2.5 JAR (`tools/autoroute.py`, `tools/freerouting`): fine on simple boards, left 12 nets open here (planes, island) |
| Finish | `tools/finish.py`: outer GND pours, refill, DRC-driven removal of dangling router leftovers (undone if it opens a connection) |
| Checks | `tools/checks.py`: board-neutral `calc/` scripts on the board profile (`critical.py` layout constraints from `notifier-v1/critical.yaml`, `buck.py`, `impedance.py`), ERC + DRC + schematic parity (kicad-cli JSON), `waivers.yaml`, pin map vs Kconfig, reserved S3 pins, live JLC BOM (`tools/jlcparts.py`). DRC rules: project floors from `rules/jlc-4layer.json` (JLC 4-layer capabilities with margin) plus `rules/notifier.kicad_dru`: no via in SMD pad (0.1 mm physical clearance), via to same-net SMD pad >= 0.15 mm (`physical_clearance` also sees same-net copper; hand-placed vias keep >= 0.2, `finish.py` nudges router vias toward 0.2, the crowded corner left of the module only allows 0.15), via to edge >= 0.35, PTH annular ring >= 0.15, silk text >= 0.8 mm, VBUS/SW tracks >= 0.3, no signal copper under the J1 shell |
| Review | `tools/review_pack.py` (schematic tiles, layer plots, 3D renders) + `tools/render_review.py` (annotated: no pours, nets named and coloured, mm grid) |
| Fab | `tools/fab_package.py`: gerbers + drill zip, JLC BOM (grouped by LCSC) and CPL with rotation corrections; only gitignored files are written (`pipeline_v1.sh --fab-only` verifies that and refuses a dirty tree) |

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

## Long-term direction (research 2026-10-15)
- **Keep the Python generators for now.** No alternative is clearly better yet: atopile (MIT, 0.15.x) is
  pre-release, needs Python 3.14, and its KiCad schematic output, layout reuse and JLC part flow could not
  be verified; SKiDL writes schematics but no layout; circuit-synth and Diode `pcb` (Zener) are young.
  Worth a one-board spike with atopile before the next design.
- **Stop overwriting manual edits.** Schematic stays generated (KiCad has no official schematic API).
  For the board, follow KiCad's forward annotation model: keep footprints, tracks and positions that
  exist, add new parts off-board, and pull positions edited in KiCad back into `PLACE`
  (a `tools/pull_placement.py`), so the generator and the GUI agree.
- **Move off pcbnew SWIG before KiCad 11** (removal planned; ~Feb 2027): KiCad 11 brings a headless IPC
  API server (`kicad-cli api-server`); port `layout_v1.py`/`finish.py`/`board_geom.py` to kipy then.
- **Add review layers, not generators:** kicad-happy (MIT, Claude Code skills: schematic/PCB review,
  EMC pre-check, JLC BOM/CPL) as a second opinion; visual diffs (KiRI / kidiff) for board changes.
- MCP servers (mixelpixx KiCAD-MCP-Server, lamaalrajih kicad-mcp) and cloud tools (Quilter, Flux, CELUS)
  would not replace the generator; Quilter only if KRT's routing stops being good enough.

## KiCad 10.0.x file format (research 2026-10-15)
- **Now:** the board is written by pcbnew SWIG from KiCad 10 (version 20260206). Schematics come from
  `schkit` with the 9.0 header (20250114); `build_v1.sh` runs `kicad-cli sch upgrade --force` on every
  sheet, giving 20260306 / "10.0". KiCad's own loader and saver do this, so it is the authoritative
  format. A second upgrade changes nothing (checked). Nets and ERC are unchanged. The upgrade also
  replaces our cached `lib_symbols` with the installed library versions.
- **Native 10 writer (next step, small):** have `schkit` emit the 10.0 header and property format
  (`(show_name no)(do_not_autoplace no)`, `duplicate_pin_numbers_are_jumpers` in lib symbols), and copy
  `lib_symbols` straight from the installed `.kicad_sym` files. Keep the upgrade as a gate: generate,
  upgrade, fail if anything beyond whitespace/UUIDs changes. kicad-sch-api 0.5.6 only claims KiCad 7/8
  output (no 10 support): keep it only for what `schkit` already wraps, or drop it.
  Format history: https://gitlab.com/kicad/code/kicad/-/blob/master/eeschema/sch_file_versions.h
- **Tool survey (2026-10-15, second pass):** nothing writes verified KiCad 10 schematics natively.
  Official: kicad-cli 10 has only `sch erc/export/upgrade`; no eeschema Python bindings in the bundle; IPC API
  (kicad-python 0.8.0) is PCB-only in 10, `get_schematic()` is marked "added in KiCad 11" (write coverage
  unconfirmed). Python: kicad-sch-api 0.5.6 (Nov 2025, KiCad 7/8 claims, open corruption issues #212/#207/#204),
  SKiDL 2.3.0 (active, KiCad 10 unverified), circuit-synth 0.12.1, kiutils/kicad-skip stale (2024).
  Rust: kiutils-rs / `kiutils_kicad` 0.3.0 (MIT, Sep 2026, lossless CST read/write of .kicad_sch, docs say
  KiCad 10 primary; creating schematics from scratch undocumented) - the one worth a spike if we leave Python.
  Diode `pcb` (Zener) targets KiCad 10 but emits layout/netlist, not schematics; tscircuit's KiCad schematic
  converter has no multi-sheet support. Decision: keep `schkit` + upgrade gate; revisit when KiCad 11 ships.
- **KiCad 11:** pcbnew SWIG is removed, so `layout_v1.py`, `finish.py` and `board_geom.py` need another way
  to write the board. Either kipy over the IPC API (needs headless `kicad-cli api-server`; verify it ships in
  11) or direct `.kicad_pcb` S-expression writing (fallback, no dependency on the GUI). The IPC API in 10
  has no schematic commands (community sources only, unverified), so schematics stay with the text writer.
  On each KiCad release: re-check `sch_file_versions.h`, re-run the upgrade gate, pin the KiCad version in
  `scripts/kicad-env.sh`.
