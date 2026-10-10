# Wiring and GPIO map

ESP32-C61-DevKitC-1 → two GC9A01 round displays (1.28", 7-pin modules) on one shared SPI bus.
**This file is the single source of truth. If you move a wire, change `CONFIG_EYES_LCD_PIN_*` in `firmware/main/Kconfig.projbuild` (or `sdkconfig.defaults`) to match and update this table.**

## Display wiring

Module header order: `1 VCC · 2 GND · 3 SCL · 4 SDA · 5 DC · 6 CS · 7 RST`

| Signal | ESP32-C61 GPIO | DevKit header pin | Left display | Right display | Kconfig |
|---|---|---|---|---|---|
| SCL (SPI clock) | **6** | J1-5 | pin 3 | pin 3 | `EYES_LCD_PIN_SCLK` |
| SDA (SPI MOSI) | **5** | J1-4 | pin 4 | pin 4 | `EYES_LCD_PIN_MOSI` |
| DC (data/command) | **3** | J1-13 | pin 5 | pin 5 | `EYES_LCD_PIN_DC` |
| RST (reset) | **2** | J1-12 | pin 7 | pin 7 | `EYES_LCD_PIN_RST` |
| CS left | **4** | J1-3 | pin 6 | – | `EYES_LCD_PIN_CS_L` |
| CS right | **7** | – | – | pin 6 | `EYES_LCD_PIN_CS_R` |
| VCC | 5V | J1-14 | pin 1 | pin 1 | – |
| GND | GND | J1-15 | pin 2 | pin 2 | – |

- "Left" and "right" are the viewer's left and right (logical eye 0 / eye 1).
- Shared lines (SCL, SDA, DC, RST, VCC, GND) go to both displays; only the two CS wires are separate.
- Power the modules from the 5V pin: each module has its own 3.3 V regulator for the panel and backlight.
- The modules have no backlight pin (always on), 1 kΩ series resistors on SCL/SDA/DC/CS/RST, so the SPI clock is 20 MHz by default.
- GPIO7 is a boot-strapping pin. As the right CS it works (idles high after boot); if boot ever misbehaves with the display attached, move that wire to GPIO0 or GPIO22 and set `CONFIG_EYES_LCD_PIN_CS_R`.
- The earlier plan used GPIO0 for the right CS. A mismatch between code and wire (right CS on GPIO7 but code on GPIO0) made the right display receive both displays' data (symptom: L and R alternating on the right display with the `lcd test` pattern).

## All GPIOs of the board

| GPIO | Use here | Notes |
|---|---|---|
| 0 | free | 32 kHz crystal pad on some boards; fine as a plain GPIO on the DevKitC-1 |
| 1 | free (reserved: optional backlight PWM, `EYES_LCD_PIN_BL`, default −1) | |
| 2 | **RST** (both displays) | |
| 3 | **DC** (both displays) | |
| 4 | **CS left** | JTAG MTMS pad, free for GPIO use |
| 5 | **SDA / MOSI** | JTAG pad, free for GPIO use |
| 6 | **SCL / SCLK** | JTAG pad, free for GPIO use |
| 7 | **CS right** | strapping pin, used as output after boot |
| 8 | – (onboard RGB LED, strapping) | do not use |
| 9 | – (strapping: low at reset = download mode, BOOT button) | do not use |
| 10, 11 | – (UART0 TX/RX) | console is on USB-Serial/JTAG, not here |
| 12, 13 | – (USB D−/D+) | the USB cable / console |
| 14 | – (PSRAM chip select) | do not use |
| 22, 23, 24 | free | spare; good places to move a CS or add a debug pin |
| 5V / 3V3 / GND | power | displays on 5V |

Compile-time checks in `firmware/main/config.hpp` reject LCD pins on GPIO8–14, duplicate pins, and an SPI clock above 40 MHz.

## Checking the wiring on the device

Console (`uv run python tools/console.py /dev/cu.usbmodem11101 "<command>"`):

- `lcd test 60`: left display shows red bands with a white **L**, right display blue bands with a white **R**, for 60 s. Anything of the other colour on a display means data or CS leaked between the panels.
- `lcd swap on|off`: draw the left eye on the right CS panel and vice versa.
- `lcd init [spi_hz]`: reset and re-initialise both panels, optionally at another SPI clock.
- `lcd pins`: print the idle level of the CS, DC and RST pins.

Nothing on one display usually means a wrong CS wire, or missing VCC/GND/RST on that module. The SPI link is write-only, so the firmware cannot detect a disconnected panel.
