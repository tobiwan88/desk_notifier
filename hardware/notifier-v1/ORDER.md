# notifier-v1: JLCPCB order checklist

Generated files: `hardware/out/fab/notifier-v1/` (run `hardware/tools/pipeline_v1.sh --bom` first;
it must end with `hardware checks: OK`).

| File | Upload as |
|---|---|
| `notifier-v1-gerbers.zip` | Gerber file (PCB) |
| `notifier-v1-bom.csv` | BOM (assembly) |
| `notifier-v1-cpl.csv` | CPL / pick and place (assembly) |

## PCB options
- Layers **4**, size 50 x 36 mm (the module antenna overhangs the top edge, not part of the outline), FR-4, **1.6 mm**.
- Stackup **JLC04161H-7628** (the rules in `hardware/rules/jlc-4layer.json` assume it; no impedance control ordered,
  USB is full speed).
- Surface finish **ENIG** (flat pads for the BME688 LGA and the module).
- Outer copper 1 oz, inner 0.5 oz. Via covering: tented. Min via 0.3/0.6 mm, no via-in-pad option needed.
- The BME688 sits on a peninsula cut by a 1.2 mm notch: milled outline, inner corners get the tool radius. Nothing to select.
- Order number: "Remove mark" costs extra; otherwise JLC places it somewhere free.

## Assembly options
- PCBA **top side**, **Standard** PCBA (J2 is a through-hole header; JLC solders it by hand/wave).
- Quantity: assemble all boards ordered (e.g. 5) or 2 to save on the BME688 ($11.7 each).
- Tooling holes: added by JLCPCB.

## Parts (19 BOM lines, 8 Extended: setup fee each)
Run `scripts/hw-check.sh --bom --qty N` right before ordering: it fails on missing stock.
Watch: **BME688 C3664478** (stock about 200, fluctuates), **ESP32-S3-WROOM-1-N8R2 C2913204** (about 1.7k;
N8R8 C2913201 is the fallback, it needs `CONFIG_SPIRAM_MODE_OCT=y`), J2 header C22436148 (about 680).

## Placement preview: check every IC by hand
Rotation corrections in the CPL come from an older community table (JLCKicadTools via kicad-jlcpcb-tools).
In JLC's 3D/placement preview, confirm pin 1 against the silkscreen dot for each of:
- [ ] U1 ESP32-S3 module: antenna toward the top edge, off the board.
- [ ] U2 USBLC6-2SC6 (SOT-23-6, -90 applied).
- [ ] U3 TLV62569 (SOT-23-5, -90 applied by the SOT-23 rule; most likely to be wrong).
- [ ] U4 BME688 (LGA-8, no correction): pin 1 marker top-left; the vent hole up.
- [ ] J1 USB-C (+180 applied): opening at the bottom edge.
- [ ] D1 LED cathode toward the GND side; L1, F1 non-polar.
- [ ] J2 header: body toward the right edge, opening outward.
Fix any wrong part in JLC's editor and note the correction in `ROTATION` in `hardware/tools/fab_package.py`.

## Not assembled
- The GC9A01 display module (7 pins: VCC GND SCL SDA DC CS RST). Check the pin order on the module you bought
  against the bottom silkscreen next to J2 before plugging it in: a VCC/GND swap kills it.
- LCD supply jumper JP1 ships bridged to **3V3**. Only cut it and bridge 5V if the module has its own regulator.

## Bring-up
1. Visual check; measure 3V3 (TP "3V3") and VBUS to GND for shorts.
2. USB-C from a current-limited supply or a powered hub: red LED on, 3.3 V on the 3V3 pad.
3. Flash over the native USB (S3 USB Serial/JTAG):
   `cd firmware && idf.py -B build-s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-s3/sdkconfig -p /dev/cu.usbmodem* flash monitor`
   If it does not enumerate: hold BOOT, tap RST, release BOOT. Fallback: UART pads TX/RX/GND (+EN/IO0) with a USB-UART adapter.
4. Wi-Fi credentials over the console, then `GET /api/status`: `sensor.i2c` must list 0x77.
5. Plug in the display (JP1 on 3V3), check orientation and colours (DESK_LCD_MIRROR_X/BGR may differ from the breadboard).
