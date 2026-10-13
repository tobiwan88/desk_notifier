"""Phase 0 spike: generate a toy schematic (USB 5 V -> LDO -> LED) and verify its nets."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gen"))
import kicad_sch_api as ksa  # noqa: E402
from schkit import check_nets, connect, pin_pos  # noqa: E402

out = Path(sys.argv[1] if len(sys.argv) > 1 else "hardware/out/toy")
out.mkdir(parents=True, exist_ok=True)

sch = ksa.Schematic.create("toy")
add = sch.components.add
add("Connector_Generic:Conn_01x02", "J1", "PWR", (40, 60), "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical")
add("Regulator_Linear:AP2112K-3.3", "U1", "AP2112K-3.3", (90, 60), "Package_TO_SOT_SMD:SOT-23-5")
add("Device:C", "C1", "1u", (65, 70), "Capacitor_SMD:C_0603_1608Metric")
add("Device:C", "C2", "1u", (115, 70), "Capacitor_SMD:C_0603_1608Metric")
add("Device:R", "R1", "1k", (135, 60), "Resistor_SMD:R_0603_1608Metric")
add("Device:LED", "D1", "green", (135, 80), "LED_SMD:LED_0603_1608Metric", rotation=90)
add("power:PWR_FLAG", "#FLG01", "PWR_FLAG", (40, 40))
add("power:PWR_FLAG", "#FLG02", "PWR_FLAG", (50, 40))

nets = {
    "VBUS": [("J1", "1"), ("U1", "1"), ("U1", "3"), ("C1", "1"), ("#FLG01", "1")],
    "GND": [("J1", "2"), ("U1", "2"), ("C1", "2"), ("C2", "2"), ("D1", "1"), ("#FLG02", "1")],
    "+3V3": [("U1", "5"), ("C2", "1"), ("R1", "1")],
    "LED_A": [("R1", "2"), ("D1", "2")],
}
connect(sch, nets)
sch.no_connects.add(pin_pos(sch, "U1", "4"))
path = out / "toy.kicad_sch"
sch.save(path)
errors = check_nets(path, nets)
print("\n".join(errors) or "nets OK")
sys.exit(1 if errors else 0)
