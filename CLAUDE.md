# desk_notifier: agent notes

Desk notifier on an ESP32-C61: one GC9A01 round display, a BME688 (I2C), SNTP clock, web API. Split from the esp_eyes project (history kept).

## Tooling
- ESP-IDF v6.1 in `~/esp/esp-idf`. Activate with `source scripts/idf-env.sh` (never `export.sh`). Python via `uv run`. `ls` is aliased here: use `command ls`.
- `scripts/check.sh [--quick]`: core-include check, host doctest (ASan/UBSan, -Werror), pytest (`clipprep/`, `tools/`), `idf.py build`.
- Firmware: `cd firmware && idf.py build`; flash with `idf.py -p PORT flash monitor`.

## Layout
`firmware/components/<name>/{core,idf}`; `core/` is pure C++ (no `esp_*`/`freertos/` includes) and host-tested.
- `sensor` (airq score, BME688 driver, vendored Bosch BME68x API in `third_party/`), `screen` (font, home view, render task), `clock` (SNTP/TZ), `display`, `gfx`, `net`, `web`, `settings`, `storage` + `clip` (pictures, not yet shown), `engine` (errors/types only).
- Kconfig prefix `DESK_*`. NVS namespace is still `eyes` (keeps Wi-Fi credentials from the old firmware).

## Board
ESP32-C61 MAC d8:85:ac:60:0e:d0, host `desk-600ed0` (192.168.178.86). Flash over the UART bridge `/dev/cu.usbserial-1110`; the native USB-Serial/JTAG port (the console) did not enumerate on the Mac, so logs are not visible there. Check state over HTTP (`/api/status` shows `sensor.i2c` scan). BME688 answers at 0x77. Display is mirrored-X by default (`DESK_LCD_MIRROR_X`; unconfirmed on the panel). Kconfig bools set to `n` are undefined macros: use `#ifdef`, there is no `IS_ENABLED`.
GitHub: https://github.com/tobiwan88/desk_notifier (remote `desk`; `origin` is still the local esp_eyes clone source).

## Status
Home screen shows time, temperature, humidity, pressure and an air-quality score (gas baseline, not BSEC). Alert after Poor air; button or `POST /api/ack` snoozes 15 min. API: `GET /api/status`, `GET /api/sensor`, `POST /api/ack`, `POST /api/ota`, clips CRUD.
Next: pages (time, air, temperature, outside weather), pushed pictures until ack, BSEC2, weather client.
