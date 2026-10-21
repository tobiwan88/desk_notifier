# Desk notifier

A small desk display on an ESP32-C61. A round 240x240 GC9A01 screen turns through three animated pages: a clock with today's forecast for Munich, a cute squirrel that shows the indoor air quality from a BME688 sensor, and the squirrel's day at home, which follows the clock (coffee in the morning, laptop, lunch, reading, sleep). When the air gets bad the squirrel stays on screen and asks you to open a window until you press a button.

![The pages](docs/media/pages_tour.gif)

It started as the esp_eyes project (animated eyes on two displays) and was cut down to one display. The git history keeps the old work.

## What works today
- Rotating pages with fades, 10 frames per second. See [docs/pages.md](docs/pages.md).
  - Clock: time (set over the internet), date, indoor and outdoor temperature, today's low, high and chance of rain.
  - Air: a squirrel mascot whose mood shows the air level (very good, good, okay, bad), with temperature, humidity and score.
  - Day: the squirrel at its desk at home, doing what fits the time and weekday. The room's climate changes it (stuffy air: it yawns or opens the window; too warm, too cold, too dry, too humid; a sudden smell), so does the weather (umbrella, cocoa, thunder), and special days decorate the room (birthday, anniversary, Christmas, Advent, Halloween, Oktoberfest, ...). The real weather shows in the window, the real time on the wall clock. The scene is never random, but little things are: blinks, glances, a wave, a bird on the windowsill, a ladybug on the desk, a shooting star, a paper plane, a rare golden acorn. The moon shows its real phase, rainbows follow rain, a window-sill plant grows with good air day after day, and a wall calendar counts down to birthdays.
  - Pet page at `http://<device>.local/pet`: feed it, have a coffee break or a party together, pet it, play, water the plant, say good night, give it gifts. Daily visits grow a friendship (five heart levels, never lost) with more reactions and visitors.
- Bad-air alert: the air page stays on, snoozed for 15 minutes by the button or by `POST /api/ack`. Without an alert the button shows the next page.
- Web page at `http://<device>.local/` and a small HTTP API.
- Wi-Fi, over-the-air updates, and a serial console.
- Picture upload (the clip format from esp_eyes) is stored but not shown yet.

Planned: pictures that stay on screen until acknowledged, Bosch BSEC2 for a proper IAQ value.

## What you need
- ESP32-C61 board (8 MB flash)
- GC9A01 round display, 240x240, SPI
- BME688 breakout (I2C)
- A push button (optional)
- USB cable

## The squirrel
The squirrel's mood follows the indoor air quality. What it wears follows the weather outside (Open-Meteo, Munich by default):

| Outside | Squirrel |
|---|---|
| below 8 °C | knitted hat and scarf |
| 25 °C or more | ice cream |
| sunny, daytime | sunglasses |
| drizzle, rain, thunder | umbrella |
| wind of 30 km/h or more | glides under the umbrella, tail streaming |

Items stack (cold, rainy and windy gives hat, umbrella and gliding). Preview every look on your computer with `uv run python tools/preview_pages.py out/` after `scripts/check.sh --quick`.

## Wiring
| Signal | GPIO | Note |
|---|---|---|
| Display SCL / SDA | 6 / 5 | |
| Display DC / RST / CS | 3 / 2 / 4 | |
| Display VCC / GND | 5V / GND | |
| BME688 SDA / SCL | 22 / 23 | 3V3, not 5V. Address 0x77 or 0x76, both are tried |
| Button | 24 | other leg to GND |

Details: [docs/wiring.md](docs/wiring.md). Pins can be changed in `idf.py menuconfig` under "Desk notifier".
If the picture is mirrored, flip `DESK_LCD_MIRROR_X` / `DESK_LCD_MIRROR_Y` there.

## Build and flash
You need ESP-IDF v6.1 and [uv](https://docs.astral.sh/uv/).

```sh
source scripts/idf-env.sh
cd firmware
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

## Connect to Wi-Fi
Open the serial console and run `wifi "<name>" <password>`. The device reboots and joins your network. It is then reachable as `desk-<last 6 MAC digits>.local`. The timezone is set in menuconfig (`DESK_TZ`, default Central Europe).

## API
By default the API needs no password (change with the console command `auth token`).

```sh
curl http://desk-xxxxxx.local/api/sensor     # temperature, humidity, pressure, gas, score
curl http://desk-xxxxxx.local/api/status     # uptime, memory, Wi-Fi, sensor state
curl -X POST http://desk-xxxxxx.local/api/ack   # snooze the alert
curl http://desk-xxxxxx.local/api/weather    # the last forecast
curl -X POST http://desk-xxxxxx.local/api/page -d '{"page":"day"}'   # clock, air, day or next ("weather" still works)
curl -X POST http://desk-xxxxxx.local/api/weather -d '{"lat":48.137,"lon":11.575,"place":"Munich"}'   # change the place
curl -X POST --data-binary @firmware.bin http://desk-xxxxxx.local/api/ota
```

## Console commands
`wifi`, `status`, `sensor`, `ack`, `page clock|air|day|next`, `weather [refresh | at <lat> <lon> [name]]`, `lcd pins`, `lcd init [hz]`, `hostname`, `auth open|token`, `token`, `reboot`, `factory-reset`.

## About the air-quality score
It is a simple estimate, not Bosch BSEC. 25 points come from humidity (best at 40 %) and 75 from the gas resistance compared with the highest value seen recently. It needs about 3 minutes to warm up and a stretch of clean air to be meaningful. The sensor also reads a little warm because it sits next to the board.

## For developers
`scripts/check.sh` runs the host tests (C++23, sanitizers), the Python tests and the firmware build. `--quick` skips the build. `uv run python tools/preview_pages.py` draws all pages as GIFs on your computer (after a host build). Pure logic lives in `firmware/components/*/core` and is tested on the PC. See [CLAUDE.md](CLAUDE.md) for the layout.

## License
MIT. The BME68x driver in `firmware/components/sensor/third_party` is from Bosch Sensortec (BSD-3-Clause).
