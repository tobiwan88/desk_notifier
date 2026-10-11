# Wiring (ESP32-C61 DevKit, one GC9A01 + BME688 + button)

| Signal | GPIO | Note |
|---|---|---|
| LCD SCL | 6 | |
| LCD SDA | 5 | |
| LCD DC | 3 | |
| LCD RST | 2 | |
| LCD CS | 4 | |
| LCD VCC / GND | 5V / GND | |
| BME688 SDA | 22 | `DESK_I2C_SDA` |
| BME688 SCL | 23 | `DESK_I2C_SCL` |
| BME688 VIN / GND | 3V3 / GND | not 5V; address 0x77 default (SDO high) or 0x76 (SDO to GND), see `DESK_BME_ADDR`; both are tried |
| Button | 24 | other leg to GND, internal pull-up (`DESK_BTN`, -1 disables) |

Avoid GPIO 8-14 (strapping, UART0, USB, PSRAM). GPIO 7 is free now. Console: `lcd pins`, `lcd init [hz]`, `sensor`, `ack`.
