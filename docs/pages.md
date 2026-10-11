# Screen pages

![All pages and moods](media/pages.png)

The display turns through three pages and fades between them. It draws 10 frames per second, so the pictures move.

| Page | Shows | Time on screen |
|---|---|---|
| Clock | Time, date, indoor and outdoor temperature, a small squirrel peeking in | 12 s |
| Air | The squirrel mascot in front of the outdoor weather, indoor temperature and humidity, the air level and score | 15 s |
| Weather | Today's forecast for Munich as a small animated sky, the outdoor temperature, high and low, chance of rain | 12 s |

![The rotation](media/pages_tour.gif)

## The squirrel

The squirrel shows the indoor air from the BME688:

| Air level | Squirrel |
|---|---|
| VERY GOOD (score 70+) | Hops, holds its acorn, happy closed eyes, sparkles, flowers |
| GOOD (50–69) | Smiles, holds its acorn, blinks, wags its tail |
| OKAY (35–49) | Half-closed eyes, flat mouth, acorn put down, tail droops a bit |
| BAD (below 35) | Worried face, sweat drop, fans the air with its paw, grey haze. The text switches between BAD and OPEN WINDOW |
| Sensor warming up | Sleeps under the stars, Zzz |

Behind the squirrel the sky shows the weather outside, the same as on the weather page: sun, moon and stars at night, clouds, rain, snow (the hill turns white), fog, thunderstorms with lightning. Under rain and storms the hill gets darker.

The squirrel dresses for the outdoor temperature:

| Outside | Squirrel |
|---|---|
| below 8 °C | Knitted hat with a pom-pom and a scarf |
| 25 °C and more | Strawberry ice cream with a cherry, slowly melting. It holds it in both paws (very good, good, okay), in the free paw while it fans the air (bad), or puts it down next to it (sleeping) |

Before the first forecast arrives the air page uses its own colours for each air level and no outfit.

When the air is BAD the air page stays on screen and its sky pulses red until you press the button or call `POST /api/ack` (15 minutes snooze).

## Weather

The board asks [Open-Meteo](https://open-meteo.com/) every 20 minutes. No account or key is needed. The picture comes from today's forecast (WMO code): sun (or moon at night), partly cloudy, cloudy, fog, drizzle, rain, snow, thunder. The page is left out of the rotation until the first forecast arrives, and again if the last good one is older than 3 hours.

Change the place over the API (the name shows after a reboot):

```sh
curl -X POST http://desk-xxxxxx.local/api/weather -d '{"lat":52.52,"lon":13.405,"place":"Berlin"}'
```

or with the console command `weather at 52.52 13.405 Berlin`, or in `idf.py menuconfig` → Desk notifier → Weather.

## Controls

| How | What |
|---|---|
| Button | During a BAD-air alert: snooze it. Otherwise: next page |
| `POST /api/page` `{"page":"weather"}` | Fade to a page (`clock`, `air`, `weather`, or `next`) |
| `POST /api/page` `{"refresh_weather":true}` | Fetch the forecast now |
| `POST /api/weather` `{"lat":..,"lon":..,"place":".."}` | Change the forecast location |
| `GET /api/weather` | The last forecast as JSON |
| Console `page air`, `page next` | Same as the API |
| Console `weather`, `weather refresh` | Show or fetch the forecast |

Page times and the frame rate are in `idf.py menuconfig` → Desk notifier → Pages. Set a page time to 0 to skip that page.

## Preview on your computer

```sh
scripts/check.sh --quick                 # builds build/host/page_preview
uv run python tools/preview_pages.py     # GIFs and sheet.png in build/preview
```

The preview uses the same drawing code as the board.

To check every combination, add `matrix`: one sheet per weather (`build/preview/matrix_*.png`), rows are the air levels, columns are day and night at -2, 15 and 29 °C:

```sh
uv run python tools/preview_pages.py build/preview matrix
```
