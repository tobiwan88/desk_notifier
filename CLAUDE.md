# desk_notifier: agent notes

Desk notifier on an ESP32-C61: one GC9A01 round display, a BME688 (I2C), SNTP clock, web API. Split from the esp_eyes project (history kept).

## Tooling
- ESP-IDF v6.1 in `~/esp/esp-idf`. Activate with `source scripts/idf-env.sh` (never `export.sh`). Python via `uv run`. `ls` is aliased here: use `command ls`.
- `scripts/check.sh [--quick]`: core-include check, host doctest (ASan/UBSan, -Werror), pytest (`clipprep/`, `tools/`), `idf.py build`.
- Firmware: `cd firmware && idf.py build`; flash with `idf.py -p PORT flash monitor`.

## Layout
`firmware/components/<name>/{core,idf}`; `core/` is pure C++ (no `esp_*`/`freertos/` includes) and host-tested.
- `sensor` (airq score, BME688 driver, vendored Bosch BME68x API in `third_party/`), `screen` (font, AA shape rasterizer `draw.hpp`, pages: clock / squirrel air page / squirrel's day page (`day.cpp`), `Rotator`, render task), `weather` (Open-Meteo parse in core, HTTP fetch task in idf), `clock` (SNTP/TZ), `display`, `gfx`, `net`, `web`, `settings`, `storage` + `clip` (pictures, not yet shown), `engine` (errors/types only).
- Kconfig prefix `DESK_*`. NVS namespace is still `eyes` (keeps Wi-Fi credentials from the old firmware).

## Board
ESP32-C61 MAC xx:xx:xx:xx:xx:xx, host `desk-xxxxxx` (192.168.1.x). Flash over the UART bridge `/dev/cu.usbserial-XXXX`; the native USB-Serial/JTAG port (the console) did not enumerate on the Mac, so logs are not visible there. Check state over HTTP (`/api/status` shows `sensor.i2c` scan). BME688 answers at 0x77. Display is mirrored-X by default (`DESK_LCD_MIRROR_X`; unconfirmed on the panel). Kconfig bools set to `n` are undefined macros: use `#ifdef`, there is no `IS_ENABLED`.
GitHub: https://github.com/tobiwan88/desk_notifier (remote `desk`; `origin` is still the local esp_eyes clone source).

## Squirrel outfits
`outfit_for(weather_ok, Forecast)` in `screen/core/squirrel.cpp`: hat <8 C, ice cream >=25 C, sunglasses on sunny days, umbrella in rain (drops the ice cream), glider at wind >=30 km/h (`Forecast.wind_kmh`, `/api/weather` shows it). The clock page never shows the umbrella (peeking squirrel too small). Check layouts with `tools/preview_pages.py`; scenes live in `firmware/test/host/preview.cpp`.

## Status
Three pages rotate with a fade at CONFIG_DESK_FPS (default 10): clock (+ date, in/out temperature, today's low/high/rain, peeking squirrel), air (squirrel mascot, mood = air level: VERY GOOD/GOOD/OKAY/BAD, sleepy while warming up; sky behind it = outdoor weather via `screen/core/sky`; hat+scarf below 8 C, ice cream from 25 C outside), day (the squirrel at its desk; `activity_for()` in `screen/core/day.cpp` picks the scene from weekday + time + `Schedule` (Kconfig `DESK_DAY_*`, HHMM), never at random: sleep, coffee, work, lunch, close laptop, read; weekends sleep in, no work. Window = real sky via `sky::draw`, wall clock = real time. The old weather page was removed as a duplicate of the air page's sky; `page weather` is an alias for `day`). Poor air pins the air page until ack; button = ack during an alert, else next page. API: `GET /api/status`, `GET /api/sensor`, `GET`/`POST /api/weather`, `POST /api/page` (clock/air/day/next), `POST /api/ack`, `POST /api/ota`, clips CRUD. Console: `page`, `weather`.
Preview pages on the host: build host tests, then `uv run python tools/preview_pages.py` (GIFs + sheet.png in build/preview; add `matrix` for all weather x mood x day/night x temperature sheets).
The C61 has no FPU: keep per-pixel work integer (see `draw::fill`), floats only per span/row.
Next: more day scenes (environment overrides: dry air, cold/hot room, rain at the window, snow; seasons/holidays), the clock-page squirrel holding the current activity's prop, pushed pictures until ack, BSEC2.
