# ESP Eyes

Two round screens show animated cat eyes. An ESP32-C61 board controls them. You change the eyes from a web page, from a script, or from the command line. The eyes also move by themselves.

## What you need

- 1 × ESP32-C61-DevKitC-1 board (8 MB flash, 2 MB PSRAM)
- 2 × GC9A01 round display modules (1.28 inch, 240 × 240 pixels, 7 pins)
- Wires and a USB data cable
- A computer with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v6.1, [uv](https://docs.astral.sh/uv/), and a C++ compiler

## Connect the displays

Both displays share 6 wires. Each display has its own CS wire.

| Display pin | Board pin | Both displays? |
|---|---|---|
| VCC | 5V | yes |
| GND | GND | yes |
| SCL | GPIO6 | yes |
| SDA | GPIO5 | yes |
| DC | GPIO3 | yes |
| RST | GPIO2 | yes |
| CS (left display) | GPIO4 | no |
| CS (right display) | GPIO7 | no |

"Left" and "right" are as you see them when you look at the displays.

The full pin list is in [docs/wiring.md](docs/wiring.md). If you change a wire, change the matching setting in `firmware/main/Kconfig.projbuild`.

## Build and flash

```sh
source scripts/idf-env.sh              # prepare ESP-IDF in this terminal
cd firmware
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash   # use the port of your board
```

If you flash more than one board, check the MAC address first:
`python -m esptool --port <port> chip-id`

## Connect to Wi-Fi

The board does not know your Wi-Fi at first. Send it the name and password through the USB console:

```sh
uv run python tools/console.py /dev/cu.usbmodemXXXX 'wifi "My Network" mypassword'
```

The board saves the data and restarts. It then joins your network (2.4 GHz only). Its name is `eyes-` and 6 characters, for example `eyes-xxxxxx.local`. The board prints its IP address on the console.

## Use the eyes

Open the board address in a web browser, for example `http://eyes-xxxxxx.local/`.

On the page you can:

- choose a style and an iris color
- start an animation (wink, slow blink, roll, side eye, and more)
- push **Random** for a random animation
- turn random idle animations on or off
- drag on the round pad to make the eyes look somewhere

The board remembers your style, iris color, and idle setting after a restart.

**Styles:** realistic (default), flat, line, anime, cute, halloween, christmas
**Iris colors:** amber (default), green, blue, copper, violet, odd
**Animations:** wink, slow_blink, roll, side_eye, hunt, startle, nictitate, sleepy, cross_eyed, happy, glance, special

## Use the API

The API uses JSON over HTTP. No password is necessary by default.

```sh
HOST=eyes-xxxxxx.local

# change the style and iris
curl -X POST http://$HOST/api/eye -d '{"style":"anime","iris":"green"}'

# play one animation (or "blink", or "random")
curl -X POST http://$HOST/api/eye -d '{"play":"wink"}'

# turn random idle animations off
curl -X POST http://$HOST/api/eye -d '{"auto":false}'

# look at a point: x and y from -1 to 1, hold time in ms
curl -X POST http://$HOST/api/eye -d '{"look":[0.5,-0.2,1000]}'

# save style, iris, and idle setting
curl -X POST http://$HOST/api/eye -d '{"style":"cute","save":true}'

# list all names and show the current state
curl http://$HOST/api/animations
```

Other routes: `GET /api/status` (health data), `POST /api/play` (play a clip), `POST /api/stop`, `GET /api/clips`, `PUT` and `DELETE /api/clips/<name>`, `POST /api/ota` (firmware update).

You can also use the command-line tool:

```sh
export EYES_HOST=eyes-xxxxxx.local
uv run python tools/eyesctl.py status
uv run python tools/eyesctl.py anim wink
uv run python tools/eyesctl.py style anime --iris green --save
```

### Password (optional)

Anyone on your network can control the board. To require a token, use the console command `auth token`. The command `token show` prints the token. Send it as the header `Authorization: Bearer <token>`. Use `auth open` to remove the token again. The web page does not support a token.

## Your own clips

A clip is a short video that plays on the displays. Make a clip from a GIF, a PNG sequence, or a video file:

```sh
PYTHONPATH=clipprep uv run python -m clipprep my.gif my.clip --preview preview.gif
uv run python tools/eyesctl.py upload my.clip --name hello
uv run python tools/eyesctl.py play hello
```

`eyes-studio` is a small web app that does the same steps with a form:

```sh
PYTHONPATH=clipprep:studio:tools EYES_DEVICES=eyes-xxxxxx.local uv run python -m eyes_studio
```

Then open `http://localhost:8080`.

## Console commands

Connect with `uv run python tools/console.py <port> "<command>"`. Useful commands:

| Command | Action |
|---|---|
| `status` | show health data and the last reset reasons |
| `wifi "<name>" <password>` | set Wi-Fi |
| `eye <animation>` | play an animation |
| `lcd test 30` | show a test pattern (red **L**, blue **R**) for 30 seconds |
| `lcd init` | start both displays again |
| `auth open` / `auth token` | allow anonymous access / require a token |
| `reboot` | restart the board |
| `factory-reset` | delete all saved settings |

## If something does not work

| Problem | What to do |
|---|---|
| One display is blank | Check its CS wire. Then restart the board. The board starts the displays only at boot. |
| Both displays are blank | Check VCC, GND, SCL, SDA, DC, and RST. Restart the board. |
| Wrong picture on a display | Run `lcd test 30`. Red **L** must show on the left display. Blue **R** must show on the right display. If not, check the CS wires. |
| Picture jumps or has noise | Use shorter wires. Or lower the SPI clock (`CONFIG_EYES_LCD_SPI_HZ`). The default is 20 MHz. |
| Board does not start from a power bank | Use a better cable or a stronger power source. Read the reset reasons with `status`. `BROWNOUT` means too little power. |
| Web page does not open | Use the IP address instead of the `.local` name. Check that your computer is on the same network. |

## For developers

```sh
scripts/check.sh          # all tests and the firmware build
scripts/check.sh --quick  # tests only
```

- `firmware/components/cat_eyes` draws the eyes. It is plain C.
- `firmware/components/engine`, `eyes`, `clip` are C++ and have tests that run on your computer.
- `firmware/components/display`, `net`, `web`, `storage`, `settings` use ESP-IDF and run on the board only.
- `clipprep` makes clips. `tools` has the command-line tools. `studio` is the web app for clips.
- [eyes-c61-implementation-plan.md](eyes-c61-implementation-plan.md) is the design plan.
- [CLAUDE.md](CLAUDE.md) has notes for coding agents.

## Known limits

- The speed is about 15 to 20 frames per second. The halloween and christmas styles are slower, about 14 frames per second, because they redraw the whole screen.
- The board has little free internal memory (about 37 KB at its lowest point). Add new features with care.
- The 24-hour test and the power-cut test are not done yet.

## License

This project uses the [MIT License](LICENSE). You can use, copy, change, and share it for any purpose, also in commercial products. Keep the copyright line and the license text with the code. The license covers all code in this repository, also the `cat_eyes` component and its eye designs.

The project includes other code that has its own license:

- cJSON (MIT) in `firmware/test/host/third_party`, used for tests only
- doctest (MIT) in `firmware/test/host`
- ESP-IDF and the ESP component registry libraries (Apache-2.0 and others). The build downloads them. They are not part of this repository.
