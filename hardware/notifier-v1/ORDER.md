# notifier-v1: JLCPCB order checklist

## Order of operations (the silkscreen carries the git SHA)
The full pipeline re-routes the board (the router output differs between runs), so it is **not** part of the order step.
1. Design phase: `hardware/tools/pipeline_v1.sh --bom` (regenerate, route, gate, review pack, fab package). It must end
   with `hardware checks: OK` (ERC + DRC from kicad-cli, pin map, live JLC stock). The router (KiCadRoutingTools) is
   not robust on the crowded corner left of the module (open EN/GND/VBUS nets, a via in a pad) and its result depends
   on the random object ids of the placed board, so the pipeline re-places and re-routes (up to `KRT_TRIES` = 12 attempts)
   until the gate passes and prints `routing attempt N: gate OK`.
2. Review the board (KiCad, `hardware/out/review/notifier-v1/annot_*.png`), then **commit everything under `hardware/`**
   (`git status --short -- hardware` must print nothing).
3. Order phase: `hardware/tools/pipeline_v1.sh --fab-only --bom`. It refuses a dirty `hardware/` tree, runs the gate and
   `fab_package.py` on the committed `.kicad_pcb` as is (no generation, no routing, no tracked file written: only
   `hardware/out/` and the ignored `*.net` are written) and checks afterwards that the tree is still clean. It resolves
   `${GIT_SHA}` while plotting (`kicad-cli -D GIT_SHA=...`) and writes `VERSION.txt` and `notifier-v1-<sha>-gerbers.zip`.
4. Confirm `hardware/out/fab/notifier-v1/VERSION.txt` has no `-dirty`, upload the files below, and check in JLC's gerber
   viewer that the silk reads the SHA (not `${GIT_SHA}`).

Generated files: `hardware/out/fab/notifier-v1/`

| File | Upload as |
|---|---|
| `notifier-v1-<sha>-gerbers.zip` | Gerber file (PCB) |
| `notifier-v1-bom.csv` | BOM (assembly) |
| `notifier-v1-cpl.csv` | CPL / pick and place (assembly) |

Gerber settings follow JLC's KiCad guide: Protel extensions, soldermask subtracted from silk, zones checked, no X2
and no netlist attributes; layers F/In1/In2/B Cu, F/B Paste, F/B Mask, F/B Silkscreen, Edge_Cuts; Excellon in mm
(decimal), `-PTH.drl` and `-NPTH.drl` separate, no map file. The four M2 holes and the J1 locating posts are in
NPTH; the sensor slot is milled on Edge_Cuts. BOM headers: Comment, Designator, Footprint, LCSC Part #; CPL:
Designator, Mid X, Mid Y, Layer, Rotation. Mounting holes, test pads and the solder jumper (no LCSC) are not in BOM/CPL.

## PCB options
- Layers **4**, size 50 x 36 mm (the module antenna overhangs the top edge by 6.0 mm, body y -6.0..0, not part of the
  outline), FR-4, **1.6 mm**. No V-cut, no breakaway rails of our own (see Edge rails below).
- Stackup: select **JLC04161H-7628** (L1-L2 prepreg 7628, 0.2104 mm, er about 4.4; the rules in
  `hardware/rules/jlc-4layer.json` assume it).
- **Impedance control: do not tick it.** USB full speed (12 Mbit/s, edges about 4 ns) has a critical length of
  more than 100 mm; the pair here is about 13 mm, so reflections do not matter and +-20 % on Zdiff is harmless.
  Ticking it costs money and adds a test coupon. The geometry still sizes for 90 ohm where the pair is coupled:
  USB D+/D- are **0.27 mm tracks, 0.15 mm gap** on L1 over the In1 GND plane. They are coupled only for about 0.5 mm
  (elsewhere about 2 mm apart); it is a short (about 13 mm) full-speed pair, so coupling is not critical. A few
  stubs next to J1 are 0.2 mm and the D- A7/B7 join uses two 0.45/0.2 vias and a 1.4 mm B.Cu jumper (edge-coupled microstrip, `h` 0.2104 mm,
  er 4.4, copper 0.035 mm; 2D field solver `hardware/calc/impedance.py`): Zdiff about 95 ohm bare, about 91 ohm with
  soldermask. To cross-check in JLC's online calculator (jlcpcb.com/impedance): 4 layers, 1.6 mm, JLC04161H-7628,
  differential, L1 reference L2, width 0.27, spacing 0.15.
- Surface finish **ENIG**: flat pads for the BME688 LGA (0.8 mm pitch) and SHT40 DFN, no HASL meniscus; costs a little
  more than lead-free HASL, which would also work (0.8 mm pitch is not fine pitch), but the paste-print flatness is worth it.
- Outer copper 1 oz, inner 0.5 oz. Via covering: **tented** (JLC default "Epoxy filled/tented", not plugged).
  Min via 0.3/0.6 mm for most vias; 16 holes are 0.2 mm in the committed board: 12 thermal vias of the module pad,
  3 hand-drawn 0.45/0.2 vias at J1/U2 (D- jumper x2, VBUS_IN x1) and 1 router via (the router makes 0.2 mm vias where
  it has no room; here the EN net at (13.75, 5.3), not GND, and the net changes between pipeline runs): inside
  JLC's 4-layer minimum (0.15 mm drill, 0.45 mm pad here), no option needed. Min trace/space 0.127 mm used here
  (0.09 allowed), clearance 0.15: fine.
- The BME688 sits on a peninsula cut by a 1.2-1.8 mm slot: milled outline, inner corners get the tool radius. Nothing to select.
- **Order number: "Specify a location".** `JLCJLCJLCJLC` (1 mm high) is on F.Silkscreen at the bottom edge, left of
  the buck converter. JLC replaces it with the order number there; "Remove mark" costs extra and random placement
  could land on a pad area.

## Assembly options
- PCBA **top side**, **Standard** PCBA. J2 (C22436148) is a through-hole socket: it stays in BOM and CPL and JLC
  solders it, which adds a **through-hole / manual soldering fee** (shown on the quote: check it before paying).
- **Edge rails / panel:** parts sit within 3 mm of the left and right edges (J1, J2) and the board is small, so the machine
  needs rails. Select **Process edge: added by JLCPCB**, rails on the **LEFT and RIGHT edges only** (5 mm, "Single
  boards", no V-cut/panel of our own). Not on the top: the module antenna overhangs the top edge by 6.0 mm (body y
  -6.0..0), so a rail or V-cut there would hit the antenna and the pick-up area. Nothing is needed at the bottom.
  The rails also carry the fiducials and tooling holes JLC adds itself: none are on the board. Confirm the rail option
  in the PCBA quote (wording changes; its fee, if any, is shown there).
  **J1 protrudes 0.25 mm past the left edge** (fab outline at x = -0.25) and J2's socket face is at x = 50.03 (0.03 mm past
  the right edge): ask JLC to leave a routed gap/clearance on the left rail so the plug opening is not clipped. Order remark
  (paste into the PCBA remark field):
  > `Process edge on LEFT and RIGHT sides only, none on top/bottom (ESP32-S3-WROOM-1 antenna overhangs the top edge by
  > 6 mm, body is above the board outline). USB-C J1 (C165948, HRO TYPE-C-31-M-12) protrudes 0.25 mm past the left
  > board edge and header J2 0.03 mm past the right edge: please keep a routed gap / clearance between board and rail at
  > both, do not clip the connectors. Specify order-number location on silkscreen (marked JLCJLCJLCJLC). Please send the
  > placement preview before assembly (U1, U2, U4, U5, D1, J1, J2 polarity/rotation).`
- Quantity: assemble all boards ordered (e.g. 5) or 2 to save on the BME688 ($11.7 each).

## Parts (22 BOM lines, Extended-part setup fee each)
Run `scripts/hw-check.sh --bom --qty N` right before ordering: it fails on missing stock.
Watch (stock seen 2026-10-16, it moves: **check live and order soon**): **BME688 C3664478** (about 34 left),
**ESP32-S3-WROOM-1-N8R2 C2913204** (about 767; N8R8 C2913201 is the fallback, it needs `CONFIG_SPIRAM_MODE_OCT=y`),
J2 header C22436148 (hundreds).

## Placement preview: check every IC by hand
CPL rotation = KiCad rotation + a correction (community table JLCKicadTools / kicad-jlcpcb-tools `cpl_rotations_db`;
`ROTATION` in `hardware/tools/fab_package.py`). Pin positions below are the KiCad footprints at 0 degrees; JLC's package
convention is pin 1 top left. None of this is tested against JLC's own library: the preview is the test.
In JLC's placement preview confirm pin 1 against the silkscreen/body for each of:
- [ ] **U1** ESP32-S3-WROOM-1 (CPL 0, no correction: the table only has `^ESP32-W`, 270, which does not match). Look for:
  antenna toward the top edge, off the board; pin 1 (GND) at the top of the left pin column.
- [ ] **U2** USBLC6-2SC6 (SOT-23-6, KiCad 180, `^SOT-23` -90 -> CPL 90). KiCad footprint at 0: 1-3 down the left column, 4-6 up
  the right one. The rule makes JLC's zero orientation pin 1 bottom left with 1-2-3 along the bottom; CPL 90 then puts
  pin 1 bottom right and pins 1-2-3 up the right column, which is what the board has (pin 1 at (12.74, 25.15), the D+
  row). A 180 error would put VBUS_IN (pin 5, middle of the *left* column, x 10.46) on the GND side: check that the
  middle pin on the side toward J1 is the one with the 5 V track and the right-hand middle pin is the one with the GND via.
- [ ] **U3** TLV62569 (SOT-23-5, KiCad 180 -> CPL 90): same rule, probably right.
- [ ] **U4** BME688 (Bosch LGA-8 3x3 mm, KiCad 180, **+90 added -> CPL 270**). The KiCad footprint at 0 has pin 1 top left, pins
  1-4 along the top, 5-8 back along the bottom (clockwise): the same layout as the Bosch 2 x 2.5 mm LGA, for which the
  community table adds 90. Assumed the same for the 3 x 3 mm part: **uncertain**. Look for: pin 1 marker on the preview at
  the corner next to the silk dot (the corner toward the board's left/bottom here), vent hole up.
- [ ] **U5** SHT40 (Sensirion DFN-4 1.5 mm, KiCad 90, **+270 added -> CPL 0**). The KiCad footprint at 0 has pin columns
  left/right (1-2 down the left column, 3-4 up the right one, pin 1 top left): the same topology as the KiCad `DFN-*`
  footprints for which the table adds 270 (the pattern `^DFN-` does not match the name `Sensirion_DFN-4...`, so the
  correction is applied by an explicit entry). **Uncertain.** Look for: pin 1 (SDA) against the silk dot; the sensor opening
  must face up; JLC lists it as "DFN-4-EP", the KiCad footprint has no centre pad.
- [ ] **J1** USB-C (HRO, KiCad 270 + 180 -> CPL 90): the rotation follows the table (certain if the table holds), but **the
  position offset is uncertain**: JLC may use its own part origin; the shell must match the footprint outline (fab
  outline x -0.25..7.1), opening out of the left edge.
- [ ] **D1** LED 0603 (KiCad 180 -> CPL 180, no table entry): **uncertain**, community reports 0603 LEDs often need 180; cathode toward
  GND (D1 is placed at KiCad 180: the cathode is pad 1 on the GND side, at the right, x 31.4; the anode, pad 2, faces R3). Wrong polarity is harmless (LED stays dark) but fix it.
- [ ] **J2** header (THT, CPL 180): **uncertain**; body toward the right edge, opening outward, pin 1 at the bottom (matches silk "1").
- [ ] L1, F1, R/C (non-polar): certain.
Fix any wrong part in JLC's editor and note the correction in `ROTATION` in `hardware/tools/fab_package.py`.

## Sensors (datasheet handling notes)
- No conformal coating, no-clean flux only near U4/U5: the SHT40 sensor opening must stay open (SHT4x datasheet 5.3),
  the BME688 vent must not be contaminated (Bosch BME68x handling instructions, BST-BME680-HS000).
- In the enclosure: an air path to both sensors, BME688 vent away from direct light, no heat source next to the island.

## Not assembled
- The GC9A01 display module (7 pins). J2 is mirrored for the right-edge socket: pin 1 RST, 2 CS, 3 DC, 4 SDA (MOSI),
  5 SCL (SCLK), 6 GND, 7 VCC. **Reversed plug-in is not a supply reversal**: the module's VCC pin would land on pin 1
  (RST, GPIO14) and its GND pin on pin 2 (CS, GPIO10): two GPIOs driven into the module's supply pins (and the module's
  RST/CS pins on the board's GND and LCD_VCC). Probably survivable for a moment, but it can damage the GPIOs or the display.
  Check before plugging in: hold the module from the back (the header side), header down; the labels must read
  **VCC GND SCL SDA DC CS RST** from left to right, and that is how it plugs into J2 (pin 7 VCC is the top pin of the
  socket seen from above, pin 1 RST at the bottom; the bottom silkscreen next to J2 lists the pins).
- LCD supply jumper JP1 ships bridged to **3V3**. Only cut it and bridge 5V if the module has its own regulator.

## Reference designators and the assembly drawing
The board carries **no reference designators on the silkscreen** (no space; only user-facing labels and the order number).
References are on the fab layers (F.Fab / B.Fab) of `notifier-v1.kicad_pcb`. The review pack's `layer_top.png` is rendered
from `F.Cu,F.SilkS,F.Fab,Edge.Cuts` (`tools/review_pack.py`), so the small reference texts in it come from F.Fab, not from
silk. The assembly drawing for debugging is that review pack (`hardware/out/review/notifier-v1/`) or the KiCad file itself
(File > Plot or the 3D/Fab layer view); nothing is printed on the board.

## Bring-up
1. Visual check; measure 3V3 (TP "3V3") and VBUS to GND for shorts.
2. USB-C from a current-limited supply or a powered hub: red LED on, 3.3 V on the 3V3 pad.
3. First delete `firmware/build-s3/sdkconfig` (or run `idf.py -B build-s3 fullclean`) so the Kconfig defaults for notifier-v1
   (`DESK_BTN=-1`, pin map) apply instead of an older cached sdkconfig. Flash over the native USB (S3 USB Serial/JTAG):
   `cd firmware && idf.py -B build-s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-s3/sdkconfig -p /dev/cu.usbmodem* flash monitor`
   If it does not enumerate (no buttons on this board): bridge GND-IO0 with tweezers, bridge EN-GND briefly, release IO0. Fallback: UART pads TX/RX/GND with a USB-UART adapter.
4. Wi-Fi credentials over the console, then `GET /api/status`: `sensor.i2c` must list 0x44 (SHT40) and 0x77 (BME688).
5. Plug in the display (JP1 on 3V3), check orientation and colours (DESK_LCD_MIRROR_X/BGR may differ from the breadboard).
