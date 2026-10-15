"""notifier-v1 schematic drawing: root block diagram + Power, MCU, Sensors, Display sheets.

Connectivity is defined by PARTS/NETS in notifier_v1.py and verified against KiCad's netlist
(schkit.check_nets); this file only decides where things go. Coordinates are cells of 2.54 mm,
x to the right, y down. Signal flow runs left to right, positive rails up, GND down.

Net naming in KiCad terms: GND, +3V3, VBUS are power symbols (global); VBUS_IN and SW are global
labels (routing patterns refer to them); USB_DP/USB_DN, I2C_* and LCD_* cross sheets through
hierarchical pins and a local label on the root page ("/USB_DP", ...); the rest is local to its
sheet ("/MCU/EN", "/Power/CC1", ...).
"""

import uuid
from pathlib import Path

from schkit import Sheet, root_sheet

LCD = ["LCD_SCLK", "LCD_MOSI", "LCD_DC", "LCD_CS", "LCD_RST"]
SHEET_FILES = {"Power": "power.kicad_sch", "MCU": "mcu.kicad_sch", "Sensors": "sensors.kicad_sch", "Display": "display.kicad_sch"}
SHEET_UUID = {n: str(uuid.uuid5(uuid.NAMESPACE_URL, "sheet/" + n)) for n in SHEET_FILES}


def draw_power(parts) -> Sheet:
    s = Sheet("Power", SHEET_FILES["Power"], parts, "A3", "USB-C in, ESD, polyfuse, 3.3 V buck, power LED")
    # --- USB-C receptacle: power, CC, data ---
    s.frame(10, 22, 58, 56, "USB-C input")
    s.part("J1", 20, 40)
    s.tag("J1", (-8, -22), (-8, -19.5))
    s.wire(("J1", "A4"), (26, 28), ("F1", "1"))               # VBUS_IN rail
    s.wire((26, 28), (24, 28))
    s.label("VBUS_IN", 24, 28, 180, "global")
    s.wire(("U2", "5"), (41, 28))
    s.wire((46, 28), (46, 26))
    s.label("VBUS_IN", 46, 26, 0, "global")                   # second label: a global label must appear twice
    s.part("R1", 34, 37.5)                                    # 5.1k CC pull-downs: sink identification
    s.wire(("J1", "A5"), ("R1", "1"))
    s.label("CC1", 28, 36, 0)
    s.wire(("R1", "2"), (34, 40))
    s.power("GND", 34, 40)
    s.part("R2", 30, 38.5)
    s.wire(("J1", "B5"), ("R2", "1"))
    s.label("CC2", 28, 37, 0)
    s.wire(("R2", "2"), (30, 41))
    s.power("GND", 30, 41)
    s.wire(("J1", "A6"), (27, 41))                            # D+ pair joined, then down to the ESD diode
    s.wire(("J1", "B6"), (27, 42))
    s.wire((27, 41), (27, 46), ("U2", "1"))
    s.wire(("J1", "A7"), (28, 39))                            # D- pair
    s.wire(("J1", "B7"), (28, 40))
    s.wire((28, 39), (28, 47), ("U2", "3"))
    s.part("U2", 41, 46)                                      # ESD diode in line with the data pair
    s.tag("U2", (3, -9), (3, -6.5))
    s.label("USB_DP", 30, 46, 0)
    s.wire((32, 47), (32, 49))
    s.label("USB_DN", 32, 49, 0)
    s.wire(("U2", "2"), (41, 51))
    s.power("GND", 41, 51)
    s.wire(("U2", "6"), (52, 46))
    s.label("USB_DP", 52, 46, 0, "hier")
    s.wire(("U2", "4"), (52, 47))
    s.label("USB_DN", 52, 47, 0, "hier")
    s.wire(("J1", "A1"), (20, 52))                            # GND (A1, A12, B1, B12 share one pin point), shield
    s.power("GND", 20, 52)
    s.wire(("J1", "SH"), (17, 51), (20, 51))
    s.wire((17, 51), (14, 51))
    s.flag("#FLG02", 14, 51)
    s.no_connect("J1", ["A8", "B8"])
    # --- polyfuse and protected VBUS ---
    s.frame(59, 22, 80, 34, "Fuse")
    s.part("F1", 63, 28, 90)
    s.wire(("F1", "2"), (74, 28))
    s.part("TP7", 68, 28)
    s.flag("#FLG01", 71, 28)
    s.wire((74, 28), (74, 25))
    s.power("VBUS", 74, 25)
    # --- 3.3 V buck ---
    b = 14                                                    # x offset of the buck group
    s.frame(70 + b, 30, 108 + b, 56, "3.3 V buck (TLV62569), Vout = 0.6 V * (1 + 100k/22k)")
    s.part("U3", 68 + b, 40)
    s.tag("U3", (-6, -11), (-6, -8.5))
    s.wire((60 + b, 39), ("U3", "4"))
    s.wire(("U3", "1"), (63 + b, 40), (63 + b, 39))           # EN tied to VIN: always on
    s.part("C1", 60 + b, 43)                                  # input cap beside VIN
    s.wire(("C1", "2"), (60 + b, 46))
    s.power("GND", 60 + b, 46)
    s.wire(("U3", "2"), (68 + b, 45))
    s.power("GND", 68 + b, 45)
    s.part("L1", 80 + b, 39, 90)
    s.wire(("U3", "3"), ("L1", "1"))
    s.wire((76 + b, 39), (76 + b, 36))
    s.label("SW", 76 + b, 36, 0, "global")
    s.wire(("L1", "2"), (104 + b, 39))                        # +3V3 rail
    s.wire((104 + b, 39), (104 + b, 36))
    s.power("+3V3", 104 + b, 36)
    s.part("R9", 84 + b, 43)                                  # feedback divider
    s.part("R10", 84 + b, 48)
    s.wire(("R9", "1"), (84 + b, 39))
    s.wire(("R9", "2"), ("R10", "1"))
    s.wire(("U3", "5"), (75 + b, 40), (75 + b, 45.5), (84 + b, 45.5))
    s.label("FB", 77 + b, 45.5, 0)
    s.wire(("R10", "2"), (84 + b, 51))
    s.power("GND", 84 + b, 51)
    s.part("C2", 90 + b, 43)                                  # output caps
    s.part("C3", 94 + b, 43)
    s.part("C11", 98 + b, 43)                                 # second 22 uF: TI wants >= 10 uF effective (DC-bias derating)
    s.wire(("C11", "1"), (98 + b, 39))
    s.wire(("C2", "1"), (90 + b, 39))
    s.wire(("C3", "1"), (94 + b, 39))
    s.wire(("C2", "2"), (90 + b, 47), (94 + b, 47), ("C3", "2"))
    s.wire((94 + b, 47), (98 + b, 47), ("C11", "2"))
    s.wire((92 + b, 47), (92 + b, 50))
    s.power("GND", 92 + b, 50)
    s.flag("#FLG03", 102 + b, 39)
    s.wire((60 + b, 28), ("C1", "1"))                         # VBUS trunk down to the buck input
    # --- power LED ---
    s.frame(112 + b, 30, 124 + b, 56, "Power LED")
    s.wire((104 + b, 39), (112 + b, 39))
    s.part("R3", 112 + b, 43)
    s.part("D1", 112 + b, 48, 90)
    s.wire(("R3", "1"), (112 + b, 39))
    s.wire(("R3", "2"), ("D1", "2"))
    s.label("LED_A", 112 + b, 45.5, 0)
    s.wire(("D1", "1"), (112 + b, 52))
    s.power("GND", 112 + b, 52)
    s.text(14, 62, "Notes\n"
           "- Buck instead of LDO: from 5 V an LDO would burn about 0.6 W and warm the sensors.\n"
           "- USB D+/D- are a 90 ohm pair (0.25 mm / 0.15 mm gap): keep short, ESD diode U2 right at J1.\n"
           "- CC1/CC2 5.1k to GND: the host offers 5 V. VBUS_IN is before the polyfuse, VBUS after it.\n"
           "- Layout: buck bottom centre, far from the BME688; input cap C1 at VIN, output caps at L1.\n"
           "- Output caps: C2 + C11 22 uF 0805 derate to about 11 uF each at 3.3 V; TLV62569 wants 10-47 uF effective.", 1.27)
    return s


def draw_mcu(parts, no_connect) -> Sheet:
    s = Sheet("MCU", SHEET_FILES["MCU"], parts, "A3", "ESP32-S3 module, reset/boot/user buttons, debug pads")
    s.frame(58, 26, 142, 72, "ESP32-S3-WROOM-1-N8R2")
    s.part("U1", 80, 55, 0, mirror=True)                      # mirrored: USB/UART left, GPIOs right
    s.tag("U1", (3, 27.5), (3, 30))
    s.no_connect("U1", no_connect["U1"])
    # supply and decoupling
    s.wire(("U1", "2"), (80, 36), (72, 36))
    s.wire((80, 36), (80, 33))
    s.power("+3V3", 80, 33)
    s.part("C5", 76, 37.5)
    s.part("C6", 72, 37.5)
    for r, x in (("C5", 76), ("C6", 72)):
        s.wire((r, "2"), (x, 40))
        s.power("GND", x, 40)
    s.part("C10", 68, 37.5)                                   # 1 uF: 10 uF + 1 uF + 0.1 uF at the 3V3 pin
    s.wire((72, 36), (68, 36))
    s.wire(("C10", "2"), (68, 40))
    s.power("GND", 68, 40)
    s.wire((80, 36), (84, 36))
    s.part("TP4", 84, 36)
    s.wire(("U1", "1"), (80, 70))
    s.power("GND", 80, 70)
    s.wire((80, 68), (84, 68))
    s.part("TP3", 84, 68, 180)
    # USB data and UART pads on the left
    for ref, pin, net, y, up in (("R11", "13", "USB_DN", 50, True), ("R12", "14", "USB_DP", 51, False)):
        s.part(ref, 70, y, 90)                                # 22 ohm series resistors at the module pins
        a, b_ = s.at(ref, "1"), s.at(ref, "2")
        left, right = ("1", "2") if a[0] < b_[0] else ("2", "1")
        assert left == "1", "R pin 1 must face the connector side"
        s.wire((ref, "1"), (66, y))
        s.label(net, 66, y, 180, "hier")
        s.wire((ref, "2"), (73, y), ("U1", pin))
        s.wire((73, y), (73, y - 1 if up else y + 1))
        s.label(net + "_MCU", 73, y - 1 if up else y + 1, 180)
        s.tag(ref, (0, -5.2 if up else 5.2), (0, -3.9 if up else 3.9), just="center")
    s.wire(("U1", "37"), (70, 46), (70, 43))
    s.label("TXD0", 70, 45, 0)
    s.part("TP1", 70, 43)
    s.wire(("U1", "36"), (62, 47), (62, 43))
    s.label("RXD0", 62, 45, 180)
    s.part("TP2", 62, 43)
    # I2C and display bus on the right
    for pin, net, y in (("4", "I2C_SDA", 52), ("5", "I2C_SCL", 53)):
        s.wire(("U1", pin), (89, y))
        s.label(net, 89, y, 0, "hier")
    for pin, net, y in (("20", "LCD_SCLK", 60), ("19", "LCD_MOSI", 59), ("17", "LCD_DC", 57), ("18", "LCD_CS", 58), ("22", "LCD_RST", 62)):
        s.wire(("U1", pin), (89, y))
        s.label(net, 89, y, 0, "hier")
    # EN: pull-up, RC delay cap, reset button, test pad
    s.frame(88, 28, 134, 50, "Reset, boot and user button")
    s.wire((89, 36), (120, 36))                                # +3V3 rail for the three pull-ups
    s.wire((80, 36), (89, 36))
    s.part("R4", 89, 37.5)
    s.wire(("U1", "3"), (89, 46), ("R4", "2"))
    s.label("EN", 89, 44, 0)
    s.wire((89, 41), (101, 41))
    s.part("C4", 93, 42.5)
    s.wire(("C4", "2"), (93, 45))
    s.power("GND", 93, 45)
    s.part("SW1", 97, 43, 270, value="RESET")
    s.wire(("SW1", "2"), (97, 46))
    s.power("GND", 97, 46)
    s.part("TP5", 101, 41)
    # BOOT (IO0): pull-up, button, test pad
    s.part("R5", 104, 37.5)
    s.wire(("U1", "27"), (104, 48), (104, 41), ("R5", "2"))
    s.label("BOOT", 104, 46, 0)
    s.wire((104, 41), (116, 41))
    s.part("SW2", 108, 43, 270, value="BOOT")
    s.wire(("SW2", "2"), (108, 46))
    s.power("GND", 108, 46)
    s.part("TP6", 116, 41)
    # user button (IO6)
    s.part("R6", 120, 37.5)
    s.wire(("U1", "6"), (120, 54), (120, 41), ("R6", "2"))
    s.label("BTN", 120, 46, 0)
    s.wire((120, 41), (126, 41))
    s.part("SW3", 126, 43, 270, value="BUTTON")
    s.wire(("SW3", "2"), (126, 46))
    s.power("GND", 126, 46)
    s.part("H1", 130, 68)
    s.part("H2", 134, 68)
    s.text(60, 78, "Notes\n"
           "- Module antenna end at the board edge: no copper, no vias in the antenna strip.\n"
           "- EN: 10k + 1u gives the reset delay; RESET pulls it low. BOOT = IO0 (strapping), 10k pull-up, button to GND.\n"
           "- UART0 pads TXD0/RXD0 + GND + 3V3 + EN + IO0 are the programming header (ESP auto-program order).\n"
           "- GPIO map matches firmware Kconfig DESK_BOARD_NOTIFIER_V1 (checked by hardware/tools/checks.py).\n"
           "- USB D+/D- (IO20/IO19) come from the Power sheet: 90 ohm pair, keep short. Unused pins carry no-connect flags.", 1.27)
    return s


def draw_sensors(parts) -> Sheet:
    s = Sheet("Sensors", SHEET_FILES["Sensors"], parts, "A4", "BME688 gas/pressure, SHT40 temperature/humidity, I2C pull-ups")
    s.frame(8, 14, 36, 36, "I2C pull-ups")
    s.wire((12, 31), (16, 31), (45, 31))
    s.wire(("U4", "3"), (45, 31))
    s.label("I2C_SDA", 12, 31, 180, "hier")
    s.wire((24, 29), (47, 29))
    s.wire(("U4", "4"), (47, 29))
    s.label("I2C_SCL", 24, 29, 180, "hier")
    s.part("R7", 16, 29.5)
    s.part("R8", 28, 27.5)
    s.wire(("R7", "1"), (16, 21), (28, 21))
    s.wire(("R8", "1"), (28, 21))
    s.wire((22, 21), (22, 19))
    s.power("+3V3", 22, 19)
    # BME688
    s.frame(42, 14, 82, 42, "BME688 (0x77)")
    s.part("U4", 60, 30, 0, mirror=True)
    s.tag("U4", (14, -2), (14, 1))
    s.wire(("U4", "8"), (59, 21), (72, 21))
    s.wire(("U4", "6"), (61, 21))
    s.wire((60, 21), (60, 19))
    s.power("+3V3", 60, 19)
    s.part("C7", 68, 22.5)
    s.part("C8", 72, 22.5)
    for r, x in (("C7", 68), ("C8", 72)):
        s.wire((r, "2"), (x, 26))
        s.power("GND", x, 26)
    s.wire(("U4", "7"), (59, 38), (61, 38))
    s.wire(("U4", "1"), (61, 38))
    s.wire((60, 38), (60, 40))
    s.power("GND", 60, 40)
    s.wire(("U4", "5"), (52, 27))
    s.power("+3V3", 52, 27)
    s.wire(("U4", "2"), (52, 33))
    s.power("+3V3", 52, 33, flip=True)
    # SHT40
    s.frame(42, 44, 72, 64, "SHT40 (0x44), 6 mm from the BME688")
    s.part("U5", 60, 56)
    s.tag("U5", (-10, 9), (-10, 11.5))
    s.wire((47, 29), (47, 55), ("U5", "2"))
    s.wire((45, 31), (45, 57), ("U5", "1"))
    s.wire(("U5", "3"), (61, 51), (66, 51))
    s.wire((64, 51), (64, 49))
    s.power("+3V3", 64, 49)
    s.part("C9", 66, 52.5)
    s.wire(("U5", "4"), (61, 61))
    s.power("GND", 61, 61)
    s.wire(("C9", "2"), (66, 55))
    s.power("GND", 66, 55)
    s.text(8, 48, "Notes\n"
           "- BME688: gas and pressure only. Its\n  gas heater self-heats, so T/RH come\n  from the SHT40 away from it.\n"
           "- I2C addresses: BME688 0x77 (SDO high),\n  SHT40 0x44. CSB high selects I2C.\n"
           "- 4.7k pull-ups sit at the bus entry;\n  no pours on the sensor island.", 1.27)
    return s


def draw_display(parts) -> Sheet:
    s = Sheet("Display", SHEET_FILES["Display"], parts, "A4", "GC9A01 round display header, LCD_VCC jumper")
    s.frame(10, 10, 44, 28, "Display supply")
    s.part("JP1", 30, 22)
    s.tag("JP1", (7, -1), (7, 1.5))
    s.wire(("JP1", "1"), (28, 19))
    s.power("+3V3", 28, 19)
    s.wire(("JP1", "3"), (32, 19))
    s.power("VBUS", 32, 19)
    s.wire(("JP1", "2"), (30, 33), ("J2", "1"))
    s.label("LCD_VCC", 40, 33, 0)
    s.frame(10, 30, 72, 46, "GC9A01 module (SPI, write only)")
    s.part("J2", 60, 36)
    s.tag("J2", (-2, -12), (-2, -9.5))
    s.wire(("J2", "2"), (47, 34), (47, 36))
    s.power("GND", 47, 36)
    for pin, net in zip("34567", LCD):
        y = s.at("J2", pin)[1] / 2.54
        s.wire(("J2", pin), (55, y))
        s.label(net, 55, y, 180, "hier")
    s.text(10, 50, "Notes\n"
           "- JP1: 1-2 bridged = LCD_VCC from 3V3 (default, 3.3 V modules).\n"
           "  For 5 V modules cut the 1-2 bridge and bridge 2-3 (VBUS).\n"
           "- J2 pin order = module header: VCC, GND, SCL, SDA, DC, CS, RST\n  (no MISO, no backlight pin).\n"
           "- Display plugs in from the side; its face sits past the board edge.", 1.27)
    return s


def build(out: Path, parts, no_connect) -> Path:
    from schkit import Sheet as _S
    _S.pwr_count = 0
    sheets = [draw_power(parts), draw_mcu(parts, no_connect), draw_sensors(parts), draw_display(parts)]
    # root: block diagram with sheet symbols, wired through hierarchical pins
    root_uuid = str(uuid.uuid5(uuid.NAMESPACE_URL, "root"))
    sd = [
        dict(name="Power", file=SHEET_FILES["Power"], x=14, y=30, w=30, h=22, page="2", uuid=SHEET_UUID["Power"],
             pins=[("right", 38, "USB_DP"), ("right", 40, "USB_DN")]),
        dict(name="MCU", file=SHEET_FILES["MCU"], x=64, y=26, w=36, h=44, page="3", uuid=SHEET_UUID["MCU"],
             pins=[("left", 38, "USB_DP"), ("left", 40, "USB_DN"), ("right", 30, "I2C_SDA"), ("right", 32, "I2C_SCL")]
             + [("right", 46 + 2 * i, n) for i, n in enumerate(LCD)]),
        dict(name="Sensors", file=SHEET_FILES["Sensors"], x=120, y=22, w=30, h=16, page="4", uuid=SHEET_UUID["Sensors"],
             pins=[("left", 30, "I2C_SDA"), ("left", 32, "I2C_SCL")]),
        dict(name="Display", file=SHEET_FILES["Display"], x=120, y=42, w=30, h=22, page="5", uuid=SHEET_UUID["Display"],
             pins=[("left", 46 + 2 * i, n) for i, n in enumerate(LCD)]),
    ]
    wires, labels = [], []
    for y, n in ((38, "USB_DP"), (40, "USB_DN")):
        wires.append(((44, y), (64, y)))
        labels.append((n, 50, y))
    for y, n in ((30, "I2C_SDA"), (32, "I2C_SCL")) + tuple((46 + 2 * i, n) for i, n in enumerate(LCD)):
        wires.append(((100, y), (120, y)))
        labels.append((n, 106, y))
    texts = [
        (15, 32, "USB-C connector, CC, ESD\npolyfuse, 3.3 V buck, LED\n\nout: +3V3, VBUS (symbols)"),
        (65, 28, "ESP32-S3-WROOM-1 module\nEN / BOOT / RESET, user button\ndecoupling, UART debug pads"),
        (121, 24, "BME688 (gas, 0x77)\nSHT40 (T/RH, 0x44)\nI2C pull-ups"),
        (121, 44, "GC9A01 round display\nheader J2, LCD_VCC jumper JP1"),
        (14, 8, "#desk notifier v1: ESP32-S3 desk notifier, USB-C powered"),
        (14, 12, "Signal flow left to right: power entry (USB-C), MCU in the middle, sensors and display on the right."),
        (14, 16, "GND, +3V3 and VBUS are power symbols on every sheet (not routed here). Open a sheet: double-click it."),
        (14, 58, "Power: USB-C, ESD, polyfuse, 3.3 V buck, LED"),
        (14, 61, "MCU: module, EN/BOOT/RESET, buttons, debug pads"),
        (14, 64, "Sensors: BME688 (gas), SHT40 (T/RH), I2C pull-ups"),
        (14, 67, "Display: GC9A01 header, LCD_VCC jumper JP1"),
        (14, 74, "Design intent"),
        (14, 77, "- 3.3 V from a buck, not an LDO: 0.6 W would heat the sensors."),
        (14, 80, "- SHT40 for temperature/humidity: the BME688 gas heater self-heats."),
        (14, 83, "- USB D+/D- are a 90 ohm pair: keep short. Antenna keepout under the module's antenna."),
        (14, 86, "- I2C: BME688 0x77, SHT40 0x44. JP1: 3V3 by default; cut and bridge for 5 V modules."),
    ]
    path, _ = root_sheet(out, "root", sd, texts, wires, labels)
    for sh in sheets:
        sh.save(out, root_uuid, SHEET_UUID[sh.name])
    return path
