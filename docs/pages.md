# Screen pages

![All pages and moods](media/pages.png)

The display turns through three pages and fades between them. It draws 10 frames per second, so the pictures move.

| Page | Shows | Time on screen |
|---|---|---|
| Clock | Time, date, indoor and outdoor temperature, today's low and high (with the chance of rain from 20 %), a small squirrel peeking in | 12 s |
| Air | The squirrel mascot in front of the outdoor weather, indoor temperature and humidity, the air level and score | 15 s |
| Day | The squirrel at its desk at home, doing what fits the time of day. The real weather in the window, the real time on the wall clock | 12 s |

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

Behind the squirrel the sky shows the weather outside: sun, moon and stars at night, clouds, rain, snow (the hill turns white), fog, thunderstorms with lightning. Under rain and storms the hill gets darker.

The squirrel dresses for the outdoor temperature:

| Outside | Squirrel |
|---|---|
| below 8 °C | Knitted hat with a pom-pom and a scarf |
| 25 °C and more | Strawberry ice cream with a cherry, slowly melting. It holds it in both paws (very good, good, okay), in the free paw while it fans the air (bad), or puts it down next to it (sleeping) |

Before the first forecast arrives the air page uses its own colours for each air level and no outfit.

When the air is BAD the air page stays on screen and its sky pulses red until you press the button or call `POST /api/ack` (15 minutes snooze).

## The squirrel's day

The day page shows what the squirrel is doing at home. It follows the clock, never chance: the same time on the same weekday always gives the same scene, and only the small animation inside a scene moves.

| Weekdays | Squirrel | Text |
|---|---|---|
| 06:30–08:59 | Holds a steaming mug, takes a sip every few seconds | COFFEE TIME |
| 09:00–11:59, 13:00–17:59 | Laptop open, screen light on its face, cold coffee on the desk | BUSY |
| 12:00–12:59 | Pumpkin soup, spoon to mouth, cheeks full while chewing | LUNCH |
| 18:00–18:29 | Closes the laptop, then a big happy stretch | WORK DONE |
| 18:30–22:59 | Reads a book under the desk lamp, a page turns now and then | READING |
| 23:00–06:29 | In bed with a nightcap, the blanket rises and falls, Zzz | GOOD NIGHT |

On Saturday and Sunday it sleeps 90 minutes longer, has coffee for two hours, lunch as usual, and reads the rest of the day.

The window shows the weather outside (sun, moon, clouds, rain, snow, fog, thunder), the wall clock the real time, and the room gets darker in the evening. The face still follows the indoor air. The page is left out until the time is known.

Change the times in `idf.py menuconfig` → Desk notifier → Squirrel's day (as HHMM, so 630 is 06:30). Bedtime may be after midnight (e.g. 30).

## Weather

The board asks [Open-Meteo](https://open-meteo.com/) every 20 minutes. No account or key is needed. The air page shows it as the sky behind the squirrel and as the squirrel's outfit, the day page in the window, the clock page as numbers. There used to be a separate weather page; it showed the same sky and temperature again and was replaced by the day page. If the last good forecast is older than 3 hours, the pages fall back to their own colours.

Change the place over the API:

```sh
curl -X POST http://desk-xxxxxx.local/api/weather -d '{"lat":52.52,"lon":13.405,"place":"Berlin"}'
```

or with the console command `weather at 52.52 13.405 Berlin`, or in `idf.py menuconfig` → Desk notifier → Weather.

## Controls

| How | What |
|---|---|
| Button | During a BAD-air alert: snooze it. Otherwise: next page |
| `POST /api/page` `{"page":"day"}` | Fade to a page (`clock`, `air`, `day`, or `next`; `weather` still works and means `day`) |
| `POST /api/page` `{"refresh_weather":true}` | Fetch the forecast now |
| `POST /api/weather` `{"lat":..,"lon":..,"place":".."}` | Change the forecast location |
| `GET /api/weather` | The last forecast as JSON |
| Console `page day`, `page next` | Same as the API |
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
