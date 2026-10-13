"""notifier-v1 schematic: ESP32-S3-WROOM-1, USB-C power/data, AMS1117, BME688, 7-pin GC9A01 header.

PARTS and NETS are the source of truth; the checks (tools/checks.py) import them.
Run: uv run python hardware/gen/notifier_v1.py [out_dir]
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kicad_sch_api as ksa  # noqa: E402
from schkit import check_nets, connect, no_connect, save  # noqa: E402

R0402, C0402 = "Resistor_SMD:R_0402_1005Metric", "Capacitor_SMD:C_0402_1005Metric"
C0603, C0805 = "Capacitor_SMD:C_0603_1608Metric", "Capacitor_SMD:C_0805_2012Metric"

# ref: (symbol, value, footprint, LCSC, (x, y) on the A3 sheet in mm, rotation)
PARTS = {
    # USB-C input and ESD
    "J1": ("Connector:USB_C_Receptacle_USB2.0_16P", "USB-C", "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12", "C165948", (45, 80), 0),
    "R1": ("Device:R", "5.1k", R0402, "C25905", (80, 105), 0),
    "R2": ("Device:R", "5.1k", R0402, "C25905", (90, 105), 0),
    "U2": ("Power_Protection:USBLC6-2SC6", "USBLC6-2SC6", "Package_TO_SOT_SMD:SOT-23-6", "C7519", (95, 45), 0),
    # 3.3 V regulator
    "U3": ("Regulator_Linear:AMS1117-3.3", "AMS1117-3.3", "Package_TO_SOT_SMD:SOT-223-3_TabPin2", "C6186", (160, 45), 0),
    "C1": ("Device:C", "10u", C0603, "C19702", (140, 60), 0),
    "C2": ("Device:C", "22u", C0805, "C45783", (180, 60), 0),
    "C3": ("Device:C", "100n", C0402, "C1525", (190, 60), 0),
    "R3": ("Device:R", "1k", R0402, "C11702", (210, 45), 0),
    "D1": ("Device:LED", "red", "LED_SMD:LED_0603_1608Metric", "C2286", (210, 65), 90),
    # ESP32-S3 module, reset, boot, user button
    "U1": ("RF_Module:ESP32-S3-WROOM-1", "ESP32-S3-WROOM-1-N8R2", "RF_Module:ESP32-S3-WROOM-1", "C2913204", (300, 150), 0),
    "C5": ("Device:C", "22u", C0805, "C45783", (250, 90), 0),
    "C6": ("Device:C", "100n", C0402, "C1525", (260, 90), 0),
    "R4": ("Device:R", "10k", R0402, "C25744", (215, 120), 0),
    "C4": ("Device:C", "1u", C0402, "C52923", (225, 140), 0),
    "SW1": ("Switch:SW_Push", "RESET", "Button_Switch_SMD:SW_Push_1P1T_XKB_TS-1187A", "C318884", (205, 140), 90),
    "R5": ("Device:R", "10k", R0402, "C25744", (215, 165), 0),
    "SW2": ("Switch:SW_Push", "BOOT", "Button_Switch_SMD:SW_Push_1P1T_XKB_TS-1187A", "C318884", (205, 185), 90),
    "R6": ("Device:R", "10k", R0402, "C25744", (235, 165), 0),
    "SW3": ("Switch:SW_Push", "BUTTON", "Button_Switch_SMD:SW_Push_1P1T_XKB_TS-1187A", "C318884", (235, 185), 90),
    # BME688 (BME680 symbol: same LGA-8 pinout)
    "U4": ("Sensor:BME680", "BME688", "Package_LGA:Bosch_LGA-8_3x3mm_P0.8mm_ClockwisePinNumbering", "C3664478", (90, 200), 0),
    "C7": ("Device:C", "100n", C0402, "C1525", (60, 185), 0),
    "C8": ("Device:C", "100n", C0402, "C1525", (70, 185), 0),
    "R7": ("Device:R", "4.7k", R0402, "C25900", (120, 185), 0),
    "R8": ("Device:R", "4.7k", R0402, "C25900", (130, 185), 0),
    # GC9A01 display header and its supply selector (default 3V3, cut and bridge 2-3 for 5 V)
    "J2": ("Connector_Generic:Conn_01x07", "GC9A01", "Connector_PinSocket_2.54mm:PinSocket_1x07_P2.54mm_Horizontal", "C22436148", (170, 230), 0),
    "JP1": ("Jumper:SolderJumper_3_Bridged12", "LCD_VCC", "Jumper:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm", None, (140, 215), 0),
    # debug UART pads, mounting holes
    "TP1": ("Connector:TestPoint", "TXD0", "TestPoint:TestPoint_Pad_D1.5mm", None, (370, 120), 0),
    "TP2": ("Connector:TestPoint", "RXD0", "TestPoint:TestPoint_Pad_D1.5mm", None, (380, 120), 0),
    "TP3": ("Connector:TestPoint", "GND", "TestPoint:TestPoint_Pad_D1.5mm", None, (390, 120), 0),
    "H1": ("Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", None, (370, 250), 0),
    "H2": ("Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", None, (385, 250), 0),
    "#FLG01": ("power:PWR_FLAG", "PWR_FLAG", None, None, (30, 30), 0),
    "#FLG02": ("power:PWR_FLAG", "PWR_FLAG", None, None, (45, 30), 0),
}

# module pin -> GPIO, for the pin map check against firmware Kconfig
U1_GPIO = {"4": 4, "5": 5, "6": 6, "17": 9, "18": 10, "19": 11, "20": 12, "22": 14, "27": 0, "13": 19, "14": 20, "37": 43, "36": 44}
U1_UNUSED = ["7", "8", "9", "10", "11", "12", "15", "16", "21", "23", "24", "25", "26", "28", "29", "30", "31", "32", "33", "34", "35", "38", "39"]

NETS = {
    "VBUS": [("J1", "A4"), ("J1", "A9"), ("J1", "B4"), ("J1", "B9"), ("U2", "5"), ("U3", "3"), ("C1", "1"), ("JP1", "3"), ("#FLG01", "1")],
    "GND": [("J1", "A1"), ("J1", "A12"), ("J1", "B1"), ("J1", "B12"), ("J1", "SH"), ("R1", "2"), ("R2", "2"), ("U2", "2"),
            ("U3", "1"), ("C1", "2"), ("C2", "2"), ("C3", "2"), ("D1", "1"), ("U1", "1"), ("U1", "40"), ("U1", "41"),
            ("C5", "2"), ("C6", "2"), ("C4", "2"), ("SW1", "2"), ("SW2", "2"), ("SW3", "2"), ("U4", "1"), ("U4", "7"),
            ("C7", "2"), ("C8", "2"), ("J2", "2"), ("TP3", "1"), ("#FLG02", "1")],
    "+3V3": [("U3", "2"), ("C2", "1"), ("C3", "1"), ("R3", "1"), ("U1", "2"), ("C5", "1"), ("C6", "1"), ("R4", "1"),
             ("R5", "1"), ("R6", "1"), ("U4", "8"), ("U4", "6"), ("U4", "2"), ("U4", "5"), ("C7", "1"), ("C8", "1"),
             ("R7", "1"), ("R8", "1"), ("JP1", "1")],
    "CC1": [("J1", "A5"), ("R1", "1")],
    "CC2": [("J1", "B5"), ("R2", "1")],
    "USB_DP": [("J1", "A6"), ("J1", "B6"), ("U2", "1"), ("U2", "6"), ("U1", "14")],
    "USB_DN": [("J1", "A7"), ("J1", "B7"), ("U2", "3"), ("U2", "4"), ("U1", "13")],
    "LED_A": [("R3", "2"), ("D1", "2")],
    "EN": [("U1", "3"), ("R4", "2"), ("C4", "1"), ("SW1", "1")],
    "BOOT": [("U1", "27"), ("R5", "2"), ("SW2", "1")],
    "BTN": [("U1", "6"), ("R6", "2"), ("SW3", "1")],
    "I2C_SDA": [("U1", "4"), ("U4", "3"), ("R7", "2")],
    "I2C_SCL": [("U1", "5"), ("U4", "4"), ("R8", "2")],
    "LCD_VCC": [("JP1", "2"), ("J2", "1")],
    "LCD_SCLK": [("U1", "20"), ("J2", "3")],
    "LCD_MOSI": [("U1", "19"), ("J2", "4")],
    "LCD_DC": [("U1", "17"), ("J2", "5")],
    "LCD_CS": [("U1", "18"), ("J2", "6")],
    "LCD_RST": [("U1", "22"), ("J2", "7")],
    "TXD0": [("U1", "37"), ("TP1", "1")],
    "RXD0": [("U1", "36"), ("TP2", "1")],
}
NO_CONNECT = {"J1": ["A8", "B8"], "U1": U1_UNUSED}


def build(out: Path) -> Path:
    out.mkdir(parents=True, exist_ok=True)
    sch = ksa.Schematic.create("notifier-v1")
    sch.set_paper_size("A3")
    sch.set_title_block(title="desk notifier v1", rev="1", company="desk_notifier")
    for ref, (sym, value, fp, lcsc, pos, rot) in PARTS.items():
        props = {"LCSC": lcsc} if lcsc else {}
        comp = sch.components.add(sym, ref, value, pos, fp, rotation=rot, **props)
        if not lcsc and not ref.startswith("#"):
            comp.in_bom = False
    connect(sch, NETS)
    for ref, pins in NO_CONNECT.items():
        no_connect(sch, ref, pins)
    path = out / "notifier-v1.kicad_sch"
    save(sch, path)
    return path


if __name__ == "__main__":
    path = build(Path(sys.argv[1] if len(sys.argv) > 1 else "hardware/notifier-v1"))
    errors = check_nets(path, NETS)
    print("\n".join(errors) or f"{path}: nets OK")
    sys.exit(1 if errors else 0)
